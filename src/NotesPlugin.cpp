#include "NotesPlugin.hpp"

using std::vector;

struct NotesModule : Module {
	enum ParamIds {
		PITCH_PARAM,
        DURATION_PARAM,
        SELECT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
        RESET_INPUT,
        V_OCT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_1_OUTPUT,
        GATE_1_OUTPUT,
        NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
    const float CLOCK_UP_THRESHOLD = 3;
    const float CLOCK_DOWN_THRESHOLD = 1;

	vector<float> notes;
    vector<float> durations;
    int currentIndex = 0;
    int editIndex = 0;
    bool clockDown = false;

    NotesModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void NotesModule::step() {
	float pitch = params[PITCH_PARAM].value;
    if ((int) notes.size() > editIndex) {
        notes[editIndex] = pitch;
    }
    if ((int) notes.size() > currentIndex) {
        pitch = notes[currentIndex];
    }
	pitch += inputs[V_OCT_INPUT].value;
    outputs[CV_1_OUTPUT].value = pitch;

    if (clockDown && inputs[CLOCK_INPUT].value > CLOCK_UP_THRESHOLD) {
        ++currentIndex;
        if (currentIndex >= (int) notes.size()) {
            currentIndex = 0;
        }
    }
    clockDown = inputs[CLOCK_INPUT].value < CLOCK_DOWN_THRESHOLD;
}

struct NotesDisplay : TransparentWidget {

    void draw(NVGcontext *vg) override {
    }

    NotesModule* module;
};

NotesModuleWidget::NotesModuleWidget() {
	auto module = new NotesModule();
	setModule(module);
	box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/NotesModule.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    NotesDisplay *display = new NotesDisplay();
    display->module = module;
    display->box.pos = Vec(0, 44);
    display->box.size = Vec(box.size.x, 140);
    addChild(display);

	addParam(createParam<Davies1900hBlackKnob>(Vec(28, 87), module, NotesModule::PITCH_PARAM, -3.0, 3.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(33, 186), module, NotesModule::CLOCK_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(33, 275), module, NotesModule::CV_1_OUTPUT));
    addOutput(createOutput<PJ301MPort>(Vec(63, 275), module, NotesModule::GATE_1_OUTPUT));

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
