#pragma once

#include "SchmickleWorks.hpp"

struct WheelWidget : widget::Widget {
	void draw(const DrawArgs& args) override;
};

struct NoteTakerWheel : app::Knob {
	widget::FramebufferWidget* fb;
    float lastRealValue = INT_MAX;  // manually maintained
    int lastValue = INT_MAX;
    Vec size;
    int shadow;

    NoteTakerWheel() {
        fb = new widget::FramebufferWidget;
        addChild(fb);
        WheelWidget* wh = new WheelWidget;
        fb->addChild(wh);
    }

    virtual std::string debugString() const {
        return std::to_string(paramQuantity->getValue()) + " (" + std::to_string(paramQuantity->minValue) + ", "
                + std::to_string(paramQuantity->maxValue) + ")";
    }

    void drawGear(NVGcontext *vg, float frame);

    void dataFromJson(json_t* root) {
        speed = json_real_value(json_object_get(root, "speed"));
        paramQuantity->minValue = json_real_value(json_object_get(root, "minValue"));
        paramQuantity->maxValue = json_real_value(json_object_get(root, "maxValue"));
    }

    // to do: not working yet
    void onHoverScroll(const event::HoverScroll &e) override {
        debug("onScroll %s\n");
	    Knob::onHoverScroll(e);
    }

    void step() override {
        fb->step();
    }

    void onChange(const event::Change &e) override {
        fb->dirty = true;
	    Knob::onChange(e);
    }

    void reset() override {
        lastValue = INT_MAX;
        Knob::reset();
    }

    json_t *dataToJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "speed", json_real(speed));
        json_object_set_new(root, "minValue", json_real(paramQuantity->minValue));
        json_object_set_new(root, "maxValue", json_real(paramQuantity->maxValue));
        return root;
    }

    bool hasChanged() {
        int result = (int) paramQuantity->getValue();
        if (result == lastValue) {
            return false;
        }
        lastValue = result;
        return true;
    }

    int wheelValue() const {
        return lastValue;
    }
};

void WheelWidget::draw(const DrawArgs& args) {
    auto wheel = this->getAncestorOfType<NoteTakerWheel>();
    wheel->draw(args);
}

struct HorizontalWheel : NoteTakerWheel {
    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = 1;
        shadow = 3;
        horizontal = true;
    }

    std::string debugString() const override {
        return "horz " + NoteTakerWheel::debugString();
    }

    void draw(NVGcontext *vg) override {
        drawGear(vg, 1.f - fmodf(paramQuantity->getValue() + 1, 1));
    }

    // all = -1, 0 to CV_OUTPUTS - 1, all = CV_OUTPUTS
    int part() const {
        float fslot = paramQuantity->getValue() + .5;
        return fslot < 0 ? -1 : (int) fslot;
    }

    void onDragMove(const event::DragMove& e) override;
};

struct VerticalWheel : NoteTakerWheel {
    VerticalWheel() {
        size.y = box.size.x = 17;
        size.x = box.size.y = 100;
        speed = .1;
        shadow = 1;
    }

    std::string debugString() const override {
        return "vert " + NoteTakerWheel::debugString();
    }

    void draw(NVGcontext *vg) override {
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        drawGear(vg, 1.f - fmodf(paramQuantity->getValue() + 1, 1));
    }

    void onDragMove(const event::DragMove& e) override;
};
