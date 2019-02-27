#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"
#include <fstream>
#include <iterator>

using std::vector;

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

    static void WriteMidi(const vector<uint8_t>& midi, unsigned slot) {
        std::string dest = assetLocal("plugins/Schmickleworks/midi/");
        dest += std::to_string(slot) + ".mid";
        int err = remove(dest.c_str());
        if (err) {
            debug("remove %s err %d", dest.c_str(), err);
        }
        if (midi.empty()) {
            return;
        }
        std::ofstream fout(dest.c_str(), std::ios::out | std::ios::binary);
        fout.write((const char*) &midi.front(), midi.size());
        fout.close();
    }

    static bool ReadMidi(vector<uint8_t>* midi, unsigned slot) {
        std::string dest = assetLocal("plugins/Schmickleworks/midi/");
        dest += std::to_string(slot) + ".mid";
        std::ifstream in(dest.c_str(), std::ifstream::binary);
        if (in.fail()) {
            debug("%s not opened for reading", dest.c_str());
            return false;
        }
        std::istream_iterator<uint8_t> start(in), end;
        midi->insert(midi->begin(), start, end);
        return true;
    }

};

// sets up 10 empty slots in addition to any read slots
void NoteTaker::readStorage() {
    unsigned limit = 10;
    for (unsigned index = 0; index < limit; ++index) {
        vector<uint8_t> midi;
        if (NoteTakerStorage::ReadMidi(&midi, index)) {
            storage.push_back(midi);
            ++limit;
        } else {
            storage.push_back(vector<uint8_t>());
        }
    }
}

void NoteTaker::writeStorage(unsigned slot) const {
    NoteTakerStorage::WriteMidi(storage[slot], slot);
}

json_t *NoteTaker::toJson() {
    json_t* root = json_object();
    json_t* notes = json_array();
    for (const auto& note : allNotes) {
        json_array_append_new(notes, note.toJson());
    }
    json_object_set_new(root, "allNotes", notes);
    json_t* clip = json_array();
    for (const auto& note : clipboard) {
        json_array_append_new(clip, note.toJson());
    }
    json_object_set_new(root, "clipboard", clip);
    json_t* chans = json_array();
    for (const auto& channel : channels) {
        json_array_append_new(chans, channel.toJson());
    }
    json_object_set_new(root, "channels", chans);
    // many of these are no-ops, but permits statefulness to change without recoding this block
    json_object_set_new(root, "display", display->toJson());
    json_object_set_new(root, "cutButton", cutButton->toJson());
    json_object_set_new(root, "fileButton", fileButton->toJson());
    json_object_set_new(root, "insertButton", insertButton->toJson());
    json_object_set_new(root, "partButton", partButton->toJson());
    json_object_set_new(root, "restButton", restButton->toJson());
    json_object_set_new(root, "runButton", runButton->toJson());
    json_object_set_new(root, "selectButton", selectButton->toJson());
    json_object_set_new(root, "sustainButton", sustainButton->toJson());
    json_object_set_new(root, "timeButton", timeButton->toJson());
    json_object_set_new(root, "horizontalWheel", horizontalWheel->toJson());
    json_object_set_new(root, "verticalWheel", verticalWheel->toJson());
    // end of mostly no-op section
    // this section saves more state than strictly necessary ...
    json_object_set_new(root, "displayStart", json_integer(displayStart));
    json_object_set_new(root, "displayEnd", json_integer(displayEnd));
    json_object_set_new(root, "selectStart", json_integer(selectStart));
    json_object_set_new(root, "selectEnd", json_integer(selectEnd));
    json_object_set_new(root, "selectChannels", json_integer(selectChannels));
    json_object_set_new(root, "elapsedSeconds", json_integer(elapsedSeconds));
    json_object_set_new(root, "lastHorizontal", json_integer(lastHorizontal));
    json_object_set_new(root, "lastVertical", json_integer(lastVertical));
    json_object_set_new(root, "tempo", json_integer(tempo));
    json_object_set_new(root, "ppq", json_integer(ppq));
    json_object_set_new(root, "allOutputsOff", json_boolean(allOutputsOff));
    json_object_set_new(root, "playingSelection", json_boolean(playingSelection));
    json_object_set_new(root, "loading", json_boolean(loading));
    json_object_set_new(root, "saving", json_boolean(saving));
    return root;
}

void NoteTaker::fromJson(json_t *root) {
    json_t* notes = json_object_get(root, "allNotes");
    size_t index;
    json_t* value;
    allNotes.reserve(json_array_size(notes));
    json_array_foreach(notes, index, value) {
        allNotes[index].fromJson(value);
    }
    json_t* clip = json_object_get(root, "clipboard");
    clipboard.reserve(json_array_size(clip));
    json_array_foreach(clip, index, value) {
        clipboard[index].fromJson(value);
    }
    json_t* chans = json_object_get(root, "channels");
    json_array_foreach(chans, index, value) {
        channels[index].fromJson(value);
    }
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
    displayStart = json_integer_value(json_object_get(root, "displayStart"));
    displayEnd = json_integer_value(json_object_get(root, "displayEnd"));
    selectStart = json_integer_value(json_object_get(root, "selectStart"));
    selectEnd = json_integer_value(json_object_get(root, "selectEnd"));
    selectChannels = json_integer_value(json_object_get(root, "selectChannels"));
    lastHorizontal = json_integer_value(json_object_get(root, "lastHorizontal"));
    lastVertical = json_integer_value(json_object_get(root, "lastVertical"));
    tempo = json_integer_value(json_object_get(root, "tempo"));
    ppq = json_integer_value(json_object_get(root, "ppq"));
    allOutputsOff = json_integer_value(json_object_get(root, "allOutputsOff"));
    playingSelection = json_integer_value(json_object_get(root, "playingSelection"));
    loading = json_integer_value(json_object_get(root, "loading"));
    saving = json_integer_value(json_object_get(root, "saving"));
}

// to do : remove, to run unit tests only
NoteTakerStorage storage;

