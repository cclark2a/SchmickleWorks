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

#if RUN_UNIT_TEST
    enum class TestType {
        digit,
        random,
        expected,
    };

    void UnitTest(struct NoteTakerWidget* , TestType );
#endif

