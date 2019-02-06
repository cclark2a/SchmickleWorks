#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"

// todo:
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    gateOut.fill(0);
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
}

// to compute range for horizontal wheel when selecting notes
int NoteTaker::horizontalCount(const DisplayNote& match, int* index) const {
    int count = 0;
    int lastTime = -1;
    *index = -1;
    for (auto& note : allNotes) {
        if (&match == &note) {
            *index = count;
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
    assert(-1 != *index);
    return count;
}

void NoteTaker::updateHorizontal() {
    if (running) {
        // to do : while running, change tempo
        return;
    }
    unsigned wheelValue = (unsigned) (int) horizontalWheel->value;
    bool noteChanged = false;
    if (durationButton->ledOn) {
        int diff = 0;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
            if (note.isSelectable(selectChannels)) {
                assert(wheelValue < noteDurations.size());
                note.setNote((int) wheelValue);
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
            if (wheelValue == 0) {
                unsigned noteIndex = &note - &allNotes.front();
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
    cvOut[note.channel] = noteIndex;
    gateOut[note.channel] = noteIndex;
    if (NOTE_ON == note.type) {
        outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
        assert(note.channel < channels);
	    float v_oct = inputs[V_OCT_INPUT].value;
        outputs[CV1_OUTPUT + note.channel].value = v_oct + note.pitch() / 12.f;
    } else {
        assert(REST_TYPE == note.type);
        outputs[GATE1_OUTPUT + note.channel].value = 0;
    }
}

void NoteTaker::playSelection() {
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime);
    playingSelection = true;
}

void NoteTaker::setWheelRange() {
    const DisplayNote& note = allNotes[selectStart];
    // horizontal wheel range and value
    if (durationButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size());
        horizontalWheel->setValue(note.note());
    } else {
        int index;
        int count = this->horizontalCount(note, &index);
        horizontalWheel->setLimits(0, count);
        horizontalWheel->setValue(index);

    }
    // vertical wheel range 0 to 127 for midi pitch
    verticalWheel->setValue(note.pitch());
    debug("horz %g (%g %g)\n", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("vert %g (%g %g)\n", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
}

void NoteTaker::step() {
	if (runningTrigger.process(params[RUN_BUTTON].value)) {
		running = !running;
        if (!running) {
            this->setWheelRange();
            for (unsigned channel = 0; channel < channels; ++channel) {
                gateOut[channel] = 0;
                outputs[GATE1_OUTPUT + channel].value = 0;
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
        int start = INT_MAX;
        if (!displayEnd) {
            // todo: allow time range to display to be scaled from box width
            // calc here must match NoteTakerDisplay::draw:75
            displayEnd = this->lastAt(allNotes[displayStart].startTime + display->box.size.x * 4 - 32);
        }
        const DisplayNote* best = nullptr;
        for (unsigned step = displayStart; step < displayEnd; ++step) {
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
        (void) this->setSelectStart(start);
    } else if (playingSelection) { // not running, play note after selecting or editing
        for (unsigned step = selectStart; step < selectEnd; ++step) {
            const DisplayNote& note = allNotes[step];
            if (note.isSelectable(selectChannels) && note.isActive(midiTime)) {
                this->outputNote(note);
            }
        }
        if (this->lastNoteEnded(selectEnd, midiTime)) {
            debug("step playingSelection = false");
            playingSelection = false;
            this->setExpiredGatesLow(INT_MAX);
        }
    }
}
