#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"

// to do:
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior
    //   when user clicks these from the context menu

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    this->reset();
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync2.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    this->readStorage();
}

float NoteTaker::beatsPerHalfSecond() const {
    float deltaTime = stdMSecsPerQuarterNote / tempo;
    if (this->isRunning() && !fileButton->ledOn) {
        // to do : rework so that tempo accels / decels elapsedSeconds
        int horzIndex = (int) horizontalWheel->value;
        int floor = noteDurations[horzIndex];
        int ceil = noteDurations[horzIndex + 1];
        float interp = floor + (ceil - floor) * (horizontalWheel->value - horzIndex);
        deltaTime *= stdTimePerQuarterNote / interp;
    }
    return deltaTime;
}

// to compute range for horizontal wheel when selecting notes
// to do : horizontalCount, noteToWheel, wheelToNote share loop logic. Consolidate?
unsigned NoteTaker::horizontalCount() const {
    unsigned count = 0;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (!this->isSelectable(note)) {
            debug("! selectable %s", note.debugString().c_str());
            continue;
        }
        if (lastTime == note.startTime && NOTE_ON == note.type && NOTE_ON == lastType) {
            debug("== lastTime %s", note.debugString().c_str());
            continue;
        }
        ++count;
        lastTime = note.startTime;
        lastType = note.type;
    }
    debug("horizontal count %u", count);
    return count;
}

unsigned NoteTaker::isBestSelectStart(int midiTime) const {
    unsigned startIndex = playingSelection ? selectStart : 0;
    unsigned endIndex = playingSelection ? selectEnd : allNotes.size() - 1;
    int start = INT_MAX;
    const DisplayNote* best = nullptr;
    for (unsigned step = startIndex; step < endIndex; ++step) {
        const DisplayNote& note = allNotes[step];
        if (NOTE_ON != note.type) {
            continue;
        }
        if (note.isBestSelectStart(&best, midiTime)) {
            start = step;
        }
    }
    return start;
}

bool NoteTaker::isEmpty() const {
    for (auto& note : allNotes) {
        if (this->isSelectable(note)) {
            return false;
        }
    }
    return true;
}

bool NoteTaker::isRunning() const {
    return runButton->ledOn;
}

bool NoteTaker::isSelectable(const DisplayNote& note) const {
    return note.isSelectable(selectButton->ledOn ? ALL_CHANNELS : selectChannels);
}

void NoteTaker::loadScore() {
    this->reset();
    unsigned index = (unsigned) horizontalWheel->value;
    assert(index < storage.size());
    NoteTakerParseMidi parser(storage[index], allNotes, channels);
    if (!parser.parseMidi()) {
        this->setScoreEmpty();
    }
    display->xPositionsInvalid = true;
    if (!this->isEmpty()) {
        this->setSelectStart(this->wheelToNote(0));
    }
}

int NoteTaker::noteToWheel(const DisplayNote& match) const {
    if (MIDI_HEADER == match.type) {
        return -1;
    }
    int count = 0;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (&match == &note) {
            return count;
        }
        if (!this->isSelectable(note)) {
            continue;
        }
        if (lastTime == note.startTime && NOTE_ON == lastType && NOTE_ON == note.type) {
            continue;
        }
        ++count;
        lastTime = note.startTime;
        lastType = note.type;
    }
    assert(0);
}

void NoteTaker::outputNote(const DisplayNote& note, int midiTime) {
    auto& channelInfo = channels[note.channel];
    if (this->noteIndex(note) == channelInfo.noteIndex) {
        if (midiTime < channelInfo.noteEnd) {
            return;
        }
    }
    assert(note.channel < CHANNEL_COUNT);
    channelInfo.gateLow = note.startTime + channelInfo.sustain(note.duration);
    channelInfo.noteEnd = note.endTime();
#if 0 // avoid this since it slows down critical step time
    debug("outputNote time=%d dur=%d sustain=%d gateLow=%d midiTime=%d"
            " noteIndex=%d channelInfo.noteIndex=%d channelInfo.noteEnd=%d"
            " verticalWheel->value=%g",
            note.startTime, note.duration, channelInfo.sustain(note.duration), channelInfo.gateLow,
            midiTime, 
            this->noteIndex(note), channelInfo.noteIndex, channelInfo.noteEnd,
            verticalWheel->value);
    debug("y horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("y vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
#endif
    if (note.channel < CV_OUTPUTS) {
        outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
        const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
        float v_oct = inputs[V_OCT_INPUT].value;
        if (this->isRunning() && !fileButton->ledOn) {
            v_oct += (verticalWheel->wheelValue() - 60) / 12.f;
        }
        outputs[CV1_OUTPUT + note.channel].value = bias + v_oct + note.pitch() / 12.f;
    }
    channelInfo.noteIndex = this->noteIndex(note);
}

void NoteTaker::outputNoteAt(int midiTime) {
    unsigned start = this->isBestSelectStart(midiTime);
    if (INT_MAX == start) {
        return;
    }
    const auto& note = allNotes[start];
    if (!note.isActive(midiTime) || NOTE_ON != note.type) {
        return;
    }
    this->outputNote(note, midiTime);
}

void NoteTaker::playSelection() {
    this->zeroGates();
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime, ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    playingSelection = true;
}

void NoteTaker::reset() {
    debug("notetaker reset");
    allNotes.clear();
    clipboard.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    displayStart = displayEnd = selectStart = selectEnd = 0;
    elapsedSeconds = 0;
    playingSelection = false;
    selectChannels = ALL_CHANNELS;
    this->zeroGates();
    Module::reset();
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
    horizontalWheel->reset();
    verticalWheel->reset();
}

void NoteTaker::saveScore() {
    unsigned index = (unsigned) horizontalWheel->value;
    assert(index <= storage.size());
    if (storage.size() == index) {
        storage.push_back(vector<uint8_t>());
    }
    auto& dest = storage[index];
    NoteTakerMakeMidi midiMaker;
    midiMaker.createFromNotes(*this, dest);
    this->writeStorage(index);
}

void NoteTaker::setScoreEmpty() {
    this->reset();
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, allNotes, channels);
    bool success = emptyParser.parseMidi();
    assert(success);
    this->resetButtons();
    display->xPositionsInvalid = true;
    display->updateXPosition();
}

void NoteTaker::setSelect(unsigned start, unsigned end) {
    if (this->isEmpty()) {
        selectStart = selectEnd = displayStart = displayEnd = 0;
        return;
    }
    assert(start < end);
    assert(allNotes.size() >= 2);
    assert(end <= allNotes.size() - 1);
    display->updateXPosition();
    int selectStartTime = display->startTime(start);
    int selectEndTime = display->endTime(end - 1);
    if (!displayEnd || allNotes.size() <= displayEnd) {
        displayEnd = allNotes.size() - 1;
    }
    if (displayEnd <= displayStart) {
        displayStart = displayEnd - 1;
    }
    int displayStartTime = display->startTime(displayStart);
    int displayEndTime = displayStartTime + display->box.size.x;
    while (displayEnd < allNotes.size() - 1 && display->endTime(displayEnd) < displayEndTime) {
        ++displayEnd;
    }
    debug("selectStartTime %d selectEndTime %d displayStartTime %d displayEndTime %d",
            selectStartTime, selectEndTime, displayStartTime, displayEndTime);
    debug("setSelect displayStart %u displayEnd %u", displayStart, displayEnd);
    bool recomputeDisplayOffset = display->xPos(displayStart) < 0
            || display->xPos(displayEnd) > display->box.size.x;
    debug("display->xPos(displayStart) %d display->xPos(displayEnd) %d display->box.size.x %g",
            display->xPos(displayStart), display->xPos(displayEnd), display->box.size.x);
    bool offsetFromEnd = false;
    if (displayStartTime > selectStartTime || selectEndTime > displayEndTime) {
        recomputeDisplayOffset = true;
        // scroll small selections to center?
        // move larger selections the smallest amount?
        // prefer to show start? end?
        // while playing, scroll a 'page' at a time?
        // allow for smooth scrolling?
        bool recomputeStart = false;
        if (selectEnd != end && selectStart == start) { // only end moved
            if (displayStartTime > selectEndTime || selectEndTime > displayEndTime) {
                displayEnd = end;  // if end isn't visible
                displayStart = displayEnd - 1;
                recomputeStart = true;
                debug("1 displayStart %u displayEnd %u", displayStart, displayEnd);
            }
        } else if (displayStartTime > selectStartTime || selectStartTime > displayEndTime
                || ((displayStartTime > selectEndTime || selectEndTime > displayEndTime)
                && display->duration(start) < display->box.size.x)) {
            if (display->endTime(start) > displayEndTime) {
                displayEnd = start + 1;
                displayStart = start;
                recomputeStart = true;
                offsetFromEnd = true;
                debug("2 displayStart %u displayEnd %u", displayStart, displayEnd);
            } else if (selectStartTime < displayStartTime) {
                displayStart = start;   // if start isn't fully visible
                displayEnd = display->lastAt(start);
                debug("3 displayStart %u displayEnd %u", displayStart, displayEnd);
            }
        }
        if (recomputeStart) {
            int endTime = display->endTime(displayStart);
            while (displayStart
                    && endTime - display->startTime(displayStart) < display->box.size.x) {
                --displayStart;
            }
            if (displayStart < allNotes.size() - 1
                    && endTime - display->startTime(displayStart) > display->box.size.x) {
                ++displayStart;
            }
            debug("displayStart %u displayEnd %u endTime %d boxWidth %g xPos %d",
                    displayStart, displayEnd, endTime, display->box.size.x,
                    display->startTime(displayStart));
        }
    }
    if (recomputeDisplayOffset) {
        if (offsetFromEnd) {
            display->xAxisOffset = std::max(display->endTime(start)
                    + (selectButton->editStart() ? 8 : 0) - display->box.size.x, 0.f);
        } else if (display->startTime(displayEnd) <= display->box.size.x) {
            display->xAxisOffset = 0;
        } else {
            display->xAxisOffset = display->startTime(displayStart);
        }
        debug("display->xAxisOffset %g", display->xAxisOffset);
    }
    debug("setSelect old %u %u new %u %u", selectStart, selectEnd, start, end);
    selectStart = start;
    selectEnd = end;
}

bool NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
    debug("setSelectEnd wheelValue=%d end=%u singlePos=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->singlePos, selectStart, selectEnd);
    if (end < selectButton->singlePos) {
        this->setSelect(end, selectButton->singlePos);
        debug("setSelectEnd < s:%u e:%u", selectStart, selectEnd);
   } else if (end == selectButton->singlePos) {
        unsigned start = selectButton->singlePos;
        unsigned end;
        if (TRACK_END == allNotes[start].type) {
            end = start;
            start = isEmpty() ? 0 : wheelToNote(wheelValue - 1);
        } else {
            end = this->wheelToNote(wheelValue + 1);
        }
        this->setSelect(start, end);
        debug("setSelectEnd == s:%u e:%u", selectStart, selectEnd);
    } else {
        this->setSelect(selectButton->singlePos, end);
        debug("setSelectEnd > s:%u e:%u", selectStart, selectEnd);
    }
    assert(selectEnd != selectStart);
    return true;
}

bool NoteTaker::setSelectStart(unsigned start) {
    if (selectStart == start) {
        return false;
    }
    unsigned end = start;
    do {
        ++end;
    } while (allNotes[start].startTime == allNotes[end].startTime && NOTE_ON == allNotes[start].type
            && NOTE_ON == allNotes[end].startTime);
    this->setSelect(start, end);
    return true;
}

void NoteTaker::setSelectStartAt(int midiTime) {
    unsigned start = this->isBestSelectStart(midiTime);
    if (INT_MAX != start) {
        (void) this->setSelectStart(start);
    }
}

void NoteTaker::step() {
    realSeconds += engineGetSampleTime();
    if (!runButton) {
        return;  // do nothing if we're not set up yet
    }
    if ((!this->isRunning() && !playingSelection) || this->isEmpty()) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    elapsedSeconds += engineGetSampleTime() * this->beatsPerHalfSecond();
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->setExpiredGatesLow(midiTime);
    unsigned lastNote = playingSelection ? selectEnd : allNotes.size() - 1;
    if (this->lastNoteEnded(lastNote - 1, midiTime)) {
        if (playingSelection) {
            playingSelection = false;
            this->setExpiredGatesLow(INT_MAX);
        } else {
            // to do : add option to stop running
            elapsedSeconds = 0;
            midiTime = 0;
            displayStart = 0;
        }
        return;
    }
    if (this->isRunning()) {
        this->setSelectStartAt(midiTime);
    }
    this->outputNoteAt(midiTime);
}

unsigned NoteTaker::wheelToNote(int value) const {
    debug("wheelToNote start %d", value);
    this->debugDump(false);
    if (value < 0) {
        assert(selectButton->editStart());
        return 0;
    }
    int count = value;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (!this->isSelectable(note)) {
            continue;
        }
        if (count <= 0) {
            return this->noteIndex(note);
        }
        if (lastTime == note.startTime && NOTE_ON == lastType && NOTE_ON == note.type) {
            continue;
        }
        --count;
        lastTime = note.startTime;
        lastType = note.type;
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
