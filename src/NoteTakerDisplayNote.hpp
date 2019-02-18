#pragma once

#include "SchmickleWorks.hpp"

constexpr unsigned CHANNEL_COUNT = 16;      // MIDI channels
constexpr unsigned ALL_CHANNELS = 0xFFFF;   // one bit set per MIDI channels
constexpr unsigned ALL_CV_CHANNELS = 0x0F;  // one bit set per CV output

enum DisplayType : uint8_t {
// enums below match MIDI channel voice message high nybble order
    UNUSED,            // midi note off slot
    NOTE_ON,
    KEY_PRESSURE,
    CONTROL_CHANGE,
    PROGRAM_CHANGE,
    CHANNEL_PRESSURE,
    PITCH_WHEEL,
    MIDI_SYSTEM,       // data that doesn't fit in DisplayNote pointed to be index into midi file
// any order OK
    MIDI_HEADER,
    KEY_SIGNATURE,
    TIME_SIGNATURE,
    MIDI_TEMPO,
    REST_TYPE,
    TRACK_END,
    NUM_TYPES
};

// midi is awkward to parse at run time to draw notes since the duration is some
// where in the future stream. It is not trival (or maybe not possible) to walk
// backwards to find the note on given the position of the note off
        
// better to have a structured array of notes that more closely resembles the
// data we want to draw
struct DisplayNote {
    int startTime;      // MIDI time (e.g. stdTimePerQuarterNote: 1/4 note == 96)
    int duration;       // MIDI time
    int data[4];        // type-specific values
    uint8_t channel;    // set to 0xFF if type doesn't have channel
    DisplayType type;

    void setChannel(uint8_t c) {
        channel = c;
        assertValid(type == NOTE_ON ? NOTE_ON : REST_TYPE);
    }

    int pitch() const {
        assertValid(NOTE_ON);
        return data[0];             // using MIDI note assignment
    }

    void setPitch(int pitch) {
        data[0] = pitch;
        assertValid(NOTE_ON);
    }

    int format() const {
        assertValid(MIDI_HEADER);
        return data[0];
    }

    int key() const {
        assertValid(KEY_SIGNATURE);
        return data[0];
    }

    int numerator() const {
        assertValid(TIME_SIGNATURE);
        return data[0];
    }

    int note() const {
        assertValid(NOTE_ON);
        return data[1];
    }

    void setNote(int note) {
        data[1] = note;
        assertValid(NOTE_ON);
    }

    int rest() const {
        assertValid(REST_TYPE);
        return data[1];
    }

    void setRest(int note) {
        data[1] = note;
        assertValid(REST_TYPE);
    }

    int denominator() const {
        assertValid(TIME_SIGNATURE);
        return data[1];
    }

    int minor() const {
        assertValid(KEY_SIGNATURE);
        return data[1];
    }

    int onVelocity() const {
        assertValid(NOTE_ON);
        return data[2];
    }

    void setOnVelocity(int velocity) {
        data[2] = velocity;
        assertValid(NOTE_ON);
    }

    int clocksPerClick() const {
        assertValid(TIME_SIGNATURE);
        return data[2];
    }

    int notated32NotesPerQuarterNote() const {
        assertValid(TIME_SIGNATURE);
        return data[3];
    }

    int offVelocity() const {
        assertValid(NOTE_ON);
        return data[3];
    }

    void setOffVelocity(int velocity) {
        data[3] = velocity;
        assertValid(NOTE_ON);
    }

    void assertValid(DisplayType t) const {
        if (type != t) {
            debug("type %d != t %d\n", type, t);
            assert(type == t);
        }
        return assert(isValid());
    }

    bool isValid() const;

    bool isActive(int midiTime) const {
        return startTime <= midiTime && startTime + duration > midiTime;
    }

    bool isBestSelectStart(const DisplayNote** bestPtr, int midiTime) const {
        const DisplayNote* best = *bestPtr;
        if (!best) {
            *bestPtr = this;
            return true;
        }
        if (best->isActive(midiTime)) {
            return false;
        }
        if (startTime > best->startTime && startTime <= midiTime) {
            *bestPtr = this;
            return true;
        }
        return false;
    }

    bool isSelectable(unsigned selectChannels) const {
        return (NOTE_ON == type || REST_TYPE == type) && (selectChannels & (1 << channel));
    }

    std::string debugString() const;
};
