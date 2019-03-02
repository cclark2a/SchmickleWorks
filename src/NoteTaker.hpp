#pragma once

#include "NoteTakerChannel.hpp"
#include "NoteTakerDisplayNote.hpp"

struct CutButton;
struct FileButton;
struct InsertButton;
struct PartButton;
struct RestButton;
struct RunButton;
struct SelectButton;
struct SustainButton;
struct TimeButton;
struct NoteTakerDisplay;
struct NoteTakerWheel;

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
    array<NoteTakerChannel, CHANNEL_COUNT> channels;
    NoteTakerDisplay* display = nullptr;
    CutButton* cutButton = nullptr;
    FileButton* fileButton = nullptr;
    InsertButton* insertButton = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    RunButton* runButton = nullptr;
    SelectButton* selectButton = nullptr;
    SustainButton* sustainButton = nullptr;
    TimeButton* timeButton = nullptr;
    NoteTakerWheel* horizontalWheel = nullptr;
    NoteTakerWheel* verticalWheel = nullptr;
    unsigned displayStart = 0;              // index into allNotes of first visible note
    unsigned displayEnd = 0;
    unsigned selectStart = 0;               // index into allNotes of first selected (any channel)
    unsigned selectEnd = 0;                 // one past last selected
    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    int tempo = 500000;                     // default to 120 beats/minute
    int ppq = 96;                           // default to 96 pulses/ticks per quarter note
    // end of state saved into json
    float elapsedSeconds = 0;
    bool playingSelection = false;          // if set, provides feedback when editing notes
    
    NoteTaker();

    void alignStart() {
        if (!selectStart && !this->isEmpty()) {
            unsigned start = this->wheelToNote(0);
            unsigned end = std::max(start + 1, selectEnd);
            this->setSelect(start, end);
        }
    }

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

    void debugDump(bool validatable = true) const;
    static void DebugDump(const vector<DisplayNote>& , const vector<int>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);

    void eraseNotes(unsigned start, unsigned end) {
        this->debugDump();
        debug("eraseNotes start %u end %u", start, end);
        allNotes.erase(allNotes.begin() + start, allNotes.begin() + end);
        this->debugDump();
    }

    // returns lowest numbered selectChannel, channel-wide edits affect all select
    unsigned firstChannel() const {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            if (selectChannels & (1 << index)) {
                return index;
            }
        }
        assert(0);  // at least one should always be selected
        return INT_MAX;
    }

    void fromJson(json_t *rootJ) override;
    unsigned horizontalCount() const;
    unsigned isBestSelectStart(int midiTime) const;
    bool isEmpty() const;
    bool isRunning() const;
    bool isSelectable(const DisplayNote& note) const;

    bool lastNoteEnded(unsigned index, int midiTime) const {
        assert(index < allNotes.size());
        const DisplayNote& last = allNotes[index];
        return last.endTime() <= midiTime;
    }

    void loadScore();

    static void MapChannel(vector<DisplayNote>& notes, unsigned channel) {
         for (auto& note : notes) {
             if (NOTE_ON == note.type) {
                note.channel = channel;
             }
        }    
    }

    unsigned noteIndex(const DisplayNote& note) const {
        return (unsigned) (&note - &allNotes.front());
    }

    int noteToWheel(unsigned index) const {
        assert(index < allNotes.size());
        return noteToWheel(allNotes[index]);
    }

    int noteToWheel(const DisplayNote& ) const;
    unsigned wheelToNote(int value) const;  // maps wheel value to index in allNotes
    void outputNote(const DisplayNote& note, int midiTime);
    void outputNoteAt(int midiTime);
    void playSelection();
    void readStorage();
    void reset() override;
    void resetButtons();
    void saveScore();
    void setScoreEmpty();

    void setGateLow(const DisplayNote& note) {
        auto &chan = channels[note.channel];
        chan.gateLow = 0;
        chan.noteEnd = 0;
    }

    void setExpiredGatesLow(int midiTime) {
        for (unsigned channel = 0; channel < CHANNEL_COUNT; ++channel) {
            auto &chan = channels[channel];
            if (!chan.noteEnd) {
                continue;
            }
            if (chan.gateLow && chan.gateLow < midiTime) {
                debug("expire gateLow=%d midiTime=%d", chan.gateLow, midiTime);
                chan.gateLow = 0;
                if (channel < CV_OUTPUTS) {
                    outputs[GATE1_OUTPUT + channel].value = 0;
                }
            }
            if (chan.noteEnd < midiTime) {
                debug("expire noteEnd=%d midiTime=%d", chan.noteEnd, midiTime);
                chan.noteEnd = 0;
            }
        }
    }

    void setSelect(unsigned start, unsigned end);
    bool setSelectEnd(int wheelValue, unsigned end);
    bool setSelectStart(unsigned start);
    void setSelectStartAt(int midiTime);
    void setWheelRange();

    void shiftNotes(unsigned start, int diff) {
        if (ALL_CHANNELS == selectChannels) {
            return ShiftNotes(allNotes, start, diff);
        }
        for (unsigned index = start; index < allNotes.size(); ++index) {
            DisplayNote& note = allNotes[index];
            if (note.isSelectable(selectChannels)) {
                note.startTime += diff;
            }
            // to do : if adjusted note overlaps rest, reduce rest
            //         if adjusted note overlaps measure, insert rest to create new one
        }
    }

    static void ShiftNotes(vector<DisplayNote>& notes, unsigned start, int diff) {
         for (unsigned index = start; index < notes.size(); ++index) {
            notes[index].startTime += diff;
        }    
   }

	void step() override;
    json_t *toJson() override;
    void updateHorizontal();
    void updateVertical();
    void updateXPosition();
    void validate() const;
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
