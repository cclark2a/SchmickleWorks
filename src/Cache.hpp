#pragma once

#include "DisplayNote.hpp"

struct BarPosition;
struct BeamPositions;
struct DisplayNote;
struct DisplayState;
struct Notes;
struct NoteTakerDisplay;

struct BeamPosition {
    unsigned beamMin = INT_MAX;
    float xOffset;
    int yOffset;
    float slurOffset;
    float sx;
    float ex;
    int yStemExtend;
    int y;
    int yLimit;
    unsigned first;
    unsigned last;
    bool outsideStaff;  // set true for tuplet

    BeamPosition(unsigned f, unsigned l, bool os = false) 
        : first(f)
        , last(l)
        , outsideStaff(os) {
    }

    void drawOneBeam(NVGcontext* vg);
    void set(const vector<NoteCache>& notes);
};

struct DisplayCache {
    vector<NoteCache> notes;  // where note is drawn (computed cache, not saved)
    vector<BeamPosition> beams; // where beams/ties/slurs/triplets are drawn
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

    BarPosition();

    void addPos(const NoteCache& , float cacheWidth);

    void advance(const NoteCache& noteCache) {
        SCHMICKLE(INT_MAX != duration);
        while (noteCache.vStartTime >= midiEnd) {
            midiEnd += duration;
        }
    }

    int count(const NoteCache& noteCache) const;
    int noteRegular(int noteDuration , PositionType , int ppq);
    int notesTied(const DisplayNote& , PositionType , int ppq);
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
    Notes* notes;
    DisplayCache* cache;
    
    CacheBuilder(const DisplayState& , Notes* , DisplayCache* );

    void cacheBeams();
    void cacheSlurs();
    void cacheStaff();
    void cacheTuplets(const vector<PositionType>& tuplets);
    void closeBeam(unsigned first, unsigned limit);
    void closeSlur(unsigned first, unsigned limit);
    void setDurations(const vector<PositionType>& );
    void trackPos(std::list<PosAdjust>& posAdjust, float xOff, int endTime);
    void updateXPosition();
};
