#pragma once

#include "SchmickleWorks.hpp"

struct DisplayNote;

struct Voice {
    const DisplayNote* note;  // the note currently playing on this channel
    double realStart;   // real time when note started (used to recycle voice)
    int gateLow;        // midi time when gate goes low (start + sustain)
    int noteEnd;        // midi time when note expires (start + duration)
};

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
    // written by step:
    array<Voice, 16> voices;
    unsigned voiceCount;     // number of simultaneous notes on channel (needs better name?)

    NoteTakerChannel() {
        this->reset();
    }

    std::string debugString(const DisplayNote* base) const;

    static int DefaultLimit(Limit limit) {
         switch (limit) {
            case Limit::releaseMax:
                return 24;
            case Limit::releaseMin:
                return 1;
            case Limit::sustainMin:
                return 1;
            case Limit::sustainMax:
                return 24;
            default:
                _schmickled();
        }
        return 0;
    }

    int getLimit(Limit limit) const {
        switch (limit) {
            case Limit::releaseMax:
                return releaseMax;
            case Limit::releaseMin:
                return releaseMin;
            case Limit::sustainMin:
                return sustainMin;
            case Limit::sustainMax:
                return sustainMax;
            default:
                _schmickled();
        }
        return 0;
    }

    bool isDefault(Limit limit) const {
        return this->getLimit(limit) == DefaultLimit(limit);
    }

    void reset() {
    //    DEBUG("channel reset");
        releaseMax = DefaultLimit(Limit::releaseMax);
        releaseMin = DefaultLimit(Limit::releaseMin);
        sustainMin = DefaultLimit(Limit::sustainMin);
        sustainMax = DefaultLimit(Limit::sustainMax);
        voiceCount = 0;
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
                _schmickled();
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

    json_t* dataToJson() const {
        json_t* root = json_object();
        if (DefaultLimit(Limit::releaseMax) != releaseMax) {
            json_object_set_new(root, "releaseMax", json_integer(releaseMax));
        }
        if (DefaultLimit(Limit::releaseMin) != releaseMin) {
            json_object_set_new(root, "releaseMin", json_integer(releaseMin));
        }
        if (DefaultLimit(Limit::sustainMin) != sustainMin) {
            json_object_set_new(root, "sustainMin", json_integer(sustainMin));
        }
        if (DefaultLimit(Limit::sustainMax) != sustainMax) {
            json_object_set_new(root, "sustainMax", json_integer(sustainMax));
        }
        return root;
    }

    void dataFromJson(json_t* root) {
        json_t* obj;
        if ((obj = json_object_get(root, "releaseMax"))) {
            releaseMax = json_integer_value(obj);
        }
        if ((obj = json_object_get(root, "releaseMin"))) {
            releaseMin = json_integer_value(obj);
        }
        if ((obj = json_object_get(root, "sustainMin"))) {
            sustainMin = json_integer_value(obj);
        }
        if ((obj = json_object_get(root, "sustainMax"))) {
            sustainMax = json_integer_value(obj);
        }
    }
};

extern const NoteTakerChannel::Limit NoteTakerChannelLimits[4];

