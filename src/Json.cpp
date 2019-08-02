
#include "Button.hpp"
#include "Display.hpp"
#include "Taker.hpp"
#include "Storage.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"


json_t* Notes::toJson() const {
    json_t* root = json_object();
    ToJsonCompressed(notes, root, "notesCompressed");
    json_object_set_new(root, "selectStart", json_integer(selectStart));
    json_object_set_new(root, "selectEnd", json_integer(selectEnd));
    json_object_set_new(root, "ppq", json_integer(ppq));
    return root;
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
    clipboard.toJsonCompressed(root);
    json_object_set_new(root, "clipboardSlots", clipboard.playBackToJson());
    // many of these are no-ops, but permits statefulness to change without recoding this block
    json_object_set_new(root, "display", display->range.toJson());
    json_object_set_new(root, "edit", edit.toJson());
    json_object_set_new(root, "cutButton", cutButton->toJson());
    json_object_set_new(root, "fileButton", fileButton->toJson());
    json_object_set_new(root, "insertButton", insertButton->toJson());
    json_object_set_new(root, "partButton", partButton->toJson());
    json_object_set_new(root, "restButton", restButton->toJson());
    json_object_set_new(root, "runButton", runButton->toJson());
    json_object_set_new(root, "selectButton", selectButton->toJson());
    json_object_set_new(root, "slotButton", slotButton->toJson());
    json_object_set_new(root, "sustainButton", sustainButton->toJson());
    json_object_set_new(root, "timeButton", timeButton->toJson());
    json_object_set_new(root, "horizontalWheel", horizontalWheel->toJson());
    json_object_set_new(root, "verticalWheel", verticalWheel->toJson());
    // end of mostly no-op section
    json_object_set_new(root, "selectChannels", json_integer(selectChannels));
    json_object_set_new(root, "storage", storage.toJson());
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
    json_object_set_new(root, "selectStart", json_integer(selectStart));
    json_object_set_new(root, "selectEnd", json_integer(selectEnd));
    return root;
}

void Notes::fromJson(json_t* root) {
    FromJsonUncompressed(json_object_get(root, "notesUncompressed"), &notes);
    // compressed overrides if both present
    if (!FromJsonCompressed(json_object_get(root, "notesCompressed"), &notes, &ppq)) {
        Notes empty;
        notes = empty.notes;
    }
    selectStart = json_integer_value(json_object_get(root, "selectStart"));
    if (selectStart >= notes.size()) {
        selectStart = 0;
    }
    selectEnd = json_integer_value(json_object_get(root, "selectEnd"));
    if (selectEnd <= selectStart) {
        selectEnd = selectStart + 1;
    }
    ppq = json_integer_value(json_object_get(root, "ppq"));
    // to do : add ppq validate
}

void NoteTaker::dataFromJson(json_t* root) {
    json_t* voiceCounts = json_object_get(root, "voiceCounts");
    size_t index;
    json_t* value;
    json_array_foreach(voiceCounts, index, value) {
        outputs[CV1_OUTPUT + index].setChannels(json_integer_value(value));
    }
    tempo = json_integer_value(json_object_get(root, "tempo"));
}

void NoteTakerSlot::fromJson(json_t* root) {
    n.fromJson(json_object_get(root, "n"));
    json_t* chans = json_object_get(root, "channels");
    size_t index;
    json_t* value;
    json_array_foreach(chans, index, value) {
        channels[index].fromJson(value);
    }
    directory = std::string(json_string_value(json_object_get(root, "directory")));
    filename = std::string(json_string_value(json_object_get(root, "filename")));
}

void NoteTakerWidget::fromJson(json_t* root) {
    ModuleWidget::fromJson(root);
    clipboard.fromJsonUncompressed(json_object_get(root, "clipboardUncompressed"));
    clipboard.fromJsonCompressed(json_object_get(root, "clipboardCompressed"));
    // read back controls' state
    edit.fromJson(json_object_get(root, "edit"));
    cutButton->fromJson(json_object_get(root, "cutButton"));
    fileButton->fromJson(json_object_get(root, "fileButton"));
    insertButton->fromJson(json_object_get(root, "insertButton"));
    partButton->fromJson(json_object_get(root, "partButton"));
    restButton->fromJson(json_object_get(root, "restButton"));
    runButton->fromJson(json_object_get(root, "runButton"));
    selectButton->fromJson(json_object_get(root, "selectButton"));
    slotButton->fromJson(json_object_get(root, "slotButton"));
    sustainButton->fromJson(json_object_get(root, "sustainButton"));
    timeButton->fromJson(json_object_get(root, "timeButton"));
    horizontalWheel->fromJson(json_object_get(root, "horizontalWheel"));
    verticalWheel->fromJson(json_object_get(root, "verticalWheel"));
    // end of controls' state
    selectChannels = json_integer_value(json_object_get(root, "selectChannels"));
    storage.fromJson(json_object_get(root, "storage"));
    display->range.fromJson(json_object_get(root, "display"));
    // update display cache
    this->setWheelRange();
    this->invalidateAndPlay(Inval::load);
    this->setClipboardLight();
}

// to do : for all json reading, validate inputs, and make illegal values legal
void SlotArray::fromJson(json_t* root) {
    json_t* _slots = json_object_get(root, "slots");
    size_t index;
    json_t* value;
    json_array_foreach(_slots, index, value) {
        slots[index].fromJson(value);
    }
    json_t* _playback = json_object_get(root, "playback");
    size_t playbackSize = json_array_size(_playback);
    if (playbackSize) {
        playback.resize(playbackSize);
        json_array_foreach(_playback, index, value) {
            playback[index].fromJson(value);
        }
    } else {
        playback.emplace_back();
        playbackSize = playback.size();
    }
    selectStart = json_integer_value(json_object_get(root, "selectStart"));
    if (selectStart >= playbackSize) {
        selectStart = 0;
    }
    selectEnd = json_integer_value(json_object_get(root, "selectEnd"));
    if (selectEnd <= selectStart) {
        selectEnd = selectStart + 1;
    }
}
