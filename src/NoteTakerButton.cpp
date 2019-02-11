#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

void EditButton::onDragStart(EventDragStart &e) {
    NoteTaker* nt = nModule();
    nt->runEnterButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void InsertButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    if (!ledOn) {   // will be on
        unsigned insertLoc;
        selectButton->ledOn = false;
        unsigned insertSize;
        if (nt->isEmpty()) {
        // todo: still need to figure out duration of note vs time of NOTE_OFF
            insertLoc = nt->allNotes.size() - 1;
            assert(TRACK_END == nt->allNotes[insertLoc].type);
            DisplayNote midC = { 0, stdTimePerQuarterNote / 2,
                    { 60, 0, stdKeyPressure, stdKeyPressure}, 0, NOTE_ON  };
            midC.setNote(stdTimePerQuarterNote);
            nt->allNotes.insert(nt->allNotes.begin() + insertLoc, midC);
            midC.type = NOTE_OFF;
            midC.startTime = stdTimePerQuarterNote;
            nt->allNotes.insert(nt->allNotes.end() - 1, midC);
            insertSize = 2;
        } else {
            unsigned iStart, iEnd;
            if (selectButton->ledOn) {
                iStart = selectButton->rangeStart;
                iEnd = selectButton->rangeEnd;
                insertLoc = nt->selectStart;
            } else {
                iStart = nt->selectStart;
                iEnd = nt->selectEnd;
                insertLoc = nt->selectEnd;
            }
            vector<DisplayNote> span{ nt->allNotes.begin() + iStart,
                    nt->allNotes.begin() + iEnd };
            nt->allNotes.insert(nt->allNotes.begin() + insertLoc,
                     span.begin(), span.end());
            insertSize = span.size();
        }
        nt->selectStart = insertStart = insertLoc;
        nt->selectEnd = insertEnd = insertLoc + insertSize;
        nt->setWheelRange();  // range is larger
    }
    NoteTakerButton::onDragEnd(e);
}

void PartButton::draw(NVGcontext *vg) {
    EditButton::draw(vg);
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
        runEnterCount = 0;
    } else {    // select off
        nt->selectStart = rangeStart;
        nt->selectEnd = rangeEnd;

    }
    nt->setWheelRange();  // if on, set to horz from -1 to size
    NoteTakerButton::onDragEnd(e);
}

void CutButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    if (selectButton->ledOn) {
        nt->copyNotes();
    }
    nt->eraseNotes();
    if (selectButton->editEnd()) {
        nt->selectButton->runEnterCount = 0;  // set to no selection so paste point can be chosen
    }
    NoteTakerButton::onDragEnd(e);
}

bool RunEnterButton::enterMode() const {
    const NoteTaker* nt = nModule();
    return nt->insertButton->ledOn || nt->selectButton->ledOn;
}

void RunEnterButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    if (this->enterMode()) {
        InsertButton* insertButton = nt->insertButton;
        SelectButton* selectButton = nt->selectButton;
        if (insertButton->ledOn) {
            assert(!selectButton->ledOn);
            unsigned iStart = insertButton->insertStart;
            vector<DisplayNote> span{nt->allNotes.begin() + iStart,
                                        nt->allNotes.begin() + insertButton->insertEnd };
            nt->allNotes.insert(nt->allNotes.begin() + iStart + span.size(),
                    span.begin(), span.end());
            insertButton->insertStart = iStart + span.size();
            insertButton->insertEnd = insertButton->insertStart + span.size();
            nt->setWheelRange();
        } else if (selectButton->ledOn) {
            if (!selectButton->runEnterCount) {
                selectButton->rangeStart = nt->selectStart;
                selectButton->rangeEnd = nt->selectStart + 1;
            } else {
                selectButton->rangeEnd = nt->selectEnd;
            }
            selectButton->runEnterCount++;
            nt->setWheelRange();
        }
    } else {
        if (ledOn) {    // will be off
            nModule()->zeroGates();
        }
    }
    NoteTakerButton::onDragEnd(e);
}