#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

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
    if (mainWidget->runningWithButtonsOff()) {
        // to do : decide how external clock works
        // external clock input could work in one of three modes:
        // 1/2) input voltage overrides / multiplies wheel value
        //   3) voltage step marks beat
        // for 3), second step determines bphs -- tempo change always lags 1 beat
        // playback continues for one beat after clock stops
        static float lastRatio = 0;
        auto horizontalWheel = mainWidget->widget<HorizontalWheel>();
        float tempoRatio = this->wheelToTempo(horizontalWheel->getValue());
        if (lastRatio != tempoRatio) {
            auto display = mainWidget->widget<NoteTakerDisplay>();
            display->fb->dirty = true;
            lastRatio = tempoRatio;
        }
        deltaTime *= tempoRatio;
    }
    return deltaTime;
}

int NoteTaker::externalTempo(bool clockEdge) {
    if (inputs[CLOCK_INPUT].active) {
        if (clockEdge) {
            // upward edge, advance clock
            if (!this->isRunning()) {
                event::DragEnd e;
                auto runButton = mainWidget->widget<RunButton>();
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
        note = &notes()[--before];
        if (type == note->type) {
            return true;
        }
        if (!note->isSignature()) {
            break;
        }
    }
    while (loc < notes().size()) {
        note = &notes()[loc];
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

bool NoteTaker::isRunning() const {
    return mainWidget->widget<RunButton>()->ledOn;
}

void NoteTaker::playSelection() {
    this->zeroGates();
    elapsedSeconds = MidiToSeconds(notes()[selStart()].startTime, ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    this->setPlayStart();
    int midiTime = SecondsToMidi(elapsedSeconds, ppq);
    this->advancePlayStart(midiTime, notes().size() - 1);
}

void NoteTaker::onReset() {
    this->resetState();
    // to do : figure out whether these should be in module or main widget
    mainWidget->readStorage();
    Module::onReset();
}

void NoteTaker::resetState() {
    if (debugVerbose) debug("notetaker reset");
    notes().clear();
    for (auto channel : channels) {
        channel.reset();
    }
    this->resetRun();
    this->setScoreEmpty();
    selStart() = 0;
    selEnd() = 1;
    mainWidget->resetState();
    mainWidget->resetControls();
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
    mainWidget->resetRun();
    this->setPlayStart();
}

// to do : could optimize with binary search
void NoteTaker::setPlayStart() {
    playStart = 0;
    if (!notes().size()) {
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
    NoteTakerParseMidi emptyParser(emptyMidi, notes(), channels, ppq);
    bool success = emptyParser.parseMidi();
    assert(success);
    auto display = mainWidget->widget<NoteTakerDisplay>();
    display->invalidateCache();
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
    auto display = mainWidget->widget<NoteTakerDisplay>();
    if (mainWidget->isEmpty()) {
        selStart() = 0;
        selEnd() = 1;
        display->setRange();
        display->fb->dirty = true;
        if (debugVerbose) debug("setSelect set empty");
        return;
    }
    assert(start < end);
    assert(notes().size() >= 2);
    assert(end <= notes().size() - 1);
    display->setRange();
    if (!this->isRunning()) {
        mainWidget->enableInsertSignature(end);  // disable buttons that already have signatures in score
    }
    if (debugVerbose) debug("setSelect old %u %u new %u %u", selStart(), selEnd(), start, end);
    selStart() = start;
    selEnd() = end;
    display->fb->dirty = true;
}

bool NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
    auto selectButton = mainWidget->widget<SelectButton>();
    debug("setSelectEnd wheelValue=%d end=%u button->selStart=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->selStart, selStart(), selEnd());
    bool changed = true;
    if (end < selectButton->selStart) {
        this->setSelect(end, selectButton->selStart);
        debug("setSelectEnd < s:%u e:%u", selStart(), selEnd());
    } else if (end == selectButton->selStart) {
        unsigned start = selectButton->selStart;
        assert(TRACK_END != notes()[start].type);
        unsigned end = mainWidget->wheelToNote(wheelValue + 1);
        this->setSelect(start, end);
        debug("setSelectEnd == s:%u e:%u", selStart(), selEnd());
    } else if (end != selEnd()) {
        this->setSelect(selectButton->selStart, end);
        debug("setSelectEnd > s:%u e:%u", selStart(), selEnd());
    } else {
        changed = false;
    }
    assert(selEnd() != selStart());
    return changed;
}

bool NoteTaker::setSelectStart(unsigned start) {
    unsigned end = start;
    do {
        ++end;
    } while (notes()[start].startTime == notes()[end].startTime && notes()[start].isNoteOrRest()
            && notes()[end].isNoteOrRest());
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
#if RUN_UNIT_TEST
    if (mainWidget->runUnitTest) {
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
    auto selectButton = mainWidget->widget<SelectButton>();
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
            unsigned insertLoc = !mainWidget->noteCount() ? this->atMidiTime(0) : !selStart() ?
                    mainWidget->wheelToNote(1) : selEnd();
            int startTime = notes()[insertLoc].startTime;
            DisplayNote note = { nullptr, startTime, duration,
                    { midiNote, 0, stdKeyPressure, stdKeyPressure},
                     (uint8_t) mainWidget->unlockedChannel(), NOTE_ON, false };
            notes().insert(notes().begin() + insertLoc, note);
            mainWidget->insertFinal(duration, insertLoc, 1);
        }
        localTempo = tempo;
    } else {
        localTempo = this->externalTempo(clockCycle);
        if (resetCycle) {
            mainWidget->widget<NoteTakerDisplay>()->resetXAxisOffset();
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
    unsigned lastNote = running ? notes().size() - 1 : selEnd() - 1;
    if (this->advancePlayStart(midiTime, lastNote)) {
        playStart = 0;
        if (running) {
            // to do : add option to stop running
            mainWidget->widget<NoteTakerDisplay>()->resetXAxisOffset();
            this->resetRun();
            outputs[EOS_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
            eosTime = realSeconds + 0.01f;
            this->advancePlayStart(midiTime, notes().size() - 1);
        } else {
            this->setExpiredGatesLow(INT_MAX);
        }
        return;
    }
    unsigned start = playStart - 1;
    unsigned sStart = INT_MAX;
    while (++start <= lastNote) {
        const auto& note = notes()[start];
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
            sStart = std::min(sStart, start);
        }
        // recompute pitch all the time to prepare for tremelo / vibrato / slur / etc
        if (note.channel < CV_OUTPUTS) {
            float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            auto verticalWheel = mainWidget->widget<VerticalWheel>();
            if (mainWidget->runningWithButtonsOff()) {
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
        if (INT_MAX != sStart && selStart() != sStart) {
            (void) this->setSelectStart(sStart);
        }
    }
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
    assert(!mainWidget->widget<NoteTakerDisplay>()->cacheInvalid);
    return notes()[selEnd() - 1].cache->xPosition;
}

int NoteTaker::xPosAtEndEnd() const {
    auto display = mainWidget->widget<NoteTakerDisplay>();
    assert(!display->cacheInvalid);
    const NoteCache* noteCache = notes()[selEnd() - 1].cache;
    return display->xEndPos(*noteCache);
}

int NoteTaker::xPosAtStartEnd() const {
    assert(!mainWidget->widget<NoteTakerDisplay>()->cacheInvalid);
    unsigned startEnd = this->selectEndPos(selStart());
    return notes()[startEnd].cache->xPosition;
}

int NoteTaker::xPosAtStartStart() const {
    assert(!mainWidget->widget<NoteTakerDisplay>()->cacheInvalid);
    return notes()[selStart()].cache->xPosition;
}
