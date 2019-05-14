#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"

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

// move everything hanging off NoteTaker for inter-app communication and hang it off
// NoteTakerWidget instead, 
// make sure things like loading midi don't happen if module is null
NoteTakerWidget::NoteTakerWidget(NoteTaker* module) {
    this->setModule(module);
    this->setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteTaker.svg")));
    std::string musicFontName = asset::plugin(pluginInstance, "res/MusiSync3.ttf");
    musicFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/MusiSync3.ttf"));
    std::string textFontName = asset::plugin(pluginInstance, "res/leaguegothic-regular-webfont.ttf");
    this->addChild(new NoteTakerDisplay(Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2), // pos
                    Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9),  // size
                    musicFontName, textFontName));
    panel->addChild(new DisplayBevel());
    addParam(createParam<HorizontalWheel>(
            Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
            module, NoteTaker::HORIZONTAL_WHEEL));
    // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
    addParam(createParam<VerticalWheel>(
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

    addParam(createParam<RunButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON));
    addParam(createParam<SelectButton>(Vec(30, 202), module, NoteTaker::EXTEND_BUTTON));
    addParam(createParam<InsertButton>(Vec(62, 202), module, NoteTaker::INSERT_BUTTON));
    addParam(createParam<CutButton>(Vec(94, 202), module, NoteTaker::CUT_BUTTON));
    addParam(createParam<RestButton>(Vec(126, 202), module, NoteTaker::REST_BUTTON));
    addParam(createParam<PartButton>(Vec(158, 202), module, NoteTaker::PART_BUTTON));
    addParam(createParam<FileButton>(Vec(190, 202), module, NoteTaker::FILE_BUTTON));
    addParam(createParam<SustainButton>(Vec(30, 252), module, NoteTaker::SUSTAIN_BUTTON));
    addParam(createParam<TimeButton>(Vec(62, 252), module, NoteTaker::TIME_BUTTON));
    addParam(createParam<KeyButton>(Vec(94, 252), module, NoteTaker::KEY_BUTTON));
    addParam(createParam<TieButton>(Vec(126, 252), module, NoteTaker::TIE_BUTTON));
    addParam(createParam<TrillButton>(Vec(158, 252), module, NoteTaker::TRILL_BUTTON));
    addParam(createParam<TempoButton>(Vec(190, 252), module, NoteTaker::TEMPO_BUTTON));

    // debug button is hidden to the right of tempo
    addParam(createParam<DumpButton>(Vec(222, 252), module, NoteTaker::TEMPO_BUTTON));
}

// to do : once clipboard is set, don't reset unless:
// selection range was enlarged
// insert or cut
// 
void NoteTakerWidget::copyNotes() {
    if (!clipboardInvalid) {
        return;
    }
    unsigned start = nt()->selectStart;
    // don't allow midi header on clipboard
    if (MIDI_HEADER == nt()->notes[nt()->selectStart].type) {
        ++start;
    }
    if (start < nt()->selectEnd) {
        assert(TRACK_END != nt()->notes[nt()->selectEnd - 1].type);
        clipboard.assign(nt()->notes.begin() + start, nt()->notes.begin() + nt()->selectEnd);
    }
    clipboardInvalid = false;
}

void NoteTakerWidget::copySelectableNotes() {
    if (!clipboardInvalid) {
        return;
    }
    clipboard.clear();
    for (unsigned index = nt()->selectStart; index < nt()->selectEnd; ++index) {
        auto& note = nt()->notes[index];
        if (this->isSelectable(note)) {
            clipboard.push_back(note);
        }
    }
    clipboardInvalid = false;
}

void NoteTakerWidget::enableInsertSignature(unsigned loc) {
    unsigned insertLoc = this->nt()->atMidiTime(nt()->notes[loc].startTime);
    std::pair<NoteTakerButton*,  DisplayType> pairs[] = {
            { (NoteTakerButton*) this->widget<KeyButton>(), KEY_SIGNATURE },
            { (NoteTakerButton*) this->widget<TimeButton>(), TIME_SIGNATURE },
            { (NoteTakerButton*) this->widget<TempoButton>(), MIDI_TEMPO } };
    for (auto& pair : pairs) {
        pair.first->af = this->nt()->insertContains(insertLoc, pair.second);
    }
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
    unsigned count = 0;
    int lastStart = -1;
    for (auto& note : nt()->notes) {
        if (this->isSelectable(note) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
    }
    return count;
}

bool NoteTakerWidget::isEmpty() const {
    for (auto& note : nt()->notes) {
        if (this->isSelectable(note)) {
            return false;
        }
    }
    return true;
}

bool NoteTakerWidget::isSelectable(const DisplayNote& note) const {
    return note.isSelectable(selectChannels);
}

// true if a button that brings up a secondary menu on display is active
bool NoteTakerWidget::menuButtonOn() const {
    return this->widget<FileButton>()->ledOn
            || this->widget<PartButton>()->ledOn
            || this->widget<SustainButton>()->ledOn
            || this->widget<TieButton>()->ledOn;
}

int NoteTakerWidget::nextStartTime(unsigned start) const {
    for (unsigned index = start; index < nt()->notes.size(); ++index) {
        const DisplayNote& note = nt()->notes[index];
        if (this->isSelectable(note)) {
            return note.startTime;
        }
    }
    return nt()->notes.back().startTime;
}

// to do : need only early exit if result > 0 ?
int NoteTakerWidget::noteCount() const {
    int result = 0;
    for (auto& note : nt()->notes) {
        if (NOTE_ON == note.type && note.isSelectable(selectChannels)) {
            ++result;
        }
    }
    return result;
}

bool NoteTakerWidget::resetControls() {
    this->turnOffLedButtons();
    for (NoteTakerButton* button : {
            (NoteTakerButton*) this->widget<CutButton>(),
            (NoteTakerButton*) this->widget<InsertButton>(),
            (NoteTakerButton*) this->widget<RestButton>(),
            (NoteTakerButton*) this->widget<SelectButton>(),
            (NoteTakerButton*) this->widget<TimeButton>() }) {
        button->reset();
    }
//    partButton->resetChannels();
    this->widget<HorizontalWheel>()->reset();
    this->widget<VerticalWheel>()->reset();
    this->setWheelRange();
    this->widget<NoteTakerDisplay>()->reset();
    return true;
}

bool NoteTakerWidget::runningWithButtonsOff() const {
    return this->widget<RunButton>()->ledOn && !this->menuButtonOn();
}

 //   to do
    // think about whether : the whole selection fits on the screen
    // the last note fits on the screen if increasing the end
    // the first note fits on the screen if decreasing start
        // scroll small selections to center?
        // move larger selections the smallest amount?
        // prefer to show start? end?
        // while playing, scroll a 'page' at a time?
void NoteTakerWidget::setSelect(unsigned start, unsigned end) {
    auto display = this->widget<NoteTakerDisplay>();
    if (this->isEmpty()) {
        nt()->selectStart = 0;
        nt()->selectEnd = 1;
        display->setRange();
        display->fb->dirty = true;
        if (debugVerbose) debug("setSelect set empty");
        return;
    }
    assert(start < end);
    assert(nt()->notes.size() >= 2);
    assert(end <= nt()->notes.size() - 1);
    display->setRange();
    this->enableInsertSignature(end);  // disable buttons that already have signatures in score
    if (debugVerbose) debug("setSelect old %u %u new %u %u", nt()->selectStart, nt()->selectEnd, start, end);
    nt()->selectStart = start;
    nt()->selectEnd = end;
    display->fb->dirty = true;
}

void NoteTakerWidget::shiftNotes(unsigned start, int diff) {
    if (debugVerbose) debug("shiftNotes start %u diff %d selectChannels 0x%02x", start, diff, selectChannels);
    NoteTaker::ShiftNotes(nt()->notes, start, diff, selectChannels);
    NoteTaker::Sort(nt()->notes);
}

// never turns off select button, since it cannot be turned off if score is empty,
// and it contains state that should not be arbitrarily deleted. select button
// is explicitly reset only when notetaker overall state is reset
void NoteTakerWidget::turnOffLedButtons(const NoteTakerButton* exceptFor) {
    for (NoteTakerButton* button : {
                (NoteTakerButton*) this->widget<FileButton>(),
                (NoteTakerButton*) this->widget<PartButton>(),
                (NoteTakerButton*) this->widget<RunButton>(),
                (NoteTakerButton*) this->widget<SustainButton>(),
                (NoteTakerButton*) this->widget<TieButton>() }) {
        if (exceptFor != button) {
            button->onTurnOff();
        }
    }
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per pluginInstance, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = createModel<NoteTaker, NoteTakerWidget>("NoteTaker");
