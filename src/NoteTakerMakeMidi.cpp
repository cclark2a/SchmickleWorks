#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"
#include <set>

struct TimeNote {
    float time;  // 1 == quarter note
    int note;
};

    // TODO: save and load working data
    // to bootstrap, create a midi file
    // has the downside that if there are mistakes here, there will also
    // be mistakes in the midi parser
void NoteTakerMakeMidi::createDefaultAsMidi(vector<uint8_t>& midi) {
    this->standardHeader(midi);
    const TimeNote notes[] = { {0, 0}, {1, 4}, {2, 7}, {3, 11}, {4, -1} };
    const int c4 = 0x3C;
    int lastNote = -1;
    int lastTime = 0;
    for (auto n : notes) {
        if (lastNote >= 0) {
            add_delta((int) (n.time * stdTimePerQuarterNote), &lastTime);
            add_one(midiNoteOff + 0);  // channel 1
            add_one(c4 + lastNote);
            add_one(stdKeyPressure);
        }
        add_delta((int) (n.time * stdTimePerQuarterNote), &lastTime);
        if (n.note >= 0) {
            add_one(midiNoteOn + 0);
            add_one(c4 + n.note);
            add_one(stdKeyPressure);
        } else {
            add_track_end();
        }
        lastNote = n.note;
    }
    this->standardTrailer(midi);
}

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
        return note->startTime + note->duration < n.note->startTime + n.note->duration;
    }

    const DisplayNote* note;
};

void NoteTakerMakeMidi::createFromNotes(const vector<DisplayNote>& notes, vector<uint8_t>& midi) {
    this->standardHeader(midi);
    std::set<LastNote> lastNotes;
    int lastTime = 0;
    for (auto& n : notes) {
        switch(n.type) {
            case MIDI_HEADER:
                // to do : allow custom ticks / quarter note (hardcoded to 96)
                break;
            case NOTE_ON:
                while (!lastNotes.empty()) {
                    auto off = lastNotes.begin()->note;
                    if (off->startTime + off->duration > n.startTime) {
                        break;
                    }
                    add_delta(off->startTime + off->duration, &lastTime);
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
            case TRACK_END:
                for (auto last : lastNotes) {
                    auto off = last.note;
                    add_delta(off->startTime + off->duration, &lastTime);
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
