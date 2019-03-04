#include "NoteTakerDisplay.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

// map midi note for C major to drawable position
struct StaffNote {
    uint8_t position;  // 0 == G9, 38 == middle C (C4), 74 == C-1
    uint8_t accidental; // 0 none, 1 sharp, 2 flat, 3 natural
};

const uint8_t TREBLE_TOP = 29;  // smaller values need additional staff lines and/or 8/15va
const uint8_t C_5 = 32;
const uint8_t MIDDLE_C = 39;
const uint8_t C_3 = 46;
const uint8_t BASS_BOTTOM = 49; // larger values need additional staff lines and/or 8/15vb

// C major only, for now
// Given a MIDI pitch 0 - 127, looks up staff line position and presence of accidental (# only for now)
const StaffNote pitchMap[] = {
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

// if preceding note in bar (or if there is no bar) is sharp
const StaffNote sharpPitchMap[] = {

};

const char* upFlagNoteSymbols[] = {   "C", "D", "D.", "E", "E.", "F", "F.", "G", "G.", "H", "H.",
                                           "I", "I.", "J", "J.", "K", "K.", "L", "L.", "M" };
const char* downFlagNoteSymbols[] = { "c", "d", "d.", "e", "e.", "f", "f.", "g", "g.", "h", "h.",
                                           "i", "i.", "j", "j.", "k", "k.", "l", "l.", "m" };

void NoteTakerDisplay::drawNote(NVGcontext* vg, const DisplayNote& note, Accidental accidental,
        int xPos, int alpha) const {
    const StaffNote& pitch = pitchMap[note.pitch()];
    float yPos = pitch.position * 3 - 48.25; // middle C 60 positioned at 39 maps to 66
    float staffLine = yPos - 3;
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
    if (staffLineCount) {
    	nvgBeginPath(vg);
        do {
            nvgMoveTo(vg, xPos - 2, staffLine);
            nvgLineTo(vg, xPos + 8, staffLine);
            staffLine += 6;
        } while (--staffLineCount);
        nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgStroke(vg);
    }
    nvgFillColor(vg, nvgRGBA((1 == note.channel) * 0xBf, (3 == note.channel) * 0x7f,
            (2 == note.channel) * 0x7f, alpha));
    switch (accidental) {
        case NO_ACCIDENTAL:
            break;
        case SHARP_ACCIDENTAL:
            nvgText(vg, xPos - 7, yPos + 1, "#", NULL);
            break;
        case FLAT_ACCIDENTAL:
            nvgText(vg, xPos - 7, yPos + 1, "$", NULL);
            break;
        case NATURAL_ACCIDENTAL:
            nvgText(vg, xPos - 7, yPos + 1, "%", NULL);
            break;
    }
    static_assert(sizeof(upFlagNoteSymbols) / sizeof(char*) == noteDurations.size(),
            "symbol duration mismatch");
    static_assert(sizeof(downFlagNoteSymbols) / sizeof(char*) == noteDurations.size(),
            "symbol duration mismatch");
    unsigned symbol = note.note();
    nvgFontSize(vg, 42);
    const char* noteStr = (pitch.position <= MIDDLE_C && pitch.position > C_5)
            || pitch.position >= C_3 ? upFlagNoteSymbols[symbol] : downFlagNoteSymbols[symbol];
    nvgText(vg, xPos, yPos, noteStr, nullptr);
}

void NoteTakerDisplay::drawBarNote(NVGcontext* vg, const DisplayNote& note, int xPos,
        int alpha) {
    const Accidental lookup[][3]= {
    // next:      no                  #                b           last:
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // no
        { NATURAL_ACCIDENTAL, NO_ACCIDENTAL,    FLAT_ACCIDENTAL }, // #
        { NATURAL_ACCIDENTAL, SHARP_ACCIDENTAL, NO_ACCIDENTAL   }, // b
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // natural
    };
    const StaffNote& pitch = pitchMap[note.pitch()];
    Accidental accidental = lookup[accidentals[pitch.position]][pitch.accidental];
    drawNote(vg, note, accidental, xPos, alpha);
    accidentals[pitch.position] = accidental;
}

void NoteTakerDisplay::drawFreeNote(NVGcontext* vg, const DisplayNote& note, int xPos,
        int alpha) const {
    drawNote(vg, note, (Accidental) pitchMap[note.pitch()].accidental, xPos, alpha);
}

void NoteTakerDisplay::drawRest(NVGcontext* vg, const DisplayNote& note, int xPos, int alpha) const {
    const char restSymbols[] = "oppqqrrssttuuvvwwxxyy";
    unsigned symbol = note.rest();
    float yPos = 36 * 3 - 49;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontSize(vg, 42);
    do {
        unsigned restIndex = std::min((unsigned) sizeof(restSymbols) - 1, symbol);
        nvgText(vg, xPos, yPos, &restSymbols[restIndex], &restSymbols[restIndex] + 1);
        if (restIndex > 0 && (restIndex & 1) == 0) {
            const int xOff[] = {5, 5, 5, 5, 5, 5, 5, 5, 5,  // 128 - 8
                               10, 10, 12, 12, 12, 12, 12, 12, 12, 12, 14};
            float xDot = xPos + xOff[restIndex];
            float yDot = yPos - 9;
            nvgText(vg, xDot, yDot, ".", nullptr);
        }
        xPos += noteDurations[restIndex] / 4;
        symbol -= restIndex;
    } while (symbol);
}

void NoteTakerDisplay::draw(NVGcontext* vg) {
    if (!module->allNotes.size()) {
        return;  // do nothing if we're not set up yet
    }
    accidentals.fill(NO_ACCIDENTAL);
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgFill(vg);
    // draw bevel
    const float bevel = 2;
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, 0);
    nvgLineTo(vg, 0, box.size.y);
    nvgLineTo(vg, bevel, box.size.y - bevel);
    nvgLineTo(vg, bevel, bevel);
    nvgFillColor(vg, nvgRGB(0x6f, 0x6f, 0x6f));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, box.size.y);
    nvgLineTo(vg, box.size.x, box.size.y);
    nvgLineTo(vg, box.size.x - bevel, box.size.y - bevel);
    nvgLineTo(vg, bevel, box.size.y - bevel);
    nvgFillColor(vg, nvgRGB(0x9f, 0x9f, 0x9f));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x, box.size.y);
    nvgLineTo(vg, box.size.x, 0);
    nvgLineTo(vg, box.size.x - bevel, bevel);
    nvgLineTo(vg, box.size.x - bevel, box.size.y - bevel);
    nvgFillColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x, 0);
    nvgLineTo(vg, 0, 0);
    nvgLineTo(vg, bevel, bevel);
    nvgLineTo(vg, box.size.x - bevel, bevel);
    nvgFillColor(vg, nvgRGB(0x5f, 0x5f, 0x5f));
    nvgFill(vg);
    // draw vertical line at end of staff lines
    nvgBeginPath(vg);
    nvgMoveTo(vg, 3 - xAxisOffset, 36);
    nvgLineTo(vg, 3 - xAxisOffset, 96);
    nvgStrokeWidth(vg, 0.5);
    nvgStroke(vg);
    // draw staff lines
    nvgBeginPath(vg);
    for (int staff = 36; staff <= 72; staff += 36) {
        for (int y = staff; y <= staff + 24; y += 6) { 
	        nvgMoveTo(vg, std::max(0.f, 3 - xAxisOffset), y);
	        nvgLineTo(vg, box.size.x - 1, y);
        }
    }
	nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
	nvgStroke(vg);
     // draw treble and bass clefs
    nvgFontFaceId(vg, module->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 42);
    nvgText(vg, 5 - xAxisOffset, 60, "(", NULL);
    nvgFontSize(vg, 36);
    nvgText(vg, 5 - xAxisOffset, 92, ")", NULL);
    // draw selection rect
    nvgBeginPath(vg);
    if (module->selectButton->editStart()) {
        int xStart = this->xPos(module->selectStart + 1);
        nvgRect(vg, xStart - 2, 2, 4, box.size.y - 4);
    } else {
        int xStart = this->xPos(module->selectStart);
        bool timeSignatureEdit = TIME_SIGNATURE == module->allNotes[module->selectStart].type
                && module->selectStart + 1 == module->selectEnd && !module->selectButton->ledOn;
        int yTop = timeSignatureEdit ? (1 - (int) module->verticalWheel->value) * 12 + 35 : 2;
        int yHeight = timeSignatureEdit ? 13 : box.size.y - 4;
        nvgRect(vg, xStart, yTop, this->xPos(module->selectEnd) - xStart, yHeight);
    }
    unsigned selectChannels = module->selectChannels;
    nvgFillColor(vg, nvgRGBA((2 == selectChannels) * 0xBf, (8 == selectChannels) * 0x7f,
            (4 == selectChannels) * 0x7f, ALL_CHANNELS == selectChannels ? 0x3f : 0x1f));
    nvgFill(vg);
    // draw notes
    for (unsigned index = module->displayStart; index < module->displayEnd; ++index) {
        const DisplayNote& note = module->allNotes[index];
        switch (note.type) {
            case NOTE_ON:
                this->drawBarNote(vg, note, this->xPos(index) + 8, 0xFF);              
            break;
            case REST_TYPE:
                this->drawRest(vg, note, this->xPos(index) + 8, 0xFF);
            break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE:
                // to do
            break;
            case TIME_SIGNATURE: {
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                std::string numerator = std::to_string(note.numerator());
                int xPos = this->xPos(index) - 4 * (numerator.size() - 1) + 3;
                nvgText(vg, xPos, 32 * 3 - 49, numerator.c_str(), NULL);
                nvgText(vg, xPos, 44 * 3 - 49, numerator.c_str(), NULL);
                std::string denominator = std::to_string(1 << note.denominator());
                xPos = this->xPos(index) - 4 * (denominator.size() - 1) + 3;
                nvgText(vg, xPos, 36 * 3 - 49, denominator.c_str(), NULL);
                nvgText(vg, xPos, 48 * 3 - 49, denominator.c_str(), NULL);
            } break;
            case MIDI_TEMPO:
            break;
            case TRACK_END:
            break;
            default:
                debug("to do: add type %d\n", note.type);
                assert(0); // incomplete
        }
    }
    if (module->fileButton->ledOn) {
        this->drawFileControl(vg);
    }
    if (module->sustainButton->ledOn) {
        this->drawSustainControl(vg);
    }
    if (module->isRunning()) {
        this->drawDynamicPitchTempo(vg);
    }
	FramebufferWidget::draw(vg);
    dirty = false;
}

void NoteTakerDisplay::drawDynamicPitchTempo(NVGcontext* vg) const {
    if (dynamicPitchAlpha > 0) {
        DisplayNote note = {0, stdTimePerQuarterNote, { 0, 0, 0, 0}, 0, NOTE_ON };
        note.setPitch((int) module->verticalWheel->value);
        note.setNote(DurationIndex(note.duration));
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 16, 2, 14, box.size.y - 4);
        nvgFillColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
        nvgFill(vg);
        this->drawFreeNote(vg, note, box.size.x - 10, dynamicPitchAlpha);
    }
    if (dynamicTempoAlpha > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, 2, 2, 30, 12);
        nvgFillColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
        nvgFill(vg);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, dynamicTempoAlpha));
        nvgFontFaceId(vg, module->musicFont->handle);
        nvgFontSize(vg, 16);
        nvgText(vg, 5, 10, "H", nullptr);
        nvgFontFaceId(vg, module->textFont->handle);
        nvgFontSize(vg, 12);
        std::string tempoStr("= " + std::to_string((int) (120.f * module->beatsPerHalfSecond())));
        nvgText(vg, 9, 10, tempoStr.c_str(), nullptr);
    }
}

void NoteTakerDisplay::drawFileControl(NVGcontext* vg) const {
        // draw vertical control
    nvgFontFaceId(vg, module->textFont->handle);
    nvgFontSize(vg, 16);
    if ((unsigned) module->horizontalWheel->value < module->storage.size()) {
        nvgFillColor(vg, nvgRGB(0, 0, 0));
    } else {
        nvgFillColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    }
    if (loading) {
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 33, 20, 23, 12);
        nvgFill(vg);
        nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    }
    nvgText(vg, box.size.x - 30, 30, "load", NULL);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    if (saving) {
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 33, box.size.y - 30, 23, 12);
        nvgFill(vg);
        nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    }
    nvgText(vg, box.size.x - 30, box.size.y - 20, "save", NULL);
    this->drawVerticalControl(vg);
    // draw horizontal control
    const float boxWidth = 20;
    int slot = (int) module->horizontalWheel->value;
    nvgBeginPath(vg);
    nvgRect(vg, 5 + slot * boxWidth, box.size.y - boxWidth - 5, boxWidth, boxWidth);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgFill(vg);
    auto drawEmpty = [&](unsigned index){
        nvgBeginPath(vg);
        nvgRect(vg, 5 + index * boxWidth, box.size.y - boxWidth - 5, boxWidth, boxWidth);
        nvgMoveTo(vg, 5 + index * boxWidth, box.size.y - boxWidth - 5);
        nvgLineTo(vg, 5 + (index + 1) * boxWidth, box.size.y - 5);
        nvgMoveTo(vg, 5 + (index + 1) * boxWidth, box.size.y - boxWidth - 5);
        nvgLineTo(vg, 5 + index * boxWidth, box.size.y - 5);
        nvgStrokeWidth(vg, 1);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgStroke(vg);
    };
    for (unsigned index = 0; index < module->storage.size() && index * boxWidth < box.size.x;
            ++index) {            
        if (module->storage[index].empty()) {
            drawEmpty(index);
        } else {
            nvgBeginPath(vg);
            nvgRect(vg, 5 + index * boxWidth, box.size.y - boxWidth - 5, boxWidth, boxWidth);
            nvgStrokeWidth(vg, 1);
            nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
            nvgStroke(vg);
            nvgFontFaceId(vg, module->musicFont->handle);
            nvgFontSize(vg, 16);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
            nvgText(vg, 5 + (index + .5) * boxWidth, box.size.y - 8, "H", NULL);
        }
    }
    unsigned index = module->storage.size();
    if (index * boxWidth <= box.size.x) {
        drawEmpty(index);
    }
    float wheel = module->horizontalWheel->value;
    nvgBeginPath(vg);
    nvgRect(vg, 5 + wheel * boxWidth, box.size.y - boxWidth - 5, boxWidth, boxWidth);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

void NoteTakerDisplay::drawSustainControl(NVGcontext* vg) const {
    // draw vertical control
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 35, 25, 35, box.size.y - 30);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x7f));
    nvgFill(vg);
    int select = (int) module->verticalWheel->value;
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x - 30, 30);
    nvgLineTo(vg, box.size.x - 30, 20);
    nvgLineTo(vg, box.size.x - 15, 20);
    nvgLineTo(vg, box.size.x - 15, 30);
    nvgStrokeWidth(vg, 1 + (3 == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    float yOff = (box.size.y - 40) / 3;
    nvgMoveTo(vg, box.size.x - 30, 30 + yOff);
    nvgLineTo(vg, box.size.x - 30, 20 + yOff);
    nvgLineTo(vg, box.size.x - 25, 20 + yOff);
    nvgLineTo(vg, box.size.x - 25, 30 + yOff);
    nvgStrokeWidth(vg, 1 + (2 == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    yOff = (box.size.y - 40) * 2 / 3;
    nvgMoveTo(vg, box.size.x - 30, 20 + yOff);
    nvgQuadTo(vg, box.size.x - 28, 30 + yOff, box.size.x - 22, 30 + yOff);
    nvgStrokeWidth(vg, 1 + (1 == select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, box.size.x - 30, box.size.y - 30);
    nvgQuadTo(vg, box.size.x - 24, box.size.y - 20, box.size.x - 15, box.size.y - 20);
    nvgStrokeWidth(vg, 1 + (0 == select));
    nvgStroke(vg);
    this->drawVerticalControl(vg);
    // draw horizontal control
    nvgBeginPath(vg);
    NoteTakerChannel& channel = module->channels[module->firstChannel()];
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
        assert(susMin + susMax + relMin + relMax <= box.size.x - 80);
    }
    debug("sus %d %d rel %d %d", susMin, susMax, relMin, relMax);
    nvgMoveTo(vg, 40, box.size.y - 5);
    nvgLineTo(vg, 40, box.size.y - 20);
    nvgLineTo(vg, 40 + susMin, box.size.y - 20);
    nvgStrokeWidth(vg, 1 + (2 <= select));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 40 + susMin, box.size.y - 20);
    nvgLineTo(vg, 40 + susMin, box.size.y - 5);
    nvgStrokeWidth(vg, 1 + (2 == select));
    nvgStroke(vg);
    if (susMax) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 40 + susMin, box.size.y - 20);
        nvgLineTo(vg, 40 + susMin + susMax, box.size.y - 20);
        nvgLineTo(vg, 40 + susMin + susMax, box.size.y - 5);
        nvgStrokeWidth(vg, 1 + (3 == select));
        nvgStroke(vg);
    }
    int xPos = 40 + susMin + susMax;
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos, box.size.y - 20);
    nvgQuadTo(vg, xPos + relMin / 4, box.size.y - 5, xPos + relMin, box.size.y - 5);
    nvgStrokeWidth(vg, 1 + (1 == select));
    nvgStroke(vg);
    if (relMax) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, xPos, box.size.y - 20);
        int rel = relMin + relMax;
        nvgQuadTo(vg, xPos + rel / 2, box.size.y - 5, xPos + rel, box.size.y - 5);
        nvgStrokeWidth(vg, 1 + (0 == select));
        nvgStroke(vg);
    }
    nvgFontSize(vg, 24);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 2 == select ? 0xFF : 0x7f));
    nvgText(vg, 42, box.size.y - 18, downFlagNoteSymbols[
            DurationIndex(channel.sustainMin)], nullptr);
    if (susMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 3 == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin, box.size.y - 18, downFlagNoteSymbols[
                DurationIndex(channel.sustainMax)], nullptr);
    }
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 1 == select ? 0xFF : 0x7f));
    nvgText(vg, 42 + susMin + susMax, box.size.y - 18, downFlagNoteSymbols[
            DurationIndex(channel.releaseMin)], nullptr);
    if (relMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0 == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin + susMax + relMin, box.size.y - 18, downFlagNoteSymbols[
                DurationIndex(channel.releaseMax)], nullptr);
    }
}

void NoteTakerDisplay::drawVerticalControl(NVGcontext* vg) const {
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 10, 20, 5, box.size.y - 40);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
    nvgBeginPath(vg);
    float value = module->verticalWheel->value;
    float range = module->verticalWheel->maxValue;
    float yPos = box.size.y - 20 - value * (box.size.y - 40) / range;
    nvgMoveTo(vg, box.size.x - 10, yPos);
    nvgLineTo(vg, box.size.x - 5, yPos - 3);
    nvgLineTo(vg, box.size.x - 5, yPos + 3);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

int NoteTakerDisplay::duration(unsigned index) const {
    return module->allNotes[index].duration * xAxisScale;
}

void NoteTakerDisplay::updateXPosition() {
    debug("updateXPosition");
    if (!xPositionsInvalid) {
        return;
    }
    debug("updateXPosition invalid");
    xPositionsInvalid = false;
    xPositions.resize(module->allNotes.size());
    int pos = 0;
    for (unsigned index = 0; index < module->allNotes.size(); ++index) {
        xPositions[index] = module->allNotes[index].startTime * xAxisScale + pos;
        switch (module->allNotes[index].type) {
            case MIDI_HEADER:
                assert(!pos);
                assert(!index);
                pos = 96 * xAxisScale;
                break;
            case NOTE_ON:
                // to do : if note is very short, pad it a bit
                //         if multiple notes from different channels overlap, space them out
                break;
            case REST_TYPE:
                break;
            case TIME_SIGNATURE:
                // to do : add space to draw time signature (guess for now)
                pos += 48 * xAxisScale;
                break;
#if 0
            case BAR_MARKER:
                pos += 24 * xAxisScale;
                break;
#endif
            case TRACK_END:
                debug("[%u] xPos %d start %d", index, xPositions[index],
                        module->allNotes[index].startTime);
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    debug("updateXPosition size %u", xPositions.size());
}
