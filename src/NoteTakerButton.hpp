#pragma once

#include "SchmickleWorks.hpp"

struct NoteTaker;

struct NoteTakerButton : FramebufferWidget, MomentarySwitch {
    int af = 0;  // animation frame, 0 to 1
    bool hasLed = false;
    bool ledOn = false;

    virtual void fromJson(json_t* root) {
        if (!hasLed) {
            return;
        }
        ledOn = json_boolean_value(json_object_get(root, "ledOn"));
        af = ledOn ? 1 : 0;
    }

    void onDragStart(EventDragStart &e) override {
	    EventAction eAction;
	    onAction(eAction);
	    dirty = true;
        if (hasLed) {
            af = af ? 0 : 1;
        } else {
            af = 1;
        }
    }

    void onDragEnd(EventDragEnd &e) override {
	    dirty = true;
        if (hasLed) {
            ledOn = !ledOn;
        } else {
            af = 0;
        }
    }

    NoteTaker* nModule() {
        return (NoteTaker* ) module;
    }

    const NoteTaker* nModule() const {
        return (const NoteTaker* ) module;
    }

    void reset() override {
        ledOn = false;
        af = 0;
        MomentarySwitch::reset();
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
    EditButton() {
        box.size.x = 20;
        box.size.y = 40;
    }

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

    void onDragStart(EventDragStart &e) override;
};

struct EditLEDButton : EditButton {

    EditLEDButton() {
        hasLed = true;
    }

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

// insert adds new note to right of select, defaulting to selected note / cut buffer
// insert turns off select
struct InsertButton : EditButton {
    void draw(NVGcontext *vg) override;
    void onDragEnd(EventDragEnd &e) override;
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

    unsigned singlePos = 0;
    State state = State::ledOff;

    void draw(NVGcontext *vg) override;
    bool editEnd() const { return ledOn && State::extend == state; }
    bool editStart() const { return ledOn && State::single == state; }

    void fromJson(json_t* root) override {
        NoteTakerButton::fromJson(root);
        state = (State) json_integer_value(json_object_get(root, "state"));
        singlePos = json_integer_value(json_object_get(root, "singlePos"));
    }

    void onDragEnd(EventDragEnd &e) override;
    void reset() override;
    void setExtend();
    void setSingle() { state = State::single; ledOn = true; }

    NVGcolor ledColor() const override {
        if (State::extend == state) {
            return nvgRGB(0x17, 0x37, 0xF7);
        }
        return EditLEDButton::ledColor();
    }

    json_t *toJson() const override {
        json_t* root = NoteTakerButton::toJson();
        json_object_set_new(root, "state", json_integer((int) state));
        json_object_set_new(root, "singlePos", json_integer(singlePos));
        return root;
    }
};

// stateful button that chooses if vertical wheel changes pitch or part
// wheel up / down chooses one channel or chooses all channels
struct PartButton : EditLEDButton {
    unsigned lastChannels = 1; // saved channel last time part button pressed

    void draw(NVGcontext *vg) override;

    void fromJson(json_t* root) override {
        NoteTakerButton::fromJson(root);
        lastChannels = json_integer_value(json_object_get(root, "lastChannels"));
    }

    void onDragEnd(EventDragEnd &e) override;

    json_t *toJson() const override {
        json_t* root = NoteTakerButton::toJson();
        json_object_set_new(root, "lastChannels", json_integer(lastChannels));
        return root;
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

    void onDragEndPreamble(EventDragEnd &e);
    void onDragEnd(EventDragEnd &e) override;
};

// if selection, exchange note / reset
// if insertion, insert/delete rest
// if duration, horizontal changes rest size, vertical has no effect
struct RestButton : AdderButton {
    void draw(NVGcontext *vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

// cut button, momentary (no led)
//   cut with single select saves cut, deletes one note
//   cut with extend select saves cut, deletes range, select select to single
//   cut with select button off deletes current selection, does not modify clipboard
//   select off discards clipboard
//   if cut is saved in clipboard, insert adds clipboard as paste
struct CutButton : EditButton {
    void draw(NVGcontext* vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

// to do : consider allowing quick on/off to advance to next note in selection
struct RunButton : NoteTakerButton {
    RunButton() {
        hasLed = true;
        box.size.x = 25;
        box.size.y = 45;
    }

    // to do : make a run button that depresses
    void draw(NVGcontext* vg) override {
        nvgBeginPath(vg);
        nvgCircle(vg, 10, 10, 10);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, 10, 10, 5);
        nvgFillColor(vg, ledOn ? nvgRGB(0xF7, 0x37, 0x17) : nvgRGB(0x77, 0x17, 0x07));
        nvgFill(vg);
    }

    void onDragEnd(EventDragEnd &e) override;
};

struct FileButton : EditLEDButton {
    void draw(NVGcontext* vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

struct SustainButton : EditLEDButton {
    void draw(NVGcontext* vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

struct KeyButton : AdderButton {
    void draw(NVGcontext* vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

struct TimeButton : AdderButton {
    void draw(NVGcontext* vg) override;
    void onDragEnd(EventDragEnd &e) override;
};

// to do : remove below
// temporary button to dump notes for debugging
struct DumpButton : EditButton {
    void onDragEnd(EventDragEnd &e) override;
};
