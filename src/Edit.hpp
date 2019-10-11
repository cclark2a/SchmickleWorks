#pragma once

#include "Notes.hpp"
#include "Storage.hpp"

// the wheel that was last used
enum class Wheel {
    none,
    horizontal,
    vertical,
};

std::string InvalDebugStr(Inval );

// keep the original selection in case the edit re-sorts the notes and changes the start, end
struct NoteTakerEdit {
    Notes base;
    vector<SlotPlay> pbase;
    vector<unsigned> voices;  // index of notes in note array, sorted in ascending pitch
    const DisplayNote* horizontalNote;  // note, if any, used to determine wheel value
    const DisplayNote* verticalNote;
    int horizontalValue;
    int selectMaxEnd;  // largest end that does not overlap note following selection
    int nextStart;  // start of first note following selection
    int verticalValue;
    unsigned originalStart; // sel start / end before voice select
    unsigned originalEnd;
    Wheel wheel;
    // because voice is read at play time in process thread, it is duplicated in taker
    // and the duplicate is kept in sync with the bool here
    bool voice;             // true if current or last edit selects one voice from channel


    NoteTakerEdit() {
        clear();
    }

    // called by set wheel range to begin a new edit session
    void clear() {
        base.clear();
        voices.clear();
        this->clearNotes();
        originalStart = originalEnd = selectMaxEnd = nextStart = 0;
        wheel = Wheel::none;
        // leave voice alone
    }

    void clearNotes() {
        horizontalNote = verticalNote = nullptr;
        horizontalValue = verticalValue = INT_MAX;
    }

    void fromJson(json_t* root) {
        BOOL_FROM_JSON(voice);
    }

    void init(const SlotArray& storage) {
        originalStart = storage.slotStart;
        originalEnd = storage.slotEnd;
        pbase.assign(storage.playback.begin() + originalStart,
                storage.playback.begin() + originalEnd);
    }

    // Under some editing conditions, base could contain a subset of notes.
    // But because it might require select start to end of track, copy the whole thing to keep
    // things simple. If this becomes a performance issue, could lazily copy on first edit.

    // Vertical edits: select start to select end is enough
    // Horizontal edits: if edit voice, select start to select end is enough
    // Horizontal edits: if ! edit voice, select start to track end is required

    // When a selection of notes change duration, their order could change, because different
    // notes with different initial durations may their relative ending times. This means that
    // subsequent notes dependent on the prior ending times may need to be resorted.
    // Resorting changes both the pointer to and the index of the note; the note may not be
    // tracked after editing. However, if each editing pass always starts with the base copy,
    // then pointers and indices are reliable. If it is not feasible to start with the base copy,
    // then notes will need an ID field to find them again after they are sorted.

    // base should permit undoing edits, including restoring triplets
    void init(const Notes& n, unsigned selectChannels) {
        base = n;  // see note above: more copying than required most of the time
        nextStart = n.nextStart(selectChannels);
        this->clearNotes();
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            const auto& test = base.notes[index];
            if (!test.isSelectable(selectChannels)) {
                continue;
            }
            int endTime = test.endTime();
            if (endTime <= nextStart) {
                selectMaxEnd = std::max(selectMaxEnd, endTime);
            }
            // would be confusing if horz and vert effected different notes
            // to do : add idiot light to show that vertical wheel has an effect?
            if (!horizontalNote && test.isNoteOrRest()) {
                horizontalNote = &test;
                horizontalValue = NoteDurations::FromMidi(test.duration, n.ppq);
                if (debugVerbose) DEBUG("edit init horizontalValue %d horizontalNote %s",
                        horizontalValue, horizontalNote->debugString().c_str());
                if (NOTE_ON == test.type) {
                    verticalNote = &test;
                    verticalValue = test.pitch();
                } else {
                    verticalNote = nullptr;
                    verticalValue = INT_MAX;
                }
            }
        }
        if (debugVerbose) DEBUG("edit init nextStart %d selectMaxEnd %d", nextStart, selectMaxEnd);
    }

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "voice", json_boolean(voice));
        return root;
    }

};
