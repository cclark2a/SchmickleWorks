#include "Notes.hpp"
#include "NoteTakerChannel.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"

void Notes::deserialize(const vector<uint8_t>& storage) {
    array<NoteTakerChannel, CHANNEL_COUNT> dummyChannels;
    NoteTakerParseMidi midiParser(storage, *this, dummyChannels);
    vector<uint8_t>::const_iterator iter = midiParser.midi.begin();
    DisplayNote note(UNUSED);
    do {
        int delta;
        if (!midiParser.midi_size8(iter, &delta) || delta < 0) {
            DEBUG("invalid midi time");
            midiParser.debug_out(iter);
            return;
        }
        note.startTime += delta;
        if (!midiParser.midi_size8(iter, &note.duration) || note.duration < 0) {
            DEBUG("invalid duration");
            midiParser.debug_out(iter);
            return;
        }
        for (unsigned index = 0; index < 4; ++index) {
            if (!midiParser.midi_size8(iter, &note.data[index])) {
                DEBUG("invalid data %u", index);
                midiParser.debug_out(iter);
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
            midiParser.debug_out(iter);
            return;
        }
        note.selected = *iter++;
        notes.push_back(note);
    } while (iter != midiParser.midi.end());
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

void Notes::eraseNotes(unsigned start, unsigned end, unsigned selectChannels) {
    for (auto iter = notes.begin() + end; iter-- != notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            notes.erase(iter);
        }
    }
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
        bool collision = Notes::UniquePitch(notes, note, newPitch, &overlaps);
        collision |= Notes::UniquePitch(transposed, note, newPitch, &overlaps);  // collect new notes in span 
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

bool Notes::UniquePitch(const vector<DisplayNote>& notes, const DisplayNote& note,
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

bool Notes::uniquePitch(const DisplayNote& note, int newPitch) const {
    return Notes::UniquePitch(notes, note, newPitch, nullptr);
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

