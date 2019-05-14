#pragma once

#include "rack0.hpp"
#include <algorithm>
#include <array>
#include <float.h>
#include <functional>
#include <limits.h>
#include <list>
#include <math.h>
#include <map>
#include <stdint.h>
#include <unordered_map>
#include "NoteTakerConstants.hpp"
#include "NoteTakerDisplayNote.hpp"

#ifndef M_PI
#define M_PI 3.141592653589793238463 
#endif

using namespace rack;
using std::array;
using std::vector;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelNoteTaker;

#define RUN_UNIT_TEST 1 // to do : set to zero for shipping code

#if RUN_UNIT_TEST
    enum class TestType {
        digit,
        random,
        expected,
    };

    void UnitTest(struct NoteTakerWidget* , TestType );
#endif

struct NoteTaker;
struct NoteTakerButton;

struct NoteTakerWidget  : ModuleWidget {
    std::shared_ptr<Font> musicFont = nullptr;
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
    bool extractClipboard(vector<DisplayNote>* span) const;
    void fromJson(json_t* rootJ);
    unsigned horizontalCount() const;
    bool isEmpty() const;
    bool isSelectable(const DisplayNote& note) const;
    void loadScore();
    std::string midiDir() const;
    bool menuButtonOn() const;
    int nextStartTime(unsigned start) const;
    int noteCount() const;

    NoteTaker* nt() {
        return dynamic_cast<NoteTaker* >(module);
    }

    const NoteTaker* nt() const {
        return dynamic_cast<const NoteTaker* >(module);
    }

    void readStorage();
    bool resetControls();
    bool runningWithButtonsOff() const;
    void saveScore();
    void setHorizontalWheelRange();
    void setSelect(unsigned start, unsigned end);
    void setVerticalWheelRange();
    void setWheelRange();
    void shiftNotes(unsigned start, int diff);
    json_t* toJson();
    void turnOffLedButtons(const NoteTakerButton* exceptFor = nullptr);
    void validate() const;

    template <class T>
    T* widget() {
        return this->getFirstDescendantOfType<T>();
    }

    template <class T>
    const T* widget() const {
        return this->getFirstDescendantOfType<T>();
    }

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
    void writeStorage(unsigned index) const;
};

static NoteTakerWidget* NTW(Widget* widget) {
    return widget->getAncestorOfType<NoteTakerWidget>();
}

static const NoteTakerWidget* NTW(const Widget* widget) {
    // to do : request VCV for const version ?
    return const_cast<Widget*>(widget)->getAncestorOfType<NoteTakerWidget>();
}

static NoteTaker* NT(Widget* widget) {
    return NTW(widget)->nt();
}

static const NoteTaker* NT(const Widget* widget) {
    return NTW(widget)->nt();
}

static int MusicFont(Widget* widget) {
    return NTW(widget)->musicFont->handle;
}

template <class T>
T* NTWidget(Widget* widget) {
    return NTW(widget)->widget();
}
