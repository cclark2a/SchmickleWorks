#pragma once

#include "NoteTakerDisplayNote.hpp"

using std::vector;

class NoteTakerParseMidi {
public:
    NoteTakerParseMidi(const vector<uint8_t>& m, vector<DisplayNote>& d)
        : midi(m)
        , displayNotes(d) {       
    }

    void parseMidi();
private:
    const vector<uint8_t>& midi;
    vector<DisplayNote>& displayNotes;

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

    bool midi_size8(vector<uint8_t>::const_iterator& iter, int* result) const {
        *result = 0;
        do {
            uint8_t byte = *iter++;
            *result += byte & 0x7F;
            if (0 == (byte & 0x80)) {
                break;
            }
            if (iter == midi.end()) {
                return false;
            }
            *result <<= 7;
        } while (true);
        return true;
    }

    bool midi_size24(vector<uint8_t>::const_iterator& iter, int* result) const {
        *result = 0;
        for (int i = 0; i < 3; ++i) {
            if (iter == midi.end()) {
                return false;
            }
            *result |= *iter++;
            *result <<= 8;
        }
        return true;
    }

    bool midi_size32(vector<uint8_t>::const_iterator& iter, int* result) const {
        *result = 0;
        for (int i = 0; i < 4; ++i) {
            if (iter == midi.end()) {
                return false;
            }
            *result |= *iter++;
            *result <<= 8;
        }
        return true;
    }

    void read_midi16(vector<uint8_t>::const_iterator& iter, int* store) {
        *store = *iter++ << 8;
        *store |= *iter++;
    }

};
