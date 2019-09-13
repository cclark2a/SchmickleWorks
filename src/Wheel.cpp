#include "Display.hpp"
#include "MakeMidi.hpp"
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

void NoteTakerWheel::onDragEnd(const event::DragEnd& e) {
    Knob::onDragEnd(e);
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

template<class TWheel>
std::string WidgetToolTip<TWheel>::getDisplayValueString() {
    if (!widget) {
        return "!uninitialized";
    }
    if (defaultLabel.empty()) {
        defaultLabel = label;
    }
    auto ntw = widget->ntw();
    if (ntw->sustainButton->ledOn()) {
        label = "Gate";
    } else if (ntw->selectButton->isSingle()) {
        label = "Insert";
    } else if (ntw->selectButton->isExtend()){
        label = "Select";
    } else if (ntw->tieButton->ledOn()) {
        label = "Note Group";
    } else if (ntw->fileButton->ledOn()) {
        label = "Select";
    } else if (ntw->partButton->ledOn()) {
        label = "Select";
    } else {    // running, or off select button state
        label = defaultLabel;
    }
    return "";
}

template struct WidgetToolTip<HorizontalWheel>;
template struct WidgetToolTip<VerticalWheel>;

static std::string leading_zero(int val, int digits) {
    SCHMICKLE(2 == digits || 3 == digits);
    int divisor = digits == 2 ? 10 : 100;
    SCHMICKLE(val < divisor * 10);
    std::string result;
    while (divisor) {
        result += std::to_string(val / divisor);
        val = val % divisor;
        divisor /= 10;
    }
    return result;
}

static std::string midi_to_time_string(int duration, int ppq) {
    float time = MidiToSeconds(duration, ppq);
    time += 0.0005f;  // round
    int seconds = (int) (time);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int fraction = (int) (fmodf(time, 1) * 1000);
    std::string result;
    if (minutes > 99) {
        minutes = 99;
        seconds = 99;
        fraction = 999;
    }
    return leading_zero(minutes, 2) + ":" + leading_zero(seconds, 2) + "."
            + leading_zero(fraction, 3);
}

// to do : show duration as name
// to do : some of the states that return empty strings could return more with some effort...
std::string HorizontalWheelToolTip::getDisplayValueString() {
    std::string result = WidgetToolTip<HorizontalWheel>::getDisplayValueString();
    if (!result.empty()) {
        return result;
    }
    auto ntw = widget->ntw();
    if (ntw->runButton->ledOn()) {
        // to do : show tempo
        return "";
    }
    if (ntw->fileButton->ledOn()) {
        unsigned roundedValue = (unsigned) (this->getValue() + .5);
        SCHMICKLE(roundedValue < ntw->storage.size());
        return "slot " + std::to_string(roundedValue + 1);
    }
    if (ntw->partButton->ledOn()) {
        int roundedValue = widget->part();
        if (-1 == roundedValue) {
            return "all CV/Gate outputs";
        }
        return "CV/Gate output " + std::to_string(roundedValue + 1);
    }
    int value = (int) this->getValue();
    int vertical = (int) ntw->verticalWheel->getValue();
    auto& n = ntw->n();
    if (ntw->slotButton->ledOn()) {
        if (ntw->selectButton->isOff()) {
            switch (vertical) {
                case 2:  // repeat
                    return value <= 1 ? std::string("no repeat") : "repeat "
                            + std::to_string(value) + " times";
                break;
                case 1:  // slot
                // to do : shows 'Time Wheel : 1' ; think of something better
                    return std::to_string(value + 1);
                break;
                case 0: { // stage
                    switch((SlotPlay::Stage) value) {
                        case SlotPlay::Stage::step:
                            return "end immediately";
                        case SlotPlay::Stage::beat:
                            return "end on beat";
                        case SlotPlay::Stage::quarterNote:
                            return "end on quarter note";
                        case SlotPlay::Stage::bar:
                            return "end on bar";
                        case SlotPlay::Stage::song:
                            return "end on song";
                        case SlotPlay::Stage::never:
                            return "end never";
                        default:
                            DEBUG("unexpected slot stage %d", value);
                            _schmickled();
                    }
                } break;
                default:
                    DEBUG("unexpected slot vertical %d", vertical);
                    _schmickled();
            }
        } 
        result = std::to_string(ntw->storage.slotStart + 1);
        if (ntw->selectButton->isExtend()) {
            if (ntw->storage.slotStart + 1 < ntw->storage.slotEnd) {
                result = "slots " + result + " - " + std::to_string(ntw->storage.slotEnd);
            } else {
                result = "slot " + result;
            }
        } else if (ntw->storage.saveZero) {
            result = "before slot 1";
        } else {
            result = "after slot " + result;
        }
        return result;
    }
    if (ntw->sustainButton->ledOn()) {
        std::string prefix;
        auto limit = (NoteTakerChannel::Limit) vertical;
        switch (limit) {
            case NoteTakerChannel::Limit::releaseMax:
                prefix = "release no more than ";
                break;
            case NoteTakerChannel::Limit::releaseMin:
                prefix = "release no less than ";
                break;
            case NoteTakerChannel::Limit::sustainMin:
                prefix = "sustain to at least ";
                break;
            case NoteTakerChannel::Limit::sustainMax:
                prefix = "sustain up to ";
                break;
            default:
                DEBUG("unexpected sustain limit %d", vertical);
                _schmickled();
        }
        int duration = ntw->storage.current().channels[ntw->unlockedChannel()].getLimit(limit);
        return prefix + Notes::FullName(duration, n.ppq);
    }
    if (ntw->tieButton->ledOn()) {
        int roundedValue = (int) (this->getValue() + .5);
        switch (roundedValue) {
            case 0:
            case 3:
                return "no ties or slurs";
            case 1:
                return "no change";
            case 2:
                return "all ties and slurs";
            default:
                DEBUG("unexpected tie %g %d", this->getValue(), roundedValue);
                _schmickled();
        }
    }
    if (n.isEmpty(ntw->selectChannels)) {
        return "";
    }
    const DisplayNote* note = &n.notes[n.selectStart];
    if (ntw->selectButton->isOff()) {
        if (n.selectStart + 1 == n.selectEnd) {
            switch (note->type) {
                case NOTE_ON:
                case REST_TYPE:
                    return Notes::Name(note, n.ppq);
                case KEY_SIGNATURE:
                    return Notes::KeyName(note->key(), value);
                case TIME_SIGNATURE:
                    return vertical ? Notes::TSNumer(note, n.ppq) : Notes::TSDenom(note, n.ppq);
                case MIDI_TEMPO:
                    return "";  // to do : add tempo info
                default:
                    return "";  // nothing for midi header
            }
        }
        // if multiple notes, and all same duration, return that
        const int duration = note->duration;
        if (duration) {
            for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
                if (duration != n.notes[index].duration) {
                    return "(multiple note lengths)";
                }
            }
        }
        return Notes::Name(note, n.ppq);
    }
    // to do : show bar # ?
    int endTime;
    if (ntw->selectButton->isExtend()) {
        result = midi_to_time_string(note->startTime, n.ppq) + " - ";
        unsigned selEndPos = n.selectEndPos(n.selectEnd - 1);
        endTime = n.notes[selEndPos].endTime();
    } else {
        result = "";
        endTime = n.notes[n.selectEnd].startTime;
    }
    return result + midi_to_time_string(endTime, n.ppq);
}

// show pitch as note name
// to do : some of the states that return empty strings could return more with some effort...
std::string VerticalWheelToolTip::getDisplayValueString() {
    std::string result = WidgetToolTip<VerticalWheel>::getDisplayValueString();
    if (!result.empty()) {
        return result;
    }
    int value = (int) widget->getValue();
    // to do : set state in set vertical wheel range and use that state here?
    auto ntw = widget->ntw();
    if (ntw->runButton->ledOn()) {  // show pitch as : transpose C4 to C4
        // to do : detect if key is flat and choose flats?
        return "C4 to " + Notes::SharpName(value);
    }
    if (ntw->fileButton->ledOn()) {
        // to do : incomplete
        return "";
    }
    if (ntw->partButton->ledOn()) {
        // to do : incomplete
        return "";
    }
    if (ntw->slotButton->ledOn()) {
        // to do : shows 'Pitch Wheel: ' think of something better
        return "";
    }
    if (ntw->sustainButton->ledOn()) {
        switch ((NoteTakerChannel::Limit) value) {
            case NoteTakerChannel::Limit::releaseMax:
                return "maximum release";
            case NoteTakerChannel::Limit::releaseMin:
                return "minimum release";
            case NoteTakerChannel::Limit::sustainMin:
                return "minimum sustain";
            case NoteTakerChannel::Limit::sustainMax:
                return "maximum sustain";
            default:
                DEBUG("unexpected sustain limit %d", value);
                _schmickled();
        }
    }
    if (ntw->tieButton->ledOn()) {
        int roundedValue = (int) (this->getValue() + .5);
        switch (roundedValue) {
            case 0:
            case 3:
                return "no triplets";
            case 1:
                return "no change";
            case 2:
                return "all triplets";
            default:
                DEBUG("unexpected triplet %g %d", this->getValue(), roundedValue);
                _schmickled();
        }
    }
    auto& n = ntw->n();
    if (n.isEmpty(ntw->selectChannels)) {
        return "";
    }
    const DisplayNote* note = &n.notes[n.selectStart];
    if (NOTE_ON == note->type && ntw->selectButton->editStart() && !ntw->edit.voice) {
        return "add to chord";        
    }
    if (n.selectStart + 1 == n.selectEnd) {
        switch (note->type) {
            case NOTE_ON:
                if (ntw->selectButton->editEnd() && ntw->edit.voice) {
                    value = note->pitch();
                }
                return Notes::SharpName(value); // to do : use flat if preceded by flat key?
            case KEY_SIGNATURE:
                return Notes::KeyName(value, note->minor());
            case TIME_SIGNATURE:
                return value ? "beats per measure" : "duration of beat";
            case MIDI_TEMPO:
                return "(unused)";
            default:
                return "(unused)"; // for rests, midi header
        }
    } else if (ntw->selectButton->editEnd() && !ntw->edit.voice) {
        return "select chord note";
    }
    return "transpose chord";  // for selected chords, phrases
}
