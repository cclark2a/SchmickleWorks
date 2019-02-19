#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

// map midi note for C major to drawable position
enum Accidental : uint8_t {
    NO_ACCIDENTAL,
    SHARP_ACCIDENTAL,
    FLAT_ACCIDENTAL,
    NATURAL_ACCIDENTAL,
};

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

void NoteTakerDisplay::drawNote(NVGcontext *vg, const DisplayNote& note, int xPos, int alpha) {
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
    switch (pitch.accidental) {
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
    const char upFlagNoteSymbols[] = "CDDEEFFGGHHIIJJKKLLM";
    const char downFlagNoteSymbols[] = "cddeeffgghhiijjkkllm";
    static_assert(sizeof(upFlagNoteSymbols) - 1 == noteDurations.size(),
            "symbol duration mismatch");
    static_assert(sizeof(downFlagNoteSymbols) - 1 == noteDurations.size(),
            "symbol duration mismatch");
    unsigned symbol = note.note();
    nvgFontSize(vg, 42);
    char noteStr[2];
    noteStr[0] = (pitch.position <= MIDDLE_C && pitch.position > C_5) || pitch.position >= C_3
            ? upFlagNoteSymbols[symbol] : downFlagNoteSymbols[symbol];
    int noteCount = 1 + (symbol > 0 && (symbol & 1) == 0);
    if (2 == noteCount) {
        noteStr[1] = '.';
    }
    nvgText(vg, xPos, yPos, noteStr, noteStr + noteCount);
}

void NoteTakerDisplay::drawRest(NVGcontext *vg, const DisplayNote& note, int xPos, int alpha) {
    const char restSymbols[] = "oppqqrrssttuuvvwwxxyy";
    unsigned symbol = note.rest();
    float yPos = 36 * 3 - 49;
    nvgFillColor(vg, nvgRGBA((1 == note.channel) * 0xBf, (3 == note.channel) * 0x7f,
            (2 == note.channel) * 0x7f, alpha));
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

void NoteTakerDisplay::draw(NVGcontext *vg) {
    // draw bounds
    // to do : clip to bounds
	nvgStrokeWidth(vg, 1.0);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
	nvgStrokeColor(vg, nvgRGB(0, 0, 0));
	nvgStroke(vg);
    // draw vertical line at end of staff lines
    nvgBeginPath(vg);
    nvgMoveTo(vg, 2, 36);
    nvgLineTo(vg, 2, 96);
    nvgStrokeWidth(vg, 0.5);
    nvgStroke(vg);
    // draw staff lines
    nvgBeginPath(vg);
    for (int staff = 36; staff <= 72; staff += 36) {
        for (int y = staff; y <= staff + 24; y += 6) { 
	        nvgMoveTo(vg, 2, y);
	        nvgLineTo(vg, box.size.x - 1, y);
        }
    }
	nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
	nvgStroke(vg);
     // draw treble and bass clefs
    nvgFontFaceId(vg, module->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 42);
    nvgText(vg, 4, 60, "(", NULL);
    nvgFontSize(vg, 36);
    nvgText(vg, 4, 92, ")", NULL);
    // draw selection rect
    nvgBeginPath(vg);
    const DisplayNote& selectStart = module->allNotes[module->selectStart];
    int xStart = 32 + selectStart.startTime / 4;
    if (module->selectButton->editStart()) {
        if (module->selectStart == 0) {
            nvgRect(vg, 24, 0, 4, box.size.y);
        } else {
            nvgRect(vg, xStart + 13, 0, 4, box.size.y);
        }
    } else {
        const DisplayNote& selectEnd = module->allNotes[module->selectEnd];
        int xEnd = 32 + selectEnd.startTime / 4;
        nvgRect(vg, xStart - 5, 0, xEnd - xStart, box.size.y);
    }
    unsigned selectChannels = module->selectChannels;
    nvgFillColor(vg, nvgRGBA((1 == selectChannels) * 0xBf, (4 == selectChannels) * 0x7f,
            (2 == selectChannels) * 0x7f, 0x1f));
    nvgFill(vg);
    // draw notes
    for (unsigned i = module->displayStart; i < module->displayEnd; ++i) {
        const DisplayNote& note = module->allNotes[i];
        switch (note.type) {
            case NOTE_ON: {
                int xPos = 32 + note.startTime / 4;
                this->drawNote(vg, note, xPos, 0xFF);
            } break;
            case REST_TYPE: {
                int xPos = 32 + note.startTime / 4;
                this->drawRest(vg, note, xPos, 0xFF);
            } break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE:
                // to do
            break;
            case TIME_SIGNATURE:
                debug("draw time signature %d %d\n", note.startTime, note.pitch());
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                nvgText(vg, 32 + note.startTime / 4, note.pitch(), "'", NULL);
            break;
            case MIDI_TEMPO:
            break;
            case TRACK_END:
            break;
            default:
                debug("to do: add type %d\n", note.type);
                assert(0); // incomplete
        }
    }
}

