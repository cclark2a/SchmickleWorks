#pragma once

#include "DisplayNote.hpp"

struct DisplayCache;
struct DisplayState;
struct NoteTakerDisplay;

struct TripletCandidate {
    unsigned lastIndex = INT_MAX;
    unsigned startIndex = INT_MAX;
    bool atLeastOneNote = false;
};

// break out notes and range so that preview can draw notes without instantiated module
// to do : add dirty bit and cache serialized form for autosave (make notes private?)
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

    static void AddNoteOff(vector<DisplayNote>& notes);

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

    void findTriplets(DisplayCache* displayCache);
#if DEBUG_STD
    const DisplayNote& d(unsigned index) const;
#endif
    static void DebugDump(const vector<DisplayNote>& , unsigned start = 0,
            unsigned end = INT_MAX, const vector<NoteCache>* xPos = nullptr,
            unsigned selectStart = INT_MAX, unsigned selectEnd = INT_MAX);
    static bool Deserialize(const vector<uint8_t>& , vector<DisplayNote>* , int* ppq);
    void eraseNotes(unsigned start, unsigned end, unsigned selectChannels);
    // truncates / expands duration preventing note from colliding with same pitch later on 
    void fixCollisionDuration(DisplayNote* );
    static std::string FlatName(unsigned midiPitch);
    void fromJson(json_t* root);
    static bool FromJsonCompressed(json_t* , vector<DisplayNote>* , int* ppq, bool uncompressed);
    static bool FromJsonUncompressed(json_t* , vector<DisplayNote>* );
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

    void insertNote(unsigned insertLoc, int startTime, int duration, unsigned channel, int pitch);
    bool isEmpty(unsigned selectChannels) const;
    static std::string KeyName(int key, int minor);
    const NoteCache* lastCache(unsigned index) const;

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
            if (NOTE_OFF == note.type) {
                continue;
            }
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
    // set duration, pitch here because editing note on needs to edit note off as well
    void setDuration(DisplayNote* note, int duration);
    void setPitch(DisplayNote* note, int pitch);
    void setSlurs(unsigned selectChannels, bool condition);
    void setTriplets(unsigned selectChannels, bool condition);
    bool slursOrTies(unsigned selectChannels, HowMany , bool* atLeastOneSlur) const;
    static std::string SharpName(unsigned midiPitch);

    unsigned selectEndPos(unsigned select) const {
        const DisplayNote& first = notes[select];
        const DisplayNote* test;
        do {
            test = &notes[++select];
        } while (NOTE_OFF == test->type || (first.isNoteOrRest() && test->isNoteOrRest()
                && first.startTime == test->startTime));
        return select;
    }

    static void Serialize(const vector<DisplayNote>& , vector<uint8_t>& );

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
            if (note.isEnabled(selectChannels)) {
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

    // to do : move notetaker sort here?

    void sort() {
        if (debugVerbose) DEBUG("sort notes");
        const auto& first = notes[selectStart];
        const auto& last = notes[selectEnd - 1];
        std::sort(notes.begin(), notes.end());
        // sort may move selection end (and maybe start?)
        // set up select start / end after sorting to where they landed
        auto firstIter = std::lower_bound(notes.begin(), notes.end(), first);
        SCHMICKLE(firstIter != notes.end() && !(first < *firstIter));
        selectStart = firstIter - notes.begin();
        auto lastIter = std::lower_bound(notes.begin(), notes.end(), last);
        SCHMICKLE(lastIter != notes.end() && !(last < *lastIter));
        selectEnd = lastIter - notes.begin() + 1;
    }

    void sortOffNote(const DisplayNote* note, const DisplayNote& oldOff, const DisplayNote& newOff);

    static std::string TSDenom(const DisplayNote* , int ppq);
    static std::string TSNumer(const DisplayNote* , int ppq);
    static std::string TSUnit(const DisplayNote* , int count, int ppq);
    bool transposeSpan(vector<DisplayNote>& span);
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
