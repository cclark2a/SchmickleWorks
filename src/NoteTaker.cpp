#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include <limits.h>

// to do:
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior
    //   when user clicks these from the context menu

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    gateExpiration.fill(0);
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync2.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
}

// to compute range for horizontal wheel when selecting notes
unsigned NoteTaker::horizontalCount() const {
    unsigned count = 0;
    int lastTime = -1;
    for (auto& note : allNotes) {
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (lastTime == note.startTime) {
            continue;
        }
        ++count;
        lastTime = note.startTime;
    }
    return count;
}

bool NoteTaker::isEmpty() const {
    for (auto& note : allNotes) {
        if (note.isSelectable(selectChannels)) {
            return false;
        }
    }
    return true;
}

int NoteTaker::nthNoteIndex(int value) const {
    if (value < 0) {
        assert(selectButton->editStart());
        return 0;
    }
    int count = value;
    int lastTime = -1;
    for (auto& note : allNotes) {
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (count <= 0) {
            return &note - &allNotes.front();
        }
        if (lastTime == note.startTime) {
            continue;
        }
        --count;
        lastTime = note.startTime;
    }
    debug("nthNoteIndex value %d", value);
    this->debugDump();
    assert(0);
    return -1;
}

int NoteTaker::noteIndex(const DisplayNote& match) const {
    if (MIDI_HEADER == match.type) {
        return -1;
    }
    int count = 0;
    int lastTime = -1;
    for (auto& note : allNotes) {
        if (&match == &note) {
            return count;
        }
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (lastTime == note.startTime) {
            continue;
        }
        ++count;
        lastTime = note.startTime;
    }
    assert(0);
}

void NoteTaker::updateHorizontal() {
    if (runButton->running()) {
        // to do : while running, change tempo
        return;
    }
    int wheelValue = (int) horizontalWheel->value;
    bool noteChanged = false;
    if (!selectButton->ledOn) {
        int diff = 0;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
            if (note.isSelectable(selectChannels)) {
                if (NOTE_ON == note.type) {
                    assert((unsigned) wheelValue < noteDurations.size());
                    note.setNote(wheelValue);
                } else {
                    note.setRest(wheelValue);
                }
                int duration = noteDurations[wheelValue];
                diff += duration - note.duration;
                note.duration = duration;
            }
        }
        if (diff) {
            this->shiftNotes(selectEnd, diff);
            noteChanged = true;
        }
    } else {
    // value should range from 0 to max - 1, where max is number of starts for active channels
    // if insert mode, value ranges from -1 to max - 1
        int index = this->nthNoteIndex(wheelValue);
        assert(index >= 0);
        noteChanged = selectButton->editEnd() ? this->setSelectEnd(index) :
                this->setSelectStart(index);
    }
    if (noteChanged) {
        this->playSelection();
    }
}
    
void NoteTaker::updateVertical() {
    if (runButton->running()) {
        // to do : if running, transpose all up/down
        return;
    }
    if (partButton->ledOn) {
        unsigned value = (unsigned) verticalWheel->value;
        assert(value < CHANNEL_COUNT);
        if (selectButton->ledOn) {  // set part to chosen value
            selectChannels = 1 << value;
            this->playSelection();
        } else {  // set selection to chosen part value
            for (unsigned index = selectStart; index < selectEnd; ++index) {
                DisplayNote& note = allNotes[index];
                if (!note.isSelectable(selectChannels)) {
                    continue;
                }
                note.setChannel(value);
            }
        }
        this->playSelection();
        return;
    }
    // transpose selection
    // loop below computes diff of first note, and adds diff to subsequent notes in select
    int diff = 0;
    for (unsigned index = selectStart ; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        int value;
        if (!diff) {
            value = (int) verticalWheel->value;
            diff = value - note.pitch();
            if (!diff) {
                return;
            }
        } else {
            value = std::max(0, std::min(127, note.pitch() + diff));
        }
        note.setPitch(value);
    }
    this->playSelection();
}

void NoteTaker::outputNote(const DisplayNote& note) {
    assert((0xFF == note.channel && MIDI_HEADER == note.type) || note.channel < CV_OUTPUTS);
    if (NOTE_ON == note.type) {
        gateExpiration[note.channel] = note.startTime + note.duration;
        outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
	    float v_oct = inputs[V_OCT_INPUT].value;
        outputs[CV1_OUTPUT + note.channel].value = v_oct + note.pitch() / 12.f;
    } else {
        assert((MIDI_HEADER == note.type && selectButton->editStart()) || REST_TYPE == note.type);
        if (0xFF == note.channel) {
            for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
                outputs[GATE1_OUTPUT + index].value = 0;
            }
        } else {
            outputs[GATE1_OUTPUT + note.channel].value = 0;
        }
    }
    display->dirty = true;
}

void NoteTaker::playSelection() {
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime);
    playingSelection = true;
}

void NoteTaker::setDisplayEnd() {
    // to do: allow time range to display to be scaled from box width
    // calc here must match NoteTakerDisplay::draw:75
    displayEnd = this->lastAt(allNotes[displayStart].startTime
            + display->box.size.x * 4 - 32);
}

bool NoteTaker::setSelectStart(unsigned start) {
    if (selectStart == start) {
        return false;
    }
    selectEnd = selectStart = start;
    do {
        ++selectEnd;
    } while (allNotes[selectStart].startTime == allNotes[selectEnd].startTime);
    this->setWheelRange();
    return true;
}

void NoteTaker::setSelectStartAt(int midiTime, unsigned startIndex, unsigned endIndex) {
    int start = INT_MAX;
    const DisplayNote* best = nullptr;
    for (unsigned step = startIndex; step < endIndex; ++step) {
        const DisplayNote& note = allNotes[step];
        if (note.isSelectable(selectChannels)) {
            if (note.isBestSelectStart(&best, midiTime)) {
                start = step;
            }
            if (note.isActive(midiTime)) {
                this->outputNote(note);
            }
        }
    }
    if (INT_MAX != start) {
        (void) this->setSelectStart(start);
    }
}

void NoteTaker::setWheelRange() {
    if (!selectButton) {
        return;
    }
    // horizontal wheel range and value
    int selectMin = selectButton->editStart() ? -1 : 0;
    int selectMax = this->horizontalCount();
    const DisplayNote* note = &allNotes[selectStart];
    if (!selectButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
    } else {
        horizontalWheel->setLimits(selectMin, selectMax);
    }
    if (!selectButton->ledOn) {
        horizontalWheel->setValue(REST_TYPE == note->type ? note->rest() :note->note());
    } else {
        int index = this->noteIndex(*note);
        if (index < selectMin || index >= selectMax) {
            debug("! note type %d index %d selectMin %d selectMax %d",
                    note->type, index, selectMin, selectMax);
            this->debugDump();
        }
        assert(index >= selectMin);
        assert(index < selectMax);
        horizontalWheel->setValue(index);
    }
    // vertical wheel range 0 to 127 for midi pitch
    debug("setWheelRange partButton->ledOn %d selectButton->ledOn %d",
            partButton->ledOn, selectButton->ledOn);
    if (partButton->ledOn) {
        verticalWheel->setLimits(0, CV_OUTPUTS);
        if (NOTE_ON == note->type || REST_TYPE == note->type) {
            verticalWheel->setValue(note->channel);
        }
        // only makes sense if there is a selectStart?
        // to do : set speed so small number of entries moves OK
        // to do : only do this if SelectButton::single_Select == selectButton->state ?
        // to do : set vertical wheel value to selectStart channel
    } else {
        verticalWheel->setLimits(0, 127);
        if (NOTE_ON == note->type) {
            verticalWheel->setValue(note->pitch());
        }
    }
    debug("horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
}

void NoteTaker::step() {
    if (!runButton || (!runButton->running() && !playingSelection)) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
	float deltaTime = engineGetSampleTime();
    elapsedSeconds += deltaTime;
    int midiTime = SecondsToMidi(elapsedSeconds);
    if (runButton->running()) {
        // to do: use info in midi header, time signature, tempo to get this right
        this->setExpiredGatesLow(midiTime);
        // if midi time exceeds displayEnd: scroll; reset displayStart, displayEnd
        // if midi time exceeds last of allNotes: either reset to zero and repeat,
        // or stop running
        bool recomputeDisplayEnd = false;
        if (this->lastNoteEnded(displayEnd, midiTime)) {
            elapsedSeconds = 0;  // to do : don't repeat unconditionally?
            midiTime = 0;
            displayStart = this->nthNoteIndex(0);
            if (!displayStart) { // no notes to play
                runButton->ledOn = false;
                return;
            }
            recomputeDisplayEnd = true;
        } else {
            do {
                const DisplayNote& note = allNotes[displayStart];
                if (note.startTime + note.duration > midiTime || TRACK_END != note.type) {
                    break;
                }
                ++displayStart;
                recomputeDisplayEnd = true;
            } while (true);
        }
        if (recomputeDisplayEnd) {
            this->setDisplayEnd();
        }
        this->setSelectStartAt(midiTime, displayStart, displayEnd);
    } else if (playingSelection) { // not running, play note after selecting or editing
        for (unsigned step = selectStart; step < selectEnd; ++step) {
            const DisplayNote& note = allNotes[step];
            if (note.isSelectable(selectChannels) && note.isActive(midiTime)) {
                this->outputNote(note);
            } else if (!step) {
                assert(selectButton->editStart());
                this->outputNote(note);
            }
        }
        if (this->lastNoteEnded(selectEnd, midiTime)) {
            playingSelection = false;
            this->setExpiredGatesLow(INT_MAX);
        }
    }
}
