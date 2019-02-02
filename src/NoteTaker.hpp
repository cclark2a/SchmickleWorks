#pragma once

#include "SchmickleWorks.hpp"
#include "NoteTakerDisplayNote.hpp"
#include "dsp/digital.hpp"
#include <float.h>

using std::vector;

struct DurationButton;

const uint8_t channel1 = 0;
const uint8_t channel2 = 1;
const uint8_t channel3 = 2;
const uint8_t channel4 = 3;

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
    bool running = true;
    SchmittTrigger runningTrigger;

    float phase = 0.0;

    vector<uint8_t> midi;
    vector<DisplayNote> displayNotes;
    vector<DisplayNote*> activeNotes;
	std::shared_ptr<Font> musicFont;
	std::shared_ptr<Font> textFont;
    DurationButton* durationButton = nullptr;
    unsigned displayFirst = 0;
    unsigned displayStep = 0;
    float elapsedSeconds = 0;
    DisplayNote* lastPitchOut = nullptr;
    bool stepOnce = false;

    NoteTaker();
    void backupOne();
	void step() override;

    void buttonChange(bool isOn, int paramId) {
        debug("isOn: %d paramId: %d\n", isOn, paramId);
    }

    void wheelBump(bool isHorizontal, bool positive);

};
