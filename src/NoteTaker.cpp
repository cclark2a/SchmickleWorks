#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include <limits.h>

// todo:
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    gateOut.fill(0);
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
}

// to compute range for horizontal wheel when selecting notes
int NoteTaker::horizontalCount() const {
    int count = 0;
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

int NoteTaker::noteIndex(const DisplayNote& match) const {
    if (MIDI_HEADER == match.type) {
        return -1;
    }
    int count = 0;
    for (auto& note : allNotes) {
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (&match == &note) {
            return count;
        }
        ++count;
    }
    assert(0);
}

void NoteTaker::updateHorizontal() {
    if (running) {
        // to do : while running, change tempo
        return;
    }
    int wheelValue = (int) horizontalWheel->value;
    bool noteChanged = false;
    if (durationButton->ledOn) {
        int diff = 0;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
            if (note.isSelectable(selectChannels)) {
                assert((unsigned) wheelValue < noteDurations.size());
                note.setNote(wheelValue);
                int duration = noteDurations[wheelValue];
                diff += duration - note.duration;
                note.duration = duration;
            }
        }
        if (diff) {
            for (unsigned index = selectEnd; index < allNotes.size(); ++index) {
                DisplayNote& note = allNotes[index];
                note.startTime += diff;
            }
            noteChanged = true;
        }
    } else {
    // value should range from 0 to max - 1, where max is number of starts for active channels
    // if insert mode, value ranges from 0 to max
        int lastTime = -1;
        for (auto& note : allNotes) {
            if (!note.isSelectable(selectChannels)) {
                continue;
            }
            if (wheelValue <= 0) {
                unsigned index;
                if (wheelValue < 0) {
                    assert(insertButton->ledOn);
                    index = 0;
                } else {
                    index = &note - &allNotes.front();
                }
                noteChanged = selectButton->ledOn ? setSelectEnd(index) : setSelectStart(index);
                break;
            }
            if (lastTime == note.startTime) {
                continue;
            }
            --wheelValue;
            lastTime = note.startTime;
        }
    }
    if (noteChanged) {
        this->playSelection();
    }
}
    
void NoteTaker::updateVertical() {
    // to do : if running, transpose all up/down
    array<DisplayNote*, channels> lastNote;
    lastNote.fill(nullptr);
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (note.isSelectable(selectChannels)) {
            note.setPitch((int) verticalWheel->value);
            lastNote[note.channel] = &note;
        }
    }
    for (auto note : lastNote) {
        if (nullptr == note) {
            continue;
        }
        this->outputNote(*note);
        this->playSelection();
    }
}

void NoteTaker::outputNote(const DisplayNote& note) {
    unsigned noteIndex = &note - &allNotes.front();
    assert(note.channel < CV_OUTPUTS);
    gateOut[note.channel] = noteIndex;
    if (NOTE_ON == note.type) {
        outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
	    float v_oct = inputs[V_OCT_INPUT].value;
        outputs[CV1_OUTPUT + note.channel].value = v_oct + note.pitch() / 12.f;
    } else {
        assert((MIDI_HEADER == note.type && insertButton->ledOn) || REST_TYPE == note.type);
        outputs[GATE1_OUTPUT + note.channel].value = 0;
    }
}

void NoteTaker::playSelection() {
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime);
    playingSelection = true;
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
    if (!insertButton) {
        return;
    }
    // horizontal wheel range and value
    int selectMin = insertButton->ledOn ? -1 : 0;
    int selectMax = this->horizontalCount();
    if (durationButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size());
    } else {
        horizontalWheel->setLimits(selectMin, selectMax);
    }
    const DisplayNote* note = &allNotes[selectStart];
    int index = this->noteIndex(*note);
    debug("note type %d index %d selectMin %d", note->type, index, selectMin);
    if (index < selectMin) {
        setSelectStartAt(0, 0, allNotes.size());
        note = &allNotes[selectStart];
        index = this->noteIndex(*note);
        debug("note type %d index %d", note->type, index);
    }
    if (index >= selectMax) {
        setSelectStartAt(INT_MAX, 0, allNotes.size());
        note = &allNotes[selectStart];
        index = this->noteIndex(*note);
    }
    if (durationButton->ledOn) {
        horizontalWheel->setValue(note->note());
    } else {
        horizontalWheel->setValue(index);
    }
    // vertical wheel range 0 to 127 for midi pitch
    if (MIDI_HEADER != note->type) {
        verticalWheel->setValue(note->pitch());
    }
    debug("horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
}

void NoteTaker::step() {
    if (!display) {
        return;
    }   
	if (runningTrigger.process(params[RUN_BUTTON].value)) {
		running = !running;
        if (!running) {
            for (unsigned index = 0; index < channels; ++index) {
                gateOut[index] = 0;
            }
            for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
                outputs[GATE1_OUTPUT + index].value = 0;
            }
        }
        lights[RUNNING_LIGHT].value = running;
	}
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
	float deltaTime = engineGetSampleTime();
    elapsedSeconds += deltaTime;
    int midiTime = SecondsToMidi(elapsedSeconds);
    if (running) {
        // to do: use info in midi header, time signature, tempo to get this right
        this->setExpiredGatesLow(midiTime);
        // if midi time exceeds displayEnd: scroll; reset displayStart, displayEnd
        // if midi time exceeds last of allNotes: either reset to zero and repeat, or stop running
        if (this->lastNoteEnded(displayEnd, midiTime)) {
            elapsedSeconds = 0;  // to do : don't repeat unconditionally?
            midiTime = 0;
            displayStart = this->firstOn();
            if (!displayStart) { // no notes to play
                running = false;
                lights[RUNNING_LIGHT].value = false;
                return;
            }
            displayEnd = 0;  // recompute display end
        } else {
            do {
                const DisplayNote& note = allNotes[displayStart];
                if (note.startTime + note.duration > midiTime || TRACK_END != note.type) {
                    break;
                }
                ++displayStart;
                displayEnd = 0;  // recompute display end
            } while (true);
        }
        if (!displayEnd) {
            // todo: allow time range to display to be scaled from box width
            // calc here must match NoteTakerDisplay::draw:75
            displayEnd = this->lastAt(allNotes[displayStart].startTime + display->box.size.x * 4 - 32);
        }
        this->setSelectStartAt(midiTime, displayStart, displayEnd);
    } else if (playingSelection) { // not running, play note after selecting or editing
        for (unsigned step = selectStart; step < selectEnd; ++step) {
            const DisplayNote& note = allNotes[step];
            static unsigned debugLast = INT_MAX;
            if (note.isSelectable(selectChannels) && note.isActive(midiTime)) {
                this->outputNote(note);
                if (step != debugLast) {
                    debugLast = step;
                }
            } else if (!step) {
                assert(insertButton->ledOn);
                this->outputNote(note);
            }
        }
        if (this->lastNoteEnded(selectEnd, midiTime)) {
            playingSelection = false;
            this->setExpiredGatesLow(INT_MAX);
        }
    }
}
