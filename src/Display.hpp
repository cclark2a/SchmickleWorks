#pragma once

#include "DisplayNote.hpp"
#include "Storage.hpp"

struct BarPosition;
struct BeamPositions;
struct NoteTakerDisplay;

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

struct DisplayBuffer : Widget {
    NoteTakerWidget* mainWidget = nullptr;
    FramebufferWidget* fb = nullptr;

    DisplayBuffer(const Vec& pos, const Vec& size,  NoteTakerWidget* );

    NoteTakerWidget* ntw() {
         return mainWidget;
    }
};

// elements visible by cache builder
struct DisplayState {
    NVGcontext* vg = nullptr;
    FramebufferWidget* fb = nullptr;
    const float xAxisScale;
    float callInterval = 1 / 70.f;
    const int musicFont;
    const bool debugVerbose;

    DisplayState(float xAxisScale, FramebufferWidget* , int musicFont );
};

struct DisplayRange {
    const DisplayState& state;
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
    const bool debugVerbose;

    DisplayRange(const DisplayState& , float boxWidth);

    void fromJson(json_t* root) {
        displayStart = json_integer_value(json_object_get(root, "displayStart"));
        displayEnd = json_integer_value(json_object_get(root, "displayEnd"));
        xAxisOffset = json_real_value(json_object_get(root, "xAxisOffset"));
        xAxisScale = json_real_value(json_object_get(root, "xAxisScale"));
        this->invalidate();
    }

    void invalidate() {
        if (debugVerbose) DEBUG("invalidate state.fb %p dirty %d", state.fb,
                state.fb ? state.fb->dirty : false);
        invalid = true;
        if (state.fb) {
            state.fb->dirty = true;
        }
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

    void scroll(NVGcontext* vg);
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

    DisplayControl(NoteTakerDisplay* , NVGcontext*);
    void autoDrift(float value, float frameTime, int visCells = 3);
    void drawActive(unsigned start, unsigned end) const;
    void drawActiveNarrow(unsigned slot) const;
    void drawEmpty() const;
    void drawEnd() const;
    void drawNumber(const char* prefix, unsigned index) const;
    void drawNote(SlotPlay::Stage) const;
    void drawSlot(unsigned position, unsigned slotIndex, unsigned repeat, SlotPlay::Stage ,
            unsigned firstVisible, unsigned lastVisible) const;
    void drawStart() const;
};

struct NoteTakerDisplay : Widget {
    NoteTakerWidget* mainWidget = nullptr;
    NoteTakerSlot* slot = nullptr;
    NoteTakerSlot* stagedSlot = nullptr;
    Notes* previewNotes = nullptr; // hardcoded set of notes for preview
    array<Accidental, 75> accidentals;  // marks when accidental was used in bar
    DisplayRange range;
    DisplayState state;
    const StaffNote* pitchMap = nullptr;
    int dynamicPitchAlpha = 0;
    int dynamicSelectAlpha = 0;
    int dynamicTempoAlpha = 0;
    const float fadeDuration = 1;
    float lastCall = 0;
    float dynamicPitchTimer = 0;
    float dynamicSelectTimer = 0;
    float dynamicTempoTimer = 0;
    float xControlOffset = 0;
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
    void draw(const DrawArgs& ) override;
    void drawArc(const BeamPositions& bp, unsigned start, unsigned index) const;
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
    void drawNote(Accidental , const NoteCache&, unsigned char alpha, int size) const;
    void drawNotes(BarPosition& bar);
    void drawPartControl() const;
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

    FramebufferWidget* fb() {
        return dynamic_cast<FramebufferWidget*>(parent);
    }

    static const char* GMInstrumentName(unsigned index);

    void invalidateCache();

    void invalidateRange() {
        range.invalidate();
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
        } while (duration > NoteDurations::ToMidi(0, ppq));
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
