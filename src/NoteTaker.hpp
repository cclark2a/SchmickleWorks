#pragma once

#include "SchmickleWorks.hpp"
#include "NoteTakerDisplayNote.hpp"
#include "dsp/digital.hpp"
#include <array>
#include <float.h>

using std::array;
using std::vector;

struct DurationButton;
struct InsertButton;
struct PartButton;
struct RestButton;
struct SelectButton;
struct NoteTakerDisplay;
struct NoteTakerWheel;

constexpr float DEFAULT_GATE_HIGH_VOLTAGE = 5;
constexpr unsigned CV_OUTPUTS = 4;

struct NoteTaker : Module {
	enum ParamIds {
        RUN_BUTTON,
        DURATION_BUTTON,
        INSERT_BUTTON,
        EXTEND_BUTTON,
        PART_BUTTON,
        REST_BUTTON,
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
		RUNNING_LIGHT,
		NUM_LIGHTS
	};
    SchmittTrigger runningTrigger;

    vector<uint8_t> midi;
    vector<DisplayNote> allNotes;
    array<unsigned, channels> gateOut;     // index into allNotes of current gate out per channel
	std::shared_ptr<Font> musicFont;
	std::shared_ptr<Font> textFont;
    NoteTakerDisplay* display = nullptr;
    DurationButton* durationButton = nullptr;
    InsertButton* insertButton = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    SelectButton* selectButton = nullptr;
    NoteTakerWheel* horizontalWheel = nullptr;
    NoteTakerWheel* verticalWheel = nullptr;
    unsigned displayStart = 0;              // index into allNotes of first visible note
    unsigned displayEnd = 0;
    unsigned selectStart = 0;               // index into allNotes of first selected (any channel)
    unsigned selectEnd = 0;                 // one past last selected
    unsigned copyStart = 0;                 // copied from select when insert is activated
    unsigned copyEnd = 0;
    unsigned selectChannels = 0x0F;         // bit set for each active channel (all by default)
    float elapsedSeconds = 0;
    bool running = false;
    bool playingSelection = false;             // if set, provides feedback when editing notes

    NoteTaker();
    void debugDump() const;
    unsigned firstOn() const {
        for (unsigned index = 0; index < allNotes.size(); ++index) {
            if (NOTE_ON == allNotes[index].type) {
                return index;
            }
        }
        return 0;
    }

    int horizontalCount() const;

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

    DisplayNote* lastOn(unsigned index) {
        assert(index < allNotes.size());
        return this->lastOn(&allNotes[index]);
    }

    DisplayNote* lastOn(DisplayNote* note) {
        do {
            --note;
            assert(note != &allNotes.front());
        } while (NOTE_ON != note->type);
        return note;
    }

    int noteIndex(const DisplayNote& match) const;
    void outputNote(const DisplayNote& note);
    void playSelection();

    void setExpiredGatesLow(int midiTime) {
        for (unsigned channel = 0; channel < channels; ++channel) {
            unsigned index = gateOut[channel];
            if (!index) {
                continue;
            }
            const DisplayNote& gateOnNote = allNotes[index];
            assert(gateOnNote.channel == channel);
            if (gateOnNote.startTime + gateOnNote.duration <= midiTime) {
                gateOut[channel] = 0;
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
	void step() override;
    void updateHorizontal();
    void updateVertical();
};
