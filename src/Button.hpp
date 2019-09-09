#pragma once

#include "DisplayNote.hpp"

struct Notes;
struct NoteTaker;
struct NoteTakerButton;
struct NoteTakerWidget;

struct ButtonBuffer : Widget {
	FramebufferWidget* fb = nullptr;

    ButtonBuffer(NoteTakerButton* );

    NoteTakerWidget* ntw() {
         return getAncestorOfType<NoteTakerWidget>();
    }
};

struct NoteTakerButton : Switch {
    float lastTransform[6] = {};
    unsigned slotNumber = INT_MAX;
    int animationFrame = 0;

    NoteTakerButton() {
        momentary = true;
    }

    void draw(const DrawArgs& args) override;

    FramebufferWidget* fb() const {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    virtual void fromJson(json_t* root) {
    }

    int getValue() const {
        return paramQuantity ? (int) paramQuantity->getValue() : 0;
    }

    virtual bool ledOn() const {
        return false;
    }

    NoteTakerWidget* ntw() {
        return getAncestorOfType<NoteTakerWidget>();
    }

    const NoteTakerWidget* ntw() const {
        return const_cast<NoteTakerButton*>(this)->getAncestorOfType<NoteTakerWidget>();
    }

    void onDoubleClick(const event::DoubleClick& ) override {
    }

    void onButton(const event::Button &e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT
                && (e.mods & RACK_MOD_MASK) == 0) {
            e.consume(nullptr);
        }
        Switch::onButton(e);
    }

    void onDragStart(const event::DragStart &e) override {
        Switch::onDragStart(e);
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
            return;
        }
        DEBUG("NoteTakerButton onDragStart af %d ledOn %d val %d",
                animationFrame, this->ledOn(), this->getValue());
        animationFrame = momentary ? 1 : (int) !this->ledOn();
        auto framebuffer = this->fb();
        if (framebuffer) {
            framebuffer->dirty = true;
        }
        DEBUG("onDragStart end af %d ledOn %d", animationFrame, this->ledOn());
    }

    void onDragEnd(const event::DragEnd &e) override {
        Switch::onDragEnd(e);
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
            return;
        }
        DEBUG("NoteTakerButton onDragEnd af %d ledOn %d val %d",
                animationFrame, this->ledOn(), this->getValue());
        auto framebuffer = this->fb();
        if (framebuffer) {
            framebuffer->dirty = true;
        }
        animationFrame = momentary ? 0 : (int) !!this->getValue();
    }

    virtual void onTurnOff() {
        this->setValue(0);
        fb()->dirty = true;
    }

    void reset() override {
        NoteTakerButton::onTurnOff();
        ParamWidget::reset();
    }

    int runAlpha() const;

    std::string runningTooltip() const {
        return "slot " + std::to_string(slotNumber + 1);
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

    bool stageSlot(const event::DragEnd& );

    virtual json_t *toJson() const {
        return nullptr;
    }
};

struct EditButton : NoteTakerButton {

    void draw(const DrawArgs& args) override {
        const int af = animationFrame;
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
        if (!momentary) {
            this->drawLED(vg);
        }
        NoteTakerButton::draw(args);
    }

    void drawLED(NVGcontext *vg) {
        const int af = animationFrame;
        nvgBeginPath(vg);
        nvgRect(vg, 4 + af, 8 - af, 11, 5);
        NVGcolor color = ledColor();
        if (!this->ledOn()) {
            color.r /= 2;
            color.g /= 2;
            color.b /= 2;
        }
        color = nvgTransRGBA(color, this->runAlpha());
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
        momentary = false;
    }

    bool ledOn() const override {
         return paramQuantity ? this->paramQuantity->getValue() : false;
    }

    void onDragEnd(const event::DragEnd &e) override;
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
    enum class State {
        unknown,
        running,
        cutAll,
        cutPart,
        clearClipboard,
        cutToClipboard,
        cutAndShift,
        insertCutAndShift,
    };

    State state = State::unknown;

    void draw(const DrawArgs& ) override;
    void getState();
    void onDragEnd(const event::DragEnd &e) override;
};

struct CutButtonToolTip : WidgetToolTip<CutButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<CutButton>::getDisplayValueString();
        if (!result.empty()) {
            return result;
        }
        widget->getState();
        switch (widget->state) {
            case CutButton::State::unknown: return "!unknown";
            case CutButton::State::running: return "(Running)";
            case CutButton::State::cutAll: return "All";
            case CutButton::State::cutPart: return "Editable Parts";
            case CutButton::State::clearClipboard: return "Clear Clipboard";
            case CutButton::State::cutToClipboard: return "to Clipboard (in place)";
            case CutButton::State::cutAndShift: return "to Clipboard";
            case CutButton::State::insertCutAndShift: return "Left and Discard";
            default:
                SCHMICKLE(0);
        }
        return "";
    }
};

// hidden button to dump notes for debugging
struct DumpButton : NoteTakerButton {
    void onDragEnd(const event::DragEnd &e) override;
};

struct FileButton : EditLEDButton {
    void draw(const DrawArgs& ) override;
};

struct FileButtonToolTip : WidgetToolTip<FileButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<FileButton>::getDisplayValueString();
        return result.empty() ? "or Save" : result;
    }
};

// insert adds new note to right of select, defaulting to selected note / cut buffer
// insert turns off select
struct InsertButton : EditButton {
    enum class State {
        unknown,
        running,
        middleCInPlace,
        middleCShift,
        dupInPlace,
        dupLeft,
        dupShift,
        clipboardInPlace,
        clipboardShift,
    };

    vector<DisplayNote> span;
    unsigned insertLoc = INT_MAX;
    int insertTime = INT_MAX;
    int lastEndTime = INT_MAX;
    State state = State::unknown;

    void getState();
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

// to do : determine how much will be inserted
struct InsertButtonToolTip : WidgetToolTip<InsertButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<InsertButton>::getDisplayValueString();
        if (!result.empty()) {
            return result;
        }
        widget->getState();
        switch (widget->state) {
            case InsertButton::State::unknown: return "!unknown";
            case InsertButton::State::running: return "(Running)";
            case InsertButton::State::middleCInPlace: return "Middle C (in place)";
            case InsertButton::State::middleCShift: return "Middle C";
            case InsertButton::State::dupInPlace: return "Selection (in place)";
            case InsertButton::State::dupLeft: return "Copy Left of Selection";
            case InsertButton::State::dupShift: return "Selection";
            case InsertButton::State::clipboardInPlace: return "Clipboard (in place)";
            case InsertButton::State::clipboardShift: return "Clipboard";
            default:
                SCHMICKLE(0);
        }
        return "";
    }
};

struct KeyButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct KeyButtonToolTip : WidgetToolTip<KeyButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<KeyButton>::getDisplayValueString();
        return result.empty() ? "Key Signature" : result;
    }
};

// stateful button that chooses part to add, and parts to insert before
// editStart / part / choose channel / add or cut
// select / [ part / choose channels to copy ] / insert / part / choose where to insert / add
// (no select) / part / choose channel / add or cut
struct PartButton : EditLEDButton {
    PartButton() {
        DEBUG("PartButton %p", this);
    }
    
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct PartButtonToolTip : WidgetToolTip<PartButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<PartButton>::getDisplayValueString();
        return result.empty() ? "to Edit" : result;
    }
};

// if selection, exchange note / reset
// if insertion, insert/delete rest
// if duration, horizontal changes rest size, vertical has no effect
struct RestButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct RestButtonToolTip : WidgetToolTip<RestButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<RestButton>::getDisplayValueString();
        return result.empty() ? "Rest" : result;
    }
};

struct RunButton : NoteTakerButton {
    int dynamicRunAlpha = 0;
    float dynamicRunTimer = 0;
    const float fadeDuration = 1;

    RunButton() {
        momentary = false;
    }

    void draw(const DrawArgs& args) override {
        const int af = animationFrame;
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
        nvgFillColor(vg, this->ledOn() ? nvgRGB(0xF7, 0x37, 0x17) : nvgRGB(0x77, 0x17, 0x07));
        nvgFill(vg);
    }

    bool ledOn() const override {
         return paramQuantity ? this->paramQuantity->getValue() : false;
    }

    void onDragEnd(const event::DragEnd &e) override;
};

struct RunButtonToolTip : ParamQuantity {

    std::string getDisplayValueString() override {
        return "Score";
    }
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

    SelectButton() {
        this->setState(State::single);
    }

    void draw(const DrawArgs& ) override;
    bool editEnd() const { return this->isExtend(); }
    bool editStart() const { return this->isSingle(); }

    void fromJson(json_t* root) override {
        NoteTakerButton::fromJson(root);
        selStart = json_integer_value(json_object_get(root, "selStart"));
        saveZero = json_integer_value(json_object_get(root, "saveZero"));
    }

    bool isOff() const { 
        return State::ledOff == (State) (int) this->getValue();
    }

    bool isExtend() const {
        return State::extend == (State) (int) this->getValue();
    }

    bool isSingle() const {
        return State::single == (State) (int) this->getValue();
    }

    void onDragEnd(const event::DragEnd &e) override;

    void reset() override {
        selStart = 1;
        saveZero = true;
        this->setState(State::single);
        EditLEDButton::reset();
        animationFrame = 1;
    }

    void setOff();
    // void setExtend();  // currently not called by anyone
    void setSingle();

    void setState(State state) {
        this->setValue((int) state);
    }

    NVGcolor ledColor() const override {
        if (State::extend == (State) (int) this->getValue()) {
            return nvgRGB(0x17, 0x37, 0xF7);
        }
        return EditLEDButton::ledColor();
    }

    json_t *toJson() const override {
        json_t* root = NoteTakerButton::toJson();
        if (!root) {
            root = json_object();
        }
        json_object_set_new(root, "selStart", json_integer(selStart));
        json_object_set_new(root, "saveZero", json_integer(saveZero));
        return root;
    }
};

struct SelectButtonToolTip : WidgetToolTip<SelectButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<SelectButton>::getDisplayValueString();
        if (!result.empty()) {
            return result;
        }
        int val = (int) this->getValue();
        switch (val) {
            case 0: return "Edit Mode";
            case 1: return "Insert Mode";
            case 2: return "Select Mode";
            default:
                SCHMICKLE(0);
        }
        return "";
    }
};

struct SlotButton : EditLEDButton {
    enum class Select {
        end,
        slot,
        repeat,
    };

    void draw(const DrawArgs& ) override;
};

struct SlotButtonToolTip : WidgetToolTip<SlotButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<SlotButton>::getDisplayValueString();
        return result.empty() ? "Playback Order" : result;
    }
};

struct SustainButton : EditLEDButton {
    void draw(const DrawArgs& ) override;
};

struct SustainButtonToolTip : WidgetToolTip<SustainButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<SustainButton>::getDisplayValueString();
        return result.empty() ? "Sustain / Release" : result;
    }
};

struct TempoButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TempoButtonToolTip : WidgetToolTip<TempoButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<TempoButton>::getDisplayValueString();
        return result.empty() ? "Tempo Change" : result;
    }
};

struct TieButton : EditLEDButton {
    bool setSlur = false;   // defaults true; selection is empty (all slurable notes are slurred)
    bool setTie = false;
    bool setTriplet = false;
    bool clearSlur = false;
    bool clearTie = false;
    bool clearTriplet = false;

    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TieButtonToolTip : WidgetToolTip<TieButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<TieButton>::getDisplayValueString();
        return result.empty() ? "Slur / Tie / Tuplet" : result;
    }
};

struct TimeButton : AdderButton {
    void draw(const DrawArgs& ) override;
    void onDragEnd(const event::DragEnd &e) override;
};

struct TimeButtonToolTip : WidgetToolTip<TimeButton> {

    std::string getDisplayValueString() override {
        std::string result = WidgetToolTip<TimeButton>::getDisplayValueString();
        return result.empty() ? "Time Signature" : result;
    }
};
