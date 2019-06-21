#pragma once

#include "rack.hpp"
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
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelNoteTaker;

#define RUN_UNIT_TEST 1 // to do : set to zero for shipping code
#define DEBUG_VERBOSE true

#if RUN_UNIT_TEST
    enum class TestType {
        digit,
        random,
        expected,
        encode,
    };

    void UnitTest(struct NoteTakerWidget* , TestType );
#endif

// to do : create a compile time option to omit debugging code?
#define SCHMICKLE(x) _schmickle(x, #x)

inline bool _schmickled() {
    std::string st = system::getStackTrace();
    DEBUG("%s", st.c_str());
    assert(0);  // to do : remove from production, so that innoculous asserts don't cause a crash?
    return false;
}

inline bool _schmickle(bool cond, const char* condStr) {
    if (cond) {
        return true;
    }
    DEBUG("%s", condStr);
    return _schmickled();
}
