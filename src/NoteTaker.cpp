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
    unsigned count = -1;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (TRACK_END == note.type) {
            break;
        }
        if (NOTE_ON != note.type || NOTE_ON != lastType ||
                (this->isSelectable(note) && lastTime != note.startTime)) {
            ++count;
        }
        lastTime = note.startTime;
        lastType = note.type;
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

bool NoteTaker::isRunning() const {
    return runButton->ledOn;
}

bool NoteTaker::isSelectable(const DisplayNote& note) const {
    return note.isSelectable(selectChannels);
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
        this->setSelectStart(this->wheelToNote(1));
    }
}

int NoteTaker::noteToWheel(const DisplayNote& match, bool dbug) const {
    int count = -1;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (NOTE_ON != note.type || NOTE_ON != lastType ||
                (this->isSelectable(note) && lastTime != note.startTime)) {
            ++count;
        }
        if (&match == &note) {
            return count;
        }
        lastTime = note.startTime;
        lastType = note.type;
    }
    if (dbug) {
        debug("noteToWheel match %s", match.debugString().c_str());
        debugDump(false, true);
        assert(0);
    }
    return -1;
}

void NoteTaker::playSelection() {
    this->zeroGates();
    elapsedSeconds = MidiToSeconds(allNotes[selectStart].startTime, ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    playStart = 0;
    this->setPlayStart();
}

// to do : could optimize with binary search
void NoteTaker::setPlayStart() {
    this->zeroGates();
    if (playStart >= allNotes.size()) {
        return;
    }
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->advancePlayStart(midiTime, INT_MAX);
}

void NoteTaker::reset() {
    debug("notetaker reset");
    allNotes.clear();
    clipboard.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    displayStart = displayEnd = selectStart = selectEnd = 0;
    this->resetRun();
    selectChannels = ALL_CHANNELS;
    this->setScoreEmpty();
    Module::reset();
}

void NoteTaker::resetControls() {
    this->resetLedButtons();
    for (NoteTakerButton* button : {
            (NoteTakerButton*) cutButton, (NoteTakerButton*) insertButton,
            (NoteTakerButton*) restButton, (NoteTakerButton*) timeButton }) {
        if (button) {
            button->reset();
        }
    }
    if (horizontalWheel) {
        horizontalWheel->reset();
    }
    if (verticalWheel) {
        verticalWheel->reset();
    }
    if (display) {
        display->reset();
    }
}

void NoteTaker::resetLedButtons(const NoteTakerButton* exceptFor) {
    for (NoteTakerButton* button : {
                (NoteTakerButton*) fileButton, (NoteTakerButton*) partButton,
                (NoteTakerButton*) runButton, (NoteTakerButton*) selectButton,
                (NoteTakerButton*) sustainButton }) {
        if (button && exceptFor != button) {
            button->reset();
        }
    }
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
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, allNotes, channels);
    bool success = emptyParser.parseMidi();
    assert(success);
    this->resetControls();
    if (display) {
        display->updateXPosition();
    }
}

void NoteTaker::setSelect(unsigned start, unsigned end) {
    display->updateXPosition();
    if (this->isEmpty()) {
        selectStart = selectEnd = displayStart = displayEnd = 0;
        return;
    }
    assert(start < end);
    assert(allNotes.size() >= 2);
    assert(end <= allNotes.size() - 1);
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

void NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
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

// since this runs on a high frequency thread, avoid state except to play notes
void NoteTaker::step() {
    realSeconds += engineGetSampleTime();
    if (!playStart) {
        return;  // do nothing if we're not set up yet
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    elapsedSeconds += engineGetSampleTime() * this->beatsPerHalfSecond();
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->setExpiredGatesLow(midiTime);
    bool running = this->isRunning();
    unsigned lastNote = running ? allNotes.size() - 1 : selectEnd - 1;
    if (this->advancePlayStart(midiTime, lastNote)) {
        playStart = 0;
        if (running) {
            // to do : add option to stop running
            this->resetRun();
        } else {
            this->setExpiredGatesLow(INT_MAX);
        }
        return;
    }
    unsigned start = playStart - 1;
    unsigned selStart = INT_MAX;
    unsigned selEnd = 0;
    while (++start <= lastNote) {
        const auto& note = allNotes[start];
        if (note.startTime > midiTime) {
            break;
        }
        if (NOTE_ON != note.type) {
            continue;
        }
        auto& channelInfo = channels[note.channel];
        unsigned noteIndex = this->noteIndex(note);
        if (noteIndex == channelInfo.noteIndex) {
            if (midiTime < channelInfo.noteEnd) {
                continue;
            }
        } else {
            channelInfo.gateLow = note.startTime + channelInfo.sustain(note.duration);
            channelInfo.noteEnd = note.endTime();
            channelInfo.noteIndex = noteIndex;
            if (note.channel < CV_OUTPUTS) {
                outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
            }
        }
        if (running) {
            selStart = std::min(selStart, start);
            selEnd = std::max(selEnd, start + 1);
        }
        // recompute pitch all the time to prepare for tremelo / vibrato / slur / etc
        if (note.channel < CV_OUTPUTS) {
            const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            float v_oct = inputs[V_OCT_INPUT].value;
            if (this->isRunning() && !fileButton->ledOn) {
                v_oct += (verticalWheel->wheelValue() - 60) / 12.f;
            }
            outputs[CV1_OUTPUT + note.channel].value = bias + v_oct + note.pitch() / 12.f;
        }
    }
    if (running) {
        // to do : don't write to same state in different threads
        // to do : select selStart through selEnd, move displayStart / end calc to display thread
        if (INT_MAX != selStart) {
            (void) this->setSelectStart(selStart);
        }
    }
}

// counts to nth selectable note; returns index into notes array
unsigned NoteTaker::wheelToNote(int value, bool dbug) const {
    assert(!dbug || value > 0 || selectButton->editStart());
    assert(!dbug || value < (int) allNotes.size());
    int count = value;
    int lastTime = -1;
    DisplayType lastType = UNUSED;
    for (auto& note : allNotes) {
        if (NOTE_ON != note.type || NOTE_ON != lastType ||
                (this->isSelectable(note) && lastTime != note.startTime)) {
            if (--count < 0) {
                return this->noteIndex(note);
            }
        }
        lastTime = note.startTime;
        lastType = note.type;
    }
    if (dbug) {
        debug("wheelToNote value %d", value);
        this->debugDump(false, true);
        assert(0);  // probably means wheel range is larger than selectable note count
    }
    return (unsigned) -1;
}
