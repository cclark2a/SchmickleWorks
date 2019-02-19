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
        add_delta(n.time, &lastTime, 0);
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
    add_size32(temp.size());
    midi.insert(midi.end(), temp.begin(), temp.end());
}

void NoteTakerMakeMidi::createEmpty(vector<uint8_t>& midi) {
    midi.insert(midi.end(), MThd.begin(), MThd.end());
    midi.insert(midi.end(), MThd_length.begin(), MThd_length.end());
    midi.insert(midi.end(), MThd_data.begin(), MThd_data.end());
    midi.insert(midi.end(), MTrk.begin(), MTrk.end());
    // defer adding until size of data is known
    std::vector<uint8_t> temp;
    target = &temp;
    add_size8(0);
    temp.insert(temp.end(), MTrk_end.begin(), MTrk_end.end());
    target = &midi;
    add_size32(temp.size());
    midi.insert(midi.end(), temp.begin(), temp.end());
}
