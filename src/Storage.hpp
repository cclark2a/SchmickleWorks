#pragma once

#include "Cache.hpp"
#include "Channel.hpp"
#include "Notes.hpp"

// which cache elements are invalidated; whether to play the current selection
enum class Inval {
    none,
    display,    // invalidates display range, plays note
    cut,        // inval cache, range; does not play
    change,     // pitch or duration change: inval display cache, range, plays note
    note,       // insert note, inval voice, cache, range; plays note
    load,       // inval voice, cache, range; does not play
};

extern std::string InvalDebugStr(Inval );

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
    unsigned repeat = INT_MAX;
    Stage stage = Stage::song;

    void fromJson(json_t* root) {
        INT_FROM_JSON(index);
        INT_FROM_JSON(repeat);
        INT_FROM_JSON(stage);
    }

    json_t* toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "index", json_integer(index));
        json_object_set_new(root, "repeat", json_integer(repeat));
        json_object_set_new(root, "stage", json_integer((int) stage));
        return root;
    }
};

// to fluidly switch between banks of notes on the fly, the display cache and so on must
// be precalcuated. Store them with notes in the slot for that reason
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
    std::string debugString(unsigned index) const;
    void fromJson(json_t* root);
    bool setFromMidi();
    json_t* toJson() const;
    static void UnitTest();
    void writeToMidi() const;
};

struct SlotArray {
    array<NoteTakerSlot, SLOT_COUNT> slots;
    vector<SlotPlay> playback;
    unsigned slotStart = 0; // current selection in playback vector
    unsigned slotEnd = 1;
    bool saveZero = false;   // set if single was at left-most position


    SlotArray() {
        playback.emplace_back();
    }

    const NoteTakerSlot& current() const {
        return slots[slotStart];
    }

    NoteTakerSlot& current() {
        return slots[slotStart];
    }

    static void FromJson(json_t* root, vector<SlotPlay>* playback);
    void fromJson(json_t* root);

    void invalidate() {
#if DEBUG_STORAGE
        if (debugVerbose) DEBUG("%s slotStart:%u", __func__, slotStart);
#endif
        slots[slotStart].invalid = true;
    }

    // to do : assert if select button is not set to insert ?
    unsigned startToWheel() const {  // factors in whether selection is left of first slot
        return slotStart + (int) !saveZero;
    }

    void shiftSlots(unsigned start, unsigned end) {  // do a 'cut'
        SCHMICKLE(start || playback.size() != end);
        SCHMICKLE(start < end);
        playback.erase(playback.begin() + start, playback.begin() + end);
    }
      
    unsigned size() const { return slots.size(); }
    json_t* toJson() const;
    static std::string UserDirectory() { return asset::user("Schmickleworks/"); }

    // to do : assert if select button is not set to insert ?
    void setStartFromWheel(int wheelValue) {
        slotStart = std::max(0, wheelValue - 1);
        slotEnd = slotStart + 1;
        saveZero = !wheelValue;
    }
};
