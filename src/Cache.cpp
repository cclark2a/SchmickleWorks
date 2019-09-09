#include "Cache.hpp"
#include "Display.hpp"
#include "Notes.hpp"
#include "Widget.hpp"

BarPosition::BarPosition() {
    pos.clear();
    priorBars = 0;
    duration = INT_MAX;
    midiEnd = INT_MAX;
    tsStart = 0;
}

void BarPosition::addPos(const NoteCache& noteCache, float cacheWidth) {
    bool hasDuration = INT_MAX != duration;
    int barCount = this->count(noteCache);
    pos[barCount].xMin = std::min(pos[barCount].xMin, (float) noteCache.xPosition);
#if DEBUG_BAR_ADD_POS
    if (debugVerbose) DEBUG("addPos [%d] xMin %g xPos %d",
            barCount, pos[barCount].xMin, noteCache.xPosition);
#endif
    // note the 'add duration less one' to round up
    barCount = hasDuration ? 
            (noteCache.vEndTime() - tsStart + duration - 1) / duration + priorBars : 1;  // rounds up
    pos[barCount].xMax = std::max(pos[barCount].xMax, noteCache.xPosition + cacheWidth);
#if DEBUG_BAR_ADD_POS
    if ( debugVerbose) DEBUG("addPos [%d] xMax %g xPos %d width %g",
            barCount, pos[barCount].xMax, noteCache.xPosition, cacheWidth);
#endif
}

int BarPosition::count(const NoteCache& noteCache) const {
    return (noteCache.vStartTime - tsStart) / duration + priorBars;
}

int BarPosition::noteRegular(const DisplayNote& note, int ppq, bool twoThirds) {
    int result = NoteTakerDisplay::TiedCount(duration, note.duration, ppq);
#if DEBUG_BAR
    if (debugVerbose) DEBUG("noteRegular result %d twoThirds %d note %s", result, twoThirds,
            note.debugString().c_str());
#endif
    if (1 == result || !twoThirds) {
        return result;
    }
    return NoteTakerDisplay::TiedCount(duration, note.duration * 3 / 2, ppq);
}

int BarPosition::notesTied(const DisplayNote& note, int ppq, bool* twoThirds) {
    *twoThirds = NoteDurations::TripletPart(note.duration, ppq);
    int inTsStartTime = note.startTime - tsStart;
    int startBar = inTsStartTime / duration;
    int inTsEndTime = note.endTime() - tsStart;  // end time relative to time signature start
    int endBar = inTsEndTime / duration;
#if DEBUG_BAR
    if (debugVerbose) DEBUG("notesTied inTsStartTime %d startBar %d inTsEndTime %d endBar %d",
            inTsStartTime, startBar, inTsEndTime, endBar);
#endif
    if (startBar == endBar) {
        leader = note.duration;
        return this->noteRegular(note, ppq, *twoThirds);
    }
    // note.startTime needs be relative to last bar start, not zero
    leader = (startBar + 1) * duration - inTsStartTime;
    SCHMICKLE(leader >= 0);
    int trailer = inTsEndTime - endBar * duration;
    SCHMICKLE(0 <= trailer);
    if (trailer >= duration) {
#if DEBUG_BAR
        if (debugVerbose) DEBUG("notesTied trailer %d duration %d inTsEndTime %d endBar %d inTsStartTime %d"
                " startBar %d leader %d",
                trailer, duration, inTsEndTime, endBar, inTsStartTime, startBar, leader);
        if (debugVerbose) DEBUG("note %s", note.debugString().c_str());
#endif
    }
    SCHMICKLE(trailer < duration);
#if DEBUG_BAR
    if (debugVerbose) DEBUG("notesTied leader %d trailer %d duration %d endBar %d startBar %d",
            leader, trailer, duration, endBar, startBar);
#endif
    int result = 0;
    if (leader >= NoteDurations::Smallest(ppq)) {
        result += NoteTakerDisplay::TiedCount(duration, leader, ppq);
    }
    result += endBar - startBar - 1;  // # of bars with whole notes
    if (trailer >= NoteDurations::Smallest(ppq)) {
        result += NoteTakerDisplay::TiedCount(duration, trailer, ppq);
    }
    return result;
}

// for x position: figure # of bars added by previous signature
int BarPosition::resetSignatureStart(const DisplayNote& note, float barWidth) {
    int result = 0;
    if (INT_MAX != duration) {
        result = (note.startTime - tsStart + duration - 1) / duration * barWidth; // round up
    } else if (tsStart < note.startTime) {
        result = barWidth;
    }
    tsStart = note.startTime;
    return result;
}

CacheBuilder::CacheBuilder(const DisplayState& sd, const Notes& n, DisplayCache* c, int _ppq) 
    : state(sd)
    , notes(n)
    , cache(c)
    , ppq(_ppq) {
}

// if note cache is in new measure, close previous beam if any and start a new one
void CacheBuilder::cacheBeams() {
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    vector<NoteCache>& notes = cache->notes;
    int startBar = -1;
    for (unsigned cacheIndex = 0; cacheIndex < notes.size(); ++cacheIndex) {
        NoteCache& noteCache = notes[cacheIndex];
        const DisplayNote& note = *noteCache.note;
        if (NOTE_ON != note.type) {
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

        if (!noteCache.staff) {
#if DEBUG_BEAM
            if (debugVerbose) DEBUG("cacheBeams no staff %s note %s",
                    noteCache.debugString().c_str(), 
                    noteCache.note->debugString().c_str());
#endif
            continue;
        }
        unsigned& beamStart = beamStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != beamStart) {
            bool stemsMatch = notes[beamStart].stemDirection == noteCache.stemDirection
                    && (notes[beamStart].pitchPosition <= MIDDLE_C)
                    == (noteCache.pitchPosition <= MIDDLE_C);
            if (!stemsMatch || noteCache.vDuration >= ppq || startBar != noteCache.bar) {
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
#if DEBUG_BEAM
        if (debugVerbose) DEBUG("cacheBeams checkForStart %d noteCache.vDuration %d ppq %d",
                checkForStart, noteCache.vDuration, ppq);
#endif
        if (checkForStart && noteCache.vDuration < ppq) {
            // to do : if 4:4 time, allow beaming up to half note ?
            beamStart = cacheIndex;
            noteCache.beamPosition = PositionType::left;
            startBar = noteCache.bar;
#if DEBUG_BEAM
            if (debugVerbose) DEBUG("cacheBeams beamStart %s", noteCache.debugString().c_str());
#endif
        }
        if (INT_MAX != beamStart || PositionType::right == noteCache.beamPosition) {
            noteCache.beamCount = NoteDurations::Beams(noteCache.twoThirds ?
                    NoteDurations::FromTripletMidi(noteCache.vDuration, ppq) : 
                    NoteDurations::FromMidi(noteCache.vDuration, ppq));
        }
    }
    for (auto beamStart : beamStarts) {
        if (INT_MAX != beamStart) {
            this->closeBeam(beamStart, notes.size());
#if DEBUG_BEAM
            if (debugVerbose) DEBUG("cacheBeams beam unmatched %s",
                    notes[beamStart].note->debugString().c_str());
#endif
        }
    }
}

void CacheBuilder::cacheSlurs() {
    array<unsigned, CHANNEL_COUNT> slurStarts;
    slurStarts.fill(INT_MAX);
    vector<NoteCache>& notes = cache->notes;
    for (unsigned cacheIndex = 0; cacheIndex < notes.size(); ++cacheIndex) {
        NoteCache& noteCache = notes[cacheIndex];
        const DisplayNote* note = noteCache.note;
        SCHMICKLE(noteCache.channel == note->channel);
        if (NOTE_ON != note->type) {
            if (REST_TYPE == note->type) {
                this->closeSlur(slurStarts[noteCache.channel], cacheIndex);
            } else {
                for (auto slurStart : slurStarts) {
                    this->closeSlur(slurStart, cacheIndex);
                }
            }
            continue;
        }
        if (!noteCache.staff) {
            continue;
        }
        unsigned& slurStart = slurStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != slurStart) {
            if (!note->slurEnd()) {
                this->closeSlur(slurStart, cacheIndex);
#if DEBUG_SLUR
                if (debugVerbose) DEBUG("set slur close %d/%d %s", slurStart,
                        notes[slurStart].note - &this->notes.notes.front(),
                        notes[slurStart].note->debugString().c_str());
#endif
                slurStart = INT_MAX;
            } else {
                noteCache.slurPosition = PositionType::mid;
                checkForStart = false;
#if DEBUG_SLUR
                if (debugVerbose) DEBUG("set slur mid %d/%d %s", cacheIndex,
                    note - &this->notes.notes.front(),
                    note->debugString().c_str());
#endif
            }
        }
        if (checkForStart && note->slurStart()) {
            slurStart = cacheIndex;
            if (PositionType::none == noteCache.slurPosition) {
                noteCache.slurPosition = PositionType::left;
#if DEBUG_SLUR
                if (debugVerbose) DEBUG("set slur left %d/%d %s", cacheIndex,
                        note - &this->notes.notes.front(),
                        note->debugString().c_str());
#endif
            }
        }
    }
    for (auto slurStart : slurStarts) {
        this->closeSlur(slurStart, notes.size());
#if DEBUG_SLUR
         if (INT_MAX != slurStart && debugVerbose) 
                DEBUG("slur unmatched %d/%d %s", slurStart,
                        notes[slurStart].note - &this->notes.notes.front(),
                        notes[slurStart].note->debugString().c_str());
#endif
    }
}

static StemType stem_up(int pitch) {
    if (pitch <= TREBLE_MID) {
        return pitch == TREBLE_MID ? StemType::either : StemType::down;
    }
    if (pitch <= MIDDLE_C) {
        return pitch == MIDDLE_C ? StemType::either : StemType::up;
    }
    if (pitch <= BASS_MID) {
        return pitch == BASS_MID ? StemType::either : StemType::down;
    }
    return StemType::up;
}

// to do : add new code to horizontally shift notes with different durations?
// to do : if quarter and eighth share staff, set stem up in different directions?
// collect the notes with the same start time and duration
// mark only one as the staff owner
void CacheBuilder::cacheStaff() {
    vector<NoteCache>& notes = cache->notes;
    for (NoteCache* first = &notes.front(); first <= &notes.back(); ++first) {
        if (StemType::unknown != first->stemDirection) {
            continue;
        }
        if (NOTE_ON != first->note->type) {
            continue;
        }
        first->staff = true;
        auto next = first;
        int duration = first->vDuration;
        int firstPitch = first->pitchPosition;
        bool hasMiddleC = MIDDLE_C == firstPitch;
        bool firstTreble = firstPitch < MIDDLE_C;
        bool firstBass = firstPitch > MIDDLE_C;
        int highestTreble = firstTreble ? firstPitch : 255;
        int lowestTreble = firstTreble || hasMiddleC ? firstPitch : 0;
        int highestBass = firstBass ? firstPitch : 255;
        int lowestBass = firstBass ? firstPitch : 0;
        int trebleChord = firstPitch <= MIDDLE_C;
        int bassChord = firstPitch >= MIDDLE_C;
        while (++next <= &notes.back()) {
            if (StemType::unknown != next->stemDirection) {
                continue;
            }
            if (NOTE_ON != next->note->type) {
                break;
            }
            if (next->vStartTime != first->vStartTime) {
                break;
            }
            if (duration != next->vDuration) {
                continue;
            }
            int nextPitch = next->pitchPosition;
            hasMiddleC |= MIDDLE_C == nextPitch;
            // to do : if pitch has already been encountered, could put both on one stem
            //         if 3 or more, need more stems
            if (nextPitch < MIDDLE_C) {
                highestTreble = std::min(nextPitch, highestTreble);
            }
            if (nextPitch <= MIDDLE_C) {
                lowestTreble = std::max(nextPitch, lowestTreble);
                ++trebleChord;
            }
            if (nextPitch > MIDDLE_C) {
                highestBass = std::min(nextPitch, highestBass);
                lowestBass = std::max(nextPitch, lowestBass);
            }
            bassChord += nextPitch >= MIDDLE_C;
        }
        if (bassChord <= 1 && trebleChord <= 1) {
            first->stemDirection = stem_up(first->pitchPosition);
            // to do : more sophisticated tie breaker
            if (StemType::either == first->stemDirection) {
                first->stemDirection = StemType::down;
            }
            continue;
        }
        // notehead furthest away from center of stave determines stem direction (BB p47)
        StemType trebleStemDirection = (TREBLE_MID - highestTreble) >= (lowestTreble - TREBLE_MID) ?
                StemType::down : StemType::up;
        StemType bassStemDirection = (BASS_MID - highestBass) >= (lowestBass - BASS_MID) ?
                StemType::down : StemType::up;
        for (NoteCache* entry = first; entry < next; ++entry) {
            if (StemType::unknown != next->stemDirection) {
                continue;
            }
            if (duration != next->vDuration) {
                continue;
            }
            entry->stemDirection = entry->pitchPosition < MIDDLE_C
                    || (entry->pitchPosition == MIDDLE_C && trebleChord > 1) ? trebleStemDirection :
                    bassStemDirection;
        }
    }
}

// only mark triplets if notes are the right duration and collectively a multiple of three
// to do : if too few notes are of the right duration, they (probably) need to be drawn as ties --
// 2/3rds of 1/4 note is 1/6 (.167) which could be 1/8 + 1/32 (.156)
void CacheBuilder::cacheTuplets() {
    array<unsigned, CHANNEL_COUNT> tripStarts;  // first index in notes (not cache) of triplet
    tripStarts.fill(INT_MAX);
    uint8_t tupletId = 0;
    for (unsigned index = 0; index < notes.notes.size(); ++index) {
        const DisplayNote& note = notes.notes[index];
        if (!note.isNoteOrRest()) {
            tripStarts.fill(INT_MAX);
            continue;
        }
        if (!note.cache->staff) {
            continue;
        }
        if (PositionType::none != note.cache->tupletPosition) {
            continue;
        }
        unsigned& tripStart = tripStarts[note.channel];
        if (INT_MAX != tripStart) {
            const DisplayNote& start = notes.notes[tripStart];
            int totalDuration = note.endTime() - start.startTime;
            int tripDur = start.duration * 3;
            int factor = totalDuration / tripDur;
                // to do : use time signature to limit triplet to bar 
                // for now, limit triplet to whole note
            if ((totalDuration > ppq * 4)
                // to do : take advantage of start time % 3 to note that remainder is 0/1/2
                //         if the note is part of the 1st/2nd/3rd triplet
                    || (totalDuration * 2 != tripDur && factor * tripDur != totalDuration)) {
                tripStart = INT_MAX;
            } else {
                NoteCache* noteCache = start.cache;
                if (!++tupletId) {
                    tupletId = 1;
                }
#if DEBUG_TRIPLET_DRAW
                std::string dbg = "tripStart: " + std::to_string(tripStart)
                        + " total: " + std::to_string(totalDuration)
                        + " trip: " + std::to_string(tripDur)
                        + " factor: " + std::to_string(factor)
                        + " tupletId: " + std::to_string(tupletId)
                        + " L" + std::to_string(noteCache - &this->cache->notes.front())
                        + "/" + std::to_string(noteCache->note - &n.notes.front())
                        DEBUG_TRIPLET_DRAW_DETAILS(noteCache);
                if (PositionType::none != noteCache->tupletPosition) {
                    auto noteStart = &n.notes.front();
                    DEBUG("!!!left %s", dbg.c_str());
                    DEBUG("noteCache->tupletPosition %s", 
                            debugPositionType[(int) noteCache->tupletPosition]);
                    Notes::DebugDump(n.notes, &start - noteStart - 10,
                            noteCache->note - noteStart + 10, &cache->notes,
                            &start - noteStart, noteCache->note - noteStart);
                }
#endif
                SCHMICKLE(PositionType::none == noteCache->tupletPosition);
                noteCache->tupletPosition = PositionType::left;
                noteCache->tupletId = tupletId;
                int chan = start.channel;
                NoteCache* last = note.cache;
                NoteCache* next = last;
                while (++next < &cache->notes.back()) {
                    if (!next->staff) {
                        continue;
                    }
                    if (chan != next->channel) {
                        continue;
                    }
                    if (next->note != &note) {
                        break;
                    }
                    last = next;
                }
                while (++noteCache < last) {
                    if (!noteCache->staff) {
                        continue;
                    }
                    if (chan == noteCache->channel) {
#if DEBUG_TRIPLET_DRAW
                        if (PositionType::none != noteCache->tupletPosition) {
                            auto noteStart = &n.notes.front();
                            DEBUG("!!!mid %s", dbg.c_str());
                            DEBUG("noteCache->tupletPosition %s", 
                                    debugPositionType[(int) noteCache->tupletPosition]);
                            Notes::DebugDump(n.notes, &start - noteStart - 10,
                                    noteCache->note - noteStart + 10, &cache->notes,
                                    &start - noteStart, noteCache->note - noteStart);
                        }
#endif
                        SCHMICKLE(PositionType::none == noteCache->tupletPosition);
                        noteCache->tupletPosition = PositionType::mid;
                        noteCache->tupletId = tupletId;
#if DEBUG_TRIPLET_DRAW
                        dbg += " M" + std::to_string(noteCache - &this->cache->notes.front())
                                + "/" + std::to_string(noteCache->note - &n.notes.front())
                                DEBUG_TRIPLET_DRAW_DETAILS(noteCache);
#endif
                    }
                }
                SCHMICKLE(PositionType::none == last->tupletPosition);
                last->tupletPosition = PositionType::right;
                last->tupletId = tupletId;
                tripStart = INT_MAX;
#if DEBUG_TRIPLET_DRAW
                dbg += " R" + std::to_string(last - &this->cache->notes.front())
                        + "/" + std::to_string(last->note - &n.notes.front())
                        DEBUG_TRIPLET_DRAW_DETAILS(last);
                DEBUG("%s", dbg.c_str());
#endif
                continue;  // don't reuse note as start of next triplet
            }
        }
        // to do : remove or mark as debugging only
        if (!NoteDurations::InStd(note.duration, ppq)) {
#if DEBUG_TRIPLET_DRAW
            DEBUG("note !InStd %s ppq %d", note.debugString().c_str(), n.ppq);
#endif
        }
        if (NoteDurations::TripletPart(note.duration, ppq)) {
            tripStart = index;
        }
    }
}

void CacheBuilder::clearTuplet(unsigned index, unsigned limit) {
    vector<NoteCache>& notes = cache->notes;
    SCHMICKLE(PositionType::left == notes[index].tupletPosition);
    const uint8_t tupletId = notes[index].tupletId;
    SCHMICKLE(tupletId);
    notes[index].tupletPosition = PositionType::none;
    while (++index < limit) {
        if (tupletId != notes[index].tupletId) {
            continue;
        }
        SCHMICKLE(PositionType::mid == notes[index].tupletPosition);
        notes[index].tupletPosition = PositionType::none;
    }
}

void CacheBuilder::closeBeam(unsigned first, unsigned limit) {
    vector<NoteCache>& notes = cache->notes;
    SCHMICKLE(PositionType::left == notes[first].beamPosition);
    int chan = notes[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (!notes[index].staff) {
            continue;
        }
        if (chan == notes[index].channel) {
            if (PositionType::mid != notes[index].beamPosition) {
                break;
            }
            last = index;
        }
    }
    notes[last].beamPosition = last == first ? PositionType::none : PositionType::right;
#if DEBUG_BEAM
    if (debugVerbose) DEBUG("close beam first %d last %d pos %u", first, last, notes[last].beamPosition);
#endif
}

// start here;
// debug and assert if closed slur does not have a left and right
void CacheBuilder::closeSlur(unsigned first, unsigned limit) {
    if (INT_MAX == first) {
        return;
    }
    vector<NoteCache>& notes = cache->notes;
    SCHMICKLE(PositionType::left == notes[first].slurPosition);
    int chan = notes[first].channel;
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (!notes[index].staff) {
            continue;
        }
        if (chan == notes[index].channel) {
            if (PositionType::mid != notes[index].slurPosition) {
                break;
            }
            last = index;
        }
    }
    notes[last].slurPosition = last == first ? PositionType::none : PositionType::right;
#if DEBUG_SLUR
    if (debugVerbose) DEBUG("close slur first %d/%d last %d/%d pos %s",
            first, notes[first].note - &this->notes.notes.front(), 
            last, notes[last].note - &this->notes.notes.front(),
            debugPositionType[(int) notes[last].slurPosition]);
#endif
}


// compute ties first, so we know how many notes to draw
// to do : use cache notes' bar count instead of recomputing the bar count
void CacheBuilder::setDurations() {
    const StaffNote* pitchMap = sharpMap;
    BarPosition bar;
#if DEBUG_DURATIONS
    if (debugVerbose) DEBUG("setDurations %u ", n.notes.size());
#endif
    for (unsigned index = 0; index < notes.notes.size(); ++index) {
        const DisplayNote& note = notes.notes[index];
        cache->notes.emplace_back(&note);
        NoteCache& cacheEntry = cache->notes.back();
        cacheEntry.bar = bar.count(cacheEntry);
        if (!note.isNoteOrRest()) {
            if (TIME_SIGNATURE == note.type) {
                bar.setSignature(note, ppq);
            } else if (note.isSignature()) {
                bar.tsStart = note.startTime;
                if (KEY_SIGNATURE == note.type) {
                    pitchMap = note.key() >= 0 ? sharpMap : flatMap;
                }
            }
            continue;
        }
        uint8_t pitchPosition = NOTE_ON == note.type ? pitchMap[note.pitch()].position : 0;
        // if note is 2/3rds of a regular duration, it may be part of a third; defer tie in case
        bool twoThirds = false;
        int notesTied = bar.notesTied(note, ppq, &twoThirds);
        if (1 == notesTied) {
            cacheEntry.twoThirds = twoThirds;
            cacheEntry.vDuration = note.duration;
#if DEBUG_DURATIONS
            if (debugVerbose) DEBUG("setDurations vDur %d note %s", cacheEntry.vDuration,
                    note.debugString().c_str());
#endif
            cacheEntry.endsOnBeat = (bool) (note.endTime() % ppq);
            if (NOTE_ON == note.type) {
                cacheEntry.accidentalSpace = true;
                cacheEntry.pitchPosition = pitchPosition;
                cacheEntry.yPosition = NoteTakerDisplay::YPos(pitchPosition);
            }
        } else {
            int duration = bar.leader;
            int remaining = note.duration - bar.leader;
            int tieTime = note.startTime;
#if DEBUG_DURATIONS
            if (debugVerbose) DEBUG("duration %d remaining %d tieTime %d added %s",
                    duration, remaining, tieTime, note.debugString().c_str());
#endif
            bool accidentalSpace = NOTE_ON == note.type;
            cache->notes.pop_back();
            do {
                do {
                    cache->notes.emplace_back(&note);
                    NoteCache& tiePart = cache->notes.back();
                    tiePart.twoThirds = twoThirds;
                    tiePart.vStartTime = tieTime;
                    tiePart.bar = bar.count(tiePart);
                    tiePart.tiePosition = accidentalSpace ? PositionType::left : PositionType::mid;
                    // if in a triplet, scale up by 3/2, look up note, scale result by 2/3
                    tiePart.vDuration = NoteDurations::LtOrEq(duration, ppq, twoThirds);
#if DEBUG_DURATIONS
                    if (debugVerbose) {
                        DEBUG("vStart %d vDuration %d duration %d [%d] cache [%d] note",
                                tiePart.vStartTime, tiePart.vDuration, duration,
                                &tiePart - &cache->notes.front(), tiePart.note - &n.notes.front());
                    }
#endif
                    tieTime += tiePart.vDuration;
                    tiePart.endsOnBeat = (bool) (tieTime % ppq);
                    tiePart.accidentalSpace = accidentalSpace;
                    accidentalSpace = false;
                    duration -= tiePart.vDuration;
                    if (NOTE_ON == note.type) {
                        tiePart.pitchPosition = pitchPosition;
                        tiePart.yPosition = NoteTakerDisplay::YPos(pitchPosition);
                    }
                } while (duration >= NoteDurations::Smallest(ppq, twoThirds));
                tieTime += std::max(0, duration);   // if some fraction couldn't be represented...
                duration = std::min(bar.duration, remaining);
                if (!duration) {
                    cache->notes.back().tiePosition = PositionType::right;
                    break;
                }
                remaining -= duration;
            } while (true);
        }
    }
    // don't use std::sort function; use insertion sort to minimize reordering
    for (auto it = cache->notes.begin(), end = cache->notes.end(); it != end; ++it) {
        auto const insertion_point = std::upper_bound(cache->notes.begin(), it, *it);
        std::rotate(insertion_point, it, it + 1);
    }
    cache->notes.shrink_to_fit();
    // sort, shrink, vector push back may move cache locations, so set them up last
    // walk backwards to put point note at first cache entry if note has more than one
    for (auto riter = cache->notes.rbegin(); riter != cache->notes.rend(); ++riter) {
        const_cast<DisplayNote*>(riter->note)->cache = &*riter;
    }
    // check to see if things moved
    SCHMICKLE(notes.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes.notes.front());
#if DEBUG_DURATIONS
    if (debugVerbose) DEBUG("finished set durations");
    if (debugVerbose) {
        int index = 0;
        for (const auto& note : n.notes) {
            DEBUG("[%d] note %s cache [%d]", index++, note.debugString().c_str(),
                    note.cache - &cache->notes.front());
        }
        index = 0;
        for (const auto& note : cache->notes) {
            DEBUG("[%d] cache %s note [%d]", index++, note.debugString().c_str(),
                    note.note - &n.notes.front());
        }
    }
#endif
}

void CacheBuilder::trackPos(std::list<PosAdjust>& posAdjust, float xOff, int endTime) {
    if (xOff > 0) {
        PosAdjust adjust = { xOff, endTime };
        posAdjust.push_back(adjust);
#if DEBUG_POS
        if (debugVerbose) DEBUG("xOff %g endTime %d", xOff, endTime);
#endif
    }
}

void CacheBuilder::updateXPosition() {
    auto vg = state.vg;
    float xAxisScale = state.xAxisScale;
    SCHMICKLE(vg);
    nvgFontFaceId(vg, state.musicFont);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    cache->notes.clear();
    cache->notes.reserve(notes.notes.size());
    this->setDurations();  // adds cache per tied note part, sets its duration and note index
    // to do : If a pair of notes landed at the same place at the same time, but have different
    //         pitches, flip the notes' accidentals to move them to different staff fines.
    //       : Next, figure out how many horizontal positions are required to show non-overlapping
    //         notes.
    this->cacheStaff();  // set staff flag if note owns shared staff
    SCHMICKLE(notes.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes.notes.front());
    if (!(ppq % 3)) {  // to do : only triplets for now
        this->cacheTuplets();
    }
    for (auto& noteCache : cache->notes) {
        if (noteCache.note->isNoteOrRest()) {
            noteCache.setDurationSymbol(ppq);
        }
    }
    this->cacheBeams();
    float pos = 0;
    std::list<PosAdjust> posAdjust;
    cache->leadingTempo = false;
    BarPosition bar;
    for (auto& noteCache : cache->notes) {
        PosAdjust nextAdjust = { 0, 0 };
        for (auto& adjust : posAdjust) {
            if (noteCache.vStartTime >= adjust.time && adjust.time > nextAdjust.time) {
                nextAdjust = adjust;
            }
        }
        if (nextAdjust.x) {
#if DEBUG_POS
            if (debugVerbose) DEBUG("posAdjust x %g time %d", nextAdjust.x, nextAdjust.time);
#endif
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
        if (noteCache.note->isSignature()) {
            bar.setPriorBars(noteCache);
        }
        int bars = bar.priorBars + ((noteCache.vStartTime - bar.tsStart) / bar.duration);
        int stdStart = NoteDurations::InStd(noteCache.vStartTime, ppq);
        noteCache.xPosition = (int) std::ceil((stdStart + bars * BAR_WIDTH)
                * xAxisScale + pos);
#if DEBUG_POS
        if (debugVerbose) DEBUG("vStartTime %d n.ppq %d bars %d xPos %d",
                noteCache.vStartTime, n.ppq, bars, noteCache.xPosition);
#endif
        if (noteCache.accidentalSpace) {
            noteCache.xPosition += 8;  // space for possible accidental
        } else if (NOTE_ON == noteCache.note->type) {  // if another note at same time allows for
            auto next = &noteCache;                    // accidental, add space here, too
            SCHMICKLE(&cache->notes.front() < next && next < &cache->notes.back());
            while (++next < &cache->notes.back()) {
                if (noteCache.vStartTime < next->vStartTime) {
                    break;
                }
                SCHMICKLE(noteCache.vStartTime == next->vStartTime);
                if (next->accidentalSpace) {
                    noteCache.xPosition += 8;
                    break;
                }
            }
        }
#if DEBUG_POS
        if (debugVerbose) DEBUG("%d [%d] stdStart %d bars %d pos %g accidental %d",
                &noteCache - &cache->notes.front(),
                noteCache.xPosition, stdStart, bars, pos, noteCache.accidentalSpace ? 8 : 0);
#endif
        const DisplayNote& note = *noteCache.note;
        switch (note.type) {
            case MIDI_HEADER:
                SCHMICKLE(!pos);
                pos = CLEF_WIDTH * xAxisScale;
#if DEBUG_POS
                if (debugVerbose) DEBUG("header pos %g", pos);
#endif
                break;
            case NOTE_ON:
            case REST_TYPE: {
                    float drawWidth = NoteTakerDisplay::CacheWidth(noteCache, vg)
                            + (noteCache.accidentalSpace ? 8 : 0);
                    // to do : if note cache has no staff, it still needs to adjust draw width
                    if (drawWidth < 10 && PositionType::none != noteCache.beamPosition) {
                        drawWidth += 3; // make space for half beams
                    }
                    float xOff = drawWidth - NoteDurations::InStd(noteCache.vDuration, ppq)
                            * xAxisScale;
                    this->trackPos(posAdjust, xOff, noteCache.vEndTime());
#if DEBUG_POS
                    if (debugVerbose) DEBUG("%d track_pos drawWidth %g xOff %g cache start %d dur %d",
                            &noteCache - &cache->notes.front(), drawWidth,
                            xOff, noteCache.vStartTime, noteCache.vDuration);
#endif
                } break;
            case KEY_SIGNATURE:
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                // to do : measure key sig chars instead
                pos += std::max(MIN_KEY_SIGNATURE_WIDTH * xAxisScale, 
                        abs(note.key()) * ACCIDENTAL_WIDTH * xAxisScale + 2);
// I don't think building the cache is impacted by the key signature, but leave this as a reminder..
//                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                pos += NoteTakerDisplay::TimeSignatureWidth(note, vg, state.musicFont);
                pos += TIME_SIGNATURE_GAP * xAxisScale;
                bar.setSignature(note, ppq);
                break;
            case MIDI_TEMPO:
                cache->leadingTempo |= !note.startTime;
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                pos += TEMPO_WIDTH * xAxisScale;
                break;
            case TRACK_END:
#if DEBUG_POS
                if (debugVerbose) DEBUG("[%u] xPos %d start %d",
                        noteCache.note - &n.notes.front(),
                        noteCache.xPosition, note.stdStart(n.ppq));
#endif
                break;
            default:
                // to do : incomplete
                _schmickled();
        }
    }
    this->cacheSlurs();
    SCHMICKLE(notes.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes.notes.front());
}
