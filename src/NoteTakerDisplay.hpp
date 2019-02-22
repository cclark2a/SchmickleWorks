#pragma once

#include "SchmickleWorks.hpp"

struct DisplayNote;
struct NoteTaker;

static constexpr std::array<int, 20> noteDurations = {
       3, //        128th note
       6, //         64th
       9, // dotted  64th
      12, //         32nd
      18, // dotted  32nd
      24, //         16th
      36, // dotted  16th
      48, //          8th
      72, // dotted   8th
      96, //        quarter note
     144, // dotted quarter     
     192, //        half
     288, // dotted half
     384, //        whole note
     576, // dotted whole
     768, //        double whole
    1052, //        triple whole (dotted double whole)
    1536, //     quadruple whole
    2304, //      sextuple whole (dotted quadruple whole)
    3072, //       octuple whole
      };

struct NoteTakerDisplay : TransparentWidget, FramebufferWidget {
    NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* m)
        : module(m) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(NVGcontext* vg) override;
    void drawNote(NVGcontext* , const DisplayNote& , int offset, int alpha);
    void drawRest(NVGcontext* , const DisplayNote& , int offset, int alpha);

    static unsigned DurationIndex(int duration) {
        for (unsigned i = 0; i < noteDurations.size(); ++i) {
            if (duration <= noteDurations[i]) {
                return i;
            }
        }
        return noteDurations.size() - 1;
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

    NoteTaker* module;
};
