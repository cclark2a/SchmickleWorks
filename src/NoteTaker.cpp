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
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync3.ttf"));
}

float NoteTaker::beatsPerHalfSecond(int localTempo) const {
    float deltaTime = (float) stdMSecsPerQuarterNote / localTempo;
    if (this->runningWithButtonsOff()) {
        // to do : decide how external clock works
        // external clock input could work in one of three modes:
        // 1/2) input voltage overrides / multiplies wheel value
        //   3) voltage step marks beat
        // for 3), second step determines bphs -- tempo change always lags 1 beat
        // playback continues for one beat after clock stops
        float tempoRatio = this->wheelToTempo(horizontalWheel->value);
        deltaTime *= tempoRatio;
    }
    return deltaTime;
}

void NoteTaker::enableInsertSignature(unsigned loc) {
    unsigned insertLoc = this->atMidiTime(allNotes[loc].startTime);
    std::pair<NoteTakerButton*,  DisplayType> pairs[] = {
            { (NoteTakerButton*) keyButton, KEY_SIGNATURE },
            { (NoteTakerButton*) timeButton, TIME_SIGNATURE },
            { (NoteTakerButton*) tempoButton, MIDI_TEMPO } };
    for (auto& pair : pairs) {
        pair.first->af = this->insertContains(insertLoc, pair.second);
    }
}

void NoteTaker::eraseNotes(unsigned start, unsigned end) {
    debug("eraseNotes start %u end %u", start, end);
    for (auto iter = allNotes.begin() + end; iter-- != allNotes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            allNotes.erase(iter);
        }
    }
    this->debugDump(true, true);  // wheel range is inconsistent here
}

int NoteTaker::externalTempo(bool clockEdge) {
    if (inputs[CLOCK_INPUT].active) {
        if (clockEdge) {
            // upward edge, advance clock
            if (!this->isRunning()) {
                EventDragEnd e;
                runButton->onDragEnd(e);
            } else {
                externalClockTempo =
                        (int) ((lastClock ? ((realSeconds - lastClock) * 2) : 1) * tempo);
            }
            lastClock = realSeconds;
        }
        return externalClockTempo;
    }
    return tempo;
}

// for clipboard to be usable, either: all notes in clipboard are active, 
// or: all notes in clipboard are with a single channel (to paste to same or another channel)
// mono   locked
// false  false   copy all notes unchanged (return true)
// false  true    do nothing (return false)
// true   false   copy all notes unchanged (return true)
// true   true    copy all notes to unlocked channel (return true)
bool NoteTaker::extractClipboard(vector<DisplayNote>* span) const {
    bool mono = true;
    bool locked = false;
    int channel = -1;
    for (auto& note : clipboard) {
        if (!note.isNoteOrRest()) {
            mono = false;
            locked |= selectChannels != ALL_CHANNELS;
            continue;
        }
        if (channel < 0) {
            channel = note.channel;
        } else {
            mono &= channel == note.channel;
        }
        locked |= !(selectChannels & (1 << note.channel));
    }
    if (!mono && locked) {
        return false;
    }
    *span = clipboard;
    if (locked) {
        MapChannel(*span, partButton->addChannel); 
    }
    return true;
}

// to compute range for horizontal wheel when selecting notes
// to do : horizontalCount, noteToWheel, wheelToNote share loop logic. Consolidate?
unsigned NoteTaker::horizontalCount() const {
    unsigned count = 0;
    int lastStart = -1;
    for (auto& note : allNotes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
    }
    return count;
}

bool NoteTaker::insertContains(unsigned loc, DisplayType type) const {
    unsigned before = loc;
    const DisplayNote* note;
    while (before) {
        note = &allNotes[--before];
        if (type == note->type) {
            return true;
        }
        if (!note->isSignature()) {
            break;
        }
    }
    while (loc < allNotes.size()) {
        note = &allNotes[loc];
        if (type == note->type) {
            return true;
        }
        if (!note->isSignature()) {
            return false;
        }
        ++loc;
    }
    return false;
}

void NoteTaker::insertFinal(int shiftTime, unsigned insertLoc, unsigned insertSize) {
    if (shiftTime) {
        this->shiftNotes(insertLoc + insertSize, shiftTime);
    }
    display->invalidateCache();
    display->displayEnd = 0;  // force recompute of display end
    this->setSelect(insertLoc, this->nextAfter(insertLoc, insertSize));
    debug("insert final");
    this->setWheelRange();  // range is larger
    this->playSelection();
    this->debugDump(true);
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
    unsigned index = (unsigned) horizontalWheel->value;
    assert(index < storage.size());
    NoteTakerParseMidi parser(storage[index], allNotes, channels, ppq);
    DebugDumpRawMidi(storage[index]);
    if (!parser.parseMidi()) {
        this->setScoreEmpty();
    }
    this->resetRun();
    display->invalidateCache();
    this->setSelectStart(this->atMidiTime(0));
}

// to do : allow for triplet + slurred ?
void NoteTaker::makeNormal() {
    // to do : if notes are slurred (missing note off) or odd timed (triplets) restore
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (note.isSelectable(selectChannels)) {
            note.setSlur(false);
        }
    }
}

void NoteTaker::makeSlur() {
    // to do
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (note.isSelectable(selectChannels)) {
            note.setSlur(true);
        }
    }
}

// if current selection includes durations not in std note durations, do nothing
// the assumption is that in this case, durations have already been tupletized 
void NoteTaker::makeTuplet() {
    int smallest = NoteDurations::ToMidi(NoteDurations::Count() - 1, ppq);
    int beats = 0;
    auto addBeat = [&](int duration) {
        if (NoteDurations::Closest(duration, ppq) != duration) {
            debug("can't tuple nonstandard duration %d ppq %d", duration, ppq);
            return false;
        }
        int divisor = (int) gcd(smallest, duration);
        if (NoteDurations::Closest(divisor, ppq) != divisor) {
            debug("can't tuple gcd: smallest %d divisor %d ppq %d", smallest, divisor, ppq);
            return false;
        }
        if (divisor < smallest) {
            beats *= smallest / divisor;
            smallest = divisor;
        }
        beats += divisor / smallest;
        return true;
    };
    // count number of beats or beat fractions, find smallest factor
    int last = 0;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        const DisplayNote& note = allNotes[index];
        if (note.isSelectable(selectChannels)) {
            int restDuration = last - note.startTime;
            if (restDuration > 0) {    // implied rest
                if (!addBeat(restDuration)) {
                    return;
                }
            }
            if (!addBeat(note.duration)) {
                return;
            }
            last = note.endTime();
        }
    }
    if (beats % 3) {
        return;   // to do : only support triplets for now
    }
    // to do : to support 5-tuple, ppq must be a multiple of 5
    //       : to support 7-tuple, ppq must be a multiple of 7
    // to do : general tuple rules are complicated, so only implement a few simple ones
    // to do : respect time signature (ignored for now)
    int adjustment = 0;
    for (int tuple : { 3, 5, 7 } ) {
        // 3 in time of 2, 5 in time of 4, 7 in time of 4
        if (beats % tuple) {
            continue;
        }
        last = 0;
        int factor = 3 == tuple ? 1 : 3;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            int restDuration = last - note.startTime;
            if (restDuration > 0) {
                adjustment -= restDuration / tuple;
            }
            note.startTime += adjustment;
            int delta = note.duration * factor / tuple;
            adjustment -= delta;
            note.duration -= delta;
            last = note.endTime();
        }
        break;
    }
    this->shiftNotes(selectEnd, adjustment);
    display->invalidateCache();
}

int NoteTaker::noteToWheel(const DisplayNote& match, bool dbug) const {
    int count = 0;
    int lastStart = -1;
    for (auto& note : allNotes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (&match == &note) {
            return count + (TRACK_END == match.type);
        }
    }
    if (MIDI_HEADER == match.type) {
        return -1;
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
    this->setPlayStart();
}

void NoteTaker::reset() {
    this->resetState();
    this->readStorage();
}

void NoteTaker::resetState() {
    debug("notetaker reset");
    allNotes.clear();
    clipboard.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    this->resetRun();
    selectChannels = ALL_CHANNELS;
    this->setScoreEmpty();
    selectStart = 0;
    selectEnd = 1;
    this->resetControls();
    Module::reset();
}

bool NoteTaker::resetControls() {
    if (!display) {
        return false;
    }
    this->turnOffLedButtons();
    for (NoteTakerButton* button : {
            (NoteTakerButton*) cutButton, (NoteTakerButton*) insertButton,
            (NoteTakerButton*) restButton, (NoteTakerButton*) selectButton,
            (NoteTakerButton*) timeButton }) {
        button->reset();
    }
    partButton->resetChannels();
    this->horizontalWheel->reset();
    this->verticalWheel->reset();
    this->setWheelRange();
    display->reset();
    return true;
}

void NoteTaker::resetRun() {
    outputs[CLOCK_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
    elapsedSeconds = 0;
    clockHighTime = FLT_MAX;
    lastClock = 0;
    externalClockTempo = stdMSecsPerQuarterNote;
    resetHighTime = FLT_MAX;
    eosTime = FLT_MAX;
    midiClockOut = ppq;
    clockOutTime = realSeconds + 0.001f;
    sawClockLow = false;
    sawResetLow = false;
    if (display) {
        display->resetXAxisOffset();
        display->displayStart = display->displayEnd = 0;
    }
    this->setPlayStart();
}

bool NoteTaker::runningWithButtonsOff() const {
    return this->isRunning() && !fileButton->ledOn
            && !partButton->ledOn && !sustainButton->ledOn && !tieButton->ledOn;

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

// to do : could optimize with binary search
void NoteTaker::setPlayStart() {
    playStart = 0;
    if (!allNotes.size()) {
        return;
    }
    tempo = stdMSecsPerQuarterNote;
    this->zeroGates();
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->advancePlayStart(midiTime, allNotes.size() - 1);
}

void NoteTaker::setScoreEmpty() {
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    DebugDumpRawMidi(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, allNotes, channels, ppq);
    bool success = emptyParser.parseMidi();
    assert(success);
    if (display) {
        display->invalidateCache();
    }
}

 //   to do
    // think about whether : the whole selection fits on the screen
    // the last note fits on the screen if increasing the end
    // the first note fits on the screen if decreasing start
        // scroll small selections to center?
        // move larger selections the smallest amount?
        // prefer to show start? end?
        // while playing, scroll a 'page' at a time?
void NoteTaker::setSelect(unsigned start, unsigned end) {
    if (this->isEmpty()) {
        selectStart = 0;
        selectEnd = 1;
        display->displayStart = display->displayEnd = 0;
        return;
    }
    assert(start < end);
    assert(allNotes.size() >= 2);
    assert(end <= allNotes.size() - 1);
    display->setRange();
    this->enableInsertSignature(end);  // disable buttons that already have signatures in score
    debug("setSelect old %u %u new %u %u", selectStart, selectEnd, start, end);
    selectStart = start;
    selectEnd = end;
    displayFrameBuffer->dirty = true;
}

bool NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
    debug("setSelectEnd wheelValue=%d end=%u button->selStart=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->selStart, selectStart, selectEnd);
    bool changed = true;
    if (end < selectButton->selStart) {
        this->setSelect(end, selectButton->selStart);
        debug("setSelectEnd < s:%u e:%u", selectStart, selectEnd);
    } else if (end == selectButton->selStart) {
        unsigned start = selectButton->selStart;
        assert(TRACK_END != allNotes[start].type);
        unsigned end = this->wheelToNote(wheelValue + 1);
        this->setSelect(start, end);
        debug("setSelectEnd == s:%u e:%u", selectStart, selectEnd);
    } else if (end != selectEnd) {
        this->setSelect(selectButton->selStart, end);
        debug("setSelectEnd > s:%u e:%u", selectStart, selectEnd);
    } else {
        changed = false;
    }
    assert(selectEnd != selectStart);
    return changed;
}

bool NoteTaker::setSelectStart(unsigned start) {
    unsigned end = start;
    do {
        ++end;
    } while (allNotes[start].startTime == allNotes[end].startTime && allNotes[start].isNoteOrRest()
            && allNotes[end].isNoteOrRest());
    this->setSelect(start, end);
    return true;
}

// since this runs on a high frequency thread, avoid state except to play notes
void NoteTaker::step() {
    realSeconds += engineGetSampleTime();
    if (eosTime < realSeconds) {
        outputs[EOS_OUTPUT].value = 0;
        eosTime = FLT_MAX;
    }
    if (clockOutTime < realSeconds) {
        outputs[CLOCK_OUTPUT].value = 0;
        clockOutTime = FLT_MAX;
    }
    if (!runButton) {
        return;  // do nothing if we're not set up yet
    }
    bool running = this->isRunning();
    float clockCycle = 0;
    int localTempo;
    if (inputs[CLOCK_INPUT].active) {
        sawClockLow |= inputs[CLOCK_INPUT].value < 2;
        if (inputs[CLOCK_INPUT].value > 8 && sawClockLow) {
            clockCycle = std::max(0., realSeconds - clockHighTime);
            sawClockLow = false;
            clockHighTime = realSeconds;
        }
    }
    float resetCycle = 0;
    if (inputs[RESET_INPUT].active) {
        sawResetLow |= inputs[RESET_INPUT].value < 2;
        if (inputs[RESET_INPUT].value > 8 && sawResetLow) {
            resetCycle = std::max(0., realSeconds - resetHighTime);
            sawResetLow = false;
            resetHighTime = realSeconds;
        }
    }
    if (clockCycle && !running && selectButton->editStart() && inputs[CLOCK_INPUT].active
            && inputs[V_OCT_INPUT].active) {
        int duration = 0;
        if (inputs[RESET_INPUT].active) {
            sawResetLow = inputs[RESET_INPUT].value < 2;
            if (inputs[RESET_INPUT].value > 8 && sawResetLow) {
                if (realSeconds > resetHighTime) {
                    // insert note with pitch v/oct and duration quantized by clock step
                    duration = ppq * (realSeconds - resetHighTime) / clockCycle;
                }
                sawResetLow = false;
                resetHighTime = realSeconds;
            }
        } else {
            duration = ppq;   // on clock step, insert quarter note with pitch set from v/oct input
        }        
        if (duration) { // to do : make bias common const (if it really is one)
            const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            int midiNote = (int) ((inputs[V_OCT_INPUT].value - bias) * 12);
            unsigned insertLoc = !this->noteCount() ? this->atMidiTime(0) : !selectStart ?
                    this->wheelToNote(1) : selectEnd;
            int startTime = allNotes[insertLoc].startTime;
            DisplayNote note = { nullptr, startTime, duration,
                    { midiNote, 0, stdKeyPressure, stdKeyPressure},
                    partButton->addChannel, NOTE_ON, false };
            allNotes.insert(allNotes.begin() + insertLoc, note);
            this->insertFinal(duration, insertLoc, 1);
        }
        localTempo = tempo;
    } else {
        localTempo = this->externalTempo(clockCycle);
        if (resetCycle) {
            this->resetRun();
        }
    }
    if (!playStart) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    elapsedSeconds += engineGetSampleTime() * this->beatsPerHalfSecond(localTempo);
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    if (midiTime >= midiClockOut) {
        midiClockOut += ppq;
        outputs[CLOCK_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
        clockOutTime = 0.001f + realSeconds;
    }
    this->setExpiredGatesLow(midiTime);
    unsigned lastNote = running ? allNotes.size() - 1 : selectEnd - 1;
    if (this->advancePlayStart(midiTime, lastNote)) {
        playStart = 0;
        if (running) {
            // to do : add option to stop running
            this->resetRun();
            outputs[EOS_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
            eosTime = realSeconds + 0.01f;
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
        int endTime = note.endTime();
        if (endTime <= midiTime) {
            continue;
        }
        auto& channelInfo = channels[note.channel];
        unsigned noteIndex = this->noteIndex(note);
        if (noteIndex == channelInfo.noteIndex) {
            if (midiTime < channelInfo.noteEnd) {
                continue;
            }
        } else {
            unsigned prior = channelInfo.noteIndex;
            channelInfo.gateLow = note.slur() && start < lastNote ? INT_MAX : 
                    note.startTime + channelInfo.sustain(note.duration);
            channelInfo.noteEnd = endTime;
            channelInfo.noteIndex = noteIndex;
            if (false) debug("setGate [%u] gateLow %d noteEnd %d noteIndex %u prior %u midiTime %d",
                    note.channel, channelInfo.gateLow, channelInfo.noteEnd, channelInfo.noteIndex,
                    prior, midiTime);
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
            float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            if (runningWithButtonsOff()) {
                bias += inputs[V_OCT_INPUT].value;
                bias += (verticalWheel->wheelValue() - 60) / 12.f;
            }
            outputs[CV1_OUTPUT + note.channel].value = bias + note.pitch() / 12.f;
        }
    }
    if (running) {
        // to do : don't write to same state in different threads
        // to do : select selStart through selEnd, move displayStart / end calc to display thread
        if (INT_MAX != selStart && selectStart != selStart) {
            (void) this->setSelectStart(selStart);
        }
    }
}

// never turns off select button, since it cannot be turned off if score is empty,
// and it contains state that should not be arbitrarily deleted. select button
// is explicitly reset only when notetaker overall state is reset
void NoteTaker::turnOffLedButtons(const NoteTakerButton* exceptFor) {
    for (NoteTakerButton* button : {
                (NoteTakerButton*) fileButton, (NoteTakerButton*) partButton,
                (NoteTakerButton*) runButton, (NoteTakerButton*) sustainButton,
                (NoteTakerButton*) tieButton }) {
        if (exceptFor != button) {
            button->onTurnOff();
        }
    }
}

// counts to nth selectable note; returns index into notes array
unsigned NoteTaker::wheelToNote(int value, bool dbug) const {
    if (!value) {
        return 0;  // midi header
    }
    assert(value >= 0);
    assert(value < (int) allNotes.size());
    int count = value - 1;
    int lastStart = -1;
    for (auto& note : allNotes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            if (--count < 0) {
                return this->noteIndex(note);
            }
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (TRACK_END == note.type) {
            assert(count <= 0);
            return allNotes.size() - 1;
        }
    }
    if (dbug) {
        debug("wheelToNote value %d", value);
        this->debugDump(false, true);
        assert(0);  // probably means wheel range is larger than selectable note count
    }
    return (unsigned) -1;
}

float NoteTaker::wheelToTempo(float value) const {
    int horzIndex = (int) value;
    int floor = NoteDurations::ToStd(horzIndex);
    int ceil = NoteDurations::ToStd(horzIndex + 1);
    float interp = floor + (ceil - floor) * (value - horzIndex);
    return stdTimePerQuarterNote / interp;
}

int NoteTaker::xPosAtEndStart() const {
    assert(display && !display->cacheInvalid);
    return allNotes[selectEnd - 1].cache->xPosition;
}

int NoteTaker::xPosAtEndEnd() const {
    assert(display && !display->cacheInvalid);
    const NoteCache* noteCache = allNotes[selectEnd - 1].cache;
    return display->xEndPos(*noteCache);
}

int NoteTaker::xPosAtStartEnd() const {
    assert(display && !display->cacheInvalid);
    unsigned startEnd = this->selectEndPos(selectStart);
    return allNotes[startEnd].cache->xPosition;
}

int NoteTaker::xPosAtStartStart() const {
    assert(display && !display->cacheInvalid);
    return allNotes[selectStart].cache->xPosition;
}
