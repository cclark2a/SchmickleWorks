#pragma once

#include "SchmickleWorks.hpp"

constexpr uint8_t midiCVMask = 0xF0;
constexpr uint8_t midiNoteOff = 0x80;
constexpr uint8_t midiNoteOn = 0x90;
constexpr uint8_t midiKeyPressure = 0xA0;
constexpr uint8_t midiControlChange = 0xB0;
constexpr uint8_t midiProgramChange = 0xC0;
constexpr uint8_t midiChannelPressure = 0xD0;
constexpr uint8_t midiPitchWheel = 0xE0;
constexpr uint8_t midiSystem = 0xF0;

constexpr uint8_t midiMetaEvent = 0xFF;
constexpr uint8_t midiEndOfTrack = 0x2F;
constexpr uint8_t midiSetTempo = 0x51;
constexpr uint8_t midiTimeSignature = 0x58;
constexpr uint8_t midiKeySignature = 0x59;

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
constexpr unsigned MAX_VOICES = 16;

constexpr float DEFAULT_GATE_HIGH_VOLTAGE = 5;
constexpr unsigned CV_OUTPUTS = 4;

struct NoteDurations {
    static int Beams(unsigned index);
    static int LtOrEq(int midi, int ppq);
    static unsigned Count();
    static unsigned FromMidi(int midi, int ppq);
    static unsigned FromStd(int duration);
        
    static int InMidi(int std, int ppq) {
        return std * ppq / stdTimePerQuarterNote;
    }

    static int InStd(int midi, int ppq) {
        return midi * stdTimePerQuarterNote / ppq;
    }

    static int ToMidi(unsigned index, int ppq);
    static int ToStd(unsigned index);
    static bool TripletPart(int midi, int ppq);
};

 static inline unsigned gcd(unsigned one, unsigned two) {
    return !two ? one : gcd(two, one % two);
}
