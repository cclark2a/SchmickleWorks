#pragma once

#include "SchmickleWorks.hpp"

struct DisplayNote;
struct NoteTaker;
struct NoteTakerWidget;
struct NoteTakerWheel;
struct Notes;

struct WheelBuffer : Widget {
	FramebufferWidget* fb = nullptr;

    WheelBuffer(NoteTakerWheel* );

    NoteTakerWidget* ntw() {
         return getAncestorOfType<NoteTakerWidget>();
    }
};

struct NoteTakerWheel : app::SliderKnob {
    NoteTakerWidget* mainWidget;
    Vec gearSize;  // reversed for vertical wheel since it is drawn rotated
    float lastRealValue = INT_MAX;  // manually maintained
    int lastValue = INT_MAX;
    int shadow;
    bool inUse = false;

    NoteTakerWheel() {
    }

    virtual std::string debugString() const {
        auto pq = paramQuantity;
        return std::to_string(pq->getValue()) + " (" + std::to_string(pq->minValue) + ", "
                + std::to_string(pq->maxValue) + ") speed=" + std::to_string(speed);
    }

    void drawGear(NVGcontext *vg, float frame) const;

    FramebufferWidget* fb() const {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    void fromJson(json_t* root) {
        float minValue = json_real_value(json_object_get(root, "minValue"));
        float maxValue = json_real_value(json_object_get(root, "maxValue"));
        this->setLimits(minValue, maxValue); 
    }

    // to do: not working yet
    // to do: make this allow using scroll wheel to turn these wheels?
    void onHoverScroll(const event::HoverScroll &e) override {
        DEBUG("onHoverScroll %s [%g, %g] \n", horizontal ? "horz" : "vert", e.pos.x, e.pos.y);
	    Knob::onHoverScroll(e);
    }

    void onChange(const event::Change &e) override {
        this->fb()->dirty = true;
	    Knob::onChange(e);
    }

    void onDragEnd(const event::DragEnd& ) override;

    void onDragStart(const event::DragStart& ) override {
        inUse = true;
    }

    void reset() override {
        lastValue = INT_MAX;
        Knob::reset();
    }

    json_t* toJson() const {
        json_t* root = json_object();
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
        return getAncestorOfType<NoteTakerWidget>();
    }

    const NoteTakerWidget* ntw() const {
        return const_cast<NoteTakerWheel*>(this)->getAncestorOfType<NoteTakerWidget>();
    }

    NoteTaker* nt();
    const NoteTaker* nt() const;
    Notes* n();
    const Notes* n() const;

    void setLimits(float lo, float hi) {
        if (paramQuantity) {
            paramQuantity->minValue = lo;
            paramQuantity->maxValue = hi;
            speed = 50.f / paramQuantity->getRange();
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
        DEBUG("HorizontalWheel init");
        shadow = 3;
        horizontal = true;
        this->setValue(0);
        this->setLimits(0, 100);
    }

    std::string debugString() const override {
        return "horz " + NoteTakerWheel::debugString();
    }

    void draw(const DrawArgs& args) override {
        gearSize = box.size;
        drawGear(args.vg, 1.f - fmodf(this->getValue() + 1, 1));
    }

    // all = -1, 0 to CV_OUTPUTS - 1, all = CV_OUTPUTS
    int part() const {
        float fslot = this->getValue() + .5;
        return fslot < 0 ? -1 : (int) fslot;
    }

    void onDragMove(const event::DragMove& e) override;

    void setDenominator(const DisplayNote& );
};

struct VerticalWheel : NoteTakerWheel {
    VerticalWheel() {
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
        gearSize = Vec(box.size.y, box.size.x);
        drawGear(vg, 1.f - fmodf(this->getValue() + 1, 1));
    }

    void onDragMove(const event::DragMove& e) override;
};
