#pragma once

#include "DisplayNote.hpp"
#include "Storage.hpp"

struct BarPosition;
struct BeamPositions;
struct NoteTakerDisplay;
struct NoteTakerWidget;

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

extern const StaffNote sharpMap[];
extern const StaffNote flatMap[];

static void debugCaptureRedraw(FramebufferWidget* fb) {
    if (fb) {
        fb->dirty = true;
    }
}

struct DisplayBuffer : Widget {
    NoteTakerWidget* mainWidget = nullptr;
    FramebufferWidget* fb = nullptr;

    DisplayBuffer(const Vec& pos, const Vec& size,  NoteTakerWidget* );

    NoteTakerWidget* ntw() {
         return mainWidget;
    }

    void redraw() {
        debugCaptureRedraw(fb);
    }
};

// elements visible by cache builder
struct DisplayState {
    NVGcontext* vg = nullptr;
    FramebufferWidget* fb = nullptr;
    const float xAxisScale;
    float callInterval = 1 / 70.f;
    const int musicFont;
    unsigned selectedChannels; 

    DisplayState(float xAxisScale, FramebufferWidget* , int musicFont , unsigned selChan);

    void redraw() {
        debugCaptureRedraw(fb);
    }
};

struct DisplayRange {
    DisplayState& state;
    float xAxisOffset = 0;
    float xAxisScale = 0.25;
    float dynamicXOffsetTimer = 0;
    float dynamicXOffsetStep = 0;
    const float bw;       // from display->box.width
    unsigned displayStart = 0;
    unsigned displayEnd = 0;
    unsigned oldStart = 0;
    unsigned oldEnd = 0;
    bool invalid = true;

    DisplayRange(DisplayState& , float boxWidth);

    void fromJson(json_t* root) {
        INT_FROM_JSON(displayStart);
        INT_FROM_JSON(displayEnd);
        REAL_FROM_JSON(xAxisOffset);
        REAL_FROM_JSON(xAxisScale);
        this->invalidate();
    }

    void invalidate() {
        invalid = true;
        state.redraw();
    }

    void reset() {
        displayStart = displayEnd = 0;
        this->invalidate();
    }
    
    void resetXAxisOffset() {
        xAxisOffset = 0;
        dynamicXOffsetTimer = 0;
        dynamicXOffsetStep = 0;
    }

    void scroll();
    void setRange(const Notes& );

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "displayStart", json_integer(displayStart));
        json_object_set_new(root, "displayEnd", json_integer(displayEnd));
        json_object_set_new(root, "xAxisOffset", json_real(xAxisOffset));
        json_object_set_new(root, "xAxisScale", json_real(xAxisScale));
        return root;
    }

    void updateRange(const Notes& , const DisplayCache* , bool editStart);
};

struct DisplayControl {
    NoteTakerDisplay* display;  // initialized by constructor
    NVGcontext* vg;  // initialized by constructor
    const float boxWidth = 25;
    const float margin = 10;   // gap between boxes
    bool horizontal;

    DisplayControl(NoteTakerDisplay* , NVGcontext* , bool horizontal = true);
    void autoDrift(float value, float frameTime, int visCells = 3);
    void drawActive(unsigned start, unsigned end) const;
    void drawActivePointer(float position, float offset) const;
    void drawActiveNarrow(unsigned slot) const;
    void drawEdgeGradient(unsigned position, unsigned firstVisible, unsigned lastVisible) const;
    void drawEmpty() const;
    void drawEnd() const;
    void drawNumber(const char* prefix, unsigned index) const;
    void drawNoteCommon() const;
    void drawSlot(unsigned position, unsigned slotIndex, unsigned repeat, SlotPlay::Stage ,
            unsigned firstVisible, unsigned lastVisible) const;
    void drawSlotNote(SlotPlay::Stage) const;
    void drawStart() const;
    void drawTie(int position, unsigned index) const;
    void drawTriplet(int position, unsigned index) const;
};

struct NoteTakerDisplay : Widget {
    NoteTakerWidget* mainWidget = nullptr;
    NoteTakerSlot* slot = nullptr;
    NoteTakerSlot* stagedSlot = nullptr;
//    Notes* previewNotes = nullptr; // hardcoded set of notes for preview
    array<Accidental, 75> accidentals;  // marks when accidental was used in bar
    DisplayRange range;
    DisplayState state;
    const StaffNote* pitchMap = nullptr;
    int dynamicPitchAlpha = 0;
    int dynamicCNaturalAlpha = 0;   // used to draw C natural to show 'invisible' key signatures
    int dynamicTempoAlpha = 0;
    const float fadeDuration = 1;
    float lastCall = 0;
    float dynamicPitchTimer = 0;
    float dynamicCNaturalTimer = 0;
    float dynamicTempoTimer = 0;
    float xControlOffset = 0;
    float yControlOffset = 0;
    int keySignature = 0;
    int lastTranspose = 60;
    int lastTempo = stdTimePerQuarterNote;
    bool upSelected = false;
    bool downSelected = false;

    NoteTakerDisplay(const Vec& pos, const Vec& size, FramebufferWidget* , NoteTakerWidget* );
    void advanceBar(BarPosition& bar, unsigned index);
    void applyKeySignature();
    const DisplayCache* cache() const;
    static float CacheWidth(const NoteCache& , NVGcontext* );
    void debugDump(unsigned start, unsigned end) const;
    void draw(const DrawArgs& ) override;
    void drawArc(const BeamPosition& bp, unsigned start, unsigned index) const;
    void drawBars(const BarPosition& bar);
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
    void drawName(std::string ) const;
    void drawNote(Accidental , const NoteCache&, unsigned char alpha, int size) const;
    void drawNotes(BarPosition& bar);
    void drawPartControl();
    void drawSelectionRect();
    void drawSlotControl();
    void drawSlur(unsigned start, unsigned char alpha) const;
    void drawStaffLines() const;
    void drawSustainControl() const;
    void drawTempo(int xPos, int tempo, unsigned char alpha);
    void drawTie(unsigned start, unsigned char alpha) const;
    void drawTieControl();
    void drawTuple(unsigned index, unsigned char alpha, bool drewBeam) const;
    void drawVerticalControl() const;
    void drawVerticalLabel(const char* label, bool enabled, bool selected, float y) const;

    static const char* GMInstrumentName(unsigned index);

    void invalidateCache();

    void invalidateRange() {
        range.invalidate();
    }

    Notes* notes();

    NoteTakerWidget* ntw() {
         return mainWidget;
    }

    const NoteTakerWidget* ntw() const {
         return mainWidget;
    }

    void recenterVerticalWheel();

    void redraw() {
        FramebufferWidget* fb = dynamic_cast<FramebufferWidget*>(parent);
        debugCaptureRedraw(fb);
    }

    void reset() {
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
        this->invalidateCache();
        range.resetXAxisOffset();
    }

    static void SetNoteColor(NVGcontext* vg, unsigned chan, unsigned char alpha);
    static void SetPartColor(NVGcontext* vg, int index, int part);
    static void SetSelectColor(NVGcontext* vg, unsigned chan);
    void setKeySignature(int key);
    void setUpAccidentals(BarPosition& bar);

    static bool StemUp(int position) {
        return (position <= MIDDLE_C && position > C_5) || position >= C_3;
    }

    // to do : make this more efficient
   // there may be more than two tied notes: subtract note times to figure number
   static unsigned TiedCount(int barDuration, int duration, int ppq) {
        unsigned count = 0;
        do {
            duration -= NoteDurations::LtOrEq(std::min(barDuration, duration), ppq);
            ++count;
        } while (duration >= NoteDurations::ToMidi(0, ppq));
        return count;
    }

    static float TimeSignatureWidth(const DisplayNote& note, NVGcontext* vg,
            int musicFont);

    static int XEndPos(const NoteCache& noteCache, NVGcontext* vg) {
        return noteCache.xPosition + CacheWidth(noteCache, vg);
    }

    static float YPos(int position) {
        return position * 3 - 48.25;   // middle C 60 positioned at 39 maps to 66
    }

};
