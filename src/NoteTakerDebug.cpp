#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

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

std::string DisplayNote::debugString() const {
    std::string s;    
    s += std::to_string(startTime) + " " + typeNames[type].name
            + " [" +  std::to_string(channel) + "] " + std::to_string(duration);
    switch (type) {
        case NOTE_ON:
            s += " pitch=" + std::to_string(pitch());
            if (slur()) {
                s += " slur";
            }
            s += " onVel=" + std::to_string(onVelocity());
            s += " offVel=" + std::to_string(offVelocity());
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
            debug("to do: add type %d %s\n", type, typeNames[type].name);
            break;
    }
    return s;
}

void NoteTakerWidget::debugDump(bool validatable, bool inWheel) const {
    NoteTakerWidget* ntw = const_cast<NoteTakerWidget*>(this);
    auto display = ntw->widget<NoteTakerDisplay>();
    debug("display xOffset: %g horzCount: %u", display->xAxisOffset, this->horizontalCount());
    auto horizontalWheel = ntw->widget<HorizontalWheel>();
    auto verticalWheel = ntw->widget<VerticalWheel>();
    debug("horz: %s vert: %s",
            horizontalWheel->debugString().c_str(), verticalWheel->debugString().c_str());
    auto n = this->n();
    debug("select s/e %u %u display s/e %u %u chans 0x%02x unlocked %d tempo %d ppq %d",
            n.selectStart, n.selectEnd, display->displayStart, display->displayEnd, selectChannels, 
            this->unlockedChannel(), nt()->tempo, n.ppq);
    NoteTaker::DebugDump(n.notes, display->cacheInvalid ? nullptr : &display->cache,
            n.selectStart, n.selectEnd);
    debug("clipboard");
    NoteTaker::DebugDump(clipboard);
    this->nt()->debugDumpChannels();
    auto selectButton = ntw->widget<SelectButton>();
    auto runButton = ntw->widget<RunButton>();
    if (!inWheel && selectButton->ledOn && !this->menuButtonOn() && !runButton->ledOn) {
        std::string w2n;
        for (int index = horizontalWheel->paramQuantity->minValue;
                index <= horizontalWheel->paramQuantity->maxValue; ++index) {
            w2n += " " + std::to_string(index) + "/"
                    + std::to_string((int) this->wheelToNote(index, false));
        }
        debug("wheelToNote:%s", w2n.c_str());
        std::string n2w;
        unsigned idx = 0;
        for (const auto& note : n.notes) {
            n2w += " " + std::to_string(idx++) + "/"
                    + std::to_string(this->noteToWheel(note, false));
        }
        debug("noteToWheel:%s", n2w.c_str());
    }
    if (validatable) {
        this->n().validate();
    }
}

std::string PosAsStr(PositionType pType) {
    return PositionType::left == pType  ? "L" : PositionType::mid == pType ? "M" : "R";
}

std::string TrimmedFloat(float f) {
    std::string str = std::to_string(f);
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    str.erase(str.find_last_not_of('.') + 1, std::string::npos);
    return str;
}

void NoteTaker::DebugDump(const vector<DisplayNote>& notes, const vector<NoteCache>* cache,
        unsigned selectStart, unsigned selectEnd) {
    for (unsigned i = 0; i < NUM_TYPES; ++i) {
        assert(i == typeNames[i].type);
    }
    debug("notes: %d", notes.size());
    unsigned cIndex = 0;
    for (unsigned index = 0; index < notes.size(); ++index) {
        const DisplayNote& note = notes[index];
        std::string s;
        if (cache) {
            do {
                const NoteCache& c = (*cache)[cIndex];
                if ("" != s) {
                    debug("%s", s.c_str());
                }
                s = std::to_string(index);
                s += " [" + TrimmedFloat(c.xPosition) + ", " + TrimmedFloat(c.yPosition) + "] ";
                s += "vS:" + std::to_string(c.vStartTime) + " vD:" + std::to_string(c.vDuration);
                s += " ";
                if (PositionType::none != c.slurPosition) {
                    s += "s" + PosAsStr(c.slurPosition) + " ";
                }
                if (PositionType::none != c.beamPosition) {
                    s += "b" + PosAsStr(c.beamPosition) + std::to_string(c.beamCount) + " ";
                }
                if (PositionType::none != c.tiePosition) {
                    s += "t" + PosAsStr(c.tiePosition) + " ";
                }
                if (PositionType::none != c.tupletPosition) {
                    s += "3" + PosAsStr(c.tupletPosition) + " ";
                }
                s += std::to_string(c.symbol) +  (c.accidentalSpace ? "#" : "")
                        + (c.endsOnBeat ? "b" : "") + (c.stemUp ? "u " : "d ");
                ++cIndex;
            } while (cIndex < cache->size() && (*cache)[cIndex].note <= &note);
        }
        if (INT_MAX != selectStart && &note == &notes[selectStart]) {
            s += "< ";
        }
        if (INT_MAX != selectEnd && &note == &notes[selectEnd]) {
            s += "> ";
        }
        s += note.debugString();
        debug("%s", s.c_str());
    }
}

void NoteTaker::DebugDumpRawMidi(vector<uint8_t>& v) {
    std::string s;
    unsigned line = v.size();
    for (unsigned i = 0; i < v.size(); ++i) {
        uint8_t u = v[i];
        if (--line && 0xFF != u) {
            s += ", ";
        } else {
            debug("%s", s.c_str());
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
    debug("%s", s.c_str());
}

void Notes::validate() const {
    int time = 0;
    array<int, CHANNEL_COUNT> channelTimes;
    channelTimes.fill(0);
    bool sawHeader = false;
    bool sawTrailer = false;
    bool malformed = false;
    for (const auto& note : notes) {
        note.assertValid(note.type);
        switch (note.type) {
            case MIDI_HEADER:
                if (sawHeader) {
                    debug("duplicate midi header");
                    malformed = true;
                }
                sawHeader = true;
                break;
            case NOTE_ON:
            case REST_TYPE:
                if (!sawHeader) {
                    debug("missing midi header before note");
                    malformed = true;
                }
                if (sawTrailer) {
                    debug("note after trailer");
                    malformed = true;
                }
                if (time > note.startTime) {
                    debug("note out of order");
                    malformed = true;
                }
                time = note.startTime;
                if (channelTimes[note.channel] > note.startTime) {
                    if (REST_TYPE == note.type) {
                        debug("rest channel time error");
                    } else {
                        debug("note channel time error");
                    }
                    malformed = true;
                }
                channelTimes[note.channel] = note.endTime();
                break;
            case KEY_SIGNATURE:
            case TIME_SIGNATURE:
            case MIDI_TEMPO:
                if (!sawHeader) {
                    debug("missing midi header before time signature");
                    malformed = true;
                }
                break;
            case TRACK_END:
                if (!sawHeader) {
                    debug("missing midi header before trailer");
                    malformed = true;
                }
                if (sawTrailer) {
                    debug("duplicate midi trailer");
                    malformed = true;
                }
                for (int c : channelTimes) {
                    if (c > note.startTime) {
                        debug("track end time error");
                        malformed = true;
                    }
                }
                sawTrailer = true;
                break;
            default:
                assert(0); // incomplete
        }
        if (malformed) {
            break;
        }
    }
    if (!malformed && !sawTrailer) {
        debug("missing trailer");
        malformed = true;
    }
    if (malformed) {
        assert(0);
    }
}

std::string NoteTakerChannel::debugString() const {
    std::string s;    
    s =   "relMax " + std::to_string(releaseMax);
    s += " relMin " + std::to_string(releaseMin);
    s += " susMin " + std::to_string(sustainMin);
    s += " susMax " + std::to_string(sustainMax);
    s += " noteIdx " + std::to_string(noteIndex);
    s += " gateLow " + std::to_string(gateLow);
    s += " noteEnd " + std::to_string(noteEnd);
    return s;
}
