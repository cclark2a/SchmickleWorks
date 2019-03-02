#pragma once

#include "SchmickleWorks.hpp"

struct DisplayNote;
struct NoteTaker;

struct NoteTakerDisplay : TransparentWidget, FramebufferWidget {
    NoteTaker* module;
    vector<int> xPositions;  // where note is drawn (computed cache, not saved)
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
    void drawNote(NVGcontext* , const DisplayNote& , int offset, int alpha) const;
    void drawRest(NVGcontext* , const DisplayNote& , int offset, int alpha) const;
    void drawVerticalControl(NVGcontext* ) const;
    int duration(unsigned index) const;

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

    int startTime(unsigned start) const {
        return xPositions[start];
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

};
