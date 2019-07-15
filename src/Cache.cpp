#include "Cache.hpp"
#include "Display.hpp"
#include "Notes.hpp"
#include "Widget.hpp"

BarPosition::BarPosition(bool dbug)
    : debugVerbose(dbug) {
    pos.clear();
    priorBars = 0;
    duration = INT_MAX;
    midiEnd = INT_MAX;
    tsStart = INT_MAX;
}

void BarPosition::addPos(const NoteCache& noteCache, float cacheWidth) {
    bool hasDuration = INT_MAX != duration;
    int barCount = hasDuration ? (noteCache.vStartTime - tsStart) / duration + priorBars : 0;
    pos[barCount].xMin = std::min(pos[barCount].xMin, (float) noteCache.xPosition);
    if (false && debugVerbose) DEBUG("addPos [%d] xMin %g xPos %d",
            barCount, pos[barCount].xMin, noteCache.xPosition);
    // note the adds duration less one to round up
    barCount = hasDuration ? 
            (noteCache.vEndTime() - tsStart + duration - 1) / duration + priorBars : 1;  // rounds up
    pos[barCount].xMax = std::max(pos[barCount].xMax, noteCache.xPosition + cacheWidth);
    if (false && debugVerbose) DEBUG("addPos [%d] xMax %g xPos %d width %g",
            barCount, pos[barCount].xMax, noteCache.xPosition, cacheWidth);
}

int BarPosition::notesTied(const DisplayNote& note, int ppq) {
    if (INT_MAX == duration) {
        leader = note.duration;
        return NoteTakerDisplay::TiedCount(duration, note.duration, ppq);
    }
    int inTsStartTime = note.startTime - tsStart;
    int startBar = inTsStartTime / duration;
    int inTsEndTime = note.endTime() - tsStart;  // end time relative to time signature start
    int endBar = inTsEndTime / duration;
    if (startBar == endBar) {
        return 1;
    }
    // note.startTime needs be relative to last bar start, not zero
    leader = (startBar + 1) * duration - inTsStartTime;
    SCHMICKLE(leader >= 0);
    int trailer = inTsEndTime - endBar * duration;
    SCHMICKLE(0 <= trailer);
    if (trailer >= duration) {
        if (debugVerbose) DEBUG("notesTied trailer %d duration %d inTsEndTime %d endBar %d inTsStartTime %d"
                " startBar %d leader %d",
                trailer, duration, inTsEndTime, endBar, inTsStartTime, startBar, leader);
        if (debugVerbose) DEBUG("note %s", note.debugString().c_str());
    }
    SCHMICKLE(trailer < duration);
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

CacheBuilder::CacheBuilder(const DisplayState& sd, DisplayCache* c) 
    : state(sd)
    , cache(c)
    , debugVerbose(sd.debugVerbose) {
}

void CacheBuilder::cacheBeams() {
    array<unsigned, CHANNEL_COUNT> beamStarts;
    beamStarts.fill(INT_MAX);
    vector<NoteCache>& notes = cache->notes;
    for (unsigned cacheIndex = 0; cacheIndex < notes.size(); ++cacheIndex) {
        NoteCache& noteCache = notes[cacheIndex];
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
        if (!noteCache.staff) {
            continue;
        }
        unsigned& beamStart = beamStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != beamStart) {
            bool stemsMatch = notes[beamStart].stemUp == noteCache.stemUp &&
                    (notes[beamStart].pitchPosition <= MIDDLE_C)
                    == (noteCache.pitchPosition <= MIDDLE_C);
            if (!stemsMatch || noteCache.vDuration >= ppq) {
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
        if (checkForStart && noteCache.vDuration < ppq) {
            // to do : if 4:4 time, allow beaming up to half note ?
            beamStart = cacheIndex;
            noteCache.beamPosition = PositionType::left;
        }
        if (INT_MAX != beamStart || PositionType::right == noteCache.beamPosition) {
            noteCache.beamCount = NoteDurations::Beams(
                    NoteDurations::FromMidi(noteCache.vDuration, ppq));
        }
    }
    for (auto beamStart : beamStarts) {
        if (INT_MAX != beamStart) {
            this->closeBeam(beamStart, notes.size());
            if (debugVerbose) DEBUG("beam unmatched %s", notes[beamStart].note->debugString().c_str());
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
        if (NOTE_ON != note->type) {
            continue;
        }
        if (!noteCache.staff) {
            continue;
        }
        unsigned& slurStart = slurStarts[noteCache.channel];
        bool checkForStart = true;
        if (INT_MAX != slurStart) {
            if (!note->slur()) {
                this->closeSlur(slurStart, cacheIndex);
                if (debugVerbose) DEBUG("set slur close %s", notes[slurStart].note->debugString().c_str());
                slurStart = INT_MAX;
            } else {
                noteCache.slurPosition = PositionType::mid;
                checkForStart = false;
                if (debugVerbose) DEBUG("set slur mid %s", note->debugString().c_str());
            }
        }
        if (checkForStart && note->slur()) {
            slurStart = cacheIndex;
            if (PositionType::none == noteCache.slurPosition) {
                noteCache.slurPosition = PositionType::left;
            }
            if (debugVerbose) DEBUG("set slur left %s", note->debugString().c_str());
        }
    }
    for (auto slurStart : slurStarts) {
        if (INT_MAX != slurStart) {
            this->closeSlur(slurStart, notes.size());
            if (debugVerbose) DEBUG("slur unmatched %s", notes[slurStart].note->debugString().c_str());
        }
    }
}

// to do : add new code to horizontally shift notes with different durations?
// to do : if quarter and eighth share staff, set stem up in different directions?
// collect the notes with the same start time and duration
// mark only one as the staff owner
void CacheBuilder::cacheStaff() {
    NoteCache* next = nullptr;
    vector<NoteCache>& notes = cache->notes;
    for (NoteCache* last = &notes.front(); last <= &notes.back(); last = next) {
        auto first = last;
        next = last;
        while (++next <= &notes.back()) {
            if (NOTE_ON != first->note->type || NOTE_ON != next->note->type) {
                break;
            }
            if (next->vStartTime != first->vStartTime) {
                break;
            }
            if (next->vDuration != first->vDuration) {
                break;
            }
            // to do : if notes are on the same or adjacent lines, slide one over
            SCHMICKLE(last->pitchPosition <= next->pitchPosition);
            // how far apart are the notes? if close, use one staff, align stem up
            if (last->pitchPosition + 6 <= next->pitchPosition) {
                break;
            }
            // but don't tie from below middle C to above middle C (score line 39)
            if (MIDDLE_C > first->pitchPosition && MIDDLE_C < next->pitchPosition) {
                break;
            }
            last = next;
        }
        if (first + 1 == next) {
            first->staff = true;
            continue;
        } 
        last = first;
        if (MIDDLE_C <= last->pitchPosition) {
            if (MIDDLE_C == last->pitchPosition) {
                last->stemUp = false;
            }
            bool stemUp = last->stemUp;
            last->staff = stemUp;
            next[-1].staff = !stemUp;
            while (++last < next) {
                last->stemUp = stemUp;
            }
            continue;
        }
        last = next - 1;
    // to do : remove or mark as debugging only
        if (MIDDLE_C < last->pitchPosition) {
            for (NoteCache* test = first; test < next; ++test) {
                DEBUG("[%d] %d pitchPos %d stemUp %d staff %d", test->channel, test->vStartTime, 
                        test->pitchPosition, test->stemUp, test->staff);
            }
        }
        SCHMICKLE(MIDDLE_C >= last->pitchPosition);
        bool stemUp = last->stemUp;
        last->staff = !stemUp;
        first->staff = stemUp;
        while (--last >= first) {
            last->stemUp = stemUp;
        }
    }
}

// only mark triplets if notes are the right duration and collectively a multiple of three
void CacheBuilder::cacheTuplets(const Notes& n) {
    array<unsigned, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(INT_MAX);
    int tupletId = 1;
    for (unsigned index = 0; index < n.notes.size(); ++index) {
        const DisplayNote& note = n.notes[index];
        if (!note.isNoteOrRest()) {
            tripStarts.fill(INT_MAX);
            continue;
        }
        if (!note.cache->staff) {
            continue;
        }
        unsigned& tripStart = tripStarts[note.channel];
        if (INT_MAX != tripStart) {
            const DisplayNote& start = n.notes[tripStart];
            int totalDuration = note.endTime() - start.startTime;
            if (totalDuration > n.ppq * 4) {
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
                            noteCache->tupletPosition = PositionType::mid;
                            noteCache->tupletId = tupletId;
                        }
                    }
                    last->tupletPosition = PositionType::right;
                    last->tupletId = tupletId;
                    if (!++tupletId) {
                        tupletId = 1;  // reserve zero to mean 'not part of a tuplet'
                    }
                    tripStart = INT_MAX;
                }
                continue;
            }
        }
        // to do : remove or mark as debugging only
        if (!NoteDurations::InStd(note.duration, n.ppq)) {
            DEBUG("note !InStd %s ppq %d", note.debugString().c_str(), n.ppq);
        }
        if (NoteDurations::TripletPart(note.duration, n.ppq)) {
            tripStart = index;
        }
    }
}

void CacheBuilder::clearTuplet(unsigned index, unsigned limit) {
    vector<NoteCache>& notes = cache->notes;
    SCHMICKLE(PositionType::left == notes[index].tupletPosition);
    const int tupletId = notes[index].tupletId;
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
    if (debugVerbose) DEBUG("close beam first %d last %d pos %u", first, last, notes[last].beamPosition);
}

void CacheBuilder::closeSlur(unsigned first, unsigned limit) {
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
    if (debugVerbose) DEBUG("close slur first %d last %d pos %u", first, last, notes[last].slurPosition);
}


// compute ties first, so we know how many notes to draw
void CacheBuilder::setDurations(const Notes& n) {
    const StaffNote* pitchMap = sharpMap;
    BarPosition bar(debugVerbose);
    for (unsigned index = 0; index < n.notes.size(); ++index) {
        const DisplayNote& note = n.notes[index];
        cache->notes.emplace_back(&note);
        NoteCache& cacheEntry = cache->notes.back();
        if (!note.isNoteOrRest()) {
            if (TIME_SIGNATURE == note.type) {
                bar.setSignature(note, n.ppq);
            } else if (note.isSignature()) {
                bar.tsStart = note.startTime;
                if (KEY_SIGNATURE == note.type) {
                    pitchMap = note.key() >= 0 ? sharpMap : flatMap;
                }
            }
            continue;
        }
        uint8_t pitchPosition = NOTE_ON == note.type ? pitchMap[note.pitch()].position : 0;
        int notesTied = bar.notesTied(note, n.ppq);
        if (1 == notesTied) {
            cacheEntry.vDuration = note.duration;
            cacheEntry.endsOnBeat = (bool) (note.endTime() % n.ppq);
            if (NOTE_ON == note.type) {
                cacheEntry.accidentalSpace = true;
                cacheEntry.pitchPosition = pitchPosition;
                cacheEntry.yPosition = NoteTakerDisplay::YPos(pitchPosition);
                cacheEntry.stemUp = NoteTakerDisplay::StemUp(pitchPosition);
            }
        } else {
            int duration = bar.leader;
            int remaining = note.duration - bar.leader;
            int tieTime = note.startTime;
            bool accidentalSpace = NOTE_ON == note.type;
            cache->notes.pop_back();
            do {
                do {
                    if (debugVerbose) DEBUG("add tie to %s", note.debugString().c_str());
                    cache->notes.emplace_back(&note);
                    NoteCache& tiePart = cache->notes.back();
                    tiePart.vStartTime = tieTime;
                    tiePart.tiePosition = accidentalSpace ? PositionType::left : PositionType::mid;
                    tiePart.vDuration = NoteDurations::LtOrEq(duration, n.ppq);
                    auto inStd = NoteDurations::InStd(duration, n.ppq);
                    if (debugVerbose) 
                            DEBUG("InStd %u FromMidi %u FromStd %u LtOrEq %d duration %d",
                            inStd,
                            NoteDurations::FromMidi(duration, n.ppq),
                            NoteDurations::FromStd(duration),
                            tiePart.vDuration, duration);
                    tieTime += tiePart.vDuration;
                    tiePart.endsOnBeat = (bool) (tieTime % n.ppq);
                    tiePart.accidentalSpace = accidentalSpace;
                    accidentalSpace = false;
                    duration -= tiePart.vDuration;
                    if (NOTE_ON == note.type) {
                        tiePart.pitchPosition = pitchPosition;
                        tiePart.yPosition = NoteTakerDisplay::YPos(pitchPosition);
                        tiePart.stemUp = NoteTakerDisplay::StemUp(pitchPosition);
                    }
                } while (duration >= NoteDurations::ToMidi(0, n.ppq));
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
    DisplayNote* last = nullptr;
    for (auto& entry : cache->notes) {
        if (last == entry.note) {
            continue;
        }
        last = const_cast<DisplayNote*>(entry.note);
        last->cache = &entry;
    }
    // check to see if things moved
    SCHMICKLE(n.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &n.notes.front());
    if (debugVerbose) DEBUG("finished setCacheDuration");
}

void CacheBuilder::trackPos(std::list<PosAdjust>& posAdjust, float xOff, int endTime) {
    if (xOff > 0) {
        PosAdjust adjust = { xOff, endTime };
        posAdjust.push_back(adjust);
        if (debugVerbose) DEBUG("xOff %g endTime %d", xOff, endTime);
    }
}

void CacheBuilder::updateXPosition(const Notes& n) {
    auto vg = state.vg;
    float xAxisScale = state.xAxisScale;
    SCHMICKLE(vg);
    nvgFontFaceId(vg, state.musicFont);
    nvgFontSize(vg, NOTE_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT);
    cache->notes.clear();
    cache->notes.reserve(n.notes.size());
    this->setDurations(n);  // adds cache per tied note part, sets its duration and note index
    this->cacheStaff();  // set staff flag if note owns shared staff
    SCHMICKLE(n.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &n.notes.front());
    if (!(n.ppq % 3)) {  // to do : only triplets for now
        this->cacheTuplets(n);
    }
    for (auto& noteCache : cache->notes) {
        noteCache.setDurationSymbol(n.ppq);
    }
    this->cacheBeams();
    float pos = 0;
    std::list<PosAdjust> posAdjust;
    cache->leadingTempo = false;
    BarPosition bar(debugVerbose);
    for (auto& noteCache : cache->notes) {
        PosAdjust nextAdjust = { 0, 0 };
        for (auto& adjust : posAdjust) {
            if (noteCache.vStartTime >= adjust.time && adjust.time > nextAdjust.time) {
                nextAdjust = adjust;
            }
        }
        if (nextAdjust.x) {
            if (debugVerbose) DEBUG("posAdjust x %g time %d", nextAdjust.x, nextAdjust.time);
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
        int bars = bar.priorBars + (INT_MAX == bar.duration ? 0 :
                (noteCache.vStartTime - bar.tsStart) / bar.duration);
        int stdStart = NoteDurations::InStd(noteCache.vStartTime, n.ppq);
        noteCache.xPosition = (int) std::ceil((stdStart + bars * BAR_WIDTH)
                * xAxisScale + pos);
        if (debugVerbose) DEBUG("vStartTime %d n.ppq %d bars %d xAxisScale %g xPos %d",
                noteCache.vStartTime, n.ppq, bars, xAxisScale, noteCache.xPosition);
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
        if (debugVerbose) DEBUG("%d [%d] stdStart %d bars %d pos %g accidental %d",
                &noteCache - &cache->notes.front(),
                noteCache.xPosition, stdStart, bars, pos, noteCache.accidentalSpace ? 8 : 0);
        const DisplayNote& note = *noteCache.note;
        switch (note.type) {
            case MIDI_HEADER:
                SCHMICKLE(!pos);
                pos = CLEF_WIDTH * xAxisScale;
                if (debugVerbose) DEBUG("header pos %g", pos);
                break;
            case NOTE_ON:
            case REST_TYPE: {
                    float drawWidth = NoteTakerDisplay::CacheWidth(noteCache, vg)
                            + (noteCache.accidentalSpace ? 8 : 0);
                    // to do : if note cache has no staff, it still needs to adjust draw width
                    if (drawWidth < 10 && PositionType::none != noteCache.beamPosition) {
                        drawWidth += 3; // make space for half beams
                    }
                    float xOff = drawWidth - NoteDurations::InStd(noteCache.vDuration, n.ppq)
                            * xAxisScale;
                    this->trackPos(posAdjust, xOff, noteCache.vEndTime());
                    if (debugVerbose) DEBUG("%d track_pos xOff %g cache start %d dur %d",
                            &noteCache - &cache->notes.front(),
                            xOff, noteCache.vStartTime, noteCache.vDuration);
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
                bar.setSignature(note, n.ppq);
                break;
            case MIDI_TEMPO:
                cache->leadingTempo |= !note.startTime;
                pos += bar.resetSignatureStart(note, BAR_WIDTH * xAxisScale);
                pos += TEMPO_WIDTH * xAxisScale;
                break;
            case TRACK_END:
                if (debugVerbose) DEBUG("[%u] xPos %d start %d",
                        noteCache.note - &n.notes.front(),
                        noteCache.xPosition, note.stdStart(n.ppq));
                break;
            default:
                // to do : incomplete
                _schmickled();
        }
    }
    this->cacheSlurs();
    SCHMICKLE(n.notes.front().cache == &cache->notes.front());
    SCHMICKLE(cache->notes.front().note == &n.notes.front());
}