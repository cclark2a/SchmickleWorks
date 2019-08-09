#include "Display.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

// frame varies from 0 to 1 to rotate the wheel
void NoteTakerWheel::drawGear(NVGcontext *vg, float frame) const {
    nvgShapeAntiAlias(vg, 1);
    int topcolor = 0xdf;
    int bottom = gearSize.y - 12;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, gearSize.x, bottom);
    nvgFillColor(vg, nvgRGB(topcolor, topcolor, topcolor));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, bottom / 2);
    nvgLineTo(vg, gearSize.x / 2, bottom);
    nvgLineTo(vg, gearSize.x, bottom / 2);
    nvgLineTo(vg, gearSize.x, bottom);
    nvgLineTo(vg, 0, bottom);
    nvgFillColor(vg, nvgRGB(0xaf, 0xaf, 0xaf));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, gearSize.x, bottom);
    nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    float frameWidth = 1;
    nvgStrokeWidth(vg, frameWidth);
	nvgStroke(vg);
    // draw shadow cast by wheel
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, bottom);
    nvgLineTo(vg, gearSize.x, bottom);
    nvgQuadTo(vg, gearSize.x * shadow / 4, bottom + 18, 4, bottom);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x0b));
    nvgFill(vg);
    // draw wheel
    nvgScissor(vg, frameWidth / 2, frameWidth / 2, gearSize.x - frameWidth, gearSize.y);
    const int segments = 40;
    const float depth = 2;  // tooth depth
    const float radius = gearSize.x * 2 / 3;
    const float height = bottom / 2;
    const float slant = 3;
    float endTIx = 0, endTIy = 0, endTOx = 0, endTOy = 0, endBIy = 0, endBOy = 0;
    nvgTranslate(vg, gearSize.x / 2, -radius / slant + 10);
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
        float offcenter = abs(segments / 4 - i - frame * 2) / (float) segments; // <= .25
        int facecolor = 0xF0 - (int) (0x17F * offcenter);
        if (i & 1) {
            // back face of inset tooth
            nvgBeginPath(vg);
            nvgMoveTo(vg, startTIx, startTIy);
            nvgLineTo(vg, endTIx, endTIy);
            nvgLineTo(vg, endTIx, endBIy);
            nvgLineTo(vg, startTIx, startBIy);
            // closest to center is brightest
            nvgFillColor(vg, nvgRGB(facecolor, facecolor, facecolor));
	        nvgFill(vg);
        } else {
            // front face of outset tooth
            nvgBeginPath(vg);
            nvgMoveTo(vg, startTOx, startTOy);
            nvgLineTo(vg, endTOx, endTOy);
            nvgLineTo(vg, endTOx, endBOy);
            nvgLineTo(vg, startTOx, startBOy);
            nvgFillColor(vg, nvgRGB(facecolor, facecolor, facecolor));
	        nvgFill(vg);
            int edgecolor = 0x80 + (int) (0x1FF * offcenter);
            if (startTIx > startTOx) {
                // edge on right
                nvgBeginPath(vg);
                nvgMoveTo(vg, startTIx, startTIy);
                nvgLineTo(vg, startTIx, startBIy);
                nvgLineTo(vg, startTOx, startBOy);
                nvgLineTo(vg, startTOx, startTOy);
                nvgFillColor(vg, nvgRGB(edgecolor, edgecolor, edgecolor));
	            nvgFill(vg);
            }
            if (endTIx < endTOx) {
                // edge on left
                nvgBeginPath(vg);
                nvgMoveTo(vg, endTIx, endTIy);
                nvgLineTo(vg, endTIx, endBIy);
                nvgLineTo(vg, endTOx, endBOy);
                nvgLineTo(vg, endTOx, endTOy);
                nvgFillColor(vg, nvgRGB(edgecolor, edgecolor, edgecolor));
	            nvgFill(vg);
            }
        }
    }
    endTIx = endTIy = 0;
    // accumulate top of outset tooth
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
        if (i & 1) {
            continue;
        }
        nvgMoveTo(vg, startTIx, startTIy);
        nvgLineTo(vg, startTOx, startTOy);
        nvgLineTo(vg, endTOx, endTOy);
        nvgLineTo(vg, endTIx, endTIy);
    }
    nvgFillColor(vg, nvgRGB(topcolor, topcolor, topcolor));
	nvgFill(vg);
    endTIx = endTIy = 0;
    // accumulate edge lines
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
    nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    nvgStrokeWidth(vg, .5f);
	nvgStroke(vg);
}

void NoteTakerWheel::onDragEnd(const event::DragEnd& ) {
//    inUse = false;
    ntw()->displayBuffer->redraw();
    if (debugVerbose) DEBUG("wheel on drag end");
}

void HorizontalWheel::onDragMove(const event::DragMove& e) {
    Knob::onDragMove(e);
    auto ntw = this->getAncestorOfType<NoteTakerWidget>();
    ntw->updateHorizontal();
}

void HorizontalWheel::setDenominator(const DisplayNote& note) {
    this->setLimits(0, 6.99f);   // denom limit 0 to 6 (2^N, 1 to 64)
    this->setValue(note.denominator());
}

int HorizontalWheel::part() const {
    int outputCount = ntw()->nt() ? ntw()->nt()->outputCount() : CV_OUTPUTS;
    float fslot = this->getValue() + .5;
    return fslot < 0 ? -1 : std::min(outputCount, (int) fslot);
}

void VerticalWheel::onDragMove(const event::DragMove& e) {
    Knob::onDragMove(e);
    auto ntw = this->getAncestorOfType<NoteTakerWidget>();
    ntw->updateVertical();
}

WheelBuffer::WheelBuffer(NoteTakerWheel* wheel) {
    fb = new FramebufferWidget();
    fb->dirty = true;
    this->addChild(fb);
    fb->addChild(wheel);
}

// to do : show duration as name
// to do : some of the states that return empty strings could return more with some effort...
std::string HorizontalWheelToolTip::getDisplayValueString() {
    if (!wheel) {
        return "!uninitialized";
    }
    auto ntw = wheel->ntw();
    int value = (int) this->getValue();
    int vertical = (int) ntw->verticalWheel->getValue();
    if (ntw->slotButton->ledOn()) {
        if (ntw->selectButton->isOff()) {
            switch (vertical) {
                case 2:  // repeat
                    return value <= 1 ? std::string("no repeat") : "repeat "
                            + std::to_string(value) + " times";
                break;
                case 1:  // slot
                    return "slot " + std::to_string(value + 1);
                break;
                case 0: { // stage
                    SlotPlay::Stage stage = std::max(SlotPlay::Stage::step, 
                            std::min(SlotPlay::Stage::never, (SlotPlay::Stage) this->getValue()));
                    switch(stage) {
                        case SlotPlay::Stage::step:
                            return "end immediately";
                            break;
                        case SlotPlay::Stage::beat:
                            return "end on beat";
                            break;
                        case SlotPlay::Stage::quarterNote:
                            return "end on quarter note";
                            break;
                        case SlotPlay::Stage::bar:
                            return "end on bar";
                            break;
                        case SlotPlay::Stage::song:
                            return "end on song";
                            break;
                        case SlotPlay::Stage::never:
                            return "end never";
                            break;
                        default:
                            DEBUG("unexpected stage %d", stage);
                            _schmickled();
                    }
                } break;
            }
        }
    }
    auto& n = ntw->n();
    if (n.isEmpty(ntw->selectChannels)) {
        return "";
    }
    const DisplayNote* note = &n.notes[n.selectStart];
    if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                return Notes::KeyName(note->key(), value);
            case TIME_SIGNATURE:
                return vertical ? Notes::TSNumer(note, n.ppq) : Notes::TSDenom(note, n.ppq);
            case MIDI_TEMPO:
                return "";
            default:
                _schmickled(); // incomplete
        }
    }
    return std::to_string(wheel->getValue());
}

// show pitch as note name
// to do : some of the states that return empty strings could return more with some effort...
std::string VerticalWheelToolTip::getDisplayValueString() {
    if (!wheel) {
        return "!uninitialized";
    }
    int value = (int) wheel->getValue();
    // to do : set state in set vertical wheel range and use that state here?
    auto ntw = wheel->ntw();
    if (ntw->runButton->ledOn()) {  // show pitch as : transpose C4 to C4
        // to do : detect if key is flat and choose flats?
        return "C4 to " + Notes::SharpName(value);
    }
    if (ntw->fileButton->ledOn() || ntw->partButton->ledOn()) {
        return "";
    }
    if (ntw->slotButton->ledOn()) {
        return "";
    }
    if (ntw->sustainButton->ledOn()) {
        return "";
    }
    if (ntw->tieButton->ledOn()) {
        return "";
    }
    auto& n = ntw->n();
    if (n.isEmpty(ntw->selectChannels)) {
        return "";
    }
    const DisplayNote* note = &n.notes[n.selectStart];
    if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                return Notes::KeyName(value, note->minor());
            case TIME_SIGNATURE:
                return value ? "beats per measure" : "duration of beat";
            case MIDI_TEMPO:
                return "";
            default:
                _schmickled(); // incomplete
        }
    }
    if (!ntw->selectButton->ledOn()) {
        return Notes::SharpName(value);
    } else {
        return "";
    }
    return std::to_string(value);
}
