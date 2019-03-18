#pragma once

#include "NoteTakerChannel.hpp"
#include "NoteTakerDisplayNote.hpp"

struct CutButton;
struct FileButton;
struct InsertButton;
struct KeyButton;
struct NoteTakerButton;
struct PartButton;
struct RestButton;
struct RunButton;
struct SelectButton;
struct SustainButton;
struct TimeButton;
struct NoteTakerDisplay;
struct HorizontalWheel;
struct VerticalWheel;

struct NoteTaker : Module {
	enum ParamIds {
        RUN_BUTTON,
        EXTEND_BUTTON,
        INSERT_BUTTON,
        CUT_BUTTON,
        REST_BUTTON,
        PART_BUTTON,
        FILE_BUTTON,
        SUSTAIN_BUTTON,
        TIME_BUTTON,
        BUTTON_9,
        BUTTON_10,
        BUTTON_11,
        BUTTON_12,
        VERTICAL_WHEEL,
        HORIZONTAL_WHEEL,
		NUM_PARAMS
	};
	enum InputIds {
		V_OCT_INPUT,
        CLOCK_INPUT,
        RESET_INPUT,
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
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

    vector<vector<uint8_t>> storage;
	std::shared_ptr<Font> musicFont;
	std::shared_ptr<Font> textFont;
    // state saved into json
    vector<DisplayNote> allNotes;
    vector<DisplayNote> clipboard;
    array<NoteTakerChannel, CHANNEL_COUNT> channels;    // written to by step
    NoteTakerDisplay* display = nullptr;
    CutButton* cutButton = nullptr;
    FileButton* fileButton = nullptr;
    InsertButton* insertButton = nullptr;
    KeyButton* keyButton = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    RunButton* runButton = nullptr;
    SelectButton* selectButton = nullptr;
    SustainButton* sustainButton = nullptr;
    TimeButton* timeButton = nullptr;
    HorizontalWheel* horizontalWheel = nullptr;
    VerticalWheel* verticalWheel = nullptr;
    unsigned displayStart = 0;              // index into allNotes of first visible note
    unsigned displayEnd = 0;
    unsigned selectStart = 0;               // index into allNotes of first selected (any channel)
    unsigned selectEnd = 0;                 // one past last selected
    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    int tempo = stdMSecsPerQuarterNote;     // default to 120 beats/minute
    int ppq = stdTimePerQuarterNote;        // default to 96 pulses/ticks per quarter note
    // end of state saved into json; written by step
    float elapsedSeconds = 0;               // seconds into score
    float realSeconds = 0;                  // seconds for UI timers
    unsigned playStart = 0;                 // index of notes output
    
    NoteTaker();

    bool advancePlayStart(int midiTime, unsigned lastNote) {
        while (allNotes[playStart].endTime() <= midiTime) {
            if (playStart == lastNote) {
                return true;
            }
            ++playStart;
        }
        return false;
    }

    unsigned atMidiTime(int midiTime) const {
        for (unsigned index = 0; index < allNotes.size(); ++index) {
            const DisplayNote& note = allNotes[index];
            if (midiTime < note.startTime || TRACK_END == note.type
                    || ((NOTE_ON == note.type || REST_TYPE == note.type) 
                    && midiTime == note.startTime)) {
                return index;
            }
        }
        assert(0);  // should have hit track end
        return 0;
    }

    float beatsPerHalfSecond() const;

    void copyNotes() {
        clipboard.assign(allNotes.begin() + selectStart, allNotes.begin() + selectEnd);
    }

    void debugDumpChannels() const {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            auto& chan = channels[index];
            if (chan.noteIndex >= allNotes.size()) {
                continue;
            }
            debug("[%d] %u note=%s gateLow=%d noteEnd=%d", index, chan.noteIndex,
                    allNotes[chan.noteIndex].debugString().c_str(), chan.gateLow, chan.noteEnd);
        }
    }

    void debugDump(bool validatable = true, bool inWheel = false) const;
    static void DebugDump(const vector<DisplayNote>& , const vector<int>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);

    void eraseNotes(unsigned start, unsigned end) {
        this->debugDump();
        debug("eraseNotes start %u end %u", start, end);
        allNotes.erase(allNotes.begin() + start, allNotes.begin() + end);
        this->debugDump();
    }

    void fromJson(json_t *rootJ) override;
    unsigned horizontalCount() const;
    bool isEmpty() const;
    bool isRunning() const;
    bool isSelectable(const DisplayNote& note) const;

    int lastEndTime(unsigned end) const {
        int result = 0;
        for (unsigned index = 0; index < end; ++index) {
            const DisplayNote& note = allNotes[index];
            if (this->isSelectable(note)) {
                result = std::max(result, note.endTime());
            }
        }
        return result;
    }

    void loadScore();

    static void MapChannel(vector<DisplayNote>& notes, unsigned channel) {
         for (auto& note : notes) {
             if (NOTE_ON == note.type) {
                note.channel = channel;
             }
        }    
    }

    int nextStartTime(unsigned start) const {
        for (unsigned index = start; index < allNotes.size(); ++index) {
            const DisplayNote& note = allNotes[index];
            if (this->isSelectable(note)) {
                return note.startTime;
            }
        }
        return allNotes.back().startTime;
    }

    unsigned noteIndex(const DisplayNote& note) const {
        return (unsigned) (&note - &allNotes.front());
    }

    int noteToWheel(unsigned index, bool dbug = true) const {
        assert(index < allNotes.size());
        return noteToWheel(allNotes[index], dbug);
    }

    int noteToWheel(const DisplayNote& , bool dbug = true) const;
    void playSelection();

    void readStorage();
    void reset() override;
    void resetLedButtons(const NoteTakerButton* exceptFor = nullptr);

    void resetRun() {
        elapsedSeconds = 0;
        playStart = 0;
        this->setPlayStart();
}

    void resetControls();
    void saveScore();

    unsigned selectEndPos(unsigned select) const {
        const DisplayNote& first = allNotes[select];
        const DisplayNote* test;
        do {
            test = &allNotes[++select];
        } while (NOTE_ON == first.type && NOTE_ON == test->type
                && first.startTime == test->startTime);
        return select;
    }

    void setScoreEmpty();

    void setGateLow(const DisplayNote& note) {
        auto &chan = channels[note.channel];
        chan.gateLow = 0;
        chan.noteEnd = 0;
    }

    void setExpiredGatesLow(int midiTime) {
        // to do : remove debugging code
        static int debugLastGateLow = -1;
        static int debugLastNoteEnd = -1;
        static int debugMidiTime = -1;
        for (unsigned channel = 0; channel < CHANNEL_COUNT; ++channel) {
            auto &chan = channels[channel];
            if (!chan.noteEnd) {
                continue;
            }
            if ((chan.gateLow && chan.gateLow < midiTime) || chan.noteEnd < midiTime) {
                if (debugLastGateLow != chan.gateLow
                        || debugLastNoteEnd != chan.noteEnd
                        || (midiTime != debugMidiTime && midiTime != debugMidiTime + 1)) {
                    debugLastGateLow = chan.gateLow;
                    debugLastNoteEnd = chan.noteEnd;
                    debug("expire [%u] gateLow=%d noteEnd=%d noteIndex=%u midiTime=%d",
                            channel, chan.gateLow, chan.noteEnd, chan.noteIndex, midiTime);
                }
                debugMidiTime = midiTime;
            }
            if (chan.gateLow && chan.gateLow < midiTime) {
                chan.gateLow = 0;
                if (channel < CV_OUTPUTS) {
                    outputs[GATE1_OUTPUT + channel].value = 0;
                }
            }
            if (chan.noteEnd < midiTime) {
                chan.noteEnd = 0;
            }
        }
    }

    void setPlayStart();
    void setSelect(unsigned start, unsigned end);
    void setSelectEnd(int wheelValue, unsigned end);
    bool setSelectStart(unsigned start);
    void setHorizontalWheelRange();
    void setVerticalWheelRange();
    void setWheelRange();

    void shiftNotes(unsigned start, int diff) {
        debug("shiftNotes start %u diff %d selectChannels 0x%02x", start, diff, selectChannels);
        ShiftNotes(allNotes, start, diff, selectChannels);
    }

    // shift track end only if another shifted note bumps it out
    static void ShiftNotes(vector<DisplayNote>& notes, unsigned start, int diff,
            unsigned selectChannels = ALL_CHANNELS) {
        // After channel-specific notes are shifted, notes that affect all channels
        // may also need to be shifted. Since they are in the stream in time order,
        // move them enough to keep the order ascending.
        // to do : optimize this so that loop starts on last element (in time) of insertion
        int latest = 0;
        for (unsigned index = 0; index < start; ++index) {
            latest = std::max(latest, notes[index].endTime());
        }
        // Note that this logic assumes that notes can't overlap time or key signatures
        // prior to shifting.
        // to do : enforce that added time and key signatures clip any long notes before them
        for (unsigned index = start; index < notes.size(); ++index) {
            DisplayNote& note = notes[index];
            if (0xFF == note.channel) {
                if (diff < 0) { // move as much as diff allows without overlapping
                    note.startTime = std::max(latest, note.startTime + diff);
                } else { // move as little as possible, but no more than diff
                    note.startTime = std::min(latest, note.startTime + diff);
                }
            } else if (note.isSelectable(selectChannels)) {
                note.startTime += diff;
            }
            latest = std::max(latest, note.endTime());
        }
   }

	void step() override;
    json_t *toJson() override;
    void updateHorizontal();
    void updateVertical();
    void updateXPosition();
    void validate() const;
    unsigned wheelToNote(int value, bool dbug = true) const;  // maps wheel value to index in allNotes
    void writeStorage(unsigned index) const;

    void zeroGates() {
        debug("zero gates");
        for (auto& channel : channels) {
            channel.noteIndex = INT_MAX;
            channel.gateLow = channel.noteEnd = 0;
        }
        for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
            outputs[GATE1_OUTPUT + index].value = 0;
        }
    }
};
