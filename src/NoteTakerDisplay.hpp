#pragma once

#include "SchmickleWorks.hpp"
#include <array>

struct NoteTaker;

static constexpr std::array<int, 9> noteDurations = { 24, 48, 64, 96, 144, 192, 288, 384, 576 };

struct NoteTakerDisplay : TransparentWidget {
    NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* m)
        : module(m) {
        box.pos = pos;
        box.size = size;
        musicFont = Font::load(assetPlugin(plugin, "res/MusiSync.ttf"));
        textFont = Font::load(assetPlugin(plugin, "res/leaguegothic-regular-webfont.ttf"));
    }
    
    void draw(NVGcontext *vg) override;

    static unsigned DurationIndex(int duration) {
        for (unsigned i = 0; i < noteDurations.size(); ++i) {
            if (duration <= noteDurations[i]) {
                return i;
            }
        }
        return noteDurations.size() - 1;
    }

    NoteTaker* module;
	std::shared_ptr<Font> musicFont;
	std::shared_ptr<Font> textFont;
};
