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

    std::string sequenceName;
    std::string instrumentName;
    int gmInstrument = -1;
    int releaseMax = 24;
    int releaseMin = 1;  // midi time for smallest interval gate goes low
    int sustainMin = 1;  // midi time for smallest interval gate goes high
    int sustainMax = 24;

    std::string debugString() const;

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

    bool isDefault() const {
        return sequenceName.empty() && instrumentName.empty() && !gmInstrument
                && isDefault(Limit::releaseMax) && isDefault(Limit::releaseMin)
                && isDefault(Limit::sustainMin) && isDefault(Limit::sustainMax);
    }

    bool isDefault(Limit limit) const {
        return this->getLimit(limit) == DefaultLimit(limit);
    }

    void reset() {
        if (false && debugVerbose) DEBUG("channel reset");
        sequenceName = "";
        instrumentName = "";
        gmInstrument = 0;
        releaseMax = DefaultLimit(Limit::releaseMax);
        releaseMin = DefaultLimit(Limit::releaseMin);
        sustainMin = DefaultLimit(Limit::sustainMin);
        sustainMax = DefaultLimit(Limit::sustainMax);
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

    json_t* toJson() const {
        json_t* root = json_object();
        if (!sequenceName.empty()) {
            json_object_set_new(root, "sequenceName", json_string(sequenceName.c_str()));
        }
        if (!instrumentName.empty()) {
            json_object_set_new(root, "instrumentName", json_string(instrumentName.c_str()));
        }
        if (0 != gmInstrument) {
            json_object_set_new(root, "gmInstrument", json_integer(gmInstrument));
        }
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

    void fromJson(json_t* root) {
        json_t* obj;
        if ((obj = json_object_get(root, "sequenceName"))) {
            sequenceName = json_string_value(obj);
        }
        if ((obj = json_object_get(root, "instrumentName"))) {
            instrumentName = json_string_value(obj);
        }
        if ((obj = json_object_get(root, "gmInstrument"))) {
            gmInstrument = json_integer_value(obj);
        }
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

