#include "SchmickleWorks.hpp"
#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"

using std::vector;

NoteTaker::NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    musicFont = Font::load(assetPlugin(plugin, "res/MusiSync.ttf"));
    textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    NoteTakerMakeMidi maker;
    maker.createDefaultAsMidi(midi);
    auto iter = midi.begin();
    for (int x = 0; x < 30; ++x) {
        debug("%c 0x%02x\n", *iter, *iter);
        iter++;
    }
    NoteTakerParseMidi parser(midi, displayNotes);
    parser.parseMidi();
}

void NoteTaker::wheelBump(bool isHorizontal, bool positive) {
    debug("wheel bump  h=%d p=%d\n", isHorizontal, positive);
    if (isHorizontal) {
        if (!running) {
            if (positive) {
                backupOne();
            } else {
                running = true;
                stepOnce = true;
            }
        }
    } else {
        for (unsigned index = displayFirst; index < displayNotes.size(); ++index) {
            DisplayNote& note = displayNotes[index];
            if (NOTE_ON == note.type && note.cvOn) {
                if (durationButton->ledOn) {
                    int duration = note.duration;
                    bool dotted = duration / 3 * 3 == duration;
                    if (positive && duration > 0) {
                        if (dotted) {   // remove the dot
                            duration = duration * 2 / 3;
                        } else {        // if undotted, go to 1/2 + dot
                            duration = duration * 3 / 4;
                        }
                    } else if (!positive && duration < 127) {
                            if (dotted) {  // remove dot and double
                            duration = duration * 4 / 3;
                            } else {       // if undotted, add dot
                            duration = duration * 3 / 2;
                            }
                    }
                } else {
                    int pitch = note.pitch();
                    if (positive && pitch > 0) {
                        note.setPitch(pitch - 1);
                    } else if (!positive && pitch < 127) {
                        note.setPitch(pitch + 1);
                    }
	                float v_oct = inputs[V_OCT_INPUT].value;
                    outputs[CV1_OUTPUT].value = v_oct + note.pitch() / 12.f;
                }
            }
        }
    }
}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

void NoteTaker::backupOne() {
    unsigned currentStep = displayStep;
    // move to first step prior to current time
    debug("%d backupOne displayStep 1: %d\n", displayStep);
    do {
        while (displayStep > 0 && NOTE_ON != displayNotes[displayStep].type) {
            --displayStep;
        }
        if (!displayStep || displayNotes[displayStep].cvOn) {
            break;
        }
        --displayStep;
    } while (true);
    debug("backupOne displayStep 2: %d\n", displayStep);
    while (displayStep > 0) {
        --displayStep;
        if (NOTE_ON == displayNotes[displayStep].type) {
            break;
        }
    }
    DisplayNote* backNote = &displayNotes[displayStep];
    int priorMidiTime = backNote->startTime;
    debug("backupOne displayStep 3: %d\n", displayStep);
    debug("%d priorMidiTime\n", priorMidiTime);
    while (displayStep > 0 && priorMidiTime == displayNotes[displayStep - 1].startTime) {
        --displayStep;
    }
    if (displayStep == currentStep) {
        fflush(stderr);
        return;
    }
    debug("%g backupOne displayStep 4: %d\n", elapsedSeconds, displayStep);
    elapsedSeconds = priorMidiTime / (stdTimePerQuarterNote * 2.f);
    debug("%g new elapsed seconds\n", elapsedSeconds);
    for (unsigned index = 0; index < activeNotes.size(); ) {
        DisplayNote* note = activeNotes[index];
        if (note->gateOn && NOTE_ON == note->type && note->startTime > priorMidiTime) {
            note->gateOn = false;
            debug("backupOne zero gate: %d\n", index);
            outputs[GATE1_OUTPUT].value = 0;
        }
        if (note->startTime > priorMidiTime) {
            debug("backupOne erase: %d\n", index);
            activeNotes.erase(activeNotes.begin() + index);
        } else {
            ++index;
        }
    }
    unsigned step = displayStep;
    while (step < displayNotes.size() && displayNotes[step].startTime <= priorMidiTime) {
        DisplayNote* note = &displayNotes[step];
        if (NOTE_ON == note->type) {
            debug("backupOne gate on %d\n", step);
            note->gateOn = true;
            outputs[GATE1_OUTPUT].value = 5;
            activeNotes.push_back(note);
        }
        ++step;
    }
    for (unsigned index = 0; index < activeNotes.size(); ++index) {
        DisplayNote* note = activeNotes[index];
        if (NOTE_ON == note->type) {
            if (lastPitchOut) {
                lastPitchOut->cvOn = false;
            }
            note->cvOn = true;
            lastPitchOut = note;
	        float pitch = inputs[V_OCT_INPUT].value;
            debug("backupOne set pitch startTime %d\n", note->startTime);
            outputs[CV1_OUTPUT].value = pitch + note->pitch() / 12.f;
        }
    }
}

void NoteTaker::step() {
	if (runningTrigger.process(params[RUN_BUTTON].value)) {
		running = !running;
	}
    // read data from display notes to determine pitch
    // note on event start changes cv and sets gate high
    // note on event duration sets gate low
    if (running) {
	    float deltaTime = engineGetSampleTime();

	    // Compute the frequency from the pitch parameter and input
	    float pitch = inputs[V_OCT_INPUT].value;

        elapsedSeconds += deltaTime;
        // to do: use info in midi header, time signature, tempo to get this right
        int midiTime = stdTimePerQuarterNote * elapsedSeconds * 2;
        for (unsigned index = 0; index < activeNotes.size(); ) {
            DisplayNote* note = activeNotes[index];
            if (note->gateOn && NOTE_ON == note->type
                    && note->startTime + note->duration == midiTime) {
                if (0) debug("%g %d set gate to zero time=%d pitch=%d\n", elapsedSeconds, midiTime,
                        note->startTime, note->pitch());
                note->gateOn = false;
                outputs[GATE1_OUTPUT].value = 0;
            }
            if (note->startTime + note->duration < midiTime) {
                if (0) debug("%g %d erase time=%d pitch=%d\n", elapsedSeconds, midiTime,
                        note->startTime, note->pitch());
                activeNotes.erase(activeNotes.begin() + index);
            } else {
                ++index;
            }
        }
        while (displayStep < displayNotes.size() && displayNotes[displayStep].startTime <= midiTime) {
            DisplayNote* note = &displayNotes[displayStep];
            if (NOTE_ON == note->type) {
                if (0) debug("%g %d add to active time=%d pitch=%d\n", elapsedSeconds, midiTime,
                        note->startTime, note->pitch());
                note->gateOn = true;
                outputs[GATE1_OUTPUT].value = 5;
                activeNotes.push_back(note);
                if (stepOnce) {
                    running = false;
                    stepOnce = false;
                }
            }
            ++displayStep;
        }
        static float debugLast = 0;
        for (unsigned index = 0; index < activeNotes.size(); ++index) {
            DisplayNote* note = activeNotes[index];
            if (NOTE_ON == note->type) {
                if (elapsedSeconds - .25f > debugLast) {
                    if (0) debug("%g %d set pitch %g %g\n", elapsedSeconds, midiTime,
                            pitch, note->pitch() / 12.f);
                    debugLast = elapsedSeconds;
                }
                if (lastPitchOut) {
                    lastPitchOut->cvOn = false;
                }
                note->cvOn = true;
                lastPitchOut = note;
                outputs[CV1_OUTPUT].value = pitch + note->pitch() / 12.f;
            }
        }
        if (displayStep == displayNotes.size() && activeNotes.empty()) {
            if (0) debug("%g %d reset elapsed seconds\n", elapsedSeconds, midiTime);
            elapsedSeconds = 0;  // to do : don't repeat unconditionally?
            debugLast = 0;
            displayStep = 0;
        }
    }
    lights[RUNNING_LIGHT].value = running;
}

struct NoteTakerWidget : ModuleWidget {
	NoteTakerWidget(NoteTaker *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/NoteTaker.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        NoteTakerDisplay *display = new NoteTakerDisplay();
        display->module = (NoteTaker*) module;
        display->box.pos = Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2);
        display->box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9);
        addChild(display);
        ParamWidget* horizontalWheel = ParamWidget::create<HorizontalWheel>(
                Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
                module, NoteTaker::HORIZONTAL_WHEEL, 0.0, 100.0, 0.0);
		addParam(horizontalWheel);
        ParamWidget* verticalWheel = ParamWidget::create<VerticalWheel>(
                Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50),
                module, NoteTaker::VERTICAL_WHEEL, 0.0, 127.0, 60.0);
		addParam(verticalWheel);

		addInput(Port::create<PJ301MPort>(Vec(140, 306), Port::INPUT, module, NoteTaker::V_OCT_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(172, 306), Port::INPUT, module, NoteTaker::CLOCK_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(204, 306), Port::INPUT, module, NoteTaker::RESET_INPUT));

		addOutput(Port::create<PJ301MPort>(Vec(12, 306), Port::OUTPUT, module, NoteTaker::CV1_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(44, 306), Port::OUTPUT, module, NoteTaker::CV2_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(76, 306), Port::OUTPUT, module, NoteTaker::CV3_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(108, 306), Port::OUTPUT, module, NoteTaker::CV4_OUTPUT));

		addOutput(Port::create<PJ301MPort>(Vec(12, 338), Port::OUTPUT, module, NoteTaker::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(44, 338), Port::OUTPUT, module, NoteTaker::GATE2_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(76, 338), Port::OUTPUT, module, NoteTaker::GATE3_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(108, 338), Port::OUTPUT, module, NoteTaker::GATE4_OUTPUT));

		addParam(ParamWidget::create<LEDButton>(Vec(200, 172), module, NoteTaker::RUN_BUTTON, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(204.4f, 176.4f), module, NoteTaker::RUNNING_LIGHT));

        DurationButton* durationButton = ParamWidget::create<DurationButton>(Vec(30, 202),
                module, NoteTaker::DURATION_BUTTON, 0.0f, 1.0f, 0.0f);
        ((NoteTaker*) module)->durationButton = durationButton;
        addParam(durationButton);
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
