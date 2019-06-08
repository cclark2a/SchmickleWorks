#pragma once

#include "SchmickleWorks.hpp"

struct NoteTakerStorage {
    std::string filename;
    vector<uint8_t> midi;
    unsigned slot;

    void decode(const vector<int8_t>& encoded);
    static void EncodeTriplet(const uint8_t trips[3], vector<int8_t>* encoded);
    void encode(vector<int8_t>* encoded) const;
    static std::string MidiDir(bool unitTestRunning = false);
    bool readMidi();
    static void UnitTest();
    void writeMidi() const;
};
