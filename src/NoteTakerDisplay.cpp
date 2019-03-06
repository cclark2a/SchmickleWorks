#include "NoteTakerDisplay.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

/// drawing notes must line up with extra bar space added by updateXPosition
struct BarPosition {
    int count = 0;           // number of bars before current time signature
    int duration = INT_MAX;  // midi time of one bar
    int base = 0;            // time of current time signature
    int inSignature;         // time of note into current signature
    int start;               // number of bars in current time signature
    int end;                 // number of bars though current note duration
    int leader;              // duration of note part before first bar
    int trailer;             // remaining duration of note after last bar

    void next(const DisplayNote& note) {
        inSignature = note.startTime - base;
        start = inSignature / duration;
        leader = std::min(note.duration, (start + 1) * duration - inSignature);
    }

    int notesTied(const DisplayNote& note) {
        int noteEndTime = inSignature + note.duration;
        end = noteEndTime / duration;
        trailer = noteEndTime - end * duration;
        int result = NoteTakerDisplay::TiedCount(duration, leader);
        if (start != end) {
            result += end - start - 1;
            if (trailer) {
                result += NoteTakerDisplay::TiedCount(duration, trailer);
            }
        }
        return result;
    }

    bool setSignature(const DisplayNote& note) {
        base = note.startTime;
        // see if introducing new signature adds a bar
        bool drawBar = inSignature > 0 && inSignature < duration;
        count += drawBar;
        duration = stdTimePerQuarterNote * 4 * note.numerator() / (1 << note.denominator());
        return drawBar;
    }
};

const int CLEF_WIDTH = 96;
const int TIME_SIGNATURE_WIDTH = 48;
// key signature width is variable, from 0 (C) to NOTE_WIDTH*14+18 (C# to Cb, 7 naturals, 7 flats, space for 2 bars)
const int ACCIDENTAL_WIDTH = 12;
const int BAR_WIDTH = 12;
const int DOUBLE_BAR_WIDTH = 18;  // for key change
const int NOTE_WIDTH = 16;

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

NoteTakerDisplay::NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* m)
    : module(m)
    , pitchMap(sharpMap) {
    box.pos = pos;
    box.size = size;
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

void NoteTakerDisplay::setKeySignature(int key) {
    keySignature = key;
    pitchMap = key >= 0 ? sharpMap : flatMap;
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
}

void NoteTakerDisplay::drawNote(NVGcontext* vg, const DisplayNote& note, Accidental accidental,
        int xPos, int alpha) const {
    const StaffNote& pitch = pitchMap[note.pitch()];
    float yPos = this->yPos(pitch.position);
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
    const char* noteStr = this->stemUp(pitch.position) ? upFlagNoteSymbols[symbol]
            : downFlagNoteSymbols[symbol];
    nvgText(vg, xPos, yPos, noteStr, nullptr);
}

void NoteTakerDisplay::drawBar(NVGcontext* vg, int xPos) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos + 2, 36);
    nvgLineTo(vg, xPos + 2, 96);
    nvgStrokeWidth(vg, 0.5);
	nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
    nvgStroke(vg);
}

// to do : keep track of where notes are drawn to avoid stacking them on each other
// likewise, avoid drawing ties and slurs on top of notes and each other
// to get started though, draw ties for each note that needs it
void NoteTakerDisplay::drawBarNote(NVGcontext* vg, BarPosition& bar, const DisplayNote& note,
        int xPos, int alpha) {
    const Accidental lookup[][3]= {
    // next:      no                  #                b           last:
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // no
        { NATURAL_ACCIDENTAL, NO_ACCIDENTAL,    FLAT_ACCIDENTAL }, // #
        { NATURAL_ACCIDENTAL, SHARP_ACCIDENTAL, NO_ACCIDENTAL   }, // b
        { NO_ACCIDENTAL,      SHARP_ACCIDENTAL, FLAT_ACCIDENTAL }, // natural
    };
    const StaffNote& pitch = pitchMap[note.pitch()];
    Accidental accidental = lookup[accidentals[pitch.position]][pitch.accidental];
    accidentals[pitch.position] = (Accidental) pitch.accidental;
    int tied = bar.notesTied(note);
    if (1 == tied) {
        drawNote(vg, note, accidental, xPos, alpha);
        return;
    }
    DisplayNote copy = note;
    int lastXPos = INT_MAX;
    copy.duration = bar.leader;
    for (int barSide = bar.start; barSide <= bar.end; ++barSide) {
        if (barSide > bar.start) {
            copy.duration = barSide < bar.end ? bar.duration : bar.trailer;
        }
        while (copy.duration >= noteDurations[0]) {
            copy.setNote(DurationIndex(copy.duration));
            drawNote(vg, copy, accidental, xPos, alpha);
            // to do : advance by at least NOTE_WIDTH (need corresponding change in calc x pos)
            if (INT_MAX != lastXPos) {  // draw tie from last note to here
                // if notes' stems go down, draw arc above; otherwise, draw arc below
                int yOff = this->stemUp(pitch.position) ? 2 : -2;
                int yPos = this->yPos(pitch.position) + yOff;
                int mid = (lastXPos + xPos) / 2;
                nvgBeginPath(vg);
                nvgMoveTo(vg, lastXPos + 2, yPos);
                nvgQuadTo(vg, mid, yPos + yOff, xPos - 2, yPos);
                nvgQuadTo(vg, mid, yPos + yOff * 2, lastXPos + 2, yPos);
                nvgFill(vg);
            }
            lastXPos = xPos;
            xPos += copy.duration * xAxisScale;
            copy.duration -= noteDurations[copy.note()];
            accidental = NO_ACCIDENTAL;
        }
        if (barSide < bar.end || bar.trailer > 0) {
            // skip the space for the bar but don't draw it; multiple notes may cross the same bar
            xPos += BAR_WIDTH * xAxisScale;
        }
    }
}

// to do : whole rest should be centered in measure
void NoteTakerDisplay::drawBarRest(NVGcontext* vg, BarPosition& bar, const DisplayNote& note,
        int xPos, int alpha) const {
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
        xPos += noteDurations[restIndex] * xAxisScale;
        symbol -= restIndex;
    } while (symbol);
}

void NoteTakerDisplay::drawFreeNote(NVGcontext* vg, const DisplayNote& note,
        int xPos, int alpha) const {
    drawNote(vg, note, (Accidental) pitchMap[note.pitch()].accidental, xPos, alpha);
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
        int yTop = 2;
        int yHeight = box.size.y - 4;
        if (module->selectStart + 1 == module->selectEnd && !module->selectButton->ledOn) {
            DisplayType noteType = module->allNotes[module->selectStart].type;
            if (TIME_SIGNATURE == noteType) {
                yTop = (1 - (int) module->verticalWheel->value) * 12 + 35;
                yHeight = 13;
            }
        }
        nvgRect(vg, xStart, yTop, this->xPos(module->selectEnd) - xStart, yHeight);
    }
    unsigned selectChannels = module->selectChannels;
    nvgFillColor(vg, nvgRGBA((2 == selectChannels) * 0xBf, (8 == selectChannels) * 0x7f,
            (4 == selectChannels) * 0x7f, ALL_CHANNELS == selectChannels ? 0x3f : 0x1f));
    nvgFill(vg);
    // draw notes
    BarPosition bar;
    int nextBar = INT_MAX;
    // to do : could optimize this to skip notes except for bar prior to displayStart
    for (unsigned index = 0; index < module->displayStart; ++index) {
        const DisplayNote& note = module->allNotes[index];
        bar.next(note);
        while (nextBar <= this->xPos(index)) {
            accidentals.fill(NO_ACCIDENTAL);
            this->applyKeySignature();
            // note : separate multiplies avoids rounding error
            nextBar += bar.duration * xAxisScale + BAR_WIDTH * xAxisScale;
        }
        switch (note.type) {
            case NOTE_ON: {
                const StaffNote& pitch = pitchMap[note.pitch()];
                accidentals[pitch.position] = (Accidental) pitch.accidental;
            } break;
            case KEY_SIGNATURE:
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE: {
                int xPos = this->xPos(index);
                if (bar.setSignature(note)) {
                    xPos += BAR_WIDTH * xAxisScale;
                }
                nextBar = xPos + TIME_SIGNATURE_WIDTH * xAxisScale + bar.duration * xAxisScale;
            } break;
            default:
                ;
        }
    }
    for (unsigned index = module->displayStart; index < module->displayEnd; ++index) {
        const DisplayNote& note = module->allNotes[index];
        bar.next(note);
        // draw bar once as needed (multiple notes may start at same bar)
        while (nextBar <= this->xPos(index)) {
            this->drawBar(vg, nextBar);
            accidentals.fill(NO_ACCIDENTAL);
            this->applyKeySignature();
            // note : separate multiplies avoids rounding error
            nextBar += bar.duration * xAxisScale + BAR_WIDTH * xAxisScale;
        }
        // to do :  draw tie as needed (possibly multiple)
        switch (note.type) {
            case NOTE_ON:
                this->drawBarNote(vg, bar, note, this->xPos(index) + 8, 0xFF);              
            break;
            case REST_TYPE:
                this->drawBarRest(vg, bar, note, this->xPos(index) + 8, 0xFF);
            break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE: {
                int xPos = this->xPos(index);
                this->setKeySignature(note.key());
                if (note.startTime) {
                    this->drawBar(vg, xPos);
                    this->drawBar(vg, xPos + 2);
                    xPos += DOUBLE_BAR_WIDTH * xAxisScale;
                }
                const char* mark;
                unsigned keySig;
                const uint8_t* accKeys;
                if (keySignature > 0) {
                    mark = "#";
                    keySig = keySignature;
                    accKeys = trebleSharpKeys;
                } else {
                    mark = "$";
                    keySig = -keySignature;
                    accKeys = trebleFlatKeys;
                }
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                for (unsigned index = 0; index < keySig; ++index) {
                    nvgText(vg, xPos, accKeys[index] * 3 - 48, mark, NULL);
                    xPos += ACCIDENTAL_WIDTH * xAxisScale;
                }
            } break;
            case TIME_SIGNATURE: {
                int xPos = this->xPos(index);
                if (bar.setSignature(note)) {
                    this->drawBar(vg, xPos);
                    xPos += BAR_WIDTH * xAxisScale;
                }
                // note : separate multiplies avoids rounding error
                nextBar = xPos + TIME_SIGNATURE_WIDTH * xAxisScale + bar.duration * xAxisScale;
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                std::string numerator = std::to_string(note.numerator());
                xPos += -4 * (numerator.size() - 1) + 3;
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
    BarPosition bar;
    int pos = 0;
    for (unsigned index = 0; index < module->allNotes.size(); ++index) {
        const DisplayNote& note = module->allNotes[index];
        bar.next(note);
        debug("inSignature %d start %d note.startTime %d base %d duration %d leader %d",
                bar.inSignature, bar.start, note.startTime, bar.base, bar.duration, bar.leader);
        xPositions[index] = (note.startTime + (bar.count + bar.start) * BAR_WIDTH) * xAxisScale
                + pos;
        switch (note.type) {
            case MIDI_HEADER:
                assert(!pos);
                assert(!index);
                pos = CLEF_WIDTH * xAxisScale;
                break;
            case NOTE_ON:
            case REST_TYPE: {
                // to do : if note is very short, pad it a bit
                //         if multiple notes from different channels overlap, space them out
                // if note crossed bar, add space for tie
                int notesTied = bar.notesTied(note);
                if (notesTied > 1) {
                    int extraWidth = NOTE_WIDTH * (notesTied - 1);  // guess at how wide a note is
                    pos += (int) std::max(0.f, (extraWidth - note.duration) * xAxisScale);
                    debug("end %d trailer %d notesTied",
                            bar.end, bar.trailer, notesTied);
                }
                } break;
            case KEY_SIGNATURE:
                if (note.startTime) {
                    pos += DOUBLE_BAR_WIDTH * xAxisScale;
                }
                pos += abs(note.key()) * ACCIDENTAL_WIDTH * xAxisScale;
                break;
            case TIME_SIGNATURE:
                pos += TIME_SIGNATURE_WIDTH * xAxisScale;  // add space to draw time signature
                bar.setSignature(note);
                break;
            case TRACK_END:
                debug("[%u] xPos %d start %d", index, xPositions[index], note.startTime);
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    debug("updateXPosition size %u", xPositions.size());
}
