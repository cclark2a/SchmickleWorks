#include "Button.hpp"
#include "Cache.hpp"
#include "Display.hpp"
#include "ParseMidi.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

struct DisplayTypeNames {
    DisplayType type;
    const char* name;
 } typeNames[] = {
    { UNUSED, "note off"},
    { NOTE_ON, "note on"},
    { KEY_PRESSURE, "key pressure"},
    { CONTROL_CHANGE, "control change"},
    { PROGRAM_CHANGE, "program change"},
    { CHANNEL_PRESSURE, "channel pressure"},
    { PITCH_WHEEL, "pitch wheel"},
    { MIDI_SYSTEM, "midi system"},
    { MIDI_HEADER, "midi header"},
    { KEY_SIGNATURE, "key signature"},
    { TIME_SIGNATURE, "time signature"},
    { MIDI_TEMPO, "midi tempo"},
    { REST_TYPE, "rest type"},
    { TRACK_END, "track end"},
};

std::string InvalDebugStr(Inval inval) {
    SCHMICKLE((int) inval >= (int) Inval::none);
    SCHMICKLE((int) inval <= (int) Inval::load);
    const char* debugStrs[] = { "none", "display", "cut", "change", "note", "load" };
    return debugStrs[(int) inval];
}

std::string PosAsStr(PositionType pType) {
    return PositionType::left == pType  ? "L" : PositionType::mid == pType ? "M" : "R";
}

std::string StemAsStr(StemType sType) {
    return StemType::unknown == sType ? "?" : StemType::up == sType ? "U" :
            StemType::down == sType ? "D" : "!";

}

std::string TrimmedFloat(float f) {
    std::string str = std::to_string(f);
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    str.erase(str.find_last_not_of('.') + 1, std::string::npos);
    return str;
}

static std::string uint_str(unsigned int v) {
    return INT_MAX == v ? "INT_MAX" : std::to_string(v);
}

static std::string float_str(float v) {
    return 0 == v ? "0" : std::to_string(v);
}

std::string BeamPosition::debugString() const {
    std::string s;
    s += "beamMin:" + uint_str(beamMin);
    s += " xOffset:" + float_str(xOffset);
    s += " yOffset:"  + std::to_string(yOffset) + " slurOffset:" + float_str(slurOffset);
    s += " sx:" + float_str(sx) + " ex:" + float_str(ex);
    s += " yStemExtend:" + std::to_string(yStemExtend) + " y:" + std::to_string(y);
    s += " yLimit:" + std::to_string(yLimit);
    s += " noteFirst:" + uint_str(noteFirst) + " cacheFirst:" + uint_str(cacheFirst);
    s += " noteLast:" + uint_str(noteLast) + " cacheLast:" + uint_str(cacheLast);
    s += " outsideStaff:" + std::to_string(outsideStaff);
    return s;
}

#if DEBUG_STD
    const BeamPosition& DisplayCache::b(unsigned index) const { return beams[index]; }
    const NoteCache& DisplayCache::d(unsigned index) const { return notes[index]; }
#endif

void DisplayCache::validateNotes(const Notes* xnotes) const {
#if DEBUG_CACHE
    DEBUG("%s", __func__);
    const NoteCache* debugStart = nullptr;
    for (auto& n : notes) {
        int c = n.channel;
        SCHMICKLE(c == n.note->channel);
        if (debugStart) {
            if (!(*debugStart < n)) {
                DEBUG("%s debugNote %s >\n                              n.note %s", __func__,
                        debugStart->note->debugString(xnotes->notes, &notes, nullptr).c_str(),
                        n.note->debugString(xnotes->notes, &notes, nullptr).c_str());
            }
            SCHMICKLE(*debugStart < n);
        }
        debugStart = &n;
    }
#endif
}

std::string NoteCache::debugString() const {
    std::string s;
    s += "[" + TrimmedFloat(xPosition) + ", " + TrimmedFloat(yPosition) + "] ";
    s += "vS:" + std::to_string(vStartTime) + " vD:" + std::to_string(vDuration);
    s += " M" + std::to_string(bar) + " ";
    if (PositionType::none != beamPosition || beamCount) {
        s += "b" + PosAsStr(beamPosition) + std::to_string(beamCount) + " ";
    }
    s += "[" + std::to_string(channel) + "] ";
    if (PositionType::none != slurPosition) {
        s += "s" + PosAsStr(slurPosition) + " ";
    }
    if (PositionType::none != tiePosition) {
        s += "t" + PosAsStr(tiePosition) + " ";
    }
    if (PositionType::none != tripletPosition) {
        s += "3" + PosAsStr(tripletPosition) + " ";
    }
    if (INT_MAX != beamId) {
        s += std::to_string(beamId) + " ";
    }
    if (pitchPosition) {
        s += std::to_string(pitchPosition) + " ";
    }
    s += std::to_string(symbol) +  (accidentalSpace ? "#" : "")
            + (endsOnBeat ? "B" : "") + StemAsStr(stemDirection) + (drawStaff ? "s" : "")
            + (chord ? "c" : "");
    return s;
}

std::string DisplayNote::debugString() const {
    std::string s = std::to_string(startTime) + " " + typeNames[type].name
            + " [" +  std::to_string(channel);
    if ((uint8_t) -1 != voice) {
        s += "/" + std::to_string(voice);
    }
    s += "] " + std::to_string(duration);
    if (PositionType::none != triplet) {
        s += " 3" + PosAsStr(triplet);
    }
    switch (type) {
        case NOTE_ON:
            s += " pitch=" + std::to_string(this->pitch());
            if (this->slurStart()) {
                s += " slur ";
                s += this->slurEnd() ? "mid" : "left";
            } else if (this->slurEnd()) {
                s += " slur right";
            }
#if 0   // to do : once we're using these, show them in dump
            s += " onVel=" + std::to_string(onVelocity());
            s += " offVel=" + std::to_string(offVelocity());
#endif
            break;
        case NOTE_OFF:
            break;
        case REST_TYPE:
            break;
        case MIDI_HEADER:
            s += " format=" + std::to_string(format());
            s += " tracks=" + std::to_string(tracks());
            s += " ppq=" + std::to_string(ppq());
            break;
        case KEY_SIGNATURE:
            s += " key=" + std::to_string(key());
            s += " minor=" + std::to_string(minor());
            break;
        case TIME_SIGNATURE:
            s += " numer=" + std::to_string(numerator());
            s += " denom=" + std::to_string(denominator());
            s += " clocks=" + std::to_string(clocksPerClick());
            s += " 32nds=" + std::to_string(notated32NotesPerQuarterNote());
            break;
        case MIDI_TEMPO:
            s += " tempo=" + std::to_string(tempo());
            break;
        case TRACK_END:
            break;
        default:
            DEBUG("to do: add type %d %s\n", type, typeNames[type].name);
            break;
    }
    return s;
}

std::string DisplayNote::debugString(const vector<DisplayNote>& notes,
        const vector<NoteCache>* noteCache, const NoteCache* entry, const NoteCache* last,
        std::string angleStart, std::string angleEnd) const {
    std::string cacheIndex;
    std::string cacheDump;
    std::string result;
    if (noteCache && entry) {
        cacheIndex = std::to_string(entry - &noteCache->front()) + "/";
        cacheDump = " " + entry->debugString();
        if (last) {
            SCHMICKLE(&noteCache->front() <= last && last <= &noteCache->back());
            SCHMICKLE(last < cache);
            while (++last < cache) {
                result += std::to_string(last - &noteCache->front()) + "/"
                        + std::to_string(this - &notes.front()) + " "
                        + last->debugString() + "\n                              ";
            }
        }
    }
    result += angleStart + cacheIndex + std::to_string(this - &notes.front())
            + cacheDump + " " + this->debugString();
    return result + angleEnd;
}

void NoteTakerDisplay::debugDump(unsigned cacheStart, unsigned cacheEnd) const {
    auto& cacheNotes = this->cache()->notes;
    auto& notes = slot->n.notes;
    unsigned start = cacheNotes[cacheStart].note - &notes.front();
    unsigned end = cacheNotes[cacheEnd].note - &notes.front();
    Notes::DebugDump(notes, start - 10, start + 10, &cacheNotes, start, end);
}

void NoteTakerWidget::debugDump(bool validatable, bool inWheel) const {
    auto& n = this->n();
#if DEBUG_STORAGE
    for (auto& entry : storage.slots) {
        if (entry.directory.empty() && entry.filename.empty() && entry.n.notes.size() < 2) {
            continue;
        }
        DEBUG("[%u] %s%s notes:%d", &entry - &storage.slots.front(),
                entry.directory.c_str(), entry.filename.c_str(), entry.n.notes.size());
    }
#endif
    DEBUG("display xOffset: %g horzCount: %u", display->range.xAxisOffset, n.horizontalCount(selectChannels));
    DEBUG("horz: %s vert: %s",
            horizontalWheel->debugString().c_str(), verticalWheel->debugString().c_str());
    DEBUG("select s/e %u %u display s/e %u %u chans 0x%02x unlocked %d tempo %d ppq %d voice %d",
            n.selectStart, n.selectEnd, display->range.displayStart, display->range.displayEnd,
            selectChannels, 
            this->unlockedChannel(), nt() ? nt()->getTempo() : 0, n.ppq, edit.voice);
    auto& slot = storage.current();
    DEBUG("slot %p notes %p cache %p ", &slot, &n.notes.front(), slot.cache.notes.front());
    Notes::DebugDump(n.notes, 0, INT_MAX, slot.invalid ? nullptr : &slot.cache.notes,
            n.selectStart, n.selectEnd);
    DEBUG("clipboard");
    Notes::DebugDump(clipboard.notes);
    for (const auto& chan : slot.channels) {
        NoteTakerChannel _default;
        if (chan.isDefault()) {
            continue;
        }
        DEBUG("[%d] %s", &chan - &slot.channels.front(), chan.debugString().c_str());
    }
    if (!inWheel && selectButton->ledOn() && !this->menuButtonOn() && !runButton->ledOn()) {
        std::string w2n;
        for (int index = horizontalWheel->paramQuantity->minValue;
                index <= horizontalWheel->paramQuantity->maxValue; ++index) {
            w2n += " " + std::to_string(index) + "/"
                    + std::to_string((int) this->wheelToNote(index, false));
        }
        DEBUG("wheelToNote:%s", w2n.c_str());
        std::string n2w;
        unsigned idx = 0;
        for (const auto& note : n.notes) {
            n2w += " " + std::to_string(idx++) + "/"
                    + std::to_string(this->noteToWheel(note, false));
        }
        DEBUG("noteToWheel:%s", n2w.c_str());
    }
    if (validatable) {
        this->n().validate();
    }
}

#if DEBUG_STD
    const DisplayNote& Notes::d(unsigned index) const { return notes[index]; }
#endif

void Notes::DebugDump(const vector<DisplayNote>& notes, unsigned start, unsigned end,
        const vector<NoteCache>* cache, unsigned selectStart, unsigned selectEnd) {
    unsigned size = notes.size();
    start = std::min(start, size - std::min(size, 20U));  // show at least 20 notes
    end = std::min(size, std::max(start + 20, end));
    if (notes.size()) DEBUG("%s notes.front().cache %p &notes.front() %p", __func__,
            notes.front().cache, &notes.front());
    if (cache) DEBUG("&cache->front() %p cache->front().note %p", &cache->front(),
            cache->front().note);
    // to do : move this to some static assert or one time initialization function
    for (unsigned i = 0; i < NUM_TYPES; ++i) {
        SCHMICKLE(i == typeNames[i].type);
    }
    DEBUG("notes: %d cache: %d", notes.size(), cache ? cache->size() : 0);
    const NoteCache* last = nullptr;
    for (unsigned index = start; index < end; ++index) {
        const DisplayNote& note = notes[index];
        std::string angleStart = INT_MAX != selectStart && &note == &notes[selectStart] ? "< " : "";
        std::string angleEnd = INT_MAX != selectEnd && &note == &notes[selectEnd - 1] ? " >" : "";
        DEBUG("%s", note.debugString(notes, cache, note.cache, last, angleStart, angleEnd).c_str());
        last = note.cache;
    }
}

void NoteTakerParseMidi::DebugDumpRawMidi(const vector<uint8_t>& v) {
    std::string s;
    unsigned line = v.size();
    for (unsigned i = 0; i < v.size(); ++i) {
        uint8_t u = v[i];
        if (--line && 0xFF != u) {
            s += ", ";
        } else {
            DEBUG("%s", s.c_str());
            s = "";
        }
        const char h[] = "0123456789ABCDEF";
        s += "0x";
        s += h[u >> 4];
        s += h[u & 0xf];
        
        if (0x4d == u) {
            line = std::min(line, 13u);
        }
        if (0x80 == u || 0x90 == u) {
            line = std::min(line, 3u);
        }
    }
    DEBUG("%s", s.c_str());
}

bool Notes::validate(bool assertOnFailure) const {
    return Notes::Validate(notes, assertOnFailure);
}

bool Notes::Validate(const vector<DisplayNote>& notes, bool assertOnFailure, bool headerTrailer) {
    int time = 0;
    array<vector<const DisplayNote*>, CHANNEL_COUNT> channelTimes;
    bool sawHeader = !headerTrailer;
    bool sawTrailer = false;
    bool malformed = false;
    for (const auto& note : notes) {
        note.assertValid(note.type);
        switch (note.type) {
            case MIDI_HEADER:
                if (sawHeader) {
                    DEBUG("duplicate midi header");
                    malformed = true;
                }
                sawHeader = true;
                break;
            case NOTE_ON:
            case REST_TYPE: {
                if (!sawHeader) {
                    DEBUG("missing midi header before note");
                    malformed = true;
                }
                if (sawTrailer) {
                    DEBUG("note after trailer");
                    malformed = true;
                }
                if (time > note.startTime) {
                    DEBUG("note out of order");
                    malformed = true;
                }
                time = note.startTime;
                auto& times = channelTimes[note.channel];
                auto iter = times.begin();
                while (iter != times.end()) {
                    if ((*iter)->endTime() <= time) {
                        iter = times.erase(iter);
                    } else {
                        if (REST_TYPE == note.type || REST_TYPE == (*iter)->type) {
                            DEBUG("rest time error %s / %s", note.debugString().c_str(),
                                    (*iter)->debugString().c_str());
                            malformed = true;
                            break;
                        } else if (note.pitch() == (*iter)->pitch()) {
#if 0 // disable while debugging midi from the wild
                            DEBUG("note pitch error %s / %s", note.debugString().c_str(),
                                    (*iter)->debugString().c_str());
                            malformed = true;
#endif
                            break;
                        }
                        ++iter;
                    }
                }
                channelTimes[note.channel].push_back(&note);
                } break;
            case KEY_SIGNATURE:
            case TIME_SIGNATURE:
            case MIDI_TEMPO:
                if (!sawHeader) {
                    DEBUG("missing midi header before time signature");
                    malformed = true;
                }
                break;
            case TRACK_END:
                if (!sawHeader) {
                    DEBUG("missing midi header before trailer");
                    malformed = true;
                }
                if (sawTrailer || !headerTrailer) {
                    DEBUG("duplicate midi trailer");
                    malformed = true;
                }
                for (auto& c : channelTimes) {
                    for (auto& entry : c) {
                        if (entry->endTime() > note.startTime) {
                            DEBUG("track end time error %s / %s", note.debugString().c_str(),
                                    entry->debugString().c_str());
                            malformed = true;
                            break;
                        }
                    }
                    if (malformed) {
                        break;
                    }
                }
                sawTrailer = true;
                break;
            default:
                _schmickled(); // incomplete
        }
        if (malformed) {
            break;
        }
    }
    if (!malformed && !sawTrailer && headerTrailer) {
        DEBUG("missing trailer");
        malformed = true;
    }
    if (assertOnFailure && malformed) {
        _schmickled();
    }
    return !malformed;
}

std::string NoteTakerChannel::debugString() const {
    std::string s;
    if (!sequenceName.empty()) {
        s = " seq " + sequenceName;
    }
    if (!instrumentName.empty()) {
        s += " inst " + instrumentName;
    }
    if (gmInstrument) {
        s += " gm " + std::to_string(gmInstrument);
    }
    s += " relMax " + std::to_string(releaseMax);
    s += " relMin " + std::to_string(releaseMin);
    s += " susMin " + std::to_string(sustainMin);
    s += " susMax " + std::to_string(sustainMax);
    return s.substr(1);
}
