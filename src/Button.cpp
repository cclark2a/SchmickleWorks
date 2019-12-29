#include "Button.hpp"
#include "Display.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

// try to get rest working as well as note
void AdderButton::onDragEndPreamble(const event::DragEnd& e) {
    // insertLoc, shiftTime set by caller
    shiftLoc = insertLoc + 1;
    startTime = this->ntw()->n().notes[insertLoc].startTime;
    if (debugVerbose) DEBUG("insertLoc %u shiftLoc %u startTime %d", insertLoc, shiftLoc, startTime);
}

void AdderButton::onDragEnd(const event::DragEnd& e) {
    auto ntw = this->ntw();
    auto& n = ntw->n();
    if (shiftTime) {
        n.shift(shiftLoc, shiftTime, AddToChannels::all == addToChannels ?
                ALL_CHANNELS : this->ntw()->selectChannels);
    }
    n.sort();
    ntw->selectButton->setOff();
    NoteTakerButton::onDragEnd(e);
    ntw->invalAndPlay(Inval::cut);
    ntw->setSelect(insertLoc, insertLoc + 1);
    ntw->turnOffLEDButtons();
    ntw->setWheelRange();  // range is larger
    ntw->displayBuffer->redraw();
}

ButtonBuffer::ButtonBuffer(NoteTakerButton* button) {
    fb = new FramebufferWidget();
    fb->dirty = true;
    this->addChild(fb);
    fb->addChild(button);
}

template<class TButton>
std::string WidgetToolTip<TButton>::getDisplayValueString() {
    if (!widget) {
        return "!uninitialized";
    }
    if (defaultLabel.empty()) {
        defaultLabel = label;
    }
    if (widget->ntw()->runButton->ledOn()) {
        label = "Play";
        return widget->runningTooltip();
    } else {
        label = defaultLabel;
    }
    return "";
}

template struct WidgetToolTip<CutButton>;
template struct WidgetToolTip<FileButton>;
template struct WidgetToolTip<InsertButton>;
template struct WidgetToolTip<KeyButton>;
template struct WidgetToolTip<PartButton>;
template struct WidgetToolTip<RestButton>;
template struct WidgetToolTip<SelectButton>;
template struct WidgetToolTip<SustainButton>;
template struct WidgetToolTip<SlotButton>;
template struct WidgetToolTip<TempoButton>;
template struct WidgetToolTip<TieButton>;
template struct WidgetToolTip<TimeButton>;

void CutButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
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
    if (selectButton->editStart()
            && (ntw->slotButton->ledOn() ? ntw->storage.saveZero : selectButton->saveZero)) {
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
    if (this->stageSlot(e)) {
        return;
    }
    this->getState();
    auto ntw = this->ntw();
    auto& n = ntw->n();
    ntw->clipboardInvalid = true;
    NoteTakerButton::onDragEnd(e);
    if (State::cutAll == state) {
        SCHMICKLE(!ntw->slotButton->ledOn());
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copyNotes();  // allows add to undo accidental cut / clear all
        ntw->setScoreEmpty();
        ntw->setSelectStart(n.atMidiTime(0));
        ntw->selectButton->setSingle();
        ntw->setWheelRange();
        ntw->displayBuffer->redraw();
        return;
    }
    if (State::cutPart == state) {
        SCHMICKLE(!ntw->slotButton->ledOn());
        n.selectStart = 0;
        n.selectEnd = n.notes.size() - 1;
        ntw->copySelectableNotes();
        ntw->setSelectableScoreEmpty();
        ntw->setSelectStart(n.atMidiTime(0));
        ntw->setWheelRange();
        ntw->displayBuffer->redraw();
        return;
    }
    bool slotOn = ntw->slotButton->ledOn();
    if (State::clearClipboard == state) {
        ntw->clipboard.clear(slotOn);
        return;
    }
    unsigned start = slotOn ? ntw->storage.slotStart : n.selectStart;
    unsigned end = slotOn ? ntw->storage.slotEnd : n.selectEnd;
    if (!slotOn && (!start || end <= 1)) {
        DEBUG("*** selectButton should have been set to edit start, save zero");
        _schmickled();
        return;
    }
    if (State::insertCutAndShift != state) {
        slotOn ? ntw->copySlots() : ntw->copyNotes();
    }
    if (slotOn) {
        if (0 == start && ntw->storage.size() <= end) {
            ++start;    // always leave one slot
        }
        if (start >= end) {
            return;
        }
        ntw->storage.shiftSlots(start, end);
        if (ntw->storage.size() <= start) {
            --start;    // move select to last remaining slot
        }
        ntw->storage.slotStart = start;
        ntw->storage.slotEnd = start + 1;
    } else {
        int shiftTime = n.notes[start].startTime - n.notes[end].startTime;
        if (State::cutToClipboard == state) {
            // to do : insert a rest if existing notes do not include deleted span
            shiftTime = 0;
        } else {
            SCHMICKLE(State::cutAndShift == state || State::insertCutAndShift == state);
        }
        // set selection to previous selectable note, or zero if none
        int vWheel = (int) ntw->verticalWheel->getValue();
        if (vWheel) {
            float lo, hi;
            ntw->verticalWheel->getLimits(&lo, &hi);
            if (vWheel == (int) hi - 1) {
                --vWheel;
            }
        }
        if (vWheel)  {
            unsigned selStart = ntw->edit.voices[vWheel - 1];
            ntw->setSelect(selStart, selStart + 1);
        } else {
            int wheel = ntw->noteToWheel(start);
            unsigned previous = ntw->wheelToNote(std::max(0, wheel - 1));
            ntw->setSelect(previous, previous < start ? start : previous + 1);
        }
        n.eraseNotes(start, end, ntw->selectChannels);
        if (shiftTime) {
            ntw->shiftNotes(start, shiftTime);
        } else {
            n.sort();
        }
        ntw->invalAndPlay(Inval::cut);
        ntw->turnOffLEDButtons();
    }
    ntw->selectButton->setSingle();
    // range is smaller
    ntw->setWheelRange();
    ntw->displayBuffer->redraw();
}

void EditButton::onDragStart(const event::DragStart& e) {
    auto ntw = this->ntw();
    if (ntw->runButton->ledOn()) {
        return;
    }
    NoteTakerButton::onDragStart(e);
}

void EditLEDButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    NoteTakerButton::onDragEnd(e);
    ntw->turnOffLEDButtons(this);
    ntw->setWheelRange();
    ntw->displayBuffer->redraw();
}

void FileButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, ":", NULL);
}


// to do : change graphic on insert button to show what pressing the button will do
// distinguish : add one note after / add multiple notes after / add one note above / 
//               add multiple notes above
// also change tooltips to describe this in words
void InsertButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "H", NULL);
}

// allows tooltip to show what button is going to do without pressing it
void InsertButton::getState() {
    auto ntw = this->ntw();
    const auto& n = ntw->n();
    if (ntw->runButton->ledOn()) {
        state = State::running;
        return;
    }
    auto selectButton = ntw->selectButton;
    bool useClipboard = selectButton->ledOn();
    if (ntw->slotButton->ledOn()) {
        insertLoc = selectButton->editStart() ? ntw->storage.startToWheel() : ntw->storage.slotEnd;
        state = useClipboard && !ntw->clipboard.playback.empty() ? State::clipboardShift :
                State::dupShift;
        return;
    }
    span.clear();
    // all notes on pushed on span stack require note off to be added as well
    lastEndTime = 0;
    insertTime = 0;
    if (!n.noteCount(ntw->selectChannels) && ntw->clipboard.notes.empty()) {
        insertLoc = n.atMidiTime(0);
        span.push_back(ntw->middleC());
        Notes::AddNoteOff(span);
        state = State::middleCShift;
        return;
    }
    // select set to insert (start) or off:
    //   Insert loc is where the new note goes, but not when the new note goes; 
    //   the new start time is 'last end time(insert loc)'
    // select set to extend (end):
    //   Insert loc is select start; existing notes are not shifted, insert is transposed
    bool insertInPlace = selectButton->editEnd();
    unsigned iStart = n.selectStart;
    if (!iStart) {
        iStart = insertLoc = ntw->wheelToNote(1);
    } else if (insertInPlace) {
        insertLoc = n.selectStart;
    } else {
        insertLoc = n.selectEnd;
    // A prior edit (e.g., changing a note duration) may invalidate using select end as the
    //   insert location. Use select end to determine the last active note to insert after,
    //   but not the location to insert after.
        for (unsigned index = 0; index < n.selectEnd; ++index) {
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
    while (NOTE_OFF == n.notes[insertLoc].type) {
        ++insertLoc;
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
    if (!useClipboard || ntw->clipboard.notes.empty() || !ntw->extractClipboard(&span)) {
        for (unsigned index = iStart; index < n.selectEnd; ++index) {
            const auto& note = n.notes[index];
            if (note.isSelectable(ntw->selectChannels)) {   // use lambda for this pattern
                span.push_back(note);
                span.back().cache = nullptr;
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
                span.back().cache = nullptr;
                break;
            }
        }
    }
    if (span.empty()) {
        for (unsigned index = iStart; --index > 0; ) {
            const auto& note = n.notes[index];
            if (NOTE_ON == note.type && note.isSelectable(ntw->selectChannels)) {
                span.push_back(note);
                span.back().cache = nullptr;
                break;
            }
        }
    }
    if (span.empty()) {
        const DisplayNote& midC = ntw->middleC();
        span.push_back(midC);
        state = insertInPlace ? State::middleCInPlace : State::middleCShift;
    }
    if (1 == span.size()) {
        Notes::AddNoteOff(span);
    }
}

void InsertButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    this->getState();
    auto ntw = this->ntw();
    auto& n = ntw->n();
    ntw->clipboardInvalid = true;
    ntw->turnOffLEDButtons(nullptr, true);  // turn off pitch, file, sustain, etc but not slot
    if (debugVerbose) DEBUG("lastEndTime %d insertLoc %u", lastEndTime, insertLoc);
    bool slotOn = ntw->slotButton->ledOn();
    if (debugVerbose) DEBUG("insertTime %d clipboard size %u", insertTime,
            slotOn ? ntw->clipboard.playback.size() : ntw->clipboard.notes.size());
    vector<SlotPlay> pspan;
    if (State::middleCShift != state) {
        // select set to insert (start) or off:
        //   Insert loc is where the new note goes, but not when the new note goes; 
        //   the new start time is 'last end time(insert loc)'
        // select set to extend (end):
        //   Insert loc is select start; existing notes are not shifted, insert is transposed
        if (state != State::clipboardInPlace && state != State::clipboardShift) {
            if (debugVerbose) DEBUG(!n.selectStart ? "left of first note" :
                    "duplicate selection");
            ntw->clipboard.clear(slotOn);
            ntw->setClipboardLight();
            if (slotOn) {
                pspan.assign(ntw->storage.playback.begin() + ntw->storage.slotStart,
                        ntw->storage.playback.begin() + ntw->storage.slotEnd);
            }
        } else {
            if (slotOn) {
                pspan = ntw->clipboard.playback;
            }
            if (debugVerbose) slotOn ? DEBUG("clipboard to pspan (%u slots)", pspan.size()) :
                    DEBUG("clipboard to span (%u notes)", span.size());
        }
    }
    // insertInPlace mode disabled for now, for several reasons
    // it allows duplicating (a top note of) a phrase and transposing it, but the 'top note' part is buggy
    // the result is a non-continuous selection -- currently that isn't supported
    // it's an idea that is not uniformly supported: i.e., one can't copy and paste the top note of a phrase
    // or copy from one part to another 
    // as mentioned, highest only is buggy and without it it would be odd to copy and transpose whole chords
    bool insertInPlace = false && !slotOn && ntw->selectButton->editEnd();
    unsigned insertSize = slotOn ? pspan.size() : span.size();
    int shiftTime = 0;
    if (insertInPlace) {
// not sure what I was thinking -- already have a way to add single notes to chord
        //  Notes::HighestOnly(span);  // if edit end, remove all but highest note of chord            
        if (!n.transposeSpan(span)) {
            DEBUG("** failed to transpose span");
            return;
        }
        n.notes.insert(n.notes.begin() + insertLoc, span.begin(), span.end());
    } else {
        if (slotOn) {
            ntw->storage.playback.insert(ntw->storage.playback.begin() + insertLoc, pspan.begin(),
                    pspan.end());
            ntw->stageSlot(pspan[0].index);
        } else {
            int nextStart = ntw->nextStartTime(insertLoc);
            if (Notes::ShiftNotes(span, 0, lastEndTime - span.front().startTime)) {
                std::sort(span.begin(), span.end());
            }
            n.notes.insert(n.notes.begin() + insertLoc, span.begin(), span.end());
            // include notes on other channels that fit within the start/end window
           // shift by span duration less next start (if any) on same channel minus selectStart time
            shiftTime = (lastEndTime - insertTime)
                    + (Notes::LastEndTime(span) - span.front().startTime);
            int availableTime = nextStart - insertTime;
            if (debugVerbose) DEBUG("shift time %d available %d", shiftTime, availableTime);
            shiftTime = std::max(0, shiftTime - availableTime);
        }
        if (debugVerbose) DEBUG("insertLoc=%u insertSize=%u shiftTime=%d selectStart=%u"
                " selectEnd=%u",
                insertLoc, insertSize, shiftTime, slotOn ? ntw->storage.slotStart : n.selectStart,
                slotOn ? ntw->storage.slotEnd : n.selectEnd);
        if (debugVerbose) ntw->debugDump(false, true);
    }
    ntw->selectButton->setOff();
    ntw->insertFinal(shiftTime, insertLoc, insertSize);
    NoteTakerButton::onDragEnd(e);
}

// insert key signature
void KeyButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    auto& n = ntw->n();
    insertLoc = n.atMidiTime(n.notes[n.selectEnd].startTime);
    if (n.insertContains(insertLoc, KEY_SIGNATURE)) {
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
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 6 + af, 33 - af, "#", NULL);
    nvgText(vg, 10 + af, 41 - af, "$", NULL);
}

void NoteTakerButton::draw(const DrawArgs& args) {
    if (false && debugVerbose) {
        float t[6];
        nvgCurrentTransform(args.vg, t);
        if (memcmp(lastTransform, t, sizeof(lastTransform))) {
            DEBUG("notetakerbutton xform %g %g %g %g %g %g", t[0], t[1], t[2], t[3], t[4], t[5]);
            memcpy(lastTransform, t, sizeof(lastTransform));
            this->fb()->dirty = true;
        }
    } 
    const int af = animationFrame;
    auto ntw = this->ntw();
    auto nt = ntw->nt();
    if (!nt || slotNumber >= SLOT_COUNT) {
        return;
    }
    auto runButton = ntw->runButton;
    if (runButton->dynamicRunTimer >= nt->getRealSeconds()) {
        this->fb()->dirty = true;
    }
    auto vg = args.vg;
    int alpha = std::max(0, (int) (255 * (runButton->dynamicRunTimer - nt->getRealSeconds())
            / runButton->fadeDuration));
    if (runButton->ledOn()) {
        alpha = 255 - alpha;
    }
    runButton->dynamicRunAlpha = alpha;
    if (ntw->storage.slots[slotNumber].n.isEmpty(ALL_CHANNELS)) {
        alpha /= 3;
    }
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    std::string label = std::to_string(slotNumber + 1);
    nvgFontFaceId(vg, ntw->textFont());
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    nvgFontSize(vg, 18);
    nvgText(vg, 10 + af, 32 - af, label.c_str(), NULL);
}

int NoteTakerButton::runAlpha() const {
    return 255 - ntw()->runButton->dynamicRunAlpha;
}

bool NoteTakerButton::stageSlot(const event::DragEnd& e) {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return true;
    }
    auto ntw = this->ntw();
    if (!ntw->runButton->ledOn()) {
        return false;
    }
    if (ntw->storage.slots[slotNumber].n.isEmpty(ALL_CHANNELS)) {
        return true;
    }
    ntw->stageSlot(slotNumber);
    return true;
}

void PartButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 41 - af, "\"", NULL);
}

void PartButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    NoteTakerButton::onDragEnd(e);
    auto ntw = this->ntw();
    if (!ledOn()) {
        this->onTurnOff();
    } else if (ntw->selectButton->editEnd()) {
        ntw->clipboardInvalid = true;
        ntw->copySelectableNotes();
    }
    if (debugVerbose) DEBUG("part button onDragEnd ledOn %d part %d selectChannels %d unlocked %u",
            this->ledOn(), ntw->horizontalWheel->part(), ntw->selectChannels, ntw->unlockedChannel());
    ntw->turnOffLEDButtons(this);
    // range is larger
    ntw->setWheelRange();
    ntw->displayBuffer->redraw();
}

void RestButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 36);
    nvgText(vg, 8 + af, 41 - af, "t", NULL);
}

void RestButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    auto& n = ntw->n();
    ntw->turnOffLEDButtons();  // turn off pitch, file, sustain, etc
    if (!ntw->selectButton->editStart()) {
        event::DragEnd e;
        ntw->cutButton->onDragEnd(e);
        if (debugVerbose) ntw->debugDump();
    }
    insertLoc = n.atMidiTime(n.notes[n.selectEnd].startTime);
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
    if (!this->ledOn()) {
        ntw->enableButtons();
        dynamicRunAlpha = 255;
        nt->requests.push(RequestType::resetPlayStart);
    } else {
        nt->requests.push(RequestType::resetAndPlay);
        ntw->resetForPlay();
        ntw->turnOffLEDButtons(this, true);
        ntw->disableEmptyButtons();
        dynamicRunAlpha = 0;
    }

    dynamicRunTimer = nt->getRealSeconds() + fadeDuration;
    ntw->setWheelRange();
    ntw->displayBuffer->redraw();
    DEBUG("onDragEnd end af %d ledOn %d", animationFrame, ledOn());
}

// to do : change drawn graphic to show the mode? edit / insert / select ?
//         (not crazy about this idea)
void SelectButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "<", NULL);  // was \u00E0
}

void SelectButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    auto& n = ntw->n();
    NoteTakerButton::onDragEnd(e);
    bool slotOn = ntw->slotButton->ledOn();
    auto& storage = ntw->storage;
    if (this->isOff()) {
        SCHMICKLE(!this->ledOn());
        slotOn ? ntw->copySlots() : ntw->copySelectableNotes();
    } else if (this->isSingle()) {
        SCHMICKLE(this->ledOn());
        if (slotOn) {
            if (debugVerbose) DEBUG("isSingle pre storage saveZero=%d s=%d e=%d", storage.saveZero,
                    storage.slotStart, storage.slotEnd);
            // to do : leave slot end alone, and only look at slot start in insertion mode?
            storage.slotEnd = storage.slotStart + 1; 
            if (debugVerbose) DEBUG("isSingle post storage saveZero=%d s=%d e=%d", storage.saveZero,
                    storage.slotStart, storage.slotEnd);
        } else {
            if (ntw->edit.voice) {
                ntw->nt()->requests.push(RequestType::invalidateVoiceCount);
                ntw->edit.voice = false;
            }
            unsigned start = saveZero ? ntw->wheelToNote(0) : n.selectStart;
            ntw->setSelect(start, n.nextAfter(start, 1));
        }
    } else {
        SCHMICKLE(this->isExtend());
        SCHMICKLE(this->ledOn());
        if (slotOn) {
            if (debugVerbose) DEBUG("isExtend pre storage saveZero=%d s=%d e=%d", storage.saveZero,
                    storage.slotStart, storage.slotEnd);
        } else {
            if (ntw->edit.voice) {
                ntw->nt()->requests.push(RequestType::invalidateVoiceCount);
                ntw->edit.voice = false;
            }
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
                unsigned end = note.isSignature() ? selStart + 1 : n.nextAfter(selStart, 1);
                ntw->setSelect(selStart, end);
            }
        }
    }
    ntw->setClipboardLight();
    ntw->turnOffLEDButtons(this, true);
    // if state is single, set to horz from -1 to size
    ntw->setWheelRange();
    ntw->displayBuffer->redraw();
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
    auto& n = ntw->n();
    saveZero = !ntw->noteToWheel(n.selectStart);
    this->setState(State::single);
    fb()->dirty = true;
    ntw->setClipboardLight();
}

void SlotButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 32);
    nvgText(vg, 1.5 + af, 41 - af, "?", NULL);
}

void SustainButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditLEDButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "=", NULL);
}

void TempoButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    auto& n = ntw->n();
    insertLoc = n.atMidiTime(n.notes[n.selectEnd].startTime);
    if (n.insertContains(insertLoc, MIDI_TEMPO)) {
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
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 5 + af, 41 - af, "@", NULL);
}

// to do : call body (minus inherited part) on json load to restore state?
void TieButton::onDragEnd(const event::DragEnd& e) {
    // horz: no slur / original / all slur
    // vert: no triplet / original / all triplet
    // to do : if selection is too small to slur / trip, button should be disabled ?
    auto ntw = this->ntw();
    ntw->edit.clear();
    auto& n = ntw->n();
    ntw->edit.init(n, ntw->selectChannels);
    setTie = n.slursOrTies(ntw->selectChannels, Notes::HowMany::set, &setSlur);
    setTriplet = n.triplets(ntw->selectChannels, Notes::HowMany::set);
    clearTie = n.slursOrTies(ntw->selectChannels, Notes::HowMany::clear, &clearSlur);
    clearTriplet = n.triplets(ntw->selectChannels, Notes::HowMany::clear);
    EditLEDButton::onDragEnd(e);
}

void TieButton::draw(const DrawArgs& args) {
    const int af = animationFrame;
    EditButton::draw(args);
    NVGcontext* vg = args.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 41 - af, ">", NULL);
}

// insert time signature
void TimeButton::onDragEnd(const event::DragEnd& e) {
    if (this->stageSlot(e)) {
        return;
    }
    auto ntw = this->ntw();
    auto& n = ntw->n();
    insertLoc = n.atMidiTime(n.notes[n.selectEnd].startTime);
    if (n.insertContains(insertLoc, TIME_SIGNATURE)) {
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
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, this->runAlpha()));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 33 - af, "4", NULL);
    nvgText(vg, 8 + af, 41 - af, "4", NULL);
}
