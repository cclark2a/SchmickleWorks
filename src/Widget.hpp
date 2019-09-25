#pragma once

#include "Button.hpp"
#include "Edit.hpp"
#include "Storage.hpp"

struct CutButton;
struct DisplayBuffer;
struct HorizontalWheel;
struct Notes;
struct NoteTaker;
struct NoteTakerDisplay;
struct NoteTakerWheel;
struct VerticalWheel;

// circular buffer holds requests to modify widget state
// taker thread writes to buffer and then advances writer pointer when done
// widget thread reads from buffer and advances reader pointer when done
enum class ReqType : unsigned {
    nothingToDo,
    resetDisplayRange,
    resetXAxisOffset,
    runButtonActivate,
    setSelectStart,
    stagedSlotStart,
};

struct ReqRecord {
    ReqType type;
    unsigned data;

    std::string debugStr() const {
        std::string result;
        switch(type) {
            case ReqType::nothingToDo: return "nothingToDo";
            case ReqType::resetDisplayRange: return "resetDisplayRange";
            case ReqType::resetXAxisOffset: return "resetXAxisOffset";
            case ReqType::runButtonActivate: return "runButtonActivate";
            case ReqType::setSelectStart: return "setSelectStart: " + std::to_string(data);
            case ReqType::stagedSlotStart: return "stagedSlotStart: " + std::to_string(data);
            default:
                assert(0);  // incomplete
        }
        return "";
    }
};

struct Reqs {
    array<ReqRecord, 16> buffer;
    ReqRecord* reader;
    ReqRecord* writer;

    ReqRecord pop() {
        if (reader < writer) {
            writer = &buffer.front();
        }
        if (reader == writer) {
            return {ReqType::nothingToDo, INT_MAX};
        }
        return *writer++;
    }

    void push(const ReqRecord& record) {
        ReqRecord* temp = reader == &buffer.back() ? &buffer.front() : reader;
        *temp = record;
        reader = temp + 1;
    }

    void push(ReqType type) {
        this->push({type, 0});
    }
};

struct Clipboard {
    vector<DisplayNote> notes;
    vector<SlotPlay> playback;

    void clear(bool slotOn) {
        slotOn ? resetSlots() : resetNotes();
    }

    void resetNotes() {
        notes.clear();
    }

    void resetSlots() {
        playback.clear();
        playback.emplace_back();
    }

    void fromJsonCompressed(json_t*);
    void fromJsonUncompressed(json_t*);
    json_t* playBackToJson() const;
    void toJsonCompressed(json_t* root) const;
};

extern Clipboard clipboard;  // experiment to see if global clipboard is accessible from instances

struct NoteTakerWidget : ModuleWidget {
    Reqs reqs;
    std::shared_ptr<Font> _musicFont = nullptr;
    std::shared_ptr<Font> _textFont = nullptr;
    SlotArray storage;
    NoteTakerEdit edit;
    CutButton* cutButton = nullptr;
    DisplayBuffer* displayBuffer = nullptr;
    FileButton* fileButton = nullptr;
    HorizontalWheel* horizontalWheel = nullptr;
    InsertButton* insertButton = nullptr;
    KeyButton* keyButton = nullptr;
    NoteTakerDisplay* display = nullptr;
    PartButton* partButton = nullptr;
    RestButton* restButton = nullptr;
    RunButton* runButton = nullptr;
    SelectButton* selectButton = nullptr;
    SlotButton* slotButton = nullptr;
    SustainButton* sustainButton = nullptr;
    TempoButton* tempoButton = nullptr;
    TieButton* tieButton = nullptr;
    TimeButton* timeButton = nullptr;
    VerticalWheel* verticalWheel = nullptr;
    const Vec editButtonSize;
    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    bool clipboardInvalid = true;
#if RUN_UNIT_TEST
    bool runUnitTest = true;  // to do : ship with this disabled
    bool unitTestRunning = false;
#endif

    NoteTakerWidget(NoteTaker* module);
    NoteTakerSlot* activeSlot();  // slot selected by wheel if file button active; or module slot
    template<class TButton, class TButtonToolTip> void addButton(TButton** , int paramId );
    void addButton(const Vec& size, NoteTakerButton* );
    void addWheel(const Vec& size, NoteTakerWheel* );
    void appendContextMenu(Menu *menu) override;
    void clearSlurs();
    void clearTriplets();
    void copyNotes();
    void copySlots();
    void copySelectableNotes();
    void copyToSlot(unsigned );
    void debugDump(bool validatable = true, bool inWheel = false) const;
    void disableEmptyButtons() const;
    void draw(const DrawArgs &args) override;
    void enableButtons() const;
    void enableInsertSignature(unsigned loc);
    bool extractClipboard(vector<DisplayNote>* span = nullptr) const;
    void fromJson(json_t* rootJ) override;
    unsigned getSlot() const;  // index of module slot, not influenced by file button or wheel
    void insertFinal(int duration, unsigned insertLoc, unsigned insertSize);
    void invalAndPlay(Inval );
    void loadScore();

    void makeSlurs();
    void makeTriplets();
    bool menuButtonOn() const;
    DisplayNote middleC() const;
        
    int musicFont() const {
        return _musicFont->handle;
    }

    int nextStartTime(unsigned start) const;
    int noteToWheel(unsigned index, bool dbug = true) const;
    int noteToWheel(const DisplayNote& , bool dbug = true) const;

    const Notes& n() const {
        return storage.current().n;
    }

    Notes& n() {
        return storage.current().n;
    }

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

    void onReset() {
        this->resetChannels();
        this->resetNotes();
        this->resetState();
    }

    // ok to call from either thread
    void redraw() const {
        displayBuffer->redraw();
    }

    void resetChannels();
    bool resetControls();
    void resetForPlay();
    void resetNotes();
    void resetScore();
    void resetState();
    void restoreNotes();
    bool runningWithButtonsOff() const;
    void setClipboardLight();
    void setHorizontalNoteLimits();
    void setHorizontalWheelRange();
    void setScoreEmpty();
    void setSelect(unsigned start, unsigned end);
    void setSelectableScoreEmpty();
    bool setSelectEnd(int wheelValue, unsigned end);
    bool setSelectStart(unsigned start);
    void setVerticalWheelRange();
    void setWheelRange();
    void shiftNotes(unsigned start, int diff);
    void stageSlot(unsigned slot);
    void step() override;

    int textFont() const {
        return _textFont->handle;
    }

    json_t* toJson() override;
    void turnOffLEDButtons(const NoteTakerButton* exceptFor = nullptr, bool exceptSlot = false);

    unsigned unlockedChannel() const {
        for (unsigned x = 0; x < CHANNEL_COUNT; ++x) {
            if (selectChannels & (1 << x)) {
                return x;
            }
        }
        return 0;
    }

    void updateHorizontal();
    void updateSlotVertical(SlotButton::Select );
    void updateVertical();
    unsigned wheelToNote(int value, bool dbug = true) const;  // maps wheel value to index in notes
    void writeStorage(unsigned index) const;
};
