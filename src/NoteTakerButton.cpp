#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

void EditButton::onDragStart(EventDragStart &e) {
    NoteTaker* nt = nModule();
    nt->runButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void InsertButton::draw(NVGcontext *vg) {
    EditButton::draw(vg);
#if 0
    // replace this with a font character if one can be found
    nvgTranslate(vg, af, 6 - af);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 6, 20);
    nvgLineTo(vg, 8, 20);
    nvgArcTo(vg, 9, 20, 9, 21, 1);
    nvgLineTo(vg, 9, 32);
    nvgArcTo(vg, 9, 33, 8, 33, 1);
    nvgLineTo(vg, 6, 33);
    nvgMoveTo(vg, 12, 20);
    nvgLineTo(vg, 10, 20);
    nvgArcTo(vg, 9, 20, 9, 21, 1);
    nvgMoveTo(vg, 12, 33);
    nvgLineTo(vg, 10, 33);
    nvgArcTo(vg, 9, 33, 9, 32, 1);
    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
    nvgStrokeWidth(vg, .5);
    nvgStroke(vg);
#else
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
#endif
}

void InsertButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    unsigned insertLoc;
    unsigned insertSize;
    int shiftTime;
    if (nt->isEmpty()) {
    // to do: still need to figure out duration of note vs time of NOTE_OFF in midi
        insertLoc = nt->allNotes.size() - 1;
        assert(TRACK_END == nt->allNotes[insertLoc].type);
        DisplayNote midC = { 0, stdTimePerQuarterNote,
                { 60, 0, stdKeyPressure, stdKeyPressure}, 0, NOTE_ON  };
        midC.setNote(NoteTakerDisplay::DurationIndex(stdTimePerQuarterNote));
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, midC);
        insertSize = 1;
        shiftTime = stdTimePerQuarterNote;
        debug("add to empty");
    } else {
        vector<DisplayNote>* copyFrom = nullptr;
        vector<DisplayNote> span;
        insertLoc = !nt->selectStart ? nt->wheelToNote(0) : nt->selectEnd;
        if (selectButton->editStart() && !nt->clipboard.empty()) { // paste part of copy n paste
            copyFrom = &nt->clipboard;
            debug("paste from clipboard"); NoteTaker::DebugDump(nt->clipboard);
        } else {
            copyFrom = &span;
            unsigned iStart = !nt->selectStart ? insertLoc : nt->selectStart;
            unsigned iEnd = nt->selectEnd;
            !nt->selectStart ? debug("left of first note") : debug("duplicate selection");
            debug("iStart=%u iEnd=%u", iStart, iEnd);
            span.assign(nt->allNotes.begin() + iStart, nt->allNotes.begin() + iEnd);
        }
        NoteTaker::ShiftNotes(*copyFrom, 0, nt->allNotes[insertLoc].startTime
                - copyFrom->front().startTime);
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, copyFrom->begin(),
                copyFrom->end());
        insertSize = copyFrom->size();
        shiftTime = copyFrom->back().startTime - copyFrom->front().startTime
                + copyFrom->back().duration * 2;
        debug("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, nt->selectStart, nt->selectEnd);
        nt->debugDump();
    }
    selectButton->reset();
    nt->selectStart = insertLoc;
    nt->selectEnd = insertLoc + insertSize;
    NoteTaker::ShiftNotes(nt->allNotes, nt->selectEnd, shiftTime);
    debug("insert final"); nt->debugDump();
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
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        if (nt->selectButton->ledOn) {
            nt->selectChannels = ALL_CHANNELS;
        }
    }
    nt->setWheelRange();  // range is larger
}

void AdderButton::onDragEndPreamble(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    startTime = 0;
    // to do : salt this away so it can be shared with time signature
    if (nt->isEmpty()) {
        insertLoc = nt->allNotes.size() - 1;
        assert(TRACK_END == nt->allNotes[insertLoc].type);
        shiftLoc = insertLoc + 1;
    } else if (!nt->selectStart) {  // insert left of first note
        assert(selectButton->editStart());
        insertLoc = nt->wheelToNote(0);
        shiftLoc = nt->selectEnd;
    } else {
        duration = nt->allNotes[nt->selectEnd].startTime - nt->allNotes[nt->selectStart].startTime;
        if (selectButton->editStart()) { // insert right of selection
            insertLoc = nt->selectEnd;
            shiftLoc = nt->selectEnd;
            shiftTime = duration;
            startTime = nt->allNotes[nt->selectEnd].startTime;
        } else {  // replace selection with one rest
            startTime = nt->allNotes[nt->selectStart].startTime;
            nt->eraseNotes(nt->selectStart, nt->selectEnd);
            insertLoc = nt->selectStart;
            shiftLoc = insertLoc + 1;
            shiftTime = 0;
        }
    }
}

void AdderButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (shiftTime) {
        NoteTaker::ShiftNotes(nt->allNotes, shiftLoc, shiftTime);
    }
    nt->selectButton->reset();
    nt->selectStart = insertLoc;
    nt->selectEnd = insertLoc + 1;
    nt->setWheelRange();  // range is larger
    nt->setDisplayEnd();
    NoteTakerButton::onDragEnd(e);
    nt->debugDump();
}

void RestButton::draw(NVGcontext *vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(EventDragEnd& e) {
    shiftTime = duration = stdTimePerQuarterNote;
    onDragEndPreamble(e);
    NoteTaker* nt = nModule();
    DisplayNote rest = { startTime, duration, { 0, 0, 0, 0}, 0, REST_TYPE };
    rest.setRest(NoteTakerDisplay::RestIndex(duration));
    nt->allNotes.insert(nt->allNotes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void SelectButton::draw(NVGcontext *vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(EventDragEnd &e) {
    NoteTakerButton::onDragEnd(e);
    NoteTaker* nt = nModule();
    switch (state) {
        case State::ledOff: {
            assert(ledOn);
            int wheelIndex = nt->noteToWheel(nt->selectStart);
            state = State::single;
            nt->selectStart = nt->wheelToNote(wheelIndex - 1);
            nt->selectEnd = nt->wheelToNote(wheelIndex);
        } break;
        case State::single: {
            assert(!ledOn);
            int wheelIndex = nt->noteToWheel(nt->selectStart);
            state = State::extend;
            af = 1;
            ledOn = true;
            nt->selectStart = nt->wheelToNote(wheelIndex + 1);
            singlePos = nt->selectStart;
            if (TRACK_END == nt->allNotes[nt->selectStart].type) {
                nt->selectEnd = nt->selectStart;
                nt->selectStart = nt->isEmpty() ? 0 : nt->wheelToNote(wheelIndex);
            } else {
                nt->selectEnd = nt->wheelToNote(wheelIndex + 2);
            }
        } break;
        case State::extend:
            assert(!ledOn);
            nt->copyNotes();
            this->reset();
        break;
        default:
            assert(0);
    }
    nt->setWheelRange();  // if state is single, set to horz from -1 to size
}

void SelectButton::reset() {
    NoteTaker* nt = nModule();
    if (!nt->selectStart && !nt->isEmpty()) {
        nt->selectStart = nt->wheelToNote(0);
    }
    state = State::ledOff;
    NoteTakerButton::reset();
}

void SelectButton::setExtend() {
    NoteTaker* nt = nModule();
    if (!nt->selectStart && !nt->isEmpty()) {
        nt->selectStart = nt->wheelToNote(0);
    }
    af = 1;
    ledOn = true;
    state = State::extend;
}

void CutButton::draw(NVGcontext* vg) {
        EditButton::draw(vg);
    #if 0
        nvgTranslate(vg, af + 9, 27 - af);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, -1);
        nvgLineTo(vg, 2, -3);   
        nvgLineTo(vg, 4, -3);
        nvgLineTo(vg, 1,  0);
        nvgLineTo(vg, 4,  3);
        nvgLineTo(vg, 2,  3);
        nvgLineTo(vg, 0,  1);
        nvgLineTo(vg, -2,  3);
        nvgLineTo(vg,  -4,  3);
        nvgLineTo(vg, -1,  0);
        nvgLineTo(vg, -4, -3);
        nvgLineTo(vg, -2, -3);
        nvgLineTo(vg, 0, -1);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
    #else
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
    #endif
}

// to do : should a long press clear all?
void CutButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isEmpty() || !nt->selectStart) {
        return;
    }
    SelectButton* selectButton = nt->selectButton;
    if (selectButton->editEnd()) {
        nt->copyNotes();
    }
    selectButton->setSingle();
    unsigned start = nt->selectStart;
    unsigned end = nt->selectEnd;
    int shiftTime = nt->allNotes[start].startTime - nt->allNotes[end].startTime;
    nt->eraseNotes(start, end);
    NoteTaker::ShiftNotes(nt->allNotes, start, shiftTime);
    int wheelIndex = nt->noteToWheel(start);
    nt->selectStart = nt->wheelToNote(wheelIndex - 1);
    nt->selectEnd = start;
    nt->setWheelRange();  // range is smaller
    nt->setDisplayEnd();
    NoteTakerButton::onDragEnd(e);
}

void FileButton::onDragEnd(EventDragEnd &e) {
    NoteTakerButton::onDragEnd(e);
    nModule()->setWheelRange();
}

void FileButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}

void SustainButton::onDragEnd(EventDragEnd &e) {
    NoteTakerButton::onDragEnd(e);
    nModule()->setWheelRange();

}

void SustainButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

// insert time signature
void TimeButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    // to do : insert one note ala rest
    DisplayNote timeSignature = { startTime, 0, { 4, 4, 0, 0}, 0, TIME_SIGNATURE };
    nt->allNotes.insert(nt->allNotes.begin() + insertLoc, timeSignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TimeButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 33 - af, "4", NULL);
    nvgText(vg, 8 + af, 41 - af, "4", NULL);
}

void RunButton::onDragEnd(EventDragEnd &e) {
    if (ledOn) {    // will be off
        nModule()->zeroGates();
    }
    NoteTakerButton::onDragEnd(e);
}

// todo : remove below
void DumpButton::onDragEnd(EventDragEnd& e) {
    nModule()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

extern void UnitTest(NoteTaker* );
