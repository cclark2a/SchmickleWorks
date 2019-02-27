#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"

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
//    this->setUpSampleNotes();
    this->readStorage();
}

// to compute range for horizontal wheel when selecting notes
unsigned NoteTaker::horizontalCount() const {
    unsigned count = 0;
    int lastTime = -1;
    for (auto& note : allNotes) {
        if (!this->isSelectable(note)) {
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
        if (this->isSelectable(note)) {
            return false;
        }
    }
    return true;
}

bool NoteTaker::isSelectable(const DisplayNote& note) const {
    return note.isSelectable(selectButton->ledOn ? ALL_CHANNELS : selectChannels);
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
        if (!this->isSelectable(note)) {
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
        if (!this->isSelectable(note)) {
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
    this->writeStorage(index);
}

void NoteTaker::setUpSampleNotes() {
    vector<uint8_t> midi;
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
    channels[0].sustainMin = 1;
    channels[0].sustainMax = noteDurations.back();
    channels[0].releaseMin = 1;
    channels[0].releaseMax = 24;
}

void NoteTaker::outputNote(const DisplayNote& note, int midiTime) {
    auto& channelInfo = channels[note.channel];
    if (MIDI_HEADER == note.type) { 
        if (allOutputsOff) {
            return;
        }
    } else if (this->noteIndex(note) == channelInfo.noteIndex) {
        if (midiTime < channelInfo.noteEnd) {
            return;
        }
    }
    assert((0xFF == note.channel && MIDI_HEADER == note.type) || note.channel < CHANNEL_COUNT);
    if (NOTE_ON == note.type) {
        channelInfo.gateLow = note.startTime + channelInfo.sustain(note.duration);
        channelInfo.noteEnd = note.endTime();
        debug("outputNote time=%d dur=%d sustain=%d gateLow=%d midiTime=%d"
                " noteIndex=%d channelInfo.noteIndex=%d", note.startTime,
                note.duration, channelInfo.sustain(note.duration), channelInfo.gateLow,
                midiTime, this->noteIndex(note), channelInfo.noteIndex);
        if (note.channel < CV_OUTPUTS) {
            outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
            const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            float v_oct = inputs[V_OCT_INPUT].value;
            outputs[CV1_OUTPUT + note.channel].value = bias + v_oct + note.pitch() / 12.f;
        }
        channelInfo.noteIndex = this->noteIndex(note);
        allOutputsOff = false;
   } else {
        assert((MIDI_HEADER == note.type && selectButton->editStart()) || REST_TYPE == note.type);
        this->zeroGates();
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
        if (NOTE_ON == note.type) {
            if (note.isBestSelectStart(&best, midiTime)) {
                start = step;
            }
            if (note.isActive(midiTime)) {
                this->outputNote(note, midiTime);
            }
        }
    }
    if (INT_MAX != start) {
        (void) this->setSelectStart(start);
    }
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
            displayStart = 0;
            recomputeDisplayEnd = true;
        } else {
            do {
                const DisplayNote& note = allNotes[displayStart];
                if (note.endTime() > midiTime || TRACK_END != note.type) {
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
            this->outputNote(allNotes[step], midiTime);
            if (step + 1 == selectEnd && this->lastNoteEnded(step, midiTime)) {
                playingSelection = false;
                this->setExpiredGatesLow(INT_MAX);
            }
        }
    }
}
