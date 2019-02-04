#pragma once

#include "SchmickleWorks.hpp"
#include <float.h>

struct NoteTakerWheel : Knob, FramebufferWidget {
    void drawGear(NVGcontext *vg, float frame);

    // todo: not working yet
    void onScroll(EventScroll &e) override {
        e.consumed = true;
        debug("onScroll %s\n");
	    FramebufferWidget::onScroll(e);
    }

    void step() override {
        if (dirty) {

        }
        FramebufferWidget::step();
    }

    void onChange(EventChange &e) override {
        dirty = true;
	    Knob::onChange(e);
    }

    Vec size;
};


struct HorizontalWheel : NoteTakerWheel {
    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = 1;
    }

    void draw(NVGcontext *vg) override {
        drawGear(vg, 1.f - fmodf(value, 1));
    }

    void onDragMove(EventDragMove& e) override;
};

struct VerticalWheel : NoteTakerWheel {
    VerticalWheel() {
        size.y = box.size.x = 17;
        size.x = box.size.y = 100;
        speed = .1;
    }

    void draw(NVGcontext *vg) override {
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        drawGear(vg, 1.f - fmodf(value, 1));
    }

    void onDragMove(EventDragMove& e) override;
};
