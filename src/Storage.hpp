#pragma once

#include "SchmickleWorks.hpp"

struct NoteTaker;

struct NoteTakerStorage {
    std::string filename;
    vector<uint8_t> midi;
    bool debugVerbose = false;
    bool preset = true;

    NoteTakerStorage(bool dbug) {
        debugVerbose = dbug;
    }

    void decode(const vector<char>& encoded);
    static void EncodeTriplet(const uint8_t trips[3], vector<char>* encoded);
    void encode(vector<char>* encoded) const;
    bool readMidi(const std::string& dir);
    static void UnitTest();
    void writeMidi() const;
};

struct StorageArray {
    vector<NoteTakerStorage> storage;
    std::map<std::string, unsigned> slotMap;
    bool debugVerbose = false;

    StorageArray(bool dbug) {
        debugVerbose = dbug;
    }

    void fromJson(json_t* root, bool preset);
    void init(bool firstTime);
    bool isEmpty(unsigned index) const { return storage[index].midi.empty(); }
    void loadJson(bool preset);
    bool loadScore(NoteTaker& nt, unsigned index);
    void setMidiMap(const std::string& dir, bool preset);
    static std::string PresetDir() { return asset::user("plugins/Schmickleworks/midi/"); }
    static std::string PresetJson() { return "plugins/Schmickleworks/SchmickleWorks.json"; }
    static std::string UserDir() { return asset::user("Schmickleworks/"); }
    static std::string UserMidi() { return asset::user("Schmickleworks/midi/"); }
    static std::string UserJson() { return "SchmickleWorks.json"; }
    std::string name(unsigned index) const { return storage[index].filename; }
    void saveJson();
    void saveScore(const NoteTaker& nt, unsigned index);
    unsigned size() const { return storage.size(); }
    json_t* toJson() const;
};
