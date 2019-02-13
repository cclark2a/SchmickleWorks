#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

void EditButton::onDragStart(EventDragStart &e) {
    NoteTaker* nt = nModule();
    nt->runButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void InsertButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    unsigned insertLoc;
    unsigned shiftLoc;
    selectButton->ledOn = false;
    unsigned insertSize;
    int shiftTime;
    if (nt->isEmpty()) {
    // todo: still need to figure out duration of note vs time of NOTE_OFF in midi
        insertLoc = nt->allNotes.size() - 1;
        assert(TRACK_END == nt->allNotes[insertLoc].type);
        DisplayNote midC = { 0, stdTimePerQuarterNote / 2,
                { 60, 0, stdKeyPressure, stdKeyPressure}, 0, NOTE_ON  };
        midC.setNote(NoteTakerDisplay::DurationIndex(stdTimePerQuarterNote));
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, midC);
        insertSize = 1;
        shiftTime = stdTimePerQuarterNote;
        shiftLoc = insertLoc + 1;
    } else {
        unsigned iStart, iEnd;
        if (SelectButton::copy_Select == selectButton->state) { // paste part of copy n paste
            iStart = selectButton->rangeStart;
            iEnd = selectButton->rangeEnd;
            insertLoc = nt->selectStart;
            shiftLoc = nt->selectStart + (iEnd - iStart);
        } else if (!nt->selectStart) {  // insert left of first note
            iStart = nt->nthNoteIndex(0);
            iEnd = nt->selectEnd;
            insertLoc = iStart;
            shiftLoc = nt->selectEnd;
        } else { // duplicate selection
            iStart = nt->selectStart;
            iEnd = nt->selectEnd;
            insertLoc = nt->selectEnd;
            shiftLoc = nt->selectEnd;
        }
        vector<DisplayNote> span{ nt->allNotes.begin() + iStart, nt->allNotes.begin() + iEnd };
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, span.begin(), span.end());
        insertSize = span.size();
        shiftTime = span.back().startTime - span.front().startTime + span.back().duration * 2;
        debug("iStart=%u iEnd=%u insertLoc=%u insertSize=%u shiftLoc=%d shiftTime=%d",
                iStart, iEnd, insertLoc, insertSize, shiftLoc, shiftTime);
        nt->debugDump();
    }
    nt->shiftNotes(shiftLoc, shiftTime);
    nt->selectStart = insertLoc;
    nt->selectEnd = insertLoc + insertSize;
    nt->setWheelRange();  // range is larger
    nt->setDisplayEnd();
    nt->playSelection();
    NoteTakerButton::onDragEnd(e);
}

void PartButton::draw(NVGcontext *vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 32 - af, "q", NULL);
    nvgText(vg, 8 + af, 38 - af, "q", NULL);
    nvgText(vg, 8 + af, 44 - af, "q", NULL);
}

void RestButton::draw(NVGcontext *vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 48);
    nvgText(vg, 8 + af, 46 - af, "Q", NULL);
}

void SelectButton::draw(NVGcontext *vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "\u00E0", NULL);
}

void SelectButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    if (!ledOn) {  // will be on
        nt->insertButton->ledOn = false;
        state = off_Select == state && rangeStart == nt->selectStart
                && rangeEnd == nt->selectEnd
                ? copy_Select : single_Select;
    } else if (single_Select == state || copy_Select == state) {    // will be off
        state = extend_Select;
        ledOn = false; // keep the led on
        af = 1;
    } else if (extend_Select == state) {
        state = off_Select;
        rangeStart = nt->selectStart;
        rangeEnd = nt->selectEnd;
    }
    NoteTakerButton::onDragEnd(e);
    if (!nt->selectStart && !this->editStart()) {
        nt->selectStart = nt->nthNoteIndex(0);
    }
    nt->setWheelRange();  // (after led is updated) if state is single, set to horz from -1 to size
}

void CutButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isEmpty() || !nt->selectStart) {
        return;
    }
    SelectButton* selectButton = nt->selectButton;
    if (selectButton->ledOn) {
        nt->copyNotes();
    }
    unsigned start = nt->selectStart;
    unsigned end = nt->selectEnd;
    int shiftTime = nt->allNotes[start].startTime - nt->allNotes[end].startTime;
    nt->eraseNotes(start, end);
    nt->shiftNotes(start, shiftTime);
    nt->selectStart = std::min(start, nt->horizontalCount());
    nt->selectEnd = nt->selectStart + 1;
    selectButton->state = SelectButton::single_Select;    // set so paste point can be chosen
    nt->setWheelRange();  // range is smaller
    nt->setDisplayEnd();
    NoteTakerButton::onDragEnd(e);
}

void RunButton::onDragEnd(EventDragEnd &e) {
    if (ledOn) {    // will be off
        nModule()->zeroGates();
    }
    NoteTakerButton::onDragEnd(e);
}

void DumpButton::onDragEnd(EventDragEnd& e) {
    nModule()->debugDump();
    NoteTakerButton::onDragEnd(e);
}
