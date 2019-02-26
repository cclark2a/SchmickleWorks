#pragma once

#include "SchmickleWorks.hpp"

// per cv/gate info

// if note is shorter than sustainMax + releaseMax, releaseMax takes precedence
//       result note is gateOn:sustainMin gateOff:(duration - sustainMin)
// if note is equal to or longer than sustainMax + releaseMax:
//       result note is gateOn:sustainMax gateOff:(duration - sustainMax)
struct NoteTakerChannel {
    enum class Limit {  // order matches UI
        releaseMax,
        releaseMin,
        sustainMin,
        sustainMax,
    };

    int releaseMax;
    int releaseMin;  // midi time for smallest interval gate goes low
    int sustainMin;  // midi time for smallest interval gate goes high
    int sustainMax;
    unsigned noteIndex; // the note currently playing on this channel
    int gateLow;        // midi time when gate goes low (start + sustain)
    int noteEnd;        // midi time when note expires (start + duration)

    NoteTakerChannel() {
        this->reset();
    }

    void reset() {
        sustainMin = releaseMin = 1;
        sustainMax = releaseMax = 24;  // to do : use a constant here
        noteIndex = INT_MAX;
        gateLow = noteEnd = 0;
    }

    void setLimit(Limit limit, int duration) {
        switch (limit) {
            case Limit::releaseMax:
                releaseMax = duration;
                if (releaseMin > duration) {
                    releaseMin = duration;
                }
                break;
            case Limit::releaseMin:
                releaseMin = duration;
                if (releaseMax < duration) {
                    releaseMax = duration;
                }
                break;
            case Limit::sustainMin:
                sustainMin = duration;
                if (sustainMax < duration) {
                    sustainMax = duration;
                }
                break;
            case Limit::sustainMax:
                sustainMax = duration;
                if (sustainMin > duration) {
                    sustainMin = duration;
                }
                break;
            default:
                assert(0);
        }
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
