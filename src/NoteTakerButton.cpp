#include "NoteTakerButton.hpp"
#include "NoteTaker.hpp"

void DurationButton::draw(NVGcontext *vg) {
    NoteTakerButton::draw(vg);
    nvgFontFaceId(vg, ((NoteTaker*) module)->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 39 - af, "e", NULL);
    nvgText(vg, 8 + af, 39 - af, "q", NULL);
    nvgText(vg, 14 + af, 39 - af, "h", NULL);
    if (showLabel) {
        nvgFontFaceId(vg, ((NoteTaker*) module)->textFont->handle);
        nvgFontSize(vg, 18);
        nvgText(vg, 3 + af, 50 - af, "DUR", NULL);
    }
}

void NoteTakerButton::onDragEnd(EventDragEnd &e) {
    debug("NoteTakerButton onDragEnd\n");
	dirty = true;
    ledOn = !ledOn;
}

void PartButton::draw(NVGcontext *vg) {
    NoteTakerButton::draw(vg);
    nvgFontFaceId(vg, ((NoteTaker*) module)->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 32 - af, "q", NULL);
    nvgText(vg, 8 + af, 38 - af, "q", NULL);
    nvgText(vg, 8 + af, 44 - af, "q", NULL);
}


void RestButton::draw(NVGcontext *vg) {
    NoteTakerButton::draw(vg);
    nvgFontFaceId(vg, ((NoteTaker*) module)->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 48);
    nvgText(vg, 8 + af, 46 - af, "Q", NULL);
}

void SelectButton::draw(NVGcontext *vg) {
    NoteTakerButton::draw(vg);
    nvgFontFaceId(vg, ((NoteTaker*) module)->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "\u00E0", NULL);
}
