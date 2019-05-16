#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

struct DisplayBevel : Widget {
    DisplayBevel() {
        this->box.pos = Vec(RACK_GRID_WIDTH - 2, RACK_GRID_WIDTH * 2 - 2);
        this->box.size = Vec(RACK_GRID_WIDTH * 12 + 4, RACK_GRID_WIDTH * 9 + 4);
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgTranslate(vg, 2, 2);
        const float bevel = -2;
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, 0);
        nvgLineTo(vg, 0, box.size.y);
        nvgLineTo(vg, bevel, box.size.y - bevel);
        nvgLineTo(vg, bevel, bevel);
        nvgFillColor(vg, nvgRGB(0x6f, 0x6f, 0x6f));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, box.size.y);
        nvgLineTo(vg, box.size.x, box.size.y);
        nvgLineTo(vg, box.size.x - bevel, box.size.y - bevel);
        nvgLineTo(vg, bevel, box.size.y - bevel);
        nvgFillColor(vg, nvgRGB(0x9f, 0x9f, 0x9f));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, box.size.x, box.size.y);
        nvgLineTo(vg, box.size.x, 0);
        nvgLineTo(vg, box.size.x - bevel, bevel);
        nvgLineTo(vg, box.size.x - bevel, box.size.y - bevel);
        nvgFillColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, box.size.x, 0);
        nvgLineTo(vg, 0, 0);
        nvgLineTo(vg, bevel, bevel);
        nvgLineTo(vg, box.size.x - bevel, bevel);
        nvgFillColor(vg, nvgRGB(0x5f, 0x5f, 0x5f));
        nvgFill(vg);
    }

};

void NoteTakerWidget::addButton(NoteTakerButton* button) {
    button->mainWidget = this;
    params.push_back(button);
    ButtonBuffer* buffer = new ButtonBuffer(button);
    buffer->mainWidget = this;
    this->addChild(buffer);
}

void NoteTakerWidget::addWheel(NoteTakerWheel* wheel) {
    wheel->mainWidget = this;
    params.push_back(wheel);
    WheelBuffer* buffer = new WheelBuffer(wheel);
    buffer->mainWidget = this;
    this->addChild(buffer);
}

// move everything hanging off NoteTaker for inter-app communication and hang it off
// NoteTakerWidget instead, 
// make sure things like loading midi don't happen if module is null
NoteTakerWidget::NoteTakerWidget(NoteTaker* module) {
    this->setModule(module);
    this->setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteTaker.svg")));
    _musicFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/MusiSync3.ttf"));
    _textFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/leaguegothic-regular-webfont.ttf"));
    this->addChild(displayBuffer = new DisplayBuffer(Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2), // pos
                    Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9), this));  // size
    display = displayBuffer->getFirstDescendantOfType<NoteTakerDisplay>();
    assert(display);
    panel->addChild(new DisplayBevel());
    addWheel(horizontalWheel = createParam<HorizontalWheel>(
            Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
            module, NoteTaker::HORIZONTAL_WHEEL));
    // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
    addWheel(verticalWheel = createParam<VerticalWheel>(
            Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50),
            module, NoteTaker::VERTICAL_WHEEL));

    addInput(createInput<PJ301MPort>(Vec(140, 306), module, NoteTaker::V_OCT_INPUT));
    addInput(createInput<PJ301MPort>(Vec(172, 306), module, NoteTaker::CLOCK_INPUT));
    addInput(createInput<PJ301MPort>(Vec(204, 306), module, NoteTaker::RESET_INPUT));
    addOutput(createOutput<PJ301MPort>(Vec(172, 338), module, NoteTaker::CLOCK_OUTPUT));
    addOutput(createOutput<PJ301MPort>(Vec(204, 338), module, NoteTaker::EOS_OUTPUT));

    for (unsigned i = 0; i < CV_OUTPUTS; ++i) {
            addOutput(createOutput<PJ301MPort>(Vec(12 + i * 32, 306), module,
                    NoteTaker::CV1_OUTPUT + i));
            addOutput(createOutput<PJ301MPort>(Vec(12 + i * 32, 338), module,
                    NoteTaker::GATE1_OUTPUT + i));
    }

    addButton(runButton = createParam<RunButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON));
    addButton(selectButton = createParam<SelectButton>(Vec(30, 202), module, NoteTaker::EXTEND_BUTTON));
    addButton(insertButton = createParam<InsertButton>(Vec(62, 202), module, NoteTaker::INSERT_BUTTON));
    addButton(cutButton = createParam<CutButton>(Vec(94, 202), module, NoteTaker::CUT_BUTTON));
    addButton(restButton = createParam<RestButton>(Vec(126, 202), module, NoteTaker::REST_BUTTON));
    addButton(partButton = createParam<PartButton>(Vec(158, 202), module, NoteTaker::PART_BUTTON));
    addButton(fileButton = createParam<FileButton>(Vec(190, 202), module, NoteTaker::FILE_BUTTON));
    addButton(sustainButton = createParam<SustainButton>(Vec(30, 252), module, NoteTaker::SUSTAIN_BUTTON));
    addButton(timeButton = createParam<TimeButton>(Vec(62, 252), module, NoteTaker::TIME_BUTTON));
    addButton(keyButton = createParam<KeyButton>(Vec(94, 252), module, NoteTaker::KEY_BUTTON));
    addButton(tieButton = createParam<TieButton>(Vec(126, 252), module, NoteTaker::TIE_BUTTON));
    addButton(trillButton = createParam<TrillButton>(Vec(158, 252), module, NoteTaker::TRILL_BUTTON));
    addButton(tempoButton = createParam<TempoButton>(Vec(190, 252), module, NoteTaker::TEMPO_BUTTON));

    // debug button is hidden to the right of tempo
    addButton(dumpButton = createParam<DumpButton>(Vec(222, 252), module, NoteTaker::DUMP_BUTTON));

    if (module) {
        module->mainWidget = this;  // to do : is there a way to avoid this cross-dependency?
        module->onReset();
    }
}

// to do : once clipboard is set, don't reset unless:
// selection range was enlarged
// insert or cut
// 
void NoteTakerWidget::copyNotes() {
    if (!clipboardInvalid) {
        return;
    }
    auto& n = this->n();
    unsigned start = n.selectStart;
    // don't allow midi header on clipboard
    if (MIDI_HEADER == n.notes[n.selectStart].type) {
        ++start;
    }
    if (start < n.selectEnd) {
        assert(TRACK_END != n.notes[n.selectEnd - 1].type);
        clipboard.assign(n.notes.begin() + start, n.notes.begin() + n.selectEnd);
    }
    clipboardInvalid = false;
}

void NoteTakerWidget::copySelectableNotes() {
    if (!clipboardInvalid) {
        return;
    }
    clipboard.clear();
    auto& n = this->n();
    for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
        auto& note = n.notes[index];
        if (this->isSelectable(note)) {
            clipboard.push_back(note);
        }
    }
    clipboardInvalid = false;
}

void NoteTakerWidget::enableInsertSignature(unsigned loc) {
    auto& n = this->n();
    unsigned insertLoc = this->nt()->atMidiTime(n.notes[loc].startTime);
    std::pair<NoteTakerButton*,  DisplayType> pairs[] = {
            { (NoteTakerButton*) keyButton, KEY_SIGNATURE },
            { (NoteTakerButton*) timeButton, TIME_SIGNATURE },
            { (NoteTakerButton*) tempoButton, MIDI_TEMPO } };
    for (auto& pair : pairs) {
        pair.first->af = this->nt()->insertContains(insertLoc, pair.second);
    }
}

void NoteTakerWidget::eraseNotes(unsigned start, unsigned end) {
    if (debugVerbose) debug("eraseNotes start %u end %u", start, end);
    auto& n = this->n();
    for (auto iter = n.notes.begin() + end; iter-- != n.notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            n.notes.erase(iter);
        }
    }
    if (debugVerbose) this->debugDump(true, true);  // wheel range is inconsistent here
}

// for clipboard to be usable, either: all notes in clipboard are active, 
// or: all notes in clipboard are with a single channel (to paste to same or another channel)
// mono   locked
// false  false   copy all notes unchanged (return true)
// false  true    do nothing (return false)
// true   false   copy all notes unchanged (return true)
// true   true    copy all notes to unlocked channel (return true)
bool NoteTakerWidget::extractClipboard(vector<DisplayNote>* span) const {
    bool mono = true;
    bool locked = false;
    int channel = -1;
    for (auto& note : clipboard) {
        if (!note.isNoteOrRest()) {
            mono = false;
            locked |= selectChannels != ALL_CHANNELS;
            continue;
        }
        if (channel < 0) {
            channel = note.channel;
        } else {
            mono &= channel == note.channel;
        }
        locked |= !(selectChannels & (1 << note.channel));
    }
    if (!mono && locked) {
        return false;
    }
    *span = clipboard;
    if (locked) {
        NoteTaker::MapChannel(*span, this->unlockedChannel()); 
    }
    return true;
}

// to compute range for horizontal wheel when selecting notes
// to do : horizontalCount, noteToWheel, wheelToNote share loop logic. Consolidate?
unsigned NoteTakerWidget::horizontalCount() const {
    auto& n = this->n();
    unsigned count = 0;
    int lastStart = -1;
    for (auto& note : n.notes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
    }
    return count;
}

void NoteTakerWidget::insertFinal(int shiftTime, unsigned insertLoc, unsigned insertSize) {
    if (shiftTime) {
        this->shiftNotes(insertLoc + insertSize, shiftTime);
    } else {
        auto& n = this->n();
        NoteTaker::Sort(n.notes);
    }
    display->invalidateCache();
    display->displayEnd = 0;  // force recompute of display end
    nt()->setSelect(insertLoc, nt()->nextAfter(insertLoc, insertSize));
    if (debugVerbose) debug("insert final");
    this->setWheelRange();  // range is larger
    nt()->playSelection();
    if (debugVerbose) this->debugDump(true);
}

bool NoteTakerWidget::isEmpty() const {
    auto& n = this->n();
    for (auto& note : n.notes) {
        if (this->isSelectable(note)) {
            return false;
        }
    }
    return true;
}

bool NoteTakerWidget::isSelectable(const DisplayNote& note) const {
    return note.isSelectable(selectChannels);
}

void NoteTakerWidget::loadScore() {
    auto& n = this->n();
    unsigned index = (unsigned) horizontalWheel->getValue();
    assert(index < storage.size());
    NoteTakerParseMidi parser(storage[index], n.notes, nt()->channels, n.ppq);
    if (debugVerbose) NoteTaker::DebugDumpRawMidi(storage[index]);
    if (!parser.parseMidi()) {
        nt()->setScoreEmpty();
    }
    display->resetXAxisOffset();
    nt()->resetRun();
    display->invalidateCache();
    unsigned atZero = nt()->atMidiTime(0);
    atZero -= TRACK_END == n.notes[atZero].type;
    nt()->setSelectStart(atZero);
}

// to do : allow for triplet + slurred ?
// to do : if triplet, make normal
// to do : if triplet, start UI selection on triplet
void NoteTakerWidget::makeNormal() {
    // to do : if notes are slurred (missing note off) or odd timed (triplets) restore
    auto& n = this->n();
    for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
        DisplayNote& note = n.notes[index];
        if (note.isNoteOrRest() && note.isSelectable(selectChannels)) {
            note.setSlur(false);
        }
    }
}

// allows two or more notes, or two or more rests: to be tied
// repeats for each channel, to allow multiple channels to be selected and slurred at once
void NoteTakerWidget::makeSlur() {
    // to do: disallow choosing slur in UI if there isn't at least one pair.
    // For now, avoid singletons and non-notes
    unsigned start = -1;
    unsigned count = 0;
    int channel = -1;  // active channel to be slurred
    DisplayType type = UNUSED;
    array<unsigned, CHANNEL_COUNT> channels;  // track end of slurred notes
    channels.fill(0);
    auto& n = this->n();
    for (unsigned index = n.selectStart; index <= n.selectEnd; ++index) {  // note that this goes 1 past
        if (index != n.selectEnd) {
            auto& note = n.notes[index];
            if (index < channels[note.channel]) {  // skip if channel has already been slurred
                continue;
            }
            if (UNUSED == type && note.isNoteOrRest()) {   // allow pairing notes or rests
                type = note.type;
            }
            if (type == note.type) {
                if (note.isSelectable(selectChannels)) {
                    if ((unsigned) -1 == start) {
                        start = index;
                        channel = note.channel;
                        count = 1;
                    } else if (channel == note.channel) {
                        ++count;
                    }
                }
                continue;
            }
        }
        if ((unsigned) -1 == start || count < 2) {
            continue;
        }
        for (unsigned inner = start; inner < index; ++inner) {
            auto& iNote = n.notes[inner];
            if (type == iNote.type && channel == iNote.channel) {
                iNote.setSlur(true);
            }
        }
        channels[channel] = index;
        index = start + 1;
        start = -1;
        count = 0;
        channel = -1;
        type = UNUSED;
    }
}

// if current selection includes durations not in std note durations, do nothing
// the assumption is that in this case, durations have already been tupletized 
void NoteTakerWidget::makeTuplet() {
    auto& n = this->n();
    int smallest = NoteDurations::ToMidi(NoteDurations::Count() - 1, n.ppq);
    int beats = 0;
    auto addBeat = [&](int duration) {
        if (NoteDurations::Closest(duration, n.ppq) != duration) {
            debug("can't tuple nonstandard duration %d ppq %d", duration, n.ppq);
            return false;
        }
        int divisor = (int) gcd(smallest, duration);
        if (NoteDurations::Closest(divisor, n.ppq) != divisor) {
            debug("can't tuple gcd: smallest %d divisor %d ppq %d", smallest, divisor, n.ppq);
            return false;
        }
        if (divisor < smallest) {
            beats *= smallest / divisor;
            smallest = divisor;
        }
        debug("divisor %d smallest %d", divisor, smallest);
        beats += duration / smallest;
        return true;
    };
    // count number of beats or beat fractions, find smallest factor
    int last = 0;
    for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
        const DisplayNote& note = n.notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        int restDuration = last - note.startTime;
        if (restDuration > 0) {    // implied rest
            debug("add implied rest %d", restDuration);
            if (!addBeat(restDuration)) {
                return;
            }
        }
        debug("add beat %s", note.debugString().c_str());
        if (!addBeat(note.duration)) {
            return;
        }
        last = note.endTime();
    }
    debug("beats : %d", beats);
    if (beats % 3) {
        return;   // to do : only support triplets for now
    }
    // to do : to support 5-tuple, ppq must be a multiple of 5
    //       : to support 7-tuple, ppq must be a multiple of 7
    // to do : general tuple rules are complicated, so only implement a few simple ones
    // to do : respect time signature (ignored for now)
    int adjustment = 0;
    for (int tuple : { 3, 5, 7 } ) {
        // 3 in time of 2, 5 in time of 4, 7 in time of 4
        if (beats % tuple) {
            continue;
        }
        last = 0;
        int factor = 3 == tuple ? 1 : 3;
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            DisplayNote& note = n.notes[index];
            if (!note.isSelectable(selectChannels)) {
                continue;
            }
            int restDuration = last - note.startTime;
            if (restDuration > 0) {
                adjustment -= restDuration / tuple;
            }
            note.startTime += adjustment;
            int delta = note.duration * factor / tuple;
            adjustment -= delta;
            note.duration -= delta;
            last = note.endTime();
        }
        break;
    }
    this->shiftNotes(n.selectEnd, adjustment);
    display->invalidateCache();
}

// true if a button that brings up a secondary menu on display is active
bool NoteTakerWidget::menuButtonOn() const {
    return fileButton->ledOn || partButton->ledOn || sustainButton->ledOn || tieButton->ledOn;
}

int NoteTakerWidget::nextStartTime(unsigned start) const {
    auto& n = this->n();
    for (unsigned index = start; index < n.notes.size(); ++index) {
        const DisplayNote& note = n.notes[index];
        if (this->isSelectable(note)) {
            return note.startTime;
        }
    }
    return n.notes.back().startTime;
}

// to do : need only early exit if result > 0 ?
int NoteTakerWidget::noteCount() const {
    auto& n = this->n();
    int result = 0;
    for (auto& note : n.notes) {
        if (NOTE_ON == note.type && note.isSelectable(selectChannels)) {
            ++result;
        }
    }
    return result;
}

int NoteTakerWidget::noteToWheel(unsigned index, bool dbug) const {
    auto& n = this->n();
    assert(index < n.notes.size());
    return this->noteToWheel(n.notes[index], dbug);
}

int NoteTakerWidget::noteToWheel(const DisplayNote& match, bool dbug) const {
    auto& n = this->n();
    int count = 0;
    int lastStart = -1;
    for (auto& note : n.notes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (&match == &note) {
            return count + (TRACK_END == match.type);
        }
    }
    if (MIDI_HEADER == match.type) {
        return -1;
    }
    if (dbug) {
        debug("noteToWheel match %s", match.debugString().c_str());
        debugDump(false, true);
        assert(0);
    }
    return -1;
}

const Notes& NoteTakerWidget::n() const {
    return nt()->n;
}

Notes& NoteTakerWidget::n() {
    return nt()->n;
}

NoteTaker* NoteTakerWidget::nt() {
    return dynamic_cast<NoteTaker* >(module);
}

const NoteTaker* NoteTakerWidget::nt() const {
    return dynamic_cast<const NoteTaker* >(module);
}

bool NoteTakerWidget::resetControls() {
    this->turnOffLedButtons();
    for (NoteTakerButton* button : {
            (NoteTakerButton*) cutButton,
            (NoteTakerButton*) insertButton,
            (NoteTakerButton*) restButton,
            (NoteTakerButton*) selectButton,
            (NoteTakerButton*) timeButton }) {
        button->reset();
    }
//    partButton->resetChannels();
    horizontalWheel->reset();
    verticalWheel->reset();
    this->setWheelRange();
    display->reset();
    return true;
}

void NoteTakerWidget::resetRun() {
    display->displayStart = display->displayEnd = 0;
    display->rangeInvalid = true;
}

void NoteTakerWidget::resetState() {
    clipboard.clear();
    display->resetXAxisOffset();
    selectChannels = ALL_CHANNELS;
    this->resetControls();
}

bool NoteTakerWidget::runningWithButtonsOff() const {
    return runButton->ledOn && !this->menuButtonOn();
}

void NoteTakerWidget::saveScore() {
    unsigned index = (unsigned) horizontalWheel->getValue();
    assert(index <= storage.size());
    if (storage.size() == index) {
        storage.push_back(vector<uint8_t>());
    }
    auto& dest = storage[index];
    NoteTakerMakeMidi midiMaker;
    midiMaker.createFromNotes(*this->nt(), dest);
    this->writeStorage(index);
}

void NoteTakerWidget::setSelectableScoreEmpty() {
    auto& n = this->n();
    auto iter = n.notes.begin();
    while (iter != n.notes.end()) {
        if (this->isSelectable(*iter)) {
            iter = n.notes.erase(iter);
        } else {
            ++iter;
        }
    }
    display->invalidateCache();
    display->displayEnd = 0;  // force recompute of display end
    nt()->setSelect(0, 1);
    this->setWheelRange();
}

void NoteTakerWidget::shiftNotes(unsigned start, int diff) {
    auto& n = this->n();
    if (debugVerbose) debug("shiftNotes start %u diff %d selectChannels 0x%02x", start, diff, selectChannels);
    NoteTaker::ShiftNotes(n.notes, start, diff, selectChannels);
    NoteTaker::Sort(n.notes);
}

// never turns off select button, since it cannot be turned off if score is empty,
// and it contains state that should not be arbitrarily deleted. select button
// is explicitly reset only when notetaker overall state is reset
void NoteTakerWidget::turnOffLedButtons(const NoteTakerButton* exceptFor) {
    for (NoteTakerButton* button : {
                (NoteTakerButton*) fileButton,
                (NoteTakerButton*) partButton,
                (NoteTakerButton*) runButton,
                (NoteTakerButton*) sustainButton,
                (NoteTakerButton*) tieButton }) {
        if (exceptFor != button) {
            button->onTurnOff();
        }
    }
}


// counts to nth selectable note; returns index into notes array
unsigned NoteTakerWidget::wheelToNote(int value, bool dbug) const {
    if (!value) {
        return 0;  // midi header
    }
    if (!dbug && value < 0) {
        debug("! didn't expect < 0 : value: %d", value);
        return 0;
    }
    auto& n = this->n();
    assert(value > 0);
    assert(value < (int) n.notes.size());
    int count = value - 1;
    int lastStart = -1;
    for (auto& note : n.notes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            if (--count < 0) {
                return nt()->noteIndex(note);
            }
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (TRACK_END == note.type) {
            if (count > 0) {
                debug("! expected 0 wheelToNote value at track end; value: %d", value);
                assert(!dbug);                
            }
            return n.notes.size() - 1;
        }
    }
    debug("! out of range wheelToNote value %d", value);
    if (dbug) {
        this->debugDump(false, true);
        assert(0);  // probably means wheel range is larger than selectable note count
    }
    return (unsigned) -1;
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per pluginInstance, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = createModel<NoteTaker, NoteTakerWidget>("NoteTaker");
