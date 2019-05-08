#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

// try to get rest working as well as note
void AdderButton::onDragEndPreamble(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    // insertLoc, shiftTime set by caller
    shiftLoc = insertLoc + 1;
    startTime = nt->notes[insertLoc].startTime;
    if (nt->debugVerbose) debug("insertLoc %u shiftLoc %u startTime %d", insertLoc, shiftLoc, startTime);
}

void AdderButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (shiftTime) {
        NoteTaker::ShiftNotes(nt->notes, shiftLoc, shiftTime);
    }
    NoteTaker::Sort(nt->notes);
    nt->selectButton->setOff();
    NoteTakerButton::onDragEnd(e);
    nt->display->invalidateCache();
    nt->setSelect(insertLoc, insertLoc + 1);
    nt->turnOffLedButtons();
    nt->setWheelRange();  // range is larger
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
    if (nt->isRunning()) {
        return;
    }
    nt->clipboardInvalid = true;
    NoteTakerButton::onDragEnd(e);
    if (nt->fileButton->ledOn) {
        nt->selectStart = 0;
        nt->selectEnd = nt->notes.size() - 1;
        nt->copyNotes();  // allows add to undo accidental cut / clear all
        nt->setScoreEmpty();
        nt->display->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        nt->setWheelRange();
        return;
    }
    if (nt->partButton->ledOn) {
        nt->selectStart = 0;
        nt->selectEnd = nt->notes.size() - 1;
        nt->copySelectableNotes();
        nt->setSelectableScoreEmpty();
        nt->display->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        nt->setWheelRange();
        return;
    }
    SelectButton* selectButton = nt->selectButton;
    if (selectButton->editStart() && selectButton->saveZero) {
        return;
    }
    unsigned start = nt->selectStart;
    unsigned end = nt->selectEnd;
    if (!start || end <= 1) {
        debug("*** selectButton should have been set to edit start, save zero");
        return;
    }
    if (selectButton->editEnd()) {
        nt->copyNotes();
    }
    int shiftTime = nt->notes[start].startTime - nt->notes[end].startTime;
    nt->eraseNotes(start, end);
    nt->shiftNotes(start, shiftTime);
    nt->display->invalidateCache();
    nt->turnOffLedButtons();
    // set selection to previous selectable note, or zero if none
    int wheel = nt->noteToWheel(start);
    unsigned previous = nt->wheelToNote(std::max(0, wheel - 1));
    nt->setSelect(previous, previous < start ? start : previous + 1);
    selectButton->setSingle();
    nt->setWheelRange();  // range is smaller
}

// hidden
void DumpButton::onDragEnd(EventDragEnd& e) {
    nModule()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

void EditButton::onDragStart(EventDragStart& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    nt->runButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void FileButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    nt->turnOffLedButtons(this);
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
    if (nt->isRunning()) {
        return;
    }
    nt->clipboardInvalid = true;
    SelectButton* selectButton = nt->selectButton;
    nt->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    unsigned insertLoc;
    unsigned insertSize;
    int shiftTime;
    if (!nt->noteCount() && nt->clipboard.empty()) {
        insertLoc = nt->atMidiTime(0);
        DisplayNote midC = { nullptr, 0, nt->ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                (uint8_t) nt->unlockedChannel(), NOTE_ON, false };
        nt->notes.insert(nt->notes.begin() + insertLoc, midC);
        insertSize = 1;
        shiftTime = nt->ppq;
        if (nt->debugVerbose) debug("add to empty");
    } else {
        vector<DisplayNote> span;
        // Insert loc is where the new note goes, but not when the new note goes; 
        //   the new start time is 'last end time(insert loc)'
        unsigned iStart;
        unsigned iEnd = nt->selectEnd;
        int lastEndTime = 0;
        if (!nt->selectStart) {
            iStart = insertLoc = nt->wheelToNote(1);
        } else {
            insertLoc = nt->selectEnd;
            iStart = nt->selectStart;
        // A prior edit (e.g., changing a note duration) may invalidate using select end as the
        //   insert location. Use select end to determine the last active note to insert after,
        //   but not the location to insert after.
            for (unsigned index = 0; index < iEnd; ++index) {
                const auto& note = nt->notes[index];
                if (nt->isSelectable(note)) {
                    lastEndTime = std::max(lastEndTime, note.endTime());
                    insertLoc = nt->selectEnd;
                }
                if (note.isNoteOrRest() && note.startTime >= lastEndTime && index < insertLoc) {
                    insertLoc = index;
                }
            }
            debug("lastEndTime %d insertLoc %u", lastEndTime, insertLoc);
        }
        // insertLoc may be different channel, so can't use that start time by itself
        // shift to selectStart time, but not less than previous end (if any) on same channel
        int insertTime = nt->notes[insertLoc].startTime;
        while (insertTime < lastEndTime) {
            insertTime = nt->notes[++insertLoc].startTime;
        }
        debug("insertTime %d insertLoc %u clipboard size %u", insertTime, insertLoc,
                nt->clipboard.size());
        if (!selectButton->editStart() || nt->clipboard.empty() || !nt->extractClipboard(&span)) {
            !nt->selectStart ? debug("left of first note") : debug("duplicate selection");
            debug("iStart=%u iEnd=%u", iStart, iEnd);
            for (unsigned index = iStart; index < iEnd; ++index) {
                const auto& note = nt->notes[index];
                if (nt->isSelectable(note)) {
                    span.push_back(note);
                }
            }
            nt->clipboard.clear();
        }
        if (span.empty() || (1 == span.size() && NOTE_ON != span[0].type) ||
                (span[0].isSignature() && nt->notes[insertLoc].isSignature())) {
            span.clear();
            if (nt->debugVerbose) {
                debug("insert button : none selectable"); nt->debugDump();
            }
            for (unsigned index = iStart; index < nt->notes.size(); ++index) {
                const auto& note = nt->notes[index];
                if (NOTE_ON == note.type && nt->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            for (unsigned index = iStart; --index > 0; ) {
                const auto& note = nt->notes[index];
                if (NOTE_ON == note.type && nt->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            DisplayNote midC = { nullptr, 0, nt->ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                    (uint8_t) nt->unlockedChannel(), NOTE_ON, false };
            span.push_back(midC);
        }
        int nextStart = nt->nextStartTime(insertLoc);
        NoteTaker::ShiftNotes(span, 0, lastEndTime - span.front().startTime);
        nt->notes.insert(nt->notes.begin() + insertLoc, span.begin(), span.end());
        insertSize = span.size();
        // include notes on other channels that fit within the start/end window
        // shift by span duration less next start (if any) on same channel minus selectStart time
        shiftTime = (lastEndTime - insertTime)
                + (NoteTaker::LastEndTime(span) - span.front().startTime);
        int availableShiftTime = nextStart - insertTime;
        if (nt->debugVerbose) debug("shift time %d available %d", shiftTime, availableShiftTime);
        shiftTime = std::max(0, shiftTime - availableShiftTime);
        if (nt->debugVerbose) debug("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, nt->selectStart, nt->selectEnd);
        nt->display->invalidateCache();
        if (nt->debugVerbose) nt->debugDump(false);
    }
    selectButton->setOff();
    nt->insertFinal(shiftTime, insertLoc, insertSize);
    NoteTakerButton::onDragEnd(e);
}

// insert key signature
void KeyButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    insertLoc = nt->atMidiTime(nt->notes[nt->selectEnd].startTime);
    if (nt->insertContains(insertLoc, KEY_SIGNATURE)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote keySignature = { nullptr, startTime, 0, {0, 0, 0, 0}, 255, KEY_SIGNATURE, false };
    nt->notes.insert(nt->notes.begin() + insertLoc, keySignature);
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
    if (nt->isRunning()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        this->onTurnOff();
    } else if (nt->selectButton->editEnd()) {
        nt->clipboardInvalid = true;
        nt->copySelectableNotes();
    }
    if (nt->debugVerbose) debug("part button onDragEnd ledOn %d part %d selectChannels %d unlocked %u",
            ledOn, nt->horizontalWheel->part(), nt->selectChannels, nt->unlockedChannel());
    nt->turnOffLedButtons(this);
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
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    nt->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    if (!nt->selectButton->editStart()) {
        EventDragEnd e;
        nt->cutButton->onDragEnd(e);
        if (nt->debugVerbose) nt->debugDump();
    }
    insertLoc = nt->atMidiTime(nt->notes[nt->selectEnd].startTime);
    onDragEndPreamble(e);
    DisplayNote rest = { nullptr, startTime, nt->ppq, { 0, 0, 0, 0},
            (uint8_t) nt->unlockedChannel(), REST_TYPE, false };
    shiftTime = rest.duration;
    nt->notes.insert(nt->notes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void RunButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    nt->debugMidiCount = 0;
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        nt->zeroGates();
    } else {
        nt->resetRun();
        nt->display->setRange();
        unsigned next = nt->nextAfter(nt->selectStart, 1);
        nt->setSelectStart(next < nt->notes.size() - 1 ? next : 
                nt->selectButton->editEnd() ? 1 : 0);
        nt->horizontalWheel->lastRealValue = INT_MAX;
        nt->verticalWheel->lastValue = INT_MAX;
        nt->playSelection();
    }
    if (!nt->menuButtonOn()) {
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
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    switch (state) {
        case State::ledOff: {
            assert(ledOn);
            state = State::single; // does not call setSingle because saveZero should not change
            unsigned start = saveZero ? nt->wheelToNote(0) : nt->selectStart;
            nt->setSelect(start, nt->nextAfter(start, 1));
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
            assert(MIDI_HEADER != nt->notes[selStart].type);
            assert(TRACK_END != nt->notes[selStart].type);
            const auto& note = nt->notes[selStart];
            unsigned end = note.isSignature() ? selStart + 1 : nt->atMidiTime(note.endTime());
            nt->setSelect(selStart, end);
        } break;
        case State::extend:
            assert(!ledOn);
            nt->copySelectableNotes();
            state = State::ledOff; // does not call setOff because saveZero should not change
        break;
        default:
            assert(0);
    }
    nt->turnOffLedButtons(this);
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
    if (nt->isRunning()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    nt->turnOffLedButtons(this);
    nt->setWheelRange();
}

void SustainButton::draw(NVGcontext* vg) {
    EditLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

void TempoButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    insertLoc = nt->atMidiTime(nt->notes[nt->selectEnd].startTime);
    if (nt->insertContains(insertLoc, MIDI_TEMPO)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote tempo = { nullptr, startTime, 0, {500000, 0, 0, 0}, 255, MIDI_TEMPO, false };
    nt->notes.insert(nt->notes.begin() + insertLoc, tempo);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TempoButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, "@", NULL);
}

void TieButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    nt->turnOffLedButtons(this);
    nt->setWheelRange();
}

void TieButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 41 - af, ">", NULL);
}

// insert time signature
void TimeButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
    insertLoc = nt->atMidiTime(nt->notes[nt->selectEnd].startTime);
    if (nt->insertContains(insertLoc, TIME_SIGNATURE)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote timeSignature = { nullptr, startTime, 0, {4, 2, 24, 8}, 255, TIME_SIGNATURE, false };
    nt->notes.insert(nt->notes.begin() + insertLoc, timeSignature);
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

void TrillButton::onDragEnd(EventDragEnd& e) {
    NoteTaker* nt = nModule();
    if (nt->isRunning()) {
        return;
    }
   AdderButton::onDragEnd(e);
}

void TrillButton::draw(NVGcontext* vg) {
    EditButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8.5 + af, 41 - af, "?", NULL);
}
