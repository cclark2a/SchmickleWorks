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
//    int gateLow = 0;        // midi time when gate goes low (start + sustain)
//    int noteEnd = 0;        // midi time when note expires (start + duration)
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

// circular buffer holds requests to modify notetaker state
// outside thread writes to buffer and then advances writer pointer when done
// note taker thread reads from buffer and advances reader pointer when done
enum class RequestType : unsigned {
    invalidateAndPlay,
    invalidateVoiceCount,
    onReset,
    resetAndPlay,
    resetPlayStart,
    setClipboardLight,
    setPlayStart,
};

struct RequestRecord {
    RequestType type;
    unsigned data;

    std::string debugStr() const {
        std::string result;
        switch(type) {
            case RequestType::invalidateAndPlay: return "invalidateAndPlay: "
                    + InvalDebugStr((Inval) data);
            case RequestType::invalidateVoiceCount: return "invalidateVoiceCount";
            case RequestType::onReset: return "onReset";
            case RequestType::resetAndPlay: return "resetAndPlay";
            case RequestType::resetPlayStart: return "resetPlayStart";
            case RequestType::setClipboardLight: return "setClipboardLight: "
                    + std::to_string((float) data / 256.f);
            case RequestType::setPlayStart: return "setPlayStart";
            default:
                assert(0);  // incomplete
        }
        return "";
    }
};

struct Requests {
    array<RequestRecord, 16> buffer;
    RequestRecord* reader;
    RequestRecord* writer;

    bool empty() const {
        return reader == writer;
    }

    RequestRecord pop() {
        SCHMICKLE(reader != writer);
        if (reader < writer) {
            writer = &buffer.front();
        }
        return *writer++;
    }

    void push(const RequestRecord& record) {
        RequestRecord* temp = reader == &buffer.back() ? &buffer.front() : reader;
        *temp = record;
        reader = temp + 1;
    }

    void push(RequestType type) {
        this->push({type, 0});
    }
};

// most state in note taker should not be altered by the display/ui thread
// to do : do not write playStart except directly or indirectly by process()
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

    Requests requests;
private:  // avoid directly accessing cross-thread stuff
    // state saved into json
    // written by step:
    array<Voices, CHANNEL_COUNT> channels;
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger eosTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::PulseGenerator clockPulse;
    dsp::PulseGenerator eosPulse;
    dsp::Timer resetTimer;
    // end of state saved into json; written by step
    NoteTakerWidget* mainWidget = nullptr;
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
    int tempo = stdMSecsPerQuarterNote;     // default to 120 beats/minute (500,000 ms per qn)
    // reset input state (not saved)
    float resetCycle = 0;
    float resetHighTime = FLT_MAX;
    int midiClockOut = INT_MAX;
    SlotPlay::Stage runningStage;
//    unsigned stagedSlotStart = INT_MAX;
    bool invalidVoiceCount = false;

public:
    NoteTaker();
    float beatsPerHalfSecond(int tempo) const;

    double getRealSeconds() const {
        return realSeconds;
    }

    int getTempo() const {
        return tempo;
    }

    static int OutputCount(const NoteTaker* nt) {
        return nt && nt->rightExpander.module && modelSuper8 == nt->rightExpander.module->model ?
                EXPANSION_OUTPUTS : CV_OUTPUTS;
    }

    // note: only called by unit test
    void resetState(bool pushRequest = true);

    void setMainWidget(NoteTakerWidget* widget) {
        mainWidget = widget;
    }

    float wheelToTempo(float value) const;

protected:
    void onReset() override;
    void process(const ProcessArgs &args) override;

private:
    // returns true if playstart could be advanced, false if end was reached
    bool advancePlayStart(int midiTime, int midiEndTime) {
        if (midiTime >= midiEndTime) {
            return false;
        }
#if DEBUG_CPU_TIME
        unsigned startPlayTime = playStart;
#endif
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
#if DEBUG_CPU_TIME
                if (debugVerbose) DEBUG("%d %s playTime %u to %u note %s end %d", midiTime, __func__,
                        startPlayTime, playStart, note.debugString().c_str(), note.endTime());
#endif
                return true;
            }
            switch (note.type) {
                case NOTE_OFF:
                    this->setExpiredGateLow(note);
                    break;
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
#if DEBUG_CPU_TIME
                if (debugVerbose) DEBUG("%s playTime %u to %u", __func__, startPlayTime, playStart);
#endif
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

    // to do : remove this -- require access to be const
    NoteTakerWidget* ntw() {
        return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
        return mainWidget;
    }

    void playSelection();
    void resetRun(bool pushRequest = true);

    void setClipboardLight(float brightness) {
        lights[CLIPBOARD_ON_LIGHT].setBrightness(brightness);
    }

    void setExpiredGateLow(const DisplayNote& note);

#if 0
    void setExpiredGatesLow(int midiTime) {
#if DEBUG_GATES
        static array<unsigned, CHANNEL_COUNT> debugVoiceCount;
#endif
        bool hasExpander = rightExpander.module && modelSuper8 == rightExpander.module->model;
        unsigned chanCount = hasExpander ? EXPANSION_OUTPUTS : CV_OUTPUTS;
        for (unsigned channel = 0; channel < chanCount; ++channel) {
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
//                if (voice.gateLow && voice.gateLow < midiTime) {
//                    voice.gateLow = 0;
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
                    } else {
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
//                }
//                if (voice.noteEnd < midiTime) {
//                    voice.noteEnd = 0;
//                }
            }
        }
    }
#endif

    void setPlayStart();
    void setOutputsVoiceCount();
    void setVoiceCount();

    void zeroGates() {
#if DEBUG_GATES
        if (debugVerbose) DEBUG("zero gates");
#endif
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            auto& c = channels[index];
            for (unsigned inner = 0; inner < c.voiceCount; ++inner) {
                auto& voice = c.voices[inner];
                voice.note = nullptr;
                voice.realStart = 0;
//                voice.gateLow = voice.noteEnd = 0;
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
                    channels[index].voices[inner].gate = 0;
                }
            }
        }
    }
};
