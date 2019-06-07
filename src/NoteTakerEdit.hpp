#pragma once

#include "Notes.hpp"

struct NoteTakerEdit {
    vector<DisplayNote> base;
    vector<unsigned> voices;  // index of notes in note array, sorted in ascending pitch
    const DisplayNote* horizontalNote;  // note, if any, used to determine wheel value
    const DisplayNote* verticalNote;
    int horizontalValue;
    int verticalValue;
    unsigned originalStart;             // sel start / end before voice select
    unsigned originalEnd;

    void clear() {
        base.clear();
        horizontalNote = verticalNote = nullptr;
        horizontalValue = verticalValue = INT_MAX;
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
    }
};
