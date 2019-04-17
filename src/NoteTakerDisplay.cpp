#include "NoteTakerDisplay.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

/// drawing notes must line up with extra bar space added by updateXPosition
struct BarPosition {
    int prior = 0;           // number of bars before current time signature
    int duration = INT_MAX;  // midi time of one bar
    int base = 0;            // time of current time signature
    int inSignature;         // time of note into current signature
    int start;               // number of bars in current time signature
    int end;                 // number of bars though current note duration
    int leader;              // duration of note part before first bar
    int ppq;
    int trailer;             // remaining duration of note after last bar

    BarPosition(const NoteTaker& nt) {
        ppq = nt.ppq;
    }

    void next(const DisplayNote& note) {
        inSignature = note.startTime - base;
        start = inSignature / duration;
        leader = std::min(note.duration, (start + 1) * duration - inSignature);
    }

    int notesTied(const DisplayNote& note) {
        int noteEndTime = inSignature + note.duration;
        end = noteEndTime / duration;
        trailer = noteEndTime - end * duration;
        int result = NoteTakerDisplay::TiedCount(duration, leader, ppq);
        if (start != end) {
            result += end - start - 1;
            if (trailer) {
                result += NoteTakerDisplay::TiedCount(duration, trailer, ppq);
            }
        }
        return result;
    }

    bool setSignature(const DisplayNote& note) {
        base = note.startTime;
        // see if introducing new signature adds a bar
        bool drawBar = inSignature > 0 && inSignature < duration;
        prior += drawBar;
        duration = ppq * 4 * note.numerator() / (1 << note.denominator());
        return drawBar;
    }
};

static void draw_stem(NVGcontext*vg, float x, int ys, int ye) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + .5, ys);
    nvgLineTo(vg, x + .5, ye);
    nvgStrokeWidth(vg, .5);
    nvgStroke(vg);
}

struct BeamPositions {
    unsigned beamMin = INT_MAX;
    float xOffset;
    int yOffset;
    float slurOffset;
    float sx;
    float ex;
    int yStemExtend;
    int y;
    int yLimit;

    void drawOneBeam(NVGcontext* vg) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, sx, y);
        nvgLineTo(vg, ex, y);
        nvgLineTo(vg, ex, y + 3);
        nvgLineTo(vg, sx, y + 3);
        nvgFill(vg);
        y += yOffset;
    }
};

const int CLEF_WIDTH = 96;
const int TIME_SIGNATURE_WIDTH = 48;
const float MIN_KEY_SIGNATURE_WIDTH = 4;
const int TEMPO_WIDTH = 4;
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
const char* upBeamNoteSymbols[] = {   "H", "H", "H.", "H", "H.", "H", "H.", "H", "H.", "H", "H.",
                                           "I", "I.", "J", "J.", "K", "K.", "L", "L.", "M" };
const char* downBeamNoteSymbols[] = { "h", "h", "h.", "h", "h.", "h", "h.", "h", "h.", "h", "h.",
                                           "i", "i.", "j", "j.", "k", "k.", "l", "l.", "m" };

NoteTakerDisplay::NoteTakerDisplay(const Vec& pos, const Vec& size, NoteTaker* n)
    : nt(n)
    , pitchMap(sharpMap) {
    box.pos = pos;
    box.size = size;
    assert(sizeof(upFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
    assert(sizeof(downFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
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

void NoteTakerDisplay::cacheBeams() {
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        const DisplayNote& note = nt->allNotes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        NoteCache& noteCache = cache[index];
        unsigned& beamStart = beamStarts[note.channel];
        bool checkForStart = true;
        if (INT_MAX != beamStart) {
            bool stemsMatch = cache[beamStart].stemUp == noteCache.stemUp;
            if (!stemsMatch || noteCache.vDuration >= nt->ppq) {
                this->closeBeam(beamStart, index);
                beamStart = INT_MAX;
            } else {
                int onBeat = note.endTime() % nt->ppq;
                if (!onBeat || PositionType::right == noteCache.tupletPosition) {
                    noteCache.beamPosition = PositionType::right;
                    beamStart = INT_MAX;
                } else {
                    noteCache.beamPosition = PositionType::mid;
                }
                checkForStart = false;
            }
        } 
        if (checkForStart && noteCache.vDuration < nt->ppq) {
            // to do : if 4:4 time, allow beaming up to half note ?
            beamStart = index;
            noteCache.beamPosition = PositionType::left;
        }
        if (INT_MAX != beamStart || PositionType::right == noteCache.beamPosition) {
            noteCache.beamCount = NoteDurations::Beams(
                    NoteDurations::FromMidi(noteCache.vDuration, nt->ppq));
        }
    }
    for (auto beamStart : beamStarts) {
        if (INT_MAX != beamStart) {
            this->closeBeam(beamStart, cache.size());
            debug("beam unmatched %s", nt->allNotes[beamStart].debugString().c_str());
        }
    }
}

void NoteTakerDisplay::cacheSlurs() {
    array<unsigned, CHANNEL_COUNT> slurStarts;
    slurStarts.fill(INT_MAX);
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        const DisplayNote& note = nt->allNotes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        NoteCache& noteCache = cache[index];
        unsigned& slurStart = slurStarts[note.channel];
        bool checkForStart = true;
        if (INT_MAX != slurStart) {
            if (!note.slur()) {
                this->closeSlur(slurStart, index);
                debug("set slur close %s", nt->allNotes[slurStart].debugString().c_str());
                slurStart = INT_MAX;
            } else {
                noteCache.slurPosition = PositionType::mid;
                checkForStart = false;
                debug("set slur mid %s", note.debugString().c_str());
            }
        }
        if (checkForStart && note.slur()) {
            slurStart = index;
            noteCache.slurPosition = PositionType::left;
            debug("set slur left %s", note.debugString().c_str());
        }
    }
    for (auto slurStart : slurStarts) {
        if (INT_MAX != slurStart) {
            this->closeSlur(slurStart, cache.size());
            debug("slur unmatched %s", nt->allNotes[slurStart].debugString().c_str());
        }
    }
}

void NoteTakerDisplay::cacheTuplets() {
    array<unsigned, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(INT_MAX);
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        const DisplayNote& note = nt->allNotes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        NoteCache& noteCache = cache[index];
        unsigned& tripStart = tripStarts[note.channel];
        bool checkForStart = true;
        if (INT_MAX != tripStart) {
            const DisplayNote& start = nt->allNotes[tripStart];
            int totalDuration = note.endTime() - start.startTime;
            if (totalDuration > nt->ppq * 4) {
                // to do : use time time signature to limit triplet to bar 
                // for now, limit triplet to whole note
                this->clearTuplet(tripStart, index);
                tripStart = INT_MAX;
            } else {
                int tripDur = start.duration * 3;
                int factor = totalDuration / tripDur;
                if (totalDuration * 2 == tripDur || factor * tripDur == totalDuration) {
                    noteCache.tupletPosition = PositionType::right;
                    tripStart = INT_MAX;
                } else {
                    noteCache.tupletPosition = PositionType::mid;
                }
                checkForStart = false;
            }
        } 
        if (checkForStart && NoteDurations::TripletPart(note.duration, nt->ppq)) {
            tripStart = index;
            noteCache.tupletPosition = PositionType::left;
        }
    }
    for (auto tripStart : tripStarts) {
        if (INT_MAX != tripStart) {
            this->clearTuplet(tripStart, cache.size());
            debug("tuplet unmatched %s", nt->allNotes[tripStart].debugString().c_str());
        }
    }
}

void NoteTakerDisplay::clearTuplet(unsigned index, unsigned limit) {
    assert(PositionType::left == cache[index].tupletPosition);
    int chan = nt->allNotes[index].channel;
    cache[index].tupletPosition = PositionType::none;
    while (++index < limit) {
        if (chan == nt->allNotes[index].channel) {
            assert(PositionType::mid == cache[index].tupletPosition);
            cache[index].tupletPosition = PositionType::none;
        }
    }
}

void NoteTakerDisplay::closeBeam(unsigned first, unsigned limit) {
    assert(PositionType::left == cache[first].beamPosition);
    int chan = nt->allNotes[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (chan == nt->allNotes[index].channel) {
            if (PositionType::mid != cache[index].beamPosition) {
                break;
            }
            last = index;
        }
    }
    cache[last].beamPosition = last == first ? PositionType::none : PositionType::right;
}

void NoteTakerDisplay::closeSlur(unsigned first, unsigned limit) {
    assert(PositionType::left == cache[first].slurPosition);
    int chan = nt->allNotes[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (chan == nt->allNotes[index].channel) {
            if (PositionType::mid != cache[index].slurPosition) {
                break;
            }
            last = index;
        }
    }
    cache[last].slurPosition = last == first ? PositionType::none : PositionType::right;
    debug("close slur first %d last %d pos %u", first, last, cache[last].slurPosition);
}

void NoteTakerDisplay::drawBar(NVGcontext* vg, int midiTime) {
    assert(midiTime <= nt->allNotes.back().startTime);
    // to do : optimize with binary search ?
    int prior = -1;
    for (const auto& note : nt->allNotes) {
        if (note.startTime >= midiTime) {
            break;
        }
        prior++;
    }
    int start = nt->allNotes[prior].startTime;
    int end = nt->allNotes[prior + 1].startTime;
    if (false) debug("midi %d prior %d start %s end %s", midiTime, prior, 
            nt->allNotes[prior].debugString().c_str(),
            nt->allNotes[prior + 1].debugString().c_str());
    assert(start < midiTime && midiTime <= end);
    int xStart = cache[prior].xPosition;
    int xEnd = cache[prior + 1].xPosition;
    assert(xStart < xEnd);
    // to do : first approximation : may need to adjust to allow for accidentals
    float xPos = xStart + (xEnd - xStart) * (midiTime - start) / (end - start);
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
void NoteTakerDisplay::drawBarNote(NVGcontext* vg, BarPosition& bar, const DisplayNote& note,
        const NoteCache& noteCache, unsigned char alpha) {
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
    int xPos = 8;
    int tied = bar.notesTied(note);
    if (1 == tied) {
        drawNote(vg, note, accidental, noteCache, alpha, 42, xPos);
        return;
    }
    DisplayNote copy = note;
    int lastXPos = INT_MAX;
    copy.duration = bar.leader;
    for (int barSide = bar.start; barSide <= bar.end; ++barSide) {
        if (barSide > bar.start) {
            copy.duration = barSide < bar.end ? bar.duration : bar.trailer;
        }
        while (copy.duration >= NoteDurations::ToMidi(0, nt->ppq)) {
            // to do : allow triplets to cross bars?
            drawNote(vg, copy, accidental, noteCache, alpha, 42, xPos);
            // to do : advance by at least NOTE_WIDTH (need corresponding change in calc x pos)
            if (INT_MAX != lastXPos) {  // draw tie from last note to here
                // if notes' stems go down, draw arc above; otherwise, draw arc below
                int yOff = this->stemUp(pitch.position) ? 2 : -2;
                int yPos = this->yPos(pitch.position) + yOff;
                int mid = (lastXPos + noteCache.xPosition + xPos) / 2;
                nvgBeginPath(vg);
                nvgMoveTo(vg, lastXPos + 2, yPos);
                nvgQuadTo(vg, mid, yPos + yOff, noteCache.xPosition + xPos - 2, yPos);
                nvgQuadTo(vg, mid, yPos + yOff * 2, lastXPos + 2, yPos);
                nvgFill(vg);
            }
            lastXPos = noteCache.xPosition + xPos;
            xPos += NoteDurations::InStd(copy.duration, nt->ppq) * xAxisScale;
            copy.duration -= NoteDurations::Closest(copy.duration, nt->ppq);
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
        int xPos, unsigned char alpha) const {
    const char restSymbols[] = "oppqqrrssttuuvvwwxxyy";
    // to do : if rest is part of triplet, adjust duration before lookup
    unsigned symbol = NoteDurations::FromMidi(note.duration, nt->ppq);
    float yPos = 36 * 3 - 49;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, nt->musicFont->handle);
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
        xPos += NoteDurations::ToMidi(restIndex, nt->ppq) * xAxisScale;
        symbol -= restIndex;
    } while (symbol);
}

void NoteTakerDisplay::drawBeam(NVGcontext* vg, unsigned start, unsigned char alpha) const {
    assert(PositionType::left == cache[start].beamPosition);
    assert(nt->allNotes.size() == cache.size());
    BeamPositions bp;
    unsigned index = start;
    int chan = nt->allNotes[start].channel;
    do {
        if (chan != nt->allNotes[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].beamPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
    } while (++index < cache.size());
    assert(PositionType::right == cache[index].beamPosition);
    SetNoteColor(vg, chan, alpha);
    for (unsigned count = 0; count < bp.beamMin; ++count) {
        bp.drawOneBeam(vg);
    }
    index = start;
    int yReset = bp.y;
    do {
        assert(chan == nt->allNotes[index].channel);
        const NoteCache& noteCache = cache[index];
        while (++index < cache.size() && chan != nt->allNotes[index].channel)
            ;
        if (index == cache.size()) {
            debug("drawBeam found unbalanced beam");
            assert(0);
            return;
        }
        const NoteCache& next = cache[index];
        bp.sx = noteCache.xPosition + bp.xOffset;
        draw_stem(vg, bp.sx, noteCache.yPosition + bp.yStemExtend, bp.yLimit);
        bp.ex = next.xPosition + bp.xOffset;
        bp.y = yReset;
        bool fullBeam = true;
        for (unsigned count = bp.beamMin; count < noteCache.beamCount; ++count) {
            if (fullBeam && next.beamCount <= count) { // draw partial beam on left
                bp.ex = bp.sx + 6;
                fullBeam = false;
            } 
            bp.drawOneBeam(vg);
        }
        if (PositionType::right == next.beamPosition) { // draw partial beam on right
            bp.ex = next.xPosition + bp.xOffset;
            draw_stem(vg, bp.ex, next.yPosition + bp.yStemExtend, bp.yLimit);
            bp.sx = bp.ex - 6;
            for (unsigned count = noteCache.beamCount; count < next.beamCount; ++count) {
                bp.drawOneBeam(vg);
            }
            break;
        }
    } while (true);
}

void NoteTakerDisplay::drawBevel(NVGcontext* vg) const {
    const float bevel = -2;
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
}

void NoteTakerDisplay::drawClefs(NVGcontext* vg) const {
         // draw treble and bass clefs
    nvgFontFaceId(vg, nt->musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, 42);
    nvgText(vg, 5, 60, "(", NULL);
    nvgFontSize(vg, 36);
    nvgText(vg, 5, 92, ")", NULL);
}

void NoteTakerDisplay::drawFreeNote(NVGcontext* vg, const DisplayNote& note,
        int xPos, unsigned char alpha) const {
    const StaffNote& pitch = pitchMap[note.pitch()];
    NoteCache noteCache;
    noteCache.resetTupleBeam();
    noteCache.xPosition = 0;
    noteCache.yPosition = this->yPos(pitch.position);
    noteCache.vDuration = note.duration;
    noteCache.symbol = (uint8_t) NoteDurations::FromMidi(note.duration, nt->ppq);
    noteCache.stemUp = true;
    drawNote(vg, note, (Accidental) pitchMap[note.pitch()].accidental, noteCache, alpha, 24, xPos);
}

void NoteTakerDisplay::drawNote(NVGcontext* vg, const DisplayNote& note, Accidental accidental,
        const NoteCache& noteCache, unsigned char alpha, int size, int xOffset) const {
    const StaffNote& pitch = pitchMap[note.pitch()];
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
    int xPos = noteCache.xPosition + xOffset;
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
    SetNoteColor(vg, note.channel, alpha);
    nvgFontFaceId(vg, nt->musicFont->handle);
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
    auto& symbols = PositionType::none != noteCache.beamPosition ?
            noteCache.stemUp ? upBeamNoteSymbols : downBeamNoteSymbols :
            noteCache.stemUp ? upFlagNoteSymbols : downFlagNoteSymbols; 
    const char* noteStr = symbols[noteCache.symbol];
    nvgText(vg, xPos, yPos, noteStr, nullptr);
}

void NoteTakerDisplay::drawNotes(NVGcontext* vg, BarPosition& bar, int nextBar) {
    for (unsigned index = nt->displayStart; index < nt->displayEnd; ++index) {
        const DisplayNote& note = nt->allNotes[index];
        bar.next(note);
        // draw bar once as needed (multiple notes may start at same bar)
    //    debug("%u drawNotes nextBar %d xPos %d", index, nextBar, cache[index].xPosition);
        while (nextBar <= note.startTime) {
            this->drawBar(vg, nextBar);
            this->restartBar(bar, nextBar);
        }
        switch (note.type) {
            case NOTE_ON: {
                unsigned char alpha = nt->isSelectable(note) ? 0xff : 0x7f;
                this->drawBarNote(vg, bar, note, cache[index], alpha);
                bool drawBeams = PositionType::left == cache[index].beamPosition;
                if (drawBeams) {
                    this->drawBeam(vg, index, alpha);
                }
                if (PositionType::left == cache[index].tupletPosition) {
                    this->drawTuple(vg, index, alpha, drawBeams);
                }
                if (PositionType::left == cache[index].slurPosition) {
                    this->drawSlur(vg, index, alpha);
                }
            } break;
            case REST_TYPE:
                this->drawBarRest(vg, bar, note, cache[index].xPosition + 8,
                        nt->isSelectable(note) ? 0xff : 0x7f);
            break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE: {
                int xPos = cache[index].xPosition;
                this->setKeySignature(note.key());
                if (note.startTime) {
                    this->drawBar(vg, xPos);
                    this->drawBar(vg, xPos + 2);
                    xPos += DOUBLE_BAR_WIDTH * xAxisScale;
                }
                const char* mark;
                unsigned keySig;
                const uint8_t* accKeys, * bassKeys;
                if (keySignature > 0) {
                    mark = "#";
                    keySig = keySignature;
                    accKeys = trebleSharpKeys;
                    bassKeys = bassSharpKeys;
                } else {
                    mark = "$";
                    keySig = -keySignature;
                    accKeys = trebleFlatKeys;
                    bassKeys = bassFlatKeys;
                }
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                for (auto keys : { accKeys, bassKeys }) {
                    int xKeyPos = xPos;
                    for (unsigned index = 0; index < keySig; ++index) {
                        nvgText(vg, xKeyPos, keys[index] * 3 - 48, mark, NULL);
                        xKeyPos += ACCIDENTAL_WIDTH * xAxisScale;
                    }
                }
            } break;
            case TIME_SIGNATURE: {
                int xPos = cache[index].xPosition;
                if (bar.setSignature(note)) {
                    this->drawBar(vg, xPos);
                    xPos += BAR_WIDTH * xAxisScale;
                }
                // note : separate multiplies avoids rounding error
                // to do : use nvgTextAlign() and nvgTextBounds() instead of hard-coding
                nextBar = note.startTime + bar.duration;
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                std::string numerator = std::to_string(note.numerator());
                xPos += -4 * (numerator.size() - 1) + 3;
                nvgFontFaceId(vg, nt->musicFont->handle);
                nvgFontSize(vg, 42);
                nvgText(vg, xPos, 32 * 3 - 49, numerator.c_str(), NULL);
                nvgText(vg, xPos, 44 * 3 - 49, numerator.c_str(), NULL);
                std::string denominator = std::to_string(1 << note.denominator());
                xPos = cache[index].xPosition - 4 * (denominator.size() - 1) + 3;
                nvgText(vg, xPos, 36 * 3 - 49, denominator.c_str(), NULL);
                nvgText(vg, xPos, 48 * 3 - 49, denominator.c_str(), NULL);
            } break;
            case MIDI_TEMPO:
                this->drawTempo(vg, cache[index].xPosition, note.tempo(), 0xFF);
            break;
            case TRACK_END:
            break;
            default:
                debug("to do: add type %d\n", note.type);
                assert(0); // incomplete
        }
    }
}

void NoteTakerDisplay::drawSelectionRect(NVGcontext* vg) const {
    // draw selection rect
    int xStart = cache[nt->selectEndPos(nt->selectStart)].xPosition - 2;
    int width = 4;
    int yTop = 0;
    int yHeight = box.size.y;
    unsigned channel;
    if (nt->selectButton->editStart()) {
        channel = 1 << nt->partButton->addChannel;
    } else {
        channel = nt->selectChannels;
        if (nt->fileButton->ledOn) {
            yHeight = box.size.y - 35;
        }
        if (nt->selectStart + 1 == nt->selectEnd) {
            DisplayType noteType = nt->allNotes[nt->selectStart].type;
            if (TIME_SIGNATURE == noteType) {
                if (!nt->selectButton->ledOn) {
                    yTop = (1 - (int) nt->verticalWheel->value) * 12 + 35;
                    yHeight = 13;
                }
            } else if (MIDI_TEMPO == noteType) {
                nvgBeginPath(vg);
                nvgRect(vg, xStart + width, yTop, 30 - width, 12);
                SetSelectColor(vg, channel);
                nvgFill(vg);
            }
        }
        if (nt->selectEnd > 0) {
            xStart = cache[nt->selectStart].xPosition;
            width = cache[nt->selectEndPos(nt->selectEnd - 1)].xPosition - xStart;
        }
    }
    nvgBeginPath(vg);
    nvgRect(vg, xStart, yTop, width, yHeight);
    SetSelectColor(vg, channel);
    nvgFill(vg);
}

void NoteTakerDisplay::drawStaffLines(NVGcontext* vg) const {
        // draw vertical line at end of staff lines
    float beginEdge = std::max(0.f, 3 - xAxisOffset - dynamicXOffsetTimer);
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

void NoteTakerDisplay::draw(NVGcontext* vg) {
    if (!nt->allNotes.size()) {
        return;  // do nothing if we're not set up yet
    }
    accidentals.fill(NO_ACCIDENTAL);
    this->drawBevel(vg);
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgFill(vg);
    this->drawStaffLines(vg);
    nvgSave(vg);
    this->scroll(vg);
    this->drawClefs(vg);
    this->drawSelectionRect(vg);
    BarPosition bar(*nt);
    int nextBar = INT_MAX;
    this->setUpAccidentals(vg, bar, nextBar);
    this->drawNotes(vg, bar, nextBar);
    nvgRestore(vg);
    this->recenterVerticalWheel();
    if (nt->fileButton->ledOn) {
        this->drawFileControl(vg);
    }
    if (nt->partButton->ledOn) {
        this->drawPartControl(vg);
    }
    if (nt->sustainButton->ledOn) {
        this->drawSustainControl(vg);
    }
    if (nt->tieButton->ledOn) {
        this->drawTieControl(vg);
    }
    if (nt->runningWithButtonsOff()) {
        this->drawDynamicPitchTempo(vg);
    }
	FramebufferWidget::draw(vg);
    dirty = false;
}

void NoteTakerDisplay::drawDynamicPitchTempo(NVGcontext* vg) {
    if (!nt->fileButton->ledOn) {
        if (nt->verticalWheel->hasChanged()) {
            dynamicPitchTimer = nt->realSeconds + fadeDuration;
        }
        if (nt->horizontalWheel->lastRealValue != nt->horizontalWheel->value) {
            dynamicTempoTimer = nt->realSeconds + fadeDuration;
            nt->horizontalWheel->lastRealValue = nt->horizontalWheel->value;
        }
        dynamicTempoAlpha = (int) (255 * (dynamicTempoTimer - nt->realSeconds) / fadeDuration);
        dynamicPitchAlpha = (int) (255 * (dynamicPitchTimer - nt->realSeconds) / fadeDuration);
    }
    if (dynamicPitchAlpha > 0) {
        DisplayNote note = {0, nt->ppq, { 0, 0, 0, 0}, 0, NOTE_ON, false };
        note.setPitch((int) nt->verticalWheel->value);
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 10, 2, 10, box.size.y - 4);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, dynamicPitchAlpha));
        nvgFill(vg);
        this->drawFreeNote(vg, note, box.size.x - 7, dynamicPitchAlpha);
    }
    if (dynamicTempoAlpha > 0) {
        this->drawTempo(vg, 2, nt->tempo, dynamicTempoAlpha);
    }
}

void NoteTakerDisplay::drawFileControl(NVGcontext* vg) {
    bool loadEnabled = (unsigned) nt->horizontalWheel->value < nt->storage.size();
    this->drawVerticalLabel(vg, "load", loadEnabled, upSelected, 0);
    this->drawVerticalLabel(vg, "save", true, downSelected, box.size.y - 50);
    this->drawVerticalControl(vg);
    // draw horizontal control
    const float boxWidth = 25;
    float fSlot = nt->horizontalWheel->value;
    int slot = (int) (fSlot + .5);
    nvgBeginPath(vg);
    nvgRect(vg, 40 + (slot - xControlOffset) * boxWidth, box.size.y - boxWidth - 5,
            boxWidth, boxWidth);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgFill(vg);
    auto drawEmpty = [&](unsigned index) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, boxWidth, boxWidth);
        nvgMoveTo(vg, 0, 0);
        nvgLineTo(vg, boxWidth, boxWidth);
        nvgMoveTo(vg, boxWidth, 0);
        nvgLineTo(vg, 0, boxWidth);
        nvgStrokeWidth(vg, 0.5);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgStroke(vg);
    };
    auto drawNumber = [&](unsigned index) {
        float textX = boxWidth - 2;
        float textY = 8;
        nvgFontFaceId(vg, nt->textFont->handle);
        nvgFontSize(vg, 10);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT);
        std::string label = std::to_string(index);
        float bounds[4];
        nvgTextBounds(vg, textX, textY, label.c_str(), NULL, bounds);
        nvgBeginPath(vg);
        nvgRect(vg, bounds[0] - 1, bounds[1], bounds[2] - bounds[0] + 2,
                bounds[3] - bounds[1]);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0x7F));
        nvgFill(vg);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgText(vg, textX, textY, label.c_str(), NULL);
    };
    auto drawNote = [&](unsigned index) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, boxWidth, boxWidth);
        nvgStrokeWidth(vg, 1);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgStroke(vg);
        nvgFontFaceId(vg, nt->musicFont->handle);
        nvgFontSize(vg, 16);
        nvgTextAlign(vg, NVG_ALIGN_CENTER);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgText(vg, boxWidth / 2, boxWidth - 3, "H", NULL);
    };
    // horizontalWheel->value auto-drifts towards integer, range of 0 to nt->storage.size()
    if (fSlot < slot) {
        nt->horizontalWheel->value = std::min((float) slot, fSlot + .02f);
    } else if (fSlot > slot) {
        nt->horizontalWheel->value = std::max((float) slot, fSlot - .02f);
    }
    // xControlOffset draws current location, 0 to nt->storage.size() - 4
    if (nt->horizontalWheel->value - xControlOffset > 3) {
        xControlOffset += .04f;
    } else if (nt->horizontalWheel->value < xControlOffset) {
        xControlOffset -= .04f;
    }
    if (xControlOffset != floorf(xControlOffset)) {
        if (xControlOffset > 0 && (nt->horizontalWheel->value - xControlOffset < 1
                || (xControlOffset - floorf(xControlOffset) < .5
                && nt->horizontalWheel->value - xControlOffset < 3))) {
            xControlOffset = std::max(floorf(xControlOffset), xControlOffset - .04f);
        } else {
            xControlOffset = std::min(ceilf(xControlOffset), xControlOffset + .04f);
        }
    }
    const int first = std::max(0, (int) (xControlOffset - 1));
    const int last = std::min((int) nt->storage.size(), (int) (xControlOffset + 5));
    nvgSave(vg);
    nvgScissor(vg, 40 - boxWidth / 2, box.size.y - boxWidth - 5, boxWidth * 5, boxWidth);
    for (int index = first; index < last; ++index) {
        nvgSave(vg);
        nvgTranslate(vg, 40 + (index - xControlOffset) * boxWidth, box.size.y - boxWidth - 5);
        if (nt->storage[index].empty()) {
            drawEmpty(index);
        } else {
            drawNote(index);
        }
        drawNumber(index);
        if (first == index && xControlOffset > .5) {  // to do : offscreen with transparency ?
            NVGpaint paint = nvgLinearGradient(vg, boxWidth / 2, 0, boxWidth, 0,
                    nvgRGBA(0xFF, 0xFF, 0xFF, 0), nvgRGBA(0xFF, 0xFF, 0xFF, 0xFF));
            nvgBeginPath(vg);
            nvgRect(vg, boxWidth / 2, 0, boxWidth / 2, boxWidth);
            nvgFillPaint(vg, paint);
            nvgFill(vg);
        } else if (last - 1 == index && xControlOffset + 4.5 < (float) nt->storage.size()) {
            NVGpaint paint = nvgLinearGradient(vg, 0, 0, boxWidth / 2, 0,
                    nvgRGBA(0xFF, 0xFF, 0xFF, 0xFF), nvgRGBA(0xFF, 0xFF, 0xFF, 0));
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, boxWidth / 2, boxWidth);
            nvgFillPaint(vg, paint);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
    nvgRestore(vg);
    unsigned index = nt->storage.size();
    if (index * boxWidth <= box.size.x) {
        drawEmpty(index);
    }
    float wheel = nt->horizontalWheel->value;
    nvgBeginPath(vg);
    nvgRect(vg, 40 + (wheel - xControlOffset) * boxWidth, box.size.y - boxWidth - 5,
            boxWidth, boxWidth);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

void NoteTakerDisplay::drawPartControl(NVGcontext* vg) const {
    int part = nt->horizontalWheel->part();
    bool lockEnable = part < 0 ? true : nt->selectChannels & (1 << part);
    bool editEnable = part < 0 ? true : !lockEnable;
    // to do : locked disabled if channel is already locked, etc
    this->drawVerticalLabel(vg, "lock", lockEnable, upSelected, 0);
    this->drawVerticalLabel(vg, "edit", editEnable, downSelected, box.size.y - 50);
    this->drawVerticalControl(vg);
    // draw horizontal control
    const float boxWidth = 20;
    const float boxHeight = 15;
    nvgBeginPath(vg);
//    nvgRect(vg, 40, box.size.y - boxHeight - 25, boxWidth * 4, 10);
//    nvgRect(vg, 40, box.size.y - 15, boxWidth * 4, 10);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7f));
    nvgFill(vg);
    nvgFontFaceId(vg, nt->textFont->handle);
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    for (int index = -1; index < (int) CV_OUTPUTS; ++index) {
        nvgBeginPath(vg);
        nvgRect(vg, 60 + index * boxWidth, box.size.y - boxHeight - 15,
                boxWidth, boxHeight);
        bool chanLocked = ~nt->selectChannels & (1 << index);
        if (index >= 0 && chanLocked) {
            nvgRect(vg, 60 + index * boxWidth, box.size.y - boxHeight - 25,
                   boxWidth, 10);            
        }
        if (index >= 0 && !chanLocked) {
            nvgRect(vg, 60 + index * boxWidth, box.size.y - 15,
                   boxWidth, 10);            
        }
        SetPartColor(vg, index, part);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, 60 + (index + 0.5f) * boxWidth,
            box.size.y - boxHeight / 2 - 15, 7);
        nvgFill(vg);
        const char num[2] = { (char) ('1' + index), '\0' };
        nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xbf));
        const char* str = index < 0 ? "all" : num;
        nvgFontSize(vg, 13);
        nvgText(vg, 60 + (index + 0.5f) * boxWidth, box.size.y - 18,
                str, nullptr);
        if (0 <= index) {
            nvgSave(vg);
            nvgScissor(vg, 60 + index * boxWidth, box.size.y - boxHeight - 25, boxWidth,
                    boxHeight + 20);
            nvgFontSize(vg, 9);
            nvgFillColor(vg, chanLocked ? nvgRGBA(0xff, 0xff, 0xff, 0xbf) : nvgRGBA(0, 0, 0, 0xbf));
            nvgText(vg, 40 + boxWidth * 3, box.size.y - boxHeight - 18,
                                                            "l    o    c    k    e    d", nullptr);
            nvgFillColor(vg, !chanLocked ? nvgRGBA(0xff, 0xff, 0xff, 0xbf) : nvgRGBA(0, 0, 0, 0xbf));
            nvgText(vg, 40 + boxWidth * 3, box.size.y - 7, "e   d   i   t   a   b   l   e", nullptr);
            nvgRestore(vg);
        }
    }
}

// to do : share code with drawing ties?
// to do : mock score software ?
// in scoring software, slur is drawn from first to last, offset by middle, if needed
void NoteTakerDisplay::drawSlur(NVGcontext* vg, unsigned start, unsigned char alpha) const {
    BeamPositions bp;
    unsigned index = start;
    int chan = nt->allNotes[start].channel;
    do {
        if (chan != nt->allNotes[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].slurPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
    } while (++index < cache.size());
    assert(PositionType::right == cache[index].slurPosition);
    SetNoteColor(vg, chan, alpha);
    float yOff = bp.slurOffset;
    int midOff = (cache[start].stemUp ? 2 : -2);
    float xMid = (bp.sx + bp.ex) / 2;
    float yStart = cache[start].yPosition + yOff * 2;
    float yEnd = cache[index].yPosition + yOff * 2;
    float yMid = (yStart + yEnd) / 2;
    float dx = bp.ex - bp.sx;
    float dy = yEnd - yStart;
    float len = sqrt(dx * dx + dy * dy);
    float dyxX = dx / len * midOff;
    float dyxY = dy / len * midOff;
    nvgBeginPath(vg);
    nvgMoveTo(vg, bp.sx + 2, yStart + yOff);
    nvgQuadTo(vg, xMid - dyxY,     yMid + yOff + dyxX,     bp.ex - 2, yEnd + yOff);
    nvgQuadTo(vg, xMid - dyxY * 2, yMid + yOff + dyxX * 2, bp.sx + 2, yStart + yOff);
    nvgFill(vg);
}

void NoteTakerDisplay::drawSustainControl(NVGcontext* vg) const {
    // draw vertical control
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 35, 25, 35, box.size.y - 30);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x7f));
    nvgFill(vg);
    int select = (int) nt->verticalWheel->value;
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
    NoteTakerChannel& channel = nt->channels[nt->partButton->addChannel];
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
    if (false) debug("sus %d %d rel %d %d", susMin, susMax, relMin, relMax);
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
    nvgFontFaceId(vg, nt->musicFont->handle);
    nvgFontSize(vg, 24);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 2 == select ? 0xFF : 0x7f));
    nvgText(vg, 42, box.size.y - 18, downFlagNoteSymbols[
            NoteDurations::FromMidi(channel.sustainMin, nt->ppq)], nullptr);
    if (susMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 3 == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin, box.size.y - 18, downFlagNoteSymbols[
                NoteDurations::FromMidi(channel.sustainMax, nt->ppq)], nullptr);
    }
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 1 == select ? 0xFF : 0x7f));
    nvgText(vg, 42 + susMin + susMax, box.size.y - 18, downFlagNoteSymbols[
            NoteDurations::FromMidi(channel.releaseMin, nt->ppq)], nullptr);
    if (relMax) {
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0 == select ? 0xFF : 0x7f));
        nvgText(vg, 42 + susMin + susMax + relMin, box.size.y - 18, downFlagNoteSymbols[
                NoteDurations::FromMidi(channel.releaseMax, nt->ppq)], nullptr);
    }
}

void NoteTakerDisplay::drawTempo(NVGcontext* vg, int xPos, int tempo, unsigned char alpha) {
    nvgBeginPath(vg);
    nvgRect(vg, xPos, 2, 30, 12);
    nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, alpha));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, nt->musicFont->handle);
    nvgFontSize(vg, 16);
    nvgText(vg, xPos + 3, 10, "H", nullptr);
    nvgFontFaceId(vg, nt->textFont->handle);
    nvgFontSize(vg, 12);
    std::string tempoStr("= " + std::to_string((int) (120.f * nt->beatsPerHalfSecond(tempo))));
    nvgText(vg, xPos + 7, 10, tempoStr.c_str(), nullptr);
}

// to do : maybe objectify this and share common code for display controls
void NoteTakerDisplay::drawTieControl(NVGcontext* vg) {
    const char labels[] = {'{', 'H', '>'};
    const float boxWidth = 20;
    const float boxSpace = 25;
    const float leftEdge = (box.size.x - boxWidth * sizeof(labels)
            - (boxSpace - boxWidth) * (sizeof(labels) - 1)) / 2;
    nvgBeginPath(vg);
    nvgRect(vg, leftEdge - 5, box.size.y - boxWidth - 10, 
            boxWidth * sizeof(labels) + (boxSpace - boxWidth) * (sizeof(labels) - 1), boxWidth + 20);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x7f));
    nvgFill(vg);
    nvgSave(vg);
    nvgTranslate(vg, leftEdge, box.size.y - boxSpace);
    for (auto ch : labels) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, boxWidth, boxWidth);
        nvgStrokeWidth(vg, 1);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgStroke(vg);
        nvgFontFaceId(vg, nt->musicFont->handle);
        nvgFontSize(vg, 16);
        nvgTextAlign(vg, NVG_ALIGN_CENTER);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgText(vg, boxWidth / 2, boxWidth - 3, &ch, &ch + 1);
        nvgTranslate(vg, boxSpace, 0);
    }
    nvgRestore(vg);
    float fSlot = nt->horizontalWheel->value;
    int slot = (int) (fSlot + .5);
    if (fSlot < slot) {
        nt->horizontalWheel->value = std::min((float) slot, fSlot + .02f);
    } else if (fSlot > slot) {
        nt->horizontalWheel->value = std::max((float) slot, fSlot - .02f);
    }
    nvgBeginPath(vg);
    nvgRect(vg, leftEdge + (nt->horizontalWheel->value) * boxSpace,
            box.size.y - boxWidth - 5, boxWidth, boxWidth);
    nvgStrokeWidth(vg, 3);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

void NoteTakerDisplay::drawTuple(NVGcontext* vg, unsigned start, unsigned char alpha,
        bool drewBeam) const {
    BeamPositions bp;
    unsigned index = start;
    int chan = nt->allNotes[start].channel;
    do {
        if (chan != nt->allNotes[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].tupletPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
    } while (++index < cache.size());
    assert(PositionType::right == cache[index].tupletPosition);
    SetNoteColor(vg, chan, alpha);
    // draw '3' at center of notes above or below staff
    nvgFontFaceId(vg, nt->musicFont->handle);
    nvgFontSize(vg, 16);
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    float centerX = (bp.sx + bp.ex) / 2;
    bool stemUp = cache[start].stemUp;
    float yOffset = stemUp ? -1 : 7.5;
    nvgText(vg, centerX, bp.y + yOffset, "3", NULL);
    // to do : if !drew beam, draw square brackets on either side of '3'
    if (!drewBeam) {
        bp.sx -= 2;
        bp.y += stemUp ? 0 : 3;
        nvgBeginPath(vg);
        nvgMoveTo(vg, bp.sx, bp.y);
        bp.y += stemUp ? -3 : 3;
        nvgLineTo(vg, bp.sx, bp.y);
        nvgLineTo(vg, centerX - 3, bp.y);
        nvgMoveTo(vg, centerX + 3, bp.y);
        bp.ex += 2;
        nvgLineTo(vg, bp.ex, bp.y);
        bp.y -= stemUp ? -3 : 3;
        nvgLineTo(vg, bp.ex, bp.y);
        nvgStrokeWidth(vg, 0.5);
        nvgStroke(vg);
    }
}

void NoteTakerDisplay::drawVerticalControl(NVGcontext* vg) const {
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 10, 20, 5, box.size.y - 40);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
    nvgBeginPath(vg);
    float value = nt->verticalWheel->value;
    float range = nt->verticalWheel->maxValue;
    float yPos = box.size.y - 20 - value * (box.size.y - 40) / range;
    nvgMoveTo(vg, box.size.x - 10, yPos);
    nvgLineTo(vg, box.size.x - 5, yPos - 3);
    nvgLineTo(vg, box.size.x - 5, yPos + 3);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

void NoteTakerDisplay::drawVerticalLabel(NVGcontext* vg, const char* label, bool enabled,
        bool selected, float y) const {
    nvgFontFaceId(vg, nt->textFont->handle);
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

int NoteTakerDisplay::duration(unsigned index, int ppq) const {
    return NoteDurations::InStd(nt->allNotes[index].duration, ppq) * xAxisScale;
}

void NoteTakerDisplay::recenterVerticalWheel() {
    if (upSelected || downSelected) {
        float val = nt->verticalWheel->value;
        const float autoLoadSaveWheelSpeed = .2f;
        nt->verticalWheel->value += val > 5 ? -autoLoadSaveWheelSpeed : +autoLoadSaveWheelSpeed;
        if (5 - autoLoadSaveWheelSpeed < val && val < 5 + autoLoadSaveWheelSpeed) {
            upSelected = false;
            downSelected = false;
        }
    }
}

void NoteTakerDisplay::restartBar(BarPosition& bar, int& nextBar) {
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
    nextBar += bar.duration;
}

void NoteTakerDisplay::scroll(NVGcontext* vg) {
    nvgTranslate(vg, -xAxisOffset - dynamicXOffsetTimer, 0);
    if (!dynamicXOffsetStep) {
        return;
    }
    dynamicXOffsetTimer += dynamicXOffsetStep;
    if ((dynamicXOffsetTimer < 0) == (dynamicXOffsetStep < 0)) {
        dynamicXOffsetTimer = dynamicXOffsetStep = 0;
    }
}

// used by beams and tuplets
void NoteTakerDisplay::setBeamPos(unsigned first, unsigned last, BeamPositions* bp) const {
    unsigned index = first;
    int chan = nt->allNotes[first].channel;
    float yMax = 0;
    float yMin = FLT_MAX;
    unsigned beamMax = 0;
    unsigned lastMeasured = first;
    do {
        if (chan != nt->allNotes[index].channel) {
            continue;
        }
        const NoteCache& noteCache = cache[index];
        yMax = std::max(noteCache.yPosition, yMax);
        yMin = std::min(noteCache.yPosition, yMin);
        beamMax = std::max((unsigned) noteCache.beamCount, beamMax);
        bp->beamMin = std::min((unsigned) noteCache.beamCount, bp->beamMin);
        lastMeasured = index;
    } while (++index <= last);
    bool stemUp = cache[first].stemUp;
    bp->xOffset = stemUp ? 14 : 7.75;
    bp->yOffset = stemUp ? 5 : -5;
    bp->sx = cache[first].xPosition + bp->xOffset;
    bp->ex = cache[last].xPosition + bp->xOffset;
    int yStem = stemUp ? -22 : 15;
    bp->yStemExtend = yStem + (stemUp ? 0 : 3);
    bp->y = (stemUp ? yMin : yMax) + yStem;
    if (beamMax > 3) {
        bp->y -= ((int) beamMax - 3) * bp->yOffset;
    }
    bp->yLimit = bp->y + (stemUp ? 0 : 3);
    if (stemUp) {
        int highFirstLastY = std::max(cache[first].yPosition, cache[lastMeasured].yPosition);
        bp->slurOffset = std::max(0.f, yMax - highFirstLastY);
    } else {
        int lowFirstLastY = std::min(cache[first].yPosition, cache[lastMeasured].yPosition);
        bp->slurOffset = std::min(0.f, yMin - lowFirstLastY) - 3;
    }
}

void NoteTakerDisplay::setKeySignature(int key) {
    keySignature = key;
    pitchMap = key >= 0 ? sharpMap : flatMap;
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
}

void NoteTakerDisplay::setUpAccidentals(NVGcontext* vg, BarPosition& bar, int& nextBar) {
    // draw notes
    // to do : could optimize this to skip notes except for bar prior to displayStart
    for (unsigned index = 0; index < nt->displayStart; ++index) {
        const DisplayNote& note = nt->allNotes[index];
        bar.next(note);
        while (nextBar <= note.startTime) {
            this->restartBar(bar, nextBar);
        }
        switch (note.type) {
            case NOTE_ON: {
                const StaffNote& pitch = pitchMap[note.pitch()];
                accidentals[pitch.position] = (Accidental) pitch.accidental;
            } break;
            case KEY_SIGNATURE:
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                (void) bar.setSignature(note);
                nextBar = note.startTime + bar.duration;
                break;
            default:
                ;
        }
    }
}

void NoteTakerDisplay::updateXPosition() {
    if (!cacheInvalid) {
        return;
    }
    cacheInvalid = false;
    cache.resize(nt->allNotes.size());
    BarPosition bar(*nt);
    float pos = 0;
    float posAdjust = 0;
    int posAdjustTime = 0;
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        cache[index].resetTupleBeam();
    }
    if (!(nt->ppq % 3)) {  // to do : only triplets for now
        this->cacheTuplets();
    }
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        const DisplayNote& note = nt->allNotes[index];
        if (posAdjust && note.startTime > posAdjustTime) {
            pos += posAdjust;
            posAdjust = 0;
        }
        bar.next(note);
        NoteCache& noteCache = cache[index];
        noteCache.xPosition = (int) ((note.stdStart(nt->ppq) + (bar.prior + bar.start) * BAR_WIDTH)
                * xAxisScale + pos);
        noteCache.vDuration = note.duration;
        if (PositionType::none != noteCache.tupletPosition) {
            noteCache.vDuration = noteCache.vDuration * 3 / 2;  // to do : just support triplets for now
        }
        noteCache.symbol = (uint8_t) NoteDurations::FromMidi(noteCache.vDuration, nt->ppq);
        if (NOTE_ON != note.type) {
            noteCache.yPosition = 0;
            noteCache.stemUp = false;
        }
        if (INT_MAX != bar.duration) {
            debug("%d [%d] bar start %d prior %d stdStart %d", index, cache[index].xPosition,
                    bar.start, bar.prior, note.stdStart(nt->ppq));
        }
        switch (note.type) {
            case MIDI_HEADER:
                assert(!pos);
                assert(!index);
                pos = CLEF_WIDTH * xAxisScale;
                break;
            case NOTE_ON: {
                const StaffNote& pitch = pitchMap[note.pitch()];
                noteCache.yPosition = this->yPos(pitch.position);
                noteCache.stemUp = this->stemUp(pitch.position);             
            }
                // fall through ...
            case REST_TYPE: {  // add space if note is dotted (any symbol mapping will do)
                int adjust = strlen(upFlagNoteSymbols[noteCache.symbol]) == 2 ? 19 : 15; 
                posAdjust = std::max(posAdjust, adjust - note.duration * xAxisScale);
                posAdjustTime = note.startTime;
                // to do : if multiple notes from different channels overlap, space them out
                // if note crossed bar, add space for tie
                int notesTied = bar.notesTied(note);
                if (notesTied > 1) {
                    int extraWidth = NOTE_WIDTH * (notesTied - 1);  // guess at how wide a note is
                    pos += std::max(0.f, (extraWidth - note.stdDuration(nt->ppq)) * xAxisScale);
                    debug("end %d trailer %d notesTied", bar.end, bar.trailer, notesTied);
                }
                } break;
            case KEY_SIGNATURE:
                if (note.startTime) {
                    pos += DOUBLE_BAR_WIDTH * xAxisScale;
                }
                pos += std::max(MIN_KEY_SIGNATURE_WIDTH, 
                        abs(note.key()) * ACCIDENTAL_WIDTH * xAxisScale + 2);
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                pos += TIME_SIGNATURE_WIDTH * xAxisScale;  // add space to draw time signature
                bar.setSignature(note);
                break;
            case TRACK_END:
                debug("[%u] xPos %d start %d", index, cache[index].xPosition, note.stdStart(nt->ppq));
                break;
            case MIDI_TEMPO:
                pos += TEMPO_WIDTH * xAxisScale;
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    this->cacheBeams();
    this->cacheSlurs();
    debug("updateXPosition size %u", cache.size());
}
