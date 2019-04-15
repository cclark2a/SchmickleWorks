#pragma once

#include "rack.hpp"
#include "dsp/digital.hpp"
#include <algorithm>
#include <array>
#include <float.h>
#include <functional>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "NoteTakerConstants.hpp"

using namespace rack;
using std::array;
using std::vector;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *plugin;

// Forward-declare each Model, defined in each module source file
extern Model *modelNoteTaker;
