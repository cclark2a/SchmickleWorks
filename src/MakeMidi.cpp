#include "Display.hpp"
#include "MakeMidi.hpp"
#include "Taker.hpp"
#include <set>

// to do : move this to NoteTakeChannel.cpp
const NoteTakerChannel::Limit NoteTakerChannelLimits[] = {
    NoteTakerChannel::Limit::releaseMax,
    NoteTakerChannel::Limit::releaseMin,
    NoteTakerChannel::Limit::sustainMin,
    NoteTakerChannel::Limit::sustainMax
};

void NoteTakerMakeMidi::createEmpty(vector<uint8_t>& midi) {
    this->standardHeader(midi, stdTimePerQuarterNote);
    DisplayNote dummy(UNUSED);
    int last = 0;
    add_track_end(dummy, last);
    this->standardTrailer(midi);
}

struct LastNote {
    LastNote(const DisplayNote* n)
        : note(n) {
    }

    bool operator<(const LastNote& n) const { 
        return note->endTime() < n.note->endTime() ||
                (note->endTime() == n.note->endTime() && note->channel < n.note->channel);
    }

    const DisplayNote* note;
};

void NoteTakerMakeMidi::createFromNotes(const NoteTakerSlot& slot, vector<uint8_t>& midi) {
    this->standardHeader(midi, slot.n.ppq);
    // after header, write channel dur/sus as control change 0xBx
                // 0x57 release max mapped to duration index
                // 0x58 release min
                // 0x59 sustain min
                // 0x5A sustain max
    // to do : allow sustain/release changes during playback?
    for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
        const auto& chan = slot.channels[index];
        if (!chan.sequenceName.empty()) {
            add_channel_prefix(index);
            add_size8(0);
            add_one(midiMetaEvent);
            add_one(0x03);
            add_string(chan.sequenceName);
        }
        if (!chan.instrumentName.empty()) {
            add_channel_prefix(index);
            add_size8(0);
            add_one(midiMetaEvent);
            add_one(0x04);
            add_string(chan.instrumentName);
        }
        if (chan.gmInstrument) {
            add_size8(0);
            add_one(midiProgramChange + index);
            add_one(chan.gmInstrument);
        }
        for (const auto& limit : NoteTakerChannelLimits) {
            if (!chan.isDefault(limit)) {
                add_size8(0);
                add_one(midiControlChange + index);
                add_one(midiReleaseMax + (int) limit);
                add_one(NoteDurations::FromMidi(chan.getLimit(limit), slot.n.ppq));
            }
        }
    }
    std::set<LastNote> lastNotes;
    int lastTime = 0;
    for (auto& n : slot.n.notes) {
        switch(n.type) {
            case MIDI_HEADER:
                break;
            case NOTE_ON:
                while (!lastNotes.empty()) {
                    auto off = lastNotes.begin()->note;
                    if (off->endTime() > n.startTime) {
                        DEBUG("%u break off %s", lastNotes.size(), off->debugString().c_str());
                        break;
                    }
                    add_delta(off->endTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                    lastNotes.erase(lastNotes.begin());
                    DEBUG("%u write off %s", lastNotes.size(), off->debugString().c_str());
                }
                add_delta(n.startTime, &lastTime);
                add_one(midiNoteOn + n.channel);
                add_one(n.pitch());
                add_one(n.onVelocity());
                lastNotes.insert(LastNote(&n));
                DEBUG("%u write on %s", lastNotes.size(), n.debugString().c_str());
                break;
            case REST_TYPE:
                // assume there's nothing to do here
                break;
            case KEY_SIGNATURE:
                add_delta(n.startTime, &lastTime);
                add_one(midiMetaEvent);
                add_one(midiKeySignature);
                add_one(2); // number of bytes of data to follow
                add_one(n.key());   // key may be negative, but should be OK to push unsigned byte
                add_one(n.minor());
                break;
            case TIME_SIGNATURE:
                add_delta(n.startTime, &lastTime);
                add_one(midiMetaEvent);
                add_one(midiTimeSignature);
                add_one(4); // number of bytes of data to follow
                add_one(n.numerator());
                add_one(n.denominator());
                add_one(n.clocksPerClick());
                add_one(n.notated32NotesPerQuarterNote());
                break;
            case MIDI_TEMPO:
                add_delta(n.startTime, &lastTime);
                add_one(midiMetaEvent);
                add_one(midiSetTempo);
                add_one(3); // number of bytes of data to follow
                add_size24(n.tempo());
                break;
            case TRACK_END:
                for (auto last : lastNotes) {
                    auto off = last.note;
                    add_delta(off->endTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                }
                add_track_end(n, lastTime);
                break;
            default:
                // to do : incomplete
                _schmickled();
        }
    }
    this->standardTrailer(midi);
}
