#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

// to do : to run headless, allow mainWidget and ntw() to be nullptr

NoteTaker::NoteTaker() {
    this->config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
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
        auto horizontalWheel = ntw()->horizontalWheel;
        float tempoRatio = this->wheelToTempo(horizontalWheel->getValue());
        if (lastRatio != tempoRatio) {
            auto display = ntw()->displayBuffer;
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
                auto runButton = ntw()->runButton;
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
        note = &n.notes[--before];
        if (type == note->type) {
            return true;
        }
        if (!note->isSignature()) {
            break;
        }
    }
    while (loc < n.notes.size()) {
        note = &n.notes[loc];
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
    return ntw()->runButton->ledOn;
}

void NoteTaker::playSelection() {
    elapsedSeconds = MidiToSeconds(n.notes[n.selectStart].startTime, n.ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    this->setPlayStart();
    int midiTime = SecondsToMidi(elapsedSeconds, n.ppq);
    this->advancePlayStart(midiTime, n.notes.size() - 1);
}

// since this runs on a high frequency thread, avoid state except to play notes
void NoteTaker::process(const ProcessArgs &args) {
    realSeconds += args.sampleTime;
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
    if (clockCycle && !running && ntw()->selectButton->editStart() && inputs[CLOCK_INPUT].active
            && inputs[V_OCT_INPUT].active) {
        int duration = 0;
        if (inputs[RESET_INPUT].active) {
            sawResetLow = inputs[RESET_INPUT].value < 2;
            if (inputs[RESET_INPUT].value > 8 && sawResetLow) {
                if (realSeconds > resetHighTime) {
                    // insert note with pitch v/oct and duration quantized by clock step
                    duration = n.ppq * (realSeconds - resetHighTime) / clockCycle;
                }
                sawResetLow = false;
                resetHighTime = realSeconds;
            }
        } else {
            duration = n.ppq;   // on clock step, insert quarter note with pitch set from v/oct input
        }        
        if (duration) { // to do : make bias common const (if it really is one)
            const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            int midiNote = (int) ((inputs[V_OCT_INPUT].value - bias) * 12);
            unsigned insertLoc = !n.noteCount(ntw()->selectChannels) ? this->atMidiTime(0) :
                    !n.selectStart ? mainWidget->wheelToNote(1) : n.selectEnd;
            int startTime = n.notes[insertLoc].startTime;
            DisplayNote note = { nullptr, startTime, duration,
                    { midiNote, 0, stdKeyPressure, stdKeyPressure},
                     (uint8_t) mainWidget->unlockedChannel(), NOTE_ON, false };
            n.notes.insert(n.notes.begin() + insertLoc, note);
            mainWidget->insertFinal(duration, insertLoc, 1);
        }
        localTempo = tempo;
    } else {
        localTempo = this->externalTempo(clockCycle);
        if (resetCycle) {
            ntw()->display->resetXAxisOffset();
            this->resetRun();
        }
    }
    if (!playStart) {
        return;
    }
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    int midiTime = SecondsToMidi(elapsedSeconds, n.ppq);
    elapsedSeconds += args.sampleTime * this->beatsPerHalfSecond(localTempo);
    if (midiTime >= midiClockOut) {
        midiClockOut += n.ppq;
        outputs[CLOCK_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
        clockOutTime = 0.001f + realSeconds;
    }
    for (unsigned chan = 0; chan < CV_OUTPUTS; ++chan) {
        outputs[CV1_OUTPUT + chan].setChannels(channels[chan].voiceCount);
        outputs[GATE1_OUTPUT + chan].setChannels(channels[chan].voiceCount);
    }
    this->setExpiredGatesLow(midiTime);
    unsigned lastNote = running ? n.notes.size() - 1 : n.selectEnd - 1;
    if (this->advancePlayStart(midiTime, lastNote)) {
        playStart = 0;
        if (running) {
            // to do : add option to stop running (waiting for feature req. / design inspiration)
            ntw()->display->resetXAxisOffset();
            this->resetRun();
            outputs[EOS_OUTPUT].value = DEFAULT_GATE_HIGH_VOLTAGE;
            eosTime = realSeconds + 0.01f;
            this->advancePlayStart(midiTime, n.notes.size() - 1);
        } else {
            this->setExpiredGatesLow(INT_MAX);
        }
        return;
    }
    unsigned start = playStart - 1;
    unsigned sStart = INT_MAX;
    while (++start <= lastNote) {
        const auto& note = n.notes[start];
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
#if POLY_EXPERIMENT
        unsigned& voiceIndex = noteVoice[&note - &n.notes.front()];
        auto& voice = channelInfo.voices[voiceIndex];
        if (&note == voice.note) {
            if (midiTime < voice.noteEnd) {
                continue;
            }
        } else {
            voice.note = &note;
            voice.realStart = realSeconds;
            voice.gateLow = note.slur() && start < lastNote ? INT_MAX : 
                    note.startTime + channelInfo.sustain(note.duration);
            voice.noteEnd = endTime;
            if (note.channel < CV_OUTPUTS) {
                if (debugVerbose) DEBUG("setGate [%u] gateLow %d noteEnd %d noteIndex %u midiTime %d old %g",
                        note.channel, voice.gateLow, voice.noteEnd, voice.note - &n.notes.front(),
                        midiTime, outputs[GATE1_OUTPUT + note.channel].getVoltage(voiceIndex));
                outputs[GATE1_OUTPUT + note.channel].setVoltage(DEFAULT_GATE_HIGH_VOLTAGE, voiceIndex);
            }
        }
        if (running) {
            sStart = std::min(sStart, start);
        }
        // recompute pitch all the time to prepare for tremelo / vibrato / slur / etc
        if (note.channel < CV_OUTPUTS) {
            float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
            auto verticalWheel = ntw()->verticalWheel;
            if (mainWidget->runningWithButtonsOff()) {
                bias += inputs[V_OCT_INPUT].value;
                bias += ((int) verticalWheel->getValue() - 60) / 12.f;
            }
            if (debugVerbose) DEBUG("setNote [%u] bias %g v_oct %g wheel %g pitch %g new %g old %g",
                note.channel, bias,
                inputs[V_OCT_INPUT].value, verticalWheel->getValue(),
                note.pitch() / 12.f, bias + note.pitch() / 12.f,
                outputs[CV1_OUTPUT + note.channel].getVoltage(voiceIndex));
            outputs[CV1_OUTPUT + note.channel].setVoltage(bias + note.pitch() / 12.f, voiceIndex);
        }
#else
        if (&note == channelInfo.note) {
            if (midiTime < channelInfo.noteEnd) {
                continue;
            }
        } else {
            auto prior = channelInfo.note;
            channelInfo.gateLow = note.slur() && start < lastNote ? INT_MAX : 
                    note.startTime + channelInfo.sustain(note.duration);
            channelInfo.noteEnd = endTime;
            channelInfo.note = &note;
            if (note.channel < CV_OUTPUTS) {
                if (debugVerbose) DEBUG("setGate [%u] gateLow %d noteEnd %d noteIndex %u prior %u midiTime %d old %g",
                        note.channel, channelInfo.gateLow, channelInfo.noteEnd, 
                        channelInfo.note - &n.notes.front(),
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
            auto verticalWheel = ntw()->verticalWheel;
            if (mainWidget->runningWithButtonsOff()) {
                bias += inputs[V_OCT_INPUT].value;
                bias += ((int) verticalWheel->getValue() - 60) / 12.f;
            }
            if (debugVerbose) DEBUG("setNote [%u] bias %g v_oct %g wheel %g pitch %g new %g old %g",
                note.channel, bias,
                inputs[V_OCT_INPUT].value, verticalWheel->getValue(),
                note.pitch() / 12.f, bias + note.pitch() / 12.f,
                outputs[CV1_OUTPUT + note.channel].value);
            outputs[CV1_OUTPUT + note.channel].value = bias + note.pitch() / 12.f;
        }
#endif
    }
    if (running) {
        // to do : don't write to same state in different threads
        // not sure how to enforce this, but the gist is that here is only written to
        // while running; everywhere else is only written to while not running
        if (INT_MAX != sStart && n.selectStart != sStart) {
            (void) this->setSelectStart(sStart);
        }
    }
}

void NoteTaker::onReset() {
    this->resetState();
    mainWidget->readStorage();
    Module::onReset();
}

void NoteTaker::resetState() {
    if (debugVerbose) DEBUG("notetaker reset");
    n.notes.clear();
    for (auto channel : channels) {
        channel.reset();
    }
    this->resetRun();
    this->setScoreEmpty();
    n.selectStart = 0;
    n.selectEnd = 1;
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
    midiClockOut = n.ppq;
    clockOutTime = realSeconds + 0.001f;
    sawClockLow = false;
    sawResetLow = false;
    invalidVoiceCount |= n.voice;
    n.voice = false;
    mainWidget->resetRun();
    this->setPlayStart();
}

void NoteTaker::setPlayStart() {
    this->zeroGates();
    playStart = n.voice ? n.selectStart : 0;
    tempo = stdMSecsPerQuarterNote;
#if POLY_EXPERIMENT
    this->setVoiceCount();
#endif
}

void NoteTaker::setScoreEmpty() {
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    if (debugVerbose) DebugDumpRawMidi(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, n.notes, channels, n.ppq);
    bool success = emptyParser.parseMidi();
    SCHMICKLE(success);
    invalidVoiceCount = true;
    ntw()->invalidateCaches();
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
    auto displayBuffer = ntw()->displayBuffer;
    auto display = ntw()->display;
    if (n.isEmpty(ntw()->selectChannels)) {
        n.selectStart = 0;
        n.selectEnd = 1;
        display->setRange();
        displayBuffer->fb->dirty = true;
        if (debugVerbose) DEBUG("setSelect set empty");
        return;
    }
    SCHMICKLE(start < end);
    SCHMICKLE(n.notes.size() >= 2);
    SCHMICKLE(end <= n.notes.size() - 1);
    display->setRange();
    if (!this->isRunning()) {
        mainWidget->enableInsertSignature(end);  // disable buttons that already have signatures in score
    }
    if (debugVerbose) DEBUG("setSelect old %u %u new %u %u", n.selectStart, n.selectEnd, start, end);
    n.selectStart = start;
    n.selectEnd = end;
    displayBuffer->fb->dirty = true;
}

bool NoteTaker::setSelectEnd(int wheelValue, unsigned end) {
    auto selectButton = ntw()->selectButton;
    DEBUG("setSelectEnd wheelValue=%d end=%u button->selStart=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->selStart, n.selectStart, n.selectEnd);
    bool changed = true;
    if (end < selectButton->selStart) {
        this->setSelect(end, selectButton->selStart);
        DEBUG("setSelectEnd < s:%u e:%u", n.selectStart, n.selectEnd);
    } else if (end == selectButton->selStart) {
        unsigned start = selectButton->selStart;
        SCHMICKLE(TRACK_END != n.notes[start].type);
        unsigned end = mainWidget->wheelToNote(wheelValue + 1);
        this->setSelect(start, end);
        DEBUG("setSelectEnd == s:%u e:%u", n.selectStart, n.selectEnd);
    } else if (end != n.selectEnd) {
        this->setSelect(selectButton->selStart, end);
        DEBUG("setSelectEnd > s:%u e:%u", n.selectStart, n.selectEnd);
    } else {
        changed = false;
    }
    SCHMICKLE(n.selectEnd != n.selectStart);
    return changed;
}

bool NoteTaker::setSelectStart(unsigned start) {
    unsigned end = start;
    do {
        ++end;
    } while (n.notes[start].startTime == n.notes[end].startTime && n.notes[start].isNoteOrRest()
            && n.notes[end].isNoteOrRest());
    this->setSelect(start, end);
    return true;
}

#if POLY_EXPERIMENT
// to do : output debug data to show what set voice count did

void NoteTaker::setVoiceCount() {
    if (!invalidVoiceCount) {
        return;
    }
    invalidVoiceCount = false;
    size_t count = n.notes.size();
    noteVoice.resize(count);
    noteVoice.clear();
    for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
        channels[chan].voiceCount = 0;
    }
    if (!count) {
        return;
    }
    if (n.voice) {
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            auto& base = n.notes[index];
            channels[base.channel].voiceCount = 1;
        }
    } else {
        vector<const DisplayNote* > overlaps;
        overlaps.resize(CHANNEL_COUNT * MAX_VOICES);
        overlaps.clear();
        --count;
        SCHMICKLE(TRACK_END == n.notes[count].type);
        for (unsigned index = 0; index < count; ++index) {
            const auto& note = n.notes[index];
            if (NOTE_ON != note.type) {
                continue;
            }
            auto chan = note.channel;
            auto overStart = &overlaps[chan * MAX_VOICES];
            const auto overEnd = overStart + MAX_VOICES;
            unsigned vCount = 0;
            unsigned vMax = 0;
            auto over = overStart - 1;
            while (++over < overEnd) {
                if (!*over) {
                    continue;
                }
                // to do : if note is slurred, allow one midi time unit of overlap
                // -- or -- keep notes un-overlapped, overlap only when reading/writing midi...
                if ((*over)->endTime() <= note.startTime) {
                    *over = nullptr;
                    continue;
                }
                ++vCount;
                vMax = over - overStart;
            }
            if (MAX_VOICES == vCount) {
                over = overStart - 1;
                const DisplayNote** oldestOverlap = nullptr;
                int oldestTime = INT_MAX;
                while (++over < overEnd) {
                    if (oldestTime > (*over)->startTime) {
                        oldestTime = (*over)->startTime;
                        oldestOverlap = over;
                    }
                }
                *oldestOverlap = nullptr;
            } else {
                ++vMax;
            }
            over = overStart - 1;
            while (++over < overEnd) {
                if (!*over) {
                    *over = &note;
                    break;
                }
            }
            channels[chan].voiceCount =
                    std::max(channels[chan].voiceCount, vMax + 1);
            noteVoice[index] = over - overStart;
        }
    }
}
#endif

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
