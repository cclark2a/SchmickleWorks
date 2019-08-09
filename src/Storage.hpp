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
    unsigned repeat = INT_MAX;
    Stage stage = Stage::song;

    void fromJson(json_t* root) {
        index = json_integer_value(json_object_get(root, "index"));
        repeat = json_integer_value(json_object_get(root, "repeat"));
        stage = (Stage) json_integer_value(json_object_get(root, "stage"));
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
    void fromJson(json_t* root);
    bool setFromMidi();
    json_t* toJson() const;
    static void UnitTest();
    void writeToMidi() const;
};

struct SlotArray {
    array<NoteTakerSlot, SLOT_COUNT> slots;
    vector<SlotPlay> playback;
    unsigned selectStart = 0; // current selection in playback vector
    unsigned selectEnd = 1;
    bool saveZero = false;   // set if single was at left-most position

    void fromJson(json_t* root);

    SlotArray() {
        playback.emplace_back();
    }

    void invalidate() {
        slots[selectStart].invalid = true;
    }

    void shiftSlots(unsigned start, unsigned end) {  // do a 'cut'
        SCHMICKLE(start || playback.size() != end);
        SCHMICKLE(start < end);
        playback.erase(playback.begin() + start, playback.begin() + end);
    }
      
    unsigned size() const { return slots.size(); }
    json_t* toJson() const;
    static std::string UserDirectory() { return asset::user("Schmickleworks/"); }
};
