#pragma once

#include "SchmickleWorks.hpp"

// per cv/gate info

// if note is shorter than sustainMax + releaseMax, releaseMax takes precedence
//       result note is gateOn:sustainMin gateOff:(duration - sustainMin)
// if note is equal to or longer than sustainMax + releaseMax:
//       result note is gateOn:sustainMax gateOff:(duration - sustainMax)
struct NoteTakerChannel {
    int sustainMin;  // midi time for smallest interval gate goes high
    int sustainMax;
    int releaseMin;  // midi time for smallest interval gate goes low
    int releaseMax;
    unsigned noteIndex; // the note currently playing on this channel
    int expiration;  // midi time when gate goes low

    NoteTakerChannel() {
        this->reset();
    }

    void reset() {
        sustainMin = releaseMin = 1;
        sustainMax = releaseMax = 24;  // to do : use a constant here
        noteIndex = INT_MAX;
        expiration = 0;
    }

    int shortest() const {  // shortest time allowed newly created note
        return sustainMin + releaseMin;
    }

    int sustain(int duration) const {
        if (duration < sustainMin) {  // if note is short, sustainMin takes precedence
            return duration;
        }
        if (duration - releaseMax < sustainMax) {
            return std::max(sustainMin, duration - releaseMax);
        }
        return sustainMax;
    }
};
