#pragma once

#include "NoteTakerDisplayNote.hpp"

struct BarPosition;
struct BeamPositions;
struct DisplayNote;
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

struct NoteTakerDisplay : TransparentWidget, FramebufferWidget {
    NoteTaker* nt;
    vector<NoteCache> cache;  // where note is drawn (computed cache, not saved)
    array<Accidental, 75> accidentals;  // marks when accidental was used in bar
    const StaffNote* pitchMap = nullptr;
    float xAxisOffset = 0;
    float xAxisScale = 0.25;
    float xControlOffset = 0;
    int dynamicPitchAlpha = 0;
    int dynamicTempoAlpha = 0;
    const float fadeDuration = 1;
    float dynamicPitchTimer = 0;
    float dynamicTempoTimer = 0;
    float dynamicXOffsetTimer = 0;
    float dynamicXOffsetStep = 0;
    int keySignature = 0;
    int lastTranspose = 60;
    int lastTempo = stdTimePerQuarterNote;
    bool upSelected = false;
    bool downSelected = false;
    bool cacheInvalid = false;

    NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* m);
    void applyKeySignature();
    void cacheBeams();
    void cacheSlurs();
    void cacheTuplets();
    void clearTuplet(unsigned index, unsigned limit);
    void closeBeam(unsigned start, unsigned limit);
    void closeSlur(unsigned start, unsigned limit);
    void draw(NVGcontext* ) override;
    void drawBar(NVGcontext* , int xPos);
    void drawBarNote(NVGcontext* , BarPosition& , const DisplayNote& , const NoteCache& ,
            unsigned char alpha);
    void drawBarRest(NVGcontext* , BarPosition& , const DisplayNote& , int offset,
            unsigned char alpha) const;
    void drawBeam(NVGcontext* , unsigned start, unsigned char alpha) const;
    void drawBevel(NVGcontext* ) const;
    void drawClefs(NVGcontext* ) const;
    void drawDynamicPitchTempo(NVGcontext* );
    void drawFileControl(NVGcontext* );
    void drawFreeNote(NVGcontext* , const DisplayNote& note, int xPos, unsigned char alpha) const;
    void drawNote(NVGcontext* , const DisplayNote& , Accidental , const NoteCache&,
            unsigned char alpha, int size, int xPos) const;
    void drawNotes(NVGcontext* , BarPosition& bar, int nextBar);
    void drawPartControl(NVGcontext* ) const;
    void drawSelectionRect(NVGcontext* ) const;
    void drawSlur(NVGcontext* , unsigned start, unsigned char alpha) const;
    void drawStaffLines(NVGcontext* ) const;
    void drawSustainControl(NVGcontext* ) const;
    void drawTempo(NVGcontext* , int xPos, int tempo, unsigned char alpha);
    void drawTieControl(NVGcontext* ) ;
    void drawTuple(NVGcontext* , unsigned index, unsigned char alpha, bool drewBeam) const;
    void drawVerticalControl(NVGcontext* ) const;
    void drawVerticalLabel(NVGcontext* , const char* label,
            bool enabled, bool selected, float y) const;
    int duration(unsigned index, int ppq) const;

    int endTime(unsigned end, int ppq) const {
        return cache[end].xPosition + this->duration(end, ppq);
    }

    void fromJson(json_t* root) {
        xAxisOffset = json_real_value(json_object_get(root, "xAxisOffset"));
        xAxisScale = json_real_value(json_object_get(root, "xAxisScale"));
    }

    void invalidateCache() {
        cacheInvalid = true;
    }

    unsigned lastAt(unsigned start, int ppq) const {
        assert(start < cache.size());
        int endTime = this->startTime(start) + NoteDurations::InMidi(box.size.x, ppq);
        unsigned index;
        for (index = start; index < cache.size(); ++index) {
            if (this->startTime(index) >= endTime) {
                break;
            }
        } 
        return index;
    }

    void recenterVerticalWheel();

    void reset() {
        xAxisOffset = 0;
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

    void restartBar(BarPosition& bar, int& nextBar);

    void resetXAxisOffset() {
        xAxisOffset = 0;
        dynamicXOffsetTimer = 0;
        dynamicXOffsetStep = 0;
    }

    void scroll(NVGcontext* );
    void setBeamPos(unsigned first, unsigned last, BeamPositions* bp) const;
    void setKeySignature(int key);

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

    void setUpAccidentals(NVGcontext* , BarPosition& bar, int& nextBar);

    int startTime(unsigned start) const {
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
        json_object_set_new(root, "xAxisOffset", json_real(xAxisOffset));
        json_object_set_new(root, "xAxisScale", json_real(xAxisScale));
        return root;
    }

    void updateXPosition();

    float yPos(int position) const {
        return position * 3 - 48.25;   // middle C 60 positioned at 39 maps to 66
    }

};
