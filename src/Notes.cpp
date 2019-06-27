#include "Notes.hpp"
#include "Channel.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Storage.hpp"

void Notes::deserialize(const vector<uint8_t>& storage) {
    array<NoteTakerChannel, CHANNEL_COUNT> dummyChannels;
    NoteTakerParseMidi midiParser(storage, *this, dummyChannels);
    vector<uint8_t>::const_iterator iter = midiParser.midi.begin();
    DisplayNote note(UNUSED);
    int lastSuccess = 0;
    do {
        int delta;
        int attempt = iter - midiParser.midi.begin();
        if (!midiParser.midi_size8(iter, &delta) || delta < 0) {
            DEBUG("invalid midi time");
            midiParser.debug_out(iter, lastSuccess);
            return;
        }
        note.startTime += delta;
        if (!midiParser.midi_size8(iter, &note.duration) || note.duration < 0) {
            DEBUG("invalid duration");
            midiParser.debug_out(iter, lastSuccess);
            return;
        }
        for (unsigned index = 0; index < 4; ++index) {
            if (!midiParser.midi_size8(iter, &note.data[index])) {
                DEBUG("invalid data %u", index);
                midiParser.debug_out(iter, lastSuccess);
                return;
            }
        }
        if (iter + 1 >= midiParser.midi.end()) {
            DEBUG("unexpected eof");
            return;
        }
        uint8_t byte = *iter++;
        note.type = (DisplayType) (byte >> 4);
        note.channel = byte & 0x0F;
        if (iter >= midiParser.midi.end()) {
            DEBUG("unexpected eof 2");
            return;
        }
        if (*iter != !!*iter) {
            DEBUG("invalid selected");
            midiParser.debug_out(iter, lastSuccess);
            return;
        }
        note.selected = *iter++;
        notes.push_back(note);
        lastSuccess = attempt;
    } while (iter != midiParser.midi.end());
}

void Notes::eraseNotes(unsigned start, unsigned end, unsigned selectChannels) {
    for (auto iter = notes.begin() + end; iter-- != notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            notes.erase(iter);
        }
    }
}

void Notes::fromJson(json_t* jNotes) {
    size_t index;
    json_t* value;
    notes.resize(json_array_size(jNotes), DisplayNote(UNUSED));
    json_array_foreach(jNotes, index, value) {
        notes[index].dataFromJson(value);
    }
}

void Notes::fromJsonCompressed(json_t* jNotes) {
    if (!jNotes) {
        return;
    }
    const char* encodedString = json_string_value(jNotes);
    vector<char> encoded(encodedString, encodedString + strlen(encodedString));
    NoteTakerStorage storage(debugVerbose);
    storage.decode(encoded);
    this->deserialize(storage.midi);    
}

vector<unsigned> Notes::getVoices(unsigned selectChannels, bool atStart) const {
    vector<unsigned> result;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (note.isSelectable(selectChannels)) {
            if (atStart && result.size() && notes[result[0]].startTime != note.startTime) {
                break;
            }
            result.push_back(index);
        }
    }
    std::sort(result.begin(), result.end(), [this](const unsigned& lhs, const unsigned& rhs) {
        return notes[lhs].pitch() < notes[rhs].pitch();
    });
    return result;
}

// not sure what I was thinking (also, this doesn't work)
#if 0
void Notes::HighestOnly(vector<DisplayNote>& span) {
    vector<DisplayNote> highest;
    for (auto& note : span) {
        if (NOTE_ON != note.type) {
            highest.push_back(note);
            continue;
        }
        bool add = true;
        for (auto& test : span) {
            if (NOTE_ON != test.type) {
                continue;
            }
            if (test.channel != note.channel) {
                continue;
            }
            if (test.endTime() <= note.startTime) {
                continue;
            }
            if (test.startTime >= note.endTime()) {
                continue;
            }
            if (test.pitch() >= note.pitch()) {
                add = false;
                break;
            }
        }
        if (add) {
            highest.push_back(note);
        }
    }
    std::swap(highest, span);
}
#endif

// to compute range for horizontal wheel when selecting notes
// to do : horizontalCount, noteToWheel, wheelToNote share loop logic. Consolidate?
unsigned Notes::horizontalCount(unsigned selectChannels) const {
    unsigned count = 0;
    int lastStart = -1;
    for (auto& note : notes) {
        if (note.isSelectable(selectChannels) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
    }
    return count;
}

bool Notes::isEmpty(unsigned selectChannels) const {
    for (auto& note : notes) {
        if (note.isSelectable(selectChannels)) {
            return false;
        }
    }
    return true;
}

// to do : need only early exit if result > 0 ?
int Notes::noteCount(unsigned selectChannels) const {
    int result = 0;
    for (auto& note : notes) {
        if (NOTE_ON == note.type && note.isSelectable(selectChannels)) {
            ++result;
        }
    }
    return result;
}

int Notes::noteTimes(unsigned selectChannels) const {
    int result = 0;
    int lastStart = -1;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isNoteOrRest()) {
            continue;
        }
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (note.startTime == lastStart) {
            continue;
        }
        ++result;
        SCHMICKLE(lastStart < note.startTime);
        lastStart = note.startTime;
    }
    return result;
}

bool Notes::PitchCollision(const vector<DisplayNote>& notes, const DisplayNote& note,
        int pitch, vector<const DisplayNote*>* overlaps) {
    bool collision = false;
    for (auto& test : notes) {
        if (NOTE_ON != test.type) {
            continue;
        }
        if (test.channel != note.channel) {
            continue;
        }
        if (test.endTime() <= note.startTime) {
            continue;
        }
        if (test.startTime >= note.endTime()) {
            break;
        }
        if (&note == &test) {
            continue;
        }
        if (overlaps) {
            overlaps->push_back(&test);
        }
        collision |= test.pitch() == pitch;
    }
    return collision;
}

bool Notes::pitchCollision(const DisplayNote& note, int newPitch) const {
    return Notes::PitchCollision(notes, note, newPitch, nullptr);
}

void Notes::setDuration(DisplayNote* note) {
    vector<const DisplayNote*> overlaps;
    if (!Notes::PitchCollision(notes, *note, note->pitch(), &overlaps)) {
        return;
    }
    for (auto test : overlaps) {
        if (note->endTime() > test->startTime) {
            if (test->pitch() == note->pitch()) {
                note->duration = test->startTime - note->startTime;
            }
        }
    }
}

// unlike saving as MIDI, this preserves rests and other specialized information
void Notes::serialize(vector<uint8_t>& storage) const {
    NoteTakerMakeMidi midiMaker;
    midiMaker.target = &storage;
    int lastStart = 0;
    for (auto& note : notes) {
        if (note.duration < 0) {
            DEBUG("[%d / %u] dur < 0", &note - &notes.front(), notes.size());
            DEBUG(" %s", note.debugString().c_str());
        }
        midiMaker.add_delta(note.startTime, &lastStart);
        midiMaker.add_size8(note.duration);
        for (unsigned index = 0; index < 4; ++index) {
            midiMaker.add_size8(note.data[index]);
        }
        // MIDI-like: pack type and channel into first byte
        uint8_t chan = note.isNoteOrRest() ? note.channel : 0;
        uint8_t byte = (note.type << 4) + chan;
        midiMaker.add_one(byte);
        midiMaker.add_one(note.selected);
    }
}

void Notes::toJson(json_t* root, std::string jsonName) const {
    // to do : write short note seq using old method for ease in editing json manually
#if 0
    json_t* _notes = json_array();
    for (const auto& note : notes) {
        json_array_append_new(_notes, note.dataToJson());
    }
    json_object_set_new(root, "notes", _notes);
#else
    NoteTakerStorage storage(debugVerbose);
    this->serialize(storage.midi);
    vector<char> encoded;
    storage.encode(&encoded);
    encoded.push_back('\0'); // treat as string
    json_object_set_new(root, jsonName.c_str(), json_string(&encoded.front()));
#endif
}

// transpose the span up by approx. one score line (major/aug third) avoiding existing notes
bool Notes::transposeSpan(vector<DisplayNote>& span) const {
    vector<DisplayNote> transposed;
    int newPitch = -1;
    for (auto& note : span) {
        if (NOTE_ON != note.type) {
            continue;
        }
        // collect existing notes on this channel at this time
        vector<const DisplayNote*> overlaps;
        newPitch = (newPitch < 0 ? note.pitch() : newPitch) + 3;
        bool collision = Notes::PitchCollision(notes, note, newPitch, &overlaps);
        collision |= Notes::PitchCollision(transposed, note, newPitch, &overlaps);  // collect new notes in span 
        while (collision && ++newPitch <= 127) {  // transpose up to free slot
            collision = false;
            for (auto overlap : overlaps) {
                if (overlap->pitch() == newPitch) {
                    collision = true;
                    break;
                }
            }
        }
        if (collision) {
            // if no free slots a third-ish above existing notes, use first free slot
            for (newPitch = note.pitch() - 1; collision && newPitch > 0; --newPitch) {
                collision = false;
                for (auto overlap : overlaps) {
                    if (overlap->pitch() == newPitch) {
                        collision = true;
                        break;
                    }
                }
            }
            if (collision) {
                return false;   // fail, tell caller new notes can't be transposed and added
            }
        }
        note.setPitch(newPitch);
        transposed.push_back(note);
    }
    return true;
}

int Notes::xPosAtEndStart(const NoteTakerDisplay* display) const {
    SCHMICKLE(!display->cacheInvalid);
    return notes[selectEnd - 1].cache->xPosition;
}

int Notes::xPosAtEndEnd(const NoteTakerDisplay* display) const {
    SCHMICKLE(!display->cacheInvalid);
    const NoteCache* noteCache = notes[selectEnd - 1].cache;
    return display->xEndPos(*noteCache);
}

int Notes::xPosAtStartEnd(const NoteTakerDisplay* display) const {
    SCHMICKLE(!display->cacheInvalid);
    unsigned startEnd = this->selectEndPos(selectStart);
    return notes[startEnd].cache->xPosition;
}

int Notes::xPosAtStartStart(const NoteTakerDisplay* display) const {
    SCHMICKLE(!display->cacheInvalid);
    return notes[selectStart].cache->xPosition;
}

