#pragma once

#include "DisplayNote.hpp"

struct DisplayState;
struct NoteTakerDisplay;

// break out notes and range so that preview can draw notes without instantiated module
struct Notes {
    vector<DisplayNote> notes;
    unsigned selectStart = 0;           // index into notes of first selected (any channel)
    unsigned selectEnd = 1;             // one past last selected
    int ppq = stdTimePerQuarterNote;    // default to 96 pulses/ticks per quarter note
    
    Notes() {
        notes.emplace_back(MIDI_HEADER);
        notes.emplace_back(TRACK_END);
    }

    static bool Deserialize(const vector<uint8_t>& , vector<DisplayNote>* , int* ppq);
    void eraseNotes(unsigned start, unsigned end, unsigned selectChannels);
    static std::string FlatName(unsigned midiPitch);
    void fromJson(json_t* root);
    static bool FromJsonCompressed(json_t* , vector<DisplayNote>* , int* ppq);
    static void FromJsonUncompressed(json_t* , vector<DisplayNote>* );
    vector<unsigned> getVoices(unsigned selectChannels, bool atStart) const;
    // static void HighestOnly(vector<DisplayNote>& );
    unsigned horizontalCount(unsigned selectChannels) const;
    bool isEmpty(unsigned selectChannels) const;
    static std::string KeyName(int key, int minor);

    int nextStart(unsigned selectChannels) const {
        int result = notes.back().startTime;
        for (unsigned index = selectEnd; index < notes.size(); ++index) {
            auto& note = notes[index];
            if (note.isSelectable(selectChannels)) {
                result = note.startTime;
                break;
            }
        }
        return result;
    }

    int noteCount(unsigned selectChannels) const;
    int noteTimes(unsigned selectChannels) const;
    static bool PitchCollision(const vector<DisplayNote>& notes, const DisplayNote& , int pitch,
            vector<const DisplayNote*>* overlaps);
    bool pitchCollision(const DisplayNote& , int newPitch) const;
    static std::string SharpName(unsigned midiPitch);

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

    static void Serialize(const vector<DisplayNote>& , vector<uint8_t>& );
    // truncates / expands duration preventing note from colliding with same pitch later on 
    void setDuration(DisplayNote* );
    static std::string TSDenom(const DisplayNote* , int ppq);
    static std::string TSNumer(const DisplayNote* , int ppq);
    static std::string TSUnit(const DisplayNote* , int count, int ppq);
    bool transposeSpan(vector<DisplayNote>& span) const;
    json_t* toJson() const;
    static void ToJsonCompressed(const vector<DisplayNote>& , json_t* , std::string );
    static void ToJsonUncompressed(const vector<DisplayNote>& , json_t* , std::string );
    bool validate(bool assertOnFailure = true) const;
    static bool Validate(const vector<DisplayNote>& notes, bool assertOnFailure = true);
    int xPosAtEndEnd(const DisplayState& ) const;
    int xPosAtEndStart() const;
    int xPosAtStartEnd() const;
    int xPosAtStartStart() const;
};
