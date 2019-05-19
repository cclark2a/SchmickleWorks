#pragma once

#include "NoteTakerDisplayNote.hpp"

struct BarPosition;
struct BeamPositions;
struct DisplayNote;
struct Notes;
struct NoteTaker;

const uint8_t TREBLE_TOP = 29;  // smaller values need additional staff lines and/or 8/15va
const uint8_t C_5 = 32;
const uint8_t MIDDLE_C = 39;
const uint8_t C_3 = 46;
const uint8_t BASS_BOTTOM = 49; // larger values need additional staff lines and/or 8/15vb

enum Accidental : uint8_t {
    NO_ACCIDENTAL,
    SHARP_ACCIDENTAL,
    FLAT_ACCIDENTAL,
    NATURAL_ACCIDENTAL,
};

// map midi note for C major to drawable position
struct StaffNote {
    uint8_t position;  // 0 == G9, 38 == middle C (C4), 74 == C-1
    uint8_t accidental; // 0 none, 1 sharp, 2 flat, 3 natural
};

// drawing notes must line up with extra bar space added by updateXPosition
struct BarPosition {
    struct MinMax {
        float xMin = FLT_MAX;
        float xMax = FLT_MIN;
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

    void addPos(const NoteCache& , float cacheWidth);

    void advance(const NoteCache& noteCache) {
        assert(INT_MAX != duration);
        while (noteCache.vStartTime >= midiEnd) {
            midiEnd += duration;
        }
    }

    void init();
    int notesTied(const DisplayNote& note, int ppq);
    int resetSignatureStart(const DisplayNote& note, float barWidth);

    void setMidiEnd(const NoteCache& noteCache) {
        if (INT_MAX != duration) {
            midiEnd = noteCache.vStartTime + duration;
        }
    }

    void setPriorBars(const NoteCache& noteCache) {
        if (!noteCache.vStartTime) {
            return;
        }
        priorBars += (noteCache.vStartTime - tsStart + duration - 1) / duration;  // rounds up
        pos[priorBars].useMax = true;
        pos[priorBars].xMax = noteCache.vStartTime;
        DEBUG("setPriorBars %d useMax %d xMax", priorBars, true, noteCache.vStartTime);
    }

    void setSignature(const DisplayNote& note, int ppq) {
        duration = ppq * 4 * note.numerator() / (1 << note.denominator());
        tsStart = note.startTime;
        midiEnd = tsStart + duration;
    }

};

struct DisplayBuffer : Widget {
    NoteTakerWidget* mainWidget = nullptr;
    FramebufferWidget* fb = nullptr;

    DisplayBuffer(const Vec& pos, const Vec& size,  NoteTakerWidget* );

    NoteTakerWidget* ntw() {
         return mainWidget;
    }
};

struct NoteTakerDisplay : Widget {
    NoteTakerWidget* mainWidget;
    Notes* previewNotes = nullptr; // hardcoded set of notes for preview
    NVGcontext* vg = nullptr;
    BarPosition bar;
    vector<NoteCache> cache;  // where note is drawn (computed cache, not saved)
    array<Accidental, 75> accidentals;  // marks when accidental was used in bar
    const StaffNote* pitchMap = nullptr;
    float xAxisOffset = 0;
    float xAxisScale = 0.25;
    float xControlOffset = 0;
    unsigned displayStart = 0;
    unsigned displayEnd = 0;
    unsigned oldStart = 0;
    unsigned oldEnd = 0;
    int dynamicPitchAlpha = 0;
    int dynamicSelectAlpha = 0;
    int dynamicTempoAlpha = 0;
    const float fadeDuration = 1;
    float dynamicPitchTimer = 0;
    float dynamicSelectTimer = 0;
    float dynamicTempoTimer = 0;
    float dynamicXOffsetTimer = 0;
    float dynamicXOffsetStep = 0;
    int keySignature = 0;
    int lastTranspose = 60;
    int lastTempo = stdTimePerQuarterNote;
    bool cacheInvalid = false;
    bool upSelected = false;
    bool downSelected = false;
    bool leadingTempo = false;
    bool rangeInvalid = false;

    NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTakerWidget* _ntw);
    void advanceBar(unsigned index);
    void applyKeySignature();
    void cacheBeams();
    unsigned cacheNext(unsigned );
    unsigned cachePrevious(unsigned );
    void cacheSlurs();
    void cacheTuplets();
    float cacheWidth(const NoteCache& ) const;
    void clearTuplet(unsigned index, unsigned limit);
    void closeBeam(unsigned start, unsigned limit);
    void closeSlur(unsigned start, unsigned limit);
    void draw(const DrawArgs& ) override;
    void drawArc(const BeamPositions& bp, unsigned start, unsigned index) const;
    void drawBars();
    void drawBarAt(int xPos);
    void drawBarNote(BarPosition& , const DisplayNote& , const NoteCache& ,
            unsigned char alpha);
    void drawBarRest(BarPosition& bar, const NoteCache& noteCache,
        int xPos, unsigned char alpha) const;
    void drawBeam(unsigned start, unsigned char alpha) const;
    void drawBevel(NVGcontext* vg) const;
    void drawClefs() const;
    void drawDynamicPitchTempo();
    void drawFileControl();
    void drawFreeNote(const DisplayNote& note, NoteCache* noteCache, int xPos,
            unsigned char alpha);
    void drawKeySignature(unsigned index);
    void drawNote(Accidental , const NoteCache&, unsigned char alpha, int size) const;
    void drawNotes();
    void drawPartControl() const;
    void drawSelectionRect();
    void drawSlur(unsigned start, unsigned char alpha) const;
    void drawStaffLines() const;
    void drawSustainControl() const;
    void drawTempo(int xPos, int tempo, unsigned char alpha);
    void drawTie(unsigned start, unsigned char alpha) const;
    void drawTieControl();
    void drawTuple(unsigned index, unsigned char alpha, bool drewBeam) const;
    void drawVerticalControl() const;
    void drawVerticalLabel(const char* label, bool enabled, bool selected, float y) const;

    FramebufferWidget* fb() {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    void fromJson(json_t* root) {
        displayStart = json_integer_value(json_object_get(root, "displayStart"));
        displayEnd = json_integer_value(json_object_get(root, "displayEnd"));
        xAxisOffset = json_real_value(json_object_get(root, "xAxisOffset"));
        xAxisScale = json_real_value(json_object_get(root, "xAxisScale"));
    }

    void invalidateCache() {
        cacheInvalid = true;
        this->fb()->dirty = true;
    }

    unsigned lastAt(unsigned start, int ppq) const {
        assert(start < cache.size());
        int endTime = this->startXPos(start) + NoteDurations::InMidi(box.size.x, ppq);
        unsigned index;
        for (index = start; index < cache.size(); ++index) {
            if (this->startXPos(index) >= endTime) {
                break;
            }
        } 
        return index;
    }

    const Notes* notes();

    NoteTakerWidget* ntw() {
         return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
         return mainWidget;
    }

    void recenterVerticalWheel();

    void reset() {
        xAxisScale = 0.25;
        xControlOffset = 0;
        dynamicPitchAlpha = 0;
        dynamicTempoAlpha = 0;
        dynamicPitchTimer = 0;
        dynamicTempoTimer = 0;
        keySignature = 0;
        lastTranspose = 60;
        lastTempo = stdTimePerQuarterNote;
        upSelected = false;
        downSelected = false;
        cacheInvalid = true;
        this->resetXAxisOffset();
    }

    void resetXAxisOffset() {
        xAxisOffset = 0;
        dynamicXOffsetTimer = 0;
        dynamicXOffsetStep = 0;
    }

    void scroll();

    static void SetNoteColor(NVGcontext* vg, unsigned chan, unsigned char alpha) {
        NVGcolor color = nvgRGBA((1 == chan) * 0xBf, (3 == chan) * 0x7f, (2 == chan) * 0x7f, alpha);
        nvgFillColor(vg, color);
        nvgStrokeColor(vg, color);
    }

    static void SetPartColor(NVGcontext* vg, int index, int part) {
        nvgFillColor(vg, nvgRGBA((1 == index) * 0xBf, (3 == index) * 0x7f,
                (2 == index) * 0x7f, index == part ? 0xaf : 0x6f));
    }

    static void SetSelectColor(NVGcontext* vg, unsigned chan) {
        nvgFillColor(vg, nvgRGBA((2 == chan) * 0xBf, (8 == chan) * 0x7f,
                (4 == chan) * 0x7f, ALL_CHANNELS == chan ? 0x3f : 0x1f));
    }

    void setBeamPos(unsigned first, unsigned last, BeamPositions* bp) const;
    void setCacheDuration();
    void setKeySignature(int key);
    void setRange();
    void setUpAccidentals();

    int startXPos(unsigned start) const {
        return cache[start].xPosition;
    }

    bool stemUp(int position) const {
        return (position <= MIDDLE_C && position > C_5) || position >= C_3;
    }

    // to do : make this more efficient
   // there may be more than two tied notes: subtract note times to figure number
   static unsigned TiedCount(int barDuration, int duration, int ppq) {
        unsigned count = 0;
        do {
            duration -= NoteDurations::Closest(std::min(barDuration, duration), ppq);
            ++count;
        } while (duration > NoteDurations::ToMidi(0, ppq));
        return count;
    }

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "displayStart", json_integer(displayStart));
        json_object_set_new(root, "displayEnd", json_integer(displayEnd));
        json_object_set_new(root, "xAxisOffset", json_real(xAxisOffset));
        json_object_set_new(root, "xAxisScale", json_real(xAxisScale));
        return root;
    }

    void updateRange();
    void updateXPosition();
    void _updateXPosition();  // in progress replacement

    int xEndPos(const NoteCache& noteCache) const {
        return noteCache.xPosition + this->cacheWidth(noteCache);
    }

    float yPos(int position) const {
        return position * 3 - 48.25;   // middle C 60 positioned at 39 maps to 66
    }

};
