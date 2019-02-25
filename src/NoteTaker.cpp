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
    this->reset();
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync2.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    this->setUpSampleNotes();
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

void NoteTaker::loadScore() {
    debug("loadScore start");
    unsigned index = (unsigned) horizontalWheel->value;
    assert(index < storage.size());
    NoteTakerParseMidi parser(storage[index], allNotes);
    parser.parseMidi();
    debug("loadScore end");
}

void NoteTaker::reset() {
    allNotes.clear();
    clipboard.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    displayStart = displayEnd = selectStart = selectEnd = 0;
    elapsedSeconds = 0;
    allOutputsOff = true;
    playingSelection = false;
    selectChannels = ALL_CHANNELS;
    lastHorizontal = INT_MAX;
    lastVertical = INT_MAX;
    this->zeroGates();
    Module::reset();
}

unsigned NoteTaker::wheelToNote(int value) const {
    debug("wheelToNote start");
    this->debugDump(false);
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
            return this->noteIndex(note);
        }
        if (lastTime == note.startTime) {
            continue;
        }
        --count;
        lastTime = note.startTime;
    }
    if (0 == count && selectButton->editEnd()) {
        auto& note = allNotes.back();
        assert(TRACK_END == note.type);
        return this->noteIndex(note);
    }
    debug("wheelToNote value %d", value);
    this->debugDump(false);
    assert(0);
    return (unsigned) -1;
}

int NoteTaker::noteToWheel(const DisplayNote& match) const {
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

void NoteTaker::saveScore() {
    unsigned index = (unsigned) horizontalWheel->value;
    assert(index <= storage.size());
    if (storage.size() == index) {
        storage.push_back(vector<uint8_t>());
    }
    auto& dest = storage[index];
    NoteTakerMakeMidi midiMaker;
    midiMaker.createFromNotes(allNotes, dest);
}

void NoteTaker::setUpSampleNotes() {
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
    channels[0].sustainMin = 1;
    channels[0].sustainMax = noteDurations.back();
    channels[0].releaseMin = 1;
    channels[0].releaseMax = 24;
}

void NoteTaker::updateHorizontal() {
    if (fileButton->ledOn) {
        // to do : nudge to next valid value once I figure out how that should work, exactly
        return;
    }
    const int wheelValue = (int) horizontalWheel->value;
    if (wheelValue == lastHorizontal) {
        return;
    }
    lastHorizontal = wheelValue;
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[this->firstChannel()];
        int duration = noteDurations[wheelValue];
        switch ((int) verticalWheel->value) {
            case 3: // to do : create enum to identify values
                channel.sustainMax = duration;
                if (channel.sustainMin > duration) {
                    channel.sustainMin = duration;
                }
                break;
            case 2:
                channel.sustainMin = duration;
                if (channel.sustainMax < duration) {
                    channel.sustainMax = duration;
                }
                break;
            case 1:
                channel.releaseMin = duration;
                if (channel.releaseMax < duration) {
                    channel.releaseMax = duration;
                }
                break;
            case 0:
                channel.releaseMax = duration;
                if (channel.releaseMin > duration) {
                    channel.releaseMin = duration;
                }
                break;
        }
        return;
    }
    if (isEmpty()) {
        return;
    }
    if (runButton->running()) {
        // to do : while running, change tempo
        return;
    }
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
            this->ShiftNotes(allNotes, selectEnd, diff);
            noteChanged = true;
        }
    } else {
    // value should range from 0 to max - 1, where max is number of starts for active channels
    // if insert mode, value ranges from -1 to max - 1
        unsigned index = this->wheelToNote(wheelValue);
        noteChanged = selectButton->editEnd() ? this->setSelectEnd(wheelValue, index) :
                this->setSelectStart(index);
    }
    if (noteChanged) {
        this->playSelection();
    }
}
    
void NoteTaker::updateVertical() {
    const int wheelValue = (int) verticalWheel->value;
    if (wheelValue == lastVertical) {
        return;
    }
    lastVertical = wheelValue;
    if (fileButton->ledOn) {
        if (verticalWheel->value <= 2) {
            saving = true;
            this->saveScore();
        }
        if (verticalWheel->value >= 8 && (unsigned) horizontalWheel->value < storage.size()) {
            debug("updateVertical loadScore");
            loading = true;
            this->loadScore();
        }
        return;
    }
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[this->firstChannel()];
        int sustainDuration = INT_MAX;
        switch(wheelValue) {
            case 3: // to do : create enum to identify values
                sustainDuration = channel.sustainMax;
                break;
            case 2:
                sustainDuration = channel.sustainMin;
                break;
            case 1:
                sustainDuration = channel.releaseMin;
                break;
            case 0:
                sustainDuration = channel.releaseMax;
                break;
            default:
                assert(0);
        }
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(sustainDuration));
    }
    if (isEmpty()) {
        return;
    }
    if (runButton->running()) {
        // to do : if running, transpose all up/down
        return;
    }
    if (partButton->ledOn) {
        unsigned value = (unsigned) wheelValue;
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
            value = wheelValue;
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
    if (MIDI_HEADER == note.type) { 
        if (allOutputsOff) {
            return;
        }
    } else if (this->noteIndex(note) == channels[note.channel].noteIndex) {
        return;
    }
    assert((0xFF == note.channel && MIDI_HEADER == note.type) || note.channel < CHANNEL_COUNT);
    if (NOTE_ON == note.type) {
        channels[note.channel].expiration = note.startTime
                + channels[note.channel].sustain(note.duration);
        debug("outputNote time=%d dur=%d sustain=%d expr=%d", note.startTime, note.duration,
                channels[note.channel].sustain(note.duration), channels[note.channel].expiration);
        if (note.channel < CV_OUTPUTS) {
            outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
            float v_oct = inputs[V_OCT_INPUT].value;
            outputs[CV1_OUTPUT + note.channel].value = v_oct + note.pitch() / 12.f;
        }
        channels[note.channel].noteIndex = this->noteIndex(note);
        allOutputsOff = false;
   } else {
        assert((MIDI_HEADER == note.type && selectButton->editStart()) || REST_TYPE == note.type);
        if (0xFF == note.channel) {
            this->zeroGates();
        } else {
            if (note.channel < CV_OUTPUTS) {
                outputs[GATE1_OUTPUT + note.channel].value = 0; // rest
            }
            channels[note.channel].noteIndex = this->noteIndex(note);
        }
    }
    display->dirty = true;
}

void NoteTaker::playSelection() {
    this->zeroGates();
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime, ppq, tempo);
    playingSelection = true;
}

void NoteTaker::resetButtons() {
    cutButton->reset();
    fileButton->reset();
    insertButton->reset();
    partButton->reset();
    restButton->reset();
    runButton->reset();
    selectButton->reset();
    sustainButton->reset();
    timeButton->reset();
}

void NoteTaker::setDisplayEnd() {
    displayEnd = this->lastAt(allNotes[displayStart].startTime
            + display->box.size.x / display->xAxisScale - display->xAxisOffset);
}

bool NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
    debug("setSelectEnd wheelValue=%d end=%u singlePos=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->singlePos, selectStart, selectEnd);
    if (end < selectButton->singlePos) {
        selectStart = end;
        selectEnd = selectButton->singlePos;
        debug("setSelectEnd < s:%u e:%u", selectStart, selectEnd);
   } else if (end == selectButton->singlePos) {
        selectStart = selectButton->singlePos;
        if (TRACK_END == allNotes[selectStart].type) {
            selectEnd = selectStart;
            selectStart = isEmpty() ? 0 : wheelToNote(wheelValue - 1);
        } else {
            selectEnd = this->wheelToNote(wheelValue + 1);
        }
        debug("setSelectEnd == s:%u e:%u", selectStart, selectEnd);
    } else {
        selectStart = selectButton->singlePos;
        selectEnd = end;
        debug("setSelectEnd > s:%u e:%u", selectStart, selectEnd);
    }
    assert(selectEnd != selectStart);
    return true;
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
    if (!fileButton) {
        return;
    }
    if (fileButton->ledOn) {
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        verticalWheel->speed = 1;
        debug("horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
                horizontalWheel->maxValue);
        debug("vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
                verticalWheel->maxValue);
        return;
    }
    if (sustainButton->ledOn) {
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
        int sustainMinDuration = channels[this->firstChannel()].sustainMin;
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(sustainMinDuration));
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->speed = 1;
        verticalWheel->setValue(2.5f); // sustain min
        return;
    }
    if (isEmpty()) {
        return;
    }
    verticalWheel->speed = .1;
    // horizontal wheel range and value
    int wheelMin = selectButton->editStart() ? -1 : 0;
    int wheelMax = (int) this->horizontalCount() + wheelMin;
    const DisplayNote* note = &allNotes[selectStart];
    if (!selectButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
    } else {
        horizontalWheel->setLimits(wheelMin, wheelMax);
    }
    if (!selectButton->ledOn) {
        horizontalWheel->setValue(REST_TYPE == note->type ? note->rest() :note->note());
    } else {
        int index = this->noteToWheel(*note);
        if (index < wheelMin || index > wheelMax) {
            debug("! note type %d index %d wheelMin %d wheelMax %d",
                    note->type, index, wheelMin, wheelMax);
            this->debugDump();
            assert(0);
        }
        horizontalWheel->setValue(index);
    }
    // vertical wheel range 0 to 127 for midi pitch
    debug("setWheelRange partButton->ledOn %d selectButton->ledOn %d selectButton->state %d",
            partButton->ledOn, selectButton->ledOn, selectButton->state);
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
    if (loading || saving) {
        float val = verticalWheel->value;
        verticalWheel->value += val > 5 ? -.0001f : +.0001f;
        if (4.9999f < val && val < 5.0001f) {
            loading = false;
            saving = false;
        }
        return;
    }
    if (!runButton || (!runButton->running() && !playingSelection) || this->isEmpty()) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
	float deltaTime = engineGetSampleTime();
    elapsedSeconds += deltaTime;
    int midiTime = SecondsToMidi(elapsedSeconds, ppq, tempo);
    if (runButton->running()) {
        this->setExpiredGatesLow(midiTime);
        // to do : if midi time exceeds displayEnd: scroll; reset displayStart, displayEnd
        // if midi time exceeds last of allNotes: either reset to zero and repeat,
        // or stop running
        bool recomputeDisplayEnd = false;
        if (this->lastNoteEnded(displayEnd, midiTime)) {
            elapsedSeconds = 0;  // to do : don't repeat unconditionally?
            midiTime = 0;
            displayStart = this->wheelToNote(0);
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
