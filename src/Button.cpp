#include "Button.hpp"
#include "Display.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

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
    ntw->invalidateAndPlay(Inval::cut);
    nt->setSelect(insertLoc, insertLoc + 1);
    ntw->turnOffLedButtons();
    ntw->setWheelRange();  // range is larger
}

ButtonBuffer::ButtonBuffer(NoteTakerButton* button) {
    fb = new FramebufferWidget();
    fb->dirty = true;
    this->addChild(fb);
    fb->addChild(button);
}

void CutButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, ";", NULL);
}

void CutButton::getState() {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        state = State::running;
        return;
    }
    if (ntw->fileButton->ledOn()) {
        state = State::cutAll;
        return;
    }
    if (ntw->partButton->ledOn()) {
        state = State::cutPart;
        return;
    }
    SelectButton* selectButton = ntw->selectButton;
    if (selectButton->editStart() && selectButton->saveZero) {
        state = State::clearClipboard;
        return;
    }
    if (selectButton->editEnd()) {
        state = State::cutToClipboard;
        return;
    }
    state = selectButton->editStart() ? State::insertCutAndShift : State::cutAndShift;
}

void CutButton::onDragEnd(const rack::event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    this->getState();
    if (State::running == state) {
        return;
    }
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    ntw->clipboardInvalid = true;
    NoteTakerButton::onDragEnd(e);
    if (State::cutAll == state) {
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copyNotes();  // allows add to undo accidental cut / clear all
        nt->setScoreEmpty();
        nt->setSelectStart(nt->atMidiTime(0));
        ntw->selectButton->setSingle();
        ntw->setWheelRange();
        return;
    }
    if (State::cutPart == state) {
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copySelectableNotes();
        ntw->setSelectableScoreEmpty();
        nt->setSelectStart(nt->atMidiTime(0));
        ntw->setWheelRange();
        return;
    }
    if (State::clearClipboard == state) {
        ntw->clipboard.notes.clear();
        return;
    }
    unsigned start = n.selectStart;
    unsigned end = n.selectEnd;
    if (!start || end <= 1) {
        DEBUG("*** selectButton should have been set to edit start, save zero");
        _schmickled();
        return;
    }
    if (State::insertCutAndShift != state) {
        ntw->copyNotes();
    }
    int shiftTime = n.notes[start].startTime - n.notes[end].startTime;
    if (State::cutToClipboard == state) {
        // to do : insert a rest if existing notes do not include deleted span
        shiftTime = 0;
    } else {
        SCHMICKLE(State::cutAndShift == state || State::insertCutAndShift == state);
    }
    n.eraseNotes(start, end, ntw->selectChannels);
    if (shiftTime) {
        ntw->shiftNotes(start, shiftTime);
    } else {
        NoteTaker::Sort(n.notes);
    }
    ntw->invalidateAndPlay(Inval::cut);
    ntw->turnOffLedButtons();
    // set selection to previous selectable note, or zero if none
    int wheel = ntw->noteToWheel(start);
    unsigned previous = ntw->wheelToNote(std::max(0, wheel - 1));
    nt->setSelect(previous, previous < start ? start : previous + 1);
    ntw->selectButton->setSingle();
    ntw->setWheelRange();  // range is smaller
}

// hidden
void DumpButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    this->ntw()->debugDump();
    NoteTakerButton::onDragEnd(e);
}

void EditButton::onDragStart(const event::DragStart& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragStart(e);
}

void FileButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->storage.init();
    ntw->setWheelRange();
}

void FileButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}

void InsertButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
}

void InsertButton::getState() {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    if (ntw->runButton->ledOn()) {
        state = State::running;
        return;
    }
    span.clear();
    lastEndTime = 0;
    insertTime = 0;
    if (!n.noteCount(ntw->selectChannels) && ntw->clipboard.notes.empty()) {
        insertLoc = nt->atMidiTime(0);
        span.push_back(ntw->middleC());
        state = State::middleCShift;
        return;
    }
    // select set to insert (start) or off:
    //   Insert loc is where the new note goes, but not when the new note goes; 
    //   the new start time is 'last end time(insert loc)'
    // select set to extend (end):
    //   Insert loc is select start; existing notes are not shifted, insert is transposed
    unsigned iStart = n.selectStart;
    unsigned iEnd = n.selectEnd;
    auto selectButton = ntw->selectButton;
    bool insertInPlace = selectButton->editEnd();
    if (!n.selectStart) {
        iStart = insertLoc = ntw->wheelToNote(1);
    } else if (insertInPlace) {
        insertLoc = iStart;
    } else {
        insertLoc = iEnd;
    // A prior edit (e.g., changing a note duration) may invalidate using select end as the
    //   insert location. Use select end to determine the last active note to insert after,
    //   but not the location to insert after.
        for (unsigned index = 0; index < iEnd; ++index) {
            const auto& note = n.notes[index];
            if (note.isSelectable(ntw->selectChannels)) {
                lastEndTime = std::max(lastEndTime, note.endTime());
                insertLoc = n.selectEnd;
            }
            if (note.isNoteOrRest() && note.startTime >= lastEndTime && index < insertLoc) {
                insertLoc = index;
            }
        }
    }
    insertTime = n.notes[insertLoc].startTime;
    if (!insertInPlace) {
        // insertLoc may be different channel, so can't use that start time by itself
        // shift to selectStart time, but not less than previous end (if any) on same channel
        while (insertTime < lastEndTime) {
            SCHMICKLE(TRACK_END != n.notes[insertLoc].type);
            ++insertLoc;
            SCHMICKLE(insertLoc < n.notes.size());
            insertTime = n.notes[insertLoc].startTime;

        }
    }
    bool useClipboard = selectButton->ledOn();
    if (!useClipboard || ntw->clipboard.notes.empty() || !ntw->extractClipboard(&span)) {
        for (unsigned index = iStart; index < iEnd; ++index) {
            const auto& note = n.notes[index];
            if (note.isSelectable(ntw->selectChannels)) {
                span.push_back(note);
            }
        }
        state = insertInPlace ? State::dupInPlace : selectButton->editStart() ?
                State::dupLeft : State::dupShift;
    } else {
        state = insertInPlace ? State::clipboardInPlace : State::clipboardShift;
    }
    if (span.empty() || (1 == span.size() && NOTE_ON != span[0].type) ||
            (span[0].isSignature() && n.notes[insertLoc].isSignature())) {
        span.clear();
        state = insertInPlace ? State::dupInPlace : State::dupShift;
        for (unsigned index = iStart; index < n.notes.size(); ++index) {
            const auto& note = n.notes[index];
            if (NOTE_ON == note.type && note.isSelectable(ntw->selectChannels)) {
                span.push_back(note);
                break;
            }
        }
    }
    if (span.empty()) {
        for (unsigned index = iStart; --index > 0; ) {
            const auto& note = n.notes[index];
            if (NOTE_ON == note.type && note.isSelectable(ntw->selectChannels)) {
                span.push_back(note);
                break;
            }
        }
    }
    if (span.empty()) {
        const DisplayNote& midC = ntw->middleC();
        span.push_back(midC);
        state = insertInPlace ? State::middleCInPlace : State::middleCShift;
    }
}

// to do : separate into two pieces
// 1) determine what the insert button is going to do (for tooltip)
// 2) given an action, do it
void InsertButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    this->getState();
    if (State::running == state) {
        return;
    }
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    ntw->clipboardInvalid = true;
    ntw->turnOffLedButtons();  // turn off pitch, file, sustain, etc
    if (nt->debugVerbose) DEBUG("lastEndTime %d insertLoc %u", lastEndTime, insertLoc);
    if (nt->debugVerbose) DEBUG("insertTime %d clipboard size %u", insertTime,
            ntw->clipboard.notes.size());
    if (State::middleCShift != state) {
        // select set to insert (start) or off:
        //   Insert loc is where the new note goes, but not when the new note goes; 
        //   the new start time is 'last end time(insert loc)'
        // select set to extend (end):
        //   Insert loc is select start; existing notes are not shifted, insert is transposed
        if (state != State::clipboardInPlace && state != State::clipboardShift) {
            if (nt->debugVerbose) DEBUG(!n.selectStart ? "left of first note" :
                    "duplicate selection");
            ntw->clipboard.notes.clear();
            ntw->setClipboardLight();
        } else {
            if (nt->debugVerbose) DEBUG("clipboard to span (%u notes)", span.size());
        }
    }
    bool insertInPlace = ntw->selectButton->editEnd();
    unsigned insertSize = span.size();
    int shiftTime;
    if (insertInPlace) {
#if 0 // not sure what I was thinking -- already have a way to add single notes to chord
        Notes::HighestOnly(span);  // if edit end, remove all but highest note of chord            
#endif
        if (!n.transposeSpan(span)) {
            DEBUG("** failed to transpose span");
            return;
        }
        n.notes.insert(n.notes.begin() + insertLoc, span.begin(), span.end());
        shiftTime = 0;
    } else {
        int nextStart = ntw->nextStartTime(insertLoc);
        NoteTaker::ShiftNotes(span, 0, lastEndTime - span.front().startTime);
        n.notes.insert(n.notes.begin() + insertLoc, span.begin(), span.end());
        // include notes on other channels that fit within the start/end window
        // shift by span duration less next start (if any) on same channel minus selectStart time
        shiftTime = (lastEndTime - insertTime)
                + (NoteTaker::LastEndTime(span) - span.front().startTime);
        int availableShiftTime = nextStart - insertTime;
        if (nt->debugVerbose) DEBUG("shift time %d available %d", shiftTime, availableShiftTime);
        shiftTime = std::max(0, shiftTime - availableShiftTime);
        if (nt->debugVerbose) DEBUG("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u selectEnd=%u",
                insertLoc, insertSize, shiftTime, n.selectStart, n.selectEnd);
        if (nt->debugVerbose) ntw->debugDump(false, true);
    }
    ntw->selectButton->setOff();
    ntw->insertFinal(shiftTime, insertLoc, insertSize);
    NoteTakerButton::onDragEnd(e);
}

// insert key signature
void KeyButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
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
    DisplayNote keySignature(KEY_SIGNATURE);
    n.notes.insert(n.notes.begin() + insertLoc, keySignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void KeyButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 6 + af, 33 - af, "#", NULL);
    nvgText(vg, 10 + af, 41 - af, "$", NULL);
}

void PartButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    if (!ledOn()) {
        this->onTurnOff();
    } else if (ntw->selectButton->editEnd()) {
        ntw->clipboardInvalid = true;
        ntw->copySelectableNotes();
    }
    if (ntw->debugVerbose) DEBUG("part button onDragEnd ledOn %d part %d selectChannels %d unlocked %u",
            this->ledOn(), ntw->horizontalWheel->part(), ntw->selectChannels, ntw->unlockedChannel());
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // range is larger
}

void RestButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
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
    DisplayNote rest(REST_TYPE, startTime, n.ppq, (uint8_t) ntw->unlockedChannel());
    shiftTime = rest.duration;
    n.notes.insert(n.notes.begin() + insertLoc, rest);
    AdderButton::onDragEnd(e);
}

void RunButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    if (!ledOn()) {
        nt->zeroGates();
    } else {
        auto& n = nt->n;
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
    DEBUG("onDragEnd end af %d ledOn %d", animationFrame, ledOn());
}

void SelectButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    auto nt = ntw->nt();
    auto& n = nt->n;
    NoteTakerButton::onDragEnd(e);
    if (this->isOff()) {
        SCHMICKLE(!this->ledOn());
        ntw->copySelectableNotes();
    } else if (this->isSingle()) {
        SCHMICKLE(this->ledOn());
        nt->invalidVoiceCount |= ntw->edit.voice;
        ntw->edit.voice = false;
        unsigned start = saveZero ? ntw->wheelToNote(0) : n.selectStart;
        nt->setSelect(start, nt->nextAfter(start, 1));
    } else {
        SCHMICKLE(this->isExtend());
        SCHMICKLE(this->ledOn());
        nt->invalidVoiceCount |= ntw->edit.voice;
        ntw->edit.voice = false;
        if (!n.horizontalCount(ntw->selectChannels)) {
            ntw->clipboard.notes.clear();
            this->setState(State::single);  // can't start selection if there's nothing to select
        } else {
            int wheelStart = ntw->noteToWheel(n.selectStart);
            saveZero = !wheelStart;
            int wheelIndex = std::max(1, wheelStart);
            selStart = ntw->wheelToNote(wheelIndex);
            SCHMICKLE(MIDI_HEADER != n.notes[selStart].type);
            SCHMICKLE(TRACK_END != n.notes[selStart].type);
            const auto& note = n.notes[selStart];
            unsigned end = note.isSignature() ? selStart + 1 : nt->nextAfter(selStart, 1);
            nt->setSelect(selStart, end);
        }
    }
    ntw->setClipboardLight();
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();  // if state is single, set to horz from -1 to size
}

// currently not called by anyone
#if 0
void SelectButton::setExtend() {
    this->setState(State::extend);
    fb()->dirty = true;
    ntw()->setClipboardLight();
}
#endif

void SelectButton::setOff() {
    saveZero = false;
    this->setState(State::ledOff);
    fb()->dirty = true;
    ntw()->setClipboardLight();
}

void SelectButton::setSingle() {
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    auto& n = nt->n;
    saveZero = !ntw->noteToWheel(n.selectStart);
    this->setState(State::single);
    fb()->dirty = true;
    ntw->setClipboardLight();
}

void SustainButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void SustainButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

void TempoButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
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
    DisplayNote tempo(MIDI_TEMPO, startTime);
    n.notes.insert(n.notes.begin() + insertLoc, tempo);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TempoButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, "@", NULL);
}

void TieButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLedButtons(this);
    ntw->setWheelRange();
}

void TieButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 41 - af, ">", NULL);
}

// insert time signature
void TimeButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
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
    DisplayNote timeSignature(TIME_SIGNATURE, startTime);
    n.notes.insert(n.notes.begin() + insertLoc, timeSignature);
    shiftTime = duration = 0;
    AdderButton::onDragEnd(e);
}

void TimeButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 33 - af, "4", NULL);
    nvgText(vg, 8 + af, 41 - af, "4", NULL);
}

void TrillButton::onDragEnd(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    // to do : implement?
    return;
}

void TrillButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8.5 + af, 41 - af, "?", NULL);
}
