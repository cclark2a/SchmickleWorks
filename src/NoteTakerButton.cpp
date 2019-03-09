#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
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
        insertLoc = nt->allNotes.size() - 1;
        assert(TRACK_END == nt->allNotes[insertLoc].type);
        DisplayNote midC = { 0, stdTimePerQuarterNote, { 60, 0, stdKeyPressure, stdKeyPressure},
                (uint8_t) nt->firstChannel(), NOTE_ON, false };
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
            for (unsigned index = iStart; index < iEnd; ++index) {
                const auto& note = nt->allNotes[index];
                if (nt->selectChannels & (1 << note.channel)) {
                    span.push_back(note);
                }
            }
            if (span.empty()) {
                int channel = -1;
                for (unsigned index = iStart; index < iEnd; ++index) {
                    const auto& note = nt->allNotes[index];
                    if (channel < 0 || note.channel == channel) {
                        span.push_back(note);
                        channel = note.channel;
                    }
                }
            }
        }
        NoteTaker::ShiftNotes(*copyFrom, 0, nt->allNotes[insertLoc].startTime
                - copyFrom->front().startTime);
        if (ALL_CHANNELS != nt->selectChannels) {
            NoteTaker::MapChannel(*copyFrom, nt->firstChannel());
        }
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, copyFrom->begin(),
                copyFrom->end());
        insertSize = copyFrom->size();
        // include notes on other channels that fit within the start/end window
        int lastEndTime = copyFrom->back().endTime();
        debug("insertSize=%u lastEndTime=%d", insertSize, lastEndTime);
        do {
            const auto& note = nt->allNotes[insertLoc + insertSize];
            if (NOTE_ON != note.type || note.endTime() > lastEndTime
                    || note.channel == nt->firstChannel()) {
                break;
            }
        } while (++insertSize);
        shiftTime = copyFrom->back().startTime - copyFrom->front().startTime
                + copyFrom->back().duration;
        debug("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, nt->selectStart, nt->selectEnd);
        nt->debugDump(false);
    }
    selectButton->reset();
    nt->shiftNotes(insertLoc + insertSize, shiftTime);
    nt->display->xPositionsInvalid = true;
    nt->setSelect(insertLoc, insertLoc + insertSize);
    debug("insert final"); nt->debugDump();
    nt->setWheelRange();  // range is larger
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
        lastChannels = nt->selectChannels;
        nt->selectChannels = ALL_CHANNELS;
    } else {
        nt->selectChannels = lastChannels;
    }
    nt->setWheelRange();  // range is larger
}

void AdderButton::onDragEndPreamble(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    startTime = 0;
    // shiftTime, duration set by caller
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
            shiftLoc = nt->selectEnd + 1;
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
    debug("insertLoc %u shiftLoc %u startTime %d shiftTime %d duration %d", insertLoc, shiftLoc,
            startTime, shiftTime, duration);
}

void AdderButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (shiftTime) {
        NoteTaker::ShiftNotes(nt->allNotes, shiftLoc, shiftTime);
    }
    nt->selectButton->reset();
    NoteTakerButton::onDragEnd(e);
    nt->display->xPositionsInvalid = true;
    nt->setSelect(insertLoc, insertLoc + 1);
    nt->setWheelRange();  // range is larger
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
    DisplayNote rest = { startTime, duration, { 0, 0, 0, 0}, 0, REST_TYPE, false };
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
            nt->setSelect(nt->wheelToNote(wheelIndex - 1), nt->wheelToNote(wheelIndex));
        } break;
        case State::single: {
            assert(!ledOn);
            int wheelIndex = nt->noteToWheel(nt->selectStart);
            state = State::extend;
            af = 1;
            ledOn = true;
            unsigned start = nt->wheelToNote(wheelIndex + 1);
            unsigned end;
            singlePos = start;
            if (TRACK_END == nt->allNotes[start].type) {
                end = start;
                start = nt->isEmpty() ? 0 : nt->wheelToNote(wheelIndex);
            } else {
                end = nt->wheelToNote(wheelIndex + 2);
            }
            nt->setSelect(start, end);
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
    nModule()->alignStart();
    state = State::ledOff;
    NoteTakerButton::reset();
}

void SelectButton::setExtend() {
    nModule()->alignStart();
    af = 1;
    ledOn = true;
    state = State::extend;
}

void CutButton::draw(NVGcontext* vg) {
        EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
}

// to do : should a long press clear all?
// maybe long press should put clear all: yes / no choice in display
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
    nt->display->xPositionsInvalid = true;
    nt->setSelect(nt->wheelToNote(wheelIndex - 1), start);
    nt->setWheelRange();  // range is smaller
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
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    nt->setWheelRange();
}

void SustainButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

// insert key signature
void KeyButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote keySignature = { startTime, 0, {0, 0, 0, 0}, 255, KEY_SIGNATURE, false };
    nt->allNotes.insert(nt->allNotes.begin() + insertLoc, keySignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void KeyButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 6 + af, 33 - af, "#", NULL);
    nvgText(vg, 10 + af, 41 - af, "$", NULL);
}

// insert time signature
void TimeButton::onDragEnd(EventDragEnd &e) {
    NoteTaker* nt = nModule();
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote timeSignature = { startTime, 0, {4, 2, 24, 8}, 255, TIME_SIGNATURE, false };
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
    NoteTaker* nt = nModule();
    if (ledOn) {    // will be off
        nt->zeroGates();
    }
    NoteTakerButton::onDragEnd(e);
    if (!nt->fileButton->ledOn) {
        nt->setWheelRange();
    }
}

// to do : remove below
void DumpButton::onDragEnd(EventDragEnd& e) {
    nModule()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

extern void UnitTest(NoteTaker* );
