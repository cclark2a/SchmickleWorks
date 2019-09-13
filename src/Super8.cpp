#include "plugin.hpp"
#include "SchmickleWorks.hpp"

// to do : add fun feature:
//         set color of notes and outs to match color of cable connected to v/o out, per channel

struct Super8 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
        CV5_OUTPUT,
        CV6_OUTPUT,
        CV7_OUTPUT,
        CV8_OUTPUT,
        GATE5_OUTPUT,
        GATE6_OUTPUT,
        GATE7_OUTPUT,
        GATE8_OUTPUT,
        VELOCITY1_OUTPUT,
        VELOCITY2_OUTPUT,
        VELOCITY3_OUTPUT,
        VELOCITY4_OUTPUT,
        VELOCITY5_OUTPUT,
        VELOCITY6_OUTPUT,
        VELOCITY7_OUTPUT,
        VELOCITY8_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Super8Data producer;
	Super8Data consumer;

	Super8() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        leftExpander.producerMessage = &producer;
        leftExpander.consumerMessage = &consumer;
	}
    
    void dataFromJson(json_t* root) override {
        json_t* voiceCounts = json_object_get(root, "voiceCounts");
        size_t index;
        json_t* value;
        json_array_foreach(voiceCounts, index, value) {
            outputs[CV5_OUTPUT + index].setChannels(json_integer_value(value));
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* voices = json_array();
        for (unsigned chan = 0; chan < EXPANSION_OUTPUTS - CV_OUTPUTS; ++chan) {
            json_array_append_new(voices, json_integer(outputs[CV5_OUTPUT + chan].getChannels()));
        }
        json_object_set_new(root, "voiceCounts", voices);
        return root;
    }

	void process(const ProcessArgs& args) override {
		if (leftExpander.module && leftExpander.module->model == modelNoteTaker) {
			Super8Data* message = (Super8Data*) leftExpander.consumerMessage;
			for (unsigned index = 0; index < 4; index++) {
                unsigned voiceCount = message->exChannels[CV_OUTPUTS + index];
                outputs[CV5_OUTPUT + index].setChannels(voiceCount);
                outputs[GATE5_OUTPUT + index].setChannels(voiceCount);
                for (unsigned voice = 0; voice < voiceCount; ++voice) {
                    outputs[CV5_OUTPUT + index].setVoltage(message->exCv[index][voice], voice);
				    outputs[GATE5_OUTPUT + index].setVoltage(message->exGate[index][voice], voice);
                }
			}
			for (unsigned index = 0; index < 8; index++) {
                unsigned voiceCount = message->exChannels[index];
                outputs[VELOCITY1_OUTPUT + index].setChannels(voiceCount);
                for (unsigned voice = 0; voice < voiceCount; ++voice) {
				    outputs[VELOCITY1_OUTPUT + index].setVoltage(message->exVelocity[index][voice],
                            voice);
                }
            }
		}
	}
};

struct Super8Widget : ModuleWidget {
	Super8Widget(Super8* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Super8.svg")));
        const int top = 28;
        const int spacer = 44;
        for (unsigned index = 0; index < 4; ++index) {
            addOutput(createOutput<PJ301MPort>(Vec(4, top + index * spacer), module,
                    Super8::CV5_OUTPUT + index));
            addOutput(createOutput<PJ301MPort>(Vec(4, top + spacer * 4 + index * spacer), module,
                    Super8::GATE5_OUTPUT + index));
        }
        for (unsigned index = 0; index < 8; ++index) {
            addOutput(createOutput<PJ301MPort>(Vec(32, top + index * spacer), module,
                    Super8::VELOCITY1_OUTPUT + index));
        }
	}
};

Model *modelSuper8 = createModel<Super8, Super8Widget>("Super8");
