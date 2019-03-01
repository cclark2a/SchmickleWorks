#pragma once

#include "NoteTakerDisplayNote.hpp"

struct NoteTaker;

static constexpr uint8_t midiCVMask = 0xF0;
static constexpr uint8_t midiNoteOff = 0x80;
static constexpr uint8_t midiNoteOn = 0x90;
static constexpr uint8_t midiKeyPressure = 0xA0;
static constexpr uint8_t midiControlChange = 0xB0;
static constexpr uint8_t midiProgramChange = 0xC0;
static constexpr uint8_t midiChannelPressure = 0xD0;
static constexpr uint8_t midiPitchWheel = 0xE0;
static constexpr uint8_t midiSystem = 0xF0;

// control change codes
static constexpr uint8_t midiReleaseMax = 0x57;
static constexpr uint8_t midiReleaseMin = 0x58;
static constexpr uint8_t midiSustainMin = 0x59;
static constexpr uint8_t midiSustainMax = 0x5A;

const uint8_t stdKeyPressure = 0x64;
const int stdTimePerQuarterNote = 0x60;

inline int SecondsToMidi(float seconds, int ppq, int tempo) {
    return (int) (ppq * seconds * 1000000 / tempo);
}

inline float MidiToSeconds(int midiTime, int ppq, int tempo) {
    return (float) midiTime / ppq * tempo / 1000000;
}

static constexpr array<uint8_t, 4> MThd = {0x4D, 0x54, 0x68, 0x64}; // MIDI file header
static constexpr array<uint8_t, 4> MThd_length = {0x00, 0x00, 0x00, 0x06}; // number of bytes of data to follow
static constexpr array<uint8_t, 6> MThd_data = {0x00, 0x00,  // format 0
                                   0x00, 0x01,  // one track
                                   0x00, 0x60}; /// 96 per quarter-note
static constexpr array<uint8_t, 4> MTrk = {0x4D, 0x54, 0x72, 0x6B}; // MIDI track header
static constexpr array<uint8_t, 8> midiDefaultTimeSignature = {0x00, // delta time
                                   0xFF, 0x58, // time signature
                                   0x04, // number of bytes of data to follow
                                   0x04, // four beats per measure
                                   0x02, // beat per quarter note
                                   0x18, // clocks per quarter note
                                   0x08}; // number of 32nd notes in a quarter note
static constexpr array<uint8_t, 7> midiDefaultTempo = {0x00, // delta time
                                   0xFF, 0x51, // tempo
                                   0x03, // number of bytes of data to follow
                                   0x07, 0xA1, 0x20}; // 500,000 usec/quarter note (120 beats/min)
static constexpr array<uint8_t, 3> MTrk_end = {0xFF, 0x2F, 00}; // end of track

class NoteTakerMakeMidi {
public:
//    void createDefaultAsMidi(vector<uint8_t>& midi);
    void createEmpty(vector<uint8_t>& midi);
    void createFromNotes(const NoteTaker& nt, vector<uint8_t>& midi);
private:
    vector<uint8_t>* target = nullptr;  // used only during constructing midi, to compute track length
    vector<uint8_t> temp;

    void add_delta(int midiTime, int* lastTime) {
        int delta = midiTime - *lastTime;
        if (delta < 0) {
            debug("! midiTime:%d *lastTime:%d", midiTime, *lastTime);
        }
        assert(delta >= 0);
        add_size8(delta);  // time of first note
        *lastTime = midiTime;
    }

    void add_one(unsigned char c) {
        target->push_back(c);
    }

    void add_size32(int size) {
        assert(size > 0);
        int shift = 24;
        do {
            target->push_back((size >> shift) & 0xFF);
        } while ((shift -= 8) >= 0);
    }

    void add_size8(int size) {
        assert(size >= 0);
        int64_t buffer = size & 0x7F;
        while ((size >>= 7)) {
            buffer <<= 8;
            buffer |= 0x80 | (size & 0xFF);
        }
        do {
           target->push_back(buffer & 0xFF);
        } while ((buffer >>= 8));
    }

    void add_track_end() {
        target->insert(temp.end(), MTrk_end.begin(), MTrk_end.end());
    }

    void standardHeader(vector<uint8_t>& midi) {
        target = &midi;
        target->clear();
        target->insert(target->end(), MThd.begin(), MThd.end());
        target->insert(target->end(), MThd_length.begin(), MThd_length.end());
        target->insert(target->end(), MThd_data.begin(), MThd_data.end());
        target->insert(target->end(), MTrk.begin(), MTrk.end());
    // defer adding until size of data is known
        temp.clear();
        target = &temp;
    }

    void standardTrailer(vector<uint8_t>& midi) {
        target = &midi;
        add_size32(temp.size());
        midi.insert(midi.end(), temp.begin(), temp.end());
    }
};
