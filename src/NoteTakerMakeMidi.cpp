#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

struct TimeNote {
    float time;  // 1 == quarter note
    int note;
};

    // TODO: save and load working data
    // to bootstrap, create a midi file
    // has the downside that if there are mistakes here, there will also
    // be mistakes in the midi parser
void NoteTakerMakeMidi::createDefaultAsMidi(vector<uint8_t>& midi) {
    target = &midi;
    midi.insert(midi.end(), MThd.begin(), MThd.end());
    for (auto iter  = midi.begin(); iter != midi.end(); ++iter) {
        debug("%c\n", *iter);
    }
    midi.insert(midi.end(), MThd_length.begin(), MThd_length.end());
    midi.insert(midi.end(), MThd_data.begin(), MThd_data.end());
    midi.insert(midi.end(), MTrk.begin(), MTrk.end());
    // defer adding until size of data is known
    std::vector<uint8_t> temp;
    target = &temp;
    const TimeNote notes[] = { {0, 0}, {1, 4}, {2, 7}, {3, 11}, {4, -1} };
    const int c4 = 0x3C;
    int lastNote = -1;
    int lastTime = 0;
    for (auto n : notes) {
        if (lastNote >= 0) {
            add_delta(n.time - 0.5f, &lastTime, 1);
            add_one(midiNoteOff + 0);  // channel 1
            add_one(c4 + lastNote);
            add_one(stdKeyPressure);
        }
        add_delta(n.time, &lastTime, 0);  // channel 1
        if (n.note >= 0) {
            add_one(midiNoteOn + 0);
            add_one(c4 + n.note);
            add_one(stdKeyPressure);
        } else {
            temp.insert(temp.end(), MTrk_end.begin(), MTrk_end.end());
        }
        lastNote = n.note;
    }
    target = &midi;
    debug("temp size: %d\n", temp.size());
    size_t s = midi.size();
    debug("midi size: %d\n", midi.size());
    add_size32(temp.size());
    debug("%d %d %d %d\n", midi[s], midi[s + 1], midi[s + 2], midi[s + 3]);
    debug("midi size: %d\n", midi.size());
    midi.insert(midi.end(), temp.begin(), temp.end());
    debug("midi size: %d\n", midi.size());
    debug("%d %d %d %d\n", midi[s], midi[s + 1], midi[s + 2], midi[s + 3]);
}
