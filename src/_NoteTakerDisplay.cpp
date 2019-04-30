
#include "NoteTakerDisplay.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

#if 0   // not ready for this yet
static void draw_stem(NVGcontext*vg, float x, int ys, int ye) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + .5, ys);
    nvgLineTo(vg, x + .5, ye);
    nvgStrokeWidth(vg, .5);
    nvgStroke(vg);
}
#endif

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
// key signature width is variable, from 0 (C) to ACCIDENTAL_WIDTH*14+18
// (C# to Cb, 7 naturals, 7 flats, space for 2 bars)
const int ACCIDENTAL_WIDTH = 12;
const int BAR_WIDTH = 12;
const int NOTE_FONT_SIZE = 42;

void NoteTakerDisplay::_cacheBeams() {
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    for (unsigned cacheIndex = 0; cacheIndex < cache.size(); ++cacheIndex) {
        NoteCache& noteCache = cache[cacheIndex];
        if (NOTE_ON != noteCache.note->type) {
            continue;
        }
        unsigned& beamStart = beamStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != beamStart) {
            bool stemsMatch = cache[beamStart].stemUp == noteCache.stemUp;
            if (!stemsMatch || noteCache.vDuration >= nt->ppq) {
                this->_closeBeam(beamStart, cacheIndex);
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
            this->_closeBeam(beamStart, cache.size());
            debug("beam unmatched %s", cache[beamStart].note->debugString().c_str());
        }
    }
}

void NoteTakerDisplay::_cacheSlurs() {
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
                this->_closeSlur(slurStart, cacheIndex);
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
            this->_closeSlur(slurStart, cache.size());
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
void NoteTakerDisplay::_cacheTuplets() {
    array<unsigned, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(INT_MAX);
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        const DisplayNote& note = nt->allNotes[index];
        if (NOTE_ON != note.type) {
            continue;
        }
        unsigned& tripStart = tripStarts[note.channel];
        if (INT_MAX != tripStart) {
            const DisplayNote& start = nt->allNotes[tripStart];
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
                    NoteCache* last = nullptr;
                    while (++noteCache < &cache.back()) {
                        if (noteCache->note != &note) {
                            break;
                        }
                        if (chan == noteCache->channel) {
                            noteCache->tupletPosition = PositionType::mid;
                            last = noteCache;
                        }
                    }
                    assert(last);
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

void NoteTakerDisplay::_closeBeam(unsigned first, unsigned limit) {
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

void NoteTakerDisplay::_closeSlur(unsigned first, unsigned limit) {
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

// to do : keep track of where notes are drawn to avoid stacking them on each other
// likewise, avoid drawing ties and slurs on top of notes and each other
// to get started though, draw ties for each note that needs it
void NoteTakerDisplay::_drawBarNote(BarPosition& bar, const DisplayNote& note,
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
    this->drawNote(note, accidental, noteCache, alpha, NOTE_FONT_SIZE);
    bar.addPos(note, noteCache, this->cacheWidth(noteCache));
}

void NoteTakerDisplay::_drawNotes() {
    for (unsigned index = displayStart; index < displayEnd; ++index) {
        const NoteCache& noteCache = cache[index];
        const DisplayNote& note = *noteCache.note;
        this->advanceBar(index);
        switch (note.type) {
            case NOTE_ON: {
                unsigned char alpha = nt->isSelectable(note) ? 0xff : 0x7f;
                this->_drawBarNote(bar, note, noteCache, alpha);
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
            } break;
            case REST_TYPE:
                this->drawBarRest(bar, note, noteCache.xPosition + 8,
                        nt->isSelectable(note) ? 0xff : 0x7f);
            break;
            case MIDI_HEADER:
            break;
            case KEY_SIGNATURE: 
                bar.setPriorBars(note);
                this->drawKeySignature(index);
                bar.setMidiEnd(note);
             break;
            case TIME_SIGNATURE: {
                bar.setPriorBars(note);
                bar.setSignature(note, nt->ppq);
                bar.setMidiEnd(note);
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
                bar.setPriorBars(note);
                this->drawTempo(noteCache.xPosition, note.tempo(), 0xFF);
                bar.setMidiEnd(note);
            break;
            case TRACK_END:
            break;
            default:
                debug("to do: add type %d\n", note.type);
                assert(0); // incomplete
        }
    }
}

// to do : share code with drawing ties?
// to do : mock score software ?
// in scoring software, slur is drawn from first to last, offset by middle, if needed
void NoteTakerDisplay::_drawSlur(unsigned start, unsigned char alpha) const {
    BeamPositions bp;
    unsigned index = start;
    int chan = cache[start].channel;
    do {
        if (chan != cache[index].channel) {
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

// compute ties first, so we know how many notes to draw
void NoteTakerDisplay::setCacheDuration() {
    unsigned cIndex = -1;
    const unsigned slop = 256;  // to do : tune this
    for (unsigned index = 0; index < nt->allNotes.size(); ++index) {
        DisplayNote& note = nt->allNotes[index];
        NoteCache& cacheEntry = cache[++cIndex];
        note.cache = &cacheEntry;
        cacheEntry.note = &note;
        cacheEntry.channel = note.channel;
        cacheEntry.accidentalSpace = false;
        if (!note.isNoteOrRest()) {
            cacheEntry.vDuration = 0;
            cacheEntry.endsOnBeat = false;
            continue;
        }
        if (cIndex + slop > cache.size()) {
            cache.resize(cache.size() + slop);
        }
        int notesTied = bar.notesTied(note, nt->ppq);
        if (1 == notesTied) {
            cacheEntry.vDuration = note.duration;
            cacheEntry.endsOnBeat = (bool) (note.endTime() % nt->ppq);
            cacheEntry.accidentalSpace = true;
        } else {
            int duration = bar.leader;
            int remaining = note.duration - bar.leader;
            int tieTime = note.startTime;
            --cIndex;
            bool accidentalSpace = true;
            do {
                do {
                    NoteCache& tiePart = cache[++cIndex];
                    tiePart.vDuration = NoteDurations::Closest(duration, nt->ppq);
                    tiePart.note = &note;
                    tiePart.channel = note.channel;
                    tiePart.accidentalSpace = accidentalSpace;
                    tiePart.slurPosition = accidentalSpace ? PositionType::left : PositionType::mid;
                    accidentalSpace = false;
                    tieTime += tiePart.vDuration;
                    tiePart.endsOnBeat = (bool) (tieTime % nt->ppq);
                    duration -= tiePart.vDuration;
                } while (duration >= NoteDurations::ToMidi(0, nt->ppq));
                duration = std::min(bar.duration, remaining);
                if (!duration) {
                    cache[cIndex].slurPosition = PositionType::right;
                    break;
                }
                remaining -= duration;
            } while (true);
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
        debug("xOff %g endTime %d", xOff, endTime);
    }
}

void NoteTakerDisplay::_updateXPosition() {
    assert(vg);
    nvgFontFaceId(vg, musicFont->handle);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    cache.resize(nt->allNotes.size());
    bar.init();
    this->setCacheDuration();  // adds cache per tied note part, sets its duration and note index
    for (auto& noteCache : cache) {
        noteCache.resetTupleBeam();
    }
    if (!(nt->ppq % 3)) {  // to do : only triplets for now
        this->_cacheTuplets();
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
    this->_cacheBeams();
    float pos = 0;
    std::list<PosAdjust> posAdjust;
    leadingTempo = false;
    for (auto& noteCache : cache) {
        PosAdjust nextAdjust = { 0, 0 };
        for (auto& adjust : posAdjust) {
            if (noteCache.vStartTime >= adjust.time && adjust.time > nextAdjust.time) {
                nextAdjust = adjust;
            }
        }
        if (nextAdjust.x) {
            debug("posAdjust x %g time %d", nextAdjust.x, nextAdjust.time);
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
        debug("%d [%d] pos %g stdStart %d", noteCache.note - &nt->allNotes.front(),
                noteCache.xPosition, pos, stdStart);
        if (noteCache.accidentalSpace) {
            noteCache.xPosition += 8;  // space for possible accidental
        }
        const DisplayNote& note = *noteCache.note;
        switch (note.type) {
            case MIDI_HEADER:
                assert(!pos);
                pos = CLEF_WIDTH * xAxisScale;
                break;
            case NOTE_ON:
            case REST_TYPE: {
                    float drawWidth = this->cacheWidth(noteCache);
                    float xOff = drawWidth - NoteDurations::InStd(noteCache.vDuration, nt->ppq)
                            * xAxisScale;
                    track_pos(posAdjust, xOff, noteCache.vStartTime + noteCache.vDuration);
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
                debug("[%u] xPos %d start %d", noteCache.note - &nt->allNotes.front(),
                        noteCache.xPosition, note.stdStart(nt->ppq));
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    this->_cacheSlurs();
}
