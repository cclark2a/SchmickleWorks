#pragma once

#include "DisplayNote.hpp"

struct DisplayCache;
struct DisplayState;
struct NoteTakerDisplay;

struct TripletCandidate {
    unsigned lastIndex = INT_MAX;
    unsigned startIndex = INT_MAX;
    NoteCache* lastCache = nullptr;
    bool atLeastOneNote = false;
};

// break out notes and range so that preview can draw notes without instantiated module
struct Notes {
    vector<DisplayNote> notes;
    unsigned selectStart = 0;           // index into notes of first selected (any channel)
    unsigned selectEnd = 1;             // one past last selected
    int ppq = stdTimePerQuarterNote;    // default to 96 pulses/ticks per quarter note
        
    enum class HowMany {
        clear,         // at least one slur/trip can be cleared
        set,           // at least one slurs/trip can be set
    };

    Notes() {
        notes.emplace_back(MIDI_HEADER);
        notes.emplace_back(TRACK_END);
    }

    void clear() {
        notes.clear();
        selectStart = 0;
        selectEnd = 1;
        ppq = stdTimePerQuarterNote;
    }

    unsigned atMidiTime(int midiTime) const {
        for (unsigned index = 0; index < notes.size(); ++index) {
            const DisplayNote& note = notes[index];
            if (midiTime < note.startTime || TRACK_END == note.type
                    || (note.isNoteOrRest() && midiTime == note.startTime)) {
                return index;
            }
        }
        return _schmickle_false();  // should have hit track end
    }

    void findTriplets(vector<PositionType>* tuplets, DisplayCache* displayCache);
    static void DebugDump(const vector<DisplayNote>& , unsigned start = 0,
            unsigned end = INT_MAX, const vector<NoteCache>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);
    static bool Deserialize(const vector<uint8_t>& , vector<DisplayNote>* , int* ppq);
    void eraseNotes(unsigned start, unsigned end, unsigned selectChannels);
    static std::string FlatName(unsigned midiPitch);
    void fromJson(json_t* root);
    static bool FromJsonCompressed(json_t* , vector<DisplayNote>* , int* ppq);
    static void FromJsonUncompressed(json_t* , vector<DisplayNote>* );
    static std::string FullName(int duration, int ppq);
    vector<unsigned> getVoices(unsigned selectChannels, bool atStart) const;
    // static void HighestOnly(vector<DisplayNote>& );
    unsigned horizontalCount(unsigned selectChannels) const;

    bool insertContains(unsigned loc, DisplayType type) const {
        unsigned before = loc;
        const DisplayNote* note;
        while (before) {
            note = &notes[--before];
            if (type == note->type) {
                return true;
            }
            if (!note->isSignature()) {
                break;
            }
        }
        while (loc < notes.size()) {
            note = &notes[loc];
            if (type == note->type) {
                return true;
            }
            if (!note->isSignature()) {
                return false;
            }
            ++loc;
        }
        return false;
    }

    bool isEmpty(unsigned selectChannels) const;
    static std::string KeyName(int key, int minor);

    static int LastEndTime(const vector<DisplayNote>& notes) {
        int result = 0;
        for (auto& note : notes) {
            result = std::max(result, note.endTime());
        }
        return result;
    }

    static void MapChannel(vector<DisplayNote>& notes, unsigned channel) {
         for (auto& note : notes) {
             if (note.isNoteOrRest()) {
                note.channel = channel;
             }
        }    
    }

    static std::string Name(const DisplayNote* , int ppq);

    unsigned nextAfter(unsigned first, unsigned len) const {
        SCHMICKLE(len);
        unsigned start = first + len;
        const auto& priorNote = notes[start - 1];
        if (!priorNote.duration) {
            return start;
        }
        int priorTime = priorNote.startTime;
        int startTime = notes[start].startTime;
        if (priorTime < startTime) {
            return start;
        }
        for (unsigned index = start; index < notes.size(); ++index) {
            const DisplayNote& note = notes[index];
            if (note.isSignature() || note.startTime > startTime) {
                return index;
            }
        }
        _schmickled();
        return INT_MAX;
    }

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
    void setSlurs(unsigned selectChannels, bool condition);
    void setTriplets(unsigned selectChannels, bool condition);
    bool slursOrTies(unsigned selectChannels, HowMany , bool* atLeastOneSlur) const;
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

    void shift(unsigned start, int diff, unsigned selectChannels = ALL_CHANNELS) {
        if (Notes::ShiftNotes(notes, start, diff, selectChannels)) {
            this->sort();
        }
    }

    // shift track end only if another shifted note bumps it out
    // If all notes are selected, shift signatures. Otherwise, leave them be.
    static bool ShiftNotes(vector<DisplayNote>& notes, unsigned start, int diff,
            unsigned selectChannels = ALL_CHANNELS) {
        bool sortResult = false;
        int trackEndTime = 0;
        bool hasTrackEnd = TRACK_END == notes.back().type;
        if (hasTrackEnd) {
            for (unsigned index = 0; index < start; ++index) {
                trackEndTime = std::max(trackEndTime, notes[index].endTime());
            }
        }
        for (unsigned index = start; index < notes.size(); ++index) {
            DisplayNote& note = notes[index];
            if ((note.isSignature() && ALL_CHANNELS == selectChannels)
                    || note.isSelectable(selectChannels)) {
                note.startTime += diff;
            } else {
                sortResult = true;
            }
            if (hasTrackEnd && TRACK_END != note.type) {
                trackEndTime = std::max(trackEndTime, note.endTime());
            }
        }
        if (hasTrackEnd) {
            notes.back().startTime = trackEndTime;
        }
        return sortResult;
   }

    // don't use std::sort function; use insertion sort to minimize reordering
     void sort() {
        if (debugVerbose) DEBUG("sort notes");
        DisplayType start = SELECT_START;
        DisplayType end = SELECT_END;
        SCHMICKLE(selectStart < selectEnd);
        SCHMICKLE(selectEnd < notes.size());
        std::swap(start, notes[selectStart].type);
        std::swap(end, notes[selectEnd].type);
        Notes::SortNotes(notes);
        // sort may move selection end (and maybe start?)
        // set up select start / end after sorting to where they landed
        for (auto& note : notes) {
            if (SELECT_START == note.type) {
                selectStart = &note - &notes.front();
                std::swap(start, note.type);
            }
            if (SELECT_END == note.type) {
                selectEnd = &note - &notes.front();
                std::swap(end, note.type);
            }
        }
        SCHMICKLE(SELECT_START == start);
        SCHMICKLE(SELECT_END == end);
    }

    static void SortNotes(vector<DisplayNote>& notes) {
        for (auto it = notes.begin(), end = notes.end(); it != end; ++it) {
            auto const insertion_point = std::upper_bound(notes.begin(), it, *it);
            std::rotate(insertion_point, it, it + 1);
        }
    }

    static std::string TSDenom(const DisplayNote* , int ppq);
    static std::string TSNumer(const DisplayNote* , int ppq);
    static std::string TSUnit(const DisplayNote* , int count, int ppq);
    bool transposeSpan(vector<DisplayNote>& span) const;
    bool triplets(unsigned selectChannels, HowMany ) const;
    json_t* toJson() const;
    static void ToJsonCompressed(const vector<DisplayNote>& , json_t* , std::string );
    static void ToJsonUncompressed(const vector<DisplayNote>& , json_t* , std::string );
    bool validate(bool assertOnFailure = true) const;
    static bool Validate(const vector<DisplayNote>& notes, bool assertOnFailure = true,
            bool requireHeaderTrailer = true);
    int xPosAtEndEnd(const DisplayState& ) const;
    int xPosAtEndStart() const;
    int xPosAtStartEnd() const;
    int xPosAtStartStart() const;
};
