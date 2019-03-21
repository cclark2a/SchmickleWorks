#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

// try to get rest working as well as note
void AdderButton::onDragEndPreamble(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (!nt->selectButton->editStart()) {
        EventDragEnd e;
        nt->cutButton->onDragEnd(e);
    }
    // shiftTime set by caller
    insertLoc = nt->atMidiTime(nt->allNotes[nt->selectEnd].startTime);
    shiftLoc = insertLoc + 1;
    startTime = nt->allNotes[insertLoc].startTime;
    debug("insertLoc %u shiftLoc %u startTime %d", insertLoc, shiftLoc, startTime);
}

void AdderButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (shiftTime) {
        NoteTaker::ShiftNotes(nt->allNotes, shiftLoc, shiftTime);
    }
    nt->selectButton->setOff();
    NoteTakerButton::onDragEnd(e);
    nt->display->xPositionsInvalid = true;
    nt->setSelect(insertLoc, insertLoc + 1);
    nt->resetLedButtons();
    nt->setWheelRange();  // range is larger
    nt->debugDump();
}

void CutButton::draw(NVGcontext* vg) {
        EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
}

void CutButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    SelectButton* selectButton = nt->selectButton;
    if (selectButton->editStart() && selectButton->saveZero) {
        return;
    }
    FileButton* fileButton = nt->fileButton;
    if (selectButton->editEnd() || fileButton->ledOn) {
        // allows add to undo accidental cut / clear all
        nt->copyNotes();
    }
    if (fileButton->ledOn) {
        nt->setScoreEmpty();
        return;
    }
    unsigned start = nt->selectStart;
    unsigned end = nt->selectEnd;
    int shiftTime = nt->allNotes[start].startTime - nt->allNotes[end].startTime;
    nt->eraseNotes(start, end);
    nt->shiftNotes(start, shiftTime);
    nt->display->xPositionsInvalid = true;
    nt->resetLedButtons();
    // set selection to previous selectable note, or zero if none
    int wheel = nt->noteToWheel(start);
    unsigned previous = nt->wheelToNote(wheel - 1);
    nt->selectEnd = start;
    nt->selectStart = previous;
    selectButton->setSingle();
    nt->setWheelRange();  // range is smaller
}

// to do : remove once button needs another use
void DumpButton::onDragEnd(EventDragEnd& e) {
    nModule()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

void EditButton::onDragStart(EventDragStart& e) {
    NoteTaker* nt = nModule();
    nt->runButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void FileButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    nt->resetLedButtons(this);
    nt->setWheelRange();
}

void FileButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}

void InsertButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
}

void InsertButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    SelectButton* selectButton = nt->selectButton;
    nt->resetLedButtons();  // turn off pitch, file, sustain
    unsigned insertLoc;
    unsigned insertSize;
    int shiftTime;
    if (nt->isEmpty()) {
        insertLoc = nt->atMidiTime(0);
        DisplayNote midC = { 0, stdTimePerQuarterNote, { 60, 0, stdKeyPressure, stdKeyPressure},
                nt->partButton->addChannel, NOTE_ON, false };
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, midC);
        insertSize = 1;
        shiftTime = stdTimePerQuarterNote;
        debug("add to empty");
    } else {
        vector<DisplayNote> span;
        // insert loc is where the new note goes, but not when the new note goes; 
        // the new start time is 'last end time(insert loc)'
        insertLoc = !nt->selectStart ? nt->wheelToNote(1) : nt->selectEnd;
        if (!selectButton->editStart() || nt->clipboard.empty() || !nt->extractClipboard(&span)) {
            unsigned iStart = !nt->selectStart ? insertLoc : nt->selectStart;
            unsigned iEnd = nt->selectEnd;
            !nt->selectStart ? debug("left of first note") : debug("duplicate selection");
            debug("iStart=%u iEnd=%u", iStart, iEnd);
            for (unsigned index = iStart; index < iEnd; ++index) {
                const auto& note = nt->allNotes[index];
                if (nt->isSelectable(note)) {
                    span.push_back(note);
                }
            }
            bool onlyRest = 1 == span.size() && REST_TYPE == span[0].type;
            if (span.empty() || onlyRest) {
                span.clear();
                debug("insert button : none selectable"); nt->debugDump();
                for (unsigned index = iStart; index < nt->allNotes.size(); ++index) {
                    const auto& note = nt->allNotes[index];
                    if (NOTE_ON == note.type) {
                        span.push_back(note);
                        break;
                    }
                }
                if (span.empty() || onlyRest) {
                    for (unsigned index = iStart; --index > 0; ) {
                        const auto& note = nt->allNotes[index];
                        if (NOTE_ON == note.type) {
                            span.push_back(note);
                            break;
                        }
                    }
                }
                assert(!span.empty());
            }
        }
        // insertLoc may be different channel, so can't use that start time by itself
        // shift to selectStart time, but not less than previous end (if any) on same channel
        int priorEnd = nt->lastEndTime(insertLoc);
        int insertTime = nt->allNotes[insertLoc].startTime;
        int nextStart = nt->nextStartTime(insertLoc);
        debug("priorEnd %d insertTime %d nextStart %d", priorEnd, insertTime, nextStart);
        NoteTaker::ShiftNotes(span, 0, priorEnd - span.front().startTime);
        nt->allNotes.insert(nt->allNotes.begin() + insertLoc, span.begin(), span.end());
        insertSize = span.size();
        // include notes on other channels that fit within the start/end window
        // shift by span duration less next start (if any) on same channel minus selectStart time
        shiftTime = (priorEnd - insertTime)
                + (NoteTaker::LastEndTime(span) - span.front().startTime);
        int availableShiftTime = nextStart - insertTime;
        debug("shift time %d available %d", shiftTime, availableShiftTime);
        shiftTime = std::max(0, shiftTime - availableShiftTime);
        debug("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, nt->selectStart, nt->selectEnd);
        nt->debugDump(false);
    }
    if (shiftTime) {
        nt->shiftNotes(insertLoc + insertSize, shiftTime);
    }
    nt->display->xPositionsInvalid = true;
    nt->setSelect(insertLoc, nt->nextAfter(insertLoc + insertSize));
    selectButton->setOff();
    debug("insert final");
    nt->setWheelRange();  // range is larger
    nt->playSelection();
    NoteTakerButton::onDragEnd(e);
    nt->debugDump(true);
}

// insert key signature
void KeyButton::onDragEnd(EventDragEnd& e) {
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

void PartButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        if (!allChannels) {
            // allChannels, addChannel updated interactively so it is visible in UI
            nt->selectChannels |= 1 << addChannel;
        }
    }
    debug("part button onDragEnd ledOn %d part %d allChannels %d addChannel %u",
            ledOn, nt->horizontalWheel->part(), allChannels, addChannel);
    nt->resetLedButtons(this);
    nt->setWheelRange();  // range is larger
}

void RestButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(EventDragEnd& e) {
    onDragEndPreamble(e);
    NoteTaker* nt = nModule();
    DisplayNote rest = { startTime, stdTimePerQuarterNote, { 0, 0, 0, 0},
            nt->partButton->addChannel, REST_TYPE, false };
    shiftTime = rest.duration;
    nt->allNotes.insert(nt->allNotes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void RunButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        nt->zeroGates();
    } else {
        nt->resetRun();
    }
    if (!nt->fileButton->ledOn && !nt->partButton->ledOn && !nt->sustainButton->ledOn) {
        nt->setWheelRange();
    }
}

void SelectButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(EventDragEnd& e) {
    NoteTakerButton::onDragEnd(e);
    NoteTaker* nt = nModule();
    switch (state) {
        case State::ledOff: {
            assert(ledOn);
            state = State::single; // does not call setSingle because saveZero should not change
            unsigned start = saveZero ? nt->wheelToNote(0) : nt->selectStart;
            nt->setSelect(start, nt->nextAfter(start + 1));
        } break;
        case State::single: {
            assert(!ledOn);
            af = 1;
            ledOn = true;
            if (!nt->horizontalCount()) {
                break;  // can't start selection if there's nothing to select
            }
            int wheelStart = nt->noteToWheel(nt->selectStart);
            saveZero = !wheelStart;
            state = State::extend;
            int wheelIndex = std::max(1, wheelStart);
            selStart = nt->wheelToNote(wheelIndex);
            assert(MIDI_HEADER != nt->allNotes[selStart].type);
            assert(TRACK_END != nt->allNotes[selStart].type);
            unsigned end = nt->atMidiTime(nt->allNotes[selStart].endTime());
            nt->setSelect(selStart, end);
        } break;
        case State::extend:
            assert(!ledOn);
            nt->copyNotes();
            state = State::ledOff; // does not call setOff because saveZero should not change
        break;
        default:
            assert(0);
    }
    nt->resetLedButtons(this);
    nt->setWheelRange();  // if state is single, set to horz from -1 to size
}

void SelectButton::setExtend() {
    state = State::extend;
    af = 1;
    ledOn = true;
}

void SelectButton::setOff() {
    saveZero = false;
    state = State::ledOff;
    af = 0;
    ledOn = false;
}

void SelectButton::setSingle() {
    NoteTaker* nt = nModule();
    saveZero = !nt->noteToWheel(nt->selectStart);
    state = State::single;
    af = 1;
    ledOn = true;
}

void SustainButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    NoteTakerButton::onDragEnd(e);
    nt->resetLedButtons(this);
    nt->setWheelRange();
}

void SustainButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

// insert time signature
void TimeButton::onDragEnd(EventDragEnd& e) {
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
