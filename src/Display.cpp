#include "Cache.hpp"
#include "Display.hpp"
#include "Button.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

// to do: bug: eighth notes are beamed across bar in 2/4 time

static void draw_stem(NVGcontext*vg, float x, int ys, int ye) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + .4, ys);
    nvgLineTo(vg, x + .4, ye);
    nvgStrokeWidth(vg, .625);
    nvgStroke(vg);
}

//  major: G D A E B F# C#
const uint8_t trebleSharpKeys[] = { 29, 32, 28, 31, 34, 30, 33 };
//  major: F Bb Eb Ab Db Gb Cb
const uint8_t trebleFlatKeys[] =  { 33, 30, 34, 31, 35, 32, 36 };
const uint8_t bassSharpKeys[] =   { 43, 46, 42, 45, 48, 44, 47 };
const uint8_t bassFlatKeys[] =    { 47, 44, 48, 45, 49, 46, 50 };

// C major only, for now
// Given a MIDI pitch 0 - 127, looks up staff line position and presence of accidental
const StaffNote sharpMap[] = {
//     C        C#       D        D#       E        F        F#       G        G#       A        A#       B
    {74, 0}, {74, 1}, {73, 0}, {73, 1}, {72, 0}, {71, 0}, {71, 1}, {70, 0}, {70, 1}, {69, 0}, {69, 1}, {68, 0}, // C-1
    {67, 0}, {67, 1}, {66, 0}, {66, 1}, {65, 0}, {64, 0}, {64, 1}, {63, 0}, {63, 1}, {62, 0}, {62, 1}, {61, 0}, // C0
    {60, 0}, {60, 1}, {59, 0}, {59, 1}, {58, 0}, {57, 0}, {57, 1}, {56, 0}, {56, 1}, {55, 0}, {55, 1}, {54, 0}, // C1
    {53, 0}, {53, 1}, {52, 0}, {52, 1}, {51, 0}, {50, 0}, {50, 1}, {49, 0}, {49, 1}, {48, 0}, {48, 1}, {47, 0}, // C2
    {46, 0}, {46, 1}, {45, 0}, {45, 1}, {44, 0}, {43, 0}, {43, 1}, {42, 0}, {42, 1}, {41, 0}, {41, 1}, {40, 0}, // C3
    {39, 0}, {39, 1}, {38, 0}, {38, 1}, {37, 0}, {36, 0}, {36, 1}, {35, 0}, {35, 1}, {34, 0}, {34, 1}, {33, 0}, // C4 (middle)
    {32, 0}, {32, 1}, {31, 0}, {31, 1}, {30, 0}, {29, 0}, {29, 1}, {28, 0}, {28, 1}, {27, 0}, {27, 1}, {26, 0}, // C5
    {25, 0}, {25, 1}, {24, 0}, {24, 1}, {23, 0}, {22, 0}, {22, 1}, {21, 0}, {21, 1}, {20, 0}, {20, 1}, {19, 0}, // C6
    {18, 0}, {18, 1}, {17, 0}, {17, 1}, {16, 0}, {15, 0}, {15, 1}, {14, 0}, {14, 1}, {13, 0}, {13, 1}, {12, 0}, // C7
    {11, 0}, {11, 1}, {10, 0}, {10, 1}, { 9, 0}, { 8, 0}, { 8, 1}, { 7, 0}, { 7, 1}, { 6, 0}, { 6, 1}, { 5, 0}, // C8
    { 4, 0}, { 4, 1}, { 3, 0}, { 3, 1}, { 2, 0}, { 1, 0}, { 1, 1}, { 0, 0}                                      // C9
};

const StaffNote flatMap[] = {
//     C        Db       D        Eb       E        F        Gb       G        Ab       A        Bb       B
    {74, 0}, {73, 2}, {73, 0}, {72, 2}, {72, 0}, {71, 0}, {70, 2}, {70, 0}, {69, 2}, {69, 0}, {68, 2}, {68, 0}, // C-1
    {67, 0}, {66, 2}, {66, 0}, {65, 2}, {65, 0}, {64, 0}, {63, 2}, {63, 0}, {62, 2}, {62, 0}, {61, 2}, {61, 0}, // C0
    {60, 0}, {59, 2}, {59, 0}, {58, 2}, {58, 0}, {57, 0}, {56, 2}, {56, 0}, {55, 2}, {55, 0}, {54, 2}, {54, 0}, // C1
    {53, 0}, {52, 2}, {52, 0}, {51, 2}, {51, 0}, {50, 0}, {49, 2}, {49, 0}, {48, 2}, {48, 0}, {47, 2}, {47, 0}, // C2
    {46, 0}, {45, 2}, {45, 0}, {44, 2}, {44, 0}, {43, 0}, {42, 2}, {42, 0}, {41, 2}, {41, 0}, {40, 2}, {40, 0}, // C3
    {39, 0}, {38, 2}, {38, 0}, {37, 2}, {37, 0}, {36, 0}, {35, 2}, {35, 0}, {34, 2}, {34, 0}, {33, 2}, {33, 0}, // C4 (middle)
    {32, 0}, {32, 2}, {31, 0}, {30, 2}, {30, 0}, {29, 0}, {28, 2}, {28, 0}, {27, 2}, {27, 0}, {26, 2}, {26, 0}, // C5
    {25, 0}, {24, 2}, {24, 0}, {23, 2}, {23, 0}, {22, 0}, {21, 2}, {21, 0}, {20, 2}, {20, 0}, {19, 2}, {19, 0}, // C6
    {18, 0}, {17, 2}, {17, 0}, {16, 2}, {16, 0}, {15, 0}, {14, 2}, {14, 0}, {13, 2}, {13, 0}, {12, 2}, {12, 0}, // C7
    {11, 0}, {10, 2}, {10, 0}, { 9, 2}, { 9, 0}, { 8, 0}, { 7, 2}, { 7, 0}, { 6, 2}, { 6, 0}, { 5, 2}, { 5, 0}, // C8
    { 4, 0}, { 3, 2}, { 3, 0}, { 2, 2}, { 2, 0}, { 1, 0}, { 0, 2}, { 0, 0}                                      // C9
};

const char* upFlagNoteSymbols[] = {   "C", "D", "D.", "E", "E.", "F", "F.", "G", "G.", "H", "H.",
                                           "I", "I.", "J", "J.", "K", "K.", "L", "L.", "M" };
const char* downFlagNoteSymbols[] = { "c", "d", "d.", "e", "e.", "f", "f.", "g", "g.", "h", "h.",
                                           "i", "i.", "j", "j.", "k", "k.", "l", "l.", "m" };
const char* upBeamNoteSymbols[] = {   "H", "H", "H.", "H", "H.", "H", "H.", "H", "H.", "H", "H.",
                                           "I", "I.", "J", "J.", "K", "K.", "L", "L.", "M" };
const char* downBeamNoteSymbols[] = { "h", "h", "h.", "h", "h.", "h", "h.", "h", "h.", "h", "h.",
                                           "i", "i.", "j", "j.", "k", "k.", "l", "l.", "m" };
const char* restSymbols[] =         { "o", "p", "p,", "q", "q,", "r", "r,", "s", "s,", "t", "t,",
                                           "u", "u,", "v", "v,", "w", "w,", "x", "x,", "y" };

const char* gmInstrumentPatchMap[] = {
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1 (Rhodes Piano)",
    "Electric Piano 2 (Chorused Piano)",
    "Harpsichord",
    "Clavinet",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer (Santur)",
    "Drawbar Organ (Hammond)",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion (French)",
    "Harmonica",
    "Tango Accordion (Band neon)",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    "Acoustic Bass",
    "Electric Bass (fingered)",
    "Electric Bass (picked)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1 (strings)",
    "String Ensemble 2 (slow strings)",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "SynthBrass 1",
    "SynthBrass 2",

    "Soprano Sax",  // 65
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square wave)",
    "Lead 2 (sawtooth wave)",
    "Lead 3 (calliope)",
    "Lead 4 (chiffer)",
    "Lead 5 (charang)",
    "Lead 6 (voice solo)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age Fantasia)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir space voice)",
    "Pad 5 (bowed glass)",
    "Pad 6 (metallic pro)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes, drops)",
    "FX 8 (sci-fi, star theme)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot",
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

const Color channelColors[] = {
    {0x00, 0x00, 0x00, 0xFF},    // ch 1
    {0x95, 0x00, 0x00, 0xFF},    // ch 2 (was 0xBF0000FF)
    {0x23, 0x23, 0x8C, 0xFF},    // ch 3 (was 0x00007FFF)
    {0x00, 0x64, 0x00, 0xFF},    // ch 4 (was 0x007F00FF)
    {0xDC, 0x78, 0x00, 0xFF},    // ch 5
    {0x00, 0x70, 0x95, 0xFF},    // ch 6
    {0x50, 0x23, 0x39, 0xFF},    // ch 7
    {0xCE, 0xC8, 0x00, 0xFF},    // ch 8
};

DisplayBuffer::DisplayBuffer(const Vec& pos, const Vec& size, NoteTakerWidget* _ntw) {
    mainWidget = _ntw;
    fb = new FramebufferWidget();
    this->redraw();
    this->addChild(fb);
    auto display = new NoteTakerDisplay(pos, size, fb, _ntw);
    fb->addChild(display);
}

DisplayControl::DisplayControl(NoteTakerDisplay* d, NVGcontext* v, bool h)
    : display(d)
    , vg(v)
    , horizontal(h) {
}

// set control offset to bring offscreen into view
// frame time works with any screen refresh rate
void DisplayControl::autoDrift(float value, float frameTime, int visCells) {
    constexpr float scrollRate = 5.6f;
    auto lo = floorf(value);
    auto offset = horizontal ? display->xControlOffset : display->yControlOffset;
    auto original = offset;
    if (lo < offset) {
        offset = std::max(lo, offset - scrollRate * frameTime);
    } else {
        auto hi = ceilf(value - visCells);
        if (hi > offset) {
            offset = std::min(hi, offset + scrollRate * frameTime);
        } else {
            offset = floorf(offset);
        }
    }
    if (original == offset) {
        return;
    }
    if (false && debugVerbose) DEBUG("autodrift old %g new %g", original, offset);
    display->redraw();
    if (horizontal) {
        display->xControlOffset = offset;
    } else {
        display->yControlOffset = offset;
    }
}

void DisplayControl::drawActiveNarrow(unsigned slot) const {
    nvgBeginPath(vg);
    nvgRect(vg, 40 + (slot - display->xControlOffset) * boxWidth,
            display->box.size.y - boxWidth - 5, 5, boxWidth);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgFill(vg);
}

void DisplayControl::drawActive(unsigned start, unsigned end) const {
    nvgBeginPath(vg);
    nvgRect(vg, 40 + (start - display->xControlOffset) * boxWidth,
            display->box.size.y - boxWidth - 5, boxWidth * (end - start), boxWidth);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

void DisplayControl::drawActivePointer(float position, float offset) const {
    float mid = (horizontal ? display->box.size.x : display->box.size.y) / 2;
    float boxMargin = boxWidth + margin;
    // draw rectangle around selected
    float rectEdge =  mid + ((int) (position + 0.5) - offset - 0.5) * boxMargin + margin / 2;
    nvgBeginPath(vg);
    if (horizontal) {
        nvgRect(vg, rectEdge, display->box.size.y - boxWidth - 5, boxWidth, boxWidth);
    } else {
        nvgRect(vg, display->box.size.x - boxWidth - 5, rectEdge, boxWidth, boxWidth);
    }
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
    // draw triangle at edge
    float triMid = mid + (position - offset) * boxMargin;
    if (false && debugVerbose) DEBUG("center %g rectEdge %g triMid %g position %g offset %g boxWidth %g margin %g",
            (horizontal ? display->box.size.x : display->box.size.y) / 2,
            rectEdge, triMid, position, offset, boxWidth, margin);
    nvgBeginPath(vg);
    if (horizontal) {
        float yOff = display->box.size.y - 5;
        nvgMoveTo(vg, triMid, yOff);
        nvgLineTo(vg, triMid + 2, yOff + 4);
        nvgLineTo(vg, triMid - 2, yOff + 4);
    } else {
        float xOff = display->box.size.x - 5;
        nvgMoveTo(vg, xOff, triMid);
        nvgLineTo(vg, xOff + 4, triMid + 2);
        nvgLineTo(vg, xOff + 4, triMid - 2);
    }
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgFill(vg);
}

void DisplayControl::drawEdgeGradient(unsigned position, unsigned firstVisible,
        unsigned lastVisible) const {
    if (firstVisible == position && display->xControlOffset > .5) {  // to do : offscreen with transparency ?
        NVGpaint paint = nvgLinearGradient(vg, boxWidth / 2, 0, boxWidth, 0,
                nvgRGBA(0xFF, 0xFF, 0xFF, 0), nvgRGBA(0xFF, 0xFF, 0xFF, 0xFF));
        nvgBeginPath(vg);
        nvgRect(vg, boxWidth / 2, 0, boxWidth / 2, boxWidth);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    } else if (lastVisible - 1 == position
            && display->xControlOffset + 4.5 < (float) display->ntw()->storage.size()) {
        NVGpaint paint = nvgLinearGradient(vg, 0, 0, boxWidth / 2, 0,
                nvgRGBA(0xFF, 0xFF, 0xFF, 0xFF), nvgRGBA(0xFF, 0xFF, 0xFF, 0));
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, boxWidth / 2, boxWidth);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }
}

void DisplayControl::drawEmpty() const {
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, boxWidth, boxWidth);
    // draw 'x'
    nvgMoveTo(vg, 0, 0);
    nvgLineTo(vg, boxWidth, boxWidth);
    nvgMoveTo(vg, boxWidth, 0);
    nvgLineTo(vg, 0, boxWidth);
    nvgStrokeWidth(vg, 0.5);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgStroke(vg);
}

void DisplayControl::drawEnd() const {
    nvgRestore(vg);
}

void DisplayControl::drawNoteCommon() const {
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, boxWidth, boxWidth);
    nvgStrokeWidth(vg, 1);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgStroke(vg);
    auto ntw = display->ntw();
    nvgFontFaceId(vg, ntw->musicFont());
    nvgFontSize(vg, 16);
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
}

void DisplayControl::drawNumber(const char* prefix, unsigned index) const {
    float textX = boxWidth - 2;
    float textY = 8;
    auto ntw = display->ntw();
    nvgFontFaceId(vg, INT_MAX == index ? ntw->musicFont() : ntw->textFont());
    nvgFontSize(vg, 10);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT);
    std::string label = INT_MAX == index ? "_": prefix + std::to_string(index);
    float bounds[4];
    (void) nvgTextBounds(vg, textX, textY, label.c_str(), NULL, bounds);
    nvgBeginPath(vg);
    nvgRect(vg, bounds[0] - 1, bounds[1], bounds[2] - bounds[0] + 2,
            bounds[3] - bounds[1]);
    nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0x7F));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgText(vg, textX, textY, label.c_str(), NULL);
}

void DisplayControl::drawSlot(unsigned position, unsigned slotIndex, unsigned repeat,
        SlotPlay::Stage stage, unsigned firstVisible, unsigned lastVisible) const {
    nvgSave(vg);
    nvgTranslate(vg, 40 + (position - display->xControlOffset) * boxWidth,
            display->box.size.y - boxWidth - 5);
    auto ntw = display->ntw();
    if (slotIndex >= ntw->storage.size() || ntw->storage.slots[slotIndex].n.isEmpty(ALL_CHANNELS)) {
        this->drawEmpty();
    } else {
        this->drawSlotNote(stage);
    }
    this->drawNumber("", slotIndex + 1);
    if (repeat > 1) {
        nvgSave(vg);
        nvgTranslate(vg, 0, boxWidth / 2);
        this->drawNumber("x", 99 == repeat ? INT_MAX : repeat);
        nvgRestore(vg);
    }
    this->drawEdgeGradient(position, firstVisible, lastVisible);
    nvgRestore(vg);
}

void DisplayControl::drawSlotNote(SlotPlay::Stage stage) const {
    this->drawNoteCommon();
    const char stages[] = " ^tH[\\]";
    nvgText(vg, boxWidth / 2, boxWidth - 7, &stages[(int) stage], &stages[(int) stage] + 1);
}

void DisplayControl::drawStart() const {
    nvgSave(vg);
    nvgScissor(vg, 40 - boxWidth / 2, display->box.size.y - boxWidth - 5, boxWidth * 5, boxWidth);
}

void DisplayControl::drawTie(int position, unsigned index) const {
    nvgSave(vg);
    nvgTranslate(vg, display->box.size.x - boxWidth - 5, position);
    this->drawNoteCommon();
    const char labels[] = {'<', '~', '{', ';'};
    nvgText(vg, boxWidth / 2, boxWidth - 7, &labels[index], &labels[index] + 1);
    nvgRestore(vg);
}

void DisplayControl::drawTriplet(int position, unsigned index) const {
    nvgSave(vg);
    if (false && debugVerbose) DEBUG("drawTriplet %g %d", display->box.size.x - boxWidth - 5, position);
    nvgTranslate(vg, position, display->box.size.y - boxWidth - 5);
    this->drawNoteCommon();
    const char labels[] = {'|', '~', '>', ';'};
    nvgText(vg, boxWidth / 2, boxWidth - 7, &labels[index], &labels[index] + 1);
    nvgRestore(vg);
}

DisplayState::DisplayState(float xas, FramebufferWidget* frameBuffer, int musicFnt,
        unsigned selChan)
    : fb(frameBuffer)
    , xAxisScale(xas)
    , musicFont(musicFnt)
    , selectedChannels(selChan) {
    if (debugVerbose) DEBUG("DisplayState fb %p", fb);
}

DisplayRange::DisplayRange(DisplayState& ds, float boxWidth)
    : state(ds)
    , bw(boxWidth) {
        if (debugVerbose) DEBUG("DisplayRange bw %g xAxisOffset %g", bw, xAxisOffset);
}

void DisplayRange::scroll() {
    static float last = 0;
    float offset = -xAxisOffset - dynamicXOffsetTimer;
    nvgTranslate(state.vg, offset, 0);
    if (offset != last || dynamicXOffsetStep) {
        state.redraw();
    }
    last = offset;
    if (!dynamicXOffsetStep || ((dynamicXOffsetTimer < 0) == (dynamicXOffsetStep < 0))) {
        dynamicXOffsetTimer = dynamicXOffsetStep = 0;
    } else {
        dynamicXOffsetTimer += dynamicXOffsetStep;
    }
}

void DisplayRange::setRange(const Notes& n) {
    oldStart = n.selectStart;
    oldEnd = n.selectEnd;
    invalidate();
}

// editStart is selectButton->editStart()
void DisplayRange::updateRange(const Notes& n, const DisplayCache* cache, bool editStart) {
    const vector<NoteCache>& notes = cache->notes;
    int selectStartXPos = n.xPosAtStartStart();
    int selectEndXPos = n.xPosAtEndEnd(state);
    int selectWidth = selectEndXPos - selectStartXPos;
    int boxWidth = (int) std::ceil(bw);
    int displayEndXPos = (int) (std::ceil(xAxisOffset + bw));
#if DEBUG_DISPLAY_RANGE
    if (debugVerbose) DEBUG("selectStartXPos %d selectEndXPos %d xAxisOffset %g boxWidth %d displayEndXPos %d",
            selectStartXPos, selectEndXPos, xAxisOffset, boxWidth, displayEndXPos);
    if (debugVerbose) DEBUG("old displayStart %u displayEnd %u", displayStart, displayEnd);
#endif
    // note condition to require the first quarter note of a very long note to be visible
    const int displayQuarterNoteWidth = stdTimePerQuarterNote * xAxisScale;
    const int displayStartMargin = editStart ? 0 : displayQuarterNoteWidth;
    const int displayEndMargin = displayQuarterNoteWidth * 2 - displayStartMargin;
    bool startIsVisible = xAxisOffset + displayStartMargin <= selectStartXPos
            && selectStartXPos + displayStartMargin <= displayEndXPos;
    bool endShouldBeVisible = selectEndXPos + displayEndMargin <= displayEndXPos
            || selectWidth > boxWidth;
    const int lastXPosition = notes.back().xPosition;
    bool recomputeDisplayOffset = !startIsVisible || !endShouldBeVisible
            || xAxisOffset > std::max(0, lastXPosition - boxWidth);
    bool recomputeDisplayEnd = recomputeDisplayOffset
            || !displayEnd || notes.size() <= displayEnd
            || cache->startXPos(displayEnd) <= displayEndXPos;
    if (recomputeDisplayOffset) {
        // compute xAxisOffset first; then use that and boxWidth to figure displayStart, displayEnd
        float oldX = xAxisOffset;
        if (n.selectEnd != oldEnd && n.selectStart == oldStart) { // only end moved
            const NoteCache* last = n.lastCache(n.selectEnd);
            xAxisOffset = (NoteTakerDisplay::CacheWidth(*last, state.vg) > boxWidth ?
                n.xPosAtEndStart() :  // show beginning of end
                selectEndXPos - boxWidth) + displayEndMargin;  // show all of end
#if DEBUG_DISPLAY_RANGE
            if (debugVerbose) DEBUG("1 xAxisOffset %g", xAxisOffset);
#endif
        } else if (xAxisOffset > selectStartXPos - displayStartMargin) { // left to start
            xAxisOffset = selectStartXPos - displayStartMargin;
#if DEBUG_DISPLAY_RANGE
            if (debugVerbose) DEBUG("2 xAxisOffset %g", xAxisOffset);
#endif
        } else {    // scroll enough to show start on right
            int selectBoxX = n.xPosAtStartEnd();
            xAxisOffset = selectBoxX - boxWidth + displayEndMargin;
#if DEBUG_DISPLAY_RANGE
            if (debugVerbose) DEBUG("3 xAxisOffset %g selectBoxX %d", xAxisOffset, selectBoxX);
#endif
        }
        xAxisOffset = std::max(0.f, std::min((float) lastXPosition - boxWidth, xAxisOffset));
        dynamicXOffsetTimer = oldX - xAxisOffset;
        if (fabsf(dynamicXOffsetTimer) <= 5) {
            dynamicXOffsetTimer = 0;
        } else {
            dynamicXOffsetStep = -dynamicXOffsetTimer * 70 * state.callInterval / 24;
        }
    }
    SCHMICKLE(xAxisOffset <= std::max(0, lastXPosition - boxWidth));
    if (recomputeDisplayEnd) {
        float displayStartXPos = std::max(0.f, xAxisOffset - displayQuarterNoteWidth);
        displayStart = std::min(notes.size() - 1, (size_t) displayStart);
        while (displayStart && cache->startXPos(displayStart) >= displayStartXPos) {
            displayStart = cache->previous(displayStart);
        }
        do {
            unsigned displayNext = cache->next(displayStart);
            if (cache->startXPos(displayNext) >= displayStartXPos) {
                break;
            }
            displayStart = displayNext;
        } while (displayStart < notes.size());
        displayEndXPos = std::min((float) lastXPosition,
                xAxisOffset + boxWidth + displayQuarterNoteWidth);
        while ((unsigned) displayEnd < notes.size()
                && cache->startXPos(displayEnd) <= displayEndXPos) {
            displayEnd = cache->next(displayEnd);
        }
        displayEnd = std::min(notes.size() - 1, (size_t) displayEnd);
        do {
            unsigned displayPrevious = cache->previous(displayEnd);
            if (cache->startXPos(displayPrevious) <= displayEndXPos) {
                break;
            }
            displayEnd = displayPrevious;
        } while (displayEnd);
#if DEBUG_DISPLAY_RANGE
        if (debugVerbose) DEBUG("displayStartXPos %g displayEndXPos %d", displayStartXPos, displayEndXPos);
        if (debugVerbose) DEBUG("displayStart %u displayEnd %u", displayStart, displayEnd);
#endif
    }
}

NoteTakerDisplay::NoteTakerDisplay(const Vec& pos, const Vec& size, FramebufferWidget* fb,
        NoteTakerWidget* ntw)
    : mainWidget(ntw)
    , slot(&ntw->storage.slots[0])
    , range(state, size.x)
    , state(range.xAxisScale, fb, ntw->musicFont(), ntw->selectChannels)
    , pitchMap(sharpMap)
{
    box.pos = pos;
    box.size = size;
    SCHMICKLE(sizeof(upFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
    SCHMICKLE(sizeof(downFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
}

void NoteTakerDisplay::advanceBar(BarPosition& bar, unsigned index) {
    if (INT_MAX == bar.duration) {
        return;
    }
    const NoteCache& noteCache = this->cache()->notes[index];
    if (noteCache.vStartTime < bar.midiEnd) {
        return;
    }
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
    bar.advance(noteCache);
}

// mark all accidentals in the selected key signature
void NoteTakerDisplay::applyKeySignature() {
    if (!keySignature) {
        return;
    }
    Accidental mark;
    unsigned keySig;
    const uint8_t* accKeys;
    if (keySignature > 0) {
        mark = SHARP_ACCIDENTAL;
        keySig = keySignature;
        accKeys = trebleSharpKeys;
    } else {
        mark = FLAT_ACCIDENTAL;
        keySig = -keySignature;
        accKeys = trebleFlatKeys;
    }
    // mark complete octaves
    for (unsigned index = 0; index < 70; index += 7) {
        for (unsigned inner = 0; inner < keySig; ++inner) {
            uint8_t acc = accKeys[inner] % 7;
            accidentals[index + acc] = mark;
        }
    }
    // mark partial octave
    for (unsigned inner = 0; inner < keySig; ++inner) {
        uint8_t acc = accKeys[inner] % 7;
        if (acc < 75) {
            accidentals[70 + acc] = mark;
        }
    }
}

const DisplayCache* NoteTakerDisplay::cache() const {
    return &slot->cache;
}

float NoteTakerDisplay::CacheWidth(const NoteCache& noteCache, NVGcontext* vg) {
    if (!noteCache.note->isNoteOrRest()) {
        if (TRACK_END == noteCache.note->type) {
            return 0;
        }
        return (&noteCache)[1].xPosition - noteCache.xPosition;
    }
    float bounds[4]; 
    auto& symbols = REST_TYPE == noteCache.note->type ? restSymbols :
            PositionType::none != noteCache.beamPosition || noteCache.chord ?
            StemType::up == noteCache.stemDirection ? upBeamNoteSymbols : downBeamNoteSymbols :
            StemType::up == noteCache.stemDirection ? upFlagNoteSymbols : downFlagNoteSymbols;
    nvgTextBounds(vg, 0, 0, symbols[noteCache.symbol], nullptr, bounds);
    return bounds[2];
}

void NoteTakerDisplay::draw(const DrawArgs& args) {
    auto ntw = this->ntw();
    auto n = this->notes();
    if (false && debugVerbose) {
        static float last[6] = {};
        float t[6];
        nvgCurrentTransform(args.vg, t);
        if (memcmp(last, t, sizeof(last))) {
            DEBUG("draw xform %g %g %g %g %g %g", t[0], t[1], t[2], t[3], t[4], t[5]);
            memcpy(last, t, sizeof(last));
            this->redraw();
            ntw->selectButton->fb()->dirty = true;
        }
    }
    auto vg = state.vg = args.vg;
    state.selectedChannels = ntw->selectChannels;
    if (false && stagedSlot) {
        if (debugVerbose) DEBUG("stagedSlot %s",
                stagedSlot->debugString(stagedSlot - &ntw->storage.slots.front()).c_str());
        slot = stagedSlot;
        stagedSlot = nullptr;
    }
    auto cache = &slot->cache;
    if (slot->invalid) {
        if (debugVerbose) DEBUG("%s slot invalid", __func__);
        CacheBuilder builder(state, n, cache);
        builder.updateXPosition();
        slot->invalid = false;
        range.invalid = true;
    }
    SCHMICKLE(&slot->cache == cache);
    if (false && debugVerbose) {    // to do : why does this ping pong between two sets, sometimes?
        static const DisplayNote* lastNotes = nullptr;
        static const NoteCache* lastCache = nullptr;
        if (lastCache != n->notes.front().cache || lastNotes != cache->notes.front().note) {
            DEBUG("slot %p notes %p cache %p ", slot, &n->notes.front(), slot->cache.notes.front());
            DEBUG("%s n->notes.front().cache %p", __func__, n->notes.front().cache);
            DEBUG("&cache->notes.front() %p", &cache->notes.front());
            lastNotes = cache->notes.front().note;
            lastCache = n->notes.front().cache;
        } 
    }
    SCHMICKLE(n->notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &n->notes.front());
    if (range.invalid) {
        if (debugVerbose && n->notes.front().cache != &cache->notes.front()) {
            DEBUG("n->notes.front().cache %p", n->notes.front().cache);
            DEBUG("&cache->notes.front() %p", &cache->notes.front());
        }
        SCHMICKLE(n->notes.front().cache == &cache->notes.front());
        SCHMICKLE(cache->notes.front().note == &n->notes.front());
        range.updateRange(*n, cache, ntw->selectButton->editStart());
        range.invalid = false;
    }
#if RUN_UNIT_TEST
    if (ntw->nt() && ntw->runUnitTest) { // to do : remove this from shipping code
        UnitTest(ntw, TestType::encode);
        ntw->runUnitTest = false;
        this->redraw();
        return;
    }
#endif
    if (ntw->nt()) {
        float realT = (float) ntw->nt()->getRealSeconds();
        state.callInterval = lastCall ? realT - lastCall :
                settings::frameSwapInterval / APP->window->getMonitorRefreshRate();
        lastCall = realT;    
    }
    nvgSave(vg);
    accidentals.fill(NO_ACCIDENTAL);
// to do : figure out why this doesn't draw?
//         for now, workaround is to append bevel draw as child to svg panel in note taker widget
//    this->drawBevel(vg);
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgFill(vg);
    this->drawStaffLines();
    nvgSave(vg);
    range.scroll();
    this->drawClefs();
    if (!ntw->menuButtonOn() || ntw->tieButton->ledOn()) {
        this->drawSelectionRect();
    }
    BarPosition bar;
    this->setUpAccidentals(bar);
    this->drawNotes(bar);
    this->drawBars(bar);
    nvgRestore(vg);
    this->recenterVerticalWheel();
    if (ntw->fileButton->ledOn()) {
        this->drawFileControl();
    }
    if (ntw->partButton->ledOn()) {
        this->drawPartControl();
    }
    if (ntw->slotButton->ledOn()) {
        this->drawSlotControl();
    }
    if (ntw->sustainButton->ledOn()) {
        this->drawSustainControl();
    }
    if (ntw->tieButton->ledOn()) {
        this->drawTieControl();
    }
    if (ntw->runningWithButtonsOff()) {
        this->drawDynamicPitchTempo();
    }
    nvgRestore(vg);
    Widget::draw(args);
}

void NoteTakerDisplay::drawArc(const BeamPosition& bp, unsigned start, unsigned index) const {
    auto& notes = this->cache()->notes;
    float yOff = bp.slurOffset;
    int midOff = (StemType::up == notes[start].stemDirection ? 2 : -2);
    float xMid = (bp.sx + bp.ex) / 2;
    float yStart = notes[start].yPosition + yOff * 2;
    float yEnd = notes[index].yPosition + yOff * 2;
    float yMid = (yStart + yEnd) / 2;
    float dx = bp.ex - bp.sx;
    float dy = yEnd - yStart;
    float len = sqrt(dx * dx + dy * dy);
    float dyxX = dx / len * midOff;
    float dyxY = dy / len * midOff;
    auto vg = state.vg;
    nvgBeginPath(vg);
    nvgMoveTo(vg, bp.sx - 2, yStart + yOff);
    nvgQuadTo(vg, xMid - dyxY,     yMid + yOff + dyxX,     bp.ex - 2, yEnd + yOff);
    nvgQuadTo(vg, xMid - dyxY * 2, yMid + yOff + dyxX * 2, bp.sx - 2, yStart + yOff);
    nvgFill(vg);
}

void NoteTakerDisplay::drawBarAt(int xPos) {
    auto vg = state.vg;
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos, 36);
    nvgLineTo(vg, xPos, 96);
    nvgStrokeWidth(vg, 0.5);
	nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    nvgStroke(vg);
}

// to do : keep track of where notes are drawn to avoid stacking them on each other
// likewise, avoid drawing ties and slurs on top of notes and each other
// to get started though, draw ties for each note that needs it
void NoteTakerDisplay::drawBarNote(BarPosition& bar, const DisplayNote& note,
        const NoteCache& noteCache, unsigned char alpha) {
    const Accidental lookup[][3]= {
    // next:      no                  #                b           last:
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // no
        { NATURAL_ACCIDENTAL, NO_ACCIDENTAL,    FLAT_ACCIDENTAL }, // #
        { NATURAL_ACCIDENTAL, SHARP_ACCIDENTAL, NO_ACCIDENTAL   }, // b
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // natural
    };
    const StaffNote& pitch = pitchMap[note.pitch()];
    Accidental accidental;
    if (noteCache.accidentalSpace) {
        accidental = lookup[accidentals[pitch.position]][pitch.accidental];
        accidentals[pitch.position] = (Accidental) pitch.accidental;
    } else {
        accidental = NO_ACCIDENTAL;
    }
    this->drawNote(accidental, noteCache, alpha, NOTE_FONT_SIZE);
    bar.addPos(noteCache, CacheWidth(noteCache, state.vg));
}

// to do : whole rest should be centered in measure
void NoteTakerDisplay::drawBarRest(BarPosition& bar, const NoteCache& noteCache,
        int xPos, unsigned char alpha) const {
    const float yPos = 36 * 3 - 49;
    auto vg = state.vg;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgText(vg, xPos, yPos, restSymbols[noteCache.symbol], nullptr);
    bar.addPos(noteCache,CacheWidth(noteCache, vg));
}

// if note crosses bar, should have recorded bar position when drawn as tied notes
void NoteTakerDisplay::drawBars(const BarPosition& bar) {
    for (const auto& p : bar.pos) {
        if (-FLT_MAX == p.second.xMax || (!p.second.useMax && FLT_MAX == p.second.xMin)) {
            continue;
        }
        this->drawBarAt(p.second.useMax ? p.second.xMax : (p.second.xMin + p.second.xMax) / 2);
        if (false && debugVerbose) DEBUG("[%d] drawBars min %g max %g useMax %d",
                p.first, p.second.xMin, p.second.xMax, p.second.useMax);
    }
}

struct BeamState {
    float sx;
    float ex;
    int y;

    BeamState(BeamPosition bp)
        : sx(bp.sx)
        , ex(bp.ex)
        , y(bp.y) {
    }

    void drawOneBeam(NVGcontext* vg, BeamPosition bp) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, sx, y);
        nvgLineTo(vg, ex, y);
        nvgLineTo(vg, ex, y + 3);
        nvgLineTo(vg, sx, y + 3);
        nvgFill(vg);
        y += bp.yOffset;
    }

};

void NoteTakerDisplay::drawBeam(unsigned start, unsigned char alpha) const {
    auto& notes = this->cache()->notes;
    SCHMICKLE(PositionType::left == notes[start].beamPosition);
    SCHMICKLE(notes[start].beamId < this->cache()->beams.size());
    const BeamPosition& bp = this->cache()->beams[notes[start].beamId];
    unsigned index = start;
    int chan = notes[start].channel;
    auto vg = state.vg;
    SetNoteColor(vg, chan, alpha);
    BeamState bs(bp);
    bs.ex += 0.25;
    for (unsigned count = 0; count < bp.beamMin; ++count) {
        bs.drawOneBeam(vg ,bp);
    }
    index = start;
    int yReset = bp.y;
    const NoteCache* prev = nullptr;
    const NoteCache* noteCache = nullptr;
    do {
        SCHMICKLE(chan == notes[index].channel);
        SCHMICKLE(!notes[index].chord);
        prev = noteCache;
        noteCache = &notes[index];
        while (++index < notes.size()
                && (chan != notes[index].channel || notes[index].chord)) {
            ;
        }
        if (index == notes.size()) {
            if (debugVerbose) DEBUG("drawBeam found unbalanced beam");
            _schmickled();
            return;
        }
        const NoteCache& next = notes[index];
        bs.sx = noteCache->xPosition + bp.xOffset;
        draw_stem(vg, bp.sx, noteCache->yPosition + bp.yStemExtend, bp.yLimit);
        bs.sx += 0.25;
        bs.ex = next.xPosition + bp.xOffset + 0.25;
        bs.y = yReset;
        bool fullBeam = true;
        for (unsigned count = bp.beamMin; count < noteCache->beamCount; ++count) {
            if (fullBeam && next.beamCount <= count) {
                if (prev && prev->beamCount >= count) {
                    continue;
                }
                bs.ex = bs.sx + 6;   // draw partial beam on left
                fullBeam = false;
            } 
            bs.drawOneBeam(vg, bp);
        }
        if (PositionType::right == next.beamPosition) { // draw partial beam on right
            bs.ex = next.xPosition + bp.xOffset;
            draw_stem(vg, bs.ex, next.yPosition + bp.yStemExtend, bp.yLimit);
            bs.ex += 0.25;
            bs.sx = bs.ex - 6;
            for (unsigned count = noteCache->beamCount; count < next.beamCount; ++count) {
                bs.drawOneBeam(vg, bp);
            }
            break;
        }
    } while (true);
}

void NoteTakerDisplay::drawClefs() const {
         // draw treble and bass clefs
    auto vg = state.vg;
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgText(vg, 5, 60, "(", NULL);
    nvgFontSize(vg, 36);
    nvgText(vg, 5, 92, ")", NULL);
}

void NoteTakerDisplay::drawFreeNote(const DisplayNote& note, NoteCache* noteCache,
        int xPos, unsigned char alpha) {
    const auto& n = *this->notes();
    const StaffNote& pitch = pitchMap[note.pitch()];
    noteCache->note = &note;
    noteCache->xPosition = xPos;
    noteCache->yPosition = YPos(pitch.position);
    noteCache->vDuration = n.ppq;
    noteCache->stemDirection = StemType::up;
    noteCache->setDurationSymbol(n.ppq);
    this->drawNote((Accidental) pitchMap[note.pitch()].accidental, *noteCache, alpha, 24);
}
            
// to do : when key signature is selected and is c major, show dark gray natural sign
void NoteTakerDisplay::drawKeySignature(unsigned index) {
    const auto& noteCache = this->cache()->notes[index];
    int xPos = noteCache.xPosition;
    auto& note = *noteCache.note;
    this->setKeySignature(note.key());
    const char* mark;
    unsigned keySig;
    const uint8_t* accKeys, * bassKeys;
    auto vg = state.vg;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    if (keySignature > 0) {
        mark = "#";
        keySig = keySignature;
        accKeys = trebleSharpKeys;
        bassKeys = bassSharpKeys;
    } else if (keySignature < 0) {
        mark = "$";
        keySig = -keySignature;
        accKeys = trebleFlatKeys;
        bassKeys = bassFlatKeys;
    } else {    
        if (dynamicCNaturalAlpha > 0) {
            auto nt = ntw()->nt();
            dynamicCNaturalAlpha = std::max(0, std::min(255, 
                    (int) (255 * (dynamicCNaturalTimer - nt->getRealSeconds()) / fadeDuration)));
            nvgFillColor(vg, nvgRGBA(0, 0, 0, dynamicCNaturalAlpha));
            nvgText(vg, xPos, 32 * 3 - 48, "%", NULL);
            nvgText(vg, xPos, 46 * 3 - 48, "%", NULL);
            this->redraw();
        }
        return;
    }
    for (auto keys : { accKeys, bassKeys }) {
        int xKeyPos = xPos;
        for (unsigned index = 0; index < keySig; ++index) {
            nvgText(vg, xKeyPos, keys[index] * 3 - 48, mark, NULL);
            xKeyPos += ACCIDENTAL_WIDTH * range.xAxisScale;
        }
    }
}

void NoteTakerDisplay::drawNote(Accidental accidental,
        const NoteCache& noteCache, unsigned char alpha, int size) const {
    const StaffNote& pitch = pitchMap[noteCache.note->pitch()];
    float yPos = noteCache.yPosition;
    float staffLine = pitch.position * 3 - 51;
    int staffLineCount = 0;
    if (pitch.position < TREBLE_TOP) {
        staffLineCount = (TREBLE_TOP - pitch.position) / 2;
        staffLine += !(pitch.position & 1) * 3;
    } else if (pitch.position > BASS_BOTTOM) {
        staffLineCount = (pitch.position - BASS_BOTTOM) / 2;
        staffLine -= (staffLineCount - 1) * 6 + !(pitch.position & 1) * 3;
    } else if (pitch.position == MIDDLE_C) {
        staffLineCount = 1;
    }
    int xPos = noteCache.xPosition;
    auto vg = state.vg;
    if (staffLineCount) {
    	nvgBeginPath(vg);
        do {
            nvgMoveTo(vg, xPos - 2, staffLine);
            nvgLineTo(vg, xPos + 8, staffLine);
            staffLine += 6;
        } while (--staffLineCount);
        nvgStrokeWidth(vg, 0.5);
        nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgStroke(vg);
    }
    SetNoteColor(vg, noteCache.channel, alpha);
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    if (24 == size) {   // to do : something better than this
        yPos -= 1;
    }
    if  (NO_ACCIDENTAL != accidental) {
        const char accidents[] = " #$%";
        // to do : adjust font so the offsets aren't needed
        nvgText(vg, xPos - 6 - (SHARP_ACCIDENTAL == accidental), yPos + 1,
                &accidents[accidental], &accidents[accidental + 1]);
    }
    auto& symbols = PositionType::none == noteCache.beamPosition && noteCache.drawStaff ?
            StemType::up == noteCache.stemDirection ? upFlagNoteSymbols : downFlagNoteSymbols : 
            StemType::up == noteCache.stemDirection ? upBeamNoteSymbols : downBeamNoteSymbols;
    const char* noteStr = symbols[noteCache.symbol];
    nvgText(vg, xPos, yPos, noteStr, nullptr);
}

void NoteTakerDisplay::drawNotes(BarPosition& bar) {
    auto ntw = this->ntw();
    const auto& n = *this->notes();
    const int outputCount = NoteTaker::OutputCount(ntw->nt());
    for (unsigned index = range.displayStart; index < range.displayEnd; ++index) {
        const NoteCache& noteCache = this->cache()->notes[index];
        if (noteCache.channel >= outputCount) {
            continue;
        }
        const DisplayNote& note = *noteCache.note;
        this->advanceBar(bar, index);
        unsigned char alpha = note.isSelectable(ntw->selectChannels) ? 0xff : 0x7f;
        switch (note.type) {
            case NOTE_ON: {
                if (!noteCache.vDuration) {
                    break;
                }
                this->drawBarNote(bar, note, noteCache, alpha);
                bool drawBeams = PositionType::left == noteCache.beamPosition;
                if (drawBeams) {
                    this->drawBeam(index, alpha);
                }
                if (PositionType::left == noteCache.tripletPosition) {
                    this->drawTuple(index, alpha, drawBeams);
                }
                if (PositionType::left == noteCache.slurPosition) {
                    this->drawSlur(index, alpha);
                }
                if ((PositionType::left == noteCache.tiePosition
                        || PositionType::mid == noteCache.tiePosition)
                        && INT_MAX != noteCache.tieId) {
                    this->drawTie(index, alpha);
                }
            } break;
            case REST_TYPE:
                if (!noteCache.vDuration) {
                    break;
                }
                this->drawBarRest(bar, noteCache, noteCache.xPosition, alpha);
                if (PositionType::left == noteCache.tripletPosition) {
                    this->drawTuple(index, alpha, false);
                }
            break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE: 
                bar.setPriorBars(noteCache);
                this->drawKeySignature(index);
                bar.setMidiEnd(noteCache);
             break;
            case TIME_SIGNATURE: {
                bar.setPriorBars(noteCache);
                bar.setSignature(note, n.ppq);
                bar.setMidiEnd(noteCache);
                auto vg = state.vg;
                float width = TimeSignatureWidth(note, vg, ntw->musicFont());
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                std::string numerator = std::to_string(note.numerator());
                nvgTextAlign(vg, NVG_ALIGN_CENTER);
                int xPos = noteCache.xPosition + width / 2
                        + TIME_SIGNATURE_GAP * range.xAxisScale / 2;
                nvgText(vg, xPos, 32 * 3 - 49, numerator.c_str(), NULL);
                nvgText(vg, xPos, 44 * 3 - 49, numerator.c_str(), NULL);
                std::string denominator = std::to_string(1 << note.denominator());
                nvgText(vg, xPos, 36 * 3 - 49, denominator.c_str(), NULL);
                nvgText(vg, xPos, 48 * 3 - 49, denominator.c_str(), NULL);
            } break;
            case MIDI_TEMPO:
                bar.setPriorBars(noteCache);
                this->drawTempo(noteCache.xPosition, note.tempo(), 0xFF);
                bar.setMidiEnd(noteCache);
            break;
            case TRACK_END:
            break;
            default:
                if (debugVerbose) DEBUG("to do: add type %d\n", note.type);
                _schmickled(); // incomplete
        }
    }
}

void NoteTakerDisplay::drawSelectionRect() {
    // draw selection rect
    auto ntw = this->ntw();
    const auto& n = *this->notes();
    unsigned start = n.selectEndPos(n.selectStart);
    const NoteCache* noteCache;
    while (!(noteCache = n.notes[start].cache) && start < n.notes.size() - 1) {
        ++start;
    }
    SCHMICKLE(noteCache);
    int xStart = noteCache->xPosition - (noteCache->accidentalSpace ? 10 : 2);
    int width = 4;
    int yTop = 0;
    int yHeight = box.size.y;
    auto selectButton = ntw->selectButton;
    unsigned channel = selectButton->editStart() ? 1 << ntw->unlockedChannel() :
            ntw->selectChannels;
    const DisplayNote* signature = nullptr;
    if (n.selectStart + 1 == n.selectEnd) {
        signature = &n.notes[n.selectStart];
        if (!signature->isSignature()) {
            signature = nullptr;
        }
    }
    auto vg = state.vg;
    if (signature) {
        switch (signature->type) {
            case MIDI_TEMPO:
                nvgBeginPath(vg);
                nvgRect(vg, xStart + width, yTop, 30 - width, 12);
                SetSelectColor(vg, channel);
                nvgFill(vg);
                break;
            case TIME_SIGNATURE:
                if (!selectButton->ledOn()) {
                    yTop = (1 - (int) ntw->verticalWheel->getValue()) * 12 + 35;
                    yHeight = 13;
                }
                break;
            default:
                ;
        }
    } else if (ntw->edit.voice) {
        auto& startCache = n.notes[n.selectStart].cache;
        yTop = startCache->yPosition - 6;
        yHeight = 6;
    } 
    if (ntw->menuButtonOn()) {
        int bottom = std::min(yTop + yHeight, (int) box.size.y - 35);
        yHeight = bottom - yTop;
        // to do : replace this with scissor to prevent drawing into display ui area
    }
    if (!selectButton->editStart() && n.selectEnd > 0) {
        auto startCache = n.notes[start].cache;
        xStart = startCache->xPosition - (startCache->accidentalSpace ? 8 : 0);
        unsigned selEndPos = n.selectEndPos(n.selectEnd - 1);
        const NoteCache* endCache;
        while (!(endCache = n.notes[selEndPos].cache) && selEndPos < n.notes.size()) {
            ++selEndPos;
        }
        SCHMICKLE(endCache);
        width = endCache->xPosition - (endCache->accidentalSpace ? 8 : 0) - xStart;
    }
    nvgBeginPath(vg);
    if (0 && debugVerbose) DEBUG("xStart %d yTop %d width %d yHeight %d", xStart, yTop, width, yHeight);
    nvgRect(vg, xStart, yTop, width, yHeight);
    SetSelectColor(vg, channel);
    nvgFill(vg);
}

void NoteTakerDisplay::drawStaffLines() const {
        // draw vertical line at end of staff lines
    float beginEdge = std::max(0.f, 3 - range.xAxisOffset - range.dynamicXOffsetTimer);
    auto vg = state.vg;
    if (beginEdge > 0) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, beginEdge, 36);
        nvgLineTo(vg, beginEdge, 96);
        nvgStrokeColor(vg, nvgRGB(0, 0, 0));
        nvgStrokeWidth(vg, 0.5);
        nvgStroke(vg);
    }
    // draw staff lines
    nvgBeginPath(vg);
    for (int staff = 36; staff <= 72; staff += 36) {
        for (int y = staff; y <= staff + 24; y += 6) { 
	        nvgMoveTo(vg, beginEdge, y);
	        nvgLineTo(vg, box.size.x, y);
        }
    }
	nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    nvgStrokeWidth(vg, 0.5);
	nvgStroke(vg);
}

void NoteTakerDisplay::drawDynamicPitchTempo() {
    auto ntw = this->ntw();
    if (!ntw->menuButtonOn()) {
        auto nt = ntw->nt();
        if (ntw->verticalWheel->hasChanged()) {
            dynamicPitchTimer = nt->getRealSeconds() + fadeDuration;
        }
        auto horizontalWheel = ntw->horizontalWheel;
        if (!this->cache()->leadingTempo
                && horizontalWheel->lastRealValue != horizontalWheel->getValue()) {
            dynamicTempoTimer = nt->getRealSeconds() + fadeDuration;
            horizontalWheel->lastRealValue = horizontalWheel->getValue();
        }
        if ((dynamicTempoAlpha > 0 && dynamicTempoAlpha < 255)
                || (dynamicPitchAlpha > 0 && dynamicPitchAlpha < 255)) {
            this->redraw();
        }
        dynamicTempoAlpha = std::max(0,
                (int) (255 * (dynamicTempoTimer - nt->getRealSeconds()) / fadeDuration));
        dynamicPitchAlpha = std::max(0,
                (int) (255 * (dynamicPitchTimer - nt->getRealSeconds()) / fadeDuration));
        if ((dynamicTempoAlpha > 0 && dynamicTempoAlpha < 255)
                || (dynamicPitchAlpha > 0 && dynamicPitchAlpha < 255)) {
            this->redraw();
        }
    }
    auto vg = state.vg;
    if (dynamicPitchAlpha > 0) {
        const auto& n = *this->notes();
        DisplayNote note(NOTE_ON);
        note.duration = n.ppq;
        NoteCache noteCache(&note);
        note.cache = &noteCache;
        note.setPitch((int) ntw->verticalWheel->getValue());
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 10, 2, 10, box.size.y - 4);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, dynamicPitchAlpha));
        nvgFill(vg);
        this->drawFreeNote(note, &noteCache, box.size.x - 7, dynamicPitchAlpha);
    }
    if (dynamicTempoAlpha > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, 2, 2, 30, 12);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, dynamicTempoAlpha));
        nvgFill(vg);
        this->drawTempo(2, stdMSecsPerQuarterNote, dynamicTempoAlpha);
        this->redraw();
    }
}

void NoteTakerDisplay::drawFileControl() {
    DisplayControl control(this, state.vg);
    auto ntw = this->ntw();
    auto horizontalWheel = ntw->horizontalWheel;
    this->drawVerticalLabel("load", true, upSelected, 0);
    this->drawVerticalLabel("save", true, downSelected, box.size.y - 50);
    this->drawVerticalControl();
    // draw horizontal control
    float fSlot = std::min((float) (ntw->storage.size() - 1), horizontalWheel->getValue());
    int slot = (int) (fSlot + .5);
    control.autoDrift(fSlot, state.callInterval);
    unsigned firstVisible = xControlOffset >= 1 ? (unsigned) xControlOffset - 1 : 0;
    unsigned lastVisible = std::min(ntw->storage.size() - 1, (unsigned) (xControlOffset + 5));
    control.drawStart();
    for (unsigned index = firstVisible; index <= lastVisible; ++index) {
        control.drawSlot(index, index, 0, SlotPlay::Stage::quarterNote, firstVisible,
                lastVisible);
    }
    control.drawEnd();
    control.drawActive(slot, slot + 1);
    SCHMICKLE((unsigned) slot < ntw->storage.size());
    const std::string& name = ntw->storage.slots[slot].filename;
    if (!name.empty()) {
        this->drawName(name);
    }
}

void NoteTakerDisplay::drawName(std::string name) const {
    float bounds[4];
    auto vg = state.vg;
    nvgFontSize(vg, 10);
    nvgFontFaceId(vg, ntw()->textFont());
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    nvgTextBounds(vg, box.size.x / 2, box.size.y - 2, name.c_str(), nullptr, bounds);
    nvgBeginPath(vg);
    nvgRect(vg, bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]);
    nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0xBF));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgText(vg, box.size.x / 2, box.size.y - 2, name.c_str(), NULL);
}

void NoteTakerDisplay::drawPartControl() {
    auto ntw = this->ntw();
    auto horizontalWheel = ntw->horizontalWheel;
    int part = horizontalWheel->part();
    bool lockEnable = part < 0 ? true : ntw->selectChannels & (1 << part);
    bool editEnable = part < 0 ? true : !lockEnable;
    // to do : locked disabled if channel is already locked, etc
    this->drawVerticalLabel("lock", lockEnable, upSelected, 0);
    this->drawVerticalLabel("edit", editEnable, downSelected, box.size.y - 50);
    this->drawVerticalControl();
    // draw horizontal control
    const float boxWidth = 20;
    const float boxHeight = 15;
    auto vg = state.vg;
    nvgBeginPath(vg);
    nvgRect(vg, 60 - boxWidth, box.size.y - boxHeight - 15, boxWidth * 5, boxHeight);
    nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0xBf));
    nvgFill(vg);
    nvgFontFaceId(vg, ntw->textFont());
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    nvgFontSize(vg, 13);
    if (part >= 0) {
        auto& channel = ntw->storage.current().channels[part];
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7f));
        std::string s(channel.sequenceName);
        if (!channel.instrumentName.empty() && !s.empty()) {
            s += " / ";
        }
        s += channel.instrumentName;
        if (!s.empty()) {
            s += " / ";
        }
        s += GMInstrumentName(channel.gmInstrument);
        this->drawName(s);
    }
    DisplayControl control(this, state.vg);
    int outputCount = NoteTaker::OutputCount(ntw->nt());
    control.autoDrift(std::min((float) outputCount, horizontalWheel->getValue()),
            state.callInterval);
    int firstVisible = xControlOffset >= 0 ? xControlOffset - 1 : -1;
    int lastVisible = std::min(outputCount - 1, (int) (xControlOffset + 4));
    control.drawStart();
    for (int index = firstVisible; index <= lastVisible; ++index) {
        nvgSave(vg);
        nvgTranslate(vg, 60 + (index - xControlOffset) * boxWidth, box.size.y - boxHeight - 15);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, boxWidth, boxHeight);
        SetPartColor(vg, index, part);
        nvgFill(vg);
        if (index >= 0 && ~ntw->selectChannels & (1 << index)) {
            nvgFontFaceId(vg, ntw->musicFont());
            nvgFontSize(vg, 34);
            nvgText(vg, boxWidth - 2, boxHeight, "}", nullptr);
            nvgFontFaceId(vg, ntw->textFont());
        } else {
            nvgBeginPath(vg);
            nvgCircle(vg, 0.5f * boxWidth, boxHeight / 2, 7);
            nvgFill(vg);
        }
        const char num[2] = { (char) ('1' + index), '\0' };
        nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xbf));
        const char* str = index < 0 ? "all" : num;
        nvgFontSize(vg, 16);
        nvgText(vg, 0.5f * boxWidth, boxHeight - 3, str, nullptr);
        nvgRestore(vg);
    }
    control.drawEnd();
}

// to do : share code with draw beam ?
// to do : move slur so it doesn't draw on top of tie
// to do : mock score software ?
// in scoring software, slur is drawn from first to last, offset by middle, if needed
void NoteTakerDisplay::drawSlur(unsigned first, unsigned char alpha) const {
    auto& cacheNotes = this->cache()->notes;
    auto& notes = ntw()->n().notes;
    SCHMICKLE(PositionType::left == cacheNotes[first].slurPosition);
    SCHMICKLE(cacheNotes[first].tieId < this->cache()->beams.size());
    const BeamPosition& bp = this->cache()->beams[cacheNotes[first].tieId];
    unsigned last = bp.cacheLast;
    int chan = cacheNotes[first].channel;
    auto& slurRight = cacheNotes[last];
    if (DEBUG_SLUR_TEST && PositionType::right != slurRight.slurPosition) {
        DEBUG("PositionType::mid != cache[%d].slurPosition (%s) cache[%u]: %s note[%u]: %s",
                last, debugPositionType[(int) slurRight.slurPosition], last,
                slurRight.debugString().c_str(), slurRight.note - &notes.front(),
                slurRight.note->debugString().c_str());
        debugDump(first, last);
    }
    SCHMICKLE(PositionType::right == slurRight.slurPosition);
    SetNoteColor(state.vg, chan, alpha);
    this->drawArc(bp, first, last);
}

// note this assumes storage playback has at least one entry
void NoteTakerDisplay::drawSlotControl() {
    auto ntw = this->ntw();
    auto selectButton = ntw->selectButton;
    if (selectButton->isOff()) {
        SlotButton::Select select = (SlotButton::Select) ntw->verticalWheel->getValue();
        this->drawVerticalLabel("repeat", true, SlotButton::Select::repeat == select, 0);
        this->drawVerticalLabel("slot", true, SlotButton::Select::slot == select, (box.size.y - 50) / 2);
        this->drawVerticalLabel("end", true, SlotButton::Select::end == select, box.size.y - 50);
        this->drawVerticalControl();
    }
    DisplayControl control(this, state.vg);
    float fSlot = selectButton->ledOn() ? ntw->horizontalWheel->getValue() :
            (float) ntw->storage.slotStart;
    control.autoDrift(fSlot, state.callInterval, 3 + (int) selectButton->isSingle());
    unsigned firstVisible = xControlOffset >= 1 ? (unsigned) xControlOffset - 1 : 0;
    unsigned lastVisible = std::min((unsigned) ntw->storage.playback.size() - 1,
            (unsigned) (xControlOffset + 5));
    control.drawStart();
    for (unsigned index = firstVisible; index <= lastVisible; ++index) {
        auto& slotPlay = ntw->storage.playback[index];
        control.drawSlot(index, slotPlay.index, slotPlay.repeat, slotPlay.stage,
                firstVisible, lastVisible);
    }
    control.drawEnd();
    if (ntw->selectButton->isSingle()) {
        control.drawActiveNarrow(ntw->storage.startToWheel());
    } else {
        control.drawActive(ntw->storage.slotStart, ntw->storage.slotEnd);
    }
}

void NoteTakerDisplay::drawSustainControl() const {
    // draw vertical control
    auto vg = state.vg;
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 35, 25, 35, box.size.y - 30);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x7f));
    nvgFill(vg);
    auto ntw = this->ntw();
    auto select = (NoteTakerChannel::Limit) ntw->verticalWheel->getValue();
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x - 30, 30);
    nvgLineTo(vg, box.size.x - 30, 20);
    nvgLineTo(vg, box.size.x - 15, 20);
    nvgLineTo(vg, box.size.x - 15, 30);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::sustainMax == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    float yOff = (box.size.y - 40) / 3;
    nvgMoveTo(vg, box.size.x - 30, 30 + yOff);
    nvgLineTo(vg, box.size.x - 30, 20 + yOff);
    nvgLineTo(vg, box.size.x - 25, 20 + yOff);
    nvgLineTo(vg, box.size.x - 25, 30 + yOff);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::sustainMin == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    yOff = (box.size.y - 40) * 2 / 3;
    nvgMoveTo(vg, box.size.x - 30, 20 + yOff);
    nvgQuadTo(vg, box.size.x - 28, 30 + yOff, box.size.x - 22, 30 + yOff);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::releaseMin == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x - 30, box.size.y - 30);
    nvgQuadTo(vg, box.size.x - 24, box.size.y - 20, box.size.x - 15, box.size.y - 20);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::releaseMax == select));
    nvgStroke(vg);
    this->drawVerticalControl();
    // draw horizontal control
    nvgBeginPath(vg);
    const NoteTakerChannel& channel = ntw->storage.current().channels[ntw->unlockedChannel()];
    int susMin = std::max(6, channel.sustainMin);
    int susMax = channel.sustainMin == channel.sustainMax ? 0
            : std::max(6, channel.sustainMax - channel.sustainMin);
    int relMin = std::max(6, channel.releaseMin);
    int relMax = channel.releaseMin == channel.releaseMax ? 0
            : std::max(6, channel.releaseMax - channel.releaseMin);
    int total = susMin + susMax + relMin + relMax;
    if (total > box.size.x - 80) {
        int overhead = 12 + (susMax ? 6 : 0) + (relMax ? 6 : 0);
        float scale = (box.size.x - 80 - overhead) / (total - overhead);
        susMin = std::max(6, (int) (susMin * scale));
        if (susMax) {
            susMax = std::max(6, (int) (susMax * scale));
        }
        relMin = std::max(6, (int) (relMin * scale));
        if (relMax) {
            relMax = std::max(6, (int) (relMax * scale));
        }
        SCHMICKLE(susMin + susMax + relMin + relMax <= box.size.x - 80);
    }
    if (false) DEBUG("sus %d %d rel %d %d", susMin, susMax, relMin, relMax);
    nvgMoveTo(vg, 40, box.size.y - 5);
    nvgLineTo(vg, 40, box.size.y - 20);
    nvgLineTo(vg, 40 + susMin, box.size.y - 20);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::sustainMin <= select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 40 + susMin, box.size.y - 20);
    nvgLineTo(vg, 40 + susMin, box.size.y - 5);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::sustainMin == select));
    nvgStroke(vg);
    if (susMax) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 40 + susMin, box.size.y - 20);
        nvgLineTo(vg, 40 + susMin + susMax, box.size.y - 20);
        nvgLineTo(vg, 40 + susMin + susMax, box.size.y - 5);
        nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::sustainMax == select));
        nvgStroke(vg);
    }
    int xPos = 40 + susMin + susMax;
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos, box.size.y - 20);
    nvgQuadTo(vg, xPos + relMin / 4, box.size.y - 5, xPos + relMin, box.size.y - 5);
    nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::releaseMin == select));
    nvgStroke(vg);
    if (relMax) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, xPos, box.size.y - 20);
        int rel = relMin + relMax;
        nvgQuadTo(vg, xPos + rel / 2, box.size.y - 5, xPos + rel, box.size.y - 5);
        nvgStrokeWidth(vg, 1 + (NoteTakerChannel::Limit::releaseMax == select));
        nvgStroke(vg);
    }
    nvgFontFaceId(vg, ntw->musicFont());
    nvgFontSize(vg, 24);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, NoteTakerChannel::Limit::sustainMin == select ? 0xFF : 0x7f));
    nvgText(vg, 42, box.size.y - 18, downFlagNoteSymbols[
            NoteDurations::FromMidi(channel.sustainMin, ntw->storage.current().n.ppq)], nullptr);
    if (susMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, NoteTakerChannel::Limit::sustainMax == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin, box.size.y - 18, downFlagNoteSymbols[
                NoteDurations::FromMidi(channel.sustainMax, ntw->storage.current().n.ppq)], nullptr);
    }
    nvgFillColor(vg, nvgRGBA(0, 0, 0, NoteTakerChannel::Limit::releaseMin == select ? 0xFF : 0x7f));
    nvgText(vg, 42 + susMin + susMax, box.size.y - 18, downFlagNoteSymbols[
            NoteDurations::FromMidi(channel.releaseMin, ntw->storage.current().n.ppq)], nullptr);
    if (relMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, NoteTakerChannel::Limit::releaseMax == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin + susMax + relMin, box.size.y - 18, downFlagNoteSymbols[
                NoteDurations::FromMidi(channel.releaseMax, ntw->storage.current().n.ppq)], nullptr);
    }
}

void NoteTakerDisplay::drawTempo(int xPos, int tempo, unsigned char alpha) {
    auto vg = state.vg;
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFontSize(vg, 16);
    nvgText(vg, xPos + 3, 10, "H", nullptr);
    nvgFontFaceId(vg, ntw()->textFont());
    nvgFontSize(vg, 12);
    auto nt = ntw()->nt();
    std::string tempoStr("= " + std::to_string((int) (120.f * nt->beatsPerHalfSecond(tempo))));
    nvgText(vg, xPos + 7, 10, tempoStr.c_str(), nullptr);
}

// to do : for all scrolling controls -- if user turns off button in middle of scroll, 
//         make sure timers are canceled, scrolling state is set to nearest integer, etc.
void NoteTakerDisplay::drawTieControl() {
    auto ntw = this->ntw();
    DisplayControl horzCtrl(this, state.vg);
    auto horizontalWheel = ntw->horizontalWheel;
    float selected = horizontalWheel->getValue();
    float flo, fhi;
    horizontalWheel->getLimits(&flo, &fhi);
    int lo = (int) flo;
    int hi = (int) fhi;
    int count = hi - lo + 1;
    int totalWidth = horzCtrl.boxWidth * count + horzCtrl.margin * std::max(0, count - 1);
    int leftEdge = (box.size.x - totalWidth) / 2;
    // draw 1, 2, or 3 boxes, depending on triplet button settings
    for (int index = lo; index < lo + count; ++index) {
        horzCtrl.drawTriplet(leftEdge, index);
        leftEdge += horzCtrl.boxWidth + horzCtrl.margin;
    };
    if (lo < hi) {
        if (false && debugVerbose) DEBUG("horzCtrl selected %g lo %d hi %d", selected, lo, hi);
        horzCtrl.drawActivePointer(selected, (float) (lo + hi) / 2);
    }
    // to do : draw vertical triplet control
    DisplayControl vertCtrl(this, state.vg, false);
    auto verticalWheel = ntw->verticalWheel;
    selected = verticalWheel->getValue();
    verticalWheel->getLimits(&flo, &fhi);
    lo = (int) flo;
    hi = (int) fhi;
    count = hi - lo + 1;
    int yOffset = (box.size.y + vertCtrl.boxWidth * (count - 2) + vertCtrl.margin * (count - 1)) / 2;
    for (int index = lo; index < lo + count; ++index) {
        vertCtrl.drawTie(yOffset, index);
        yOffset -= vertCtrl.boxWidth + vertCtrl.margin;
    }
    if (lo < hi) {
#if DEBUG_SLUR
        if (debugVerbose) {
            static float lastS = 0;
            static int lastLo = 0;
            static int lastHi = 0;
            if (lastS != selected || lastLo != lo || lastHi != hi) {
                DEBUG("vertCtrl selected %g lo %d hi %d", selected, lo, hi);
                lastS = selected;
                lastLo = lo;
                lastHi = hi;
            }
        }
#endif
        vertCtrl.drawActivePointer(fhi - selected, 2 == count ? 0.5f : 1);  // to do : '1' offset is a guess
    }
}

void NoteTakerDisplay::drawTie(unsigned first, unsigned char alpha) const {
    auto& notes = this->cache()->notes;
    SCHMICKLE(PositionType::left == notes[first].tiePosition
            || PositionType::mid == notes[first].tiePosition);
    if (notes[first].tieId >= this->cache()->beams.size()) {
        if (debugVerbose) DEBUG("** %s tieId out of range; cache: [%d] %s note: [%d] %s  beams size: %u ",
                __func__, first, notes[first].debugString().c_str(),
                notes[first].note - &ntw()->n().notes.front(),
                notes[first].note->debugString().c_str(), this->cache()->beams.size());
        return;
    }
    const BeamPosition& bp = this->cache()->beams[notes[first].tieId];
    int chan = notes[first].channel;
    unsigned last = bp.cacheLast;
    if (PositionType::right != notes[last].tiePosition
            && PositionType::mid != notes[last].tiePosition) {
        // to do : reenable this to debug : disable for now because it outputs continuously
        if (debugVerbose) {
            DEBUG("%s %s", __func__, bp.debugString().c_str());
            DEBUG("** missing tie end %s", notes[first].note->debugString().c_str());
        }
    }
    SetNoteColor(state.vg, chan, alpha);
    this->drawArc(bp, first, last);
}

// to do : share code with draw slur, draw beam ?
void NoteTakerDisplay::drawTuple(unsigned first, unsigned char alpha, bool drewBeam) const {
    auto& cacheNotes = this->cache()->notes;
    auto& tupletLeft = cacheNotes[first];
    if (INT_MAX == tupletLeft.tripletId) {
        // to do : figure out why this happens, sometimes
        DEBUG("%s missing cache entry %s", __func__,
                tupletLeft.note->debugString(ntw()->n().notes, &cacheNotes, &tupletLeft).c_str());
        return;
    }
    const BeamPosition& bp = this->cache()->beams[tupletLeft.tripletId];
    if (PositionType::left != tupletLeft.tripletPosition) {
        DEBUG("PositionType::left != cache[%u].tripletPosition (%s) bp: [%u] %s note: %s"
                " note[%u]: %s",
                first, debugPositionType[(int) tupletLeft.tripletPosition],
                tupletLeft.tripletId, bp.debugString().c_str(),
                tupletLeft.note->debugString(ntw()->n().notes, &cacheNotes, &tupletLeft).c_str());
        debugDump(first, first + 1);
    }
    SCHMICKLE(PositionType::left == tupletLeft.tripletPosition);
    auto& tupletRight = cacheNotes[bp.cacheLast];
    if (PositionType::right != tupletRight.tripletPosition) {
        DEBUG("PositionType::right != cache[%d].tripletPosition (%s) bp: [%u] %s note: %s"
                " note[%u]: %s",
                bp.cacheLast, debugPositionType[(int) tupletRight.tripletPosition],
                tupletLeft.tripletId, bp.debugString().c_str(),
                tupletRight.note->debugString(ntw()->n().notes, &cacheNotes, &tupletRight).c_str());
        debugDump(first, bp.cacheLast);
    }
    SCHMICKLE(PositionType::right == tupletRight.tripletPosition);
    auto vg = state.vg;
    SetNoteColor(vg, tupletLeft.channel, alpha);
    // draw '3' at center of notes above or below staff
    nvgFontFaceId(vg, ntw()->musicFont());
    nvgFontSize(vg, 16);
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    float centerX = (bp.sx + bp.ex) / 2;
    bool stemUp = StemType::up == tupletLeft.stemDirection;
    float yOffset = stemUp ? -1 : 7.5;
    nvgText(vg, centerX, bp.y + yOffset, "3", NULL);
    // to do : if !drew beam, draw square brackets on either side of '3'
    if (false && DEBUG_TRIPLET_TEST) DEBUG("drewBeam %d bp.sx %g bp.ex %g", drewBeam, bp.sx, bp.ex);
    if (!drewBeam) {
        BeamState bs(bp);
        bs.sx -= 2;
        bs.y += stemUp ? 0 : 3;
        nvgBeginPath(vg);
        nvgMoveTo(vg, bs.sx, bs.y);
        bs.y += stemUp ? -3 : 3;
        nvgLineTo(vg, bs.sx, bs.y);
        nvgLineTo(vg, centerX - 3, bs.y);
        nvgMoveTo(vg, centerX + 3, bs.y);
        bs.ex += 2;
        nvgLineTo(vg, bs.ex, bs.y);
        bs.y -= stemUp ? -3 : 3;
        nvgLineTo(vg, bs.ex, bs.y);
        nvgStrokeWidth(vg, 0.5);
        nvgStroke(vg);
    }
}

void NoteTakerDisplay::drawVerticalControl() const {
    auto vg = state.vg;
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 10, 20, 5, box.size.y - 40);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
    nvgBeginPath(vg);
    auto ntw = this->ntw();
    auto verticalWheel = ntw->verticalWheel;
    float value = verticalWheel->getValue();
    float range = verticalWheel->paramQuantity->maxValue;
    float yPos = box.size.y - 20 - value * (box.size.y - 40) / range;
    nvgMoveTo(vg, box.size.x - 10, yPos);
    nvgLineTo(vg, box.size.x - 5, yPos - 3);
    nvgLineTo(vg, box.size.x - 5, yPos + 3);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

void NoteTakerDisplay::drawVerticalLabel(const char* label, bool enabled,
        bool selected, float y) const {
    auto vg = state.vg;
    nvgFontFaceId(vg, ntw()->textFont());
    nvgFontSize(vg, 16);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT);
    if (enabled) {
        nvgFillColor(vg, nvgRGB(0, 0, 0));
    } else {
        nvgFillColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    }
    float textX = box.size.x - 13;
    float textY = y + 30;
    if (selected) {
        float bounds[4];
        nvgTextBounds(vg, textX, textY, label, NULL, bounds);
        nvgBeginPath(vg);
        nvgRect(vg, bounds[0] - 3, bounds[1] - 3, bounds[2] - bounds[0] + 6,
                bounds[3] - bounds[1] + 6);
        nvgFill(vg);
        nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    }
    nvgText(vg, textX, textY, label, NULL);
}

const char* NoteTakerDisplay::GMInstrumentName(unsigned index) {
    SCHMICKLE(index < sizeof(gmInstrumentPatchMap) / sizeof(gmInstrumentPatchMap[0]));
    return gmInstrumentPatchMap[index];
}

void NoteTakerDisplay::invalidateCache() {
    this->ntw()->storage.invalidate();
    this->redraw();
}

Notes* NoteTakerDisplay::notes() {
    return &ntw()->storage.current().n;
}

void NoteTakerDisplay::recenterVerticalWheel() {
    if (upSelected || downSelected) {
        auto ntw = this->ntw();
        auto verticalWheel = ntw->verticalWheel;
        float val = verticalWheel->getValue();
        const float autoLoadSaveWheelSpeed = 70 * state.callInterval * .2f;
        verticalWheel->setValue(verticalWheel->getValue()
                + (val > 5 ? -autoLoadSaveWheelSpeed : +autoLoadSaveWheelSpeed));
        if (5 - autoLoadSaveWheelSpeed < val && val < 5 + autoLoadSaveWheelSpeed) {
            upSelected = false;
            downSelected = false;
        } else {
            this->redraw();
        }
    }
}

void NoteTakerDisplay::setKeySignature(int key) {
    keySignature = key;
    pitchMap = key >= 0 ? sharpMap : flatMap;
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
}

void NoteTakerDisplay::SetNoteColor(NVGcontext* vg, unsigned chan, unsigned char alpha) {
    if (chan >= sizeof(channelColors) / sizeof(channelColors[0])) {
        return;
    }
    const Color& c = channelColors[chan];
    NVGcolor color = nvgRGBA(c.r, c.g, c.b, alpha);
    nvgFillColor(vg, color);
    nvgStrokeColor(vg, color);
}

void NoteTakerDisplay::SetPartColor(NVGcontext* vg, int index, int part) {
    SCHMICKLE(index < (int) (sizeof(channelColors) / sizeof(channelColors[0])));
    const Color& c = channelColors[std::max(0, index)];
    nvgFillColor(vg, nvgRGBA(c.r, c.g, c.b, index == part ? 0xaf : 0x6f));
}

// to do : show multiple parts in selection color (stripes?)
void NoteTakerDisplay::SetSelectColor(NVGcontext* vg, unsigned chan) {
    unsigned index = 0;
    SCHMICKLE(chan);
    while (!((1 << index) & chan)) {
        ++index;
    }
    SCHMICKLE(index < sizeof(channelColors) / sizeof(channelColors[0]));
    const Color& c = channelColors[index];
    nvgFillColor(vg, nvgRGBA(c.r, c.g, c.b, ALL_CHANNELS == chan ? 0x3f : 0x1f));
}

void NoteTakerDisplay::setUpAccidentals(BarPosition& bar) {
    // prepare accidental, key, bar state prior to drawing
    // to do : could optimize this to skip notes except for bar prior to displayStart
    const auto& n = *this->notes();
    const auto& cache = this->cache()->notes;
    unsigned noteStart = cache[range.displayStart].note - &n.notes.front();
    for (unsigned index = 0; index < noteStart; ++index) {
        const DisplayNote& note = n.notes[index];
        switch (note.type) {
            case NOTE_ON: {
                this->advanceBar(bar, index);
                const StaffNote& pitch = pitchMap[note.pitch()];
                accidentals[pitch.position] = (Accidental) pitch.accidental;
            } break;
            case KEY_SIGNATURE:
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                (void) bar.setSignature(note, n.ppq);
                break;
            default:
                ;
        }
    }
}

float NoteTakerDisplay::TimeSignatureWidth(const DisplayNote& note, NVGcontext* vg,
            int musicFont) {
    SCHMICKLE(TIME_SIGNATURE == note.type);
    nvgFontFaceId(vg, musicFont);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    std::string numerator = std::to_string(note.numerator());
    float bounds[4];
    (void) nvgTextBounds(vg, 0, 0, numerator.c_str(), nullptr, bounds);
    float width = bounds[2] - bounds[0];
    std::string denominator = std::to_string(1 << note.denominator());
    (void) nvgTextBounds(vg, 0, 0, denominator.c_str(), nullptr, bounds);
    return std::max(width, bounds[2] - bounds[0]);
}
