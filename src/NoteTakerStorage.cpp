#include "NoteTakerDisplayNote.hpp"

using std::vector;

struct NoteTakerStorage {
    NoteTakerStorage() {
        // unit test
        vector<uint8_t> midi = { 0x80, 0x90, 0xFF, 0x3F, 0x5F, 0x7F, 0x00, 0x20, 0xAA, 0xCC};
        vector<int8_t> encoded;
        Encode(midi, &encoded);
        vector<uint8_t> copy;
        Decode(encoded, &copy);
        assert(!memcmp(&midi.front(), &copy.front(), midi.size()));
        vector<int8_t> encodeJunk = { '0', 'A', '/', 'b', 'o', '/', '7', '='};
        Decode(encodeJunk, &copy);
        Encode(copy, &encoded);
        assert(!memcmp(&encodeJunk.front(), &encoded.front(), encodeJunk.size()));
    }

    static void Decode(const vector<int8_t>& encoded, vector<uint8_t>* midi) {
        midi->clear();
        midi->reserve(encoded.size() * 3 / 4);
        assert(encoded.size() / 4 * 4 == encoded.size());
        for (unsigned index = 0; index < encoded.size(); index += 4) {
            uint8_t stripped[4];
            for (unsigned inner = 0; inner < 4; ++inner) {
                int8_t ch = encoded[index + inner];
                stripped[inner] = ('/' == ch ? '\\' : ch) - '0';
            }
            stripped[0] |= stripped[3] << 6;
            stripped[1] |= (stripped[3] << 4) & 0xC0;
            stripped[2] |= (stripped[3] << 2) & 0xC0;
            midi->insert(midi->end(), stripped, &stripped[3]);
        }
    }

    static void Encode(const vector<uint8_t>& midi, vector<int8_t>* encoded) {
        encoded->clear();
        encoded->reserve(midi.size() * 4 / 3);
        unsigned triplets = midi.size() / 3 * 3;
        for (unsigned index = 0; index < triplets; index += 3) {
            EncodeTriplet(&midi[index], encoded);
        }
        unsigned remainder = midi.size() - triplets;
        if (!remainder) {
            return;
        }
        uint8_t trips[3] = {0, 0, 0};
        for (unsigned index = 0; index < remainder; ++index) {
            trips[index] = midi[triplets + index];
        }
        EncodeTriplet(trips, encoded);
    }

    static void EncodeTriplet(const uint8_t trips[3], vector<int8_t>* encoded) {
        int8_t out[4];
        out[0] = trips[0];
        out[3] = trips[0] >> 6;
        out[1] = trips[1];
        out[3] |= (trips[1] >> 4) & 0x0C;
        out[2] = trips[2];
        out[3] |= (trips[2] >> 2) & 0x30;
        for (unsigned inner = 0; inner < 4; ++inner) {
            int8_t ch = '0' + (out[inner] & 0x3F);
            out[inner] = '\\' == ch ? '/' : ch;
        }
        encoded->insert(encoded->end(), out, &out[4]);
    }

};

NoteTakerStorage storage;

