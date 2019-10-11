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
    Vec gearSize;  // reversed for vertical wheel since it is drawn rotated
    float lastRealValue = INT_MAX;  // manually maintained
    int lastValue = INT_MAX;
    int shadow;

    NoteTakerWheel() {
    }

    virtual std::string debugString() const {
        auto pq = paramQuantity;
        std::string result;
        if (pq) {
            result = std::to_string(pq->getValue()) + " (" + std::to_string(pq->minValue) + ", "
                    + std::to_string(pq->maxValue) + ") ";
        }
        result += "speed=" + std::to_string(speed);
        return result;
    }

    void drawGear(NVGcontext *vg, float frame) const;

    FramebufferWidget* fb() const {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    void fromJson(json_t* root) {
        if (!paramQuantity) {
            return;
        }
        float minValue = paramQuantity->minValue;
        float maxValue = paramQuantity->maxValue;
        REAL_FROM_JSON(minValue);
        REAL_FROM_JSON(maxValue);
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

    void getLimits(float* lo, float* hi) const {
        if (paramQuantity) {
            *lo = paramQuantity->minValue;
            *hi = paramQuantity->maxValue;
        } else {
            *lo = *hi = 0;
        }
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

    bool hasRoundedChanged() {
        int result = (int) (this->getValue() + 0.5f);
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
        if (debugVerbose) DEBUG("%s value %g last %d", __func__, value, lastValue);
        if (paramQuantity) {
            paramQuantity->setValue(value);
        }
        lastValue = (int) value;
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
    int part() const;
    void onDragMove(const event::DragMove& e) override;

    void setDenominator(const DisplayNote& );
};

struct HorizontalWheelToolTip : WidgetToolTip<HorizontalWheel> {
    std::string getDisplayValueString() override;
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

struct VerticalWheelToolTip : WidgetToolTip<VerticalWheel> {
    std::string getDisplayValueString() override;
};

