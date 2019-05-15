#pragma once

#include "NoteTakerDisplayNote.hpp"

struct NoteTaker;
struct NoteTakerButton;

struct NoteTakerWidget : ModuleWidget {
    std::shared_ptr<Font> musicFont = nullptr;
    std::shared_ptr<Font> textFont = nullptr;
    vector<DisplayNote> clipboard;
    vector<vector<uint8_t>> storage;
    unsigned selectChannels = ALL_CHANNELS; // bit set for each active channel (all by default)
    bool clipboardInvalid = true;
    bool debugVerbose = true;
#if RUN_UNIT_TEST
    bool runUnitTest = false;  // to do : ship with this disabled
    bool unitTestRunning = false;
#endif

    NoteTakerWidget(NoteTaker* module);
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
    int nextStartTime(unsigned start) const;
    int noteCount() const;
    int noteToWheel(unsigned index, bool dbug = true) const;
    int noteToWheel(const DisplayNote& , bool dbug = true) const;
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
    void validate() const;
    unsigned wheelToNote(int value, bool dbug = true) const;  // maps wheel value to index in notes

    template <class T> T* widget() {
        return this->getFirstDescendantOfType<T>();
    }

    template <class T> const T* widget() const {
        return const_cast<const T*>(
                const_cast<NoteTakerWidget*>(this)->getFirstDescendantOfType<T>());
    }

    void writeStorage(unsigned index) const;
};

static NoteTakerWidget* NTW(Widget* widget) {
    return widget->getAncestorOfType<NoteTakerWidget>();
}

static const NoteTakerWidget* NTW(const Widget* widget) {
    // to do : request VCV for const version ?
    return const_cast<Widget*>(widget)->getAncestorOfType<NoteTakerWidget>();
}

static inline NoteTaker* NT(Widget* widget) {
    return NTW(widget)->nt();
}

static inline const NoteTaker* NT(const Widget* widget) {
    return NTW(widget)->nt();
}

static inline int MusicFont(const Widget* widget) {
    return NTW(widget)->musicFont->handle;
}

static inline int TextFont(const Widget* widget) {
    return NTW(widget)->textFont->handle;
}


template <class T>
T* NTWidget(Widget* widget) {
    auto ntw = widget->getAncestorOfType<NoteTakerWidget>();
    return ntw->getFirstDescendantOfType<T>();
}

template <class T>
const T* NTWidget(const Widget* widget) {
    auto ntw = const_cast<Widget*>(widget)->getAncestorOfType<NoteTakerWidget>();
    return const_cast<const T*>(ntw->getFirstDescendantOfType<T>());
}
