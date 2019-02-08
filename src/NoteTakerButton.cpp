#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTaker.hpp"

void NoteTakerLEDButton::onDragEnd(EventDragEnd &e) {
    ledOn = !ledOn;
    nModule()->setWheelRange();
    NoteTakerButton::onDragEnd(e);
}

void DurationButton::draw(NVGcontext *vg) {
    NoteTakerLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 1 + af, 39 - af, "e", NULL);
    nvgText(vg, 8 + af, 39 - af, "q", NULL);
    nvgText(vg, 14 + af, 39 - af, "h", NULL);
    if (showLabel) {
        // to do : see if we want to add this sort of hint
        nvgFontFaceId(vg, nModule()->textFont->handle);
        nvgFontSize(vg, 18);
        nvgText(vg, 3 + af, 50 - af, "DUR", NULL);
    }
}

void DurationButton::onDragEnd(EventDragEnd &e) {
    if (!ledOn) {
        nModule()->insertButton->ledOn = false;
    }
    NoteTakerLEDButton::onDragEnd(e);
}

void InsertButton::onDragEnd(EventDragEnd &e) {
    if (!ledOn) {
        nModule()->durationButton->ledOn = false;
        nModule()->copyStart = nModule()->selectStart;
        nModule()->copyEnd = nModule()->selectEnd;
    }
    NoteTakerLEDButton::onDragEnd(e);
}

void PartButton::draw(NVGcontext *vg) {
    NoteTakerLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 8 + af, 32 - af, "q", NULL);
    nvgText(vg, 8 + af, 38 - af, "q", NULL);
    nvgText(vg, 8 + af, 44 - af, "q", NULL);
}


void RestButton::draw(NVGcontext *vg) {
    NoteTakerButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 48);
    nvgText(vg, 8 + af, 46 - af, "Q", NULL);
}

void SelectButton::draw(NVGcontext *vg) {
    NoteTakerLEDButton::draw(vg);
    nvgFontFaceId(vg, nModule()->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 24);
    nvgText(vg, 4 + af, 41 - af, "\u00E0", NULL);
}

void SelectButton::onDragEnd(EventDragEnd &e) {
    if (!ledOn) {
        nModule()->insertButton->ledOn = false;
    }
    NoteTakerLEDButton::onDragEnd(e);
}
