#include "NoteTaker.hpp"

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
            s += " note=" + std::to_string(note());
            s += " onVelocity=" + std::to_string(onVelocity());
            s += " offVelocity=" + std::to_string(offVelocity());
        break;
        case REST_TYPE:
        break;
        case MIDI_HEADER:
            s += " format=" + std::to_string(format());
        break;
        case KEY_SIGNATURE:
            s += " key=" + std::to_string(key());
            s += " minor=" + std::to_string(minor());
        break;
        case TIME_SIGNATURE:
            s += " numerator=" + std::to_string(numerator());
            s += " denominator=" + std::to_string(denominator());
            s += " clocksPerClick=" + std::to_string(clocksPerClick());
            s += " 32ndsPerQuarter=" + std::to_string(notated32NotesPerQuarterNote());
        break;
        default:
            // to do
            break;
    }
    return s;
}

void NoteTaker::debugDump() const {
    // make sure debug data is self-consistent
    for (unsigned i = 0; i < NUM_TYPES; ++i) {
        assert(i == typeNames[i].type);
    }
    debug("notes: %d", allNotes.size());
    for (const auto& note : allNotes) {
        std::string s;
        if (&note == &allNotes[selectStart]) {
            s += "< ";
        }
        if (&note == &allNotes[selectEnd]) {
            s += "> ";
        }
        s += note.debugString();
        debug("%s", s.c_str());
    }
}
