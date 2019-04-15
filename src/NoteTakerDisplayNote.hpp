#pragma once

#include "SchmickleWorks.hpp"

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

// startTime is not strictly required if we assume that all notes in a channel
// start at zero and don't have gaps between them, but maintaining when a note
// starts, given just the note, is onerous. For now, have validation ensure that
// there are no gaps and call it often enough to keep the note array sane.
struct DisplayNote {
    int startTime;      // MIDI time (e.g. stdTimePerQuarterNote: 1/4 note == 96)
    int duration;       // MIDI time
    int data[4];        // type-specific values
    uint8_t channel;    // set to 0xFF if type doesn't have channel
    DisplayType type;
    bool selected;      // set if channel intersects selectChannels prior to channel edit

    bool operator<(const DisplayNote& rhs) const {
        return startTime < rhs.startTime;
    }

    void setChannel(uint8_t c) {
        channel = c;
        assertValid(NOTE_ON == type ? NOTE_ON : REST_TYPE);
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

    void setKey(int k) {
        data[0] = k;
        assertValid(KEY_SIGNATURE);
    }

    int numerator() const {
        assertValid(TIME_SIGNATURE);
        return data[0];
    }

    void setNumerator(int n) {
        data[0] = n;
        assertValid(TIME_SIGNATURE);
    }

    int tphs() const {
        assertValid(MIDI_TEMPO);
        return data[0];
    }

    // if set, gate is kept high through next note (no midi note off until after next note on?)
    bool slur() const {
        assertValid(NOTE_ON);
        return data[1];
    }

    void setSlur(bool slur) {
        data[1] = slur;
        assertValid(NOTE_ON);
    }

    int tracks() const {
        assertValid(MIDI_HEADER);
        return data[1];
    }

    int denominator() const {
        assertValid(TIME_SIGNATURE);
        return data[1];
    }

    void setDenominator(int d) {
        data[1] = d;
        assertValid(TIME_SIGNATURE);
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

    int ppq() const {
        assertValid(MIDI_HEADER);
        assert(!(data[1] & 0x8000));
        return data[2];
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

    int endSlurTime() const {
        return endTime() + slur();
    }

    int endTime() const {
        return startTime + duration;
    }

    bool isValid() const;

    bool isActive(int midiTime) const {
        return startTime <= midiTime && endTime() > midiTime;
    }

    bool isSelectable(unsigned selectChannels) const {
        return TIME_SIGNATURE == type || KEY_SIGNATURE == type
                || (isNoteOrRest() && (selectChannels & (1 << channel)));
    }

    bool isNoteOrRest() const {
        return NOTE_ON == type || REST_TYPE == type;
    }

    int stdDuration(int ppq) const {
        return NoteDurations::InStd(this->duration, ppq);
    }

    int stdStart(int ppq) const {
        return NoteDurations::InStd(startTime, ppq);
    }

    json_t* toJson() const;
    void fromJson(json_t* rootJ);

    std::string debugString() const;
};

enum class PositionType : uint8_t {
    none,
    left,
    mid,
    right,
};

struct NoteCache {
    int xPosition;
    float yPosition;
    int vDuration;  // visible duration, for symbol selection and triplet beams
    PositionType beamPosition;
    uint8_t beamCount;
    PositionType tupletPosition;
    PositionType slurPosition;
    uint8_t symbol;
    bool stemUp;

    void resetTupleBeam() {
        beamPosition = PositionType::none;
        beamCount = 0;
        tupletPosition = PositionType::none;
        slurPosition = PositionType::none;
    }
};
