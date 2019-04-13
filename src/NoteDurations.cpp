#include "NoteTakerConstants.hpp"
#include <array>

// actual midi times are computed from midi header ppq
static constexpr const std::array<int, 20> noteDurations = {
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

static constexpr const std::array<int, 20> beams = {
    5,  4,  4,  3,  3,
    2,  2,  1,  1,  0,
    0,  0,  0,  0,  0,
    0,  0,  0,  0,  0, 
};

int NoteDurations::Beams(unsigned index) {
    return beams[index];
}

int NoteDurations::Closest(int midi, int ppq) {
    return ToMidi(FromMidi(midi, ppq), ppq);
}

unsigned NoteDurations::Count() {
    return noteDurations.size();
}

unsigned NoteDurations::FromMidi(int midi, int ppq) {
    return FromStd(InStd(midi, ppq));
}

    // to do : make this more efficient
unsigned NoteDurations::FromStd(int duration) {
    for (unsigned i = 0; i < noteDurations.size(); ++i) {
        if (duration <= noteDurations[i]) {
            return i;
        }
    }
    return noteDurations.size() - 1;
}

int NoteDurations::ToMidi(unsigned index, int ppq) {
    return InMidi(ToStd(index), ppq);
}

int NoteDurations::ToStd(unsigned index) {
    return noteDurations[index];
}

bool NoteDurations::TripletPart(int midi, int ppq) {
    int inStd = InStd(midi, ppq);
    assert(inStd);
    return !(inStd & (inStd - 1));  // true if only top bit is set
}
