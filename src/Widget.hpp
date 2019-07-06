#pragma once

#include "Edit.hpp"
#include "Storage.hpp"

struct CutButton;
struct DisplayBuffer;
struct DumpButton;
struct FileButton;
struct HorizontalWheel;
struct InsertButton;
struct KeyButton;
struct Notes;
struct NoteTaker;
struct NoteTakerButton;
struct NoteTakerDisplay;
struct NoteTakerWheel;
struct PartButton;
struct RestButton;
struct RunButton;
struct SelectButton;
struct SustainButton;
struct TempoButton;
struct TieButton;
struct TimeButton;
struct TrillButton;
struct VerticalWheel;

struct NoteTakerWidget : ModuleWidget {
    std::shared_ptr<Font> _musicFont = nullptr;
    std::shared_ptr<Font> _textFont = nullptr;
    Notes clipboard;
    SlotArray storage;
    NoteTakerEdit edit;
    CutButton* cutButton = nullptr;
    DisplayBuffer* displayBuffer = nullptr;
    DumpButton* dumpButton = nullptr;
    FileButton* fileButton = nullptr;
    HorizontalWheel* horizontalWheel = nullptr;
    InsertButton* insertButton = nullptr;
    KeyButton* keyButton = nullptr;
    NoteTakerDisplay* display = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    RunButton* runButton = nullptr;
    SelectButton* selectButton = nullptr;
    SustainButton* sustainButton = nullptr;
    TempoButton* tempoButton = nullptr;
    TieButton* tieButton = nullptr;
    TimeButton* timeButton = nullptr;
    TrillButton* trillButton = nullptr;
    VerticalWheel* verticalWheel = nullptr;
    const Vec editButtonSize;
    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    bool clipboardInvalid = true;
    const bool debugVerbose;
#if RUN_UNIT_TEST
    bool runUnitTest = true;  // to do : ship with this disabled
    bool unitTestRunning = false;
#endif

    NoteTakerWidget(NoteTaker* module);
    NoteTakerSlot* activeSlot();  // slot selected by wheel if file button active; or module slot
    template<class TButton> void addButton(TButton** , int paramId );
    void addButton(const Vec& size, NoteTakerButton* );
    void addWheel(const Vec& size, NoteTakerWheel* );
    void appendContextMenu(Menu *menu) override;
    void copyNotes();
    void copySelectableNotes();
    void copyToSlot(unsigned );
    void debugDump(bool validatable = true, bool inWheel = false) const;
    void disableEmptyButtons() const;
    bool displayUI_on() const;
    void enableButtons() const;
    void enableInsertSignature(unsigned loc);
    bool extractClipboard(vector<DisplayNote>* span = nullptr) const;
    void fromJson(json_t* rootJ) override;
    unsigned getSlot() const;  // index of module slot, not influenced by file button or wheel
    void insertFinal(int duration, unsigned insertLoc, unsigned insertSize);
    void invalidateAndPlay(Inval inval);
    void loadScore();
    void makeNormal();
    void makeSlur();
    void makeTuplet();
    bool menuButtonOn() const;
    DisplayNote middleC() const;
        
    int musicFont() const {
        return _musicFont->handle;
    }

    int textFont() const {
        return _textFont->handle;
    }

    int nextStartTime(unsigned start) const;
    int noteToWheel(unsigned index, bool dbug = true) const;
    int noteToWheel(const DisplayNote& , bool dbug = true) const;
    Notes& n();
    const Notes& n() const;
    NoteTaker* nt();
    const NoteTaker* nt() const;
    
    // ignore right-clicks on buttons
    // buttons are children of framebuffers, which are children of button buffers,
    // so need to look at great-grandchildren to get bounds
    void onButton(const event::Button &e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT
                && (e.mods & RACK_MOD_MASK) == 0) {
            if (debugVerbose) DEBUG("e.context %p pos %g/%g", e.context, e.pos.x, e.pos.y);
            for (auto& child : children) {
                auto param = dynamic_cast<ParamWidget*>(child);
                if (!param) {
                    param = child->getFirstDescendantOfType<ParamWidget>();
                }
                if (!param) {
                    continue;
                }
                if (param->box.isContaining(e.pos)) {
                    e.consume(nullptr);
                    break;
                }
            }
        }
        ModuleWidget::onButton(e);
    }

    void resetAndPlay();
    bool resetControls();
    void resetRun();
    void resetScore();
    void resetState();
    bool runningWithButtonsOff() const;
    void setClipboardLight();
    void setHorizontalWheelRange();
    void setSelectableScoreEmpty();
    void setSlot(unsigned index);
    void setVerticalWheelRange();
    void setWheelRange();
    void shiftNotes(unsigned start, int diff);
    json_t* toJson() override;
    void turnOffLedButtons(const NoteTakerButton* exceptFor = nullptr);

    unsigned unlockedChannel() const {
        for (unsigned x = 0; x < CHANNEL_COUNT; ++x) {
            if (selectChannels & (1 << x)) {
                return x;
            }
        }
        return 0;
    }

    void updateHorizontal();
    void updateVertical();
    unsigned wheelToNote(int value, bool dbug = true) const;  // maps wheel value to index in notes
    void writeStorage(unsigned index) const;
};
