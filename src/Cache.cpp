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

int BarPosition::noteRegular(int noteDuration, PositionType noteTuplet, int ppq) {
    int result = NoteTakerDisplay::TiedCount(duration, noteDuration, ppq);
#if DEBUG_BAR
    if (debugVerbose) DEBUG("noteRegular result %d twoThirds %d noteDuration %d", result, twoThirds,
            PositionType::none != noteTuplet, noteDuration);
#endif
    if (1 == result || PositionType::none == noteTuplet) {
        return result;
    }
    return NoteTakerDisplay::TiedCount(duration, noteDuration * 3 / 2, ppq);
}

int BarPosition::notesTied(PositionType noteTuplet, int quantStart, int quantDuration, int ppq) {
    int inTsStartTime = quantStart - tsStart;
    int startBar = inTsStartTime / duration;
    int quantEndTime = quantStart + quantDuration;
    int inTsEndTime = quantEndTime - tsStart;  // end time relative to time signature start
    int endBar = inTsEndTime / duration;
#if DEBUG_BAR
    if (debugVerbose) DEBUG("notesTied inTsStartTime %d startBar %d inTsEndTime %d endBar %d",
            inTsStartTime, startBar, inTsEndTime, endBar);
#endif
    if (startBar == endBar) {
        leader = quantDuration;
        return this->noteRegular(leader, noteTuplet, ppq);
    }
    // startTime needs be relative to last bar start, not zero
    leader = (startBar + 1) * duration - inTsStartTime;
    SCHMICKLE(leader >= 0);
    int trailer = inTsEndTime - endBar * duration;
    SCHMICKLE(0 <= trailer);
    if (trailer >= duration) {
        if (debugVerbose) DEBUG("notesTied trailer %d duration %d inTsEndTime %d endBar %d inTsStartTime %d"
                " startBar %d leader %d noteTuplet %d quantStart %d quantDuration %d ppq %d",
                trailer, duration, inTsEndTime, endBar, inTsStartTime, startBar, leader,
                quantStart, quantDuration, ppq);
    }
    SCHMICKLE(trailer < duration);
#if DEBUG_BAR
    if (debugVerbose) DEBUG("notesTied leader %d trailer %d duration %d endBar %d startBar %d",
            leader, trailer, duration, endBar, startBar);
#endif
    int result = 0;
    if (leader >= NoteDurations::Smallest(ppq)) {
        result += this->noteRegular(leader, noteTuplet, ppq);
    }
    result += endBar - startBar - 1;  // # of bars with whole notes
    if (trailer >= NoteDurations::Smallest(ppq)) {
        result += this->noteRegular(trailer, noteTuplet, ppq);
    }
    return result;
}

// for x position: figure # of bars added by previous signature
int BarPosition::resetSignatureStart(int quantStartTime, float barWidth) {
    int result = 0;
    if (INT_MAX != duration) {
        result = (quantStartTime - tsStart + duration - 1) / duration * barWidth; // round up
    } else if (tsStart < quantStartTime) {
        result = barWidth;
    }
    tsStart = quantStartTime;
    return result;
}

// used by beams and tuplets
// to do : tuplet rules are different from beam rules
//         tuplet beam position should be above upper stave or below lower stave
void BeamPosition::set(const vector<NoteCache>& notes) {
    if (cacheFirst >= notes.size()) {
        DEBUG("%s cacheFirst %u > notes.size() %u", __func__, cacheFirst, notes.size());
    }
    if (cacheLast >= notes.size()) {
        DEBUG("%s cacheLast %u > notes.size() %u", __func__, cacheLast, notes.size());
    }
    unsigned index = cacheFirst;
    int chan = notes[cacheFirst].channel;
    float yMax = 0;
    float yMin = FLT_MAX;
    unsigned beamMax = 0;
    unsigned lastMeasured = cacheFirst;
    do {
        const NoteCache& noteCache = notes[index];
        if (chan != noteCache.channel) {
            continue;
        }
        yMax = std::max(noteCache.yPosition, yMax);
        yMin = std::min(noteCache.yPosition, yMin);
        beamMax = std::max((unsigned) noteCache.beamCount, beamMax);
        beamMin = std::min((unsigned) noteCache.beamCount, beamMin);
        lastMeasured = index;
    } while (++index <= cacheLast);
    bool stemUp = StemType::up == notes[cacheFirst].stemDirection;
    xOffset = stemUp ? 6 : -0.25;
    yOffset = stemUp ? 5 : -5;
    sx = notes[cacheFirst].xPosition + xOffset;
    ex = notes[cacheLast].xPosition + xOffset;
    int yStem = stemUp ? -22 : 15;
    yStemExtend = yStem + (stemUp ? 0 : 3);
    y = (stemUp ? yMin : yMax) + yStem;
    if (beamMax > 3) {
        y -= ((int) beamMax - 3) * yOffset;
    }
    yLimit = y + (stemUp ? 0 : 3);
    if (stemUp) {
        int highFirstLastY = std::max(notes[cacheFirst].yPosition, notes[lastMeasured].yPosition);
        slurOffset = std::max(0.f, yMax - highFirstLastY);
    } else {
        int lowFirstLastY = std::min(notes[cacheFirst].yPosition, notes[lastMeasured].yPosition);
        slurOffset = std::min(0.f, yMin - lowFirstLastY) - 3;
    }
}

CacheBuilder::CacheBuilder(const DisplayState& sd, Notes* n, DisplayCache* c) 
    : state(sd)
    , notes(n)
    , cache(c) {
}

// if note cache is in new measure, close previous beam if any and start a new one
void CacheBuilder::cacheBeams() {
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    vector<NoteCache>& notes = cache->notes;
    int startBar = -1;
    int ppq = this->notes->ppq;
    for (unsigned cacheIndex = 0; cacheIndex < notes.size(); ++cacheIndex) {
        NoteCache& noteCache = notes[cacheIndex];
        const DisplayNote& note = *noteCache.note;
        if (NOTE_ON != note.type) {
            if (NOTE_OFF == note.type) {
                continue;
            }
            for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
                if (255 != noteCache.channel && chan != noteCache.channel) {
                    continue;
                }
                this->closeBeam(&beamStarts[chan], cacheIndex);
            }
            continue;
        }
        if (noteCache.chord) {
#if DEBUG_BEAM
            if (debugVerbose) DEBUG("cacheBeams no staff %s note %s",
                    noteCache.debugString().c_str(), 
                    noteCache.note->debugString().c_str());
#endif
            continue;
        }
        unsigned& beamStart = beamStarts[noteCache.channel];
        bool checkForStart = true;
        int shownDuration = noteCache.vDuration;
        if (PositionType::none != noteCache.tripletPosition) {
            shownDuration = shownDuration * 3 / 2;
        }
        if (INT_MAX != beamStart) {
            bool stemsMatch = notes[beamStart].stemDirection == noteCache.stemDirection
                    && (notes[beamStart].pitchPosition <= MIDDLE_C)
                    == (noteCache.pitchPosition <= MIDDLE_C);
            if (!stemsMatch || shownDuration >= ppq || startBar != noteCache.bar) {
                this->closeBeam(&beamStart, cacheIndex);
            } else {
                if (!noteCache.endsOnBeat || PositionType::right == noteCache.tripletPosition) {
                    this->closeBeam(&beamStart, cacheIndex);
                } else {
                    noteCache.beamPosition = PositionType::mid;
                }
                checkForStart = false;
            }
        } 
#if DEBUG_BEAM
        if (debugVerbose) DEBUG("cacheBeams checkForStart %d shownDuration %d ppq %d",
                checkForStart, shownDuration, ppq);
#endif
        if (checkForStart && shownDuration < ppq) {
            // to do : if 4:4 time, allow beaming up to half note ?
            beamStart = cacheIndex;
            noteCache.beamPosition = PositionType::left;
            startBar = noteCache.bar;
#if DEBUG_BEAM
            if (debugVerbose) DEBUG("cacheBeams beamStart %s", noteCache.debugString().c_str());
#endif
        }
        if (INT_MAX != beamStart || PositionType::right == noteCache.beamPosition) {
            noteCache.beamCount = NoteDurations::Beams(NoteDurations::FromMidi(shownDuration, ppq));
        }
    }
    for (auto beamStart : beamStarts) {
#if DEBUG_BEAM
        if (INT_MAX != beamStart && debugVerbose) DEBUG("cacheBeams beam unmatched %s",
                notes[beamStart].note->debugString().c_str());
#endif
        this->closeBeam(&beamStart, notes.size());
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
            if (NOTE_OFF == note->type) {
                continue;
            }
            if (REST_TYPE == note->type) {
                this->closeSlur(&slurStarts[noteCache.channel], cacheIndex);
            } else {
                for (auto& slurStart : slurStarts) {
                    this->closeSlur(&slurStart, cacheIndex);
                }
            }
            continue;
        }
        if (noteCache.chord) {
            continue;
        }
        unsigned& slurStart = slurStarts[noteCache.channel];
        bool checkForStart = true;
#if DEBUG_SLUR
        if (debugVerbose) DEBUG("%s slur start %d cache [%d] %s note [%d] %s", __func__, slurStart,
                cacheIndex, noteCache.debugString().c_str(), note - &this->notes->notes.front(),
                note->debugString().c_str());
#endif
        if (INT_MAX != slurStart) {
            if (!note->slurEnd()) {
                this->closeSlur(&slurStart, cacheIndex);
            } else {
                auto& prior = notes[slurStart];
                if (prior.note->pitch() == noteCache.note->pitch()) {
                    if (PositionType::none == prior.tiePosition) {
                        prior.slurPosition = PositionType::none;
                        prior.tiePosition = PositionType::left;
                    }
                    noteCache.tiePosition = PositionType::mid;
                } else {
                    noteCache.slurPosition = PositionType::mid;
                }
                checkForStart = false;
#if DEBUG_SLUR
                if (debugVerbose) DEBUG("set slur mid %d/%d %s", cacheIndex,
                        note - &this->notes->notes.front(),
                        note->debugString().c_str());
#endif
            }
        }
        // assume slur for new start ; change to tie in logic above on next note
        if (checkForStart && note->slurStart()
                && PositionType::none == noteCache.slurPosition
                && PositionType::none == noteCache.tiePosition) {
            slurStart = cacheIndex;
            noteCache.slurPosition = PositionType::left;
#if DEBUG_SLUR
            if (debugVerbose) DEBUG("set slur left %d/%d %s", cacheIndex,
                    note - &this->notes->notes.front(),
                    note->debugString().c_str());
#endif
        }
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
            SCHMICKLE(NOTE_OFF != next->note->type);
            if (NOTE_ON != next->note->type) {
                break;
            }
            if (next->vStartTime != first->vStartTime) {
                break;
            }
            if (duration != next->vDuration) {
                continue;
            }
            next->chord = true;
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
            first->drawStaff = true;  // if note is most extreme on stave, draw Staff
            first->stemDirection = stem_up(first->pitchPosition);
            // to do : more sophisticated tie breaker
            if (StemType::either == first->stemDirection) {
                first->stemDirection = StemType::down;
            }
            continue;
        }
        // notehead furthest away from center of stave determines stem direction (BB p47)
        // to do : start here : if multiple groups of notes with the same start time have
        //                      different durations, group them together
        //                      If they overlap, offset in x
        StemType trebleStemDirection = (TREBLE_MID - highestTreble) >= (lowestTreble - TREBLE_MID) ?
                StemType::down : StemType::up;
        StemType bassStemDirection = (BASS_MID - highestBass) >= (lowestBass - BASS_MID) ?
                StemType::down : StemType::up;
        NoteCache* trebleExtreme = nullptr;
        NoteCache* bassExtreme = nullptr;
        for (NoteCache* entry = first; entry < next; ++entry) {
            if (StemType::unknown != entry->stemDirection) {
                continue;
            }
            if (duration != entry->vDuration) {
                continue;
            }
            bool trebleClef = entry->pitchPosition < MIDDLE_C
                    || (entry->pitchPosition == MIDDLE_C && trebleChord > 1);
            entry->stemDirection = trebleClef ? trebleStemDirection : bassStemDirection;
            // if note is most extreme on stave, set drawStaff true 
            NoteCache*& extreme = trebleClef ? trebleExtreme : bassExtreme;
            if (!extreme || trebleClef == (extreme->pitchPosition > entry->pitchPosition)) {
                if (extreme) {
                    extreme->drawStaff = false;
                }
                extreme = entry;
                extreme->drawStaff = true;
            }
        }
    }
}

// map beam position note first/last to cache indices first/last
// requires that cache entries sort order match note entries sort order (and beam pos sort order)
void CacheBuilder::cacheTuplets() {
#if DEBUG_TRIPLET_DRAW
    //  note when beams are actually out of order and need resorting
    unsigned debugLast = 0;
    for (auto& b : cache->beams) {
        if (debugLast >= b.noteFirst) {
            DEBUG("%s last %u next %u", __func__, debugLast, b.noteFirst);
        }
        debugLast = b.noteFirst;
    }
    cache->validateNotes(notes);
#endif
    if (!cache->beams.size()) {
        return;
    }
    array<TripletCandidate, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(TripletCandidate());
    array<NoteCache* , CHANNEL_COUNT> lastCache;
    lastCache.fill(nullptr);
    array<unsigned , CHANNEL_COUNT> beamIds;
    beamIds.fill(INT_MAX);
    vector<BeamPosition* > beamPtrs;
    beamPtrs.reserve(cache->beams.size());
    for (auto& b : cache->beams) {
        beamPtrs.push_back(&b);
    }
    std::sort(beamPtrs.begin(), beamPtrs.end(),
            [](const BeamPosition* left, const BeamPosition* right) {
        return left->noteFirst < right->noteFirst;
    });
    // sort beam positions
    auto beamPtr = &beamPtrs.front();
    for (unsigned cacheIndex = 0; cacheIndex < cache->notes.size(); ++cacheIndex) {
        auto& entry = cache->notes[cacheIndex];
        unsigned noteIndex = entry.note - &notes->notes.front();
        entry.tripletPosition = entry.note->triplet;
        auto beam = *beamPtr;
#if DEBUG_TRIPLET_DRAW
        DEBUG("%s %s\n beam %s", __func__,
                entry.note->debugString(notes->notes,
                &cache->notes, &entry).c_str(), beam->debugString().c_str());
#endif
        if (PositionType::none == entry.tripletPosition) {
            continue;
        }
        int chan = entry.channel;
        if (PositionType::left == entry.tripletPosition) {
            while (!beam->outsideStaff || beam->noteFirst < noteIndex) {
                if (INT_MAX == beam->noteFirst || INT_MAX == beam->noteLast) {
                    DEBUG("%s %s cacheIndex %u noteIndex %u", __func__, beam->debugString().c_str(),
                            cacheIndex, noteIndex);
                }
                SCHMICKLE(INT_MAX != beam->noteFirst);
                SCHMICKLE(INT_MAX != beam->noteLast);
                beam = *++beamPtr;
                SCHMICKLE(beamPtr <= &beamPtrs.back());
            }
            if (beam->noteFirst != noteIndex) {
                DEBUG("%s beam->noteFirst %u cache: [%u] %s note: [%u] %s", __func__,
                        beam->noteFirst, cacheIndex,
                        entry.debugString().c_str(), noteIndex, entry.note->debugString().c_str());
                DEBUG("%s beamPtr %s", __func__, (*beamPtr)->debugString().c_str());
                DEBUG("%s beamPtr[1] %s", __func__, (*beamPtr)[1].debugString().c_str());
            }
            SCHMICKLE(beam->noteFirst == noteIndex);
#if DEBUG_TRIPLET_DRAW
            DEBUG("%s chan %u beam [%u] %s", __func__, chan, beam - &cache->beams.front(),
                    beam->debugString().c_str());
#endif
            if (INT_MAX == beam->cacheFirst) {
                beam->cacheFirst = cacheIndex;
                beamIds[chan] = beam - &cache->beams.front();
            }
#if DEBUG_TRIPLET_DRAW
            DEBUG("%s %s\n  beam %s", __func__,
                    entry.note->debugString(notes->notes,
                    &cache->notes, &entry).c_str(), beam->debugString().c_str());
#endif
        } else if (PositionType::right == entry.tripletPosition) {
            if (INT_MAX == beamIds[chan]) {
                DEBUG("%s %s chan %u", __func__, entry.note->debugString(notes->notes,
                        &cache->notes, &entry).c_str(), chan);
            }
            SCHMICKLE(INT_MAX != beamIds[chan]);
            auto rightBeamPtr = &cache->beams[beamIds[chan]];
            if (rightBeamPtr->noteLast != noteIndex) {
                DEBUG("%s rightBeamPtr %s", __func__, rightBeamPtr->debugString().c_str());
                DEBUG("%s %s tripletId %u tuplet %u/%u", __func__, entry.note->debugString(notes->notes,
                        &cache->notes, &entry).c_str(), beamIds[chan],
                        notes->notes[rightBeamPtr->noteFirst].triplet,
                        notes->notes[rightBeamPtr->noteLast].triplet);
            }
            SCHMICKLE(rightBeamPtr->noteLast == noteIndex);
            rightBeamPtr->cacheLast = cacheIndex;
#if DEBUG_TRIPLET_DRAW
            DEBUG("%s rightBeamPtr %s id %u chan %u", __func__, rightBeamPtr->debugString().c_str(),
                    beamIds[chan], chan);
#endif
        }
        if (INT_MAX != beamIds[chan]) {
            entry.tripletId = beamIds[chan];
        }
        if (noteIndex == tripStarts[chan].lastIndex) {
            if (PositionType::left == entry.tripletPosition) {
                entry.tripletPosition = PositionType::mid;
            } else if (PositionType::right == entry.tripletPosition) {
                SCHMICKLE(lastCache[chan]);
                lastCache[chan]->tripletPosition = PositionType::mid;
            }
        }
        lastCache[chan] = &entry;
        tripStarts[chan].lastIndex = noteIndex;
    }
}

void CacheBuilder::closeBeam(unsigned* firstPtr, unsigned limit) {
    unsigned first = *firstPtr;
    if (INT_MAX == first) {
        return;
    }
    *firstPtr = INT_MAX;
    vector<NoteCache>& notes = cache->notes;
    SCHMICKLE(PositionType::left == notes[first].beamPosition);
    int chan = notes[first].channel;
    unsigned beamId = notes[first].beamId = cache->beams.size();
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (notes[index].chord) {
            continue;
        }
        if (chan != notes[index].channel) {
            continue;
        }
        notes[index].beamId = beamId;
        if (PositionType::mid != notes[index].beamPosition) {
            break;
        }
        last = index;
    }
    if (last != first) {
        notes[last].beamPosition = PositionType::right;
        cache->beams.emplace_back(first, last);
    } else {
        notes[first].beamId = INT_MAX;
        notes[first].beamPosition = PositionType::none;
    }
#if DEBUG_BEAM
    if (debugVerbose) DEBUG("close beam first %d last %d pos %u", first, last, notes[last].beamPosition);
#endif
}

// to do : debug and assert if closed slur does not have a left and right
void CacheBuilder::closeSlur(unsigned* firstPtr, unsigned limit) {
    unsigned first = *firstPtr;
    if (INT_MAX == first) {
        return;
    }
    *firstPtr = INT_MAX;
    vector<NoteCache>& noteCache = cache->notes;
#if DEBUG_SLUR
    if (debugVerbose) DEBUG("%s cache [%d] %s note [%d] %s", __func__,
            &noteCache[first] - &cache->notes.front(), noteCache[first].debugString().c_str(),
            noteCache[first].note - &this->notes->notes.front(),
            noteCache[first].note->debugString().c_str());
#endif
    SCHMICKLE(PositionType::left == noteCache[first].slurPosition
            || PositionType::left == noteCache[first].tiePosition);
    int chan = noteCache[first].channel;
    unsigned tieId = noteCache[first].tieId = cache->beams.size();
    unsigned last = first;
    unsigned index = first;
    while (++index < limit) {
        if (noteCache[index].chord) {
            continue;
        }
        if (chan == noteCache[index].channel) {
            noteCache[index].tieId = tieId;
            if (PositionType::mid != noteCache[index].slurPosition
                    && PositionType::mid != noteCache[index].tiePosition) {
                break;
            }
            last = index;
        }
    }
    if (last != first) {
        if (PositionType::left == noteCache[first].slurPosition) {
            noteCache[last].slurPosition = PositionType::right;
        } else {
            noteCache[last].tiePosition = PositionType::right;
        }
        cache->beams.emplace_back(first, last);
    } else {
#if DEBUG_SLUR
         if (debugVerbose) DEBUG("%s unmatched tieId %d cache [%d] %s note [%d] %s", __func__,
                first, &noteCache[first] - &cache->notes.front(),
                noteCache[first].debugString().c_str(),
                noteCache[first].note - &this->notes->notes.front(),
                noteCache[first].note->debugString().c_str());
#endif
        noteCache[index].tieId = INT_MAX;
        noteCache[last].slurPosition = PositionType::none;
        noteCache[last].tiePosition = PositionType::none;
    }
}


// compute ties first, so we know how many notes to draw
// to do : use cache notes' bar count instead of recomputing the bar count
void CacheBuilder::setDurations() {
    const StaffNote* pitchMap = sharpMap;
    BarPosition bar;
#if DEBUG_DURATIONS
    if (debugVerbose) DEBUG("setDurations %u ", notes->notes.size());
#endif
    int ppq = notes->ppq;
    for (unsigned index = 0; index < notes->notes.size(); ++index) {
        DisplayNote& note = notes->notes[index];
        if (NOTE_OFF == note.type) {
            SCHMICKLE(!note.cache);
            continue;
        }
        if (REST_TYPE == note.type) {
            if (PositionType::none == note.triplet && !note.isSelectable(state.selectedChannels)) {
                SCHMICKLE(!note.cache);
                continue;
            }
            
        }
        cache->notes.emplace_back(&note);
        NoteCache& cacheEntry = cache->notes.back();
        cacheEntry.bar = bar.count(cacheEntry);
        int quantStart = NoteCache::Quantize(note.startTime);
        if (!note.isNoteOrRest()) {
            if (TIME_SIGNATURE == note.type) {
                bar.setSignature(note, ppq);
            } else if (note.isSignature()) {
                bar.tsStart = quantStart;
                if (KEY_SIGNATURE == note.type) {
                    pitchMap = note.key() >= 0 ? sharpMap : flatMap;
                }
            }
            continue;
        }
        uint8_t pitchPosition = NOTE_ON == note.type ? pitchMap[note.pitch()].position : 0;
        int quantDuration = NoteCache::Quantize(note.duration);
        // if note is 2/3rds of a regular duration, it may be part of a third; defer tie in case
        int notesTied = bar.notesTied(notes->notes[index].triplet, quantStart, quantDuration, ppq);
        if (1 == notesTied) {
            cacheEntry.vDuration = quantDuration;
#if DEBUG_DURATIONS
            if (debugVerbose) DEBUG("%s vDur %d note %s", __func__, quantDuration,
                    note.debugString(notes->notes, &cache->notes, &cacheEntry).c_str());
#endif
            cacheEntry.endsOnBeat = (bool) (note.endTime() % ppq);
            if (NOTE_ON == note.type) {
                cacheEntry.accidentalSpace = true;
                cacheEntry.pitchPosition = pitchPosition;
                cacheEntry.yPosition = NoteTakerDisplay::YPos(pitchPosition);
            }
        } else {
            int duration = bar.leader;
            int remaining = quantDuration - bar.leader;
            int tieTime = quantStart;
#if DEBUG_DURATIONS
            if (debugVerbose) DEBUG("duration %d remaining %d tieTime %d added %s",
                    duration, remaining, tieTime, note.debugString().c_str());
#endif
            bool accidentalSpace = NOTE_ON == note.type;
            cache->notes.pop_back();
            do {
                bool twoThirds = PositionType::none != notes->notes[index].triplet;
                do {
                    cache->notes.emplace_back(&note);
                    NoteCache& tiePart = cache->notes.back();
                    tiePart.vStartTime = tieTime;
                    tiePart.bar = bar.count(tiePart);
                    tiePart.tiePosition = accidentalSpace ? PositionType::left : PositionType::mid;
                    // if in a triplet, scale up by 3/2, look up note, scale result by 2/3
                    int tiePartDuration = NoteDurations::LtOrEq(duration, ppq, twoThirds);
                    tiePart.vDuration = tiePartDuration;
#if DEBUG_DURATIONS
                    if (debugVerbose) {
                        DEBUG("vStart %d vDuration %d duration %d [%d] cache [%d] note",
                                tiePart.vStartTime, tiePart.vDuration, duration,
                                &tiePart - &cache->notes.front(), tiePart.note - &notes->notes.front());
                    }
#endif
                    tieTime += tiePartDuration;
                    tiePart.endsOnBeat = (bool) (tieTime % ppq);
                    tiePart.accidentalSpace = accidentalSpace;
                    accidentalSpace = false;
                    duration -= tiePartDuration;
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
#if 1
    // don't use std::sort function; use insertion sort to minimize reordering
    for (auto it = cache->notes.begin(), end = cache->notes.end(); it != end; ++it) {
        auto const insertion_point = std::upper_bound(cache->notes.begin(), it, *it);
        std::rotate(insertion_point, it, it + 1);
    }
#else
    std::sort(cache->notes.begin(), cache->notes.end());
#endif
    cache->validateNotes(notes);
    cache->notes.shrink_to_fit();
    // sort, shrink, vector push back may move cache locations, so set them up last
    // walk backwards to put point note at first cache entry if note has more than one
    for (auto riter = cache->notes.rbegin(); riter != cache->notes.rend(); ++riter) {
        const_cast<DisplayNote*>(riter->note)->cache = &*riter;
    }
    // check to see if things moved
    SCHMICKLE(notes->notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes->notes.front());
#if DEBUG_DURATIONS
    if (debugVerbose) DEBUG("finished set durations");
    if (debugVerbose) {
        for (const auto& note : notes->notes) {
            DEBUG("%s by note %s", __func__, note.debugString(notes->notes, &cache->notes,
                    nullptr).c_str());
        }
        for (const auto& note : cache->notes) {
            DEBUG("%s by cache %s", __func__, note.note->debugString(notes->notes,
                    &cache->notes, &note).c_str());
        }
    }
#endif
    cache->validateNotes(notes);
}

void CacheBuilder::trackPos(std::list<PosAdjust>& posAdjust, float xOff, int endTime) {
    if (xOff > 0) {
        // keep the largest offset at each time
        for (auto& adjust : posAdjust) {
            if (adjust.time == endTime) {
                adjust.x = std::max(adjust.x, xOff);
                return;
            }
        }
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
    cache->beams.clear();
    int ppq = this->notes->ppq;
    // to do : what if ppq is weirdly small?
    int third = ppq / 3;
    // to do : only triplets for now / ppq must be power of 2 * 3
    bool testForTriplets = third * 3 == ppq && !(third & (third - 1));  // check if only top bit set
    if (testForTriplets) {
        notes->findTriplets(cache);
    }
    cache->notes.clear();
    cache->notes.reserve(notes->notes.size());  // just a guess
    this->setDurations();  // adds cache per tied note part, sets its duration and note index
    // to do : If a pair of notes landed at the same place at the same time, but have different
    //         pitches, flip the notes' accidentals to move them to different staff fines.
    //       : Next, figure out how many horizontal positions are required to show non-overlapping
    //         notes.
    this->cacheStaff();  // set staff flag if note owns shared staff
    SCHMICKLE(notes->notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes->notes.front());
    if (testForTriplets) {  
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
#if DEBUG_POS
            if (debugVerbose) DEBUG("adjust xOff %g endTime %d", adjust.x, adjust.time);
#endif
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
        if (debugVerbose) DEBUG("vStartTime %d ppq %d bars %d xPos %d pos %g",
                noteCache. vStartTime, ppq, bars, noteCache.xPosition, pos);
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
                pos += bar.resetSignatureStart(noteCache.vStartTime, BAR_WIDTH * xAxisScale);
                // to do : measure key sig chars instead
                pos += std::max(MIN_KEY_SIGNATURE_WIDTH * xAxisScale, 
                        abs(note.key()) * ACCIDENTAL_WIDTH * xAxisScale + 2);
// I don't think building the cache is impacted by the key signature, but leave this as a reminder..
//                this->setKeySignature(note.key());
                break;
            case TIME_SIGNATURE:
                pos += bar.resetSignatureStart(noteCache.vStartTime, BAR_WIDTH * xAxisScale);
                pos += NoteTakerDisplay::TimeSignatureWidth(note, vg, state.musicFont);
                pos += TIME_SIGNATURE_GAP * xAxisScale;
                bar.setSignature(note, ppq);
                break;
            case MIDI_TEMPO:
                cache->leadingTempo |= !noteCache.vStartTime;
                pos += bar.resetSignatureStart(noteCache.vStartTime, BAR_WIDTH * xAxisScale);
                pos += TEMPO_WIDTH * xAxisScale;
                break;
            case TRACK_END:
#if DEBUG_POS
                if (debugVerbose) DEBUG("[%u] xPos %d start %d",
                        noteCache.note - &notes->notes.front(),
                        noteCache.xPosition, note.stdStart(ppq));
#endif
                break;
            default:
                // to do : incomplete
                _schmickled();
        }
    }
    this->cacheSlurs();
#if DEBUG_CACHE
    if (debugVerbose) {
        Notes::DebugDump(notes->notes, 0, notes->notes.size() - 1, &cache->notes);
    }
#endif
#if DEBUG_TRIPLET_DRAW
    unsigned index = 0;
#endif
    for (auto& beam : cache->beams) {
#if DEBUG_TRIPLET_DRAW
        if (debugVerbose) {
            DEBUG("%s [%u] %s", __func__, index++, beam.debugString().c_str());
        }
#endif
        beam.set(cache->notes);
    }
    SCHMICKLE(notes->notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &notes->notes.front());
}
