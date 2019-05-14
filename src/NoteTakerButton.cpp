#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"

// try to get rest working as well as note
void AdderButton::onDragEndPreamble(const event::DragEnd& e) {
    NoteTaker* nt = NT(this);
    // insertLoc, shiftTime set by caller
    shiftLoc = insertLoc + 1;
    startTime = nt->notes[insertLoc].startTime;
    if (nt->debugVerbose) debug("insertLoc %u shiftLoc %u startTime %d", insertLoc, shiftLoc, startTime);
}

void AdderButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    auto nt = NT(this);
    if (shiftTime) {
        NoteTaker::ShiftNotes(ntw->nt()->notes, shiftLoc, shiftTime);
    }
    NoteTaker::Sort(ntw->nt()->notes);
    NTWidget<SelectButton>(this)->setOff();
    NoteTakerButton::onDragEnd(e);
    NTWidget<NoteTakerDisplay>(this)->invalidateCache();
    nt->setSelect(insertLoc, insertLoc + 1);
    ntw->turnOffLedButtons();
    ntw->setWheelRange();  // range is larger
}

void ButtonWidget::draw(const DrawArgs& args) {
    auto button = this->getAncestorOfType<NoteTakerButton>();
    button->draw(args);
}

void CutButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
}

void CutButton::onDragEnd(const rack::event::DragEnd& e) {
    auto ntw = NTW(this);
    auto nt = NT(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    ntw->clipboardInvalid = true;
    NoteTakerButton::onDragEnd(e);
    if (NTWidget<FileButton>(this)->ledOn) {
        nt->selectStart = 0;
        nt->selectEnd = nt->notes.size() - 1;
        ntw->copyNotes();  // allows add to undo accidental cut / clear all
        nt->setScoreEmpty();
        NTWidget<NoteTakerDisplay>(this)->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        NTWidget<SelectButton>(this)->setSingle();
        ntw->setWheelRange();
        return;
    }
    if (NTWidget<PartButton>(this)->ledOn) {
        nt->selectStart = 0;
        nt->selectEnd = nt->notes.size() - 1;
        ntw->copySelectableNotes();
        ntw->setSelectableScoreEmpty();
        NTWidget<NoteTakerDisplay>(this)->invalidateCache();
        nt->setSelectStart(nt->atMidiTime(0));
        ntw->setWheelRange();
        return;
    }
    SelectButton* selectButton = NTWidget<SelectButton>(this);
    if (selectButton->editStart() && selectButton->saveZero) {
        return;
    }
    unsigned start = nt->selectStart;
    unsigned end = nt->selectEnd;
    if (!start || end <= 1) {
        debug("*** selectButton should have been set to edit start, save zero");
        assert(0);
        return;
    }
    if (selectButton->editEnd()) {
        ntw->copyNotes();
    }
    int shiftTime = nt->notes[start].startTime - nt->notes[end].startTime;
    ntw->eraseNotes(start, end);
    ntw->shiftNotes(start, shiftTime);
    NTWidget<NoteTakerDisplay>(this)->invalidateCache();
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
    NTW(this)->debugDump();
    NoteTakerButton::onDragEnd(e);
}

void EditButton::onDragStart(const event::DragStart& e) {
    if (NTW(this)->widget<RunButton>()->ledOn) {
        return;
    }
    NTWidget<RunButton>(this)->ledOn = false;
    NoteTakerButton::onDragStart(e);
}

void FileButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void FileButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}

void InsertButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
}

void InsertButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    auto nt = NT(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    ntw->clipboardInvalid = true;
    SelectButton* selectButton = NTWidget<SelectButton>(this);
    ntw->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    unsigned insertLoc;
    unsigned insertSize;
    int shiftTime;
    if (!ntw->noteCount() && ntw->clipboard.empty()) {
        insertLoc = nt->atMidiTime(0);
        DisplayNote midC = { nullptr, 0, nt->ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                (uint8_t) ntw->unlockedChannel(), NOTE_ON, false };
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
            iStart = insertLoc = ntw->wheelToNote(1);
        } else {
            insertLoc = nt->selectEnd;
            iStart = nt->selectStart;
        // A prior edit (e.g., changing a note duration) may invalidate using select end as the
        //   insert location. Use select end to determine the last active note to insert after,
        //   but not the location to insert after.
            for (unsigned index = 0; index < iEnd; ++index) {
                const auto& note = nt->notes[index];
                if (ntw->isSelectable(note)) {
                    lastEndTime = std::max(lastEndTime, note.endTime());
                    insertLoc = nt->selectEnd;
                }
                if (note.isNoteOrRest() && note.startTime >= lastEndTime && index < insertLoc) {
                    insertLoc = index;
                }
            }
            if (nt->debugVerbose) debug("lastEndTime %d insertLoc %u", lastEndTime, insertLoc);
        }
        // insertLoc may be different channel, so can't use that start time by itself
        // shift to selectStart time, but not less than previous end (if any) on same channel
        int insertTime = nt->notes[insertLoc].startTime;
        while (insertTime < lastEndTime) {
            insertTime = nt->notes[++insertLoc].startTime;
        }
        if (nt->debugVerbose) debug("insertTime %d insertLoc %u clipboard size %u", insertTime, insertLoc,
                ntw->clipboard.size());
        if (!selectButton->editStart() || ntw->clipboard.empty() || !ntw->extractClipboard(&span)) {
            if (nt->debugVerbose) !nt->selectStart ? debug("left of first note") : debug("duplicate selection");
            if (nt->debugVerbose) debug("iStart=%u iEnd=%u", iStart, iEnd);
            for (unsigned index = iStart; index < iEnd; ++index) {
                const auto& note = nt->notes[index];
                if (ntw->isSelectable(note)) {
                    span.push_back(note);
                }
            }
            ntw->clipboard.clear();
        }
        if (span.empty() || (1 == span.size() && NOTE_ON != span[0].type) ||
                (span[0].isSignature() && nt->notes[insertLoc].isSignature())) {
            span.clear();
            if (nt->debugVerbose) { debug("insert button : none selectable"); ntw->debugDump(); }
            for (unsigned index = iStart; index < nt->notes.size(); ++index) {
                const auto& note = nt->notes[index];
                if (NOTE_ON == note.type && ntw->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            for (unsigned index = iStart; --index > 0; ) {
                const auto& note = nt->notes[index];
                if (NOTE_ON == note.type && ntw->isSelectable(note)) {
                    span.push_back(note);
                    break;
                }
            }
        }
        if (span.empty()) {
            DisplayNote midC = { nullptr, 0, nt->ppq, { 60, 0, stdKeyPressure, stdKeyPressure},
                    (uint8_t) ntw->unlockedChannel(), NOTE_ON, false };
            span.push_back(midC);
        }
        int nextStart = ntw->nextStartTime(insertLoc);
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
        NTWidget<NoteTakerDisplay>(this)->invalidateCache();
        if (nt->debugVerbose) ntw->debugDump(false);
    }
    selectButton->setOff();
    ntw->insertFinal(shiftTime, insertLoc, insertSize);
    NoteTakerButton::onDragEnd(e);
}

// insert key signature
void KeyButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTaker* nt = NT(this);
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

void KeyButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 6 + af, 33 - af, "#", NULL);
    nvgText(vg, 10 + af, 41 - af, "$", NULL);
}

void PartButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        this->onTurnOff();
    } else if (NTWidget<SelectButton>(this)->editEnd()) {
        ntw->clipboardInvalid = true;
        ntw->copySelectableNotes();
    }
    if (ntw->debugVerbose) debug("part button onDragEnd ledOn %d part %d selectChannels %d unlocked %u",
            ledOn, NTWidget<HorizontalWheel>(this)->part(), ntw->selectChannels, ntw->unlockedChannel());
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // range is larger
}

void RestButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTaker* nt = NT(this);
    ntw->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    if (!NTWidget<SelectButton>(this)->editStart()) {
        event::DragEnd e;
        NTWidget<CutButton>(this)->onDragEnd(e);
        if (ntw->debugVerbose) ntw->debugDump();
    }
    insertLoc = nt->atMidiTime(nt->notes[nt->selectEnd].startTime);
    onDragEndPreamble(e);
    DisplayNote rest = { nullptr, startTime, nt->ppq, { 0, 0, 0, 0},
            (uint8_t) ntw->unlockedChannel(), REST_TYPE, false };
    shiftTime = rest.duration;
    nt->notes.insert(nt->notes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void RunButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    NoteTaker* nt = NT(this);
    nt->debugMidiCount = 0;
    NoteTakerButton::onDragEnd(e);
    if (!ledOn) {
        nt->zeroGates();
    } else {
        nt->resetRun();
        NTWidget<NoteTakerDisplay>(this)->setRange();
        unsigned next = nt->nextAfter(nt->selectStart, 1);
        nt->setSelectStart(next < nt->notes.size() - 1 ? next : 
                NTWidget<SelectButton>(this)->editStart() ? 0 : 1);
        NTWidget<HorizontalWheel>(this)->lastRealValue = INT_MAX;
        NTWidget<VerticalWheel>(this)->lastValue = INT_MAX;
        nt->playSelection();
    }
    if (!ntw->menuButtonOn()) {
        ntw->setWheelRange();
    }
}

void SelectButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    NoteTaker* nt = NT(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    switch (state) {
        case State::ledOff: {
            assert(ledOn);
            state = State::single; // does not call setSingle because saveZero should not change
            unsigned start = saveZero ? ntw->wheelToNote(0) : nt->selectStart;
            nt->setSelect(start, nt->nextAfter(start, 1));
        } break;
        case State::single: {
            assert(!ledOn);
            af = 1;
            ledOn = true;
            if (!ntw->horizontalCount()) {
                break;  // can't start selection if there's nothing to select
            }
            int wheelStart = ntw->noteToWheel(nt->selectStart);
            saveZero = !wheelStart;
            state = State::extend;
            int wheelIndex = std::max(1, wheelStart);
            selStart = ntw->wheelToNote(wheelIndex);
            assert(MIDI_HEADER != nt->notes[selStart].type);
            assert(TRACK_END != nt->notes[selStart].type);
            const auto& note = nt->notes[selStart];
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
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // if state is single, set to horz from -1 to size
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
    auto ntw = NTW(this);
    auto nt = NT(this);
    saveZero = !ntw->noteToWheel(nt->selectStart);
    state = State::single;
    af = 1;
    ledOn = true;
}

void SustainButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void SustainButton::draw(const DrawArgs& args) {
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

void TempoButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTaker* nt = NT(this);
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

void TempoButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, "@", NULL);
}

void TieButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    if (ntw->widget<RunButton>()->ledOn) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void TieButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 41 - af, ">", NULL);
}

// insert time signature
void TimeButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = NTW(this);
    NoteTaker* nt = NT(this);
    if (ntw->widget<RunButton>()->ledOn) {
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

void TimeButton::draw(const DrawArgs& args) {
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, MusicFont(this));
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
    nvgFontFaceId(vg, MusicFont(this));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8.5 + af, 41 - af, "?", NULL);
}
