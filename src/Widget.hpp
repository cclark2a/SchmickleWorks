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
    StorageArray storage;
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

    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    bool clipboardInvalid = true;
    bool debugVerbose = DEBUG_VERBOSE;
#if RUN_UNIT_TEST
    bool runUnitTest = true;  // to do : ship with this disabled
    bool unitTestRunning = false;
#endif

    NoteTakerWidget(NoteTaker* module);
    void addButton(const Vec& size, NoteTakerButton* );
    void addWheel(const Vec& size, NoteTakerWheel* );
    void copyNotes();
    void copySelectableNotes();
    void debugDump(bool validatable = true, bool inWheel = false) const;
    bool displayUI_on() const;
    void enableInsertSignature(unsigned loc);
    bool extractClipboard(vector<DisplayNote>* span = nullptr) const;
    void fromJson(json_t* rootJ) override;
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
    
    void onButton(const event::Button &e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT
                && (e.mods & RACK_MOD_MASK) == 0) {
            e.consume(nullptr);
        }
        ModuleWidget::onButton(e);
    }

    bool resetControls();
    void resetRun();
    void resetState();
    bool runningWithButtonsOff() const;
    void setClipboardLight();
    void setHorizontalWheelRange();
    void setSelectableScoreEmpty();
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
