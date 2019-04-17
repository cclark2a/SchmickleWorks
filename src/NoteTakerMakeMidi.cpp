#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTaker.hpp"
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
    DisplayNote dummy = { 0, 0, { 0, 0, 0, 0}, 0, UNUSED, false }; // to do : empty constructor?
    int last = 0;
    add_track_end(dummy, last);
    this->standardTrailer(midi);
}

struct LastNote {
    LastNote(const DisplayNote* n)
        : note(n) {
    }

    bool operator<(const LastNote& n) const { 
        return note->endSlurTime() < n.note->endSlurTime() ||
                (note->endSlurTime() == n.note->endSlurTime() && note->channel < n.note->channel);
    }

    const DisplayNote* note;
};

void NoteTakerMakeMidi::createFromNotes(const NoteTaker& nt, vector<uint8_t>& midi) {
    // to do : allow custom ticks / quarter note (hardcoded to 96)
    this->standardHeader(midi, nt.ppq);
    // after header, write channel dur/sus as control change 0xBx
                // 0x57 release max mapped to duration index
                // 0x58 release min
                // 0x59 sustain min
                // 0x5A sustain max
    // to do : allow sustain/release changes during playback?
    for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
        const auto& chan = nt.channels[index];
        for (const auto& limit : NoteTakerChannelLimits) {
            if (!chan.isDefault(limit)) {
                add_size8(0);
                add_one(midiControlChange + index);
                add_one(midiReleaseMax + (int) limit);
                add_one(NoteDurations::FromMidi(chan.getLimit(limit), nt.ppq));
            }
        }
    }
    std::set<LastNote> lastNotes;
    int lastTime = 0;
    for (auto& n : nt.allNotes) {
        switch(n.type) {
            case MIDI_HEADER:
                break;
            case NOTE_ON:
                while (!lastNotes.empty()) {
                    auto off = lastNotes.begin()->note;
                    if (off->endSlurTime() > n.startTime) {
                        debug("%u break off %s", lastNotes.size(), off->debugString().c_str());
                        break;
                    }
                    add_delta(off->endSlurTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                    lastNotes.erase(lastNotes.begin());
                    debug("%u write off %s", lastNotes.size(), off->debugString().c_str());
                }
                add_delta(n.startTime, &lastTime);
                add_one(midiNoteOn + n.channel);
                add_one(n.pitch());
                add_one(n.onVelocity());
                lastNotes.insert(LastNote(&n));
                debug("%u write on %s", lastNotes.size(), n.debugString().c_str());
                break;
            case REST_TYPE:
                // assume there's nothing to do here
                break;
            case KEY_SIGNATURE:
                add_delta(n.startTime, &lastTime);
                add_one(midiMetaEvent);
                add_one(midiKeySignature);
                add_one(2); // number of bytes of data to follow
                add_one(n.key());
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
                    add_delta(off->endSlurTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                }
                add_track_end(n, lastTime);
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    this->standardTrailer(midi);
}
