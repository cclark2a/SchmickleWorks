#pragma once

#include "SchmickleWorks.hpp"

struct NoteTakerButton : FramebufferWidget, MomentarySwitch {

    void draw(NVGcontext *vg) override {
        nvgScale(vg, .75, .75);
        // draw shadow
        nvgBeginPath(vg);
        nvgMoveTo(vg, 23, 0);
        nvgLineTo(vg, 26 - af, 9 - af * 3);
        nvgLineTo(vg, 26 - af, 58 - af * 3);
        nvgLineTo(vg, 7 - af, 58 - af * 3);
        nvgLineTo(vg, 3, 50);
        nvgLineTo(vg, 23, 50);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x11));
        nvgFill(vg);
        // draw hole
        nvgBeginPath(vg);
        nvgRect(vg, 3, 0, 20, 50);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
        // draw face
        nvgBeginPath(vg);
        nvgRect(vg, af, 4 - af, 19, 48);
        nvgFillColor(vg, nvgRGB(0xAA, 0xAA, 0xAA));
        nvgFill(vg);
        // draw top edge
        nvgBeginPath(vg);
        nvgMoveTo(vg, af, 4 - af);
        nvgLineTo(vg, 3, 1);
        nvgLineTo(vg, 22, 1);
        nvgLineTo(vg, 19 + af, 4 - af);
        nvgFillColor(vg, nvgRGB(0x77, 0x77, 0x77));
        nvgFill(vg);
        // draw side edge
        nvgBeginPath(vg);
        nvgMoveTo(vg, 19 + af, 4 - af);
        nvgLineTo(vg, 22, 1);
        nvgLineTo(vg, 22, 49);
        nvgLineTo(vg, 19 + af, 52 - af);
        nvgFillColor(vg, nvgRGB(0xCC, 0xCC, 0xCC));
        nvgFill(vg);
    }

    void onDragStart(EventDragStart &e) override {
	    EventAction eAction;
	    onAction(eAction);
	    dirty = true;
        af = af ? 0 : 1;
    }

    void onDragEnd(EventDragEnd &e) override;

    int af = 0;  // animation frame, 0 to 1
};

struct NoteTakerLEDButton : NoteTakerButton {

    void draw(NVGcontext *vg) override {
        NoteTakerButton::draw(vg);
        this->drawLED(vg);
    }

    void drawLED(NVGcontext *vg) {
        nvgBeginPath(vg);
        nvgRect(vg, 4 + af, 8 - af, 11, 5);
        nvgFillColor(vg, ledOn ? nvgRGB(0xF7, 0x37, 0x17) : nvgRGB(0x77, 0x17, 0x07));
        nvgFill(vg);
    }

    bool ledOn = false;
};

// duration on/off -- turning on turns insert off
// if select multiple
// wheel left / right changes duration of all notes
// wheel up / down tranposes selection
struct DurationButton : NoteTakerLEDButton {
    void draw(NVGcontext *vg) override;

    bool showLabel = false;
};

// insert/delete on/off -- turning on turns duration off
// if multiple select  is off
// horizontal moves selector between notes to all insertion positions
// wheel up inserts note
// wheel down deletes note to left or first note if there's nothing to the left
// if multiple select is on, wheel down cuts selection
// if horizontal moves selector, wheel down is disabled
//                           wheel up pastes / duplicates selection
struct InsertButton : NoteTakerLEDButton {
    void draw(NVGcontext *vg) override {
        NoteTakerButton::draw(vg);
        // replace this with a font character if one can be found
        nvgTranslate(vg, af, 6 - af);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 6, 20);
        nvgLineTo(vg, 8, 20);
        nvgArcTo(vg, 9, 20, 9, 21, 1);
        nvgLineTo(vg, 9, 32);
        nvgArcTo(vg, 9, 33, 8, 33, 1);
        nvgLineTo(vg, 6, 33);
        nvgMoveTo(vg, 12, 20);
        nvgLineTo(vg, 10, 20);
        nvgArcTo(vg, 9, 20, 9, 21, 1);
        nvgMoveTo(vg, 12, 33);
        nvgLineTo(vg, 10, 33);
        nvgArcTo(vg, 9, 33, 9, 32, 1);
        nvgStrokeColor(vg, nvgRGB(0, 0, 0));
        nvgStrokeWidth(vg, .5);
        nvgStroke(vg);
    }
};

// select single or multiple
// if multiple, insert off
// wheel left / right extends selection
// wheel up / down transposes selection
struct SelectButton : NoteTakerLEDButton {
    void draw(NVGcontext *vg) override;
};

// wheel up / down chooses one channel or chooses all channels
// wheel left / right ???
struct PartButton : NoteTakerLEDButton {
    void draw(NVGcontext *vg) override;
};

// if selection, exchange note / reset
// if insertion, insert/delete rest
// if duration, horizontal changes rest size, vertical has no effect
struct RestButton : NoteTakerButton {
    void draw(NVGcontext *vg) override;
};
