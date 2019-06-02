#include "Notes.hpp"
#include "NoteTakerDisplay.hpp"

void Notes::eraseNotes(unsigned start, unsigned end, unsigned selectChannels) {
    for (auto iter = notes.begin() + end; iter-- != notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            notes.erase(iter);
        }
    }
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
        assert(lastStart < note.startTime);
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
    assert(!display->cacheInvalid);
    return notes[selectEnd - 1].cache->xPosition;
}

int Notes::xPosAtEndEnd(const NoteTakerDisplay* display) const {
    assert(!display->cacheInvalid);
    const NoteCache* noteCache = notes[selectEnd - 1].cache;
    return display->xEndPos(*noteCache);
}

int Notes::xPosAtStartEnd(const NoteTakerDisplay* display) const {
    assert(!display->cacheInvalid);
    unsigned startEnd = this->selectEndPos(selectStart);
    return notes[startEnd].cache->xPosition;
}

int Notes::xPosAtStartStart(const NoteTakerDisplay* display) const {
    assert(!display->cacheInvalid);
    return notes[selectStart].cache->xPosition;
}

