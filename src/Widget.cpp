#include "Button.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

struct ClipboardLabel : Widget {
    NoteTakerWidget* mainWidget;

    ClipboardLabel(NoteTakerWidget* _ntw, Vec& displayPos, Vec& displaySize)
        : mainWidget(_ntw) {
        this->box.pos = displayPos;
        this->box.size = displaySize;
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, ntw()->musicFont());
        nvgFillColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgFontSize(vg, 32);
        nvgText(vg, 0, 0, "/", NULL);
    }

    NoteTakerWidget* ntw() {
        return mainWidget;
    }
};

struct ClipboardLight : GrayModuleLightWidget {
	ClipboardLight() {
		addBaseColor(nvgRGB(0xF7, 0x37, 0x17));
	}

    void draw(const DrawArgs& args) override {
        GrayModuleLightWidget::draw(args);
        this->drawHalo(args);   // draw extra halo to make it more pronounced
    }
};

struct DisplayBevel : Widget {
    const float bevel = -2;

    DisplayBevel(Vec& displayPos, Vec& displaySize) {
        this->box.pos = Vec(displayPos.x + bevel, displayPos.y + bevel);
        this->box.size = Vec(displaySize.x, displaySize.y);
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgTranslate(vg, -bevel, -bevel);
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
NoteTakerWidget::NoteTakerWidget(NoteTaker* module) 
    : storage(DEBUG_VERBOSE) {
    clipboard.debugVerbose = DEBUG_VERBOSE;
    this->setModule(module);
    this->setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteTaker.svg")));
    _musicFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/MusiSync3.ttf"));
    _textFont = APP->window->loadFont(asset::plugin(pluginInstance, "res/leaguegothic-regular-webfont.ttf"));
    Vec displayPos = Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2);
    Vec displaySize = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9);
    this->addChild(displayBuffer = new DisplayBuffer(displayPos, displaySize, this));
    display = displayBuffer->getFirstDescendantOfType<NoteTakerDisplay>();
    SCHMICKLE(display);
    panel->addChild(new DisplayBevel(displayPos, displaySize));
    Vec clipboardPos = Vec(RACK_GRID_WIDTH * 1.25f, RACK_GRID_WIDTH * 12.5f);
    Vec clipboardSize = Vec(16, 24);
    panel->addChild(new ClipboardLabel(this, clipboardPos, clipboardSize));
    this->addChild(createLight<SmallLight<ClipboardLight>>(Vec(RACK_GRID_WIDTH * 1.25f + 16,
            RACK_GRID_WIDTH * 11.8f), module, NoteTaker::CLIPBOARD_ON_LIGHT));
    if (module) {
        module->configParam<RunButtonToolTip>(NoteTaker::RUN_BUTTON, 0, 1, 0, "Play");
        module->configParam<SelectButtonToolTip>(NoteTaker::EXTEND_BUTTON, 0, 2, 0, "Notes");
        module->configParam<InsertButtonToolTip>(NoteTaker::INSERT_BUTTON, 0, 1, 0, "Insert");
        module->configParam<CutButtonToolTip>(NoteTaker::CUT_BUTTON, 0, 1, 0, "Cut");
        module->configParam<RestButtonToolTip>(NoteTaker::REST_BUTTON, 0, 1, 0, "Insert");
        module->configParam<PartButtonToolTip>(NoteTaker::PART_BUTTON, 0, 1, 0, "Part");
        module->configParam<FileButtonToolTip>(NoteTaker::FILE_BUTTON, 0, 1, 0, "Load");
        module->configParam<SustainButtonToolTip>(NoteTaker::SUSTAIN_BUTTON, 0, 1, 0, "Edit");
        module->configParam<TimeButtonToolTip>(NoteTaker::TIME_BUTTON, 0, 1, 0, "Insert");
        module->configParam<KeyButtonToolTip>(NoteTaker::KEY_BUTTON, 0, 1, 0, "Insert");
        module->configParam<TieButtonToolTip>(NoteTaker::TIE_BUTTON, 0, 1, 0, "Add");
        module->configParam<TrillButtonToolTip>(NoteTaker::TRILL_BUTTON, 0, 1, 0, "Unimplemented");
        module->configParam<TempoButtonToolTip>(NoteTaker::TEMPO_BUTTON, 0, 1, 0, "Insert");
        module->configParam(NoteTaker::HORIZONTAL_WHEEL, 0, 1, 0, "Time Wheel");
        module->configParam(NoteTaker::VERTICAL_WHEEL, 0, 1, 0, "Pitch Wheel");
        module->mainWidget = this;  // to do : is there a way to avoid this cross-dependency?
        module->slot = &storage.slots.front();
        module->debugVerbose = debugVerbose;
    }
    Vec hWheelPos = Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f);
    Vec hWheelSize = Vec(100, 23);
    addWheel(hWheelSize, (horizontalWheel = createParam<HorizontalWheel>(
            hWheelPos, module, NoteTaker::HORIZONTAL_WHEEL)));
    // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
    Vec vWheelPos = Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50);
    Vec vWheelSize = Vec(23, 100);
    addWheel(vWheelSize, (verticalWheel = createParam<VerticalWheel>(
            vWheelPos, module, NoteTaker::VERTICAL_WHEEL)));

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
    Vec editButtonSize = Vec(22, 43);
    Vec runButtonSize = Vec(24, 24);
    addButton(runButtonSize, (runButton = 
            createParam<RunButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON)));
    addButton(editButtonSize, (selectButton =
            createParam<SelectButton>(Vec(30, 202), module, NoteTaker::EXTEND_BUTTON)));
    addButton(editButtonSize, (insertButton =
            createParam<InsertButton>(Vec(62, 202), module, NoteTaker::INSERT_BUTTON)));
    if (insertButton->paramQuantity) {
        ((InsertButtonToolTip*) insertButton->paramQuantity)->button = insertButton;
    }
    addButton(editButtonSize, (cutButton = 
            createParam<CutButton>(Vec(94, 202), module, NoteTaker::CUT_BUTTON)));
    if (cutButton->paramQuantity) {
        ((CutButtonToolTip*) cutButton->paramQuantity)->button = cutButton;
    }
    addButton(editButtonSize, (restButton = 
            createParam<RestButton>(Vec(126, 202), module, NoteTaker::REST_BUTTON)));
    addButton(editButtonSize, (partButton = 
            createParam<PartButton>(Vec(158, 202), module, NoteTaker::PART_BUTTON)));
    addButton(editButtonSize, (fileButton = 
            createParam<FileButton>(Vec(190, 202), module, NoteTaker::FILE_BUTTON)));
    addButton(editButtonSize, (sustainButton = 
            createParam<SustainButton>(Vec(30, 252), module, NoteTaker::SUSTAIN_BUTTON)));
    addButton(editButtonSize, (timeButton = 
            createParam<TimeButton>(Vec(62, 252), module, NoteTaker::TIME_BUTTON)));
    addButton(editButtonSize, (keyButton = 
            createParam<KeyButton>(Vec(94, 252), module, NoteTaker::KEY_BUTTON)));
    addButton(editButtonSize, (tieButton = 
            createParam<TieButton>(Vec(126, 252), module, NoteTaker::TIE_BUTTON)));
    addButton(editButtonSize, (trillButton = 
            createParam<TrillButton>(Vec(158, 252), module, NoteTaker::TRILL_BUTTON)));
    addButton(editButtonSize, (tempoButton = 
            createParam<TempoButton>(Vec(190, 252), module, NoteTaker::TEMPO_BUTTON)));

    // debug button is hidden to the right of tempo
    addButton(editButtonSize, (dumpButton = 
            createParam<DumpButton>(Vec(222, 252), module, NoteTaker::DUMP_BUTTON)));

    if (module) {
        module->onReset();
    }
}

NoteTakerSlot* NoteTakerWidget::activeSlot() {
    if (fileButton->ledOn()) {
        unsigned index = (unsigned) horizontalWheel->getValue();
        SCHMICKLE(index < storage.size());
        return &storage.slots[index];
    }
    return this->nt()->slot;
}

void NoteTakerWidget::addButton(const Vec& size, NoteTakerButton* button) {
    button->box.size = size;
    params.push_back(button);
    ButtonBuffer* buffer = new ButtonBuffer(button);
    this->addChild(buffer);
}

void NoteTakerWidget::addWheel(const Vec& size, NoteTakerWheel* wheel) {
    wheel->box.size = size;
    wheel->mainWidget = this;
    params.push_back(wheel);
    WheelBuffer* buffer = new WheelBuffer(wheel);
    this->addChild(buffer);
}

struct NoteTakerMidiItem : MenuItem {
	NoteTakerWidget* widget;
    std::string directory;

	void onAction(const event::Action& e) override {
        NoteTakerSlot* slot = widget->activeSlot();
        slot->directory = directory;
        slot->filename = text;
        slot->setFromMidi();
        widget->resetScore();
	}
};

// matches dot mid and dot midi, case insensitive
static bool endsWithMid(const std::string& str) {
    if (str.size() < 5) {
        return false;
    }
    const char* last = &str.back();
    if ('i' == tolower(*last)) {
        --last;
    }
    const char dotMid[] = ".mid";
    const char* test = last - 3;
    for (int index = 0; index < 3; ++index) {
        if (dotMid[index] != tolower(test[index])) {
            return false;
        }
    }
    return true;
}

struct NoteTakerLoadItem : MenuItem {
	NoteTakerWidget* widget;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
        for (auto& dir : { asset::user("/"), SlotArray::UserDirectory() }) {
            std::list<std::string> entries(system::getEntries(dir));
            for (const auto& entry : entries) {
                if (!endsWithMid(entry)) {
                    continue;
                }
                size_t lastSlash = entry.rfind('/');
                std::string filename = std::string::npos == lastSlash ? entry : entry.substr(lastSlash + 1);
                NoteTakerMidiItem *item = new NoteTakerMidiItem;
                item->text = filename;
                item->widget = widget;
                item->directory = dir;
                menu->addChild(item);
            }
        }
		return menu;
	}
};

struct NoteTakerSaveAsMidi : ui::TextField {
	NoteTakerWidget* widget;

	void step() override {
		// Keep selected
		APP->event->setSelected(this);
		TextField::step();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
            auto slot = widget->activeSlot();
            slot->directory = SlotArray::UserDirectory();
            slot->filename = text;
            if (!endsWithMid(slot->filename)) {
                slot->filename += ".mid";
            }
            slot->writeToMidi();
			ui::MenuOverlay *overlay = getAncestorOfType<ui::MenuOverlay>();
			overlay->requestDelete();
			e.consume(this);
        }
		if (!e.getTarget()) {
			TextField::onSelectKey(e);
        }
	}

};

struct NoteTakerSaveItem : MenuItem {
	NoteTakerWidget* widget;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
        NoteTakerSaveAsMidi *saveAsMidi = new NoteTakerSaveAsMidi;
        saveAsMidi->box.size.x = 100;
        saveAsMidi->widget = widget;
        menu->addChild(saveAsMidi);
		return menu;
	}
};

void NoteTakerWidget::appendContextMenu(Menu *menu) {
    menu->addChild(new MenuEntry);
    NoteTakerLoadItem *loadItem = new NoteTakerLoadItem;
    loadItem->text = "Load MIDI";
    loadItem->rightText = RIGHT_ARROW;
    loadItem->widget = this;
    menu->addChild(loadItem);
    NoteTakerSaveItem *saveItem = new NoteTakerSaveItem;
    saveItem->text = "Save as MIDI";
    saveItem->rightText = RIGHT_ARROW;
    saveItem->widget = this;
    menu->addChild(saveItem);
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
        SCHMICKLE(TRACK_END != n.notes[n.selectEnd - 1].type);
        clipboard.notes.assign(n.notes.begin() + start, n.notes.begin() + n.selectEnd);
    }
    for (auto& note : clipboard.notes) {
        note.cache = nullptr;
    }
    clipboardInvalid = false;
    this->setClipboardLight();
}

void NoteTakerWidget::copySelectableNotes() {
    if (!clipboardInvalid) {
        return;
    }
    clipboard.notes.clear();
    auto& n = this->n();
    for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
        auto note = n.notes[index];
        if (note.isSelectable(selectChannels)) {
            note.cache = nullptr;
            clipboard.notes.push_back(std::move(note));
        }
    }
    clipboardInvalid = false;
    this->setClipboardLight();
}

void NoteTakerWidget::copyToSlot(unsigned index) {
    SCHMICKLE(index < storage.size());
    NoteTakerSlot* source = this->nt()->slot;
    NoteTakerSlot* dest = &storage.slots[index];
    *dest = *source;
}

bool NoteTakerWidget::displayUI_on() const {
    return partButton->ledOn() || fileButton->ledOn() || sustainButton->ledOn() || tieButton->ledOn();
}

void NoteTakerWidget::enableInsertSignature(unsigned loc) {
    auto& n = this->n();
    unsigned insertLoc = this->nt()->atMidiTime(n.notes[loc].startTime);
    std::pair<NoteTakerButton*,  DisplayType> pairs[] = {
            { (NoteTakerButton*) keyButton, KEY_SIGNATURE },
            { (NoteTakerButton*) timeButton, TIME_SIGNATURE },
            { (NoteTakerButton*) tempoButton, MIDI_TEMPO } };
    for (auto& pair : pairs) {
        pair.first->animationFrame = (int) this->nt()->insertContains(insertLoc, pair.second);
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
    for (auto& note : clipboard.notes) {
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
    if (span) {
        *span = clipboard.notes;
        if (locked) {
            NoteTaker::MapChannel(*span, this->unlockedChannel()); 
        }
    }
    return true;
}

void NoteTakerWidget::insertFinal(int shiftTime, unsigned insertLoc, unsigned insertSize) {
    if (shiftTime) {
        this->shiftNotes(insertLoc + insertSize, shiftTime);
    } else {
        auto& n = this->n();
        NoteTaker::Sort(n.notes);
    }
    display->displayEnd = 0;  // force recompute of display end
    nt()->setSelect(insertLoc, nt()->nextAfter(insertLoc, insertSize));
    if (debugVerbose) DEBUG("insert final");
    this->setWheelRange();  // range is larger
    this->invalidateAndPlay(Inval::note);
    if (debugVerbose) this->debugDump(true);
}

void NoteTakerWidget::invalidateAndPlay(Inval inval) {
    DEBUG("invalidateAndPlay %s", InvalDebugStr(inval).c_str());
    if (Inval::none == inval) {
        return;
    }
    auto nt = this->nt();
    display->invalidateRange();
    if (Inval::display != inval) {
        display->invalidateCache();
        if (nt && (Inval::note == inval || Inval::load == inval)) {
            nt->invalidVoiceCount = true;
            DEBUG("invalidVoiceCount true");
        }
    }
    if (!nt) {
        return;
    }
    if (Inval::load == inval) {
        nt->setVoiceCount();
        nt->setOutputsVoiceCount();

    } else if (Inval::cut != inval) {
        nt->playSelection();
    }
}

void NoteTakerWidget::loadScore() {
    unsigned slot = (unsigned) horizontalWheel->getValue();
    SCHMICKLE(slot < storage.size());
    nt()->setSlot(slot);
    this->resetScore();
}

void NoteTakerWidget::resetScore() {
    display->resetXAxisOffset();
    nt()->resetRun();
    this->invalidateAndPlay(Inval::load);
    unsigned atZero = nt()->atMidiTime(0);
    atZero -= TRACK_END == nt()->slot->n.notes[atZero].type;
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
        if (NoteDurations::LtOrEq(duration, n.ppq) != duration) {
            DEBUG("can't tuple nonstandard duration %d ppq %d", duration, n.ppq);
            return false;
        }
        int divisor = (int) gcd(smallest, duration);
        if (NoteDurations::LtOrEq(divisor, n.ppq) != divisor) {
            DEBUG("can't tuple gcd: smallest %d divisor %d ppq %d", smallest, divisor, n.ppq);
            return false;
        }
        if (divisor < smallest) {
            beats *= smallest / divisor;
            smallest = divisor;
        }
        DEBUG("divisor %d smallest %d", divisor, smallest);
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
            DEBUG("add implied rest %d", restDuration);
            if (!addBeat(restDuration)) {
                return;
            }
        }
        DEBUG("add beat %s", note.debugString().c_str());
        if (!addBeat(note.duration)) {
            return;
        }
        last = note.endTime();
    }
    DEBUG("beats : %d", beats);
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
    this->invalidateAndPlay(Inval::change);
}

// true if a button that brings up a secondary menu on display is active
bool NoteTakerWidget::menuButtonOn() const {
    return fileButton->ledOn() || partButton->ledOn() || sustainButton->ledOn() || tieButton->ledOn();
}

DisplayNote NoteTakerWidget::middleC() const {
    auto& n = this->n();
    DisplayNote midC(NOTE_ON, 0, n.ppq, (uint8_t) this->unlockedChannel());
    midC.setPitch(60);  // to do : const for middle C
    return midC;
}

int NoteTakerWidget::nextStartTime(unsigned start) const {
    auto& n = this->n();
    for (unsigned index = start; index < n.notes.size(); ++index) {
        const DisplayNote& note = n.notes[index];
        if (note.isSelectable(selectChannels)) {
            return note.startTime;
        }
    }
    return n.notes.back().startTime;
}

int NoteTakerWidget::noteToWheel(unsigned index, bool dbug) const {
    auto& n = this->n();
    SCHMICKLE(index < n.notes.size());
    return this->noteToWheel(n.notes[index], dbug);
}

int NoteTakerWidget::noteToWheel(const DisplayNote& match, bool dbug) const {
    auto& n = this->n();
    int count = 0;
    int lastStart = -1;
    for (auto& note : n.notes) {
        if (note.isSelectable(selectChannels) && lastStart != note.startTime) {
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
        DEBUG("noteToWheel match %s", match.debugString().c_str());
        debugDump(false, true);
        _schmickled();
    }
    return -1;
}

const Notes& NoteTakerWidget::n() const {
    return nt()->slot->n;
}

Notes& NoteTakerWidget::n() {
    return nt()->slot->n;
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
    edit.voice = false;
}

void NoteTakerWidget::resetState() {
    clipboard.notes.clear();
    this->setClipboardLight();
    display->resetXAxisOffset();
    selectChannels = ALL_CHANNELS;
    this->resetControls();
}

bool NoteTakerWidget::runningWithButtonsOff() const {
    return runButton->ledOn() && !this->menuButtonOn();
}

void NoteTakerWidget::setClipboardLight() {
    bool ledOn = !clipboard.notes.empty() && this->extractClipboard();
    float brightness = ledOn ? selectButton->editStart() ? 1 : 0.25 : 0;
    nt()->setClipboardLight(brightness);
}

void NoteTakerWidget::setSelectableScoreEmpty() {
    auto& n = this->n();
    auto iter = n.notes.begin();
    while (iter != n.notes.end()) {
        if (iter->isSelectable(selectChannels)) {
            iter = n.notes.erase(iter);
        } else {
            ++iter;
        }
    }
    this->invalidateAndPlay(Inval::cut);
    display->displayEnd = 0;  // force recompute of display end
    nt()->setSelect(0, 1);
    this->setWheelRange();
}

void NoteTakerWidget::shiftNotes(unsigned start, int diff) {
    auto& n = this->n();
    if (debugVerbose) DEBUG("shiftNotes start %u diff %d selectChannels 0x%02x", start, diff, selectChannels);
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
        DEBUG("! didn't expect < 0 : value: %d", value);
        return 0;
    }
    auto& n = this->n();
    SCHMICKLE(value > 0);
    SCHMICKLE(value < (int) n.notes.size());
    int count = value - 1;
    int lastStart = -1;
    for (auto& note : n.notes) {
        if (note.isSelectable(selectChannels) && lastStart != note.startTime) {
            if (--count < 0) {
                return &note - &n.notes.front();
            }
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
        if (TRACK_END == note.type) {
            if (count > 0) {
                DEBUG("! expected 0 wheelToNote value at track end; value: %d", value);
                SCHMICKLE(!dbug);                
            }
            return n.notes.size() - 1;
        }
    }
    DEBUG("! out of range wheelToNote value %d", value);
    if (dbug) {
        this->debugDump(false, true);
        _schmickled();  // probably means wheel range is larger than selectable note count
    }
    return (unsigned) -1;
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per pluginInstance, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = createModel<NoteTaker, NoteTakerWidget>("NoteTaker");
