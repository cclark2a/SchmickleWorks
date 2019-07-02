#include <iterator>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Storage.hpp"

// to do : not sure if we need radix 64 encoded midi or not ... 
// depends on whether user is allowed to choose to store sequences in patches ...
// the advantage : .vcv files can be distributed including additional sequences
// the disadvantage : auto save time could become onerous
void NoteTakerSlot::UnitTest() {
    // unit test
    vector<uint8_t> midi = { 0x80, 0x90, 0xFF, 0x3F, 0x5F, 0x7F, 0x00, 0x20, 0xAA, 0xCC};
    vector<char> encoded;
    Encode(midi, &encoded);
    vector<uint8_t> copy;
    Decode(encoded, &copy);
    SCHMICKLE(!memcmp(&midi.front(), &copy.front(), midi.size()));
    vector<char> encodeJunk = { '0', 'A', '/', 'b', 'o', '/', '7', '='};
    Decode(encodeJunk, &copy);
    Encode(copy, &encoded);
    SCHMICKLE(!memcmp(&encodeJunk.front(), &encoded.front(), encodeJunk.size()));
}

void NoteTakerSlot::Decode(const vector<char>& encoded, vector<uint8_t>* midi) {
    midi->clear();
    midi->reserve(encoded.size() * 3 / 4);
    SCHMICKLE(encoded.size() / 4 * 4 == encoded.size());
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
    size_t index = midi->size();
    size_t wall = index >= 4 ? index - 4 : 0;
    DEBUG("midi.size %u", index);
    uint8_t test;
    while (index > wall) {
        test = (*midi)[--index];
        if (0xFF == test) {   // to do : use sentinel constant
            midi->resize(index);
            break;
        }
    }
}

void NoteTakerSlot::Encode(const vector<uint8_t>& midi, vector<char>* encoded)  {
    encoded->clear();
    encoded->reserve(midi.size() * 4 / 3);
    unsigned triplets = midi.size() / 3 * 3;
    for (unsigned index = 0; index < triplets; index += 3) {
        EncodeTriplet(&midi[index], encoded);
    }
    unsigned remainder = midi.size() - triplets;  // including one for end sentinel
    if (false) DEBUG("remainder %u", remainder);
    uint8_t trips[3] = {0, 0, 0};
    for (unsigned index = 0; index < remainder; ++index) {
        trips[index] = midi[triplets + index];
    }
    trips[remainder] = 0xFF;    // to do : use sentinel constant
    EncodeTriplet(trips, encoded);
}

// only used by unit test
void NoteTakerSlot::EncodeTriplet(const uint8_t trips[3], vector<char>* encoded) {
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

void NoteTakerSlot::writeToMidi() const {
    vector<uint8_t> midi;
    NoteTakerMakeMidi maker;
    maker.createFromNotes(*this, midi);
    if (midi.empty()) {
        return;
    }
    std::string userDir = SlotArray::UserDirectory();
    if (!system::isDirectory(userDir)) {
        system::createDirectory(userDir);
        SCHMICKLE(system::isDirectory(userDir));
    }
    std::string destPath = userDir + filename;
    int err = remove(destPath.c_str());
    if (err) {
        DEBUG("remove %s err %d", destPath.c_str(), err);
    }
    FILE* dest = fopen(destPath.c_str(), "wb");
    size_t wrote = fwrite((const char*) &midi.front(), 1, midi.size(), dest);
    fclose(dest);
    if (n.debugVerbose) {
        DEBUG("%s wrote %u requested to write %d", destPath.c_str(), wrote, midi.size());
    }
}

static off_t fsize(const std::string& path) {
    struct stat st; 
    return !stat(path.c_str(), &st) ? st.st_size : -1;
}

bool NoteTakerSlot::setFromMidi() {
    std::string sourcePath = directory + filename;
    if (!system::isFile(sourcePath)) {
        DEBUG("%s file can't be read", sourcePath.c_str());
        return false;
    }
    off_t fileSize = fsize(sourcePath);
    if (fileSize < 0) {
        DEBUG("%s fsize failed", sourcePath.c_str());
        return false;
    }
    FILE* source = fopen(sourcePath.c_str(), "rb");
    if (!source) {
        DEBUG("%s fopen failed", sourcePath.c_str());
        return false;
    }
    vector<uint8_t> midi;
    midi.resize(fileSize);
    size_t bytesRead = fread(&midi.front(), 1, fileSize, source);
    fclose(source);
    if (bytesRead != (size_t) fileSize) {
        DEBUG("%s did not read all of file: requested %d read %d", sourcePath.c_str(),
                bytesRead, fileSize);
        return false;
    }
    NoteTakerParseMidi parser(midi, n, channels);
    if (!parser.parseMidi()) {
        DEBUG("failed to parseMidi %s %s", directory, filename);
        return false;
    }
    return true;
}
