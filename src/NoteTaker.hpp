#pragma once

#include "NoteTakerChannel.hpp"
#include "NoteTakerDisplayNote.hpp"

struct NoteTakerDisplay;
struct NoteTakerWidget;

// break out notes and range so that preview can draw notes without instantiated module
struct Notes {
    vector<DisplayNote> notes;
    unsigned selectStart = 0;               // index into notes of first selected (any channel)
    unsigned selectEnd = 1;                 // one past last selected
    int ppq = stdTimePerQuarterNote;        // default to 96 pulses/ticks per quarter note


    unsigned selectEndPos(unsigned select) const {
        const DisplayNote& first = notes[select];
        const DisplayNote* test;
        do {
            test = &notes[++select];
        } while (first.isNoteOrRest() && test->isNoteOrRest()
                && first.startTime == test->startTime);
        return select;
    }

    int xPosAtEndEnd(const NoteTakerDisplay* ) const;
    int xPosAtEndStart(const NoteTakerDisplay* ) const;
    int xPosAtStartEnd(const NoteTakerDisplay* ) const;
    int xPosAtStartStart(const NoteTakerDisplay* ) const;
    void validate() const;
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
        VERTICAL_WHEEL,   // 13
        HORIZONTAL_WHEEL, // 14
		NUM_PARAMS
	};
	enum InputIds {
		V_OCT_INPUT,      // 15
        CLOCK_INPUT,      // 16
        RESET_INPUT,      // 17
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
		NUM_LIGHTS
	};

    NoteTakerWidget* mainWidget;
    // state saved into json
    Notes n;
    array<NoteTakerChannel, CHANNEL_COUNT> channels;    // written to by step
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
    bool sawClockLow = false;
    bool sawResetLow = false;
    int debugMidiCount = 0;
    bool debugVerbose = true;

    NoteTaker();

    bool advancePlayStart(int midiTime, unsigned lastNote) {
        do {
            const auto& note = n.notes[playStart];
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
        for (unsigned index = 0; index < n.notes.size(); ++index) {
            const DisplayNote& note = n.notes[index];
            if (midiTime < note.startTime || TRACK_END == note.type
                    || (note.isNoteOrRest() && midiTime == note.startTime)) {
                return index;
            }
        }
        assert(0);  // should have hit track end
        return 0;
    }

    float beatsPerHalfSecond(int tempo) const;
    void dataFromJson(json_t* rootJ) override;
    json_t* dataToJson() override;

    void debugDumpChannels() const {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            auto& chan = channels[index];
            if (chan.noteIndex >= n.notes.size()) {
                continue;
            }
            debug("[%d] %s", index, chan.debugString().c_str());
        }
    }

    static void DebugDump(const vector<DisplayNote>& , const vector<NoteCache>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);
    static void DebugDumpRawMidi(vector<uint8_t>& v);

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

    // to do : figure out how to safely delineate start and end for insertion
    // probably don't need nextAfter and nextStartTime, both
    unsigned nextAfter(unsigned first, unsigned len) const {
        assert(len);
        unsigned start = first + len;
        const auto& priorNote = n.notes[start - 1];
        if (!priorNote.duration) {
            return start;
        }
        int priorTime = priorNote.startTime;
        int startTime = n.notes[start].startTime;
        if (priorTime < startTime) {
            return start;
        }
        for (unsigned index = start; index < n.notes.size(); ++index) {
            const DisplayNote& note = n.notes[index];
            if (note.startTime > startTime) {
                return index;
            }
        }
        assert(0);
        return INT_MAX;
    }

    unsigned noteIndex(const DisplayNote& note) const {
        return (unsigned) (&note - &n.notes.front());
    }

    void onReset() override;
    void playSelection();

    void resetRun();
    void resetState();

    void setScoreEmpty();

    void setGateLow(const DisplayNote& note) {
        auto& chan = channels[note.channel];
        chan.gateLow = 0;
        chan.noteEnd = 0;
    }

    void setExpiredGatesLow(int midiTime) {
        // to do : remove debugging code
        static array<int, CHANNEL_COUNT> debugLastGateLow {-1, -1, -1, -1};
        static array<int, CHANNEL_COUNT> debugLastNoteEnd {-1, -1, -1, -1};
        static array<int, CHANNEL_COUNT> debugCount {-1, -1, -1, -1};
        static int debugMidiTime = -1;
        for (unsigned channel = 0; channel < CHANNEL_COUNT; ++channel) {
            auto& chan = channels[channel];
            if (!chan.noteEnd) {
                continue;
            }
            if ((chan.gateLow && chan.gateLow < midiTime)
                    || chan.noteEnd < midiTime) {
                if (debugLastGateLow[channel] != chan.gateLow
                        || debugLastNoteEnd[channel] != chan.noteEnd
                        || (midiTime != debugMidiTime && midiTime != debugMidiTime + 1)
                        || debugCount[channel] > 500) {
                    debugLastGateLow[channel] = chan.gateLow;
                    debugLastNoteEnd[channel] = chan.noteEnd;
                    if (false) debug("expire [%u] gateLow=%d noteEnd=%d noteIndex=%u midiTime=%d",
                            channel, chan.gateLow, chan.noteEnd, chan.noteIndex, midiTime);
                    debugCount[channel] = 0;
                } else {
                    ++debugCount[channel];
                }
                debugMidiTime = midiTime;
            }
            if (chan.gateLow && chan.gateLow < midiTime) {
                chan.gateLow = 0;
                if (channel < CV_OUTPUTS) {
                    outputs[GATE1_OUTPUT + channel].value = 0;
                    debug("set expired low %d", channel);
                }
            }
            if (chan.noteEnd < midiTime) {
                chan.noteEnd = 0;
            }
        }
    }

    void setPlayStart();
    void setSelect(unsigned start, unsigned end);
    bool setSelectEnd(int wheelValue, unsigned end);
    bool setSelectStart(unsigned start);

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
  
	void step() override;

    // don't use std::sort function; use insertion sort to minimize reordering
    static void Sort(vector<DisplayNote>& notes, bool debugging = false) {
        if (debugging) debug("sort notes");
        for (auto it = notes.begin(), end = notes.end(); it != end; ++it) {
            auto const insertion_point = std::upper_bound(notes.begin(), it, *it);
            std::rotate(insertion_point, it, it + 1);
        }
    }

    float wheelToTempo(float value) const;

    void zeroGates() {
        if (debugVerbose) debug("zero gates");
        for (auto& channel : channels) {
            channel.noteIndex = INT_MAX;
            channel.gateLow = channel.noteEnd = 0;
        }
        for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
            outputs[GATE1_OUTPUT + index].value = 0;
        }
    }
};
