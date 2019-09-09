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
    { SELECT_START, "select start (sorting)"},
    { SELECT_END, "select end (sorting)"},
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

std::string NoteCache::debugString() const {
    std::string s;
    s += "[" + TrimmedFloat(xPosition) + ", " + TrimmedFloat(yPosition) + "] ";
    s += "vS:" + std::to_string(vStartTime) + " vD:" + std::to_string(vDuration);
    s += " M" + std::to_string(bar) + " ";
    if (PositionType::none != beamPosition || beamCount) {
        s += "b" + PosAsStr(beamPosition) + std::to_string(beamCount) + " ";
    }
    if (channel) {
        s += "[" + std::to_string(channel) + "] ";
    }
    if (PositionType::none != slurPosition) {
        s += "s" + PosAsStr(slurPosition) + " ";
    }
    if (PositionType::none != tiePosition) {
        s += "t" + PosAsStr(tiePosition) + " ";
    }
    if (PositionType::none != tupletPosition) {
        s += "3" + PosAsStr(tupletPosition) + " ";
    }
    if (tupletId) {
        s += std::to_string(tupletId) + " ";
    }
    if (pitchPosition) {
        s += std::to_string(pitchPosition) + " ";
    }
    s += std::to_string(symbol) +  (accidentalSpace ? "#" : "")
            + (endsOnBeat ? "B" : "") + StemAsStr(stemDirection) + (staff ? "s" : "") 
            + (twoThirds ? "2" : "");
    return s;
}

std::string DisplayNote::debugString() const {
    std::string s = std::to_string(startTime) + " " + typeNames[type].name
            + " [" +  std::to_string(channel);
    if ((uint8_t) -1 != voice) {
        s += "/" + std::to_string(voice);
    }
    s += "] " + std::to_string(duration);
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

void NoteTakerWidget::debugDump(bool validatable, bool inWheel) const {
    auto& n = this->n();
#if 1 // def DEBUGGING_STORAGE // not normally defined
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
            this->unlockedChannel(), nt() ? nt()->tempo : 0, n.ppq, edit.voice);
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

void Notes::DebugDump(const vector<DisplayNote>& notes, unsigned start, unsigned end,
        const vector<NoteCache>* cache, unsigned selectStart, unsigned selectEnd) {
    unsigned size = notes.size();
    start = std::min(start, size - std::min(size, 20U));  // show at least 20 notes
    end = std::min(size, std::max(start + 20, end));
    if (notes.size()) DEBUG("%s n.notes.front().cache %p", __func__, notes.front().cache);
    if (cache) DEBUG("&cache->notes.front() %p", &cache->front());
    // to do : move this to some static assert or one time initialization function
    for (unsigned i = 0; i < NUM_TYPES; ++i) {
        SCHMICKLE(i == typeNames[i].type);
    }
    DEBUG("notes: %d cache: %d", notes.size(), cache ? cache->size() : 0);
    unsigned lastC = INT_MAX;
    for (unsigned index = start; index < end; ++index) {
        const DisplayNote& note = notes[index];
        std::string angleStart = INT_MAX != selectStart && &note == &notes[selectStart] ? "< " : "";
        std::string cacheIndex;
        std::string cacheDump;
        if (cache && note.cache) {
            unsigned nextC = note.cache - &cache->front();
            while (++lastC < nextC) {
                const auto& c = (*cache)[lastC];
                DEBUG("%s%d/%d %s", angleStart.c_str(), lastC, c.note - &notes.front(),
                        c.debugString().c_str());
                angleStart.clear();  
            }
            cacheDump = " " + (*cache)[nextC].debugString();
            cacheIndex = std::to_string(nextC) + "/";
            lastC = nextC;
        }
        std::string angleEnd = INT_MAX != selectEnd && &note == &notes[selectEnd - 1] ? " >" : "";
        DEBUG("%s%s%u%s %s%s", angleStart.c_str(), cacheIndex.c_str(), index,
                cacheDump.c_str(), note.debugString().c_str(), angleEnd.c_str());
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

bool Notes::Validate(const vector<DisplayNote>& notes, bool assertOnFailure) {
    int time = 0;
    array<vector<const DisplayNote*>, CHANNEL_COUNT> channelTimes;
    bool sawHeader = false;
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
                if (sawTrailer) {
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
    if (!malformed && !sawTrailer) {
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
