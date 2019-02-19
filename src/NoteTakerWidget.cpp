#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"

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

        addInput(Port::create<PJ301MPort>(Vec(140, 306), Port::INPUT, module, 
                NoteTaker::V_OCT_INPUT));
        addInput(Port::create<PJ301MPort>(Vec(172, 306), Port::INPUT, module, 
                NoteTaker::CLOCK_INPUT));
        addInput(Port::create<PJ301MPort>(Vec(204, 306), Port::INPUT, module, 
                NoteTaker::RESET_INPUT));

        for (unsigned i = 0; i < CV_OUTPUTS; ++i) {
            addOutput(Port::create<PJ301MPort>(Vec(12 + i * 32, 306), Port::OUTPUT, module,
                    NoteTaker::CV1_OUTPUT + i));
            addOutput(Port::create<PJ301MPort>(Vec(12 + i * 32, 338), Port::OUTPUT, module,
                    NoteTaker::GATE1_OUTPUT + i));
        }

        module->runButton = ParamWidget::create<RunButton>(Vec(200, 172),
                 module, NoteTaker::RUN_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->runButton);

        module->selectButton = ParamWidget::create<SelectButton>(Vec(30, 202),
                module, NoteTaker::EXTEND_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->selectButton);
        module->insertButton = ParamWidget::create<InsertButton>(Vec(62, 202),
                module, NoteTaker::INSERT_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->insertButton);
        module->cutButton = ParamWidget::create<CutButton>(Vec(94, 202),
                module, NoteTaker::CUT_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->cutButton);
        module->restButton = ParamWidget::create<RestButton>(Vec(126, 202),
                module, NoteTaker::REST_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->restButton);
        module->partButton = ParamWidget::create<PartButton>(Vec(158, 202),
                module, NoteTaker::PART_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->partButton);
        addParam(ParamWidget::create<EditButton>(Vec(190, 202), module, NoteTaker::BUTTON_6,
                0.0f, 1.0f, 0.0f));

        addParam(ParamWidget::create<EditButton>(Vec(30, 252), module, NoteTaker::BUTTON_7,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<EditButton>(Vec(62, 252), module, NoteTaker::BUTTON_8,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<EditButton>(Vec(94, 252), module, NoteTaker::BUTTON_9,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<EditButton>(Vec(126, 252), module, NoteTaker::BUTTON_10,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<EditButton>(Vec(158, 252), module, NoteTaker::BUTTON_11,
                0.0f, 1.0f, 0.0f));
        addParam(ParamWidget::create<DumpButton>(Vec(190, 252), module, NoteTaker::BUTTON_12,
                0.0f, 1.0f, 0.0f));
        const bool runUnitTest = true;
        if (runUnitTest) {
            extern void UnitTest(NoteTaker* );
            UnitTest(module);
        } else {
            module->runButton->ledOn = true;  // start running after everything is set up
        }
    }
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = Model::create<NoteTaker, NoteTakerWidget>("SchmickleWorks",
        "NoteTakerWidget", "Note Taker", SEQUENCER_TAG);
