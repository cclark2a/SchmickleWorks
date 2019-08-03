#pragma once

#include "SchmickleWorks.hpp"

struct NoteCache;

enum DisplayType : uint8_t {
// enums below match MIDI channel voice message high nybble order
    UNUSED,
    NOTE_OFF = UNUSED,  // 0
    NOTE_ON,            // 1
    KEY_PRESSURE,       // 2
    CONTROL_CHANGE,     // 3
    PROGRAM_CHANGE,     // 4
    CHANNEL_PRESSURE,   // 5
    PITCH_WHEEL,        // 6
    MIDI_SYSTEM,        // 7 data that doesn't fit in DisplayNote found through index into midi file
// any order OK
    MIDI_HEADER,        // 8
    KEY_SIGNATURE,      // 9
    TIME_SIGNATURE,     // A
    MIDI_TEMPO,         // B
    REST_TYPE,          // C
    TRACK_END,          // D
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
    NoteCache* cache = nullptr;
    int startTime;          // MIDI time (e.g. stdTimePerQuarterNote: 1/4 note == 96)
    int duration;           // MIDI time
    int data[4] = { 0, 0, 0, 0}; // type-specific values / to do : make unsigned, can't serialize <0
    uint8_t channel;        // set to 0 if type doesn't have channel
    uint8_t voice = -1;     // poly voice assigned to note
    DisplayType type;

    DisplayNote(DisplayType t, int start = 0, int dur = 0, uint8_t chan = 0)
        : startTime(start)
        , duration(dur)
        , channel(chan)
        , type(t) {
        switch (type) {
            case KEY_SIGNATURE:
                this->setKey(0);
                break;
            case NOTE_ON:
                this->setOnVelocity(stdKeyPressure);
                this->setOffVelocity(stdKeyPressure);
                break;
            case TIME_SIGNATURE:
                data[2] = 24;   // clocks per click
                data[3] = 8;    // notated 32 notes per quarter note
                this->setNumerator(4);
                this->setDenominator(2);
                break;
            case MIDI_HEADER:
                data[1] = 1;
                this->setPpq(96);
                break;
            case MIDI_TEMPO:
                this->setTempo(500000);
                break;
            default:
                ;
        }
    }

    bool operator<(const DisplayNote& rhs) const {
        return startTime < rhs.startTime
            || (startTime == rhs.startTime && duration < rhs.duration);
    }

    void setChannel(uint8_t c) {
        SCHMICKLE(c <= 0x0F);
        channel = c;
        assertValid(NOTE_ON == type ? NOTE_ON : REST_TYPE);
    }

    int pitch() const {
        if (NOTE_OFF != type) {
            assertValid(NOTE_ON);
        }
        return data[0];             // using MIDI note assignment
    }

    void setPitch(int pitch) {
        SCHMICKLE(pitch >= 0 && pitch <= 127);
        data[0] = pitch;
        if (NOTE_OFF != type) {
            assertValid(NOTE_ON);
        }
    }

    int format() const {
        assertValid(MIDI_HEADER);
        return data[0];
    }

    int key() const {
        assertValid(KEY_SIGNATURE);
        return data[0] - 7;
    }

    void setKey(int k) {
        data[0] = k + 7;
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

    int tempo() const {
        assertValid(MIDI_TEMPO);
        return data[0];
    }

    void setTempo(int n) {
        data[0] = n;
        assertValid(MIDI_TEMPO);
    }

    // if set, gate is kept high through next note (no midi note off until after next note on?)
    bool slur() const {
        assertValid(NOTE_ON == type ? NOTE_ON : REST_TYPE);
        return data[1];
    }

    void setSlur(bool slur) {
        data[1] = slur;
        assertValid(NOTE_ON == type ? NOTE_ON : REST_TYPE);
    }

    int tracks() const {
        assertValid(MIDI_HEADER);
        return data[1];
    }

    void setTracks(int tracks) {
        data[1] = tracks;
        assertValid(MIDI_HEADER);
    }

    int denominator() const {
        assertValid(TIME_SIGNATURE);
        return data[1];
    }

    void setDenominator(int d) {
        data[1] = d;
        assertValid(TIME_SIGNATURE);
    }

    // to do : have a way to set this for user created key signatures?
    // if it is possible to distinguish user created from file read, then wheel tool tip
    // could show only major or minor is appropriate ...
    // another thought : double wheel range and let each key signature have major / minor choices
    int minor() const {
        assertValid(KEY_SIGNATURE);
        return data[1];
    }

    int onVelocity() const {
        if (NOTE_OFF != type) {
            assertValid(NOTE_ON);
        }
        return data[2];
    }

    void setOnVelocity(int velocity) {
        data[2] = velocity;
        if (NOTE_OFF != type) {
            assertValid(NOTE_ON);
        }
    }

    int ppq() const {
        assertValid(MIDI_HEADER);
        SCHMICKLE(!(data[1] & 0x8000));
        return data[2];
    }

    void setPpq(int ppq) {
        data[2] = ppq;
        assertValid(MIDI_HEADER);
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
            DEBUG("type %d != t %d\n", type, t);
            SCHMICKLE(type == t);
        }
        SCHMICKLE(isValid());
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
        return isSignature() || (isNoteOrRest() && (selectChannels & (1 << channel)));
    }

    bool isNoteOrRest() const {
        return NOTE_ON == type || REST_TYPE == type;
    }

    bool isSignature() const {
        return KEY_SIGNATURE == type || TIME_SIGNATURE == type || MIDI_TEMPO == type;
    }

    int stdDuration(int ppq) const {
        return NoteDurations::InStd(duration, ppq);
    }

    int stdStart(int ppq) const {
        return NoteDurations::InStd(startTime, ppq);
    }

    json_t* dataToJson() const;
    void dataFromJson(json_t* rootJ);

    std::string debugString() const;
};

enum class PositionType : uint8_t {
    none,
    left,
    mid,
    right,
};

// to do : set up end stage time for slot/stage end
struct NoteCache {
    const DisplayNote* note;  // needed because with tied notes, cache entries are > than notes
    int xPosition;
    float yPosition = 0;
    int vStartTime;  // visible start time, for multi part note alignment
    int vDuration = 0;  // visible duration, for symbol selection and triplet beams
    PositionType beamPosition = PositionType::none;
    uint8_t beamCount = 0;
    uint8_t channel;
    PositionType slurPosition = PositionType::none;
    PositionType tiePosition = PositionType::none;
    PositionType tupletPosition = PositionType::none;
    uint8_t tupletId = 0;  // distinguish between multiple simultaneous triplets
    uint8_t symbol;
    uint8_t pitchPosition = 0;
    bool accidentalSpace = false;
    bool endsOnBeat = false; // for beams
    bool stemUp = false;
    bool staff = false;  // set if note owns staff; for flags, tuplets, beaming, slurs, ties
    bool twoThirds = false; // set as hint that note may be triplet part

    NoteCache(const DisplayNote* n)
         : note(n)
         , vStartTime(n->startTime)
         , channel(n->channel) {
    }

    bool operator<(const NoteCache& rhs) const {
        return vStartTime < rhs.vStartTime
                || (vStartTime == rhs.vStartTime && (vDuration < rhs.vDuration
                || (vDuration == rhs.vDuration && yPosition < rhs.yPosition)));
    }

    void setDurationSymbol(int ppq)  {
        int dur = vDuration;
        if (PositionType::none != tupletPosition) {
            dur = dur * 3 / 2;  // to do : just support triplets for now
        }
        symbol = (uint8_t) NoteDurations::FromMidi(dur, ppq);
    }

    int vEndTime() const {
        return vStartTime + vDuration;
    }

};
