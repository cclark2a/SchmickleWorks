#include "SchmickleWorks.hpp"
#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"

// todo:
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    gateOut.fill(0);
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    NoteTakerParseMidi parser(midi, allNotes);
    parser.parseMidi();
}

// to compute range for horizontal wheel when selecting notes
int NoteTaker::horizontalCount(const DisplayNote& match, int* index) const {
    int count = 0;
    int lastTime = -1;
    *index = -1;
    this->debugDump();
    debug("---");
    match.debugDump();
    for (auto& note : allNotes) {
        if (&match == &note) {
            *index = count;
        }
        if (!note.isSelectable(selectChannels)) {
            note.debugDump();
            debug("isSelect failed");
            continue;
        }
        if (lastTime == note.startTime) {
            continue;
        }
        ++count;
        lastTime = note.startTime;
    }
    assert(-1 != *index);
    return count;
}

void NoteTaker::updateHorizontal() {
    if (running) {
        return;
    }
    unsigned wheelValue = (unsigned) (int) horizontalWheel->value;
    if (durationButton->ledOn) {
        int diff = 0;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
            if (note.isSelectable(selectChannels)) {
                assert(wheelValue < noteDurations.size());
                note.setNote((int) wheelValue);
                int duration = noteDurations[wheelValue];
                diff += duration - note.duration;
                note.duration = duration;
            }
        }
        if (diff) {
            for (unsigned index = selectEnd; index < allNotes.size(); ++index) {
                DisplayNote& note = allNotes[index];
                note.startTime += diff;
            }
        }
    } else {
    // value should range from 0 to max - 1, where max is number of starts for active channels
        int count = (int) horizontalWheel->value;
        int lastTime = -1;
        for (auto& note : allNotes) {
            if (!note.isSelectable(selectChannels)) {
                continue;
            }
            if (count == 0) {
                selectStart = &note - &allNotes.front();
                break;
            }
            if (lastTime == note.startTime) {
                continue;
            }
            --count;
            lastTime = note.startTime;
        }
        selectEnd = selectStart;
        do {
            ++selectEnd;
        } while (allNotes[selectStart].startTime == allNotes[selectEnd].startTime);
    }
    this->playSelection();
}
    
void NoteTaker::updateVertical() {
    array<DisplayNote*, channels> lastNote;
    lastNote.fill(nullptr);
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (note.isSelectable(selectChannels)) {
            note.setPitch((int) verticalWheel->value);
            lastNote[note.channel] = &note;
        }
    }
    for (auto note : lastNote) {
        if (nullptr == note) {
            continue;
        }
        this->outputNote(*note);
        this->playSelection();
    }
}

void NoteTaker::outputNote(const DisplayNote& note) {
    unsigned noteIndex = &note - &allNotes.front();
    cvOut[note.channel] = noteIndex;
    gateOut[note.channel] = noteIndex;
    if (NOTE_ON == note.type) {
        outputs[GATE1_OUTPUT + note.channel].value = DEFAULT_GATE_HIGH_VOLTAGE;
        assert(note.channel < channels);
	    float v_oct = inputs[V_OCT_INPUT].value;
        outputs[CV1_OUTPUT + note.channel].value = v_oct + note.pitch() / 12.f;
    } else {
        assert(REST_TYPE == note.type);
        outputs[GATE1_OUTPUT + note.channel].value = 0;
    }
}

void NoteTaker::setWheelRange(const DisplayNote& note) {
    // horizontal wheel range and value
    if (durationButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size());
        horizontalWheel->setValue(note.note());
    } else {
        int index;
        int count = this->horizontalCount(note, &index);
        horizontalWheel->setLimits(0, count);
        horizontalWheel->setValue(index);

    }
    // vertical wheel range 0 to 127 for midi pitch
    verticalWheel->setValue(note.pitch());
    debug("horz %g (%g %g)\n", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("vert %g (%g %g)\n", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
}

void NoteTaker::step() {
	if (runningTrigger.process(params[RUN_BUTTON].value)) {
		running = !running;
        if (!running) {
            this->setWheelRange(allNotes[selectStart]);
            for (unsigned channel = 0; channel < channels; ++channel) {
                gateOut[channel] = 0;
                outputs[GATE1_OUTPUT + channel].value = 0;
            }
        }
        lights[RUNNING_LIGHT].value = running;
	}
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
	float deltaTime = engineGetSampleTime();
    elapsedSeconds += deltaTime;
    int midiTime = stdTimePerQuarterNote * elapsedSeconds * 2;
    if (running) {
        // to do: use info in midi header, time signature, tempo to get this right
        this->setExpiredGatesLow(midiTime);
        // if midi time exceeds displayEnd: scroll; reset displayStart, displayEnd
        // if midi time exceeds last of allNotes: either reset to zero and repeat, or stop running
        if (this->lastNoteEnded(displayEnd, midiTime)) {
            elapsedSeconds = 0;  // to do : don't repeat unconditionally?
            midiTime = 0;
            displayStart = this->firstOn();
            if (!displayStart) { // no notes to play
                running = false;
                lights[RUNNING_LIGHT].value = false;
                return;
            }
            displayEnd = 0;  // recompute display end
        } else {
            do {
                const DisplayNote& note = allNotes[displayStart];
                if (note.startTime + note.duration > midiTime || TRACK_END != note.type) {
                    break;
                }
                ++displayStart;
                displayEnd = 0;  // recompute display end
            } while (true);
        }
        selectStart = INT_MAX;
        selectEnd = 0;
        if (!displayEnd) {
            // todo: allow time range to display to be scaled from box width
            // calc here must match NoteTakerDisplay::draw:75
            displayEnd = this->lastAt(allNotes[displayStart].startTime + display->box.size.x * 4 - 32);
        }
        const DisplayNote* best = nullptr;
        for (unsigned step = displayStart; step < displayEnd; ++step) {
            const DisplayNote& note = allNotes[step];
            if (note.isSelectable(selectChannels)) {
                if (note.isBestSelectStart(&best, midiTime)) {
                    selectStart = step;
                }
                if (note.isActive(midiTime)) {
                    this->outputNote(note);
                }
            }
        }
        selectEnd = selectStart + 1;
    } else if (playingSelection) { // not running, play note after selecting or editing
        for (unsigned step = selectStart; step < selectEnd; ++step) {
            const DisplayNote& note = allNotes[step];
            if (note.isSelectable(selectChannels) && note.isActive(midiTime)) {
                this->outputNote(note);
            }
        }
        if (this->lastNoteEnded(selectEnd, midiTime)) {
            playingSelection = false;
            this->setExpiredGatesLow(INT_MAX);
        }
    }
}

struct NoteTakerWidget : ModuleWidget {
	NoteTakerWidget(NoteTaker *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/NoteTaker.svg")));

        module->display = new NoteTakerDisplay(Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2), // pos
                 Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9), module);  // size
        addChild(module->display);
        module->horizontalWheel = ParamWidget::create<HorizontalWheel>(
                Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
                module, NoteTaker::HORIZONTAL_WHEEL, 0.0, 100.0, 0.0);
		addParam(module->horizontalWheel);
        // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
        module->verticalWheel = ParamWidget::create<VerticalWheel>(
                Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50),
                module, NoteTaker::VERTICAL_WHEEL, 0.0, 127.0, 60.0);
		addParam(module->verticalWheel);

		addInput(Port::create<PJ301MPort>(Vec(140, 306), Port::INPUT, module, NoteTaker::V_OCT_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(172, 306), Port::INPUT, module, NoteTaker::CLOCK_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(204, 306), Port::INPUT, module, NoteTaker::RESET_INPUT));

        for (int i = 0; i < 4; ++i) {
		    addOutput(Port::create<PJ301MPort>(Vec(12 + i * 32, 306), Port::OUTPUT, module,
                    NoteTaker::CV1_OUTPUT + i));
		    addOutput(Port::create<PJ301MPort>(Vec(12 + i * 32, 338), Port::OUTPUT, module,
                    NoteTaker::GATE1_OUTPUT + i));
        }

		addParam(ParamWidget::create<LEDButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(204.4f, 176.4f), module, NoteTaker::RUNNING_LIGHT));

        module->durationButton = ParamWidget::create<DurationButton>(Vec(30, 202),
                module, NoteTaker::DURATION_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->durationButton);
        addParam(ParamWidget::create<InsertButton>(Vec(62, 202), module, NoteTaker::INSERT_BUTTON,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<SelectButton>(Vec(94, 202), module, NoteTaker::EXTEND_BUTTON,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<PartButton>(Vec(126, 202), module, NoteTaker::PART_BUTTON,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<RestButton>(Vec(158, 202), module, NoteTaker::REST_BUTTON,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(190, 202), module, NoteTaker::BUTTON_6,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(30, 252), module, NoteTaker::BUTTON_7,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(62, 252), module, NoteTaker::BUTTON_8,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(94, 252), module, NoteTaker::BUTTON_9,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(126, 252), module, NoteTaker::BUTTON_10,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(158, 252), module, NoteTaker::BUTTON_11,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<NoteTakerButton>(Vec(190, 252), module, NoteTaker::BUTTON_12,
                0.0f, 1.0f, 0.0f));
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = Model::create<NoteTaker, NoteTakerWidget>("NoteTaker", "NoteTakerWidget", "Note Taker", OSCILLATOR_TAG);

// gear drawing : https://fiddle.skia.org/c/cd4e81fc3446c3437bb28de39f5df3a6
// width: 512 ; height : 256 ; animation checked, duration 3
#if 0
    SkPaint p;
    int segments = 40;
    float depth = 4;  // tooth depth
    float radius = 228;
    float height = 45;
    canvas->translate(radius + depth * 2, (radius + depth * 2) / 3);
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    SkPoint endTI = {0, 0}, endTO = endTI, endBI = endTI, endBO = endTI;
    for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 2; ++i) {
        float angle = (i + frame * 2) * 2 * M_PI / segments;
        SkVector a = { cosf(angle), sinf(angle) };
        float scaleI = radius - depth;
        float scaleO = radius + depth;
        SkPoint startTI = endTI;
        endTI = {a.fX * scaleI, a.fY * scaleI / 3};
        SkPoint startTO = endTO;
        endTO = {a.fX * scaleO, a.fY * scaleO / 3};
        SkPoint startBI = endBI;
        endBI = {endTI.fX, endTI.fY + height};
        SkPoint startBO = endBO;
        endBO = {endTO.fX, endTO.fY + height};
        if (startTI.fX != 0 || startTI.fY != 0) {
            if (i & 1) {
                canvas->drawLine(startTI, endTI, p);
                canvas->drawLine(endTI, endTO, p);
                SkPoint clipS = startBI;
                SkPoint clipE = endBI;
                float diff = clipS.fX - startBO.fX;
                SkVector slope = clipE - clipS;
                if (diff > 0) {
                    clipS.fX = startBO.fX;
                    clipS.fY -= slope.fY * diff / slope.fX;
                }
                diff = clipE.fX - endBO.fX;
                if (diff < 0) {
                    clipE.fX = endBO.fX;
                    clipE.fY -= slope.fY * diff / slope.fX;
                }
                canvas->drawLine(clipS, clipE, p);
            } else {
                canvas->drawLine(startTO, endTO, p);
                canvas->drawLine(endTO, endTI, p);
                canvas->drawLine(startTO, startBO, p);
                canvas->drawLine(endTO, endBO, p);
                if (startTI.fX > startTO.fX) {
                    canvas->drawLine(startTI, startBI, p);
                    canvas->drawLine(startBI, startBO, p);
                }
                if (endTI.fX < endTO.fX) {
                    canvas->drawLine(endTI, endBI, p);
                    canvas->drawLine(endBI, endBO, p);
                }
                canvas->drawLine(startBO, endBO, p);
            }
        }
    }
#endif

#if 0
// draw button
canvas->translate(10, 10);
    SkPaint p;
p.setAntiAlias(true);
    SkPath path;
path.moveTo(23, 0);
path.lineTo(26, 9);
path.lineTo(26, 58);
path.lineTo(7, 58);
path.lineTo(4, 49);
path.lineTo(23, 50);
p.setColor(0xffdddddd);
canvas->drawPath(path, p);
p.setColor(0xff000000);
canvas->drawRect({3, 0, 23, 50}, p);
p.setColor(0xff777777);
canvas->drawRect({0, 4, 19, 52}, p);
path.reset();
path.moveTo(0, 4);
path.lineTo(3, 1);
path.lineTo(22, 1);
path.lineTo(19, 4);
p.setColor(0xff555555);
canvas->drawPath(path, p);
path.reset();
path.moveTo(19, 4);
path.lineTo(22, 1);
path.lineTo(22, 49);
path.lineTo(19, 52);
p.setColor(0xff999999);
canvas->drawPath(path, p);
#endif
