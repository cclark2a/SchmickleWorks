#pragma once

#include "SchmickleWorks.hpp"

struct NoteTakerWidget;
struct NoteTaker;
struct Notes;

struct WheelBuffer : Widget {
    NoteTakerWidget* mainWidget = nullptr;
	FramebufferWidget* fb = nullptr;

    WheelBuffer(ParamWidget* );

    NoteTakerWidget* ntw() {
         return mainWidget;
    }
};

struct NoteTakerWheel : app::Knob {
    NoteTakerWidget* mainWidget;
    float lastRealValue = INT_MAX;  // manually maintained
    int lastValue = INT_MAX;
    Vec size;
    int shadow;

    NoteTakerWheel() {
    }

    virtual std::string debugString() const {
        return std::to_string(paramQuantity->getValue()) + " (" + std::to_string(paramQuantity->minValue) + ", "
                + std::to_string(paramQuantity->maxValue) + ")";
    }

    void drawGear(NVGcontext *vg, float frame);

    FramebufferWidget* fb() const {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    void fromJson(json_t* root) {
        speed = json_real_value(json_object_get(root, "speed"));
        paramQuantity->minValue = json_real_value(json_object_get(root, "minValue"));
        paramQuantity->maxValue = json_real_value(json_object_get(root, "maxValue"));
    }

    // to do: not working yet
    void onHoverScroll(const event::HoverScroll &e) override {
        debug("onScroll %s\n");
	    Knob::onHoverScroll(e);
    }

    void onChange(const event::Change &e) override {
        this->fb()->dirty = true;
	    Knob::onChange(e);
    }

    void reset() override {
        lastValue = INT_MAX;
        Knob::reset();
    }

    json_t* toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "speed", json_real(speed));
        json_object_set_new(root, "minValue", json_real(paramQuantity->minValue));
        json_object_set_new(root, "maxValue", json_real(paramQuantity->maxValue));
        return root;
    }

    float getValue() const {
        if (paramQuantity) {
            return paramQuantity->getValue();
        }
        return 0;
    }

    bool hasChanged() {
        int result = (int) this->getValue();
        if (result == lastValue) {
            return false;
        }
        lastValue = result;
        return true;
    }

    NoteTakerWidget* ntw() {
        return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
        return mainWidget;
    }

    NoteTaker* nt();
    const NoteTaker* nt() const;
    Notes* n();
    const Notes* n() const;

    void setLimits(float lo, float hi) {
        if (paramQuantity) {
            paramQuantity->minValue = lo;
            paramQuantity->maxValue = hi;
        }
    }

    void setValue(float value) {
        if (paramQuantity) {
            paramQuantity->setValue(value);
        }
    }

    int wheelValue() const {
        return lastValue;
    }
};

struct HorizontalWheel : NoteTakerWheel {
    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = 1;
        shadow = 3;
        horizontal = true;
        this->setValue(0);
        this->setLimits(0, 100);
    }

    std::string debugString() const override {
        return "horz " + NoteTakerWheel::debugString();
    }

    void draw(const DrawArgs& args) override {
        drawGear(args.vg, 1.f - fmodf(this->getValue() + 1, 1));
    }

    // all = -1, 0 to CV_OUTPUTS - 1, all = CV_OUTPUTS
    int part() const {
        float fslot = this->getValue() + .5;
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
        this->setValue(60);
        this->setLimits(0, 127);
    }

    std::string debugString() const override {
        return "vert " + NoteTakerWheel::debugString();
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        drawGear(vg, 1.f - fmodf(this->getValue() + 1, 1));
    }

    void onDragMove(const event::DragMove& e) override;
};
