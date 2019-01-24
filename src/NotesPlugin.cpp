#include "NotesPlugin.hpp"
#include <algorithm>
#include <limits>

using std::vector;

/* wheels have a pair of slits between an always on LED and a pair of photo diodes
the slits are arranged
0 1
1 0
1 1
0 1
etc
so that cw and ccw are separately detectable
a reading of 0 0 means a malfunction?

software emulation lets values wrap e.g. modulo
*/

// eventually want to add thumbwheel support, but for now, implement
// with a pair of buttons
#define THUMB_WHEEL 0

struct NotesModule : Module {
    enum ParamIds {
#if THUMB_WHEEL
        VERT_WHEEL,
        HORZ_WHEEL,
#else
        UP_BUTTON,
        DOWN_BUTTON,
        LEFT_BUTTON,
        RIGHT_BUTTON,
#endif
        TRILL_BUTTON,
        GLISSANDO_BUTTON,
        ARPEGGIO_BUTTON,
        SELECT_BUTTON,
        DURATION_BUTTON,
        NOTE_BUTTON,

        FUNCTION_BUTTON,
        LOAD_BUTTON,
        CODA_BUTTON,
        TIE_BUTTON,
        REST_BUTTON,
        PART_BUTTON,

        NUM_PARAMS
    };

    enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        V_OCT_INPUT,
        FX_1_INPUT,
        FX_2_INPUT,
        PROG_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        CV_1_OUTPUT,
        CV_2_OUTPUT,
        CV_3_OUTPUT,
        CV_4_OUTPUT,
        GATE_1_OUTPUT,
        GATE_2_OUTPUT,
        GATE_3_OUTPUT,
        GATE_4_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        TRILL_BUTTON_LIGHT,
        GLISSANDO_BUTTON_LIGHT,
        ARPEGGIO_BUTTON_LIGHT,
        SELECT_BUTTON_LIGHT,
        DURATION_BUTTON_LIGHT,
        NOTE_BUTTON_LIGHT,

        FUNCTION_BUTTON_LIGHT,
        LOAD_BUTTON_LIGHT,
        CODA_BUTTON_LIGHT,
        TIE_BUTTON_LIGHT,
        REST_BUTTON_LIGHT,
        PART_BUTTON_LIGHT,

        NUM_LIGHTS      // no lights
    };

    enum class Horz_Mode {
        Select,         // horz wheel chooses note
        Note,           // horz wheel changes note duration
        Extend,         // horz wheel extends selection
                        //  select again goes to Select
                        //  note swaps anchor and end?
        Arp,            // selects key
        Time,           // selects time signature / beats per measure
        Part,           // not sure, but assume mode is for both horz and vert
    };

    enum class Button_Mode {
        Normal,         // buttons perform primary function
        Func,           // buttons perform secondary function
        Part,           // buttons choose # of parts or cancel
    };

    struct Effects {
        int notesIndex;
        bool threeStart;       
        bool threeEnd;       
        bool tieStart;
        bool tieEnd;
        bool glissStart;
        bool glissEnd;
        bool arpStart;
        bool arpEnd;
        bool codaStart;
        bool codaEnd;
    };

    struct TimeSignature {
        int start;
        int beatsPerMeasure;
        int beatValue;
    };

    static constexpr float CLOCK_UP_THRESHOLD = 3;
    static constexpr float CLOCK_DOWN_THRESHOLD = 1;
    static const int INTERVAL_PER_SEMITONE = 100;
    
    static const int PULSES_PER_BEAT = 96;  // 96 per quarter note, at 120 bpm 1 == 1/192 sec
    static const int CENTS_PER_OCTAVE = 1200;
    static const int MAX_PARTS = 8;
    static const int MAX_DURATION = PULSES_PER_BEAT * 4;  // bigger than whole note gets tied
    static const int LOWEST_NOTE = INTERVAL_PER_SEMITONE;
    static const int HIGHEST_NOTE = INTERVAL_PER_SEMITONE * 12 * 10; // ten octaves

    struct Note {
        int part;           // 0 - 7
        int pitch;          // in cents, 1200 cents per octave;  zero value indicates rest
        int start;          // offset from zero in PULSE_PER_BEAT
        int duration;       // in PULSE_PER_BEAT
        int effectIndex;    // -1 for no effect
        bool tied;          // true if gate / pulse out is omitted
        bool gliss;         // if set, interp cv out
        bool arp;           // if set, not independently selectable
        bool three;         // if set, duration is equal to others in three run
    };

    vector<Note> notes;             // ordered by start time
    vector<Effects> effects;        // do not need to be ordered
    vector<TimeSignature> bpm;      // separate array for finding time changes, ordered

    int clockIndex;                 // note index; advanced by time from clock pulse

    bool activePart[MAX_PARTS];
    int selectionAnchor = 0;
    int selectionEnd = 0;           // may be equal, less or more than anchor
    uint64_t wallStartTick = 0;    // wall time when sequence was reset, began playing
    uint64_t wallTicks = 0;
    // clock ticks on beat
    int upClockTick = 0;            // ticks when clock last went up
    int clockTicks = 0;              // ticks into this clock cycle
    int clockBeats = 0;             // total number of clocks
    int clockNote = 0;              // time into sequence in PULSE_PER_BEAT (matches note duration)
    int clockNoteStart = 0;         // time in clock when last note started

    float lastVert = std::numeric_limits<float>::infinity();
    float lastHorz = std::numeric_limits<float>::infinity();

    Horz_Mode horzMode = Horz_Mode::Select;
    Button_Mode buttonMode = Button_Mode::Normal;
    bool clockDown = false;

    NotesModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
    void applyEffect(int effectSelectionDiff);
    std::pair<int, int> selection() const;
    void step() override;

    TimeSignature findTimeSignature(int noteIndex) const {
        assert(noteIndex >= 0);
        assert(noteIndex < (int) notes.size());
        int noteStart = notes[noteIndex].start;
        TimeSignature* result = nullptr; 
        for (auto timeSignature : bpm) {
            if (timeSignature.start <= noteStart) {
                result = &timeSignature;
            }
        }
    }

    const Effects* hasEffect() const {
        if (0 > selectionAnchor || selectionAnchor >= (int) notes.size()) {
            return nullptr;
        }
        int effectIndex = notes[selectionAnchor].effectIndex;
        if (0 > effectIndex || effectIndex >= (int) effects.size()) {
            return nullptr;
        }
        return &effects[effectIndex];
    }

    bool partIsActive(int index) const {
        assert(notes[index].part >= 0);
        assert(notes[index].part < MAX_PARTS);
        return activePart[notes[index].part];
    }

    void setOutput(const Note& note) {
        float pitch = (float) note.pitch / CENTS_PER_OCTAVE;
        pitch += inputs[V_OCT_INPUT].value;
        pitch = std::max(0.0f, std::min(10.0f, pitch));
        outputs[note.part].value = pitch;
    }

    // For more advanced Module features, read Rack's engine.hpp header file
    // - toJson, fromJson: serialization of internal data
    // - onSampleRateChange: event triggered by a change of sample rate
    // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

void NotesModule::applyEffect(int effectSelectionDiff) {
    const Effects* effects = this->hasEffect();
    if (!effects) {
        return;
    }
    if (effects->threeStart) {
            // to do: 
    }
}

std::pair<int, int> NotesModule::selection() const {
    auto minmax = std::minmax(selectionAnchor, selectionEnd);
    int selStart = std::max(0, minmax.first);
    if ((size_t) selStart >= notes.size()) {
        return std::pair<int, int>(-1, -1);
    }
    int selEnd = std::min(std::max(0, (int) notes.size() - 1), minmax.second);
    int start = notes[selStart].start;
    while (selStart > 0 && start == notes[selStart - 1].start) {
        selStart--;
    }
    start = notes[selEnd].start;
    while ((size_t) selEnd < notes.size() - 1 && start == notes[selEnd + 1].start) {
        selEnd++;
    }
    return std::pair<int, int>(selStart, selEnd + 1);
}

void NotesModule::step() {
    // advance clock
    ++wallTicks;  // TODO: do we need wallTicks?
    if (clockTicks < upClockTick) { 
        ++clockTicks;   // if there's no recent pulse, don't advance clockTicks
    }
    if (clockDown && inputs[CLOCK_INPUT].value > CLOCK_UP_THRESHOLD) {
        upClockTick = clockTicks;
        clockTicks = 0;
        clockBeats += PULSES_PER_BEAT;
    }
    clockDown = inputs[CLOCK_INPUT].value < CLOCK_DOWN_THRESHOLD;
    
    // advance selection if clock has exhausted current note
    // notes must be ordered shortest duration first for multiple parts
    int clockBeatFraction = clockTicks * PULSES_PER_BEAT / upClockTick;
    int clockBeatVal = clockBeats + clockBeatFraction;
    int start = notes[selectionAnchor].start;
    if (clockBeatVal - start >= notes[selectionAnchor].duration) {
        do {
            // TODO: wrap on coda as well as end of notes
            // TODO: if hitting end of notes, stop (unless there's a coda)
            if (++selectionAnchor >= (int) notes.size()) {
                selectionAnchor = 0;
                clockBeats = 0;
                break;
            }
        } while (notes[selectionAnchor].start == start);
    }

    std::pair<int, int> sel = this->selection();
    // adjust the current note or range of notes up or down
    bool vertChange;
    bool vertDown;
#if THUMB_WHEEL
    float vert = params[VERT_WHEEL].value;
    vertChange = lastVert != std::numeric_limits<float>::infinity() && vert != lastVert;
    vertDown = vertChange && vert < lastVert;
#else
    params[UP_BUTTON].value;
#endif
    if (vertChange) {
        switch (horzMode) {
        case Horz_Mode::Part:
            // TODO: select active part for further editing
            break;
        case Horz_Mode::Select:
        case Horz_Mode::Note:
        case Horz_Mode::Extend:
        case Horz_Mode::Arp: {
            int offset = vertDown ? -INTERVAL_PER_SEMITONE : INTERVAL_PER_SEMITONE;
            // adjust pitch of selection
            for (auto index = sel.first; index < sel.second; ++index) {
                if (!this->partIsActive(index)) {
                    continue;
                }
                int newPitch = std::max(LOWEST_NOTE, std::min(HIGHEST_NOTE,
                        notes[index].pitch + offset));
                notes[index].pitch = newPitch;
            }
            } break;
        default:
            assert(0);
        }
    }
#if THUMB_WHEEL
    lastVert = vert;
#endif

    // adjust duration or selection of notes
    bool horzChange;
    bool horzDown;
#if THUMB_WHEEL
    float horz = params[HORZ_WHEEL].value;
    horzChange = lastHorz != std::numeric_limits<float>::infinity() && horz != lastHorz;
    horzDown = horzChange && horz < lastHorz;
#else

#endif
    if (horzChange) {
        switch (horzMode) {
        case Horz_Mode::Select:
            // in select mode, vert thumbwheel selects part?
            break;
        case Horz_Mode::Note:
            for (auto index = sel.first; index < sel.second; ++index) {
                if (!this->partIsActive(index)) {
                    continue;
                }
                Note& note = notes[index];
                if (note.three || note.tied || note.gliss || note.arp) {
                    // TODO:
                    // lengthening or shortening effects is more complicated
                    // figure change to notes without effect, then distribute time to notes in effect...
                    break;
                }
                unsigned duration = (unsigned) note.duration;
                assert(duration);  // at least one bit set
                assert(!(duration & (duration >> 2)));  // no more than two bits set
                unsigned dottedNote = duration & (duration >> 1);
                if (dottedNote) {
                    duration += horzDown ? - (int) dottedNote : dottedNote;
                } else {
                    dottedNote = duration | (duration >> 1);
                    if (horzDown) {
                        dottedNote >>= 1;
                    }
                }
                note.duration = (int) duration;
            } 
            break;
        case Horz_Mode::Extend: {
            int startBase = notes[selectionEnd].start;
            int newEnd = selectionEnd;
            if (horz < lastHorz) {
                while (newEnd > 0 && !this->partIsActive(--newEnd))
                    ;
                if (newEnd < selectionEnd) {
                    int newStart = notes[newEnd].start;
                    assert(newStart < startBase);
                    while (newEnd > 0 && newStart == notes[newEnd - 1].start) {
                        --newEnd;
                    }
                }
                selectionEnd = newEnd;
            } else {
                while ((size_t) newEnd < notes.size() - 1 && startBase == notes[++newEnd].start)
                    ;
                selectionEnd = newEnd;
                while ((size_t) newEnd < notes.size() - 1 && !this->partIsActive(newEnd)) {
                    ++newEnd;
                    if (notes[selectionEnd].start < notes[newEnd].start) {
                        selectionEnd = newEnd;
                    }
                }
            }
            } break;
        case Horz_Mode::Arp:
            applyEffect(horz < lastHorz ? -1 : 1);
            break;
        default:
            assert(0);
        }
    }
    lastHorz = horz;
    // TODO: if editing selection end, use that instead of selectionAnchor for current cv
    // TODO: after timeout of inactivity, play all notes in selection then use anchor for current cv

    // notes are repeated each measure (tied if continuing from a previous measure)
    // so looking backwards one measure is the maximum required to find all part values
    bool partsFound[MAX_PARTS];
    int partsToFind = 0;
    for (int partIndex = 0; partIndex < MAX_PARTS; ++partIndex) {
        if ((partsFound[partIndex] = activePart[partIndex])) {
            partsToFind++;
        }
    }
    int noteIndex = selectionAnchor;
    assert(0 <= noteIndex);
    assert(noteIndex < (int) notes.size());
    int start = notes[noteIndex].start;
    int lastNote = (int) notes.size() - 1;
    do {
        const Note& note = notes[noteIndex];
        if (partsFound[note.part]) {
            this->setOutput(note);
            partsFound[note.part] = false;
            if (0 == --partsToFind) {
                break;
            }
        }
        ++noteIndex;
    } while (noteIndex < lastNote && notes[noteIndex].start == start);
    noteIndex = selectionAnchor;
    while (partsToFind && --noteIndex >= 0) {
        const Note& note = notes[noteIndex];
        if (partsFound[note.part]) {
            continue;
        }
        partsFound[note.part] = true;
        --partsToFind;
        float pitch = (float) note.pitch / CENTS_PER_OCTAVE;
        pitch += inputs[V_OCT_INPUT].value;
        pitch = std::max(0.0f, std::min(10.0f, pitch));
        outputs[note.part].value = pitch;
    }
}

struct NotesDisplay : TransparentWidget {

    void draw(NVGcontext *vg) override {
    }

    NotesModule* module;
};

NotesModuleWidget::NotesModuleWidget() {
    auto module = new NotesModule();
    setModule(module);
    box.size = Vec(16 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(SVG::load(assetPlugin(plugin, "res/NotesModule.svg")));
        addChild(panel);
    }

    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    NotesDisplay *display = new NotesDisplay();
    display->module = module;
    display->box.pos = Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2);
    display->box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9);
    addChild(display);

    addParam(createParam<Davies1900hBlackKnob>(Vec(28, 87), module, NotesModule::VERT_WHEEL, -3.0, 3.0, 0.0));

    addInput(Port::create<PJ301MPort>(Vec(33, 186), Port::INPUT, module, NotesModule::CLOCK_INPUT));

    addOutput(Port::create<PJ301MPort>(Vec(33, 275), Port::OUTPUT, module, NotesModule::CV_1_OUTPUT));
    addOutput(Port::create<PJ301MPort>(Vec(63, 275), Port::OUTPUT, module, NotesModule::GATE_1_OUTPUT));

}

// The plugin-wide instance of the Plugin class
Plugin *plugin;

void init(rack::Plugin *p) {
    plugin = p;
    // The "slug" is the unique identifier for your plugin and must never change after release, so choose wisely.
    // It must only contain letters, numbers, and characters "-" and "_". No spaces.
    // To guarantee uniqueness, it is a good idea to prefix the slug by your name, alias, or company name if available, e.g. "MyCompany-MyPlugin".
    // The ZIP package must only contain one folder, with the name equal to the plugin's slug.
    p->slug = TOSTRING(SLUG);
    p->version = TOSTRING(VERSION);
    p->website = "https://github.com/cclark2a/SchmickleWorks";
    p->manual = "https://github.com/cclark2a/SchmickleWorks";

    // For each module, specify the ModuleWidget subclass, manufacturer slug (for saving in patches), manufacturer human-readable name, module slug, and module name
    p->addModel(createModel<NotesModuleWidget>("Template", "MyModule", "My Module", OSCILLATOR_TAG));

    // Any other plugin initialization may go here.
    // As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
