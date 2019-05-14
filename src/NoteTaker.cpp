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

NoteTaker::NoteTaker() {
    this->onReset();
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
        static float lastRatio = 0;
        float tempoRatio = this->wheelToTempo(horizontalWheel->getValue());
        if (lastRatio != tempoRatio) {
            displayFrameBuffer->dirty = true;
            lastRatio = tempoRatio;
        }
        deltaTime *= tempoRatio;
    }
    return deltaTime;
}

void NoteTaker::eraseNotes(unsigned start, unsigned end) {
    if (debugVerbose) debug("eraseNotes start %u end %u", start, end);
    for (auto iter = notes.begin() + end; iter-- != notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            notes.erase(iter);
        }
    }
    if (debugVerbose) this->debugDump(true, true);  // wheel range is inconsistent here
}

int NoteTaker::externalTempo(bool clockEdge) {
    if (inputs[CLOCK_INPUT].active) {
        if (clockEdge) {
            // upward edge, advance clock
            if (!this->isRunning()) {
                event::DragEnd e;
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

bool NoteTaker::insertContains(unsigned loc, DisplayType type) const {
    unsigned before = loc;
    const DisplayNote* note;
    while (before) {
        note = &notes[--before];
        if (type == note->type) {
            return true;
        }
        if (!note->isSignature()) {
            break;
        }
    }
    while (loc < notes.size()) {
        note = &notes[loc];
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
    } else {
        Sort(notes);
    }
    display->invalidateCache();
    display->displayEnd = 0;  // force recompute of display end
    this->setSelect(insertLoc, this->nextAfter(insertLoc, insertSize));
    if (debugVerbose) debug("insert final");
    this->setWheelRange();  // range is larger
    this->playSelection();
    if (debugVerbose) this->debugDump(true);
}

bool NoteTaker::isRunning() const {
    return runButton->ledOn;
}

void NoteTaker::loadScore() {
    unsigned index = (unsigned) horizontalWheel->getValue();
    assert(index < storage.size());
    NoteTakerParseMidi parser(storage[index], notes, channels, ppq);
    if (debugVerbose) DebugDumpRawMidi(storage[index]);
    if (!parser.parseMidi()) {
        this->setScoreEmpty();
    }
    display->resetXAxisOffset();
    this->resetRun();
    display->invalidateCache();
    unsigned atZero = this->atMidiTime(0);
    atZero -= TRACK_END == notes[atZero].type;
    this->setSelectStart(atZero);
}

// to do : allow for triplet + slurred ?
// to do : if triplet, make normal
// to do : if triplet, start UI selection on triplet
void NoteTaker::makeNormal() {
    // to do : if notes are slurred (missing note off) or odd timed (triplets) restore
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        DisplayNote& note = notes[index];
        if (note.isNoteOrRest() && note.isSelectable(selectChannels)) {
            note.setSlur(false);
        }
    }
}

// allows two or more notes, or two or more rests: to be tied
// repeats for each channel, to allow multiple channels to be selected and slurred at once
void NoteTaker::makeSlur() {
    // to do: disallow choosing slur in UI if there isn't at least one pair.
    // For now, avoid singletons and non-notes
    unsigned start = -1;
    unsigned count = 0;
    int channel = -1;  // active channel to be slurred
    DisplayType type = UNUSED;
    array<unsigned, CHANNEL_COUNT> channels;  // track end of slurred notes
    channels.fill(0);
    for (unsigned index = selectStart; index <= selectEnd; ++index) {  // note that this goes 1 past
        if (index != selectEnd) {
            auto& note = notes[index];
            if (index < channels[note.channel]) {  // skip if channel has already been slurred
                continue;
            }
            if (UNUSED == type && note.isNoteOrRest()) {   // allow pairing notes or rests
                type = note.type;
            }
            if (type == note.type) {
                if (note.isSelectable(selectChannels)) {
                    if ((unsigned) -1 == start) {
                        start = index;
                        channel = note.channel;
                        count = 1;
                    } else if (channel == note.channel) {
                        ++count;
                    }
                }
                continue;
            }
        }
        if ((unsigned) -1 == start || count < 2) {
            continue;
        }
        for (unsigned inner = start; inner < index; ++inner) {
            auto& iNote = notes[inner];
            if (type == iNote.type && channel == iNote.channel) {
                iNote.setSlur(true);
            }
        }
        channels[channel] = index;
        index = start + 1;
        start = -1;
        count = 0;
        channel = -1;
        type = UNUSED;
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
        debug("divisor %d smallest %d", divisor, smallest);
        beats += duration / smallest;
        return true;
    };
    // count number of beats or beat fractions, find smallest factor
    int last = 0;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        const DisplayNote& note = notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        int restDuration = last - note.startTime;
        if (restDuration > 0) {    // implied rest
            debug("add implied rest %d", restDuration);
            if (!addBeat(restDuration)) {
                return;
            }
        }
        debug("add beat %s", note.debugString().c_str());
        if (!addBeat(note.duration)) {
            return;
        }
        last = note.endTime();
    }
    debug("beats : %d", beats);
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
            DisplayNote& note = notes[index];
            if (!note.isSelectable(selectChannels)) {
                continue;
            }
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
    for (auto& note : notes) {
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
    elapsedSeconds = MidiToSeconds(notes[selectStart].startTime, ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    this->setPlayStart();
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->advancePlayStart(midiTime, notes.size() - 1);
}

void NoteTaker::onReset() {
    this->resetState();
    this->readStorage();
}

void NoteTaker::resetState() {
    if (debugVerbose) debug("notetaker reset");
    notes.clear();
    clipboard.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    if (display) {
        display->resetXAxisOffset();
    }
    this->resetRun();
    selectChannels = ALL_CHANNELS;
    this->setScoreEmpty();
    selectStart = 0;
    selectEnd = 1;
    this->resetControls();
    Module::onReset();
}

void NoteTaker::resetRun() {
    if (!disabled) {
        outputs[CLOCK_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
    }
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
        display->displayStart = display->displayEnd = 0;
        display->rangeInvalid = true;
    }
    this->setPlayStart();
}

void NoteTaker::saveScore() {
    unsigned index = (unsigned) horizontalWheel->getValue();
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
    if (!notes.size()) {
        return;
    }
    tempo = stdMSecsPerQuarterNote;
    this->zeroGates();
}

void NoteTaker::setScoreEmpty() {
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    if (debugVerbose) DebugDumpRawMidi(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, notes, channels, ppq);
    bool success = emptyParser.parseMidi();
    assert(success);
    if (display) {
        display->invalidateCache();
    }
}

void NoteTaker::setSelectableScoreEmpty() {
    auto iter = notes.begin();
    while (iter != notes.end()) {
        if (this->isSelectable(*iter)) {
            iter = notes.erase(iter);
        } else {
            ++iter;
        }
    }
    display->invalidateCache();
    display->displayEnd = 0;  // force recompute of display end
    this->setSelect(0, 1);
    this->setWheelRange();
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
        assert(TRACK_END != notes[start].type);
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
    } while (notes[start].startTime == notes[end].startTime && notes[start].isNoteOrRest()
            && notes[end].isNoteOrRest());
    this->setSelect(start, end);
    return true;
}

// since this runs on a high frequency thread, avoid state except to play notes
void NoteTaker::step() {
    realSeconds += APP->engine->getSampleTime();
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
#if RUN_UNIT_TEST
    if (runUnitTest) {
        return;
    }
#endif
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
            int startTime = notes[insertLoc].startTime;
            DisplayNote note = { nullptr, startTime, duration,
                    { midiNote, 0, stdKeyPressure, stdKeyPressure},
                     (uint8_t) this->unlockedChannel(), NOTE_ON, false };
            notes.insert(notes.begin() + insertLoc, note);
            this->insertFinal(duration, insertLoc, 1);
        }
        localTempo = tempo;
    } else {
        localTempo = this->externalTempo(clockCycle);
        if (resetCycle) {
            display->resetXAxisOffset();
            this->resetRun();
        }
    }
    if (!playStart) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    if (false && running && ++debugMidiCount < 100) {
        debug("midiTime %d elapsedSeconds %g playStart %u", midiTime, elapsedSeconds, playStart);
    }
    elapsedSeconds += APP->engine->getSampleTime() * this->beatsPerHalfSecond(localTempo);
    if (midiTime >= midiClockOut) {
        midiClockOut += ppq;
        outputs[CLOCK_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
        clockOutTime = 0.001f + realSeconds;
    }
    this->setExpiredGatesLow(midiTime);
    unsigned lastNote = running ? notes.size() - 1 : selectEnd - 1;
    if (this->advancePlayStart(midiTime, lastNote)) {
        playStart = 0;
        if (running) {
            // to do : add option to stop running
            display->resetXAxisOffset();
            this->resetRun();
            outputs[EOS_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
            eosTime = realSeconds + 0.01f;
            this->advancePlayStart(midiTime, notes.size() - 1);
        } else {
            this->setExpiredGatesLow(INT_MAX);
        }
        return;
    }
    unsigned start = playStart - 1;
    unsigned selStart = INT_MAX;
    unsigned selEnd = 0;
    while (++start <= lastNote) {
        const auto& note = notes[start];
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
            if (note.channel < CV_OUTPUTS) {
                if (true) debug("setGate [%u] gateLow %d noteEnd %d noteIndex %u prior %u midiTime %d old %g",
                        note.channel, channelInfo.gateLow, channelInfo.noteEnd, channelInfo.noteIndex,
                        prior, midiTime, outputs[GATE1_OUTPUT + note.channel].value);
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
                bias += ((int) verticalWheel->getValue() - 60) / 12.f;
            }
            if (true) debug("setNote [%u] bias %g v_oct %g wheel %g pitch %g new %g old %g",
                note.channel, bias,
                inputs[V_OCT_INPUT].value, verticalWheel->getValue(),
                note.pitch() / 12.f, bias + note.pitch() / 12.f,
                outputs[CV1_OUTPUT + note.channel].value);
            outputs[CV1_OUTPUT + note.channel].value = bias + note.pitch() / 12.f;
        }
    }
    if (running) {
        // to do : don't write to same state in different threads
        if (INT_MAX != selStart && selectStart != selStart) {
            (void) this->setSelectStart(selStart);
        }
    }
}

// counts to nth selectable note; returns index into notes array
unsigned NoteTaker::wheelToNote(int value, bool dbug) const {
    if (!value) {
        return 0;  // midi header
    }
    if (!dbug && value < 0) {
        debug("! didn't expect < 0 : value: %d", value);
        return 0;
    }
    assert(value > 0);
    assert(value < (int) notes.size());
    int count = value - 1;
    int lastStart = -1;
    for (auto& note : notes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            if (--count < 0) {
                return this->noteIndex(note);
            }
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (TRACK_END == note.type) {
            if (count > 0) {
                debug("! expected 0 wheelToNote value at track end; value: %d", value);
                assert(!dbug);                
            }
            return notes.size() - 1;
        }
    }
    debug("! out of range wheelToNote value %d", value);
    if (dbug) {
        this->debugDump(false, true);
        assert(0);  // probably means wheel range is larger than selectable note count
    }
    return (unsigned) -1;
}

float NoteTaker::wheelToTempo(float value) const {
    int horzIndex = (int) value;
    int floor = NoteDurations::ToStd(horzIndex);
    int next = horzIndex + 1;
    // artificial limit imposed by storing tempo in note data
    const float largest = INT_MAX / 500000.f;
    float ceil = (unsigned) next < NoteDurations::Count() ? NoteDurations::ToStd(next) : largest;
    float interp = floor + (ceil - floor) * (value - horzIndex);
    return stdTimePerQuarterNote / interp;
}

int NoteTaker::xPosAtEndStart() const {
    assert(display && !display->cacheInvalid);
    return notes[selectEnd - 1].cache->xPosition;
}

int NoteTaker::xPosAtEndEnd() const {
    assert(display && !display->cacheInvalid);
    const NoteCache* noteCache = notes[selectEnd - 1].cache;
    return display->xEndPos(*noteCache);
}

int NoteTaker::xPosAtStartEnd() const {
    assert(display && !display->cacheInvalid);
    unsigned startEnd = this->selectEndPos(selectStart);
    return notes[startEnd].cache->xPosition;
}

int NoteTaker::xPosAtStartStart() const {
    assert(display && !display->cacheInvalid);
    return notes[selectStart].cache->xPosition;
}
