#pragma once

#include "SchmickleWorks.hpp"

struct NoteTakerWheel : Knob, FramebufferWidget {
    void drawGear(NVGcontext *vg, float frame);

    void fromJson(json_t* root) {
        speed = json_real_value(json_object_get(root, "speed"));
        minValue = json_real_value(json_object_get(root, "minValue"));
        maxValue = json_real_value(json_object_get(root, "maxValue"));
    }

    // to do: not working yet
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

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "speed", json_real(speed));
        json_object_set_new(root, "minValue", json_real(minValue));
        json_object_set_new(root, "maxValue", json_real(maxValue));
        return root;
    }

    Vec size;
    int shadow;
};


struct HorizontalWheel : NoteTakerWheel {
    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = 1;
        shadow = 3;
    }

    void draw(NVGcontext *vg) override {
        drawGear(vg, 1.f - fmodf(value + 1, 1));
    }

    void onDragMove(EventDragMove& e) override;
};

struct VerticalWheel : NoteTakerWheel {
    VerticalWheel() {
        size.y = box.size.x = 17;
        size.x = box.size.y = 100;
        speed = .1;
        shadow = 1;
    }

    void draw(NVGcontext *vg) override {
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        drawGear(vg, 1.f - fmodf(value + 1, 1));
    }

    void onDragMove(EventDragMove& e) override;
};
