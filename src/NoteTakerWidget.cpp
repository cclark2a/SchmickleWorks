#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"

struct DisplayBevel : Widget {
    DisplayBevel(NoteTakerDisplay* d)
        : display(d) {
        this->box.pos = Vec(RACK_GRID_WIDTH - 2, RACK_GRID_WIDTH * 2 - 2);
        this->box.size = Vec(RACK_GRID_WIDTH * 12 + 4, RACK_GRID_WIDTH * 9 + 4);
    }

    void draw(NVGcontext* vg) override {
        nvgTranslate(vg, 2, 2);
        display->drawBevel(vg);
    }

    NoteTakerDisplay* display;
};

struct NoteTakerWidget : ModuleWidget {
    NoteTakerWidget(NoteTaker *module) : ModuleWidget(module) {
        setPanel(SVG::load(assetPlugin(pluginInstance, "res/NoteTaker.svg")));
        std::string musicFontName = assetPlugin(pluginInstance, "res/MusiSync3.ttf");
        std::string textFontName = assetPlugin(pluginInstance, "res/leaguegothic-regular-webfont.ttf");
        auto fb = module->displayFrameBuffer = new FramebufferWidget();
        fb->box.pos = Vec();
	fb->box.size = Vec(RACK_GRID_WIDTH * 14, RACK_GRID_WIDTH * 12);
        fb->dirty = true;
        module->display = new NoteTakerDisplay(Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2), // pos
                 Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9),  // size
                 module, fb, musicFontName, textFontName);
        module->display->fb = fb;
        fb->addChild(module->display);
        this->addChild(fb);
        auto bevel = new DisplayBevel(module->display);
        panel->addChild(bevel);
        module->horizontalWheel = createParam<HorizontalWheel>(
                Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
                module, NoteTaker::HORIZONTAL_WHEEL, 0.0, 100.0, 0.0);
		addParam(module->horizontalWheel);
        // vertical wheel is horizontal wheel (+x) rotated ccw (-y); value and limits are negated
        module->verticalWheel = createParam<VerticalWheel>(
                Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50),
                module, NoteTaker::VERTICAL_WHEEL, 0.0, 127.0, 60.0);
		addParam(module->verticalWheel);

        addInput(createPort<PJ301MPort>(Vec(140, 306), PortWidget::INPUT, module, 
                NoteTaker::V_OCT_INPUT));
        addInput(createPort<PJ301MPort>(Vec(172, 306), PortWidget::INPUT, module, 
                NoteTaker::CLOCK_INPUT));
        addInput(createPort<PJ301MPort>(Vec(204, 306), PortWidget::INPUT, module, 
                NoteTaker::RESET_INPUT));
        addOutput(createPort<PJ301MPort>(Vec(172, 338), PortWidget::OUTPUT, module, 
                NoteTaker::CLOCK_OUTPUT));
        addOutput(createPort<PJ301MPort>(Vec(204, 338), PortWidget::OUTPUT, module, 
                NoteTaker::EOS_OUTPUT));

        for (unsigned i = 0; i < CV_OUTPUTS; ++i) {
            addOutput(createPort<PJ301MPort>(Vec(12 + i * 32, 306), PortWidget::OUTPUT, module,
                    NoteTaker::CV1_OUTPUT + i));
            addOutput(createPort<PJ301MPort>(Vec(12 + i * 32, 338), PortWidget::OUTPUT, module,
                    NoteTaker::GATE1_OUTPUT + i));
        }

        module->runButton = createParam<RunButton>(Vec(200, 172),
                 module, NoteTaker::RUN_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->runButton);

        module->selectButton = createParam<SelectButton>(Vec(30, 202),
                module, NoteTaker::EXTEND_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->selectButton);
        module->insertButton = createParam<InsertButton>(Vec(62, 202),
                module, NoteTaker::INSERT_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->insertButton);
        module->cutButton = createParam<CutButton>(Vec(94, 202),
                module, NoteTaker::CUT_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->cutButton);
        module->restButton = createParam<RestButton>(Vec(126, 202),
                module, NoteTaker::REST_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->restButton);
        module->partButton = createParam<PartButton>(Vec(158, 202),
                module, NoteTaker::PART_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->partButton);
        module->fileButton = createParam<FileButton>(Vec(190, 202),
                module, NoteTaker::FILE_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->fileButton);

        module->sustainButton = createParam<SustainButton>(Vec(30, 252),
                module, NoteTaker::SUSTAIN_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->sustainButton);
        module->timeButton = createParam<TimeButton>(Vec(62, 252),
                module, NoteTaker::TIME_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->timeButton);
        module->keyButton = createParam<KeyButton>(Vec(94, 252),
                module, NoteTaker::KEY_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->keyButton);
        module->tieButton = createParam<TieButton>(Vec(126, 252),
                module, NoteTaker::TIE_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->tieButton);
        module->trillButton = createParam<TrillButton>(Vec(158, 252),
                module, NoteTaker::TRILL_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->trillButton);
        module->tempoButton = createParam<TempoButton>(Vec(190, 252),
                module, NoteTaker::TEMPO_BUTTON, 0.0f, 1.0f, 0.0f);
        addParam(module->tempoButton);

        // debug button is hidden to the right of tempo
        addParam(createParam<DumpButton>(Vec(222, 252),
                module, NoteTaker::TEMPO_BUTTON, 0.0f, 1.0f, 0.0f));
    }
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per pluginInstance, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = createModel<NoteTaker, NoteTakerWidget>("SchmickleWorks");
