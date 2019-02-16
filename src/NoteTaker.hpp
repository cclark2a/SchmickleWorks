#pragma once

#include "SchmickleWorks.hpp"
#include "NoteTakerDisplayNote.hpp"
#include "dsp/digital.hpp"
#include <array>
#include <float.h>

using std::array;
using std::vector;

struct CutButton;
struct DurationButton;
struct InsertButton;
struct PartButton;
struct RestButton;
struct RunButton;
struct SelectButton;
struct NoteTakerDisplay;
struct NoteTakerWheel;

constexpr float DEFAULT_GATE_HIGH_VOLTAGE = 5;
constexpr unsigned CV_OUTPUTS = 4;

struct NoteTaker : Module {
	enum ParamIds {
        RUN_BUTTON,
        EXTEND_BUTTON,
        INSERT_BUTTON,
        CUT_BUTTON,
        REST_BUTTON,
        PART_BUTTON,
        BUTTON_6,
        BUTTON_7,
        BUTTON_8,
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

    vector<uint8_t> midi;
    vector<DisplayNote> allNotes;
    vector<DisplayNote> clipboard;
    array<int, channels> gateExpiration; // midi time when gate goes low
	std::shared_ptr<Font> musicFont;
	std::shared_ptr<Font> textFont;
    NoteTakerDisplay* display = nullptr;
    CutButton* cutButton = nullptr;
    DurationButton* durationButton = nullptr;
    InsertButton* insertButton = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    RunButton* runButton = nullptr;
    SelectButton* selectButton = nullptr;
    NoteTakerWheel* horizontalWheel = nullptr;
    NoteTakerWheel* verticalWheel = nullptr;
    unsigned displayStart = 0;              // index into allNotes of first visible note
    unsigned displayEnd = 0;
    unsigned selectStart = 0;               // index into allNotes of first selected (any channel)
    unsigned selectEnd = 0;                 // one past last selected
    unsigned selectChannels = 0x0F;         // bit set for each active channel (all by default)
    float elapsedSeconds = 0;
    bool playingSelection = false;             // if set, provides feedback when editing notes

    NoteTaker();

    void copyNotes() {
        clipboard.assign(allNotes.begin() + selectStart, allNotes.begin() + selectEnd);
    }

    void debugDump() const;

    void eraseNotes(unsigned start, unsigned end) {
        this->debugDump();
        debug("eraseNotes start %u end %u", start, end);
        allNotes.erase(allNotes.begin() + start, allNotes.begin() + end);
        this->debugDump();
    }

    unsigned horizontalCount() const;
    bool isEmpty() const;

    unsigned lastAt(int midiTime) const {
        assert(displayStart < allNotes.size());
        const DisplayNote* note = &allNotes[displayStart];
        do {
            ++note;
        } while (TRACK_END != note->type && note->startTime < midiTime);
        return note - &allNotes[0];
    }

    bool lastNoteEnded(unsigned index, int midiTime) const {
        assert(index < allNotes.size());
        const DisplayNote& last = allNotes[index];
        return last.startTime + last.duration <= midiTime;
    }

    int noteIndex(const DisplayNote& match) const;
    int nthNoteIndex(int value) const;  // maps wheel value to index in allNotes
    void outputNote(const DisplayNote& note);
    void playSelection();
    void setDisplayEnd();

    void setExpiredGatesLow(int midiTime) {
        for (unsigned channel = 0; channel < channels; ++channel) {
            int endTime = gateExpiration[channel];
            if (!endTime) {
                continue;
            }
            if (endTime < midiTime) {
                gateExpiration[channel] = 0;
                if (channel < CV_OUTPUTS) {
                    outputs[GATE1_OUTPUT + channel].value = 0;
                }
            }
        }
    }

    bool setSelectEnd(unsigned end) {
        if (end == selectStart) {
            do {
                ++end;
            } while (allNotes[selectStart].startTime == allNotes[end].startTime);       
        }
        if (selectEnd == end) {
            return false;
        }
        selectEnd = end;
        if (selectStart > selectEnd) {
            std::swap(selectStart, selectEnd);
        }
        this->setWheelRange();
        return true;
    }

    bool setSelectStart(unsigned start);
    void setSelectStartAt(int midiTime, unsigned displayStart, unsigned displayEnd);
    void setWheelRange();

    void shiftNotes(unsigned start, int diff) {
        debug("shift notes start %d diff %d", start, diff);
        for (unsigned index = start; index < allNotes.size(); ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
        }
        this->debugDump();
    }

	void step() override;
    void updateHorizontal();
    void updateVertical();

    void zeroGates() {
        gateExpiration.fill(0);
        for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
            outputs[GATE1_OUTPUT + index].value = 0;
        }
    }
};
