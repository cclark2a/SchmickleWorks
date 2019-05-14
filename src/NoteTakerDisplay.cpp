#include "NoteTakerDisplay.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"

static void draw_stem(NVGcontext*vg, float x, int ys, int ye) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + .4, ys);
    nvgLineTo(vg, x + .4, ye);
    nvgStrokeWidth(vg, .625);
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

void BarPosition::addPos(const NoteCache& noteCache, float cacheWidth) {
    if (INT_MAX == duration) {
        return;
    }
    int barCount = (noteCache.vStartTime - tsStart) / duration + priorBars;
    if (barCount) {
        pos[barCount].xMin = std::min(pos[barCount].xMin, (float) noteCache.xPosition);
        if (false) debug("addPos [%d] xMin %g xPos %d", barCount, pos[barCount].xMin, noteCache.xPosition);
    }
    // note the adds duration less one to round up
    barCount = (noteCache.vEndTime() - tsStart + duration - 1) / duration + priorBars;  // rounds up
    if (barCount) {
        pos[barCount].xMax = std::max(pos[barCount].xMax, noteCache.xPosition + cacheWidth);
        if (false) debug("addPos [%d] xMax %g xPos %d width %g", barCount, pos[barCount].xMax,
                noteCache.xPosition, cacheWidth);
    }
}

void BarPosition::init() {
    pos.clear();
    priorBars = 0;
    duration = INT_MAX;
    midiEnd = INT_MAX;
    tsStart = INT_MAX;
}

int BarPosition::notesTied(const DisplayNote& note, int ppq) {
    if (INT_MAX == duration) {
        leader = note.duration;
        return NoteTakerDisplay::TiedCount(duration, note.duration, ppq);
    }
    int inTsStartTime = note.startTime - tsStart;
    int startBar = inTsStartTime / duration;
    int inTsEndTime = note.endTime() - tsStart;  // end time relative to time signature start
    int endBar = (inTsEndTime - 1) / duration;
    if (startBar == endBar) {
        return 1;
    }
    leader = (startBar + 1) * duration - note.startTime;
    int trailer = inTsEndTime - endBar * duration;
    assert(0 <= trailer);
    if (trailer >= duration) {
        debug("notesTied trailer %d duration %d inTsEndTime %d endBar %d inTsStartTime %d"
                " startBar %de leader %d",
                trailer, duration, inTsEndTime, endBar, inTsStartTime, startBar, leader);
        debug("note %s", note.debugString().c_str());
    }
    assert(trailer < duration);
    int result = NoteTakerDisplay::TiedCount(duration, leader, ppq);
    result += endBar - startBar - 1;  // # of bars with whole notes
    if (trailer) {
        result += NoteTakerDisplay::TiedCount(duration, trailer, ppq);
    }
    return result;
}

// for x position: figure # of bars added by previous signature
int BarPosition::resetSignatureStart(const DisplayNote& note, float barWidth) {
    int result = 0;
    if (INT_MAX != duration) {
        result = (note.startTime - tsStart + duration - 1)  // round up
                / duration * barWidth;
    } else if (tsStart < note.startTime) {
        result = barWidth;
    }
    tsStart = note.startTime;
    return result;
}

const int CLEF_WIDTH = 96;
const int TIME_SIGNATURE_WIDTH = 48;
const float MIN_KEY_SIGNATURE_WIDTH = 4;
const int TEMPO_WIDTH = 4;
// key signature width is variable, from 0 (C) to ACCIDENTAL_WIDTH*14+18
// (C# to Cb, 7 naturals, 7 flats, space for 2 bars)
const int ACCIDENTAL_WIDTH = 12;
const int BAR_WIDTH = 12;
const int NOTE_FONT_SIZE = 42;

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

struct DisplayWidget : Widget {
	void draw(const DrawArgs& args) override {
        auto display = this->getAncestorOfType<NoteTakerDisplay>();
        display->draw(args);
    }
};

NoteTakerDisplay::NoteTakerDisplay(const Vec& pos, const Vec& size, std::string mfnt,
        std::string tfnt)
    : musicFontName(mfnt)
    , textFontName(tfnt) 
    , pitchMap(sharpMap) {
    fb = new widget::FramebufferWidget();
    fb->box.pos = Vec();
    fb->box.size = Vec(RACK_GRID_WIDTH * 14, RACK_GRID_WIDTH * 12);
    fb->dirty = true;
    this->addChild(fb);
    DisplayWidget* display = new DisplayWidget;
    fb->addChild(display);
    box.pos = pos;
    box.size = size;
    assert(sizeof(upFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
    assert(sizeof(downFlagNoteSymbols) / sizeof(char*) == NoteDurations::Count());
}

void NoteTakerDisplay::advanceBar(unsigned index) {
    if (INT_MAX == bar.duration) {
        return;
    }
    const NoteCache& noteCache = cache[index];
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

void NoteTakerDisplay::cacheBeams() {
    auto nt = NT(this);
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    for (unsigned cacheIndex = 0; cacheIndex < cache.size(); ++cacheIndex) {
        NoteCache& noteCache = cache[cacheIndex];
        if (NOTE_ON != noteCache.note->type) {
            for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
                if (255 != noteCache.channel && chan != noteCache.channel) {
                    continue;
                }
                unsigned& beamStart = beamStarts[chan];
                if (INT_MAX != beamStart) {
                    this->closeBeam(beamStart, cacheIndex);
                    beamStart = INT_MAX;
                }
            }
            continue;
        }
        unsigned& beamStart = beamStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != beamStart) {
            bool stemsMatch = cache[beamStart].stemUp == noteCache.stemUp;
            if (!stemsMatch || noteCache.vDuration >= nt->ppq) {
                this->closeBeam(beamStart, cacheIndex);
                beamStart = INT_MAX;
            } else {
                if (!noteCache.endsOnBeat || PositionType::right == noteCache.tupletPosition) {
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
            beamStart = cacheIndex;
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
            debug("beam unmatched %s", cache[beamStart].note->debugString().c_str());
        }
    }
}

void NoteTakerDisplay::cacheSlurs() {
    array<unsigned, CHANNEL_COUNT> slurStarts;
    slurStarts.fill(INT_MAX);
    for (unsigned cacheIndex = 0; cacheIndex < cache.size(); ++cacheIndex) {
        NoteCache& noteCache = cache[cacheIndex];
        const DisplayNote* note = noteCache.note;
        if (NOTE_ON != note->type) {
            continue;
        }
        unsigned& slurStart = slurStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != slurStart) {
            if (!note->slur()) {
                this->closeSlur(slurStart, cacheIndex);
                debug("set slur close %s", cache[slurStart].note->debugString().c_str());
                slurStart = INT_MAX;
            } else {
                noteCache.slurPosition = PositionType::mid;
                checkForStart = false;
                debug("set slur mid %s", note->debugString().c_str());
            }
        }
        if (checkForStart && note->slur()) {
            slurStart = cacheIndex;
            if (PositionType::none == noteCache.slurPosition) {
                noteCache.slurPosition = PositionType::left;
            }
            debug("set slur left %s", note->debugString().c_str());
        }
    }
    for (auto slurStart : slurStarts) {
        if (INT_MAX != slurStart) {
            this->closeSlur(slurStart, cache.size());
            debug("slur unmatched %s", cache[slurStart].note->debugString().c_str());
        }
    }
}

unsigned NoteTakerDisplay::cacheNext(unsigned index) {
    const DisplayNote* base = cache[index].note;
    while (base == cache[++index].note) {
        ;
    }
    return index;
}

unsigned NoteTakerDisplay::cachePrevious(unsigned index) {
    const DisplayNote* base = cache[--index].note;
    while (index && base == cache[index - 1].note) {
        --index;
    }
    return index;
}

// only mark triplets if notes are the right duration and collectively a multiple of three
// to do : allow rests to be part of triplet
void NoteTakerDisplay::cacheTuplets() {
    auto nt = NT(this);
    array<unsigned, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(INT_MAX);
    for (unsigned index = 0; index < nt->notes.size(); ++index) {
        const DisplayNote& note = nt->notes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        unsigned& tripStart = tripStarts[note.channel];
        if (INT_MAX != tripStart) {
            const DisplayNote& start = nt->notes[tripStart];
            int totalDuration = note.endTime() - start.startTime;
            if (totalDuration > nt->ppq * 4) {
                // to do : use time signature to limit triplet to bar 
                // for now, limit triplet to whole note
                tripStart = INT_MAX;
            } else {
                int tripDur = start.duration * 3;
                int factor = totalDuration / tripDur;
                // to do : take advantage of start time % 3 to note that remainder is 0/1/2
                //         if the note is part of the 1st/2nd/3rd triplet
                if (totalDuration * 2 == tripDur || factor * tripDur == totalDuration) {
                    NoteCache* noteCache = start.cache;
                    noteCache->tupletPosition = PositionType::left;
                    int chan = start.channel;
                    NoteCache* last = note.cache;
                    NoteCache* next = last;
                    while (++next < &cache.back()) {
                        if (chan != next->channel) {
                            continue;
                        }
                        if (next->note != &note) {
                            break;
                        }
                        last = next;
                    }
                    while (++noteCache < last) {
                        if (chan == noteCache->channel) {
                            noteCache->tupletPosition = PositionType::mid;
                        }
                    }
                    last->tupletPosition = PositionType::right;
                    tripStart = INT_MAX;
                }
                continue;
            }
        }
        if (NoteDurations::TripletPart(note.duration, nt->ppq)) {
            tripStart = index;
        }
    }
}

float NoteTakerDisplay::cacheWidth(const NoteCache& noteCache) const {
    float bounds[4];
    auto& symbols = REST_TYPE == noteCache.note->type ? restSymbols :
            PositionType::none != noteCache.beamPosition ?
            noteCache.stemUp ? upBeamNoteSymbols : downBeamNoteSymbols :
            noteCache.stemUp ? upFlagNoteSymbols : downFlagNoteSymbols;
    nvgTextBounds(vg, 0, 0, symbols[noteCache.symbol], nullptr, bounds);
    return bounds[2];
}

void NoteTakerDisplay::clearTuplet(unsigned index, unsigned limit) {
    auto nt = NT(this);
    assert(PositionType::left == cache[index].tupletPosition);
    int chan = nt->notes[index].channel;
    cache[index].tupletPosition = PositionType::none;
    while (++index < limit) {
        if (chan == nt->notes[index].channel) {
            assert(PositionType::mid == cache[index].tupletPosition);
            cache[index].tupletPosition = PositionType::none;
        }
    }
}

void NoteTakerDisplay::closeBeam(unsigned first, unsigned limit) {
    assert(PositionType::left == cache[first].beamPosition);
    int chan = cache[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (chan == cache[index].channel) {
            if (PositionType::mid != cache[index].beamPosition) {
                break;
            }
            last = index;
        }
    }
    cache[last].beamPosition = last == first ? PositionType::none : PositionType::right;
    debug("close beam first %d last %d pos %u", first, last, cache[last].beamPosition);
}

void NoteTakerDisplay::closeSlur(unsigned first, unsigned limit) {
    assert(PositionType::left == cache[first].slurPosition);
    int chan = cache[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (chan == cache[index].channel) {
            if (PositionType::mid != cache[index].slurPosition) {
                break;
            }
            last = index;
        }
    }
    cache[last].slurPosition = last == first ? PositionType::none : PositionType::right;
    debug("close slur first %d last %d pos %u", first, last, cache[last].slurPosition);
}

void NoteTakerDisplay::drawBarAt(int xPos) {
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
    bar.addPos(noteCache, this->cacheWidth(noteCache));
}

// to do : whole rest should be centered in measure
void NoteTakerDisplay::drawBarRest(BarPosition& bar, const NoteCache& noteCache,
        int xPos, unsigned char alpha) const {
    // to do : if rest is part of triplet, adjust duration before lookup
    const float yPos = 36 * 3 - 49;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, musicFont->handle);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgText(vg, xPos, yPos, restSymbols[noteCache.symbol], nullptr);
}

// if note crosses bar, should have recorded bar position when drawn as tied notes
void NoteTakerDisplay::drawBars() {
    for (const auto& p : bar.pos) {
        if (!p.second.useMax && p.second.xMax > p.second.xMin) {
            continue;
        }
        this->drawBarAt(p.second.useMax ? p.second.xMax : (p.second.xMin + p.second.xMax) / 2);
        if (false) debug("[%d] drawBars min %g max %g useMax %d", p.first, p.second.xMin, p.second.xMax,
                p.second.useMax);
    }
}

void NoteTakerDisplay::drawBeam(unsigned start, unsigned char alpha) const {
    assert(PositionType::left == cache[start].beamPosition);
    BeamPositions bp;
    unsigned index = start;
    int chan = cache[start].channel;
    while (++index < cache.size()) {
        if (chan != cache[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].beamPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
        assert(PositionType::mid == cache[index].beamPosition);
    }
    assert(PositionType::right == cache[index].beamPosition);
    SetNoteColor(vg, chan, alpha);
    bp.ex += 0.25;
    for (unsigned count = 0; count < bp.beamMin; ++count) {
        bp.drawOneBeam(vg);
    }
    index = start;
    int yReset = bp.y;
    const NoteCache* prev = nullptr;
    const NoteCache* noteCache = nullptr;
    do {
        assert(chan == cache[index].channel);
        prev = noteCache;
        noteCache = &cache[index];
        while (++index < cache.size() && chan != cache[index].channel)
            ;
        if (index == cache.size()) {
            debug("drawBeam found unbalanced beam");
            assert(0);
            return;
        }
        const NoteCache& next = cache[index];
        bp.sx = noteCache->xPosition + bp.xOffset;
        draw_stem(vg, bp.sx, noteCache->yPosition + bp.yStemExtend, bp.yLimit);
        bp.sx += 0.25;
        bp.ex = next.xPosition + bp.xOffset + 0.25;
        bp.y = yReset;
        bool fullBeam = true;
        for (unsigned count = bp.beamMin; count < noteCache->beamCount; ++count) {
            if (fullBeam && next.beamCount <= count) {
                if (prev && prev->beamCount >= count) {
                    continue;
                }
                bp.ex = bp.sx + 6;   // draw partial beam on left
                fullBeam = false;
            } 
            bp.drawOneBeam(vg);
        }
        if (PositionType::right == next.beamPosition) { // draw partial beam on right
            bp.ex = next.xPosition + bp.xOffset;
            draw_stem(vg, bp.ex, next.yPosition + bp.yStemExtend, bp.yLimit);
            bp.ex += 0.25;
            bp.sx = bp.ex - 6;
            for (unsigned count = noteCache->beamCount; count < next.beamCount; ++count) {
                bp.drawOneBeam(vg);
            }
            break;
        }
    } while (true);
}

void NoteTakerDisplay::drawClefs() const {
         // draw treble and bass clefs
    nvgFontFaceId(vg, musicFont->handle);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgText(vg, 5, 60, "(", NULL);
    nvgFontSize(vg, 36);
    nvgText(vg, 5, 92, ")", NULL);
}

void NoteTakerDisplay::drawFreeNote(const DisplayNote& note, NoteCache* noteCache,
        int xPos, unsigned char alpha) const {
    auto nt = NT(this);
    const StaffNote& pitch = pitchMap[note.pitch()];
    noteCache->note = &note;
    noteCache->resetTupleBeam();
    noteCache->xPosition = xPos;
    noteCache->yPosition = this->yPos(pitch.position);
    noteCache->setDuration(nt->ppq);
    noteCache->stemUp = true;
    drawNote((Accidental) pitchMap[note.pitch()].accidental, *noteCache, alpha, 24);
}
            
void NoteTakerDisplay::drawKeySignature(unsigned index) {
    auto nt = NT(this);
    int xPos = cache[index].xPosition;
    auto& note = *cache[index].note;
    this->setKeySignature(note.key());
    const char* mark;
    unsigned keySig;
    const uint8_t* accKeys, * bassKeys;
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
    nvgFontFaceId(vg, musicFont->handle);
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
        if (dynamicSelectAlpha > 0) {
            dynamicSelectAlpha = (int) (255 * (dynamicSelectTimer - nt->realSeconds) / fadeDuration);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, dynamicSelectAlpha));
            nvgText(vg, xPos, 32 * 3 - 48, "%", NULL);
            nvgText(vg, xPos, 46 * 3 - 48, "%", NULL);
        }
        return;
    }
    for (auto keys : { accKeys, bassKeys }) {
        int xKeyPos = xPos;
        for (unsigned index = 0; index < keySig; ++index) {
            nvgText(vg, xKeyPos, keys[index] * 3 - 48, mark, NULL);
            xKeyPos += ACCIDENTAL_WIDTH * xAxisScale;
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
    nvgFontFaceId(vg, musicFont->handle);
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

void NoteTakerDisplay::drawNotes() {
    auto ntw = NTW(this);
    auto nt = NT(this);
    for (unsigned index = displayStart; index < displayEnd; ++index) {
        const NoteCache& noteCache = cache[index];
        const DisplayNote& note = *noteCache.note;
        this->advanceBar(index);
        switch (note.type) {
            case NOTE_ON: {
                unsigned char alpha = ntw->isSelectable(note) ? 0xff : 0x7f;
                this->drawBarNote(bar, note, noteCache, alpha);
                bool drawBeams = PositionType::left == noteCache.beamPosition;
                if (drawBeams) {
                    this->drawBeam(index, alpha);
                }
                if (PositionType::left == noteCache.tupletPosition) {
                    this->drawTuple(index, alpha, drawBeams);
                }
                if (PositionType::left == noteCache.slurPosition) {
                    this->drawSlur(index, alpha);
                }
                if (PositionType::left == noteCache.tiePosition
                        || PositionType::mid == noteCache.tiePosition) {
                    this->drawTie(index, alpha);
                }
            } break;
            case REST_TYPE:
                this->drawBarRest(bar, noteCache, noteCache.xPosition,
                        ntw->isSelectable(note) ? 0xff : 0x7f);
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
                bar.setSignature(note, nt->ppq);
                bar.setMidiEnd(noteCache);
                // note : separate multiplies avoids rounding error
                // to do : use nvgTextAlign() and nvgTextBounds() instead of hard-coding
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 0xFF));
                std::string numerator = std::to_string(note.numerator());
                int xPos = noteCache.xPosition - 4 * (numerator.size() - 1) + 3;
                nvgFontFaceId(vg, musicFont->handle);
                nvgFontSize(vg, NOTE_FONT_SIZE);
                nvgText(vg, xPos, 32 * 3 - 49, numerator.c_str(), NULL);
                nvgText(vg, xPos, 44 * 3 - 49, numerator.c_str(), NULL);
                std::string denominator = std::to_string(1 << note.denominator());
                xPos = noteCache.xPosition - 4 * (denominator.size() - 1) + 3;
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
                debug("to do: add type %d\n", note.type);
                assert(0); // incomplete
        }
    }
}

void NoteTakerDisplay::drawSelectionRect() const {
    // draw selection rect
    auto ntw = NTW(this);
    auto nt = NT(this);
    unsigned start = nt->selectEndPos(nt->selectStart);
    auto& startNote = nt->notes[start];
    int xStart = startNote.cache->xPosition - 10;
    int width = 4;
    int yTop = 0;
    int yHeight = box.size.y;
    auto selectButton = ntw->widget<SelectButton>();
    unsigned channel = selectButton->editStart() ? 1 << ntw->unlockedChannel() :
            ntw->selectChannels;
    const DisplayNote* note = nullptr;
    if (nt->selectStart + 1 == nt->selectEnd) {
        note = &nt->notes[nt->selectStart];
        if (!note->isSignature()) {
            note = nullptr;
        }
    }
    if (note && MIDI_TEMPO == note->type) {
        nvgBeginPath(vg);
        nvgRect(vg, xStart + width, yTop, 30 - width, 12);
        SetSelectColor(vg, channel);
        nvgFill(vg);
    }
    if (!selectButton->editStart()) {
        if (ntw->widget<FileButton>()->ledOn) {
            yHeight = box.size.y - 35;
        }
        if (note && TIME_SIGNATURE == note->type && !selectButton->ledOn) {
            yTop = (1 - (int) ntw->widget<VerticalWheel>()->getValue()) * 12 + 35;
            yHeight = 13;
        }
        if (nt->selectEnd > 0) {
            auto& selStartNote = nt->notes[nt->selectStart];
            xStart = selStartNote.cache->xPosition - 8;
            unsigned selEndPos = nt->selectEndPos(nt->selectEnd - 1);
            auto& selEndNote = nt->notes[selEndPos];
            width = selEndNote.cache->xPosition - xStart;
        }
    }
    nvgBeginPath(vg);
    nvgRect(vg, xStart, yTop, width, yHeight);
    SetSelectColor(vg, channel);
    nvgFill(vg);
}

void NoteTakerDisplay::drawStaffLines() const {
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

void NoteTakerDisplay::draw(const DrawArgs& args) {
    auto ntw = NTW(this);
    auto nt = NT(this);
    vg = args.vg;
    musicFont = APP->window->loadFont(musicFontName);
    textFont = APP->window->loadFont(textFontName);
    if (cacheInvalid) {
        this->updateXPosition();
        cacheInvalid = false;
    }
    if (rangeInvalid) {
        assert(nt->notes.front().cache == &cache.front());
        assert(cache.front().note == &nt->notes.front());
        this->updateRange();
    }
#if RUN_UNIT_TEST
    if (ntw->runUnitTest) { // to do : remove this from shipping code
        UnitTest(ntw, TestType::digit);
        ntw->runUnitTest = false;
        return;
    }
#endif
    if (!nt->notes.size()) {
        return;  // do nothing if we're not set up yet
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
    this->scroll();
    this->drawClefs();
    this->drawSelectionRect();
    bar.init();
    this->setUpAccidentals();
    this->drawNotes();
    this->drawBars();
    nvgRestore(vg);
    this->recenterVerticalWheel();
    if (ntw->widget<FileButton>()->ledOn) {
        this->drawFileControl();
    }
    if (ntw->widget<PartButton>()->ledOn) {
        this->drawPartControl();
    }
    if (ntw->widget<SustainButton>()->ledOn) {
        this->drawSustainControl();
    }
    if (ntw->widget<TieButton>()->ledOn) {
        this->drawTieControl();
    }
    if (ntw->runningWithButtonsOff()) {
        this->drawDynamicPitchTempo();
    }
    nvgRestore(vg);
    Widget::draw(args);
}

void NoteTakerDisplay::drawDynamicPitchTempo() {
    auto ntw = NTW(this);
    auto nt = NT(this);
    if (!ntw->widget<FileButton>()->ledOn) {
        if (ntw->widget<VerticalWheel>()->hasChanged()) {
            dynamicPitchTimer = nt->realSeconds + fadeDuration;
        }
        auto horizontalWheel = ntw->widget<HorizontalWheel>();
        if (!leadingTempo && horizontalWheel->lastRealValue != horizontalWheel->getValue()) {
            dynamicTempoTimer = nt->realSeconds + fadeDuration;
            horizontalWheel->lastRealValue = horizontalWheel->getValue();
        }
        dynamicTempoAlpha = (int) (255 * (dynamicTempoTimer - nt->realSeconds) / fadeDuration);
        dynamicPitchAlpha = (int) (255 * (dynamicPitchTimer - nt->realSeconds) / fadeDuration);
    }
    if (dynamicPitchAlpha > 0) {
        NoteCache noteCache;
        DisplayNote note = {&noteCache, 0, nt->ppq, { 0, 0, 0, 0}, 0, NOTE_ON, false };
        note.setPitch((int) ntw->widget<VerticalWheel>()->getValue());
        nvgBeginPath(vg);
        nvgRect(vg, box.size.x - 10, 2, 10, box.size.y - 4);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, dynamicPitchAlpha));
        nvgFill(vg);
        this->drawFreeNote(note, &noteCache, box.size.x - 7, dynamicPitchAlpha);
        fb->dirty = true;
    }
    if (dynamicTempoAlpha > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, 2, 2, 30, 12);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, dynamicTempoAlpha));
        nvgFill(vg);
        this->drawTempo(2, stdMSecsPerQuarterNote, dynamicTempoAlpha);
        fb->dirty = true;
    }
}

void NoteTakerDisplay::drawFileControl() {
    auto ntw = NTW(this);
    auto horizontalWheel = ntw->widget<HorizontalWheel>();
    bool loadEnabled = (unsigned) horizontalWheel->getValue() < ntw->storage.size();
    this->drawVerticalLabel("load", loadEnabled, upSelected, 0);
    this->drawVerticalLabel("save", true, downSelected, box.size.y - 50);
    this->drawVerticalControl();
    // draw horizontal control
    const float boxWidth = 25;
    float fSlot = horizontalWheel->getValue();
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
        nvgFontFaceId(vg, textFont->handle);
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
        nvgFontFaceId(vg, musicFont->handle);
        nvgFontSize(vg, 16);
        nvgTextAlign(vg, NVG_ALIGN_CENTER);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgText(vg, boxWidth / 2, boxWidth - 3, "H", NULL);
    };
    // horizontalWheel->value auto-drifts towards integer, range of 0 to nt->storage.size()
    fb->dirty |= fSlot != slot;
    if (fSlot < slot) {
        horizontalWheel->setValue(std::min((float) slot, fSlot + .02f));
    } else if (fSlot > slot) {
        horizontalWheel->setValue(std::max((float) slot, fSlot - .02f));
    }
    // xControlOffset draws current location, 0 to nt->storage.size() - 4
    if (horizontalWheel->getValue() - xControlOffset > 3) {
        xControlOffset += .04f;
    } else if (horizontalWheel->getValue() < xControlOffset) {
        xControlOffset -= .04f;
    }
    if (xControlOffset != floorf(xControlOffset)) {
        if (xControlOffset > 0 && (horizontalWheel->getValue() - xControlOffset < 1
                || (xControlOffset - floorf(xControlOffset) < .5
                && horizontalWheel->getValue() - xControlOffset < 3))) {
            xControlOffset = std::max(floorf(xControlOffset), xControlOffset - .04f);
        } else {
            xControlOffset = std::min(ceilf(xControlOffset), xControlOffset + .04f);
        }
        fb->dirty = true;
    }
    const int first = std::max(0, (int) (xControlOffset - 1));
    const int last = std::min((int) ntw->storage.size(), (int) (xControlOffset + 5));
    nvgSave(vg);
    nvgScissor(vg, 40 - boxWidth / 2, box.size.y - boxWidth - 5, boxWidth * 5, boxWidth);
    for (int index = first; index < last; ++index) {
        nvgSave(vg);
        nvgTranslate(vg, 40 + (index - xControlOffset) * boxWidth, box.size.y - boxWidth - 5);
        if (ntw->storage[index].empty()) {
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
        } else if (last - 1 == index && xControlOffset + 4.5 < (float) ntw->storage.size()) {
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
    unsigned index = ntw->storage.size();
    if (index * boxWidth <= box.size.x) {
        drawEmpty(index);
    }
    float wheel = horizontalWheel->getValue();
    nvgBeginPath(vg);
    nvgRect(vg, 40 + (wheel - xControlOffset) * boxWidth, box.size.y - boxWidth - 5,
            boxWidth, boxWidth);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

void NoteTakerDisplay::drawPartControl() const {
    auto ntw = NTW(this);
    auto horizontalWheel = ntw->widget<HorizontalWheel>();
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
    nvgBeginPath(vg);
//    nvgRect(vg, 40, box.size.y - boxHeight - 25, boxWidth * 4, 10);
//    nvgRect(vg, 40, box.size.y - 15, boxWidth * 4, 10);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7f));
    nvgFill(vg);
    nvgFontFaceId(vg, textFont->handle);
    nvgTextAlign(vg, NVG_ALIGN_CENTER);
    for (int index = -1; index < (int) CV_OUTPUTS; ++index) {
        nvgBeginPath(vg);
        nvgRect(vg, 60 + index * boxWidth, box.size.y - boxHeight - 15,
                boxWidth, boxHeight);
        bool chanLocked = ~ntw->selectChannels & (1 << index);
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

// to do : share code with draw beam ?
// to do : move slur so it doesn't draw on top of tie
// to do : mock score software ?
// in scoring software, slur is drawn from first to last, offset by middle, if needed
void NoteTakerDisplay::drawSlur(unsigned start, unsigned char alpha) const {
    assert(PositionType::left == cache[start].slurPosition);
    BeamPositions bp;
    unsigned index = start;
    int chan = cache[start].channel;
    while (++index < cache.size()) {
        if (chan != cache[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].slurPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
        assert(PositionType::mid == cache[index].slurPosition);
    }
    assert(PositionType::right == cache[index].slurPosition);
    SetNoteColor(vg, chan, alpha);
    this->drawArc(bp, start, index);
}

void NoteTakerDisplay::drawTie(unsigned start, unsigned char alpha) const {
    assert(PositionType::left == cache[start].tiePosition
            || PositionType::mid == cache[start].tiePosition);
    BeamPositions bp;
    unsigned index = start;
    int chan = cache[start].channel;
    while (++index < cache.size() && chan != cache[index].channel)
        ;
    this->setBeamPos(start, index, &bp);
    if (PositionType::right != cache[index].tiePosition
            && PositionType::mid != cache[index].tiePosition) {
                debug("** missing tie end %s", cache[start].note->debugString().c_str());
            }
    SetNoteColor(vg, chan, alpha);
    this->drawArc(bp, start, index);
}

void NoteTakerDisplay::drawArc(const BeamPositions& bp, unsigned start, unsigned index) const {
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
    nvgMoveTo(vg, bp.sx - 2, yStart + yOff);
    nvgQuadTo(vg, xMid - dyxY,     yMid + yOff + dyxX,     bp.ex - 2, yEnd + yOff);
    nvgQuadTo(vg, xMid - dyxY * 2, yMid + yOff + dyxX * 2, bp.sx - 2, yStart + yOff);
    nvgFill(vg);
}

void NoteTakerDisplay::drawSustainControl() const {
    // draw vertical control
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 35, 25, 35, box.size.y - 30);
    nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x7f));
    nvgFill(vg);
    auto ntw = NTW(this);
    int select = (int) ntw->widget<VerticalWheel>()->getValue();
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
    this->drawVerticalControl();
    // draw horizontal control
    nvgBeginPath(vg);
    auto nt = NT(this);
    const NoteTakerChannel& channel = nt->channels[ntw->unlockedChannel()];
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
    nvgFontFaceId(vg, musicFont->handle);
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

void NoteTakerDisplay::drawTempo(int xPos, int tempo, unsigned char alpha) {
    nvgFillColor(vg, nvgRGBA(0, 0, 0, alpha));
    nvgFontFaceId(vg, musicFont->handle);
    nvgFontSize(vg, 16);
    nvgText(vg, xPos + 3, 10, "H", nullptr);
    nvgFontFaceId(vg, textFont->handle);
    nvgFontSize(vg, 12);
    auto nt = NT(this);
    std::string tempoStr("= " + std::to_string((int) (120.f * nt->beatsPerHalfSecond(tempo))));
    nvgText(vg, xPos + 7, 10, tempoStr.c_str(), nullptr);
}

// to do : maybe objectify this and share common code for display controls
void NoteTakerDisplay::drawTieControl() {
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
        nvgFontFaceId(vg, musicFont->handle);
        nvgFontSize(vg, 16);
        nvgTextAlign(vg, NVG_ALIGN_CENTER);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x7F));
        nvgText(vg, boxWidth / 2, boxWidth - 3, &ch, &ch + 1);
        nvgTranslate(vg, boxSpace, 0);
    }
    nvgRestore(vg);
    auto ntw = NTW(this);
    auto horizontalWheel = ntw->widget<HorizontalWheel>();
    float fSlot = horizontalWheel->getValue();
    int slot = (int) (fSlot + .5);
    if (fSlot < slot) {
        horizontalWheel->setValue(std::min((float) slot, fSlot + .02f));
        fb->dirty = true;
    } else if (fSlot > slot) {
        horizontalWheel->setValue(std::max((float) slot, fSlot - .02f));
        fb->dirty = true;
    }
    nvgBeginPath(vg);
    nvgRect(vg, leftEdge + (horizontalWheel->getValue()) * boxSpace,
            box.size.y - boxWidth - 5, boxWidth, boxWidth);
    nvgStrokeWidth(vg, 3);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
}

// to do : share code with draw slur, draw beam ?
void NoteTakerDisplay::drawTuple(unsigned start, unsigned char alpha, bool drewBeam) const {
    assert(PositionType::left == cache[start].tupletPosition);
    BeamPositions bp;
    unsigned index = start;
    int chan = cache[start].channel;
    while (++index < cache.size()) {
        if (chan != cache[index].channel) {
            continue;
        }
        if (PositionType::right == cache[index].tupletPosition) {
            this->setBeamPos(start, index, &bp);
            break;
        }
        assert(PositionType::mid == cache[index].tupletPosition);
    }
    assert(PositionType::right == cache[index].tupletPosition);
    SetNoteColor(vg, chan, alpha);
    // draw '3' at center of notes above or below staff
    nvgFontFaceId(vg, musicFont->handle);
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

void NoteTakerDisplay::drawVerticalControl() const {
    nvgBeginPath(vg);
    nvgRect(vg, box.size.x - 10, 20, 5, box.size.y - 40);
    nvgStrokeWidth(vg, 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x3F));
    nvgStroke(vg);
    nvgBeginPath(vg);
    auto ntw = NTW(this);
    auto verticalWheel = ntw->widget<VerticalWheel>();
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
    nvgFontFaceId(vg, textFont->handle);
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

void NoteTakerDisplay::recenterVerticalWheel() {
    if (upSelected || downSelected) {
        auto ntw = NTW(this);
        auto verticalWheel = ntw->widget<VerticalWheel>();
        float val = verticalWheel->getValue();
        const float autoLoadSaveWheelSpeed = .2f;
        verticalWheel->setValue(verticalWheel->getValue()
                + (val > 5 ? -autoLoadSaveWheelSpeed : +autoLoadSaveWheelSpeed));
        if (5 - autoLoadSaveWheelSpeed < val && val < 5 + autoLoadSaveWheelSpeed) {
            upSelected = false;
            downSelected = false;
        } else {
            fb->dirty = true;
        }
    }
}

void NoteTakerDisplay::scroll() {
    nvgTranslate(vg, -xAxisOffset - dynamicXOffsetTimer, 0);
    if (!dynamicXOffsetStep) {
        return;
    }
    dynamicXOffsetTimer += dynamicXOffsetStep;
    if ((dynamicXOffsetTimer < 0) == (dynamicXOffsetStep < 0)) {
        dynamicXOffsetTimer = dynamicXOffsetStep = 0;
    }
    fb->dirty = true;
}

// used by beams and tuplets
void NoteTakerDisplay::setBeamPos(unsigned first, unsigned last, BeamPositions* bp) const {
    unsigned index = first;
    int chan = cache[first].channel;
    float yMax = 0;
    float yMin = FLT_MAX;
    unsigned beamMax = 0;
    unsigned lastMeasured = first;
    do {
        const NoteCache& noteCache = cache[index];
        if (chan != noteCache.channel) {
            continue;
        }
        yMax = std::max(noteCache.yPosition, yMax);
        yMin = std::min(noteCache.yPosition, yMin);
        beamMax = std::max((unsigned) noteCache.beamCount, beamMax);
        bp->beamMin = std::min((unsigned) noteCache.beamCount, bp->beamMin);
        lastMeasured = index;
    } while (++index <= last);
    bool stemUp = cache[first].stemUp;
    bp->xOffset = stemUp ? 6 : -0.25;
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

// compute ties first, so we know how many notes to draw
void NoteTakerDisplay::setCacheDuration() {
    bool sort = false;
    auto nt = NT(this);
    for (unsigned index = 0; index < nt->notes.size(); ++index) {
        DisplayNote& note = nt->notes[index];
        cache.emplace_back();
        NoteCache& cacheEntry = cache.back();
        cacheEntry.note = &note;
        cacheEntry.vStartTime = note.startTime;
        cacheEntry.channel = note.channel;
        cacheEntry.tiePosition = PositionType::none;
        if (!note.isNoteOrRest()) {
            cacheEntry.vDuration = 0;
            cacheEntry.endsOnBeat = false;
            cacheEntry.accidentalSpace = false;
            if (TIME_SIGNATURE == note.type) {
                bar.setSignature(note, nt->ppq);
            } else if (note.isSignature()) {
                bar.tsStart = note.startTime;
            }
            continue;
        }
        int notesTied = bar.notesTied(note, nt->ppq);
        if (1 == notesTied) {
            cacheEntry.vDuration = note.duration;
            cacheEntry.endsOnBeat = (bool) (note.endTime() % nt->ppq);
            cacheEntry.accidentalSpace = true;
        } else {
            sort = true;
            int duration = bar.leader;
            int remaining = note.duration - bar.leader;
            int tieTime = note.startTime;
            bool accidentalSpace = true;
            cache.pop_back();
            do {
                do {
                    debug("add tie to %s", note.debugString().c_str());
                    cache.emplace_back();
                    NoteCache& tiePart = cache.back();
                    tiePart.note = &note;
                    tiePart.vStartTime = tieTime;
                    tiePart.channel = note.channel;
                    tiePart.tiePosition = accidentalSpace ? PositionType::left : PositionType::mid;
                    tiePart.vDuration = NoteDurations::Closest(duration, nt->ppq);
                    tieTime += tiePart.vDuration;
                    tiePart.endsOnBeat = (bool) (tieTime % nt->ppq);
                    tiePart.accidentalSpace = accidentalSpace;
                    accidentalSpace = false;
                    duration -= tiePart.vDuration;
                } while (duration >= NoteDurations::ToMidi(0, nt->ppq));
                duration = std::min(bar.duration, remaining);
                if (!duration) {
                    cache.back().tiePosition = PositionType::right;
                    break;
                }
                remaining -= duration;
            } while (true);
        }
    }
    // don't use std::sort function; use insertion sort to minimize reordering
    if (sort) {
        for (auto it = cache.begin(), end = cache.end(); it != end; ++it) {
            auto const insertion_point = std::upper_bound(cache.begin(), it, *it);
            std::rotate(insertion_point, it, it + 1);
        }
    }
    cache.shrink_to_fit();
    // sort, shrink, vector push back may move cache locations, so set them up last
    DisplayNote* last = nullptr;
    for (auto& entry : cache) {
        if (last == entry.note) {
            continue;
        }
        last = const_cast<DisplayNote*>(entry.note);
        last->cache = &entry;
    }
    // check to see if things moved
    assert(nt->notes.front().cache == &cache.front());
    assert(cache.front().note == &nt->notes.front());
    debug("finished setCacheDuration");
}

void NoteTakerDisplay::setKeySignature(int key) {
    keySignature = key;
    pitchMap = key >= 0 ? sharpMap : flatMap;
    accidentals.fill(NO_ACCIDENTAL);
    this->applyKeySignature();
}

void NoteTakerDisplay::setRange() {
    auto nt = NT(this);
    oldStart = nt->selectStart;
    oldEnd = nt->selectEnd;
    rangeInvalid = true;
}

void NoteTakerDisplay::updateRange() {
    rangeInvalid = false;
    auto ntw = NTW(this);
    auto selectButton = ntw->widget<SelectButton>();
    auto nt = NT(this);
    int selectStartXPos = nt->xPosAtStartStart();
    int selectEndXPos = nt->xPosAtEndEnd();
    int selectWidth = selectEndXPos - selectStartXPos;
    const int boxWidth = box.size.x;
    int displayEndXPos = xAxisOffset + boxWidth;
    debug("selectStartXPos %d selectEndXPos %d xAxisOffset %d displayEndXPos %d",
            selectStartXPos, selectEndXPos, xAxisOffset, displayEndXPos);
    debug("old displayStart %u displayEnd %u", displayStart, displayEnd);
    // note condition to require the first quarter note of a very long note to be visible
    const int displayQuarterNoteWidth = stdTimePerQuarterNote * xAxisScale;
    const int displayStartMargin = selectButton->editStart() ? 0 : displayQuarterNoteWidth;
    const int displayEndMargin = displayQuarterNoteWidth * 2 - displayStartMargin;
    bool startIsVisible = xAxisOffset + displayStartMargin <= selectStartXPos
            && selectStartXPos + displayStartMargin <= displayEndXPos;
    bool endShouldBeVisible = selectEndXPos + displayEndMargin <= displayEndXPos
            || selectWidth > boxWidth;
    const int lastXPostion = cache.back().xPosition;
    bool recomputeDisplayOffset = !startIsVisible || !endShouldBeVisible
            || xAxisOffset > std::max(0, lastXPostion - boxWidth);
    bool recomputeDisplayEnd = recomputeDisplayOffset
            || !displayEnd || cache.size() <= displayEnd
            || this->startXPos(displayEnd) <= displayEndXPos;
    if (recomputeDisplayOffset) {
        // compute xAxisOffset first; then use that and boxWidth to figure displayStart, displayEnd
        float oldX = xAxisOffset;
        if (nt->selectEnd != oldEnd && nt->selectStart == oldStart) { // only end moved
            const NoteCache* last = nt->notes[nt->selectEnd].cache - 1;
            xAxisOffset = (this->cacheWidth(*last) > boxWidth ?
                nt->xPosAtEndStart() :  // show beginning of end
                selectEndXPos - boxWidth) + displayEndMargin;  // show all of end
            debug("1 xAxisOffset %g", xAxisOffset);
        } else if (xAxisOffset > selectStartXPos - displayStartMargin) { // left to start
            xAxisOffset = selectStartXPos - displayStartMargin;
            debug("2 xAxisOffset %g", xAxisOffset);
        } else {    // scroll enough to show start on right
            int selectBoxX = nt->xPosAtStartEnd();
            xAxisOffset = selectBoxX - boxWidth + displayEndMargin;
            debug("3 xAxisOffset %g selectBoxX %d", xAxisOffset, selectBoxX);
        }
        xAxisOffset = std::max(0.f,
                std::min((float) lastXPostion - boxWidth, xAxisOffset));
        dynamicXOffsetTimer = oldX - xAxisOffset;
        if (fabsf(dynamicXOffsetTimer) <= 5) {
            dynamicXOffsetTimer = 0;
        } else {
            dynamicXOffsetStep = -dynamicXOffsetTimer / 24;
        }
    }
    assert(xAxisOffset <= std::max(0, lastXPostion - boxWidth));
    if (recomputeDisplayEnd) {
        float displayStartXPos = std::max(0.f, xAxisOffset - displayQuarterNoteWidth);
        displayStart = std::min(cache.size() - 1, (size_t) displayStart);
        while (displayStart && this->startXPos(displayStart) >= displayStartXPos) {
            displayStart = this->cachePrevious(displayStart);
        }
        do {
            unsigned displayNext = this->cacheNext(displayStart);
            if (this->startXPos(displayNext) >= displayStartXPos) {
                break;
            }
            displayStart = displayNext;
        } while (displayStart < cache.size());
        displayEndXPos = std::min((float) lastXPostion,
                xAxisOffset + boxWidth + displayQuarterNoteWidth);
        while ((unsigned) displayEnd < cache.size()
                && this->startXPos(displayEnd) <= displayEndXPos) {
            displayEnd = this->cacheNext(displayEnd);
        }
        displayEnd = std::min(cache.size() - 1, (size_t) displayEnd);
        do {
            unsigned displayPrevious = this->cachePrevious(displayEnd);
            if (this->startXPos(displayPrevious) <= displayEndXPos) {
                break;
            }
            displayEnd = displayPrevious;
        } while (displayEnd);
        debug("displayStartXPos %d displayEndXPos %d", displayStartXPos, displayEndXPos);
        debug("displayStart %u displayEnd %u", displayStart, displayEnd);
    }
}

void NoteTakerDisplay::setUpAccidentals() {
    // prepare accidental, key, bar state prior to drawing
    // to do : could optimize this to skip notes except for bar prior to displayStart
    auto nt = NT(this);
    for (unsigned index = 0; index < displayStart; ++index) {
        const DisplayNote& note = nt->notes[index];
        switch (note.type) {
            case NOTE_ON: {
                this->advanceBar(index);
                const StaffNote& pitch = pitchMap[note.pitch()];
                accidentals[pitch.position] = (Accidental) pitch.accidental;
            } break;
            case KEY_SIGNATURE:
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                (void) bar.setSignature(note, nt->ppq);
                break;
            default:
                ;
        }
    }
}

struct PosAdjust {
    float x;
    int time;
};

static void track_pos(std::list<PosAdjust>& posAdjust, float xOff, int endTime) {
    if (xOff > 0) {
        PosAdjust adjust = { xOff, endTime };
        posAdjust.push_back(adjust);
        if (false) debug("xOff %g endTime %d", xOff, endTime);
    }
}

void NoteTakerDisplay::updateXPosition() {
    assert(vg);
    auto nt = NT(this);
    nvgFontFaceId(vg, musicFont->handle);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    bar.init();
    cache.clear();
    cache.reserve(nt->notes.size());
    this->setCacheDuration();  // adds cache per tied note part, sets its duration and note index
    assert(nt->notes.front().cache == &cache.front());
    assert(cache.front().note == &nt->notes.front());
    for (auto& noteCache : cache) {
        noteCache.beamPosition = PositionType::none;
        noteCache.beamCount = 0;
        noteCache.slurPosition = PositionType::none;
        noteCache.tupletPosition = PositionType::none;
    }
    if (!(nt->ppq % 3)) {  // to do : only triplets for now
        this->cacheTuplets();
    }
    for (auto& noteCache : cache) {
        const DisplayNote& note = *noteCache.note;
        noteCache.setDurationSymbol(nt->ppq);
        if (NOTE_ON != note.type) {
            noteCache.yPosition = 0;
            noteCache.stemUp = false;
        } else {
            const StaffNote& pitch = pitchMap[note.pitch()];
            noteCache.yPosition = this->yPos(pitch.position);
            noteCache.stemUp = this->stemUp(pitch.position);             
        }
    }
    this->cacheBeams();
    float pos = 0;
    std::list<PosAdjust> posAdjust;
    leadingTempo = false;
    bar.init();
    for (auto& noteCache : cache) {
        PosAdjust nextAdjust = { 0, 0 };
        for (auto& adjust : posAdjust) {
            if (noteCache.vStartTime >= adjust.time && adjust.time > nextAdjust.time) {
                nextAdjust = adjust;
            }
        }
        if (nextAdjust.x) {
            if (false) debug("posAdjust x %g time %d", nextAdjust.x, nextAdjust.time);
            pos += nextAdjust.x;
            auto iter = posAdjust.begin();
            while (iter != posAdjust.end()) {
                iter->x -= nextAdjust.x;
                if (iter->x <= 0) {
                    iter = posAdjust.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
        int bars = INT_MAX == bar.duration ? 0 : (noteCache.vStartTime - bar.tsStart) / bar.duration;
        int stdStart = NoteDurations::InStd(noteCache.vStartTime, nt->ppq);
        noteCache.xPosition = (int) ((stdStart + bars * BAR_WIDTH) * xAxisScale + pos);
        if (noteCache.accidentalSpace) {
            noteCache.xPosition += 8;  // space for possible accidental
        } else if (NOTE_ON == noteCache.note->type) {  // if another note at same time allows for
            auto next = &noteCache;                    // accidental, add space here, too
            assert(&cache.front() < next && next < &cache.back());
            while (++next < &cache.back()) {
                if (noteCache.vStartTime < next->vStartTime) {
                    break;
                }
                assert(noteCache.vStartTime == next->vStartTime);
                if (next->accidentalSpace) {
                    noteCache.xPosition += 8;
                    break;
                }
            }
        }
        if (false) debug("%d [%d] stdStart %d bars %d pos %g accidental %d", &noteCache - &cache.front(),
                noteCache.xPosition, stdStart, bars, pos, noteCache.accidentalSpace ? 8 : 0);
        const DisplayNote& note = *noteCache.note;
        switch (note.type) {
            case MIDI_HEADER:
                assert(!pos);
                pos = CLEF_WIDTH * xAxisScale;
                break;
            case NOTE_ON:
            case REST_TYPE: {
                    float drawWidth = this->cacheWidth(noteCache)
                            + (noteCache.accidentalSpace ? 8 : 0);
                    if (drawWidth < 10 && PositionType::none != noteCache.beamPosition) {
                        drawWidth += 3; // make space for half beams
                    }
                    float xOff = drawWidth - NoteDurations::InStd(noteCache.vDuration, nt->ppq)
                            * xAxisScale;
                    track_pos(posAdjust, xOff, noteCache.vEndTime());
                    if (false) debug("%d track_pos xOff %g cache start %d dur %d", &noteCache - &cache.front(),
                            xOff, noteCache.vStartTime, noteCache.vDuration);
                } break;
            case KEY_SIGNATURE:
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                // to do : measure key sig chars instead
                pos += std::max(MIN_KEY_SIGNATURE_WIDTH, 
                        abs(note.key()) * ACCIDENTAL_WIDTH * xAxisScale + 2);
                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                // to do : measure time sig chars instead
                pos += TIME_SIGNATURE_WIDTH * xAxisScale;  // add space to draw time signature
                bar.setSignature(note, nt->ppq);
                break;
            case MIDI_TEMPO:
                leadingTempo |= !note.startTime;
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                pos += TEMPO_WIDTH * xAxisScale;
                break;
            case TRACK_END:
                debug("[%u] xPos %d start %d", noteCache.note - &nt->notes.front(),
                        noteCache.xPosition, note.stdStart(nt->ppq));
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    this->cacheSlurs();
    assert(nt->notes.front().cache == &cache.front());
    assert(cache.front().note == &nt->notes.front());
}

