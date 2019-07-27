#pragma once

#include "Notes.hpp"
#include "Storage.hpp"

// which cache elements are invalidated; whether to play the current selection
enum class Inval {
    none,
    display,    // invalidates display range, plays note
    cut,        // inval cache, range; does not play
    change,     // pitch or duration change: inval display cache, range, plays note
    note,       // insert note, inval voice, cache, range; plays note
    load,       // inval voice, cache, range; does not play
};

// the wheel that was last used
enum class Wheel {
    none,
    horizontal,
    vertical,
};

std::string InvalDebugStr(Inval );

struct NoteTakerEdit {
    vector<DisplayNote> base;
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
    bool voice;                 // true if current or last edit selects one voice from channel


    NoteTakerEdit() {
        clear();
    }

    // called by set wheel range to begin a new edit session
    void clear() {
        base.clear();
        voices.clear();
        horizontalNote = verticalNote = nullptr;
        horizontalValue = verticalValue = INT_MAX;
        originalStart = originalEnd = selectMaxEnd = nextStart = 0;
        wheel = Wheel::none;
        // leave voice alone
    }

    void fromJson(json_t* root) {
        voice = json_boolean_value(json_object_get(root, "voice"));
    }

    void init(const SlotArray& storage) {
        originalStart = storage.selectStart;
        originalEnd = storage.selectEnd;
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
        base = n.notes;  // see note above: more copying than required most of the time
        nextStart = n.nextStart(selectChannels);
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            const auto& test = base[index];
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
                if (NOTE_ON == test.type) {
                    verticalNote = &test;
                    verticalValue = test.pitch();
                }
            }
        }
        if (DEBUG_VERBOSE) DEBUG("edit init nextStart %d selectMaxEnd %d", nextStart, selectMaxEnd);
    }

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "voice", json_boolean(voice));
        return root;
    }

};
