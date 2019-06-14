#pragma once

#include "Notes.hpp"

// which cache elements are invalidated; whether to play the current selection
enum class Inval {
    none,
    display,    // invalidates display range, plays note
    cut,        // inval cache, range; does not play
    change,     // pitch or duration change: inval display cache, range, plays note
    note,       // insert note, inval voice, cache, range; plays note
    load,       // inval voice, cache, range; does not play
};

std::string InvalDebugStr(Inval );

struct NoteTakerEdit {
    vector<DisplayNote> base;
    vector<unsigned> voices;  // index of notes in note array, sorted in ascending pitch
    const DisplayNote* horizontalNote;  // note, if any, used to determine wheel value
    const DisplayNote* verticalNote;
    int trackEndTime;
    int horizontalValue;
    int verticalValue;
    unsigned originalStart; // sel start / end before voice select
    unsigned originalEnd;
    bool voice;                 // true if current or last edit selects one voice from channel

    NoteTakerEdit() {
        clear();
    }

    // called by set wheel range to begin a new edit session
    void clear() {
        base.clear();
        voices.clear();
        trackEndTime = 0;
        horizontalNote = verticalNote = nullptr;
        horizontalValue = verticalValue = INT_MAX;
        originalStart = originalEnd = 0;
        // leave voice alone
    }

    void fromJson(json_t* root) {
        voice = json_boolean_value(json_object_get(root, "voice"));
    }

    void init(const Notes& n, unsigned selectChannels) {
        base = vector<DisplayNote>(&n.notes[n.selectStart], &n.notes[n.selectEnd]);
        for (const auto& test : base) {
            if (!test.isSelectable(selectChannels)) {
                continue;
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
                break;
            }
        }
        SCHMICKLE(TRACK_END == n.notes.back().type);
        trackEndTime = n.notes.back().startTime;
    }

    json_t *toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "voice", json_boolean(voice));
        return root;
    }

};
