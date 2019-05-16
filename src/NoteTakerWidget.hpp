#pragma once

#include "NoteTakerDisplayNote.hpp"

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
    vector<DisplayNote> clipboard;
    vector<vector<uint8_t>> storage;
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
    bool debugVerbose = true;
#if RUN_UNIT_TEST
    bool runUnitTest = false;  // to do : ship with this disabled
    bool unitTestRunning = false;
#endif

    NoteTakerWidget(NoteTaker* module);
    void addButton(NoteTakerButton* );
    void addWheel(NoteTakerWheel* );
    void copyNotes();
    void copySelectableNotes();
    void debugDump(bool validatable = true, bool inWheel = false) const;
    void enableInsertSignature(unsigned loc);
    void eraseNotes(unsigned start, unsigned end);
    bool extractClipboard(vector<DisplayNote>* span) const;
    void fromJson(json_t* rootJ) override;
    unsigned horizontalCount() const;
    void insertFinal(int duration, unsigned insertLoc, unsigned insertSize);
    bool isEmpty() const;
    bool isSelectable(const DisplayNote& note) const;
    void loadScore();
    void makeNormal();
    void makeSlur();
    void makeTuplet();
    std::string midiDir() const;
    bool menuButtonOn() const;
        
    int musicFont() const {
        return _musicFont->handle;
    }

    int textFont() const {
        return _textFont->handle;
    }

    int nextStartTime(unsigned start) const;
    int noteCount() const;
    int noteToWheel(unsigned index, bool dbug = true) const;
    int noteToWheel(const DisplayNote& , bool dbug = true) const;
    Notes& n();
    const Notes& n() const;
    NoteTaker* nt();
    const NoteTaker* nt() const;
    void readStorage();
    bool resetControls();
    void resetRun();
    void resetState();
    bool runningWithButtonsOff() const;
    void saveScore();
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
