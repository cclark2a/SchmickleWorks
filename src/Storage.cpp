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

void NoteTakerSlot::fromJson(json_t* root) {
    n.fromJson(json_object_get(root, "notes"));
    n.fromJsonCompressed(json_object_get(root, "notesCompressed")); // overrides if both present
    json_t* chans = json_object_get(root, "channels");
    size_t index;
    json_t* value;
    json_array_foreach(chans, index, value) {
        channels[index].dataFromJson(value);
    }
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

#if 0
// matches dot mid and dot midi, case insensitive
static bool endsWithMid(const std::string& str) {
    if (str.size() < 5) {
        return false;
    }
    const char* last = &str.back();
    if ('i' == tolower(*last)) {
        --last;
    }
    const char dotMid[] = ".mid";
    const char* test = last - 3;
    for (int index = 0; index < 3; ++index) {
        if (dotMid[index] != tolower(test[index])) {
            return false;
        }
    }
    return true;
}

void StorageArray::setMidiMap(const std::string& dir, bool preset) {
    std::list<std::string> entries(system::getEntries(dir));
    unsigned freeSlot = 0;
    for (const auto& entry : entries) {
        if (!endsWithMid(entry)) {
            continue;
        }
        size_t lastSlash = entry.rfind('/');
        std::string filename = std::string::npos == lastSlash ? entry : entry.substr(lastSlash + 1);
        auto iter = slotMap.find(filename);
        if (slotMap.end() != iter) {
            continue;
        }
        while (freeSlot < storage.size()
                && !storage[freeSlot].filename.empty()) {
            ++freeSlot;
        }
        if (freeSlot >= storage.size()) {
            storage.resize(freeSlot + 1, debugVerbose);
        }
        auto& store = storage[freeSlot];
        store.filename = filename;
        store.preset = preset;
    }
}

// sets up an empty slot in addition to any read slots
void StorageArray::init(bool firstTime) {
    slotMap.clear();
    this->loadJson(false);
    this->loadJson(true);
    this->setMidiMap(StorageArray::UserMidi(), false);
    this->setMidiMap(StorageArray::PresetDir(), true);
    for (auto& entry : storage) {
        if (entry.filename.empty()) {
            if (debugVerbose) DEBUG("slot %d empty", &entry - &storage.front());
            continue;
        }
        const std::string& dir = entry.preset ? StorageArray::PresetDir() :
                StorageArray::UserMidi();
        if (!entry.readMidi(dir)) {
            entry.midi.clear();
        }
    }
}

void StorageArray::loadJson(bool preset) {
    std::string jsonFile = preset ? StorageArray::PresetJson() : StorageArray::UserJson();
    if (!system::isFile(jsonFile)) {
        DEBUG("isn't found file %s", jsonFile.c_str());
        _schmickled();
        return;
    }
    json_t* userJson = nullptr;
	FILE *file = std::fopen(jsonFile.c_str(), "r");
	if (!file) {
        return;
    }
	DEFER({
		std::fclose(file);
	});
	json_error_t error;
	userJson = json_loadf(file, 0, &error);
	if (!userJson) {
		DEBUG("SchmickleWorks JSON parsing error at %s %d:%d %s",
                error.source, error.line, error.column, error.text);
		return;
	}
	DEFER({
		json_decref(userJson);
	});
    this->fromJson(userJson, preset);
}

void StorageArray::saveJson() {
	json_t *rootJ = this->toJson();
	SCHMICKLE(rootJ);
	std::string tmpPath = UserJson() + ".tmp";
	FILE *file = std::fopen(tmpPath.c_str(), "w");
	SCHMICKLE(file);
	json_dumpf(rootJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
    std::fclose(file);
	system::moveFile(tmpPath, UserJson());
    json_decref(rootJ);
}

void StorageArray::saveScore(const NoteTaker& nt, unsigned index) {
    SCHMICKLE(index <= storage.size());
    if (storage.size() == index) {
        NoteTakerSlot noteStorage(debugVerbose);
        noteStorage.filename = std::to_string(storage.size()) + ".mid";
        slotMap[noteStorage.filename] = storage.size();
        storage.push_back(noteStorage);

    }
    auto& dest = storage[index].midi;
    NoteTakerMakeMidi midiMaker;
    midiMaker.createFromNotes(nt, dest);
    storage[index].writeMidi();
    this->saveJson();
}
#endif
