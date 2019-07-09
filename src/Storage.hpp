#pragma once

#include "Cache.hpp"
#include "Channel.hpp"
#include "Notes.hpp"

struct SlotPlay {
    // switch slots on next ...
    enum class Stage {
        unknown,
        step,
        beat,
        quarterNote,
        bar,
        song,
        never,
    };

    unsigned index = 0;
    unsigned repeat = 1;
    Stage stage = Stage::song;
};

struct NoteTakerSlot {
    Notes n;
    DisplayCache cache;
    array<NoteTakerChannel, CHANNEL_COUNT> channels;
    std::string directory;
    std::string filename;
    bool invalid = true;

    static void Decode(const vector<char>& encoded, vector<uint8_t>* midi);
    static void EncodeTriplet(const uint8_t trips[3], vector<char>* encoded);
    static void Encode(const vector<uint8_t>& midi, vector<char>* encoded);
    void fromJson(json_t* root);
    bool setFromMidi();
    json_t* toJson() const;
    static void UnitTest();
    void writeToMidi() const;
};

struct SlotArray {
    array<NoteTakerSlot, STORAGE_SLOTS> slots;
    vector<SlotPlay> playback;
    unsigned selectStart = 0;
    unsigned selectEnd = 1;
    bool saveZero = false;   // set if single was at left-most position
    const bool debugVerbose;

    void fromJson(json_t* root);

    SlotArray()
    : debugVerbose(DEBUG_VERBOSE) {
        playback.emplace_back();
    }

    void invalidate() {
        slots[selectStart].invalid = true;
    }

    unsigned size() const { return slots.size(); }
    json_t* toJson() const;
    static std::string UserDirectory() { return asset::user("Schmickleworks/"); }
};
