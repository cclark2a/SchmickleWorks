#pragma once

#include "NoteTaker.hpp"

struct NoteTakerParseMidi {
    const vector<uint8_t>& midi;
    vector<DisplayNote>& displayNotes;
    array<NoteTakerChannel, CHANNEL_COUNT>& channels;
    int& ntPpq;
    bool debugVerbose = true;

    NoteTakerParseMidi(const vector<uint8_t>& m, Notes& notes, 
            array<NoteTakerChannel, CHANNEL_COUNT>& chans)
        : midi(m)
        , displayNotes(notes.notes)
        , channels(chans)
        , ntPpq(notes.ppq) {
    }

    NoteTakerParseMidi(const vector<uint8_t>& m, NoteTaker& nt)
        : midi(m)
        , displayNotes(nt.n.notes)
        , channels(nt.channels)
        , ntPpq(nt.n.ppq) {       
        if (nt.debugVerbose) NoteTaker::DebugDumpRawMidi(m);
    }

    bool parseMidi();

    void debug_out(vector<uint8_t>::const_iterator& iter, int lastSuccess = 0) const {
        std::string s;
        auto start = std::max(&midi.front() + lastSuccess, &*iter - 25);
        auto end = std::min(&midi.back(), &*iter + 5);
        const char hex[] = "0123456789ABCDEF";
        for (auto i = start; i <= end; ++i) {
            if (i == &*iter) s += "[";
            s += "0x";
            s += hex[*i >> 4];
            s += hex[*i & 0xf];
            if (i == &*iter) s += "]";
            s += ", ";
        }
        DEBUG("%s", s.c_str());
    }

    template<std::size_t size>
    bool match_midi(vector<uint8_t>::const_iterator& iter, const std::array<uint8_t, size>& data) {
        for (auto dataIter = data.begin(); dataIter != data.end(); ++dataIter) {
            if (*iter != *dataIter) {
                return false;
            }
            ++iter;
        }
        return true;
    }

    bool midi_check7bits(uint8_t byte, const char* label, int time) const {
        if (byte & 0x80) {
            DEBUG("%d expected %s hi bit clear", time, label);
            return false;
        }
        return true;
    }

    bool midi_check7bits(vector<uint8_t>::const_iterator& iter, const char* label, int time) const {
        if (iter == midi.end()) {
            DEBUG("%d looking for %s: unexpected end of file", time, label);
            return false;
        }
        if (*iter & 0x80) {
            debug_out(iter);
        }
        return midi_check7bits(*iter, label, time);
    }

    bool midi_delta(vector<uint8_t>::const_iterator& iter, int* result) const {
        int delta;
        if (!midi_size8(iter, &delta)) {
            return false;
        }
        *result += delta;
        return true;
    }

    static bool Midi_Size8(vector<uint8_t>::const_iterator end,
            vector<uint8_t>::const_iterator& iter, int* result) {
        *result = 0;
        uint8_t byte;
        do {
            if (iter == end) {
                return false;
            }
            byte = *iter++;
            *result <<= 7;
            *result |= byte & 0x7F;
        } while (byte & 0x80);
        return true;
    }

    bool midi_size8(vector<uint8_t>::const_iterator& iter, int* result) const {
        return Midi_Size8(midi.end(), iter, result);
    }

    bool midi_size24(vector<uint8_t>::const_iterator& iter, int* result) const {
        if (iter + 3 >= midi.end()) {
            return false;
        }
        *result = 0;
        for (int i = 0; i < 3; ++i) {
            *result <<= 8;
            *result |= *iter++;
        }
        return true;
    }

    bool midi_size32(vector<uint8_t>::const_iterator& iter, int* result) const {
        if (iter + 4 >= midi.end()) {
            return false;
        }
        *result = 0;
        for (int i = 0; i < 4; ++i) {
            *result <<= 8;
            *result |= *iter++;
        }
        return true;
    }

    bool read_midi16(vector<uint8_t>::const_iterator& iter, int* store) {
        if (iter + 1 >= midi.end()) {
            return false;
        }
        *store = *iter++ << 8;
        *store |= *iter++;
        return true;
    }

#if 0
    int safeMidi_size8(vector<uint8_t>::const_iterator& limit,
            vector<uint8_t>::const_iterator& iter, int ppq);
#endif

};
