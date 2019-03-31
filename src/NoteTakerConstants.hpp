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

constexpr uint8_t midiTimeSignatureHi = 0xFF;
constexpr uint8_t midiTimeSignatureLo = 0x58;

constexpr uint8_t midiKeySignatureHi = 0xFF;
constexpr uint8_t midiKeySignatureLo = 0x59;

// control change codes
constexpr uint8_t midiReleaseMax = 0x57;
constexpr uint8_t midiReleaseMin = 0x58;
constexpr uint8_t midiSustainMin = 0x59;
constexpr uint8_t midiSustainMax = 0x5A;

constexpr uint8_t stdKeyPressure = 0x64;
constexpr int stdTimePerQuarterNote = 0x60;
constexpr int stdMSecsPerQuarterNote = 500000;  // 120 beats / minute
constexpr uint8_t stdTimeSignatureClocksPerQuarterNote = 0x24;
constexpr uint8_t stdTimeSignature32ndNotesInQuarter = 0x08;

constexpr unsigned CHANNEL_COUNT = 16;      // MIDI channels
constexpr unsigned ALL_CHANNELS = 0xFFFF;   // one bit set per MIDI channels
constexpr unsigned ALL_CV_CHANNELS = 0x0F;  // one bit set per CV output

constexpr float DEFAULT_GATE_HIGH_VOLTAGE = 5;
constexpr unsigned CV_OUTPUTS = 4;

struct NoteDurations {
    static int Closest(int midi, int ppq);
    static unsigned Count();
    static unsigned FromMidi(int midi, int ppq);
    static unsigned FromStd(int duration);
    static int ToMidi(unsigned index, int ppq);
    static int ToStd(unsigned index);
};
