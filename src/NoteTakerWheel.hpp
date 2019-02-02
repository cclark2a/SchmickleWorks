#pragma once

#include "SchmickleWorks.hpp"
#include <float.h>

struct NoteTakerWheel : Knob, FramebufferWidget {

    NoteTakerWheel() {
        lastValue = FLT_MAX;
    }

    void drawGear(NVGcontext *vg, float frame);

    void draw(NVGcontext *vg) override {
        drawGear(vg, fmodf(value, 1));
    }

    void onScroll(EventScroll &e) override {
        e.consumed = true;
        debug("onScroll %s\n", name());
        fflush(stdout);
	    FramebufferWidget::onScroll(e);
    }

    void onDragMove(EventDragMove& e) override;

    void step() override {
        if (dirty) {

        }
        FramebufferWidget::step();
    }

    void onChange(EventChange &e) override {
        dirty = true;
	    Knob::onChange(e);
    }

    virtual const char* name() = 0;
    Vec size;
    float lastValue;
    bool isHorizontal;
};


struct HorizontalWheel : NoteTakerWheel {

    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = .1;
        isHorizontal = true;
    }

    const char* name() override {
        return "horizontal";
    }
};

struct VerticalWheel : NoteTakerWheel {

    VerticalWheel() {
        size.y = box.size.x = 17;
        size.x = box.size.y = 100;
        speed = .1;
        isHorizontal = false;
    }

    void draw(NVGcontext *vg) override {
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        NoteTakerWheel::draw(vg);
    }

    const char* name() override {
        return "vertical";
    }
};
