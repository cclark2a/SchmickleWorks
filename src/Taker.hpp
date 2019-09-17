#pragma once

#include "Channel.hpp"
#include "Storage.hpp"

/* poly bugs
    - changing pitch of note needs to avoid other overlaps on same channel
    - add note while select end is on two voices adds two more
      but should add only one
    - transpose should add each note in case one interferes with the other 
    - what to do about rests when adding more voices?
*/

struct NoteTakerDisplay;
struct NoteTakerWidget;
struct DisplayNote;

struct Voice {
    const DisplayNote* note = nullptr;  // the note currently playing on this channel
    double realStart = 0;   // real time when note started (used to recycle voice)
    int gateLow = 0;        // midi time when gate goes low (start + sustain)
    int noteEnd = 0;        // midi time when note expires (start + duration)
    // for super eight, also record last state of cv / gate/ velocity
    float cv = 0;
    float gate = 0;
    float velocity = 0;

    std::string debugString(const DisplayNote* base) const;
};

struct Voices {
    array<Voice, VOICE_COUNT> voices;
    unsigned voiceCount = 0;
};

struct NoteTaker : Module {
	enum ParamIds {       // numbers used by unit test
        RUN_BUTTON,       // 0
        EXTEND_BUTTON,    // 1
        INSERT_BUTTON,    // 2
        CUT_BUTTON,       // 3
        REST_BUTTON,      // 4
        PART_BUTTON,      // 5
        FILE_BUTTON,      // 6
        SUSTAIN_BUTTON,   // 7
        TIME_BUTTON,      // 8
        KEY_BUTTON,       // 9
        TEMPO_BUTTON,     // 10
        TIE_BUTTON,       // 11
        SLOT_BUTTON,      // 12
        DUMP_BUTTON,      // 13
        VERTICAL_WHEEL,   // 14
        HORIZONTAL_WHEEL, // 15
		NUM_PARAMS
	};

	enum InputIds {
		V_OCT_INPUT,      // 16
        CLOCK_INPUT,      // 17
        EOS_INPUT,        // 18
        RESET_INPUT,      // 19
		NUM_INPUTS
	};

	enum OutputIds {
        CV1_OUTPUT,
        CV2_OUTPUT,
        CV3_OUTPUT,
        CV4_OUTPUT,
        GATE1_OUTPUT,
        GATE2_OUTPUT,
        GATE3_OUTPUT,
        GATE4_OUTPUT,
        CLOCK_OUTPUT,
        EOS_OUTPUT,
		NUM_OUTPUTS
	};
    
	enum LightIds {
        CLIPBOARD_ON_LIGHT,
		NUM_LIGHTS
	};

    const unsigned UNASSIGNED_VOICE_INDEX = (unsigned) -1;

    NoteTakerWidget* mainWidget = nullptr;
    // state saved into json
    // written by step:
    array<Voices, CHANNEL_COUNT> channels;
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger eosTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::PulseGenerator clockPulse;
    dsp::PulseGenerator eosPulse;
    dsp::Timer resetTimer;
    int tempo = stdMSecsPerQuarterNote;     // default to 120 beats/minute (500,000 ms per qn)
    // end of state saved into json; written by step
    double elapsedSeconds = 0;              // seconds into score (float is not enough bits)
    double realSeconds = 0;                 // seconds for UI timers
    unsigned playStart = 0;                 // index of notes output
    int midiEndTime = INT_MAX;
    int eosBase = INT_MAX;    // during playback, start of bar/quarter note in midi ticks
    int eosInterval = 0;      // duration of bar/quarter note in midi ticks
    unsigned repeat = INT_MAX;
    // clock input state (not saved)
    float clockCycle = 0;
    float clockHighTime = FLT_MAX;
    int externalClockTempo = stdMSecsPerQuarterNote;
    // reset input state (not saved)
    float resetCycle = 0;
    float resetHighTime = FLT_MAX;
    int midiClockOut = INT_MAX;
    SlotPlay::Stage runningStage;
    unsigned stagedSlotStart = INT_MAX;
    bool invalidVoiceCount = false;

    NoteTaker();

    // returns true if playstart could be advanced, false if end was reached
    bool advancePlayStart(int midiTime, int midiEndTime) {
        if (midiTime >= midiEndTime) {
            return false;
        }
        do {
            const auto& note = n().notes[playStart];
            if (midiTime < note.endTime()) {
                switch (runningStage) {
                    case SlotPlay::Stage::step:
                    case SlotPlay::Stage::beat:
                        eosBase = midiTime;
                        break;
                    case SlotPlay::Stage::quarterNote:
                        eosBase += (midiTime - eosBase) / n().ppq * n().ppq;
                        break;
                    case SlotPlay::Stage::bar:
                        eosBase += (midiTime  - eosBase) / eosInterval * eosInterval;
                        break;
                    default:
                        ;
                }
                return true;
            }
            switch (note.type) {
                case MIDI_TEMPO:
                    if (this->isRunning()) {
                        tempo = note.tempo();
                        externalClockTempo = 
                                (int) ((float) externalClockTempo / stdMSecsPerQuarterNote * tempo);
                        if (debugVerbose) DEBUG("tempo: %d extern: %d", tempo, externalClockTempo);
                    }
                    // fall through
                case KEY_SIGNATURE:
                    if (SlotPlay::Stage::bar == runningStage) {
                        eosBase = note.startTime;
                    }
                    break;
                case TIME_SIGNATURE:
                    if (SlotPlay::Stage::bar == runningStage) {
                        eosBase = note.startTime;
                        eosInterval = n().ppq * 4 * note.numerator() / (1 << note.denominator());
                    }
                    break;
                case TRACK_END:
                    return false;
                default:
                    ;
            }
            ++playStart;
            if (debugVerbose && playStart >= n().notes.size()) {
                DEBUG("playStart %u n().notes.size() %u", playStart, n().notes.size());
            }
            SCHMICKLE(playStart < n().notes.size());
        } while (true);
    }

    float beatsPerHalfSecond(int tempo) const;
    void dataFromJson(json_t* rootJ) override;
    json_t* dataToJson() override;

    void debugDumpVoices() const {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            auto& c = channels[index];
            for (unsigned inner = 0; inner < c.voiceCount; ++inner) {
                auto& v = c.voices[inner];
                if (!v.note) {
                    continue;
                }
                DEBUG("[%u / %u] %s", index, inner, v.debugString(&n().notes.front()).c_str());
            }
        }
    }

    int externalTempo();
    void invalidateAndPlay(Inval inval);
    bool isRunning() const;

    Notes& n();
    const Notes& n() const;

    NoteTakerWidget* ntw() {
        return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
        return mainWidget;
    }

    void onReset() override;

    int outputCount() const {
        return rightExpander.module && modelSuper8 == rightExpander.module->model ?
                EXPANSION_OUTPUTS : CV_OUTPUTS;
    }

    void playSelection();
    void process(const ProcessArgs &args) override;
    void resetRun();
    void resetState();

    void setClipboardLight(float brightness) {
        lights[CLIPBOARD_ON_LIGHT].setBrightness(brightness);
    }

    void setGateLow(const DisplayNote& note) {
        unsigned voiceIndex = note.voice;
        if (UNASSIGNED_VOICE_INDEX != voiceIndex) {
            auto& c = channels[note.channel];
            auto& v = c.voices[voiceIndex];
            v.note = nullptr;
            v.gateLow = 0;
            v.noteEnd = 0;
        }
    }

    void setExpiredGatesLow(int midiTime) {
#if DEBUG_GATES
        static array<unsigned, CHANNEL_COUNT> debugVoiceCount;
#endif
        bool hasExpander = rightExpander.module && modelSuper8 == rightExpander.module->model;
        for (unsigned channel = 0; channel < CHANNEL_COUNT; ++channel) {
            auto& c = channels[channel];
#if DEBUG_GATES
            if (debugVerbose && debugVoiceCount[channel] != c.voiceCount) {
                DEBUG("[%g] %s chan %u voice count %u", realSeconds, __func__, channel, c.voiceCount);
                debugVoiceCount[channel] = c.voiceCount;
            }
#endif
            for (unsigned index = 0; index < c.voiceCount; ++index) {
                auto& voice = c.voices[index];
                if (!voice.note) {
                    continue;
                }
                if (voice.gateLow && voice.gateLow < midiTime) {
                    voice.gateLow = 0;
                    if (channel < CV_OUTPUTS) {
            #if DEBUG_GATES
                        if (debugVerbose) {
                            if (outputs[GATE1_OUTPUT + channel].getVoltage(index)) {
                                DEBUG("[%g] %s set expired low [%u / %u]", realSeconds, __func__,
                                        channel, index);
                            }
                        }
            #endif
                        outputs[GATE1_OUTPUT + channel].setVoltage(0, index);
                    } else if (channel < EXPANSION_OUTPUTS && hasExpander) {
            #if DEBUG_GATES
                        if (debugVerbose) {
                            if (channels[channel].voices[index].gate) {
                                DEBUG("[%g] %s set expired low [%u / %u]", realSeconds, __func__,
                                        channel, index);
                            }
                        }
            #endif
                        channels[channel].voices[index].gate = 0;
                    }
                }
                if (voice.noteEnd < midiTime) {
                    voice.noteEnd = 0;
                }
            }
        }
    }

    void setOutputsVoiceCount();
    void setPlayStart();
    void setVoiceCount();
    void stageSlot(unsigned slot);
    float wheelToTempo(float value) const;

    void zeroGates() {
        if (debugVerbose) DEBUG("zero gates");
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            auto& c = channels[index];
            for (unsigned inner = 0; inner < c.voiceCount; ++inner) {
                auto& voice = c.voices[inner];
                voice.note = nullptr;
                voice.realStart = 0;
                voice.gateLow = voice.noteEnd = 0;
            }
        }
        for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
            for (unsigned inner = 0; inner < channels[index].voiceCount; ++inner) {
                outputs[GATE1_OUTPUT + index].setVoltage(0, inner);
            }
        }
        if (rightExpander.module && modelSuper8 == rightExpander.module->model) {
            for (unsigned index = CV_OUTPUTS; index < EXPANSION_OUTPUTS; ++index) {
                for (unsigned inner = 0; inner < channels[index].voiceCount; ++inner) {
                    if (debugVerbose && 4 == index && 0 == inner) {
                        DEBUG("zero gate [4][0] %p", &channels[index].voices[inner].gate);
                    }
                    channels[index].voices[inner].gate = 0;
                }
            }
        }
    }
};
