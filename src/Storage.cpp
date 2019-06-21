#include <fstream>
#include <iterator>
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Storage.hpp"

// to do : not sure if we need radix 64 encoded midi or not ... 
// depends on whether user is allowed to choose to store sequences in patches ...
// the advantage : .vcv files can be distributed including additional sequences
// the disadvantage : auto save time could become onerous
void NoteTakerStorage::UnitTest() {
    // unit test
    NoteTakerStorage test;
    test.midi = { 0x80, 0x90, 0xFF, 0x3F, 0x5F, 0x7F, 0x00, 0x20, 0xAA, 0xCC};
    vector<char> encoded;
    test.encode(&encoded);
    NoteTakerStorage copy;
    copy.decode(encoded);
    SCHMICKLE(!memcmp(&test.midi.front(), &copy.midi.front(), test.midi.size()));
    vector<char> encodeJunk = { '0', 'A', '/', 'b', 'o', '/', '7', '='};
    copy.decode(encodeJunk);
    copy.encode(&encoded);
    SCHMICKLE(!memcmp(&encodeJunk.front(), &encoded.front(), encodeJunk.size()));
}

void NoteTakerStorage::decode(const vector<char>& encoded) {
    midi.clear();
    midi.reserve(encoded.size() * 3 / 4);
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
        midi.insert(midi.end(), stripped, &stripped[3]);
    }
    size_t index = midi.size();
    size_t wall = index >= 4 ? index - 4 : 0;
    DEBUG("midi.size %u", index);
    uint8_t test;
    while (index > wall) {
        test = midi[--index];
        DEBUG("test %u", test);
        if (0xFF == test) {   // to do : use sentinel constant
            midi.resize(index);
            break;
        }
    }
}

void NoteTakerStorage::encode(vector<char>* encoded) const {
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
void NoteTakerStorage::EncodeTriplet(const uint8_t trips[3], vector<char>* encoded) {
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

void NoteTakerStorage::writeMidi() const {
    std::string root = StorageArray::UserDir();
    if (!system::isDirectory(root)) {
        system::createDirectory(root);
        SCHMICKLE(system::isDirectory(root));
    }
    std::string midi = StorageArray::UserMidi();
    if (!system::isDirectory(midi)) {
        system::createDirectory(midi);
        SCHMICKLE(system::isDirectory(midi));
    }
    std::string dest = midi + filename;
    int err = remove(dest.c_str());
    if (err) {
        DEBUG("remove %s err %d", dest.c_str(), err);
    }
    if (midi.empty()) {
        return;
    }
    std::ofstream fout(dest.c_str(), std::ios::out | std::ios::binary);
    fout.write((const char*) &midi.front(), midi.size());
    fout.close();
}

bool NoteTakerStorage::readMidi(const std::string& dir) {
    std::string dest = dir + filename;
    std::ifstream in(dest.c_str(), std::ifstream::binary);
    if (in.fail()) {
        DEBUG("%s not opened for reading", dest.c_str());
        return false;
    }
    in.unsetf(std::ios::skipws);    // don't skip whitespace (!)
    std::streampos fileSize;        // get the file size to reserve space
    in.seekg(0, std::ios::end);
    fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    midi.reserve(fileSize);        
    std::istream_iterator<uint8_t> start(in), end;
    midi.insert(midi.begin(), start, end);
    return true;
}

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
    for (const auto& entry : entries) {
        if (!endsWithMid(entry)) {
            continue;
        }
        size_t lastSlash = entry.rfind('/');
        std::string filename = std::string::npos == lastSlash ? entry : entry.substr(lastSlash + 1);
        if (slotMap.end() != slotMap.find(filename)) {
            continue;
        }
        size_t count = slotMap.size();
        slotMap[filename] = count;
        if (count >= storage.size()) {
            storage.resize(count + 1);
        }
        auto& store = storage[count];
        store.filename = filename;
        store.preset = preset;      
    }
}

// sets up an empty slot in addition to any read slots
void StorageArray::init() {
    storage.clear();
    slotMap.clear();
    this->loadJson();
    this->setMidiMap(StorageArray::UserDir(), false);
    this->setMidiMap(StorageArray::PresetDir(), true);
    for (auto& entry : storage) {
        const std::string& dir = entry.preset ? StorageArray::PresetDir() : StorageArray::UserDir();
        if (!entry.readMidi(dir)) {
            entry.midi.clear();
        }
    }
}

void StorageArray::loadJson() {
    json_t* userJson = nullptr;
	FILE *file = std::fopen(UserJson().c_str(), "r");
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
    this->fromJson(userJson);
}

bool StorageArray::loadScore(NoteTaker& nt, unsigned index) {
    SCHMICKLE(index < storage.size());
    NoteTakerParseMidi parser(storage[index].midi, nt);
    return parser.parseMidi();
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
        NoteTakerStorage noteStorage;
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
