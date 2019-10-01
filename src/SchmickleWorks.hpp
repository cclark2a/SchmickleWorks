#pragma once

#include "rack.hpp"
#include "settings.hpp"
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

#include "Constants.hpp"

using namespace rack;

#ifndef M_PI
#define M_PI 3.141592653589793238463 
#endif

using std::array;
using std::vector;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin* pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model* modelNoteTaker;
extern Model* modelSuper8;

extern bool debugCapture;
extern bool debugVerbose;  // switch to permit user debugging in shipping code
extern bool groupByGMInstrument;

#define RUN_UNIT_TEST 1 // to do : set to zero for shipping code

#if RUN_UNIT_TEST
    enum class TestType {
        digit,
        random,
        expected,
        encode,
    };

    void UnitTest(struct NoteTakerWidget* , TestType );
#endif

#define DEBUG_TRIPLET_DRAW 0
#define DEBUG_TRIPLET_TEST (DEBUG_TRIPLET_DRAW && debugVerbose)
#define DEBUG_TRIPLET_DRAW_SHOW_DETAILS 0
#if DEBUG_TRIPLET_DRAW_SHOW_DETAILS
#define DEBUG_TRIPLET_DRAW_DETAILS(noteCache) \
                            + " cache: " + noteCache->debugString() \
                            + " note: " + noteCache->note->debugString()
#else
#define DEBUG_TRIPLET_DRAW_DETAILS(noteCache)
#endif
#define DEBUG_BAR 0
#define DEBUG_BAR_ADD_POS (DEBUG_BAR && 0)
#define DEBUG_BEAM 0
#define DEBUG_DISPLAY_RANGE 0
#define DEBUG_DURATIONS 0
#define DEBUG_EDIT 0
#define DEBUG_GATES 0
#define DEBUG_PARSE 1
#define DEBUG_POS 0
#define DEBUG_RUN_TIME 0
#define DEBUG_STAFF 0
#define DEBUG_SLUR 0
#define DEBUG_SLUR_TEST (DEBUG_SLUR && debugVerbose)
#define DEBUG_VOICE_COUNT 0

struct Super8Data {
    float exCv[4][16] = {};
    float exGate[4][16] = {};
    float exVelocity[8][16] = {};
    unsigned exChannels[8] = {};
};

template<class TWidget>
struct WidgetToolTip : ParamQuantity {
    TWidget* widget = nullptr;
    std::string defaultLabel;

    std::string getDisplayValueString() override;
};

// note: when implemented as inline c++ functions, stack and assert line # was out of line
// to do : remove assert from production, so that innoculous asserts don't cause a crash?
#define _schmickled() \
    (DEBUG("%s", system::getStackTrace().c_str()), assert(0))

#define _schmickle_false() \
    _schmickled(), false

#define _schmickle(cond, condStr) \
    if (!(cond)) { \
        DEBUG("%s", condStr); \
        _schmickled(); \
    }

#define SCHMICKLE(x) \
    _schmickle(x, #x)
