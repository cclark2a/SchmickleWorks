#pragma once

#include "SchmickleWorks.hpp"

struct NoteTaker;

struct NoteTakerButton : FramebufferWidget, MomentarySwitch {

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

    int af = 0;  // animation frame, 0 to 1
    bool hasLed = false;
    bool ledOn = false;
};

struct EditButton : NoteTakerButton {

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

    void onDragStart(EventDragStart &e) override;
};

struct EditLEDButton : EditButton {

    EditLEDButton() {
        hasLed = true;
    }

    void draw(NVGcontext *vg) override {
        EditButton::draw(vg);
        this->drawLED(vg);
    }

    void drawLED(NVGcontext *vg) {
        nvgBeginPath(vg);
        nvgRect(vg, 4 + af, 8 - af, 11, 5);
        nvgFillColor(vg, ledOn ? nvgRGB(0xF7, 0x37, 0x17) : nvgRGB(0x77, 0x17, 0x07));
        nvgFill(vg);
    }

};

// by default, wheels control pitch, duration
// by default, select is last note played
// to duplicate in place : select / horz / select / horz / insert
// to copy and paste     : select / horz / select / horz / select (off) / select / horz / insert
// to cut and paste      : select / horz / select / horz / cut    / horz / insert
// to delete range       : select / horz / select / horz / cut    / select (off)
// to edit range         : select / horz / select / horz / select (off) / horz or vert
// to insert multiple   : (select / horz / select / select (off)) / insert / insert / insert / ...

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
struct InsertButton : EditLEDButton {
    void draw(NVGcontext *vg) override {
        EditLEDButton::draw(vg);
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

    void onDragEnd(EventDragEnd &e) override;

    unsigned insertStart;  // to undo insertion if enter is not pressed
    unsigned insertEnd;
};

// select button permits moving selection before, after, and between notes
//   select turns off insert
//   select off discards cut buffer
//   pressing run / enter locks start of selection, horz wheel extends selection
//   wheel up / down transposes selection
struct SelectButton : EditLEDButton {
    void draw(NVGcontext *vg) override;
    bool editEnd() const { return ledOn && runEnterCount; }
    bool editStart() const { return ledOn && !runEnterCount; }
    void onDragEnd(EventDragEnd &e) override;

    int runEnterCount = 0;
    unsigned rangeStart;  // stored on enter press
    unsigned rangeEnd;
};

// wheel up / down chooses one channel or chooses all channels
// wheel left / right ???
struct PartButton : EditButton {
    void draw(NVGcontext *vg) override;
};

// if selection, exchange note / reset
// if insertion, insert/delete rest
// if duration, horizontal changes rest size, vertical has no effect
struct RestButton : EditButton {
    void draw(NVGcontext *vg) override;
};

// cut button, momentary (no led)
//   cut with single select saves cut, deletes one note
//   cut with extended select saves cut, deletes range, select select to single
//   cut with select button off deletes current selection, does not modify clipboard
//   select off discards clipboard
//   if cut is saved in clipboard, insert adds clipboard as paste
struct CutButton : EditButton {
    void draw(NVGcontext* vg) override {
        EditButton::draw(vg);
        nvgTranslate(vg, af + 9, 27 - af);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, -1);
        nvgLineTo(vg, 2, -3);   
        nvgLineTo(vg, 4, -3);
        nvgLineTo(vg, 1,  0);
        nvgLineTo(vg, 4,  3);
        nvgLineTo(vg, 2,  3);
        nvgLineTo(vg, 0,  1);
        nvgLineTo(vg, -2,  3);
        nvgLineTo(vg,  -4,  3);
        nvgLineTo(vg, -1,  0);
        nvgLineTo(vg, -4, -3);
        nvgLineTo(vg, -2, -3);
        nvgLineTo(vg, 0, -1);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
    }

    void onDragEnd(EventDragEnd &e) override;
};

struct RunEnterButton : NoteTakerButton {
    // to do : make a button that depresses
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

    bool enterMode() const;

    void onDragStart(EventDragStart &e) override {
        hasLed = !this->enterMode();
        NoteTakerButton::onDragStart(e);
    }

    bool running() const {
        return ledOn && !this->enterMode();
    }

    void onDragEnd(EventDragEnd &e) override;

};
