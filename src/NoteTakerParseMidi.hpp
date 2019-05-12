#pragma once

#include "NoteTakerChannel.hpp"
#include "NoteTakerDisplayNote.hpp"

class NoteTakerParseMidi {
public:
    NoteTakerParseMidi(const vector<uint8_t>& m, vector<DisplayNote>& d,
            array<NoteTakerChannel, CHANNEL_COUNT>& c, int& p)
        : midi(m)
        , displayNotes(d)
        , channels(c)
        , ntPpq(p) {       
    }

    bool parseMidi();
private:
    const vector<uint8_t>& midi;
    vector<DisplayNote>& displayNotes;
    array<NoteTakerChannel, CHANNEL_COUNT>& channels;
    int& ntPpq;
    bool debugVerbose = true;

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

    bool midi_check7bits(vector<uint8_t>::const_iterator& iter) const {
        if (iter == midi.end()) {
            debug("unexpected end of file 1\n");
            return false;
        }
        if (*iter & 0x80) {
            debug("unexpected high bit set 1\n");
            return false;
        }
        return true;
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

    int safeMidi_size8(vector<uint8_t>::const_iterator& limit,
            vector<uint8_t>::const_iterator& iter, int ppq);

};
