
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerStorage.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"

json_t* NoteTaker::dataToJson() {
    json_t* root = json_object();
    json_t* voices = json_array();
    for (unsigned chan = 0; chan < CV_OUTPUTS; ++chan) {
        json_array_append_new(voices, json_integer(outputs[CV1_OUTPUT + chan].getChannels()));
    }
    json_object_set_new(root, "voices", voices);
    // to do : write short note seq using old method for ease in editing json manually
#if 0
    // to do : if notes were read from file, write only change (need to implement undo/history)
    //       : if notes have not been saved, write complete
    // to do : move notes to separate file
    // its too expensive to write the notes when the score is large
    // write the unsaved score separately (along with the clipboard), but only when dirty
    json_t* _notes = json_array();
    for (const auto& note : n.notes) {
        json_array_append_new(_notes, note.dataToJson());
    }
    json_object_set_new(root, "notes", _notes);
#else
    NoteTakerStorage storage;
    n.serialize(storage.midi);
    vector<char> encoded;
    storage.encode(&encoded);
    encoded.push_back('\0'); // treat as string
    json_object_set_new(root, "notesCompressed", json_string(&encoded.front()));
#endif
    // to do : rather than write full array, write only channels and fields not equal to default
    // to do : restructure all json writing to write only non-default elements
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

json_t* NoteTakerWidget::toJson() {
    if (debugVerbose) n().validate();  // don't write invalid notes for the next reload
    json_t* root = ModuleWidget::toJson();
    json_t* clip = json_array();
    for (const auto& note : clipboard) {
        json_array_append_new(clip, note.dataToJson());
    }
    json_object_set_new(root, "clipboard", clip);
    // many of these are no-ops, but permits statefulness to change without recoding this block
    json_object_set_new(root, "display", display->toJson());
    json_object_set_new(root, "edit", edit.toJson());
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
    json_object_set_new(root, "selectChannels", json_integer(selectChannels));
    return root;
}

json_t* StorageArray::toJson() const {
    json_t* root = json_array();
    for (const auto& slot : slotMap) {
        json_t* entry = json_object();
        json_object_set_new(entry, "filename", json_string(slot.first.c_str()));
        json_object_set_new(entry, "slot", json_integer(slot.second));
        json_array_append_new(root, entry);
    }
    json_object_set_new(root, "slots", root);
    return root;
}

void NoteTaker::dataFromJson(json_t* root) {
    json_t* voices = json_object_get(root, "voices");
    size_t index;
    json_t* value;
    json_array_foreach(voices, index, value) {
        outputs[CV1_OUTPUT + index].setChannels(json_integer_value(value));
    }
    // notes may be stored here, but are not routinely written here
    json_t* notes = json_object_get(root, "notes");
    n.notes.resize(json_array_size(notes), DisplayNote(UNUSED));
    json_array_foreach(notes, index, value) {
        n.notes[index].dataFromJson(value);
    }
    notes = json_object_get(root, "notesCompressed");   // overrides notes if both present
    if (notes) {
        const char* encodedString = json_string_value(notes);
        vector<char> encoded(encodedString, encodedString + strlen(encodedString));
        NoteTakerStorage storage;
        storage.decode(encoded);
        n.deserialize(storage.midi);    
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

void NoteTakerWidget::fromJson(json_t* root) {
    ModuleWidget::fromJson(root);
    json_t* clip = json_object_get(root, "clipboard");
    size_t index;
    json_t* value;
    clipboard.resize(json_array_size(clip), DisplayNote(UNUSED));
    json_array_foreach(clip, index, value) {
        clipboard[index].dataFromJson(value);
    }
    // read back controls' state
    display->fromJson(json_object_get(root, "display"));
    edit.fromJson(json_object_get(root, "edit"));
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
    if (!nt()->n.notes.size()) {
        unsigned scoreIndex = json_integer_value(json_object_get(root, "score"));
        storage.loadScore(*nt(), scoreIndex);
    }

    // update display cache
    this->setWheelRange();
    this->invalidateAndPlay(Inval::load);
    this->setClipboardLight();
}

void StorageArray::fromJson(json_t* root) {
    json_t* slots = json_object_get(root, "slots");
    size_t index;
    json_t* value;
    json_array_foreach(slots, index, value) {
        std::string filename = std::string(json_string_value(json_object_get(value, "filename")));
        unsigned slot = json_integer_value(json_object_get(value, "slot"));
        slotMap[filename] = slot;
    }
}
