#include "SchmickleWorks.hpp"


struct NoteTaker : Module {
	enum ParamIds {
		PITCH_PARAM,
        PITCH_SLIDER,
        DURATION_SLIDER,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
        PITCH_OUTPUT,
        GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	float phase = 0.0;
	float blinkPhase = 0.0;

	NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void NoteTaker::step() {
	// Implement a simple sine oscillator
	float deltaTime = engineGetSampleTime();

	// Compute the frequency from the pitch parameter and input
	float pitch = params[PITCH_PARAM].value;
	pitch += inputs[PITCH_INPUT].value;

    bool gate = fmodf(blinkPhase, 0.25) > 0.125;
    outputs[GATE_OUTPUT].value = gate ? 5 : 0;

    // hack to reuse blinkPhase to change pitch
    if (blinkPhase < 0.25) {
        ;
    } else if (blinkPhase < 0.5) {
        pitch += 4/12.f;
    } else if (blinkPhase < 0.75) {
        pitch += 7/12.f;
    } else {
        pitch += 11/12.f;
    }
	pitch = clamp(pitch, -4.0f, 4.0f);
    outputs[PITCH_OUTPUT].value = pitch;

	// The default pitch is C4
	float freq = 261.626f * powf(2.0f, pitch);

	// Accumulate the phase
	phase += freq * deltaTime;
	if (phase >= 1.0f)
		phase -= 1.0f;

	// Compute the sine output
	float sine = sinf(2.0f * M_PI * phase);
	outputs[SINE_OUTPUT].value = 5.0f * sine;

	// Blink light at 1Hz
	blinkPhase += deltaTime;
	if (blinkPhase >= 1.0f)
		blinkPhase -= 1.0f;
	lights[BLINK_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
}

struct NoteTakerDisplay : TransparentWidget {

    void draw(NVGcontext *vg) override {
	    nvgStrokeWidth(vg, 1.0);
    	nvgBeginPath(vg);
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
	    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
	    nvgStroke(vg);
    	nvgBeginPath(vg);
        for (int staff = 15; staff <= 50; staff += 35) {
            for (int y = staff; y <= staff + 20; y += 5) { 
	            nvgMoveTo(vg, 0, y);
	            nvgLineTo(vg, box.size.x, y);
            }
        }
	    nvgStrokeColor(vg, nvgRGB(0xaa, 0xaa, 0xaa));
	    nvgStroke(vg);
    }

    NoteTaker* module;
};

struct NoteTakerWheel : Knob, FramebufferWidget {
    NoteTakerWheel() {
        box.size.x = 100;
        box.size.y = 12;
    }

    // frame varies from 0 to 1 to rotate the wheel
    void drawGear(NVGcontext *vg, float frame) {
        nvgShapeAntiAlias(vg, 1);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 20, box.size.y);
        nvgLineTo(vg, 0, box.size.y);
        nvgLineTo(vg, 0, 0);
        nvgLineTo(vg, box.size.x, 0);
        nvgLineTo(vg, box.size.x, box.size.y);
        nvgLineTo(vg, box.size.x - 20, box.size.y);
        nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgStrokeWidth(vg, 2);
	    nvgStroke(vg);
        nvgStrokeWidth(vg, .5f);
        nvgScissor(vg, 0, 0, box.size.x, box.size.y * 2);
        const int segments = 40;
        const float depth = 4;  // tooth depth
        const float radius = box.size.x * 2 / 3;
        const float height = box.size.y / 2;
        const float slant = 3;
        float endTIx = 0, endTIy = 0, endTOx = 0, endTOy = 0, endBIy = 0, endBOy = 0;
        nvgTranslate(vg, box.size.x / 2, -radius / slant + 10);
        nvgBeginPath(vg);
        for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 3; ++i) {
            float angle = (i + frame * 2) * 2 * M_PI / segments;
            float ax = cosf(angle), ay = sinf(angle);
            float scaleI = radius - depth;
            float scaleO = radius + depth;
            float startTIx = endTIx, startTIy = endTIy;
            endTIx = ax * scaleI; endTIy = ay * scaleI / slant;
            float startTOx = endTOx, startTOy = endTOy;
            endTOx = ax * scaleO; endTOy = ay * scaleO / slant;
            float startBIy = endBIy;
            endBIy = endTIy + height;
            float startBOy = endBOy;
            endBOy = endTOy + height;
            if (!startTIx && !startTIy) {
                continue;
            }
            if (i > segments / 8 && i < 3 * segments / 8) {
                continue;
            }
            if (i & 1) {
                nvgMoveTo(vg, startTIx, startTIy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgLineTo(vg, endTOx, endTOy);
                // start here: erase frame where wheel sticks out
                nvgLineTo(vg, );
                float clipSx = startTIx, clipSy = startBIy;
                float clipEx = endTIx, clipEy = endBIy;
                float diff = clipSx - startTOx;
                float slopex = clipEx - clipSx, slopey = clipEy - clipSy;
                if (diff > 0) {
                    clipSx = startTOx;
                    clipSy -= slopey * diff / slopex;
                }
                diff = clipEx - endTOx;
                if (diff < 0) {
                   clipEx = endTOx;
                   clipEy -= slopey * diff / slopex;
                }
                nvgMoveTo(vg, clipSx, clipSy);
                nvgLineTo(vg, clipEx, clipEy);
            } else {
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, startTOx, startBOy);
                nvgMoveTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTOx, endBOy);
                if (startTIx > startTOx) {
                    nvgMoveTo(vg, startTIx, startTIy);
                    nvgLineTo(vg, startTIx, startBIy);
                    nvgLineTo(vg, startTOx, startBOy);
                }
                if (endTIx < endTOx) {
                    nvgMoveTo(vg, endTIx, endTIy);
                    nvgLineTo(vg, endTIx, endBIy);
                    nvgLineTo(vg, endTOx, endBOy);
                }
                nvgMoveTo(vg, startTOx, startBOy);
                nvgLineTo(vg, endTOx, endBOy);

        }
	    nvgFill(vg);
        nvgBeginPath(vg);
        for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 3; ++i) {
            float angle = (i + frame * 2) * 2 * M_PI / segments;
            float ax = cosf(angle), ay = sinf(angle);
            float scaleI = radius - depth;
            float scaleO = radius + depth;
            float startTIx = endTIx, startTIy = endTIy;
            endTIx = ax * scaleI; endTIy = ay * scaleI / slant;
            float startTOx = endTOx, startTOy = endTOy;
            endTOx = ax * scaleO; endTOy = ay * scaleO / slant;
            float startBIy = endBIy;
            endBIy = endTIy + height;
            float startBOy = endBOy;
            endBOy = endTOy + height;
            if (!startTIx && !startTIy) {
                continue;
            }
            if (i & 1) {
                nvgMoveTo(vg, startTIx, startTIy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgLineTo(vg, endTOx, endTOy);
                float clipSx = startTIx, clipSy = startBIy;
                float clipEx = endTIx, clipEy = endBIy;
                float diff = clipSx - startTOx;
                float slopex = clipEx - clipSx, slopey = clipEy - clipSy;
                if (diff > 0) {
                    clipSx = startTOx;
                    clipSy -= slopey * diff / slopex;
                }
                diff = clipEx - endTOx;
                if (diff < 0) {
                   clipEx = endTOx;
                   clipEy -= slopey * diff / slopex;
                }
                nvgMoveTo(vg, clipSx, clipSy);
                nvgLineTo(vg, clipEx, clipEy);
            } else {
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, startTOx, startBOy);
                nvgMoveTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTOx, endBOy);
                if (startTIx > startTOx) {
                    nvgMoveTo(vg, startTIx, startTIy);
                    nvgLineTo(vg, startTIx, startBIy);
                    nvgLineTo(vg, startTOx, startBOy);
                }
                if (endTIx < endTOx) {
                    nvgMoveTo(vg, endTIx, endTIy);
                    nvgLineTo(vg, endTIx, endBIy);
                    nvgLineTo(vg, endTOx, endBOy);
                }
                nvgMoveTo(vg, startTOx, startBOy);
                nvgLineTo(vg, endTOx, endBOy);
            }
        }
	    nvgStroke(vg);
    }

    void draw(NVGcontext *vg) override {
        float v = module->params[NoteTaker::PITCH_PARAM].value + value;
        drawGear(vg, fmodf(v, 1));
    }

    void onMouseDown(EventMouseDown &e) override {
       printf("called\n");
       fflush(stdout);
	   FramebufferWidget::onMouseDown(e);
    }

    void step() override {
        if (dirty) {

        }
        FramebufferWidget::step();
    }

    void onChange(EventChange &e) override {
        dirty = true;
	    Knob::onChange(e);
    }
};

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
        ParamWidget* wheel = ParamWidget::create<NoteTakerWheel>(
                Vec(RACK_GRID_WIDTH * 7 - 50,
                    RACK_GRID_WIDTH * 11.5f),
                module, NoteTaker::PITCH_SLIDER,
                -3.0, 3.0, 0.0);
		addParam(wheel);
#if 0
        addParam(ParamWidget::create<BefacoSlidePot>(Vec(display->box.pos.x + display->box.size.x / 2,
                display->box.pos.y + display->box.size.y + 10), module, NoteTaker::DURATION_SLIDER,
                -3.0, 3.0, 0.0));
#endif
		addInput(Port::create<PJ301MPort>(Vec(16, 226), Port::INPUT, module, NoteTaker::PITCH_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(50, 226), Port::OUTPUT, module, NoteTaker::PITCH_OUTPUT));

		addOutput(Port::create<PJ301MPort>(Vec(16, 315), Port::OUTPUT, module, NoteTaker::SINE_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(50, 315), Port::OUTPUT, module, NoteTaker::GATE_OUTPUT));

		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(141, 269), module, NoteTaker::BLINK_LIGHT));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(128, 317), module, 
                NoteTaker::PITCH_PARAM, -3.0, 3.0, 0.0));
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

