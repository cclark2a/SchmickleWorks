#include "Button.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

// to do : to run headless, allow mainWidget and ntw() to be nullptr?

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
            ntw()->displayBuffer->redraw();
            lastRatio = tempoRatio;
        }
        if ((deltaTime < 0 || tempoRatio < 0) && debugVerbose) DEBUG("local %d delta %g ratio %g", localTempo, deltaTime, tempoRatio);
        deltaTime *= tempoRatio;
    }
    return deltaTime;
}

// if clockCycle is 120 bpm, external clock tempo equals internal tempo
int NoteTaker::externalTempo() {
    if (inputs[CLOCK_INPUT].isConnected()) {
        if (clockCycle) {
            // upward edge, advance clock
            if (!this->isRunning()) {
                event::DragEnd e;
                ntw()->runButton->onDragEnd(e);
            } else {
                externalClockTempo = (int) (tempo * 2 / clockCycle);
            }
        }
        return externalClockTempo;
    }
    return tempo;
}

bool NoteTaker::isRunning() const {
    return ntw()->runButton->ledOn();
}

void NoteTaker::invalidateAndPlay(Inval inval) {
    DEBUG("invalidateAndPlay %s", InvalDebugStr(inval).c_str());
    if (Inval::none == inval) {
        return;
    }
    auto display = this->ntw()->display;
    display->invalidateRange();
    if (Inval::display != inval) {
        display->invalidateCache();
        if (Inval::note == inval || Inval::load == inval) {
            invalidVoiceCount = true;
            DEBUG("invalidVoiceCount true");
        }
    }
    if (Inval::load == inval) {
        this->setVoiceCount();
        this->setOutputsVoiceCount();

    } else if (Inval::cut != inval) {
        this->setPlayStart();
        this->playSelection();
    }
}

Notes& NoteTaker::n() {
    return ntw()->storage.current().n;
}

const Notes& NoteTaker::n() const {
    return ntw()->storage.current().n;
}

void NoteTaker::playSelection() {
    const auto& n = this->n();
    elapsedSeconds = MidiToSeconds(n.notes[n.selectStart].startTime, n.ppq);
    elapsedSeconds *= (float) stdMSecsPerQuarterNote / tempo;
    int midiTime = SecondsToMidi(elapsedSeconds, n.ppq);
    eosInterval = 0;
    const auto& storage = ntw()->storage;
    bool runningWithSlots = this->isRunning() && ntw()->slotButton->ledOn();
    if (runningWithSlots) {
        const auto& slotPlay = storage.playback[storage.slotStart];
        runningStage = slotPlay.stage;
        repeat = slotPlay.repeat;
        eosBase = 0;
        switch (runningStage) {
            case SlotPlay::Stage::step:
                break;
            case SlotPlay::Stage::beat:     // to do : rename, really midi tick
                eosInterval = 1;
                break;
            case SlotPlay::Stage::quarterNote:
                eosInterval = n.ppq;
                break;
            case SlotPlay::Stage::bar:
                // interval set by advance play start
                break;
            case SlotPlay::Stage::song:
                eosBase = midiEndTime;
                break;
            default:
                _schmickled();
        }
    } else {
        runningStage = SlotPlay::Stage::song;
        eosBase = midiEndTime;
        repeat = this->isRunning() ? INT_MAX : 1;
    }
    this->advancePlayStart(midiTime, midiEndTime);
}

// since this runs on a high frequency thread, avoid state except to play notes
// to do : while !running but notes are playing, disallow edits to notes and slots 
//         (by ignoring button presses / wheel changes)
void NoteTaker::process(const ProcessArgs &args) {
    realSeconds += args.sampleTime;
    outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10 : 0);
    outputs[EOS_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10 : 0);
    bool ignoreClock = 0.001f > resetTimer.process(args.sampleTime);
#if RUN_UNIT_TEST
    if (mainWidget->runUnitTest) {
        return;
    }
#endif
// to do : defer switch until criteria (e.g., end of bar) is met
    if (INT_MAX != stagedSlotStart) {
        if (debugVerbose) DEBUG("process stagedSlotStart %u", stagedSlotStart);
        ntw()->storage.slotStart = stagedSlotStart;
        ntw()->storage.slotEnd = stagedSlotStart + 1;
        ntw()->display->stagedSlot = &ntw()->storage.current();
        stagedSlotStart = INT_MAX;
        this->invalidateAndPlay(Inval::load);
        this->resetRun();
        ntw()->resetForPlay();
        this->playSelection();
    }
    auto& n = this->n();
    bool running = this->isRunning();
    int localTempo = tempo;
    SCHMICKLE(tempo);
    if (inputs[RESET_INPUT].isConnected()) {
        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            resetCycle = std::max(0., realSeconds - resetHighTime);
            resetHighTime = realSeconds;
            resetTimer.reset();
        }
    } else {
        resetCycle = 0;
    }
    if (inputs[CLOCK_INPUT].isConnected()) {
        if (!ignoreClock && clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
            clockCycle = std::max(0., realSeconds - clockHighTime);
            clockHighTime = realSeconds;
            if (!running && ntw()->selectButton->editStart() && inputs[V_OCT_INPUT].isConnected()) {
                int duration;
                if (resetCycle && clockCycle) {
                    // insert note with pitch v/oct and duration quantized by clock step
                    duration = n.ppq * resetCycle / clockCycle;
                    resetCycle = 0;
                } else {
                    duration = n.ppq;  // on clock step, insert quarter note with pitch set from v/oct input
                }        
                // to do : make bias common const (if it really is one)
                const float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
                int midiNote = (int) ((inputs[V_OCT_INPUT].getVoltage() - bias) * 12);
                unsigned insertLoc = !n.noteCount(ntw()->selectChannels) ? n.atMidiTime(0) :
                        !n.selectStart ? mainWidget->wheelToNote(1) : n.selectEnd;
                int startTime = n.notes[insertLoc].startTime;
                DisplayNote note(NOTE_ON, startTime, duration, (uint8_t) mainWidget->unlockedChannel());
                note.setPitch(midiNote);
                n.notes.insert(n.notes.begin() + insertLoc, note);
                mainWidget->insertFinal(duration, insertLoc, 1);
            } else {
                localTempo = this->externalTempo();
                if (resetCycle) {
                    ntw()->display->range.resetXAxisOffset();
                    this->resetRun();
                    resetCycle = 0;
                }
            }
        }
    } else {
        clockCycle = 0;
    }
    bool playNotes = (bool) playStart;
    int midiTime = 0;
    auto& storage = ntw()->storage;
    if (playNotes) {
        // read data from display notes to determine pitch
        // note on event start changes cv and sets gate high
        // note on event duration sets gate low
        midiTime = SecondsToMidi(elapsedSeconds, n.ppq);
        double debugSeconds = elapsedSeconds;
        elapsedSeconds += args.sampleTime * this->beatsPerHalfSecond(localTempo);
        if (debugSeconds >= elapsedSeconds) {
            DEBUG("last %g elapsed %g sample %g tempo %d beats %g",
                    debugSeconds, elapsedSeconds, args.sampleTime, localTempo,
                    this->beatsPerHalfSecond(localTempo));
            _schmickled();
        }
        if (midiTime >= midiClockOut) {
            midiClockOut += n.ppq;
            clockPulse.trigger();
        }
        if (eosBase < midiTime) {   // move slot ending condition so it is always equal to or ahead
            eosBase = midiTime;
            if (eosInterval) {
                eosBase += eosInterval - midiTime % eosInterval;
            }
        }
        this->setOutputsVoiceCount();
        this->setExpiredGatesLow(midiTime);
        bool slotOn = ntw()->slotButton->ledOn();
        // if eos input is high, end song on slot ending condition
        if (eosTrigger.process(inputs[EOS_INPUT].getVoltage()) && INT_MAX != eosBase) {
            repeat = 1;
            midiEndTime = eosBase;
            // to do : turn on slot button if off
        }
        if (!this->advancePlayStart(midiTime, midiEndTime)) {
            playStart = 0;
            if (running && slotOn) {
                eosBase = INT_MAX;  // ignore additional triggers until next slot is staged
                if (repeat-- <= 1) {
                    unsigned selectStart = storage.slotStart;
                    if (selectStart + 1 >= storage.playback.size()) {
                        repeat = 1;
                        this->stageSlot(0);
                        running = false;
                        // to do : turn run button off
                    } else {
                        const SlotPlay& slotPlay = storage.playback[selectStart + 1];
                        repeat = slotPlay.repeat;
                        this->stageSlot(slotPlay.index);
                    }
                }
            }
            if (running) {
                ntw()->display->range.resetXAxisOffset();
                this->resetRun();
                eosPulse.trigger();
                this->advancePlayStart(0, INT_MAX);
            }
            if (!running) {
                this->setExpiredGatesLow(INT_MAX);
            }
            playNotes = false;
        }
    }
    // if connected, set up all super eight outputs to last state before overwriting with new state
    if (rightExpander.module && rightExpander.module->model == modelSuper8) {
        Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
        for (unsigned chan = 0; chan < EXPANSION_OUTPUTS; ++chan) {
            message->channels[chan] = channels[chan].voiceCount;
            const auto& vIn = channels[chan].voices;
            for (unsigned voice = 0; voice < VOICE_COUNT; ++voice) {
                if (chan >= CV_OUTPUTS) {
                    if (debugVerbose && 4 == chan && 0 == voice && !playNotes) {
                        static float last = -1;
                        static const float* lastAddr = nullptr;
                        if (last != vIn[voice].gate || lastAddr != &vIn[voice].gate) {
                            DEBUG("vIn[voice].gate %g %p", vIn[voice].gate, &vIn[voice].gate);
                            last = vIn[voice].gate;
                            lastAddr = &vIn[voice].gate;
                        }
                    }
                    message->gate[chan - CV_OUTPUTS][voice] = vIn[voice].gate;
                    message->cv[chan - CV_OUTPUTS][voice] = vIn[voice].cv;
                }
                message->velocity[chan][voice] = vIn[voice].velocity;
            }
        }
    }
    if (playNotes) {
        unsigned start = playStart - 1;
        unsigned sStart = INT_MAX;
        do {
            const auto& note = n.notes[++start];
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
            SCHMICKLE((uint8_t) -1 != note.voice);
            unsigned voiceIndex = note.voice;
            auto& voice = channelInfo.voices[voiceIndex];
            if (&note == voice.note) {
                if (midiTime < voice.noteEnd) {
                    continue;
                }
            } else {
                voice.note = &note;
                voice.realStart = realSeconds;
                voice.gateLow = note.slurEnd() ? INT_MAX : 
                        note.startTime + storage.current().channels[note.channel].sustain(note.duration);
                voice.noteEnd = endTime;
                if (note.channel >= CV_OUTPUTS && note.channel < EXPANSION_OUTPUTS
                        && rightExpander.module && rightExpander.module->model == modelSuper8) {
                    Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
                    channels[note.channel].voices[voiceIndex].gate = DEFAULT_GATE_HIGH_VOLTAGE;
                    message->gate[note.channel - CV_OUTPUTS][voiceIndex] = DEFAULT_GATE_HIGH_VOLTAGE;
                }
                if (note.channel < CV_OUTPUTS) {
#if DEBUG_RUN_TIME
                    if (debugVerbose) DEBUG("setGate [%u] gateLow %d noteEnd %d noteIndex %u midiTime %d old %g",
                            note.channel, voice.gateLow, voice.noteEnd, voice.note - &n.notes.front(),
                            midiTime, outputs[GATE1_OUTPUT + note.channel].getVoltage(voiceIndex));
#endif
                    outputs[GATE1_OUTPUT + note.channel].setVoltage(DEFAULT_GATE_HIGH_VOLTAGE, voiceIndex);
                }
            }
            if (running) {
                sStart = std::min(sStart, start);
            }
            // recompute pitch all the time to prepare for tremelo / vibrato / slur / etc
            if (note.channel < EXPANSION_OUTPUTS) {
                float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
                auto verticalWheel = ntw()->verticalWheel;
                if (mainWidget->runningWithButtonsOff()) {
                    bias += inputs[V_OCT_INPUT].getVoltage();
                    bias += ((int) verticalWheel->getValue() - 60) / 12.f;
                }
                if (rightExpander.module && rightExpander.module->model == modelSuper8) {
                    Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
                    if (note.channel >= CV_OUTPUTS) {
                        float newCV = bias + note.pitch() / 12.f;
                        channels[note.channel].voices[voiceIndex].cv = newCV;
                        message->cv[note.channel - CV_OUTPUTS][voiceIndex] = newCV;
                    }
                    float velocity = note.onVelocity();
                    channels[note.channel].voices[voiceIndex].velocity = velocity;
                    message->velocity[note.channel][voiceIndex] = velocity;
                }
                if (note.channel < CV_OUTPUTS) {
#if DEBUG_RUN_TIME
                    if (debugVerbose) DEBUG("setNote [%u] bias %g v_oct %g wheel %g pitch %g new %g old %g",
                        note.channel, bias,
                        inputs[V_OCT_INPUT].getVoltage(), verticalWheel->getValue(),
                        note.pitch() / 12.f, bias + note.pitch() / 12.f,
                        outputs[CV1_OUTPUT + note.channel].getVoltage(voiceIndex));
#endif
                    outputs[CV1_OUTPUT + note.channel].setVoltage(bias + note.pitch() / 12.f, voiceIndex);
                }
            }
        } while (true);
        if (running) {
            // to do : don't write to same state in different threads
            // not sure how to enforce this, but the gist is that here is only written to
            // while running; everywhere else is only written to while not running
            // to do : add 'stage select start' to display so that other thread sets display start later
            if (INT_MAX != sStart && n.selectStart != sStart) {
                (void) ntw()->setSelectStart(sStart);
            }
        }
    }
    if (rightExpander.module && rightExpander.module->model == modelSuper8) {
        if (debugVerbose && !playNotes) {
            Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
            if (message->gate[0][0]) {
                DEBUG("expected zero gate");
                _schmickled();
            }
        }
        rightExpander.module->leftExpander.messageFlipRequested = true;
    }
}

void NoteTaker::onReset() {
    this->resetState();
    Module::onReset();
}

void NoteTaker::resetState() {
    if (debugVerbose) DEBUG("notetaker reset");
    auto ntw = this->ntw();
    auto& n = this->n();
    n.notes.clear();
    for (auto& channel : ntw->storage.current().channels) {
        channel.reset();
    }
    for (auto& c : channels) {
        c.voiceCount = 0;
    }
    this->resetRun();
    ntw->setScoreEmpty();
    n.selectStart = 0;
    n.selectEnd = 1;
    ntw->resetState();
    ntw->resetControls();
}

void NoteTaker::resetRun() {
    outputs[CLOCK_OUTPUT].setVoltage(DEFAULT_GATE_HIGH_VOLTAGE);
    elapsedSeconds = 0;
    clockHighTime = FLT_MAX;
    externalClockTempo = stdMSecsPerQuarterNote;
    resetHighTime = FLT_MAX;
    midiClockOut = n().ppq;
    clockTrigger.reset();
    eosTrigger.reset();
    resetTrigger.reset();
    clockPulse.reset();
    eosPulse.reset();
    resetTimer.reset();
    mainWidget->resetRun();
    this->setPlayStart();
}

void NoteTaker::setOutputsVoiceCount() {
    for (unsigned chan = 0; chan < CV_OUTPUTS; ++chan) {
        outputs[CV1_OUTPUT + chan].setChannels(channels[chan].voiceCount);
        outputs[GATE1_OUTPUT + chan].setChannels(channels[chan].voiceCount);
    }
}

void NoteTaker::setPlayStart() {
    this->zeroGates();
    tempo = stdMSecsPerQuarterNote;
    this->setVoiceCount();
    auto& n = this->n();
    playStart = ntw()->edit.voice ? n.selectStart : 0;
    unsigned lastNote = (this->isRunning() ? n.notes.size() : n.selectEnd) - 1;
    if (debugVerbose) DEBUG("setPlayStart lastNote %u", lastNote);
    midiEndTime = n.notes[lastNote].endTime();
    if (debugVerbose) DEBUG("setPlayStart midiEndTime %d", midiEndTime);
}

// to do : output debug data to show what set voice count did
void NoteTaker::setVoiceCount() {
    auto& n = this->n();
    DEBUG("setVoiceCount invalidVoiceCount %d", invalidVoiceCount);
    if (!invalidVoiceCount) {
        return;
    }
    invalidVoiceCount = false;
    size_t count = n.notes.size();
    for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
        channels[chan].voiceCount = 0;
    }
    if (!count) {
        return;
    }
    vector<const DisplayNote* > overlaps;
    overlaps.resize(CHANNEL_COUNT * VOICE_COUNT);
    overlaps.clear();
    --count;
    SCHMICKLE(TRACK_END == n.notes[count].type);
    for (unsigned index = 0; index < count; ++index) {
        auto& note = n.notes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        auto chan = note.channel;
        auto overStart = &overlaps[chan * VOICE_COUNT];
        const auto overEnd = overStart + VOICE_COUNT;
        unsigned vCount = 1;
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
#if DEBUG_VOICE_COUNT
            DEBUG("%d over note %s", vCount, note.debugString().c_str());
#endif
        }
        over = overStart - 1;
        if (VOICE_COUNT < vCount) {
            const DisplayNote** oldestOverlap = nullptr;
            int oldestTime = INT_MAX;
            while (++over < overEnd) {
                if (oldestTime > (*over)->startTime) {
                    oldestTime = (*over)->startTime;
                    oldestOverlap = over;
                }
            }
            *oldestOverlap = &note;
        } else {
            while (++over < overEnd) {
                if (!*over) {
                    *over = &note;
                    break;
                }
            }
        }
        channels[chan].voiceCount =
                std::max(channels[chan].voiceCount, vCount);
        note.voice = over - overStart;
#if DEBUG_VOICE_COUNT
        DEBUG("%u vCount %d chan %d %s", index, vCount, chan, note.debugString().c_str());
#endif
    }
}

void NoteTaker::stageSlot(unsigned slotIndex) {
    unsigned last = this->ntw()->getSlot();
    SCHMICKLE(last < SLOT_COUNT);
    if (slotIndex == last) {
        return;
    }
    if (debugVerbose) DEBUG("stageSlot %u old %u", slotIndex, last);
    stagedSlotStart = slotIndex;
}

float NoteTaker::wheelToTempo(float value) const {
    int horzIndex = (int) value;
    int floor = NoteDurations::ToStd(horzIndex);
    int next = horzIndex + 1;
    // artificial limit imposed by storing tempo in note data
    const float largest = INT_MAX / 500000.f;
    float ceil = (unsigned) next < NoteDurations::Count() ? NoteDurations::ToStd(next) : largest;
    float interp = floor + (ceil - floor) * (value - horzIndex);
    if (interp <= 0 && debugVerbose) DEBUG("value %g horz %d floor %d next %d ceil %g interp %g",
            value, horzIndex, floor, next, ceil, interp);
    return stdTimePerQuarterNote / interp;
}
