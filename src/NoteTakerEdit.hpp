#pragma once

#include "Notes.hpp"

struct NoteTakerEdit {
    vector<DisplayNote> base;
    const DisplayNote* horizontalNote;  // note, if any, used to determine wheel value
    const DisplayNote* verticalNote;
    int horizontalValue;
    int verticalValue;

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
            if (!horizontalNote && test.isNoteOrRest()) {
                horizontalNote = &test;
                horizontalValue = NoteDurations::FromMidi(test.duration, n.ppq);
            }
            if (!verticalNote && NOTE_ON == test.type) {
                verticalNote = &test;
                verticalValue = test.pitch();
                break;
            }
        }
    }
};
