#pragma once

#include "SchmickleWorks.hpp"

enum DisplayType : uint8_t {
// enums below match MIDI channel voice message high nybble order
    NOTE_OFF,          // not drawn, but kept for saving as midi
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
    int channel;        // set to -1 if type doesn't have channel
    int startTime;      // MIDI time (e.g. stdTimePerQuarterNote: 1/4 note == 96)
    int duration;       // MIDI time
    int data[4];        // type-specific values
    DisplayType type;
    bool cvOn;          // true if note is providing cv out      
    bool gateOn;        // true if note is providing gate/trigger out

    int pitch() const {
        assertValid(NOTE_ON);
        return data[0];             // using MIDI note assignment
    }

    void setPitch(int pitch) {
        data[0] = pitch;
        assertValid(type == NOTE_ON ? NOTE_ON : NOTE_OFF);
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
        assertValid(type == NOTE_ON ? NOTE_ON : NOTE_OFF);
    }

    void assertValid(DisplayType t) const {
        if (type != t) {
            debug("type %d != t %d\n", type, t);
            assert(type == t);
        }
        return assert(isValid());
    }

    bool isValid() const {
        switch (type) {
            case NOTE_OFF:
            case NOTE_ON:
                if (0 > data[0] || data[0] > 127) {
                    debug("invalid note pitch %d\n", data[0]);
                    return false;
                }
                if (0 > channel || channel > 15) {
                    debug("invalid note channel %d\n", channel);
                    return false;
                }
                if (NOTE_ON == type && (0 > data[2] || data[2] > 127)) {
                    debug("invalid on note pressure %d\n", data[2]);
                    return false;
                }
                if (0 > data[3] || data[3] > 127) {
                    debug("invalid off note pressure %d\n", data[3]);
                    return false;
                }
            break;
            case REST_TYPE:
                if (0 > data[0] || data[0] > 127) {  // todo: should be more restrictive
                    debug("invalid rest pitch %d\n", data[0]);
                    return false;
                }
                if (0 > data[1] || data[1] > 15) {
                    debug("invalid rest channel %d\n", data[1]);
                    return false;
                }
            break;
            case MIDI_HEADER:
                if (0 > data[0] || data[0] > 2) {
                    debug("invalid midi format %d\n", data[0]);
                    return false;
                }
                if (1 != data[1] && (!data[0] || 0 > data[1])) {
                    debug("invalid midi tracks %d (format=%d)\n", data[1], data[0]);
                    return false;
                }
                if (1 > data[2]) {
                    debug("invalid beats per quarter note %d\n", data[2]);
                    return false;
                }
            break;
            case KEY_SIGNATURE:
                if (-7 <= data[0] || data[0] <= 7) {
                    debug("invalid key %d\n", data[0]);
                    return false;
                }
                if (0 != data[1] && 1 != data[1]) {
                    debug("invalid minor %d\n", data[1]);
                    return false;
                }
            break;
            case TIME_SIGNATURE: {
                // although midi doesn't prohibit weird time signatures, look for
                // common ones to help validate that the file is parsed correctly
                // allowed: 2/2 n/4 (n = 2 3 4 5 6 7 9 11) m/8 (m = 3 5 6 7 9 11 12)
                int num = data[0];
                int denom = data[1];
                if ((denom == 1 && num != 2)  // allow 2/2
                        || (denom == 2 && (num < 2 || num > 11 || (num > 7 && !(num & 1))))
                        || (denom == 3 && (num < 3 || num > 12 || num == 4 || num == 8))) {
                    debug("invalid time signature %d/%d\n", data[0], data[1]);
                    return false;
                }
                if (data[2] != 24) {
                    debug("invalid clocks/click %d\n", data[2]);
                }
                if (data[3] != 8) {
                    debug("invalid 32nds/quarter ntoe %d\n", data[3]);
                }
                } break;
            default:
                debug("to do: validate %d\n", type);
                assert(0);
                return false;
        }
        return true;
    }

};
