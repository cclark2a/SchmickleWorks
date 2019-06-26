#pragma once

#include "DisplayNote.hpp"

struct NoteTakerDisplay;

// break out notes and range so that preview can draw notes without instantiated module
struct Notes {
    vector<DisplayNote> notes;
    unsigned selectStart = 0;           // index into notes of first selected (any channel)
    unsigned selectEnd = 1;             // one past last selected
    int ppq = stdTimePerQuarterNote;    // default to 96 pulses/ticks per quarter note
    bool debugVerbose;
    
    Notes(bool dbug) :
        debugVerbose(dbug) {
    }

    Notes( const Notes& ) = delete; // non construction-copyable
    Notes& operator=( const Notes& ) = delete; // non copyable

    void deserialize(const vector<uint8_t>& );
    void eraseNotes(unsigned start, unsigned end, unsigned selectChannels);
    void fromJson(json_t* );
    void fromJsonCompressed(json_t* );
    vector<unsigned> getVoices(unsigned selectChannels, bool atStart) const;
    // static void HighestOnly(vector<DisplayNote>& );
    unsigned horizontalCount(unsigned selectChannels) const;
    bool isEmpty(unsigned selectChannels) const;
    int noteCount(unsigned selectChannels) const;
    int noteTimes(unsigned selectChannels) const;
    static bool PitchCollision(const vector<DisplayNote>& notes, const DisplayNote& , int pitch,
            vector<const DisplayNote*>* overlaps);
    bool pitchCollision(const DisplayNote& , int newPitch) const;

    unsigned selectEndPos(unsigned select) const {
        const DisplayNote& first = notes[select];
        const DisplayNote* test;
        do {
            test = &notes[++select];
        } while (first.isNoteOrRest() && test->isNoteOrRest()
                && first.startTime == test->startTime);
        return select;
    }

    // to do : move notetaker sort here?

    void serialize(vector<uint8_t>& ) const;
    // truncates / expands duration preventing note from colliding with same pitch later on 
    void setDuration(DisplayNote* );
    bool transposeSpan(vector<DisplayNote>& span) const;
    void toJson(json_t* ) const;
    void validate() const;
    int xPosAtEndEnd(const NoteTakerDisplay* ) const;
    int xPosAtEndStart(const NoteTakerDisplay* ) const;
    int xPosAtStartEnd(const NoteTakerDisplay* ) const;
    int xPosAtStartStart(const NoteTakerDisplay* ) const;
};
