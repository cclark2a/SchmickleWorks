#pragma once

#include "SchmickleWorks.hpp"

struct BarPosition;
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

struct NoteTakerDisplay : TransparentWidget, FramebufferWidget {
    NoteTaker* module;
    vector<int> xPositions;  // where note is drawn (computed cache, not saved)
    array<Accidental, 75> accidentals;  // marks when accidental was used in bar
    float xAxisOffset = 0;
    float xAxisScale = 0.25;
    int dynamicPitchAlpha = 0;
    int dynamicTempoAlpha = 0;
    int lastTranspose = 60;
    int lastTempo = stdTimePerQuarterNote;
    bool loading = false;
    bool saving = false;
    bool xPositionsInvalid = false;

    NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* m)
        : module(m) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(NVGcontext* ) override;
    void drawDynamicPitchTempo(NVGcontext* ) const;
    void drawFileControl(NVGcontext* ) const;
    void drawSustainControl(NVGcontext* ) const;
    void drawBar(NVGcontext* , int xPos);
    void drawBarNote(NVGcontext* , BarPosition& , const DisplayNote& note, int xPos,
            int alpha);
    void drawBarRest(NVGcontext* , BarPosition& , const DisplayNote& , int offset,
            int alpha) const;
    void drawFreeNote(NVGcontext* , const DisplayNote& note, int xPos, int alpha) const;
    void drawNote(NVGcontext* , const DisplayNote& , Accidental , int offset, int alpha) const;
    void drawVerticalControl(NVGcontext* ) const;
    int duration(unsigned index) const;

    // to do : make this more efficient
    static unsigned DurationIndex(int duration) {
        for (unsigned i = 0; i < noteDurations.size(); ++i) {
            if (duration <= noteDurations[i]) {
                return i;
            }
        }
        return noteDurations.size() - 1;
    }

    int endTime(unsigned end) const {
        return xPositions[end] + this->duration(end);
    }

    void fromJson(json_t* root) {
        xAxisOffset = json_real_value(json_object_get(root, "xAxisOffset"));
        xAxisScale = json_real_value(json_object_get(root, "xAxisScale"));
    }

    unsigned lastAt(unsigned start) const {
        assert(start < xPositions.size());
        int endTime = this->startTime(start) + box.size.x;
        unsigned index;
        for (index = start; index < xPositions.size(); ++index) {
            if (this->startTime(index) >= endTime) {
                break;
            }
        } 
        return index;
    }

    static unsigned RestIndex(int duration) {
        assert(duration > 0);
        unsigned result = ((unsigned) duration / noteDurations[7]);  // number of whole notes
        duration -= result * noteDurations[7];
        result *= 2;  // skip dotted whole note
        if (result) {
            result += 5; // make 1 whole note return index of 7
        }
        if (duration > 0) {
            result += DurationIndex(duration);
        }
        return result;
    }

    int startTime(unsigned start) const {
        return xPositions[start];
    }

    bool stemUp(int position) const {
        return (position <= MIDDLE_C && position > C_5) || position >= C_3;
    }

    // to do : make this more efficient
   // there may be more than two tied notes: subtract note times to figure number
   static unsigned TiedCount(int barDuration, int duration) {
        unsigned count = 0;
        do {
            unsigned index = DurationIndex(std::min(barDuration, duration));
            duration -= noteDurations[index];
            ++count;
        } while (duration > noteDurations[0]);
        return count;
    }

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "xAxisOffset", json_real(xAxisOffset));
        json_object_set_new(root, "xAxisScale", json_real(xAxisScale));
        return root;
    }

    void updateXPosition();

    int xPos(int index) const {
        return xPositions[index] - xAxisOffset;
    }

    float yPos(int position) const {
        return position * 3 - 48.25;   // middle C 60 positioned at 39 maps to 66
    }

};
