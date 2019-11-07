#include "Notes.hpp"
#include "Channel.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "ParseMidi.hpp"
#include "Storage.hpp"

#define f_and_n(n) #n "\xE2" "\x99" "\xAD", #n  // flat followed by natural   (e.g., "Db", "D")
const char* flatNames[] = 
    { "C", f_and_n(D), f_and_n(E), "F", f_and_n(G), f_and_n(A), f_and_n(B) };
#undef f_and_n

#define n_and_s(n) #n, #n "\xE2" "\x99" "\xAF"  // natural followed by sharp  (e.g., "C", "C#")
const char* sharpNames[] =
    { n_and_s(C), n_and_s(D), "E", n_and_s(F), n_and_s(G), n_and_s(A), "B" };
#undef n_and_s

#define utf8_flat "\xE2" "\x99" "\xAD"
#define utf8_sharp "\xE2" "\x99" "\xAF"
const char* keyNames[] = { 
    "C" utf8_flat  " major", "A" utf8_flat  " minor",
    "G" utf8_flat  " major", "E" utf8_flat  " minor",
    "D" utf8_flat  " major", "B" utf8_flat  " minor",
    "A" utf8_flat  " major", "F"            " minor",
    "E" utf8_flat  " major", "C"            " minor",
    "B" utf8_flat  " major", "G"            " minor",
    "F"            " major", "D"            " minor",
    "C"            " major", "A"            " minor",
    "G"            " major", "E"            " minor",
    "D"            " major", "B"            " minor",
    "A"            " major", "F" utf8_sharp " minor",
    "E"            " major", "C" utf8_sharp " minor",
    "B"            " major", "G" utf8_sharp " minor",
    "F" utf8_sharp " major", "D" utf8_sharp " minor",
    "C" utf8_sharp " major", "A" utf8_sharp " minor",
        };  // 7 flats

#undef utf8_flat
#undef utf8_sharp

struct DurationName {
    const char* prefix;
    const char* body;
};

const std::array<DurationName, 20> durationNames = {{
    {"one ", "128th"},
    {"one ", "64th"},
    {"",  "three 128ths"},
    {"one ", "32nd"},
    {"",  "three 64ths"},
    {"one ", "16th"},
    {"",  "three 32nds"},
    {"one ", "8th"},
    {"",  "three 16ths"},
    {"one ", "quarter"},
    {"",  "three 8ths"},
    {"one ", "half"},
    {"",  "three quarters"},
    {"one ", "whole"},
    {"",  "one and a half whole"},
    {"",  "two whole"},
    {"",  "three whole"},
    {"",  "four whole"},
    {"",  "six whole"},
    {"",  "eight whole"}
}};

std::string Notes::FlatName(unsigned midiPitch) {
    SCHMICKLE(midiPitch < 128);
    unsigned note = midiPitch % 12;
    int octave = (int) midiPitch / 12;
    return std::string(flatNames[note] + std::to_string(octave - 1));
}

std::string Notes::FullName(int duration, int ppq) {
    unsigned index = NoteDurations::FromMidi(duration, ppq);
    SCHMICKLE(index < durationNames.size());
    return std::string(durationNames[index].prefix) + durationNames[index].body;
}

std::string Notes::KeyName(int key, int minor) {
    if (debugVerbose) DEBUG("KeyName key %d minor %d", key, minor); // to do : debug why sometimes out of ranage
    key = std::max(-7, std::min(7, key));
    minor = std::max(0, std::min(1, minor));
    return std::string(keyNames[(key + 7) * 2 + minor]);
}

std::string Notes::Name(const DisplayNote* note, int ppq) {
    unsigned index = NoteDurations::FromMidi(note->duration, ppq);
    SCHMICKLE(index < durationNames.size());
    if (false && debugVerbose) DEBUG("%s %s index %u name %s", __func__, note->debugString().c_str(), index,
            durationNames[index].body);
    return durationNames[index].body;
}

std::string Notes::SharpName(unsigned midiPitch) {
    SCHMICKLE(midiPitch < 128);
    unsigned note = midiPitch % 12;
    int octave = (int) midiPitch / 12;
    return std::string(sharpNames[note] + std::to_string(octave - 1));
}

std::string Notes::TSDenom(const DisplayNote* note, int ppq) {
    return "one " + TSUnit(note, 1, ppq) + " per beat";
}

std::string Notes::TSNumer(const DisplayNote* note, int ppq) {
    int numer = note->numerator();
    return std::to_string(numer) + " " + TSUnit(note, numer, ppq) + " per measure";
}

std::string Notes::TSUnit(const DisplayNote* note, int count, int ppq) {
    unsigned index = 13 - note->denominator() * 2;
    std::string result = durationNames[index].body;
    result += " note";
    if (1 != count) {
        result += "s";
    }
    return result;
}

// only mark triplets if notes are the right duration and collectively a multiple of three
// to do : if too few notes are of the right duration, they (probably) need to be drawn as ties --
// 2/3rds of 1/4 note is 1/6 (.167) which could be 1/8 + 1/32 (.156)
// This adds beam position records for each triplet and keep that index rather than trip id.
void Notes::findTriplets(DisplayCache* displayCache) {
#if DEBUG_TRIPLET_DRAW
    DEBUG("%s", __func__);
    auto noteTripletDebug = [=](const char* str, const DisplayNote& note) {
        DEBUG("findTriplets %s %s" , str, note.debugString(notes, nullptr, nullptr).c_str());
    };
#else
    auto noteTripletDebug = [](const char* , const DisplayNote& ) {};
#endif
    // if clearing one or all, clear tuplets as needed, but only if tuplet isn't closed
    auto clearTriplet = [=](TripletCandidate* candidate, unsigned channel) {
            if (candidate->startIndex >= notes.size()) {
                return;
            }
            if (PositionType::right != notes[candidate->lastIndex].triplet) {
                for (unsigned index = candidate->startIndex; index <= candidate->lastIndex; ++index) {
                    if (channel != notes[index].channel) {
                        continue;
                    }
                    notes[index].triplet = PositionType::none;
                }
            }
            *candidate = TripletCandidate();
    };
    vector<BeamPosition>* beams = &displayCache->beams;
    array<TripletCandidate, CHANNEL_COUNT> tripStarts;  // first index in notes (not cache) of triplet
    tripStarts.fill(TripletCandidate());
    for (unsigned index = 0; index < notes.size(); ++index) {
        DisplayNote& note = notes[index];
        if (!note.isNoteOrRest()) {
            noteTripletDebug("not note or rest", note);
            for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
                clearTriplet(&tripStarts[chan], chan);
            }
            continue;
        }
        auto& candidate = tripStarts[note.channel];
        if (!NoteDurations::TripletPart(note.duration, ppq)
                || NoteDurations::Dotted(note.duration, ppq)) {
            noteTripletDebug("not triplet part, or is dotted", note);
            clearTriplet(&candidate, note.channel);
            continue;
        }
        ;
        if (INT_MAX != candidate.lastIndex) {
            DisplayNote* last = &notes[candidate.lastIndex];
            // if chord notes overlap or are of different lengths, don't look for triplets
            if (last->startTime == note.startTime)  {
                if (last->duration != note.duration) {
                    noteTripletDebug("unequal duration", note);
                    clearTriplet(&candidate, note.channel);
                }
                noteTripletDebug("chord part", note);
                continue;
            } else {
                SCHMICKLE(last->startTime < note.startTime);
                if (last->endTime() > note.startTime) {
                    noteTripletDebug("overlapping", note);
                    clearTriplet(&candidate, note.channel);
                    continue;
                }
            }
        }
        candidate.lastIndex = index;
        candidate.atLeastOneNote |= NOTE_ON == note.type;
        if (INT_MAX == candidate.startIndex) {
            noteTripletDebug("triplet start", note);
            candidate.startIndex = index;
            continue;
        }
        if (!candidate.atLeastOneNote) {
            noteTripletDebug("not one note", note);
            continue;
        }
        DisplayNote* test = &notes[candidate.startIndex];
        int totalDuration = note.endTime() - test->startTime;
        if ((NoteDurations::InStd(totalDuration, ppq) % 3)) {
#if DEBUG_TRIPLET_DRAW
            DEBUG("%s total duration %d ppq %d", __func__, totalDuration, ppq);
            noteTripletDebug("last candidate note", note);
#endif
            continue;
        }
        // triplet in cache begins at can trip start time, ends at note end time
        notes[candidate.startIndex].triplet = PositionType::left;
        noteTripletDebug("left", *test);
        int chan = test->channel;
        DisplayNote* last = test;
        while (++test < &note) {
            if (test->channel != chan) {
                continue;
            }
            if (last->startTime >= test->startTime) {
                continue;
            }
            last = test;
            test->triplet = PositionType::mid;
            noteTripletDebug("mid", *test);
        }
        unsigned rightIndex = &note - &notes.front();
        note.triplet = PositionType::right;
        noteTripletDebug("right", note);
        // note indices are replaced with cache indices in cache builder cache tuplets
        beams->emplace_back(candidate.startIndex, rightIndex, true);
        candidate = TripletCandidate();
    }
    for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
        clearTriplet(&tripStarts[chan], chan);
    }
}

// cache may be null if note is rest and rest part is disabled
const NoteCache* Notes::lastCache(unsigned index) const {
    unsigned used = index;
    while (used && !notes[used].cache) {
        --used;
    }
    const NoteCache* result = notes[used].cache;
    if (index == used && result) {
        result -= 1;
    }
    SCHMICKLE(result);
    return result;
}

// does not change note duration: make note duration overlap by one only when saving to midi
// note that this allows slurs over tempo/key/time changes
void Notes::setSlurs(unsigned selectChannels, bool condition) {
#if DEBUG_SLUR
    if (debugVerbose) DEBUG("%s chans %08x cond %d", __func__, selectChannels, condition);
#endif
    array<DisplayNote*, CHANNEL_COUNT> last;
    last.fill(nullptr);
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (REST_TYPE == note.type) {
            last[note.channel] = nullptr;
            continue;
        }
        if (NOTE_ON != note.type) {
            last.fill(nullptr);
            continue;
        }
        auto lastOne = last[note.channel];
        if (lastOne) {
            if (lastOne->startTime >= note.startTime) { // ignore chord notes
#if DEBUG_SLUR
    if (debugVerbose) DEBUG("%s last %s note %s", __func__, lastOne->debugString().c_str(),
            note.debugString().c_str());
#endif
                continue;
            }
            if (lastOne->endTime() == note.startTime) {
                lastOne->setSlurStart(condition);
                note.setSlurEnd(condition);
            }
        }
        last[note.channel] = &note;
    }
}

// at least one can be set, or at least one can be cleared; and at least one slur
bool Notes::slursOrTies(unsigned selectChannels, HowMany howMany, bool* atLeastOneSlur) const {
    array<const DisplayNote*, CHANNEL_COUNT> last;
    last.fill(nullptr);
    if (atLeastOneSlur) {
        *atLeastOneSlur = false;
    }
    bool condition = HowMany::set == howMany;
    bool result = false;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (REST_TYPE == note.type) {
            last[note.channel] = nullptr;
            continue;
        }
        if (NOTE_ON != note.type) {
            last.fill(nullptr);
            continue;
        }
        auto lastOne = last[note.channel];
        if (lastOne) {
            if (lastOne->startTime >= note.startTime) {
                continue;  // ignore other notes on chord
            }
            if (condition != note.slurEnd()) {
                SCHMICKLE(condition != lastOne->slurStart());
                if (atLeastOneSlur) {
                    *atLeastOneSlur |= lastOne->pitch() != note.pitch();
                }
                result = true;
            }
        }
        last[note.channel] = &note;
    }
    return result;
}

// note : adjusts totalDuration by ppq
void Notes::setTriplets(unsigned selectChannels, bool condition) {
    array<TripletCandidate, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(TripletCandidate());
    int adjustment = 0;
    int numer = condition ? 2 : 3;
    int denom = condition ? 3 : 2;
    int modulo = condition ? 9 : 3;  //  3 non-trips (3*2^n) : 3 trips (2^n)
    int sign = condition ? -1 : 1;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (!note.isNoteOrRest() || NoteDurations::Dotted(note.duration, ppq)) {
            tripStarts.fill(TripletCandidate());
            continue;
        }
        if (condition == NoteDurations::TripletPart(note.duration, ppq)) {
            tripStarts.fill(TripletCandidate());
            continue;
        }
        auto& candidate = tripStarts[note.channel];
        if (INT_MAX != candidate.lastIndex
                && notes[candidate.lastIndex].startTime >= note.startTime) {
            continue;  // ignore other notes on chord
        }
        candidate.lastIndex = index;
        candidate.atLeastOneNote |= NOTE_ON == note.type;
        if (INT_MAX == candidate.startIndex) {
            candidate.startIndex = index;
            continue;
        }
        if (!candidate.atLeastOneNote) {
            continue;
        }
        // look for enough non-triplet part notes to make a multiple of three
        DisplayNote* test = &notes[candidate.startIndex];
        int startTime = test->startTime;
        int totalDuration = note.endTime() - startTime;
        if (!(NoteDurations::InStd(totalDuration, ppq) % modulo)) {
            do {
                if (debugVerbose) DEBUG("%s test->startTime old:%d new:%d dur old:%d new:%d"
                        " startTime:%d numer:%d denom:%d adjustment:%d", __func__, test->startTime,
                        test->startTime - (test->startTime - startTime) / denom + adjustment,
                        test->duration, test->duration * numer / denom, startTime, numer, denom,
                        adjustment);
                test->startTime += sign * ((test->startTime - startTime) / denom + adjustment);
                test->duration = test->duration * numer / denom;
            } while (&note != test++);
            adjustment += totalDuration / denom;
        }
    }
    this->shift(selectEnd, adjustment, selectChannels);
    this->sort();
}

// more precisely: all notes and rests that can be triplets, are
// Note that dotted notes are disallowed as triplet parts.
// This is because 2/3rds of the dotted note is the undotted note, making it indistinguishable
//   from a regular undotted note outside a triplet.
bool Notes::triplets(unsigned selectChannels, HowMany howMany) const {
    array<TripletCandidate, CHANNEL_COUNT> tripStarts;
    tripStarts.fill(TripletCandidate());
    bool condition = HowMany::set == howMany;
    int modulo = condition ? 9 : 3;  //  3 non-trips (3*2^n) : 3 trips (2^n)
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (!note.isNoteOrRest() || NoteDurations::Dotted(note.duration, ppq)) {
            tripStarts.fill(TripletCandidate());
            continue;
        }
        if (condition == NoteDurations::TripletPart(note.duration, ppq)) {
            tripStarts.fill(TripletCandidate());
            continue;
        }
        auto& candidate = tripStarts[note.channel];
        if (INT_MAX != candidate.lastIndex
                && notes[candidate.lastIndex].startTime >= note.startTime) {
            continue;  // ignore other notes on chord
        }
        candidate.lastIndex = index;
        candidate.atLeastOneNote |= NOTE_ON == note.type;
        if (INT_MAX == candidate.startIndex) {
            candidate.startIndex = index;
            continue;
        }
        if (!candidate.atLeastOneNote) {
            continue;
        }
        // look for enough non-triplet part notes to make a multiple of three
        const DisplayNote* test = &notes[candidate.startIndex];
        int totalDuration = note.endTime() - test->startTime;
        if (!(NoteDurations::InStd(totalDuration, ppq) % modulo)) {
            return true;
        }
    }
    return false;
}

bool Notes::Deserialize(const vector<uint8_t>& storage, vector<DisplayNote>* notes, int* ppq) {
    NoteTakerParseMidi midiParser(storage, notes, ppq, nullptr);
    vector<uint8_t>::const_iterator iter = midiParser.midi.begin();
    DisplayNote note(UNUSED);
    int lastSuccess = 0;
    notes->clear();
    do {
        int delta;
        int attempt = iter - midiParser.midi.begin();
        if (!midiParser.midi_size8(iter, &delta) || delta < 0) {
            DEBUG("invalid midi time");
            midiParser.debug_out(iter, lastSuccess);
            return false;
        }
        note.startTime += delta;
        if (!midiParser.midi_size8(iter, &note.duration) || note.duration < 0) {
            DEBUG("invalid duration");
            midiParser.debug_out(iter, lastSuccess);
            return false;
        }
        for (unsigned index = 0; index < 4; ++index) {
            if (!midiParser.midi_size8(iter, &note.data[index])) {
                DEBUG("invalid data %u", index);
                midiParser.debug_out(iter, lastSuccess);
                return false;
            }
        }
        if (iter >= midiParser.midi.end()) {
            DEBUG("unexpected eof");
            return false;
        }
        uint8_t byte = *iter++;
        note.type = (DisplayType) (byte >> 4);
        note.channel = byte & 0x0F;
        if (iter > midiParser.midi.end()) {
            DEBUG("unexpected eof 2");
            return false;
        }
        notes->push_back(note);
        lastSuccess = attempt;
    } while (iter != midiParser.midi.end());
    return Notes::Validate(*notes, false, !!ppq);
}

void Notes::eraseNotes(unsigned start, unsigned end, unsigned selectChannels) {
    for (auto iter = notes.begin() + end; iter-- != notes.begin() + start; ) {
        if (iter->isSelectable(selectChannels)) {
            notes.erase(iter);
        }
    }
}

vector<unsigned> Notes::getVoices(unsigned selectChannels, bool atStart) const {
    vector<unsigned> result;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (note.isSelectable(selectChannels) && NOTE_ON == note.type) {
            if (atStart && result.size() && notes[result[0]].startTime != note.startTime) {
                break;
            }
            result.push_back(index);
        }
    }
    std::sort(result.begin(), result.end(), [this](const unsigned& lhs, const unsigned& rhs) {
        return notes[lhs].pitch() < notes[rhs].pitch();
    });
    return result;
}

// not sure what I was thinking (also, this doesn't work)
#if 0
void Notes::HighestOnly(vector<DisplayNote>& span) {
    vector<DisplayNote> highest;
    for (auto& note : span) {
        if (NOTE_ON != note.type) {
            highest.push_back(note);
            continue;
        }
        bool add = true;
        for (auto& test : span) {
            if (NOTE_ON != test.type) {
                continue;
            }
            if (test.channel != note.channel) {
                continue;
            }
            if (test.endTime() <= note.startTime) {
                continue;
            }
            if (test.startTime >= note.endTime()) {
                continue;
            }
            if (test.pitch() >= note.pitch()) {
                add = false;
                break;
            }
        }
        if (add) {
            highest.push_back(note);
        }
    }
    std::swap(highest, span);
}
#endif

// to compute range for horizontal wheel when selecting notes
// to do : horizontalCount, noteToWheel, wheelToNote share loop logic. Consolidate?
unsigned Notes::horizontalCount(unsigned selectChannels) const {
    unsigned count = 0;
    int lastStart = -1;
    for (auto& note : notes) {
        if (note.isSelectable(selectChannels) && lastStart != note.startTime) {
            ++count;
            if (note.isNoteOrRest()) {
                lastStart = note.startTime;
            }
        }
    }
    return count;
}

bool Notes::isEmpty(unsigned selectChannels) const {
    for (auto& note : notes) {
        if (note.isSelectable(selectChannels)) {
            return false;
        }
    }
    return true;
}

// to do : need only early exit if result > 0 ?
int Notes::noteCount(unsigned selectChannels) const {
    int result = 0;
    for (auto& note : notes) {
        if (NOTE_ON == note.type && note.isSelectable(selectChannels)) {
            ++result;
        }
    }
    return result;
}

int Notes::noteTimes(unsigned selectChannels) const {
    int result = 0;
    int lastStart = -1;
    for (unsigned index = selectStart; index < selectEnd; ++index) {
        auto& note = notes[index];
        if (!note.isNoteOrRest()) {
            continue;
        }
        if (!note.isSelectable(selectChannels)) {
            continue;
        }
        if (note.startTime == lastStart) {
            continue;
        }
        ++result;
        SCHMICKLE(lastStart < note.startTime);
        lastStart = note.startTime;
    }
    return result;
}

bool Notes::PitchCollision(const vector<DisplayNote>& notes, const DisplayNote& note,
        int pitch, vector<const DisplayNote*>* overlaps) {
    bool collision = false;
    for (auto& test : notes) {
        if (NOTE_ON != test.type) {
            continue;
        }
        if (test.channel != note.channel) {
            continue;
        }
        if (test.endTime() <= note.startTime) {
            continue;
        }
        if (test.startTime >= note.endTime()) {
            break;
        }
        if (&note == &test) {
            continue;
        }
        if (overlaps) {
            overlaps->push_back(&test);
        }
        collision |= test.pitch() == pitch;
    }
    return collision;
}

bool Notes::pitchCollision(const DisplayNote& note, int newPitch) const {
    return Notes::PitchCollision(notes, note, newPitch, nullptr);
}

void Notes::setDuration(DisplayNote* note) {
    vector<const DisplayNote*> overlaps;
    if (!Notes::PitchCollision(notes, *note, note->pitch(), &overlaps)) {
        return;
    }
    for (auto test : overlaps) {
        if (note->endTime() > test->startTime) {
            if (test->pitch() == note->pitch()) {
                note->duration = test->startTime - note->startTime;
            }
        }
    }
}

// unlike saving as MIDI, this preserves rests and other specialized information
void Notes::Serialize(const vector<DisplayNote>& notes, vector<uint8_t>& storage) {
    NoteTakerMakeMidi midiMaker;
    midiMaker.target = &storage;
    int lastStart = 0;
    for (auto& note : notes) {
        if (note.duration < 0) {
            DEBUG("[%d / %u] dur < 0", &note - &notes.front(), notes.size());
            DEBUG(" %s", note.debugString().c_str());
        }
        midiMaker.add_delta(note.startTime, &lastStart);
        midiMaker.add_size8(note.duration);
        for (unsigned index = 0; index < 4; ++index) {
            midiMaker.add_size8(note.data[index]);
        }
        // MIDI-like: pack type and channel into first byte
        uint8_t chan = note.isNoteOrRest() ? note.channel : 0;
        uint8_t byte = (note.type << 4) + chan;
        midiMaker.add_one(byte);
    }
}

// transpose the span up by approx. one score line (major/aug third) avoiding existing notes
bool Notes::transposeSpan(vector<DisplayNote>& span) const {
    vector<DisplayNote> transposed;
    int newPitch = -1;
    for (auto& note : span) {
        if (NOTE_ON != note.type) {
            continue;
        }
        // collect existing notes on this channel at this time
        vector<const DisplayNote*> overlaps;
        newPitch = note.pitch() + 3;
        bool collision = Notes::PitchCollision(notes, note, newPitch, &overlaps);
        collision |= Notes::PitchCollision(transposed, note, newPitch, &overlaps);  // collect new notes in span 
        while (collision && ++newPitch <= 127) {  // transpose up to free slot
            collision = false;
            for (auto overlap : overlaps) {
                if (overlap->pitch() == newPitch) {
                    collision = true;
                    break;
                }
            }
        }
        if (collision) {
            // if no free slots a third-ish above existing notes, use first free slot
            for (newPitch = note.pitch() - 1; collision && newPitch > 0; --newPitch) {
                collision = false;
                for (auto overlap : overlaps) {
                    if (overlap->pitch() == newPitch) {
                        collision = true;
                        break;
                    }
                }
            }
            if (collision) {
                return false;   // fail, tell caller new notes can't be transposed and added
            }
        }
        note.setPitch(newPitch);
        transposed.push_back(note);
    }
    return true;
}

int Notes::xPosAtEndStart() const {
    unsigned endStart = selectEnd - 1;
    while (!notes[endStart].cache) {
        ++endStart;
        SCHMICKLE(endStart < notes.size());
    }
    return notes[endStart].cache->xPosition;
}

int Notes::xPosAtEndEnd(const DisplayState& state) const {
    unsigned endEnd = selectEnd - 1;
    const NoteCache* noteCache;
    while (!(noteCache = notes[endEnd].cache)) {
        ++endEnd;
        SCHMICKLE(endEnd < notes.size());
    }
    return NoteTakerDisplay::XEndPos(*noteCache, state.vg);
}

int Notes::xPosAtStartEnd() const {
    unsigned startEnd = this->selectEndPos(selectStart);
    while (!notes[startEnd].cache) {
        ++startEnd;
        SCHMICKLE(startEnd < notes.size());
    }
    return notes[startEnd].cache->xPosition;
}

int Notes::xPosAtStartStart() const {
    unsigned start = selectStart;
    while (!notes[start].cache) {
        ++start;
        SCHMICKLE(start < notes.size());
    }
    return notes[start].cache->xPosition;
}

