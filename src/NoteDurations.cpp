#include "Constants.hpp"
#include <array>

// actual midi times are computed from midi header ppq
const std::array<int, 20> noteDurations = {
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
    1152, //        triple whole (dotted double whole)
    1536, //     quadruple whole
    2304, //      sextuple whole (dotted quadruple whole)
    3072, //       octuple whole
};

// if triplet durations included 1 == 256th note, then triplets that span bars could always be
// represented. to do ??
const std::array<int, 20> tripletDurations = {
       2, //        128th note
       4, //         64th
       6, // dotted  64th
       8, //         32nd
      12, // dotted  32nd
      16, //         16th
      24, // dotted  16th
      32, //          8th
      48, // dotted   8th
      64, //        quarter note
      96, // dotted quarter     
     128, //        half
     192, // dotted half
     256, //        whole note
     384, // dotted whole
     512, //        double whole
     768, //        triple whole (dotted double whole)
    1024, //     quadruple whole
    1536, //      sextuple whole (dotted quadruple whole)
    2048, //       octuple whole
};

// number of beams on staves connecting notes of a given duration
const std::array<int, 20> beams = {
    5,  4,  4,  3,  3,
    2,  2,  1,  1,  0,
    0,  0,  0,  0,  0,
    0,  0,  0,  0,  0, 
};

int NoteDurations::Beams(unsigned index) {
    return beams[index];
}

unsigned NoteDurations::Count() {
    return noteDurations.size();
}

unsigned NoteDurations::FromMidi(int midi, int ppq) {
    return FromStd(InStd(midi, ppq));
}

static unsigned from_std(const std::array<int, 20>& durations, int duration) {
    if (durations[0] > duration) {
        return 0;
    }
    return std::upper_bound(durations.begin(), durations.end(), duration) - durations.begin() - 1;
}

bool NoteDurations::Dotted(int midi, int ppq) {
    unsigned index = FromMidi(midi, ppq);
    return index && !(index & 1);
}

unsigned NoteDurations::FromStd(int duration) {
    return from_std(noteDurations, duration);
}

unsigned NoteDurations::FromTripletMidi(int midi, int ppq) {
    return FromTripletStd(InStd(midi, ppq));
}

unsigned NoteDurations::FromTripletStd(int duration) {
    return from_std(tripletDurations, duration);
}

int NoteDurations::InMidi(int std, int ppq) {
    return std * ppq / stdTimePerQuarterNote;
}

int NoteDurations::InStd(int midi, int ppq) {
    return midi * stdTimePerQuarterNote / ppq;
}

// this doesn't return the closest, but returns equal or smaller
// so that parts of tied notes are not larger than the total
int NoteDurations::LtOrEq(int midi, int ppq, bool twoThirds) {
    return twoThirds ? ToTripletMidi(FromTripletMidi(midi, ppq), ppq) :
        ToMidi(FromMidi(midi, ppq), ppq);
}

int NoteDurations::Smallest(int ppq, bool twoThirds) {
    return InMidi(twoThirds ? ToTripletStd(0) : ToStd(0), ppq);
}

int NoteDurations::ToMidi(unsigned index, int ppq) {
    return InMidi(ToStd(index), ppq);
}

int NoteDurations::ToStd(unsigned index) {
    return noteDurations[index];
}

int NoteDurations::ToTripletMidi(unsigned index, int ppq) {
    return InMidi(ToTripletStd(index), ppq);
}

int NoteDurations::ToTripletStd(unsigned index) {
    return tripletDurations[index];
}

// to do : this is only correct if ppq is power of 2 * 3
bool NoteDurations::TripletPart(int midi, int ppq) {
    int inStd = InStd(midi, ppq);
    if (!inStd) {
        DEBUG("%s midi %d ppq %d inStd %d", __func__, midi, ppq, inStd);
        return false;
    }
    SCHMICKLE(inStd);
    return !(inStd & (inStd - 1));  // true if only top bit is set
}

static void validate_array(const std::array<int, 20>& durations, int unit) {
    SCHMICKLE(durations[0] == unit);
    unit += unit;
    bool alternate = true;
    for (unsigned index = 1; index < durations.size(); ++index) {
        if (debugVerbose && durations[index] != unit) {
            DEBUG("durations[%d] != %d", durations[index], unit);
        }
        SCHMICKLE(durations[index] == unit);
        if ((alternate ^= true)) {
            unit += unit / 3;   // dotted to whole
        } else {
            unit += unit / 2;   // whole to dotted
        }
    }
}

void NoteDurations::Validate() {
    // make sure const tables are typo-free
    validate_array(noteDurations, 3);
    validate_array(tripletDurations, 2);
}
