#pragma once

#include "rack.hpp"
#include "dsp/digital.hpp"
#include <array>
#include <float.h>
#include <limits.h>

using namespace rack;
using std::array;
using std::vector;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *plugin;

// Forward-declare each Model, defined in each module source file
extern Model *modelNoteTaker;
