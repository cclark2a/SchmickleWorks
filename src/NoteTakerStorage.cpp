#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"
#include <fstream>
#include <iterator>

// to do : not sure if we need radix 64 encoded midi or not ... 
// depends on whether user is allowed to choose to store sequences in patches ...
// the advantage : .vcv files can be distributed including additional sequences
// the disadvantage : auto save time could become onerous
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

    static void WriteMidi(const vector<uint8_t>& midi, unsigned slot, std::string dir) {
        std::string dest = dir + std::to_string(slot) + ".mid";
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

    static bool ReadMidi(vector<uint8_t>* midi, unsigned slot, std::string dir) {
        std::string dest = dir + std::to_string(slot) + ".mid";
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
        midi->reserve(fileSize);        
        std::istream_iterator<uint8_t> start(in), end;
        midi->insert(midi->begin(), start, end);
        return true;
    }

};

std::string NoteTakerWidget::midiDir() const {
    std::string dir = asset::user("plugins/Schmickleworks/midi/");
#if RUN_UNIT_TEST
    if (unitTestRunning) {
        dir = asset::user("plugins/Schmickleworks/test/");
    }
#endif
    return dir;
}

// sets up 10 empty slots in addition to any read slots
void NoteTakerWidget::readStorage() {
    unsigned limit = 10;
    storage.clear();
    std::string dir = this->midiDir();
    for (unsigned index = 0; index < limit; ++index) {
        vector<uint8_t> midi;
        if (NoteTakerStorage::ReadMidi(&midi, index, dir)) {
            storage.push_back(midi);
            ++limit;
        } else {
            storage.push_back(vector<uint8_t>());
        }
    }
}

void NoteTakerWidget::writeStorage(unsigned slot) const {
    std::string dir = this->midiDir();
    NoteTakerStorage::WriteMidi(storage[slot], slot, dir);
}

json_t *NoteTaker::dataToJson() {
    json_t *root = json_object();
    json_t* voices = json_array();
    for (unsigned chan = 0; chan < CV_OUTPUTS; ++chan) {
        json_array_append_new(voices, json_integer(outputs[CV1_OUTPUT + chan].getChannels()));
    }
    json_object_set_new(root, "voices", voices);
    json_t* _notes = json_array();
    for (const auto& note : n.notes) {
        json_array_append_new(_notes, note.dataToJson());
    }
    json_object_set_new(root, "notes", _notes);
    json_t* chans = json_array();
    for (const auto& channel : channels) {
        json_array_append_new(chans, channel.dataToJson());
    }
    json_object_set_new(root, "channels", chans);
    json_object_set_new(root, "selectStart", json_integer(n.selectStart));
    json_object_set_new(root, "selectEnd", json_integer(n.selectEnd));
    json_object_set_new(root, "tempo", json_integer(tempo));
    json_object_set_new(root, "ppq", json_integer(n.ppq));
    return root;
}

json_t *NoteTakerWidget::toJson() {
    json_t *root = ModuleWidget::toJson();
    json_t* clip = json_array();
    for (const auto& note : clipboard) {
        json_array_append_new(clip, note.dataToJson());
    }
    json_object_set_new(root, "clipboard", clip);
    // many of these are no-ops, but permits statefulness to change without recoding this block
    json_object_set_new(root, "display", this->display->toJson());
    json_object_set_new(root, "cutButton", this->cutButton->toJson());
    json_object_set_new(root, "fileButton", this->fileButton->toJson());
    json_object_set_new(root, "insertButton", this->insertButton->toJson());
    json_object_set_new(root, "partButton", this->partButton->toJson());
    json_object_set_new(root, "restButton", this->restButton->toJson());
    json_object_set_new(root, "runButton", this->runButton->toJson());
    json_object_set_new(root, "selectButton", this->selectButton->toJson());
    json_object_set_new(root, "sustainButton", this->sustainButton->toJson());
    json_object_set_new(root, "timeButton", this->timeButton->toJson());
    json_object_set_new(root, "horizontalWheel", this->horizontalWheel->toJson());
    json_object_set_new(root, "verticalWheel", this->verticalWheel->toJson());
    // end of mostly no-op section
    json_object_set_new(root, "selectChannels", json_integer(selectChannels));
    return root;
}

void NoteTaker::dataFromJson(json_t *root) {
    json_t* voices = json_object_get(root, "voices");
    size_t index;
    json_t* value;
    json_array_foreach(voices, index, value) {
        outputs[CV1_OUTPUT + index].setChannels(json_integer_value(value));
    }
    json_t* _notes = json_object_get(root, "notes");
    n.notes.resize(json_array_size(_notes));
    json_array_foreach(_notes, index, value) {
        n.notes[index].dataFromJson(value);
    }
    json_t* chans = json_object_get(root, "channels");
    json_array_foreach(chans, index, value) {
        channels[index].dataFromJson(value);
    }
    n.selectStart = json_integer_value(json_object_get(root, "selectStart"));
    n.selectEnd = json_integer_value(json_object_get(root, "selectEnd"));
    tempo = json_integer_value(json_object_get(root, "tempo"));
    n.ppq = json_integer_value(json_object_get(root, "ppq"));
}

void NoteTakerWidget::fromJson(json_t *root) {
    ModuleWidget::fromJson(root);
    json_t* clip = json_object_get(root, "clipboard");
    size_t index;
    json_t* value;
    clipboard.resize(json_array_size(clip));
    json_array_foreach(clip, index, value) {
        clipboard[index].dataFromJson(value);
    }
    // read back controls' state
    display->fromJson(json_object_get(root, "display"));
    cutButton->fromJson(json_object_get(root, "cutButton"));
    fileButton->fromJson(json_object_get(root, "fileButton"));
    insertButton->fromJson(json_object_get(root, "insertButton"));
    partButton->fromJson(json_object_get(root, "partButton"));
    restButton->fromJson(json_object_get(root, "restButton"));
    runButton->fromJson(json_object_get(root, "runButton"));
    selectButton->fromJson(json_object_get(root, "selectButton"));
    sustainButton->fromJson(json_object_get(root, "sustainButton"));
    timeButton->fromJson(json_object_get(root, "timeButton"));
    horizontalWheel->fromJson(json_object_get(root, "horizontalWheel"));
    verticalWheel->fromJson(json_object_get(root, "verticalWheel"));
    // end of controls' state
    selectChannels = json_integer_value(json_object_get(root, "selectChannels"));
    // update display cache
    this->setWheelRange();
    this->invalidateCaches();
    this->setClipboardLight();
}

// to do : remove, to run unit tests only
NoteTakerStorage storage;
