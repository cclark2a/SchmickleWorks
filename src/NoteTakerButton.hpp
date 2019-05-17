#pragma once

#include "SchmickleWorks.hpp"

struct Notes;
struct NoteTaker;
struct NoteTakerButton;
struct NoteTakerWidget;

// to do : turn off led buttons as needed : 
// e.g., partButton and fileButton are exclusive

struct ButtonBuffer : Widget {
    NoteTakerWidget* mainWidget = nullptr;
	FramebufferWidget* fb = nullptr;

    ButtonBuffer(NoteTakerWidget* , NoteTakerButton* );

    NoteTakerWidget* ntw() {
         return mainWidget;
    }
};

struct NoteTakerButton : ParamWidget {
    NoteTakerWidget* mainWidget;
    int af = 0;  // animation frame, 0 to 1
    bool hasLed = false;
    bool ledOn = false;

    NoteTakerButton() {
        this->setValue(0);
        this->setLimits(0, 1);
    }

    FramebufferWidget* fb() const {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    virtual void fromJson(json_t* root) {
        if (!hasLed) {
            return;
        }
        ledOn = json_boolean_value(json_object_get(root, "ledOn"));
        af = ledOn ? 1 : 0;
    }

    NoteTakerWidget* ntw() {
        return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
        return mainWidget;
    }

    void onDragStart(const event::DragStart &e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
            return;
        }
        auto framebuffer = this->fb();
        if (framebuffer) {
            framebuffer->dirty = true;
        }
        if (hasLed) {
            af = af ? 0 : 1;
        } else {
            af = 1;
        }
    }

    void onDragEnd(const event::DragEnd &e) override {
        auto framebuffer = this->fb();
        if (framebuffer) {
            framebuffer->dirty = true;
        }
        if (hasLed) {
            ledOn = !ledOn;
        } else {
            af = 0;
        }
    }

    virtual void onTurnOff() {
        ledOn = false;
        af = 0;
    }

    void reset() override {
        NoteTakerButton::onTurnOff();
        ParamWidget::reset();
    }

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

    virtual json_t *toJson() const {
        if (!hasLed) {
            return nullptr;
        }
        json_t* root = json_object();
        json_object_set_new(root, "ledOn", json_boolean(ledOn));
        return root;
    }
};

struct EditButton : NoteTakerButton {

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
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
        nvgFillColor(vg, nvgRGB(0xCC, 0xCC, 0xCC));
        nvgFill(vg);
        // draw side edge
        nvgBeginPath(vg);
        nvgMoveTo(vg, 19 + af, 4 - af);
        nvgLineTo(vg, 22, 1);
        nvgLineTo(vg, 22, 49);
        nvgLineTo(vg, 19 + af, 52 - af);
        nvgFillColor(vg, nvgRGB(0x77, 0x77, 0x77));
        nvgFill(vg);
        if (hasLed) {
            this->drawLED(vg);
        }
    }

    void drawLED(NVGcontext *vg) {
        nvgBeginPath(vg);
        nvgRect(vg, 4 + af, 8 - af, 11, 5);
        NVGcolor color = ledColor();
        if (!ledOn) {
            color.r /= 2;
            color.g /= 2;
            color.b /= 2;
        }
        nvgFillColor(vg, color);
        nvgFill(vg);
    }

    virtual NVGcolor ledColor() const {
        return nvgRGB(0xF7, 0x37, 0x17);
    }

    void onDragStart(const event::DragStart &e) override;
};

struct EditLEDButton : EditButton {

    EditLEDButton() {
        hasLed = true;
    }

};

// shared by time signature and rest, but not insert
struct AdderButton : EditButton {
    // locals are not read/written to json; in a struct as a convenience, not persistent
    unsigned insertLoc;
    unsigned shiftLoc;
    int startTime;
    int shiftTime;
    int duration; 

    void onDragEndPreamble(const event::DragEnd &e);
    void onDragEnd(const event::DragEnd &e) override;
};

// by default, wheels control pitch, duration
// by default, select is last note played
// to duplicate in place : select / horz / select / horz / insert
// to copy and paste     : select / horz / select / horz / select (off) / select / horz / insert
// to cut and paste      : select / horz / select / horz / cut    / horz / insert
// to delete range       : select / horz / select / horz / select (off) / cut   
// to edit range         : select / horz / select / horz / select (off) / horz or vert
// to insert multiple   : (select / horz) / insert / insert / insert / ...

// insert adds a note or clipboard, wheels edit pitch / duration of working note
//   and sets selection to note(s) added
//   cut removes last inserted note
// insert shows adds last cut to paste; to discard clipboard and insert a single note,
//   turn off select prior to pressing insert
// while select is on, enter toggles extend, horz wheel moves start/end (vert wheel pitch?)
// select off ignores clipboard; select off followed by select on preserves clipboard for paste

// stateful buttons : select
//   select off - wheels edit duration, pitch of selection
//              - cut deletes last selection / insertion
//              - insert duplicates prior note
//              - enter / run starts sequence
//   select on  - horz wheel edits selection, vert wheel edits pitch
//              - cut deletes and copies selection to clipboard
//              - insert duplicates clipboard (if cut after select) or selection
//              - enter / run (1st) extends selection
//              - enter / run (2nd) copies selection to clipboard, turns off select

// cut button, momentary (no led)
//   cut with single select saves cut, deletes one note
//   cut with extend select saves cut, deletes range, select select to single
//   cut with select button off deletes current selection, does not modify clipboard
//   select off discards clipboard
//   if cut is saved in clipboard, insert adds clipboard as paste
struct CutButton : EditButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};


// hidden button to dump notes for debugging
struct DumpButton : NoteTakerButton {
    void onDragEnd(const event::DragEnd &e) override;
};

struct FileButton : EditLEDButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

// insert adds new note to right of select, defaulting to selected note / cut buffer
// insert turns off select
struct InsertButton : EditButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct KeyButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

// stateful button that chooses part to add, and parts to insert before
// editStart / part / choose channel / add or cut
// select / [ part / choose channels to copy ] / insert / part / choose where to insert / add
// (no select) / part / choose channel / add or cut
struct PartButton : EditLEDButton {
    PartButton() {
        debug("PartButton %p", this);
    }
    
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

// if selection, exchange note / reset
// if insertion, insert/delete rest
// if duration, horizontal changes rest size, vertical has no effect
struct RestButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

// to do : consider allowing quick on/off to advance to next note in selection
struct RunButton : NoteTakerButton {
    RunButton() {
        hasLed = true;
    }

    // to do : make a run button that depresses
    void draw(const DrawArgs& args) override {
        auto vg = args.vg;
        // draw shadow
        nvgBeginPath(vg);
        nvgCircle(vg, 13 - af, 14 - af * 3, 10);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x11));
        nvgFill(vg);
        // draw hole
        nvgBeginPath(vg);
        nvgCircle(vg, 11, 10, 10);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
        // draw edge
        if (!af) {
            nvgBeginPath(vg);
            nvgCircle(vg, 11, 10, 9.5);
            nvgFillColor(vg, nvgRGB(0xCC, 0xCC, 0xCC));
            nvgFill(vg);
        }
        // draw face
        nvgBeginPath(vg);
        nvgCircle(vg, 10 + af, 11 - af, 9.5);
        nvgFillColor(vg, nvgRGB(0xAA, 0xAA, 0xAA));
        nvgFill(vg);
        // draw LED
        nvgBeginPath(vg);
        nvgCircle(vg, 10 + af, 11 - af, 5);
        nvgFillColor(vg, ledOn ? nvgRGB(0xF7, 0x37, 0x17) : nvgRGB(0x77, 0x17, 0x07));
        nvgFill(vg);
    }

    void onDragEnd(const event::DragEnd &e) override;
};

// select button permits moving selection before, after, and between notes
//   select off disables clipboard
//   select off / select on enables clipboard
//   pressing run / enter locks start of selection, horz wheel extends selection
//   wheel up / down transposes selection
struct SelectButton : EditLEDButton {
    enum class State {
        ledOff,         // redundant with !ledOn, may remove
        single,
        extend,
    };

    unsigned selStart = 1;   // note index where extend grows from, >= 1
    bool saveZero = true;    // set if single was at left-most position
    State state = State::single;

    SelectButton() {
        ledOn = true;
    }

    void draw(const DrawArgs& ) override;
    bool editEnd() const { return ledOn && State::extend == state; }
    bool editStart() const { return ledOn && State::single == state; }

    void fromJson(json_t* root) override {
        NoteTakerButton::fromJson(root);
        state = (State) json_integer_value(json_object_get(root, "state"));
        selStart = json_integer_value(json_object_get(root, "selStart"));
        saveZero = json_integer_value(json_object_get(root, "saveZero"));
    }

    void onDragEnd(const event::DragEnd &e) override;

    void onTurnOff() override {
        state = State::ledOff;
        EditLEDButton::onTurnOff();
    }

    void reset() override {
        selStart = 1;
        saveZero = true;
        state = State::single;
        EditLEDButton::reset();
        af = 1;
        ledOn = true;
    }

    void setOff();
    void setExtend();
    void setSingle();

    NVGcolor ledColor() const override {
        if (State::extend == state) {
            return nvgRGB(0x17, 0x37, 0xF7);
        }
        return EditLEDButton::ledColor();
    }

    json_t *toJson() const override {
        json_t* root = NoteTakerButton::toJson();
        json_object_set_new(root, "state", json_integer((int) state));
        json_object_set_new(root, "selStart", json_integer(selStart));
        json_object_set_new(root, "saveZero", json_integer(saveZero));
        return root;
    }
};

struct SustainButton : EditLEDButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TempoButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TieButton : EditLEDButton {
    enum class State {
        slur,
        normal,
        tuplet,
    };

    State state = State::normal;

    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TimeButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TrillButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

