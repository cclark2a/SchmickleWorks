#pragma once

#include "DisplayNote.hpp"

struct NoteTakerSlot;

inline int SecondsToMidi(float seconds, int ppq) {
    return (int) (ppq * seconds * 1000000 / stdMSecsPerQuarterNote);
}

inline float MidiToSeconds(int midiTime, int ppq) {
    return (float) midiTime / ppq / 1000000 * stdMSecsPerQuarterNote;
}

static constexpr array<uint8_t, 4> MThd = {'M', 'T', 'h', 'd'}; // MIDI file header
static constexpr array<uint8_t, 4> MTrk = {'M', 'T', 'r', 'k'}; // MIDI track header

struct NoteTakerMakeMidi {
    vector<uint8_t>* target = nullptr;  // used only during constructing midi, to compute track length
    vector<uint8_t> temp;

    void add_delta(int midiTime, int* lastTime) {
        int delta = midiTime - *lastTime;
        if (delta < 0) {
            DEBUG("! midiTime:%d *lastTime:%d", midiTime, *lastTime);
        }
        SCHMICKLE(delta >= 0);
        add_size8(delta);  // time of first note
        *lastTime = midiTime;
    }

    void add_one(unsigned char c) {
        target->push_back(c);
    }

    void add_bits(unsigned size, int bits) {
        SCHMICKLE(16 == size || 24 == size || 32 == size);
        SCHMICKLE(bits >= 0);
        int shift = size - 8;
        do {
            target->push_back((bits >> shift) & 0xFF);
        } while ((shift -= 8) >= 0);
    }

    void add_channel_prefix(unsigned chan) {
        add_size8(0);
        add_one(midiMetaEvent);
        add_one(0x20);
        add_one(0x01);
        add_one(chan);
    }

    void add_size16(int size) {
        SCHMICKLE(size <= 0xffff);
        add_bits(16, size);
    }

    void add_size24(int size) {
        SCHMICKLE(size <= 0xffffff);
        add_bits(24, size);
    }

    void add_size32(int size) {
        add_bits(32, size);
    }

    void add_size8(int size) {
        SCHMICKLE(size >= 0);
        int64_t buffer = size & 0x7F;
        while ((size >>= 7)) {
            buffer <<= 8;
            buffer |= 0x80 | (size & 0xFF);
        }
        while (true) {
           target->push_back(buffer & 0xFF);
           if (!(buffer & 0x80)) {
               break;
           }
           buffer >>= 8;
        }
    }

    void add_string(std::string str) {
        add_size8(str.length());
        for (char c : str) {
            add_one(c);
        }
    }

    void add_track_end(const DisplayNote& n, int& lastTime) {
        add_delta(n.startTime, &lastTime);
        add_one(midiMetaEvent);
        add_one(midiEndOfTrack);
        add_one(0);  // number of bytes of data to follow
    }

    void createEmpty(vector<uint8_t>& midi);
    void createFromNotes(const NoteTakerSlot& , vector<uint8_t>& midi);

    void standardHeader(vector<uint8_t>& midi, int ppq) {
        target = &midi;
        target->clear();
        target->insert(target->end(), MThd.begin(), MThd.end());
        add_size32(6);  // number of bytes of data to follow
        add_size16(0);  // hardcode to format 0; to do : support writing format 1 ?
        add_size16(1);  // hardcode to 1 track
        add_size16(ppq);
        target->insert(target->end(), MTrk.begin(), MTrk.end());
    // defer adding until size of data is known
        temp.clear();
        target = &temp;
    }

    void standardTrailer(vector<uint8_t>& midi) {
        target = &midi;
        add_size32(temp.size());
        midi.insert(target->end(), temp.begin(), temp.end());
    }
};
