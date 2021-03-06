
#include "Button.hpp"
#include "Display.hpp"
#include "Taker.hpp"
#include "Storage.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

json_t* Clipboard::playBackToJson() const {
    json_t* root = json_object();
    json_t* slots = json_array();
    for (const auto& slot : playback) {
        json_array_append_new(slots, slot.toJson());
    }
    json_object_set_new(root, "slots", slots);
    return root;
}

void Clipboard::notesToJson(json_t* root) const {
    if (compressJsonNotes) {
        Notes::ToJsonCompressed(notes, root, "clipboardCompressed");
    } else {
        Notes::ToJsonUncompressed(notes, root, "clipboardUncompressed");
    }
}

json_t* Notes::toJson() const {
    json_t* root = json_object();
    if (compressJsonNotes) {
        Notes::ToJsonCompressed(notes, root, "notesCompressed");
    } else {
        Notes::ToJsonUncompressed(notes, root, "notesUncompressed");
    }
    json_object_set_new(root, "selectStart", json_integer(selectStart));
    json_object_set_new(root, "selectEnd", json_integer(selectEnd));
    json_object_set_new(root, "ppq", json_integer(ppq));
    return root;
}

// to do : write short note seq using old method for ease in editing json manually
void Notes::ToJsonUncompressed(const vector<DisplayNote>& notes, json_t* root,
        std::string jsonName) {
    json_t* _notes = json_array();
    for (const auto& note : notes) {
        json_array_append_new(_notes, note.dataToJson());
    }
    json_object_set_new(root, jsonName.c_str(), _notes);
}

// to do : add dirty bit to notes so that serialize can be cached
// to do : add debugging to time this; if it is causing stutter, add alternative to 
//         write file name only so that autosave never takes too long
void Notes::ToJsonCompressed(const vector<DisplayNote>& notes, json_t* root, std::string jsonName) {
    vector<uint8_t> midi;
    Serialize(notes, midi);
    vector<char> encoded;
    NoteTakerSlot::Encode(midi, &encoded);
    encoded.push_back('\0'); // treat as string
    json_object_set_new(root, jsonName.c_str(), json_string(&encoded.front()));
}

json_t* NoteTaker::dataToJson() {
    json_t* root = json_object();
    json_t* voices = json_array();
    for (unsigned chan = 0; chan < CV_OUTPUTS; ++chan) {
        json_array_append_new(voices, json_integer(outputs[CV1_OUTPUT + chan].getChannels()));
    }
    json_object_set_new(root, "voiceCounts", voices);
    json_object_set_new(root, "tempo", json_integer(tempo));
    return root;
}

json_t* NoteTakerSlot::toJson() const {
    json_t* root = json_object();
    json_object_set_new(root, "n", n.toJson());
    json_t* chans = json_array();
    for (const auto& channel : channels) {
        json_array_append_new(chans, channel.toJson());
    }
    json_object_set_new(root, "channels", chans);
    json_object_set_new(root, "directory", json_string(directory.c_str()));
    json_object_set_new(root, "filename", json_string(filename.c_str()));
    return root;
}

json_t* NoteTakerWidget::toJson() {
    if (debugVerbose) n().validate();  // don't write invalid notes for the next reload
    json_t* root = ModuleWidget::toJson();
    clipboard.notesToJson(root);
    json_object_set_new(root, "clipboardSlots", clipboard.playBackToJson());
    // many of these are no-ops, but permits statefulness to change without recoding this block
    json_object_set_new(root, "display", display->range.toJson());
    json_object_set_new(root, "edit", edit.toJson());
    json_object_set_new(root, "cutButton", cutButton->toJson());
    json_object_set_new(root, "fileButton", fileButton->toJson());
    json_object_set_new(root, "insertButton", insertButton->toJson());
    json_object_set_new(root, "keyButton", keyButton->toJson());
    json_object_set_new(root, "partButton", partButton->toJson());
    json_object_set_new(root, "restButton", restButton->toJson());
// run button state is not saved and restored : module always starts with run off
//    json_object_set_new(root, "runButton", runButton->toJson());
    json_object_set_new(root, "selectButton", selectButton->toJson());
    json_object_set_new(root, "slotButton", slotButton->toJson());
    json_object_set_new(root, "sustainButton", sustainButton->toJson());
    json_object_set_new(root, "tempoButton", tempoButton->toJson());
    json_object_set_new(root, "tieButton", tieButton->toJson());
    json_object_set_new(root, "timeButton", timeButton->toJson());
    json_object_set_new(root, "horizontalWheel", horizontalWheel->toJson());
    json_object_set_new(root, "verticalWheel", verticalWheel->toJson());
    // end of mostly no-op section
    json_object_set_new(root, "selectChannels", json_integer(selectChannels));
    json_object_set_new(root, "storage", storage.toJson());
    json_object_set_new(root, "compressJsonNotes", json_integer(compressJsonNotes));
    json_object_set_new(root, "debugCapture", json_integer(debugCapture));
    json_object_set_new(root, "debugVerbose", json_integer(debugVerbose));
    json_object_set_new(root, "groupByGMInstrument", json_integer(groupByGMInstrument));
    json_object_set_new(root, "midiQuantizer", json_integer(midiQuantizer));
    return root;
}

json_t* SlotArray::toJson() const {
    json_t* root = json_object();
    json_t* _slots = json_array();
    for (auto& slot : slots) {
        json_array_append_new(_slots, slot.toJson());
    }
    json_object_set_new(root, "slots", _slots);  
    json_t* _playback = json_array();
    for (auto& slotPlay : playback) {
        json_array_append_new(_playback, slotPlay.toJson());
    }
    json_object_set_new(root, "playback", _playback);
    json_object_set_new(root, "slotStart", json_integer(slotStart));
    json_object_set_new(root, "slotEnd", json_integer(slotEnd));
    json_object_set_new(root, "saveZero", json_integer((int) saveZero));
    return root;
}

bool Clipboard::fromJsonCompressed(json_t* root, bool uncompressed) {
    if (!Notes::FromJsonCompressed(root, &notes, nullptr, uncompressed)) {
        this->resetNotes();
        return false;
    }
    return true;
}

bool Clipboard::fromJsonUncompressed(json_t* root) {
    return Notes::FromJsonUncompressed(root, &notes);
}

bool Notes::FromJsonCompressed(json_t* jNotes, vector<DisplayNote>* notes, int* ppq,
        bool uncompressed) {
    if (!jNotes) {
        return uncompressed;
    }
    const char* encodedString = json_string_value(jNotes);
    vector<char> encoded(encodedString, encodedString + strlen(encodedString));
    vector<uint8_t> midi;
    NoteTakerSlot::Decode(encoded, &midi);
    return Deserialize(midi, notes, ppq);    
}

bool Notes::FromJsonUncompressed(json_t* jNotes, vector<DisplayNote>* notes) {
    size_t index;
    json_t* value;
    notes->resize(json_array_size(jNotes), DisplayNote(NOTE_OFF));
    json_array_foreach(jNotes, index, value) {
        (*notes)[index].dataFromJson(value);
    }
    return (bool) jNotes;
}

void Notes::fromJson(json_t* root) {
    bool uncompressed = FromJsonUncompressed(json_object_get(root, "notesUncompressed"), &notes);
    // compressed overrides if both present
    if (!FromJsonCompressed(json_object_get(root, "notesCompressed"), &notes, &ppq, uncompressed)) {
        Notes empty;
        notes = empty.notes;
    }
    SCHMICKLE(notes.size() >= 2);
    INT_FROM_JSON(selectStart);
    if (selectStart + 1 >= notes.size()) {
        selectStart = 0;
    }
    INT_FROM_JSON(selectEnd);
    if (selectEnd <= selectStart) {
        selectEnd = selectStart + 1;
    } else if (selectEnd >= notes.size()) {
        selectEnd = notes.size() - 1;
    }
    INT_FROM_JSON(ppq);
    // to do : add ppq validate
}

void NoteTaker::dataFromJson(json_t* root) {
    json_t* voiceCounts = json_object_get(root, "voiceCounts");
    size_t index;
    json_t* value;
    json_array_foreach(voiceCounts, index, value) {
        outputs[CV1_OUTPUT + index].setChannels(json_integer_value(value));
    }
    INT_FROM_JSON(tempo);
}

void NoteTakerSlot::fromJson(json_t* root) {
    n.fromJson(json_object_get(root, "n"));
    json_t* chans = json_object_get(root, "channels");
    size_t index;
    json_t* value;
    json_array_foreach(chans, index, value) {
        channels[index].fromJson(value);
    }
    STRING_FROM_JSON(directory);
    STRING_FROM_JSON(filename);
}

void NoteTakerWidget::fromJson(json_t* root) {
    ModuleWidget::fromJson(root);
    bool clipboardUncompressed = clipboard.fromJsonUncompressed(json_object_get(root,
            "clipboardUncompressed"));
    // compressed overrides if both present
    clipboard.fromJsonCompressed(json_object_get(root, "clipboardCompressed"),
        clipboardUncompressed);
    SlotArray::FromJson(root, &clipboard.playback);
    // read back controls' state
    edit.fromJson(json_object_get(root, "edit"));
    cutButton->fromJson(json_object_get(root, "cutButton"));
    fileButton->fromJson(json_object_get(root, "fileButton"));
    insertButton->fromJson(json_object_get(root, "insertButton"));
    keyButton->fromJson(json_object_get(root, "keyButton"));
    partButton->fromJson(json_object_get(root, "partButton"));
    restButton->fromJson(json_object_get(root, "restButton"));
// run button state is not saved and restored : module always starts with run off
//    runButton->fromJson(json_object_get(root, "runButton"));
    runButton->setValue(0);
    selectButton->fromJson(json_object_get(root, "selectButton"));
    slotButton->fromJson(json_object_get(root, "slotButton"));
    sustainButton->fromJson(json_object_get(root, "sustainButton"));
    tempoButton->fromJson(json_object_get(root, "tempoButton"));
    tieButton->fromJson(json_object_get(root, "tieButton"));
    timeButton->fromJson(json_object_get(root, "timeButton"));
    horizontalWheel->fromJson(json_object_get(root, "horizontalWheel"));
    verticalWheel->fromJson(json_object_get(root, "verticalWheel"));
    // end of controls' state
    if (tieButton->ledOn()) { // turn tie button off if wheel is not pointing to default
            // ... since restoring would require having saved edit state earlier
        (void) horizontalWheel->hasRoundedChanged();
        (void) verticalWheel->hasRoundedChanged();
        if (1 != horizontalWheel->wheelValue() || 1 != verticalWheel->wheelValue()) {
            tieButton->setValue(0);
        }
    }
    INT_FROM_JSON(selectChannels);
    storage.fromJson(json_object_get(root, "storage"));
    display->range.fromJson(json_object_get(root, "display"));
    INT_FROM_JSON(compressJsonNotes);
    INT_FROM_JSON(debugCapture);
    INT_FROM_JSON(debugVerbose);
    INT_FROM_JSON(groupByGMInstrument);
    INT_FROM_JSON(midiQuantizer);
    // update display cache
    this->setWheelRange();
    displayBuffer->redraw();
    this->invalAndPlay(Inval::load);
    this->setClipboardLight();
}

void SlotArray::FromJson(json_t* root, vector<SlotPlay>* playback) {
    json_t* _playback = json_object_get(root, "playback");
    size_t playbackSize = json_array_size(_playback);
    if (playbackSize) {
        playback->resize(playbackSize);
        size_t index;
        json_t* value;
        json_array_foreach(_playback, index, value) {
            (*playback)[index].fromJson(value);
        }
    } else {
        playback->emplace_back();
        playbackSize = playback->size();
    }
}

// to do : for all json reading, validate inputs, and make illegal values legal
void SlotArray::fromJson(json_t* root) {
    json_t* _slots = json_object_get(root, "slots");
    size_t index;
    json_t* value;
    json_array_foreach(_slots, index, value) {
        slots[index].fromJson(value);
    }
    SlotArray::FromJson(root, &playback);
    INT_FROM_JSON(slotStart);
    if (slotStart >= playback.size()) {
        slotStart = 0;
    }
    INT_FROM_JSON(slotEnd);
    if (slotEnd <= slotStart) {
        slotEnd = slotStart + 1;
    } else if (slotEnd > playback.size()) {
        slotEnd = playback.size();
    }
    INT_FROM_JSON(saveZero);
    if (saveZero && slotStart) {
        saveZero = false;
    }
}
