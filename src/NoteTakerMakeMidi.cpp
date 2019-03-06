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
    this->standardHeader(midi);
    add_size8(0);
    add_track_end();
    this->standardTrailer(midi);
}

struct LastNote {
    LastNote(const DisplayNote* n)
        : note(n) {
    }

    bool operator<(const LastNote& n) const { 
        return note->endTime() < n.note->endTime();
    }

    const DisplayNote* note;
};

void NoteTakerMakeMidi::createFromNotes(const NoteTaker& nt, vector<uint8_t>& midi) {
    // to do : allow custom ticks / quarter note (hardcoded to 96)
    this->standardHeader(midi);
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
                add_one(NoteTakerDisplay::DurationIndex(chan.getLimit(limit)));
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
                    if (off->endTime() > n.startTime) {
                        break;
                    }
                    add_delta(off->endTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                    lastNotes.erase(lastNotes.begin());
                }
                add_delta(n.startTime, &lastTime);
                add_one(midiNoteOn + n.channel);
                add_one(n.pitch());
                add_one(n.onVelocity());
                lastNotes.insert(LastNote(&n));
                break;
            case REST_TYPE:
                // assume there's nothing to do here
                break;
            case KEY_SIGNATURE:
                add_delta(n.startTime, &lastTime);
                add_one(midiKeySignatureHi);
                add_one(midiKeySignatureLo);
                add_one(2); // number of bytes of data to follow
                add_one(n.key());
                add_one(n.data[1]);  // to do : add struct accessor
                break;
            case TIME_SIGNATURE:
                add_delta(n.startTime, &lastTime);
                add_one(midiTimeSignatureHi);
                add_one(midiTimeSignatureLo);
                add_one(4); // number of bytes of data to follow
                add_one(n.numerator());
                add_one(n.denominator());
                add_one(n.data[2]);  // to do : add struct accessor
                add_one(n.data[3]);  // to do : add struct accessor
                break;
            case TRACK_END:
                for (auto last : lastNotes) {
                    auto off = last.note;
                    add_delta(off->endTime(), &lastTime);
                    add_one(midiNoteOff + off->channel);
                    add_one(off->pitch());
                    add_one(off->offVelocity());
                }
                add_size8(0);
                add_track_end();
                break;
            default:
                // to do : incomplete
                assert(0);
        }
    }
    this->standardTrailer(midi);
}
