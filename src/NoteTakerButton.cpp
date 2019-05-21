#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"

// try to get rest working as well as note
void AdderButton::onDragEndPreamble(const event::DragEnd& e) {
    NoteTaker* nt = this->ntw()->nt();
    // insertLoc, shiftTime set by caller
    shiftLoc = insertLoc + 1;
    startTime = nt->n.notes[insertLoc].startTime;
    if (nt->debugVerbose) DEBUG("insertLoc %u shiftLoc %u startTime %d", insertLoc, shiftLoc, startTime);
}

void AdderButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    if (shiftTime) {
        NoteTaker::ShiftNotes(nt->n.notes, shiftLoc, shiftTime);
    }
    NoteTaker::Sort(nt->n.notes);
    ntw->selectButton->setOff();
    NoteTakerButton::onDragEnd(e);
    ntw->display->invalidateCache();
    nt->setSelect(insertLoc, insertLoc + 1);
    ntw->turnOffLedButtons();
    ntw->setWheelRange();  // range is larger
}

ButtonBuffer::ButtonBuffer(NoteTakerWidget* _ntw, NoteTakerButton* button) {
    fb = new FramebufferWidget();
    fb->dirty = true;
    this->addChild(fb);
    button->mainWidget = mainWidget = _ntw;
    fb->addChild(button);
}

void CutButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
}

void CutButton::onDragEnd(const rack::event::DragEnd& e) {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    if (ntw->runButton->ledOn) {
        return;
    }
    ntw->clipboardInvalid = true;
    NoteTakerButton::onDragEnd(e);
    SelectButton* selectButton = ntw->selectButton;
    if (ntw->fileButton->ledOn) {
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copyNotes();  // allows add to undo accidental cut / clear all
        nt->setScoreEmpty();
        ntw->display->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        selectButton->setSingle();
        ntw->setWheelRange();
        return;
    }
    if (ntw->partButton->ledOn) {
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copySelectableNotes();
        ntw->setSelectableScoreEmpty();
        ntw->display->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        ntw->setWheelRange();
        return;
    }
    if (selectButton->editStart() && selectButton->saveZero) {
        return;
    }
    unsigned start = n.selectStart;
    unsigned end = n.selectEnd;
    if (!start || end <= 1) {
        DEBUG("*** selectButton should have been set to edit start, save zero");
        assert(0);
        return;
    }
    if (selectButton->editEnd()) {
        ntw->copyNotes();
    }
    int shiftTime = n.notes[start].startTime - n.notes[end].startTime;
    ntw->eraseNotes(start, end);
    ntw->shiftNotes(start, shiftTime);
    ntw->display->invalidateCache();
    ntw->turnOffLedButtons();
    // set selection to previous selectable note, or zero if none
    int wheel = ntw->noteToWheel(start);
    unsigned previous = ntw->wheelToNote(std::max(0, wheel - 1));
    nt->setSelect(previous, previous < start ? start : previous + 1);
    selectButton->setSingle();
    ntw->setWheelRange();  // range is smaller
}

// hidden
void DumpButton::onDragEnd(const event::DragEnd& e) {
    this->ntw()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

void EditButton::onDragStart(const event::DragStart& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    ntw->runButton->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void FileButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void FileButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}

void InsertButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
}

void InsertButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    if (ntw->runButton->ledOn) {
        return;
    }
    ntw->clipboardInvalid = true;
    ntw->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    unsigned insertLoc;
    unsigned insertSize;
    int shiftTime;
    if (!ntw->noteCount() && ntw->clipboard.empty()) {
        insertLoc = nt->atMidiTime(0);
        DisplayNote midC = { nullptr, 0, n.ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                (uint8_t) ntw->unlockedChannel(), NOTE_ON, false };
        n.notes.insert(n.notes.begin() + insertLoc, midC);
        insertSize = 1;
        shiftTime = n.ppq;
        if (nt->debugVerbose) DEBUG("add to empty");
    } else {
        vector<DisplayNote> span;
        // Insert loc is where the new note goes, but not when the new note goes; 
        //   the new start time is 'last end time(insert loc)'
        unsigned iStart;
        unsigned iEnd = n.selectEnd;
        int lastEndTime = 0;
        if (!n.selectStart) {
            iStart = insertLoc = ntw->wheelToNote(1);
        } else {
            insertLoc = n.selectEnd;
            iStart = n.selectStart;
        // A prior edit (e.g., changing a note duration) may invalidate using select end as the
        //   insert location. Use select end to determine the last active note to insert after,
        //   but not the location to insert after.
            for (unsigned index = 0; index < iEnd; ++index) {
                const auto& note = n.notes[index];
                if (ntw->isSelectable(note)) {
                    lastEndTime = std::max(lastEndTime, note.endTime());
                    insertLoc = n.selectEnd;
                }
                if (note.isNoteOrRest() && note.startTime >= lastEndTime && index < insertLoc) {
                    insertLoc = index;
                }
            }
            if (nt->debugVerbose) DEBUG("lastEndTime %d insertLoc %u", lastEndTime, insertLoc);
        }
        // insertLoc may be different channel, so can't use that start time by itself
        // shift to selectStart time, but not less than previous end (if any) on same channel
        int insertTime = n.notes[insertLoc].startTime;
        while (insertTime < lastEndTime) {
            insertTime = n.notes[++insertLoc].startTime;
        }
        if (nt->debugVerbose) DEBUG("insertTime %d insertLoc %u clipboard size %u", insertTime, insertLoc,
                ntw->clipboard.size());
        if (!ntw->selectButton->editStart() || ntw->clipboard.empty() || !ntw->extractClipboard(&span)) {
            if (nt->debugVerbose) !n.selectStart ? DEBUG("left of first note") : DEBUG("duplicate selection");
            if (nt->debugVerbose) DEBUG("iStart=%u iEnd=%u", iStart, iEnd);
            for (unsigned index = iStart; index < iEnd; ++index) {
                const auto& note = n.notes[index];
                if (ntw->isSelectable(note)) {
                    span.push_back(note);
                }
            }
            ntw->clipboard.clear();
            ntw->setClipboardLight();
       }
        if (span.empty() || (1 == span.size() && NOTE_ON != span[0].type) ||
                (span[0].isSignature() && n.notes[insertLoc].isSignature())) {
            span.clear();
            if (nt->debugVerbose) { DEBUG("insert button : none selectable"); ntw->debugDump(false, true); }
            for (unsigned index = iStart; index < n.notes.size(); ++index) {
                const auto& note = n.notes[index];
                if (NOTE_ON == note.type && ntw->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            for (unsigned index = iStart; --index > 0; ) {
                const auto& note = n.notes[index];
                if (NOTE_ON == note.type && ntw->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            DisplayNote midC = { nullptr, 0, n.ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                    (uint8_t) ntw->unlockedChannel(), NOTE_ON, false };
            span.push_back(midC);
        }
        int nextStart = ntw->nextStartTime(insertLoc);
        NoteTaker::ShiftNotes(span, 0, lastEndTime - span.front().startTime);
        n.notes.insert(n.notes.begin() + insertLoc, span.begin(), span.end());
        insertSize = span.size();
        // include notes on other channels that fit within the start/end window
        // shift by span duration less next start (if any) on same channel minus selectStart time
        shiftTime = (lastEndTime - insertTime)
                + (NoteTaker::LastEndTime(span) - span.front().startTime);
        int availableShiftTime = nextStart - insertTime;
        if (nt->debugVerbose) DEBUG("shift time %d available %d", shiftTime, availableShiftTime);
        shiftTime = std::max(0, shiftTime - availableShiftTime);
        if (nt->debugVerbose) DEBUG("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, n.selectStart, n.selectEnd);
        ntw->display->invalidateCache();
        if (nt->debugVerbose) ntw->debugDump(false, true);
    }
    ntw->selectButton->setOff();
    ntw->insertFinal(shiftTime, insertLoc, insertSize);
    NoteTakerButton::onDragEnd(e);
}

// insert key signature
void KeyButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    insertLoc = nt->atMidiTime(n.notes[n.selectEnd].startTime);
    if (nt->insertContains(insertLoc, KEY_SIGNATURE)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote keySignature = { nullptr, startTime, 0, {0, 0, 0, 0}, 255, KEY_SIGNATURE, false };
    n.notes.insert(n.notes.begin() + insertLoc, keySignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void KeyButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 6 + af, 33 - af, "#", NULL);
    nvgText(vg, 10 + af, 41 - af, "$", NULL);
}

void PartButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        this->onTurnOff();
    } else if (ntw->selectButton->editEnd()) {
        ntw->clipboardInvalid = true;
        ntw->copySelectableNotes();
    }
    if (ntw->debugVerbose) DEBUG("part button onDragEnd ledOn %d part %d selectChannels %d unlocked %u",
            ledOn, ntw->horizontalWheel->part(), ntw->selectChannels, ntw->unlockedChannel());
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // range is larger
}

void RestButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    ntw->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    if (!ntw->selectButton->editStart()) {
        event::DragEnd e;
        ntw->cutButton->onDragEnd(e);
        if (ntw->debugVerbose) ntw->debugDump();
    }
    insertLoc = nt->atMidiTime(n.notes[n.selectEnd].startTime);
    onDragEndPreamble(e);
    DisplayNote rest = { nullptr, startTime, n.ppq, { 0, 0, 0, 0},
            (uint8_t) ntw->unlockedChannel(), REST_TYPE, false };
    shiftTime = rest.duration;
    n.notes.insert(n.notes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void RunButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    nt->debugMidiCount = 0;
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        nt->zeroGates();
    } else {
        nt->resetRun();
        ntw->display->setRange();
        unsigned next = nt->nextAfter(n.selectStart, 1);
        nt->setSelectStart(next < n.notes.size() - 1 ? next : 
                ntw->selectButton->editStart() ? 0 : 1);
        ntw->horizontalWheel->lastRealValue = INT_MAX;
        ntw->verticalWheel->lastValue = INT_MAX;
        nt->playSelection();
    }
    if (!ntw->menuButtonOn()) {
        ntw->setWheelRange();
    }
}

void SelectButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    NoteTakerButton::onDragEnd(e);
    switch (state) {
        case State::ledOff: {
            assert(ledOn);
            state = State::single; // does not call setSingle because saveZero should not change
            unsigned start = saveZero ? ntw->wheelToNote(0) : n.selectStart;
            nt->setSelect(start, nt->nextAfter(start, 1));
        } break;
        case State::single: {
            assert(!ledOn);
            af = 1;
            ledOn = true;
            if (!ntw->horizontalCount()) {
                break;  // can't start selection if there's nothing to select
            }
            int wheelStart = ntw->noteToWheel(n.selectStart);
            saveZero = !wheelStart;
            state = State::extend;
            int wheelIndex = std::max(1, wheelStart);
            selStart = ntw->wheelToNote(wheelIndex);
            assert(MIDI_HEADER != n.notes[selStart].type);
            assert(TRACK_END != n.notes[selStart].type);
            const auto& note = n.notes[selStart];
            unsigned end = note.isSignature() ? selStart + 1 : nt->atMidiTime(note.endTime());
            nt->setSelect(selStart, end);
        } break;
        case State::extend:
            assert(!ledOn);
            ntw->copySelectableNotes();
            state = State::ledOff; // does not call setOff because saveZero should not change
        break;
        default:
            assert(0);
    }
    ntw->setClipboardLight();
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // if state is single, set to horz from -1 to size
}

void SelectButton::setExtend() {
    state = State::extend;
    af = 1;
    ledOn = true;
    fb()->dirty = true;
    ntw()->setClipboardLight();
}

void SelectButton::setOff() {
    saveZero = false;
    state = State::ledOff;
    af = 0;
    ledOn = false;
    fb()->dirty = true;
    ntw()->setClipboardLight();
}

void SelectButton::setSingle() {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    saveZero = !ntw->noteToWheel(n.selectStart);
    state = State::single;
    af = 1;
    ledOn = true;
    fb()->dirty = true;
    ntw->setClipboardLight();
}

void SustainButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void SustainButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

void TempoButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    insertLoc = nt->atMidiTime(n.notes[n.selectEnd].startTime);
    if (nt->insertContains(insertLoc, MIDI_TEMPO)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote tempo = { nullptr, startTime, 0, {500000, 0, 0, 0}, 255, MIDI_TEMPO, false };
    n.notes.insert(n.notes.begin() + insertLoc, tempo);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TempoButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, "@", NULL);
}

void TieButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void TieButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 41 - af, ">", NULL);
}

// insert time signature
void TimeButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    insertLoc = nt->atMidiTime(n.notes[n.selectEnd].startTime);
    if (nt->insertContains(insertLoc, TIME_SIGNATURE)) {
        return;
    }
    shiftTime = duration = 0;
    onDragEndPreamble(e);
    DisplayNote timeSignature = { nullptr, startTime, 0, {4, 2, 24, 8}, 255, TIME_SIGNATURE, false };
    n.notes.insert(n.notes.begin() + insertLoc, timeSignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TimeButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 33 - af, "4", NULL);
    nvgText(vg, 8 + af, 41 - af, "4", NULL);
}

void TrillButton::onDragEnd(const event::DragEnd& e) {
    // to do : implement?
    return;
}

void TrillButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8.5 + af, 41 - af, "?", NULL);
}
