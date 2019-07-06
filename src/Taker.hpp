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
        TIE_BUTTON,       // 10
        TRILL_BUTTON,     // 11
        TEMPO_BUTTON,     // 12
        DUMP_BUTTON,      // 13
        VERTICAL_WHEEL,   // 14
        HORIZONTAL_WHEEL, // 15
		NUM_PARAMS
	};

	enum InputIds {
		V_OCT_INPUT,      // 16
        CLOCK_INPUT,      // 17
        RESET_INPUT,      // 18
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
    NoteTakerSlot* slot = nullptr;
    // state saved into json
    // written by step:
    array<Voices, CHANNEL_COUNT> channels;
    int tempo = stdMSecsPerQuarterNote;     // default to 120 beats/minute (500,000 ms per q)
    // end of state saved into json; written by step
    float elapsedSeconds = 0;               // seconds into score
    double realSeconds = 0;                 // seconds for UI timers
    unsigned playStart = 0;                 // index of notes output
    // clock input state (not saved)
    float clockHighTime = FLT_MAX;
    float lastClock = 0;
    int externalClockTempo = stdMSecsPerQuarterNote;
    // reset input state (not saved)
    float resetHighTime = FLT_MAX;
    float eosTime = FLT_MAX;
    int midiClockOut = INT_MAX;
    float clockOutTime = FLT_MAX;
    unsigned stagedSlot = INT_MAX;
    bool sawClockLow = false;
    bool sawResetLow = false;
    bool invalidVoiceCount = false;
    const bool debugVerbose;          // enable this in note taker widget include

    NoteTaker();

    bool advancePlayStart(int midiTime, unsigned lastNote) {
        do {
            const auto& note = n().notes[playStart];
            if (note.isNoteOrRest() && note.endTime() > midiTime) {
                break;
            }
            if (playStart >= lastNote) {
                return true;
            }
            if (MIDI_TEMPO == note.type && this->isRunning()) {
                tempo = note.tempo();
                externalClockTempo = (int) ((float) externalClockTempo / stdMSecsPerQuarterNote
                        * tempo);
            }
            ++playStart;
        } while (true);
        return false;
    }

    unsigned atMidiTime(int midiTime) const {
        for (unsigned index = 0; index < n().notes.size(); ++index) {
            const DisplayNote& note = n().notes[index];
            if (midiTime < note.startTime || TRACK_END == note.type
                    || (note.isNoteOrRest() && midiTime == note.startTime)) {
                return index;
            }
        }
        return _schmickled();  // should have hit track end
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

    static void DebugDump(const vector<DisplayNote>& , const vector<NoteCache>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);
    static void DebugDumpRawMidi(const vector<uint8_t>& v);

    int externalTempo(bool clockEdge);
    bool extractClipboard(vector<DisplayNote>* ) const;
    bool insertContains(unsigned loc, DisplayType type) const;
    bool isRunning() const;

    static int LastEndTime(const vector<DisplayNote>& notes) {
        int result = 0;
        for (auto& note : notes) {
            result = std::max(result, note.endTime());
        }
        return result;
    }

    static void MapChannel(vector<DisplayNote>& notes, unsigned channel) {
         for (auto& note : notes) {
             if (note.isNoteOrRest()) {
                note.channel = channel;
             }
        }    
    }


    Notes& n() {
        return slot->n;
    }
    
    const Notes& n() const {
        return slot->n;
    }

    unsigned nextAfter(unsigned first, unsigned len) const {
        SCHMICKLE(len);
        unsigned start = first + len;
        const auto& priorNote = n().notes[start - 1];
        if (!priorNote.duration) {
            return start;
        }
        int priorTime = priorNote.startTime;
        int startTime = n().notes[start].startTime;
        if (priorTime < startTime) {
            return start;
        }
        for (unsigned index = start; index < n().notes.size(); ++index) {
            const DisplayNote& note = n().notes[index];
            if (note.isSignature() || note.startTime > startTime) {
                return index;
            }
        }
        _schmickled();
        return INT_MAX;
    }

    NoteTakerWidget* ntw() {
        return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
        return mainWidget;
    }

    void onReset() override;
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
    #define DEBUG_GATES 0
    #if DEBUG_GATES
        static array<int, CHANNEL_COUNT> debugLastGateLow {-1, -1, -1, -1};
        static array<int, CHANNEL_COUNT> debugLastNoteEnd {-1, -1, -1, -1};
        static array<int, CHANNEL_COUNT> debugCount {-1, -1, -1, -1};
        static int debugMidiTime = -1;
    #endif
        for (unsigned channel = 0; channel < CHANNEL_COUNT; ++channel) {
            auto& c = channels[channel];
            for (unsigned index = 0; index < c.voiceCount; ++index) {
                auto& voice = c.voices[index];
                if (!voice.note) {
                    continue;
                }
                if (voice.gateLow && voice.gateLow < midiTime) {
                    voice.gateLow = 0;
                    if (channel < CV_OUTPUTS) {
                        outputs[GATE1_OUTPUT + channel].setVoltage(0, index);
                        DEBUG("set expired low [%u / %u]", channel, index);
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
    void setScoreEmpty();
    void setSelect(unsigned start, unsigned end);
    bool setSelectEnd(int wheelValue, unsigned end);
    bool setSelectStart(unsigned start);
    void setVoiceCount();

    // shift track end only if another shifted note bumps it out
    // If all notes are selected, shift signatures. Otherwise, leave them be.
    static void ShiftNotes(vector<DisplayNote>& notes, unsigned start, int diff,
            unsigned selectChannels = ALL_CHANNELS) {
        bool sort = false;
        int trackEndTime = 0;
        bool hasTrackEnd = TRACK_END == notes.back().type;
        if (hasTrackEnd) {
            for (unsigned index = 0; index < start; ++index) {
                trackEndTime = std::max(trackEndTime, notes[index].endTime());
            }
        }
        for (unsigned index = start; index < notes.size(); ++index) {
            DisplayNote& note = notes[index];
            if ((note.isSignature() && ALL_CHANNELS == selectChannels)
                    || note.isSelectable(selectChannels)) {
                note.startTime += diff;
            } else {
                sort = true;
            }
            if (hasTrackEnd && TRACK_END != note.type) {
                trackEndTime = std::max(trackEndTime, note.endTime());
            }
        }
        if (hasTrackEnd) {
            notes.back().startTime = trackEndTime;
        }
        if (sort) {
            Sort(notes);
        }
   }

    // don't use std::sort function; use insertion sort to minimize reordering
    static void Sort(vector<DisplayNote>& notes, bool debugging = false) {
        if (debugging) DEBUG("sort notes");
        for (auto it = notes.begin(), end = notes.end(); it != end; ++it) {
            auto const insertion_point = std::upper_bound(notes.begin(), it, *it);
            std::rotate(insertion_point, it, it + 1);
        }
    }

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
    }
};
