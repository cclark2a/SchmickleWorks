#pragma once

#include <stdint.h>

constexpr uint8_t midiCVMask = 0xF0;
constexpr uint8_t midiNoteOff = 0x80;
constexpr uint8_t midiNoteOn = 0x90;
constexpr uint8_t midiKeyPressure = 0xA0;
constexpr uint8_t midiControlChange = 0xB0;
constexpr uint8_t midiProgramChange = 0xC0;
constexpr uint8_t midiChannelPressure = 0xD0;
constexpr uint8_t midiPitchWheel = 0xE0;
constexpr uint8_t midiSystem = 0xF0;

// control change codes
constexpr uint8_t midiReleaseMax = 0x57;
constexpr uint8_t midiReleaseMin = 0x58;
constexpr uint8_t midiSustainMin = 0x59;
constexpr uint8_t midiSustainMax = 0x5A;

constexpr uint8_t stdKeyPressure = 0x64;
constexpr int stdTimePerQuarterNote = 0x60;

constexpr unsigned CHANNEL_COUNT = 16;      // MIDI channels
constexpr unsigned ALL_CHANNELS = 0xFFFF;   // one bit set per MIDI channels
constexpr unsigned ALL_CV_CHANNELS = 0x0F;  // one bit set per CV output

constexpr float DEFAULT_GATE_HIGH_VOLTAGE = 5;
constexpr unsigned CV_OUTPUTS = 4;

static constexpr std::array<int, 20> noteDurations = {
       3, //        128th note
       6, //         64th
       9, // dotted  64th
      12, //         32nd
      18, // dotted  32nd
      24, //         16th
      36, // dotted  16th
      48, //          8th
      72, // dotted   8th
      96, //        quarter note
     144, // dotted quarter     
     192, //        half
     288, // dotted half
     384, //        whole note
     576, // dotted whole
     768, //        double whole
    1052, //        triple whole (dotted double whole)
    1536, //     quadruple whole
    2304, //      sextuple whole (dotted quadruple whole)
    3072, //       octuple whole
};
