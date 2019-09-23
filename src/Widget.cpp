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

void Clipboard::fromJsonCompressed(json_t* root) {
    if (!Notes::FromJsonCompressed(root, &notes, nullptr) || (notes.size()
            && (MIDI_HEADER == notes.front().type || TRACK_END == notes.back().type))) {
        this->resetNotes();
    }

    SlotArray::FromJson(root, &playback);
}

void Clipboard::fromJsonUncompressed(json_t* root) {
    Notes::FromJsonUncompressed(root, &notes);
}

json_t* Clipboard::playBackToJson() const {
    json_t* root = json_object();
    json_t* slots = json_array();
    for (const auto& slot : playback) {
        json_array_append_new(slots, slot.toJson());
    }
    json_object_set_new(root, "slots", slots);
    return root;
}

void Clipboard::toJsonCompressed(json_t* root) const {
    Notes::ToJsonCompressed(notes, root, "clipboardCompressed");
}

// move everything hanging off NoteTaker for inter-app communication and hang it off
// NoteTakerWidget instead, 
// make sure things like loading midi don't happen if module is null
NoteTakerWidget::NoteTakerWidget(NoteTaker* module) 
    : editButtonSize(Vec(22, 43)) {
    reqs.reader = &reqs.buffer.front();
    reqs.writer = &reqs.buffer.front();
    if (debugVerbose) {
        NoteDurations::Validate();
    }
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
        module->configParam<SlotButtonToolTip>(NoteTaker::SLOT_BUTTON, 0, 1, 0, "Edit");
        module->configParam<TempoButtonToolTip>(NoteTaker::TEMPO_BUTTON, 0, 1, 0, "Insert");
        module->configParam<HorizontalWheelToolTip>(NoteTaker::HORIZONTAL_WHEEL, 0, 1, 0, "Time Wheel");
        module->configParam<VerticalWheelToolTip>(NoteTaker::VERTICAL_WHEEL, 0, 1, 0, "Pitch Wheel");
        module->setMainWidget(this);  // to do : is there a way to avoid this cross-dependency?
    }
    Vec hWheelPos = Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f);
    Vec hWheelSize = Vec(100, 23);
    addWheel(hWheelSize, (horizontalWheel = createParam<HorizontalWheel>(
            hWheelPos, module, NoteTaker::HORIZONTAL_WHEEL)));
    if (horizontalWheel->paramQuantity) {
        ((HorizontalWheelToolTip*) horizontalWheel->paramQuantity)->widget = horizontalWheel;
    }
    // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
    Vec vWheelPos = Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50);
    Vec vWheelSize = Vec(23, 100);
    addWheel(vWheelSize, (verticalWheel = createParam<VerticalWheel>(
            vWheelPos, module, NoteTaker::VERTICAL_WHEEL)));
    if (verticalWheel->paramQuantity) {
        ((VerticalWheelToolTip*) verticalWheel->paramQuantity)->widget = verticalWheel;
    }
    addInput(createInput<PJ301MPort>(Vec(140, 306), module, NoteTaker::V_OCT_INPUT));
    addInput(createInput<PJ301MPort>(Vec(172, 306), module, NoteTaker::CLOCK_INPUT));
    addInput(createInput<PJ301MPort>(Vec(204, 306), module, NoteTaker::EOS_INPUT));
    addInput(createInput<PJ301MPort>(Vec(140, 338), module, NoteTaker::RESET_INPUT));
    addOutput(createOutput<PJ301MPort>(Vec(172, 338), module, NoteTaker::CLOCK_OUTPUT));
    addOutput(createOutput<PJ301MPort>(Vec(204, 338), module, NoteTaker::EOS_OUTPUT));

    for (unsigned i = 0; i < CV_OUTPUTS; ++i) {
            addOutput(createOutput<PJ301MPort>(Vec(12 + i * 32, 306), module,
                    NoteTaker::CV1_OUTPUT + i));
            addOutput(createOutput<PJ301MPort>(Vec(12 + i * 32, 338), module,
                    NoteTaker::GATE1_OUTPUT + i));
    }
    Vec runButtonSize = Vec(24, 24);
    this->addButton(runButtonSize, (runButton = 
            createParam<RunButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON)));
    this->addButton<SelectButton, SelectButtonToolTip>(&selectButton, NoteTaker::EXTEND_BUTTON);
    this->addButton<InsertButton, InsertButtonToolTip>(&insertButton, NoteTaker::INSERT_BUTTON);
    this->addButton<CutButton, CutButtonToolTip>(&cutButton, NoteTaker::CUT_BUTTON);
    this->addButton<RestButton, RestButtonToolTip>(&restButton, NoteTaker::REST_BUTTON);
    this->addButton<PartButton, PartButtonToolTip>(&partButton, NoteTaker::PART_BUTTON);
    this->addButton<FileButton, FileButtonToolTip>(&fileButton, NoteTaker::FILE_BUTTON);
    this->addButton<SustainButton, SustainButtonToolTip>(&sustainButton, NoteTaker::SUSTAIN_BUTTON);
    this->addButton<TimeButton, TimeButtonToolTip>(&timeButton, NoteTaker::TIME_BUTTON);
    this->addButton<KeyButton, KeyButtonToolTip>(&keyButton, NoteTaker::KEY_BUTTON);
    this->addButton<TieButton, TieButtonToolTip>(&tieButton, NoteTaker::TIE_BUTTON);
    this->addButton<SlotButton, SlotButtonToolTip>(&slotButton, NoteTaker::SLOT_BUTTON);
    this->addButton<TempoButton, TempoButtonToolTip>(&tempoButton, NoteTaker::TEMPO_BUTTON);
    // debug button is hidden to the right of tempo
    this->addButton(editButtonSize, (dumpButton = 
            createParam<DumpButton>(Vec(222, 252), module, NoteTaker::DUMP_BUTTON)));
    this->onReset();
    if (module) {
        module->requests.push(RequestType::onReset);
    }
}

NoteTakerSlot* NoteTakerWidget::activeSlot() {
    if (fileButton->ledOn()) {
        unsigned index = (unsigned) horizontalWheel->getValue();
        SCHMICKLE(index < storage.size());
        return &storage.slots[index];
    }
    return &storage.current();
}

template<class TButton, class TButtonToolTip> 
void NoteTakerWidget::addButton(TButton** butPtr, int id) {
    unsigned slot = (unsigned) id;
    SCHMICKLE(slot > 0);
    --slot;
    const Vec position = { (float) (30 + slot % 6 * 32), (float) (202 + slot / 6 * 50) };
    TButton* button = createParam<TButton>(position, module, id);
    *butPtr = button;
    this->addButton(editButtonSize, button);
    button->slotNumber = slot;
    if (button->paramQuantity) {
        ((TButtonToolTip*) button->paramQuantity)->widget = button;
    }

}

void NoteTakerWidget::addButton(const Vec& size, NoteTakerButton* button) {
    button->box.size = size;
    params.push_back(button);
    ButtonBuffer* buffer = new ButtonBuffer(button);
    this->addChild(buffer);
}

void NoteTakerWidget::addWheel(const Vec& size, NoteTakerWheel* wheel) {
    wheel->box.size = size;
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

struct NoteTakerDebugVerboseItem : MenuItem {

	void onAction(const event::Action& e) override {
        debugVerbose ^= true;
	}
};

void NoteTakerWidget::appendContextMenu(Menu *menu) {
    menu->addChild(new MenuEntry);
    auto loadItem = createMenuItem<NoteTakerLoadItem>("Load MIDI", RIGHT_ARROW);
    loadItem->widget = this;
    menu->addChild(loadItem);
    auto saveItem = createMenuItem<NoteTakerSaveItem>("Save as MIDI", RIGHT_ARROW);
    saveItem->widget = this;
    menu->addChild(saveItem);
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem<NoteTakerDebugVerboseItem>("Verbose debugging",
            CHECKMARK(debugVerbose)));
;    // to do : add Marc Boule's reset options, approximately:
    /* Restart when run is -> turned off
                              turned on
                              both
                              neither
       Send EOS pulse when restart
       Output EOS high when not running
     */
}

void NoteTakerWidget::clearSlurs() {
    this->n().setSlurs(selectChannels, false);
    storage.invalidate();
}

void NoteTakerWidget::clearTriplets() {
    this->n().setTriplets(selectChannels, false);
    display->range.displayEnd = 0;
    displayBuffer->redraw();
    this->invalAndPlay(Inval::change);
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

void NoteTakerWidget::copySlots() {
    clipboard.playback.assign(storage.playback.begin() + storage.slotStart,
            storage.playback.begin() + storage.slotEnd);
}

void NoteTakerWidget::copySelectableNotes() {
    if (!clipboardInvalid) {
        return;
    }
    clipboard.notes.clear();
    auto& n = this->n();
    for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
        auto note = n.notes[index];
        if (MIDI_HEADER == note.type) {
            continue;
        }

        SCHMICKLE(TRACK_END != note.type);
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
    NoteTakerSlot* source = &storage.current();
    NoteTakerSlot* dest = &storage.slots[index];
    // to do : create custom copy constructor to skip copying cache, and make cache non-copy-able
    *dest = *source;
    dest->invalid = true;
}

void NoteTakerWidget::disableEmptyButtons() const {
    for (NoteTakerButton* button : {
            (NoteTakerButton*) selectButton,    (NoteTakerButton*) insertButton,
            (NoteTakerButton*) cutButton,       (NoteTakerButton*) restButton,
            (NoteTakerButton*) partButton,      (NoteTakerButton*) fileButton,
            (NoteTakerButton*) sustainButton,   (NoteTakerButton*) timeButton,
            (NoteTakerButton*) keyButton,       (NoteTakerButton*) tieButton,
            (NoteTakerButton*) slotButton,      (NoteTakerButton*) tempoButton }) {
        unsigned slotNumber = button->slotNumber;
        if (storage.slots[slotNumber].n.isEmpty(ALL_CHANNELS)
                || slotNumber == storage.slotStart) {
            button->animationFrame = 1;
        }
        button->fb()->dirty = true;
    }
}

void NoteTakerWidget::draw(const DrawArgs &args) {
    float t[6];
    static float lastTransform[6];
    nvgCurrentTransform(args.vg, t);
    if (memcmp(lastTransform, t, sizeof(lastTransform))) {
        if (false) DEBUG("widget draw xform %g %g %g %g %g %g", t[0], t[1], t[2], t[3], t[4], t[5]);
        display->redraw();
        selectButton->fb()->dirty = true;
        memcpy(lastTransform, t, sizeof(t));
    }
    ModuleWidget::draw(args);
}

void NoteTakerWidget::enableButtons() const {
    for (NoteTakerButton* button : {
            (NoteTakerButton*) selectButton,    (NoteTakerButton*) insertButton,
            (NoteTakerButton*) cutButton,       (NoteTakerButton*) restButton,
            (NoteTakerButton*) partButton,      (NoteTakerButton*) fileButton,
            (NoteTakerButton*) sustainButton,   (NoteTakerButton*) timeButton,
            (NoteTakerButton*) keyButton,       (NoteTakerButton*) tieButton,
            (NoteTakerButton*) slotButton,      (NoteTakerButton*) tempoButton }) {
        button->animationFrame = 0;
        button->fb()->dirty = true;
    }
}

void NoteTakerWidget::enableInsertSignature(unsigned loc) {
    auto& n = this->n();
    unsigned insertLoc = n.atMidiTime(n.notes[loc].startTime);
    std::pair<NoteTakerButton*,  DisplayType> pairs[] = {
            { (NoteTakerButton*) keyButton, KEY_SIGNATURE },
            { (NoteTakerButton*) timeButton, TIME_SIGNATURE },
            { (NoteTakerButton*) tempoButton, MIDI_TEMPO } };
    for (auto& pair : pairs) {
        pair.first->animationFrame = (int) n.insertContains(insertLoc, pair.second);
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
            Notes::MapChannel(*span, this->unlockedChannel()); 
        }
    }
    return true;
}

unsigned NoteTakerWidget::getSlot() const {
    unsigned result = &storage.current() - &storage.slots.front();
    SCHMICKLE(result < SLOT_COUNT);
    return result;
}

void NoteTakerWidget::insertFinal(int shiftTime, unsigned insertLoc, unsigned insertSize) {
    bool slotOn = slotButton->ledOn();
    if (slotOn) {
        storage.slotStart = insertLoc;
        storage.slotEnd = insertLoc + insertSize;
    } else {
        if (shiftTime) {
            this->shiftNotes(insertLoc + insertSize, shiftTime);
        } else {
            this->n().sort();
        }
        this->setSelect(insertLoc, n().nextAfter(insertLoc, insertSize));
    }
    display->range.displayEnd = 0;  // force recompute of display end
    if (debugVerbose) DEBUG("insert final");
    // range is larger
    this->setWheelRange();
    displayBuffer->redraw();
    if (!slotOn) {
        this->invalAndPlay(Inval::note);
    }
    if (debugVerbose) this->debugDump(true);
}

void NoteTakerWidget::invalAndPlay(Inval inval) {
    if (Inval::none == inval) {
        return;
    }
    display->invalidateRange();
    if (Inval::display != inval) {
        display->invalidateCache();
    }
    this->nt()->requests.push({RequestType::invalidateAndPlay, (unsigned) inval});
}

void NoteTakerWidget::loadScore() {
    unsigned slot = (unsigned) horizontalWheel->getValue();
    SCHMICKLE(slot < storage.size());
    this->stageSlot(slot);
    this->resetScore();
}

// allows two or more notes, or two or more rests: to be tied
// repeats for each channel, to allow multiple channels to be selected and slurred at once
void NoteTakerWidget::makeSlurs() {
    this->n().setSlurs(selectChannels, true);
    storage.invalidate();
}

// if current selection includes durations not in std note durations, do nothing
// the assumption is that in this case, durations have already been tupletized 
void NoteTakerWidget::makeTriplets() {
    this->n().setTriplets(selectChannels, true);
    display->range.displayEnd = 0;
    displayBuffer->redraw();
    this->invalAndPlay(Inval::change);
}

// true if a button that brings up a secondary menu on display is active
bool NoteTakerWidget::menuButtonOn() const {
    return fileButton->ledOn() || partButton->ledOn() || slotButton->ledOn()
            || sustainButton->ledOn() || tieButton->ledOn();
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

NoteTaker* NoteTakerWidget::nt() {
    return dynamic_cast<NoteTaker* >(module);
}

const NoteTaker* NoteTakerWidget::nt() const {
    return dynamic_cast<const NoteTaker* >(module);
}

void NoteTakerWidget::resetForPlay() {
    auto& n = this->n();
    display->range.setRange(n);
    unsigned next = n.nextAfter(n.selectStart, 1);
    this->setSelectStart(next < n.notes.size() - 1 ? next : selectButton->editStart() ? 0 : 1);
    horizontalWheel->lastRealValue = INT_MAX;
    verticalWheel->lastValue = INT_MAX;
    edit.voice = false;
}

void NoteTakerWidget::resetChannels() {
    for (auto& channel : storage.current().channels) {
        channel.reset();
    }
}

bool NoteTakerWidget::resetControls() {
    this->turnOffLEDButtons();
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
    displayBuffer->redraw();
    display->reset();
    return true;
}

void NoteTakerWidget::resetNotes() {
    auto& n = this->n();
    n.notes.clear();
    this->setScoreEmpty();
    n.selectStart = 0;
    n.selectEnd = 1;
}

void NoteTakerWidget::resetScore() {
    display->range.resetXAxisOffset();
    this->invalAndPlay(Inval::load);
    unsigned atZero = this->n().atMidiTime(0);
    atZero -= TRACK_END == storage.current().n.notes[atZero].type;
    this->setSelectStart(atZero);
}

void NoteTakerWidget::resetState() {
    clipboard.notes.clear();
    this->setClipboardLight();
    display->range.resetXAxisOffset();
    selectChannels = ALL_CHANNELS;
    this->resetControls();
}

void NoteTakerWidget::restoreNotes() {
    auto& n = this->n();
    n.notes = edit.base;
    storage.invalidate();
}

bool NoteTakerWidget::runningWithButtonsOff() const {
    return runButton->ledOn() && !this->menuButtonOn();
}

void NoteTakerWidget::setClipboardLight() {
    bool ledOn = slotButton->ledOn() ? !clipboard.playback.empty() :
            !clipboard.notes.empty() && this->extractClipboard();
    float brightness = ledOn ? selectButton->editStart() ? 1 : 0.25 : 0;
    nt()->requests.push({RequestType::setClipboardLight, (unsigned) brightness * 256});
}

void NoteTakerWidget::setScoreEmpty() {
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    if (false && debugVerbose) NoteTakerParseMidi::DebugDumpRawMidi(emptyMidi);
    auto& slot = storage.current();
    NoteTakerParseMidi emptyParser(emptyMidi, &slot.n.notes, nullptr, slot.channels);
    bool success = emptyParser.parseMidi();
    SCHMICKLE(success);
    this->invalAndPlay(Inval::cut);
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
    this->invalAndPlay(Inval::cut);
    display->range.displayEnd = 0;  // force recompute of display end
    this->setSelect(0, 1);
    this->setWheelRange();
    displayBuffer->redraw();
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
    auto& n = this->n();
    if (n.isEmpty(selectChannels)) {
        n.selectStart = 0;
        n.selectEnd = 1;
        display->range.setRange(n);
        displayBuffer->redraw();
        if (debugVerbose) DEBUG("setSelect set empty");
        return;
    }
    SCHMICKLE(start < end);
    SCHMICKLE(n.notes.size() >= 2);
    SCHMICKLE(end <= n.notes.size() - 1);
    display->range.setRange(n);
    if (!runButton->ledOn()) {
        this->enableInsertSignature(end);  // disable buttons that already have signatures in score
    }
#if DEBUG_RUN_TIME
    if (debugVerbose) DEBUG("setSelect old %u %u new %u %u", n.selectStart, n.selectEnd, start, end);
#endif
    n.selectStart = start;
    n.selectEnd = end;
    displayBuffer->redraw();
    if (false && debugVerbose) DEBUG("setSelect set");
}

bool NoteTakerWidget::setSelectEnd(int wheelValue, unsigned end) {
    const auto& n = this->n();
    DEBUG("setSelectEnd wheelValue=%d end=%u button->selStart=%u selectStart=%u selectEnd=%u", 
            wheelValue, end, selectButton->selStart, n.selectStart, n.selectEnd);
    bool changed = true;
    if (end < selectButton->selStart) {
        this->setSelect(end, selectButton->selStart);
        DEBUG("setSelectEnd < s:%u e:%u", n.selectStart, n.selectEnd);
    } else if (end == selectButton->selStart) {
        unsigned start = selectButton->selStart;
        SCHMICKLE(TRACK_END != n.notes[start].type);
        unsigned end = this->wheelToNote(wheelValue + 1);
        this->setSelect(start, end);
        DEBUG("setSelectEnd == s:%u e:%u", n.selectStart, n.selectEnd);
    } else if (end != n.selectEnd) {
        this->setSelect(selectButton->selStart, end);
        DEBUG("setSelectEnd > s:%u e:%u", n.selectStart, n.selectEnd);
    } else {
        changed = false;
    }
    SCHMICKLE(n.selectEnd != n.selectStart);
    return changed;
}

bool NoteTakerWidget::setSelectStart(unsigned start) {
    const auto& n = this->n();
    unsigned end = start;
    do {
        ++end;
    } while (n.notes[start].startTime == n.notes[end].startTime && n.notes[start].isNoteOrRest()
            && n.notes[end].isNoteOrRest());
    this->setSelect(start, end);
    return true;
}

void NoteTakerWidget::shiftNotes(unsigned start, int diff) {
    auto& n = this->n();
    if (debugVerbose) DEBUG("shift notes start %u diff %d selectChannels 0x%02x", start, diff, selectChannels);
    n.shift(start, diff, selectChannels);
    n.sort();
}

void NoteTakerWidget::stageSlot(unsigned slot) {
    unsigned last = this->getSlot();
    SCHMICKLE(last < SLOT_COUNT);
    if (slot == last) {
        return;
    }
    if (debugVerbose) DEBUG("stageSlot %u old %u", slot, last);
    storage.slotStart = slot;
    storage.slotEnd = slot + 1;
    display->stagedSlot = &storage.current();
    this->invalAndPlay(Inval::load);
    this->resetForPlay();
    this->nt()->requests.push(RequestType::resetAndPlay);
}

void NoteTakerWidget::step() {
    float t[6];
    static float lastTransform[6];
    nvgCurrentTransform(APP->window->vg, t);
    if (memcmp(lastTransform, t, sizeof(lastTransform))) {
        if (false) DEBUG("widget step xform %g %g %g %g %g %g", t[0], t[1], t[2], t[3], t[4], t[5]);
        display->redraw();
        selectButton->fb()->dirty = true;
        memcpy(lastTransform, t, sizeof(t));
    }
    ModuleWidget::step();
}

// never turns off select button, since it cannot be turned off if score is empty,
// and it contains state that should not be arbitrarily deleted. select button
// is explicitly reset only when notetaker overall state is reset
void NoteTakerWidget::turnOffLEDButtons(const NoteTakerButton* exceptFor, bool exceptSlot) {
    for (NoteTakerButton* button : {
                (NoteTakerButton*) fileButton,
                (NoteTakerButton*) partButton,
                (NoteTakerButton*) runButton,
                (NoteTakerButton*) slotButton,
                (NoteTakerButton*) sustainButton,
                (NoteTakerButton*) tieButton }) {
        if (exceptFor != button && (!exceptSlot || slotButton != button)) {
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

Model *modelNoteTaker = createModel<NoteTaker, NoteTakerWidget>("NoteTaker");
