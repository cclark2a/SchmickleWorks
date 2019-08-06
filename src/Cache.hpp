#pragma once

#include "DisplayNote.hpp"

struct BarPosition;
struct BeamPositions;
struct DisplayNote;
struct DisplayState;
struct Notes;
struct NoteTakerDisplay;

struct DisplayCache {
    vector<NoteCache> notes;  // where note is drawn (computed cache, not saved)
    bool leadingTempo = false;

    unsigned next(unsigned index) const {
        const DisplayNote* base = notes[index].note;
        while (base == notes[++index].note) {
            ;
        }
        return index;
    }

    unsigned previous(unsigned index) const {
        const DisplayNote* base = notes[--index].note;
        while (index && base == notes[index - 1].note) {
            --index;
        }
        return index;
    }

    int startXPos(unsigned start) const {
        return notes[start].xPosition;
    }
};

// drawing notes must line up with extra bar space added by updateXPosition
struct BarPosition {
    struct MinMax {
        float xMin = FLT_MAX;
        float xMax = -FLT_MAX;
        bool useMax = false;   // set true if signature introduces bar
    };
    std::unordered_map<int, MinMax> pos;
    // set by set signature
    int priorBars;      // keeps index unique for pos map when ts changes
    int duration;       // midi time of one bar
    int tsStart;        // midi time when current time signature starts
    int midiEnd;        // used to restart accidentals at start of bar
    // set by notes tied
    int leader;         // duration of note part before first bar
    const bool debugVerbose;

    BarPosition(bool debug);

    void addPos(const NoteCache& , float cacheWidth);

    void advance(const NoteCache& noteCache) {
        SCHMICKLE(INT_MAX != duration);
        while (noteCache.vStartTime >= midiEnd) {
            midiEnd += duration;
        }
    }

    int noteRegular(const DisplayNote& , int ppq, bool twoThirds);
    int notesTied(const DisplayNote& , int ppq, bool* twoThirds);
    int resetSignatureStart(const DisplayNote& , float barWidth);

    void setMidiEnd(const NoteCache& noteCache) {
        if (INT_MAX != duration) {
            midiEnd = noteCache.vStartTime + duration;
        }
    }

    void setPriorBars(const NoteCache& noteCache) {
        if (!noteCache.vStartTime) {
            return;
        }
        if (INT_MAX == duration) {
            priorBars = 1;
        } else {
            priorBars += (noteCache.vStartTime - tsStart + duration - 1) / duration;  // rounds up
        }
        pos[priorBars].useMax = true;
        pos[priorBars].xMax = noteCache.xPosition;
        if (false && debugVerbose) DEBUG("setPriorBars %d useMax %d xMax %d",
                priorBars, true, noteCache.vStartTime);
    }

    void setSignature(const DisplayNote& note, int ppq) {
        duration = ppq * 4 * note.numerator() / (1 << note.denominator());
        tsStart = note.startTime;
        midiEnd = tsStart + duration;
    }
};

struct PosAdjust {
    float x;
    int time;
};

struct CacheBuilder {
    const DisplayState& state;
    DisplayCache* cache = nullptr;
    int ppq = 0;
    const bool debugVerbose;
    
    CacheBuilder(const DisplayState& , DisplayCache* );

    void cacheBeams();
    void cacheSlurs();
    void cacheStaff();
    void cacheTuplets(const Notes& );
    void clearTuplet(unsigned index, unsigned limit);
    void closeBeam(unsigned first, unsigned limit);
    void closeSlur(unsigned first, unsigned limit);
    void setDurations(const Notes& );
    void trackPos(std::list<PosAdjust>& posAdjust, float xOff, int endTime);
    void updateXPosition(const Notes& );
};
