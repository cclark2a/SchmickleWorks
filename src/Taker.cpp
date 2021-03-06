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
    requests.reader = &requests.buffer.front();
    requests.writer = &requests.buffer.front();
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
        const auto horizontalWheel = ntw()->horizontalWheel;
        float tempoRatio = this->wheelToTempo(horizontalWheel->getValue());
        if (lastRatio != tempoRatio) {
            ntw()->redraw();    // ok to call at any time
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
                ntw()->reqs.push(ReqType::runButtonActivate);
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

// don't change other threads data here (display)
void NoteTaker::invalidateAndPlay(Inval inval) {
#if DEBUG_RUN
    if (debugVerbose) DEBUG("invalidateAndPlay %s", InvalDebugStr(inval).c_str());
#endif
    if (Inval::note == inval || Inval::load == inval || Inval::change == inval) {
        invalidVoiceCount = true;
#if DEBUG_RUN
        if (debugVerbose) DEBUG("invalidVoiceCount true");
#endif
    }
    if (Inval::load == inval) {
        this->resetRun();
        this->setVoiceCount();
        this->setOutputsVoiceCount();
    } else if (Inval::cut != inval) {
        this->setPlayStart();   // make sure notes are set up in caller before calling set start
        this->playSelection();
    }
}

// to do : document why a non-const version is required
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
#if DEBUG_RUN
    if (debugVerbose) DEBUG("%s playStart %d midiTime %d midiEndTime %d",
            __func__, playStart, midiTime, midiEndTime);
#endif
    this->advancePlayStart(midiTime, midiEndTime);
#if DEBUG_RUN
    if (debugVerbose) DEBUG("%s advance playStart %d", __func__, playStart);
#endif
}

#if DEBUG_CPU_TIME
static double worstTime = 0;
static double lastTime = 0;
static bool reportWorstLater = false;
static int reported = 0;
#endif

// since this runs on a high frequency thread, avoid state except to play notes
// to do : while !running but notes are playing, disallow edits to notes and slots 
//         (by ignoring button presses / wheel changes)
void NoteTaker::process(const ProcessArgs &args) {
#if DEBUG_CPU_TIME
    double startTime = system::getThreadTime();
#endif
    realSeconds += args.sampleTime;
#if RUN_UNIT_TEST
    if (mainWidget->runUnitTest) {
        return;
    }
#endif
    outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10 : 0);
    outputs[EOS_OUTPUT].setVoltage(eosPulse.process(args.sampleTime) ? 10 : 0);
    bool ignoreClock = 0.001f > resetTimer.process(args.sampleTime);
    while (!requests.empty()) {
        RequestRecord record = requests.pop();
#if DEBUG_REQUEST
        if (debugVerbose) {
            DEBUG("%s pop %s", __func__, record.debugStr().c_str());
        }
#endif
        switch (record.type) {
            case RequestType::invalidateAndPlay:
                this->invalidateAndPlay((Inval) record.data);
                break;
            case RequestType::invalidateVoiceCount:
                invalidVoiceCount = true;
                break;
            case RequestType::onReset:
                this->onReset();
                break;
            case RequestType::resetAndPlay:
                this->resetRun();
                this->setPlayStart();
                this->playSelection();
                break;
            case RequestType::resetPlayStart:
                playStart = 0;
                this->zeroGates();  // to do : don't do this so score can loop (last notes overlap first)
                break;
            case RequestType::setClipboardLight:
                this->setClipboardLight((float) record.data / 256.f);
                break;
            case RequestType::setPlayStart:
                this->setPlayStart();
                break;
            default:
                assert(0);
        }
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
                n.insertNote(insertLoc, startTime, duration, mainWidget->unlockedChannel(), midiNote);
                mainWidget->insertFinal(duration, insertLoc, 1);
            } else {
                localTempo = this->externalTempo();
                if (resetCycle) {
                    ntw()->reqs.push(ReqType::resetXAxisOffset);
                    this->resetRun();
                    this->setPlayStart();
                    this->playSelection();
                    resetCycle = 0;
                }
            }
        }
    } else {
        clockCycle = 0;
    }
    bool playNotes = (bool) playStart;
    int midiTime = 0;
    const auto& storage = ntw()->storage;
#if DEBUG_CPU_TIME
    double mid1 = system::getThreadTime();
#endif
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
//        this->setExpiredGatesLow(midiTime);
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
                        ntw()->reqs.push({ReqType::stagedSlotStart, 0});
                        running = false;
                        // to do : turn run button off
                    } else {
                        const SlotPlay& slotPlay = storage.playback[selectStart + 1];
                        repeat = slotPlay.repeat;
                        ntw()->reqs.push({ReqType::stagedSlotStart, slotPlay.index});
                    }
                }
            }
            if (running) {
                ntw()->reqs.push(ReqType::resetXAxisOffset);
                this->resetRun();
                this->setPlayStart();
//                this->playSelection();
                eosPulse.trigger();
                this->advancePlayStart(0, INT_MAX);
            }
            if (!running) {
                this->zeroGates();
            }
            playNotes = false;
        }
    }
#if DEBUG_CPU_TIME
    double mid2 = system::getThreadTime();
#endif
    // if connected, set up all super eight outputs to last state before overwriting with new state
    if (rightExpander.module && rightExpander.module->model == modelSuper8) {
        Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
        for (unsigned chan = 0; chan < EXPANSION_OUTPUTS; ++chan) {
            message->exChannels[chan] = channels[chan].voiceCount;
            const auto& vIn = channels[chan].voices;
#if DEBUG_GATES
            if (debugVerbose && 4 == chan && !playNotes) {
                static float last = -1;
                static const float* lastAddr = nullptr;
                if (last != vIn[0].gate || lastAddr != &vIn[0].gate) {
                    DEBUG("[%g] vIn[0].gate %g %p", realSeconds, vIn[0].gate, &vIn[0].gate);
                    last = vIn[0].gate;
                    lastAddr = &vIn[0].gate;
                }
            }
#endif
            for (unsigned voice = 0; voice < VOICE_COUNT; ++voice) {
                if (chan >= CV_OUTPUTS) {
                    message->exGate[chan - CV_OUTPUTS][voice] = vIn[voice].gate;
                    message->exCv[chan - CV_OUTPUTS][voice] = vIn[voice].cv;
                }
                message->exVelocity[chan][voice] = vIn[voice].velocity;
            }
        }
    }
#if DEBUG_CPU_TIME
    double mid3 = system::getThreadTime();
#endif
    int debugIterations = 0;
    int debugNotesSet = 0;
    int debugNotesPlay = 0;
    int debugNotesBias = 0;
    if (playNotes) {
//        unsigned start = playStart - 1;
        --playStart;
        unsigned sStart = INT_MAX;
        do {
            ++debugIterations;
            const auto& note = n.notes[++playStart];
            if (note.startTime > midiTime) {
                break;
            }
            // if not running, only play note on if it is in selection
            if (!running && playStart >= n.selectEnd) {
                break;
            }
            if (NOTE_OFF == note.type) {
                this->setExpiredGateLow(note);
            }
            if (NOTE_ON != note.type) {
                continue;
            }
            int endTime = note.endTime();
            if (endTime <= midiTime) {
                continue;
            }
            ++debugNotesPlay;
            auto& channelInfo = channels[note.channel];
            SCHMICKLE((uint8_t) -1 != note.voice);
            unsigned voiceIndex = note.voice;
            auto& voice = channelInfo.voices[voiceIndex];
            if (&note == voice.note) {
                continue;
            }
            ++debugNotesSet;
            voice.note = &note;
            voice.realStart = realSeconds;
            // to do : gate low should be set to sustain if slur is last note of non-running selection
//                voice.gateLow = note.slurStart() ? INT_MAX : 
//                        note.startTime + storage.current().channels[note.channel].sustain(note.duration);
//                voice.noteEnd = endTime;
            if (note.channel < CV_OUTPUTS) {
                outputs[GATE1_OUTPUT + note.channel].setVoltage(DEFAULT_GATE_HIGH_VOLTAGE, voiceIndex);
            } else if (note.channel < EXPANSION_OUTPUTS
                    && rightExpander.module && rightExpander.module->model == modelSuper8) {
                Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
        #if DEBUG_GATES
                if (debugVerbose && DEFAULT_GATE_HIGH_VOLTAGE
                        != channels[note.channel].voices[voiceIndex].gate) {
                    DEBUG("[%g] chan %d gate %d from %g to DEFAULT_GATE_HIGH_VOLTAGE", realSeconds,
                            note.channel, voiceIndex, channels[note.channel].voices[voiceIndex].gate);
                }
        #endif
                channels[note.channel].voices[voiceIndex].gate = DEFAULT_GATE_HIGH_VOLTAGE;
                message->exGate[note.channel - CV_OUTPUTS][voiceIndex] = DEFAULT_GATE_HIGH_VOLTAGE;
            }
            if (running) {
                sStart = std::min(sStart, playStart);
            }
            // recompute pitch all the time to prepare for tremelo / vibrato / slur / etc
            if (note.channel < EXPANSION_OUTPUTS) {
                ++debugNotesBias;
                float bias = -60.f / 12;  // MIDI middle C converted to 1 volt/octave
                const auto verticalWheel = ntw()->verticalWheel;
                if (mainWidget->runningWithButtonsOff()) {
                    bias += inputs[V_OCT_INPUT].getVoltage();
                    bias += ((int) verticalWheel->getValue() - 60) / 12.f;
                }
                if (rightExpander.module && rightExpander.module->model == modelSuper8) {
                    Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
                    if (note.channel >= CV_OUTPUTS) {
                        float newCV = bias + note.pitch() / 12.f;
                        channels[note.channel].voices[voiceIndex].cv = newCV;
                        message->exCv[note.channel - CV_OUTPUTS][voiceIndex] = newCV;
                    }
                    float velocity = note.onVelocity();
                    channels[note.channel].voices[voiceIndex].velocity = velocity;
                    message->exVelocity[note.channel][voiceIndex] = velocity;
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
            // stage select start to display so that other thread sets display start later
            if (INT_MAX != sStart && n.selectStart != sStart) {
                n.selectStart = sStart;  // jam result in so this thread plays right note
                ntw()->reqs.push({ReqType::setSelectStart, sStart}); // then compute full answer for display
            }
        }
    }
    if (rightExpander.module && rightExpander.module->model == modelSuper8) {
        if (debugVerbose && !playNotes) {
            Super8Data *message = (Super8Data*) rightExpander.module->leftExpander.producerMessage;
            if (message->exGate[0][0]) {
                DEBUG("[%g] expected zero gate", realSeconds);
                _schmickled();
            }
        }
        rightExpander.module->leftExpander.messageFlipRequested = true;
    }
#if DEBUG_CPU_TIME
    double endTime = system::getThreadTime();
    double elapsed = endTime - startTime;
    if (elapsed > worstTime) {
        if (++reported > 1) {
            worstTime = elapsed;
            reportWorstLater = true;
        }
    }
    if (0) reportWorstLater = true; // to do : remove this, debug debugging
    if (reportWorstLater && endTime - lastTime > 0.3) {
        DEBUG("worst time %g elapsed %g parts %g %g %g %g iter %d play %d set %d bias %d",
                 worstTime, elapsed,
                mid1 - startTime, mid2 - mid1, mid3 - mid2, endTime - mid3,
                debugIterations, debugNotesPlay, debugNotesSet, debugNotesBias);
        reportWorstLater = false;
        lastTime = endTime;
    }
#endif
}

void NoteTaker::onReset() {
    this->resetState();
    Module::onReset();
}

void NoteTaker::resetState(bool pushRequest) {
#if DEBUG_RUN
    if (debugVerbose) DEBUG("notetaker reset");
#endif
    for (auto& c : channels) {
        c.voiceCount = 0;
    }
    this->resetRun(pushRequest);
}

void NoteTaker::resetRun(bool pushRequest) {
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
    if (pushRequest) {
        this->ntw()->reqs.push(ReqType::resetDisplayRange);
    }
}


void NoteTaker::setExpiredGateLow(const DisplayNote& note) {
    auto c = note.channel;
    SCHMICKLE(c < CHANNEL_COUNT);
    auto& channel = channels[c];
    auto v = note.voice;
    if (v >= VOICE_COUNT) {
        Notes::DebugDump(n().notes);
    }
    SCHMICKLE(v < VOICE_COUNT);
    auto& voice = channel.voices[v];
    if (!voice.note) {
        return;
    }
    if (c < CV_OUTPUTS) {
        outputs[GATE1_OUTPUT + c].setVoltage(0, v);
    } else {
        channel.voices[v].gate = 0;
    }
    SCHMICKLE(voice.note);
    SCHMICKLE(voice.note->endTime() == note.startTime);
    SCHMICKLE(voice.note->pitch() == note.pitch());
    SCHMICKLE(voice.note->channel == note.channel);
    voice.note = nullptr;
}
    
// to do : add bool to note that some voice count has changed to skip this if there's nothing new
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
    playStart = this->ntw()->edit.voice ? n.selectStart : 0;
    // if not running, midi end time is end time of longest note on in selection
    if (this->isRunning() || n.selectEnd == n.notes.size()) {
        midiEndTime = n.notes[n.notes.size() - 1].endTime();
    } else {
        midiEndTime = n.notes[n.selectStart].endTime();
        for (unsigned index = n.selectStart + 1; index < n.selectEnd; ++index) {
            auto& note = n.notes[index];
            if (NOTE_ON == note.type) {
                midiEndTime = std::max(midiEndTime, note.endTime());
            }
        }
    }
#if DEBUG_RUN
    if (debugVerbose) DEBUG("setPlayStart playStart %u midiEndTime %d lastNote %s", playStart,
            midiEndTime, n.notes[lastNote].debugString(n.notes, nullptr, nullptr).c_str());
#endif
}

// to do : output debug data to show what set voice count did
void NoteTaker::setVoiceCount() {
    auto& n = this->n();
#if DEBUG_RUN
    DEBUG("setVoiceCount invalidVoiceCount %d", invalidVoiceCount);
#endif
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
    if (TRACK_END != n.notes[count].type) {
        Notes::DebugDump(n.notes);
    }
    SCHMICKLE(TRACK_END == n.notes[count].type);
    array<unsigned, CHANNEL_COUNT * 128> onVoice;
    onVoice.fill((unsigned) -1);
    for (unsigned index = 0; index < count; ++index) {
        auto& note = n.notes[index];
        if (NOTE_OFF == note.type) {
            if ((unsigned) -1 == onVoice[note.channel * 128 + note.pitch()]) {
                Notes::DebugDump(n.notes);
            }
            SCHMICKLE((unsigned) -1 != onVoice[note.channel * 128 + note.pitch()]);
            note.voice = onVoice[note.channel * 128 + note.pitch()];
            onVoice[note.channel * 128 + note.pitch()] = (unsigned) -1;
            continue;
        }
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
        onVoice[note.channel * 128 + note.pitch()] = note.voice;
#if DEBUG_VOICE_COUNT
        DEBUG("%u vCount %d chan %d %s", index, vCount, chan, note.debugString().c_str());
#endif
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
    if (interp <= 0 && debugVerbose) DEBUG("value %g horz %d floor %d next %d ceil %g interp %g",
            value, horzIndex, floor, next, ceil, interp);
    return stdTimePerQuarterNote / interp;
}
