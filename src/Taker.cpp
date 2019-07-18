#include "Button.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

// to do : to run headless, allow mainWidget and ntw() to be nullptr

NoteTaker::NoteTaker()
    : debugVerbose(DEBUG_VERBOSE) {
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

bool NoteTaker::insertContains(unsigned loc, DisplayType type) const {
    unsigned before = loc;
    const DisplayNote* note;
    auto& n = this->n();
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
    return ntw()->runButton->ledOn();
}

void NoteTaker::playSelection() {
    const auto& n = this->n();
    elapsedSeconds = MidiToSeconds(n.notes[n.selectStart].startTime, n.ppq);
    elapsedSeconds *= stdMSecsPerQuarterNote / tempo;
    this->setPlayStart();
    int midiTime = SecondsToMidi(elapsedSeconds, n.ppq);
    this->advancePlayStart(midiTime, midiEndTime);
    const auto& storage = ntw()->storage;
    repeat = this->isRunning() ? ntw()->slotButton->ledOn() ?
            storage.playback[storage.selectStart].repeat : INT_MAX : 1;
}

// since this runs on a high frequency thread, avoid state except to play notes
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
    if (INT_MAX != stagedSlot) {
        if (DEBUG_VERBOSE) DEBUG("process stagedSlot %u", stagedSlot);
        ntw()->setSlot(stagedSlot);
        stagedSlot = INT_MAX;
        ntw()->invalidateAndPlay(Inval::load);
        this->resetRun();
        ntw()->resetAndPlay();
    }
    auto& n = this->n();
    bool running = this->isRunning();
    int localTempo = tempo;
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
                unsigned insertLoc = !n.noteCount(ntw()->selectChannels) ? this->atMidiTime(0) :
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
        clockPulse.trigger();
    }
    this->setOutputsVoiceCount();
    this->setExpiredGatesLow(midiTime);
    bool slotOn = ntw()->slotButton->ledOn();
    auto& storage = ntw()->storage;
    // if eos input is high, set bit to look for slot ending condition
    if (eosTrigger.process(inputs[EOS_INPUT].getVoltage())) {
        repeat = 1;
        midiEndTime = n.notes[playStart].cache->endStageTime;
        // to do : turn on slot button if off
        storage.slotEndTrigger = true;
    }
    if (!this->advancePlayStart(midiTime, midiEndTime)) {
        playStart = 0;
        if (running && slotOn) {
            storage.slotEndTrigger = false;
            if (repeat-- <= 1) {
                auto& selectStart = storage.selectStart;
                if (selectStart + 1 >= storage.playback.size()) {
                    repeat = 1;
                    running = false;
                    selectStart = 0;
                    storage.selectEnd = 1;
                    // to do : turn run button off
                } else {
                    const SlotPlay& slotPlay = storage.playback[++selectStart];
                    storage.selectEnd = selectStart + 1;
                    repeat = slotPlay.repeat;
                    this->stageSlot(slotPlay.index);
//                    return;
                }
            }
        }
        if (running) {
            ntw()->display->range.resetXAxisOffset();
            this->resetRun();
            eosPulse.trigger();
            this->advancePlayStart(0, midiEndTime);
        }
        if (!running) {
            this->setExpiredGatesLow(INT_MAX);
        }
        return;
    }
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
            voice.gateLow = note.slur() ? INT_MAX : 
                    note.startTime + slot->channels[note.channel].sustain(note.duration);
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
                bias += inputs[V_OCT_INPUT].getVoltage();
                bias += ((int) verticalWheel->getValue() - 60) / 12.f;
            }
            if (debugVerbose) DEBUG("setNote [%u] bias %g v_oct %g wheel %g pitch %g new %g old %g",
                note.channel, bias,
                inputs[V_OCT_INPUT].getVoltage(), verticalWheel->getValue(),
                note.pitch() / 12.f, bias + note.pitch() / 12.f,
                outputs[CV1_OUTPUT + note.channel].getVoltage(voiceIndex));
            outputs[CV1_OUTPUT + note.channel].setVoltage(bias + note.pitch() / 12.f, voiceIndex);
        }
    } while (true);
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
    Module::onReset();
}

void NoteTaker::resetState() {
    if (debugVerbose) DEBUG("notetaker reset");
    auto& n = this->n();
    n.notes.clear();
    for (auto& channel : slot->channels) {
        channel.reset();
    }
    for (auto& c : channels) {
        c.voiceCount = 0;
    }
    this->resetRun();
    this->setScoreEmpty();
    n.selectStart = 0;
    n.selectEnd = 1;
    mainWidget->resetState();
    mainWidget->resetControls();
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
    if (DEBUG_VERBOSE) DEBUG("setPlayStart lastNote %u", lastNote);
    midiEndTime = n.notes[lastNote].endTime();
    if (DEBUG_VERBOSE) DEBUG("setPlayStart midiEndTime %d", midiEndTime);
}

void NoteTaker::setScoreEmpty() {
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    if (debugVerbose) DebugDumpRawMidi(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, &slot->n.notes, nullptr, slot->channels);
    bool success = emptyParser.parseMidi();
    SCHMICKLE(success);
    ntw()->invalidateAndPlay(Inval::cut);
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
    auto& n = this->n();
    if (n.isEmpty(ntw()->selectChannels)) {
        n.selectStart = 0;
        n.selectEnd = 1;
        display->range.setRange(n);
        displayBuffer->fb->dirty = true;
        if (debugVerbose) DEBUG("setSelect set empty");
        return;
    }
    SCHMICKLE(start < end);
    SCHMICKLE(n.notes.size() >= 2);
    SCHMICKLE(end <= n.notes.size() - 1);
    display->range.setRange(n);
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
    const auto& n = this->n();
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
    const auto& n = this->n();
    unsigned end = start;
    do {
        ++end;
    } while (n.notes[start].startTime == n.notes[end].startTime && n.notes[start].isNoteOrRest()
            && n.notes[end].isNoteOrRest());
    this->setSelect(start, end);
    return true;
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
            DEBUG("%d over note %s", vCount, note.debugString().c_str());
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
        DEBUG("%u vCount %d chan %d %s", index, vCount, chan, note.debugString().c_str());
    }
}

void NoteTaker::stageSlot(unsigned slotIndex) {
    unsigned last = INT_MAX;
    if (slot) {
        last = slot - &this->ntw()->storage.slots.front();
        SCHMICKLE(last < SLOT_COUNT);
        if (slotIndex == last) {
            return;
        }
    }
    if (debugVerbose) DEBUG("stageSlot %u old %u", slotIndex, last);
    stagedSlot = slotIndex;
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
