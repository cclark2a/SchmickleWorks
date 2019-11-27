#include "DisplayNote.hpp"
#include "Display.hpp"

const char* debugPositionType[] = {
    "none",
    "left",
    "mid",
    "right",
};

static_assert(((NOTE_OFF << 4) | 0x80) == midiNoteOff, "note off mismatch");
static_assert(((NOTE_ON << 4) | 0x80) == midiNoteOn, "note on mismatch");
static_assert(((KEY_PRESSURE << 4) | 0x80) == midiKeyPressure, "pressure mismatch");
static_assert(((CONTROL_CHANGE << 4) | 0x80) == midiControlChange, "control mismatch");
static_assert(((PROGRAM_CHANGE << 4) | 0x80) == midiProgramChange, "program mismatch");
static_assert(((CHANNEL_PRESSURE << 4) | 0x80) == midiChannelPressure, "channel mismatch");
static_assert(((PITCH_WHEEL << 4) | 0x80) == midiPitchWheel, "pitch mismatch");
static_assert(((MIDI_SYSTEM << 4) | 0x80) == midiSystem, "system mismatch");

bool DisplayNote::isValid() const {
    switch (type) {
        case NOTE_OFF:
            if (channel > CHANNEL_COUNT) {
                DEBUG("invalid note channel %d\n", channel);
                return false;
            }
            if (0 > data[0] || data[0] > 127) {
                DEBUG("invalid note pitch %d\n", data[0]);
                return false;
            }
        break;
        case NOTE_ON:
            if (channel > CHANNEL_COUNT) {
                DEBUG("invalid note channel %d\n", channel);
                return false;
            }
            if (0 > data[0] || data[0] > 127) {
                DEBUG("invalid note pitch %d\n", data[0]);
                return false;
            }
            if (0 > data[1] || data[1] > (SLUR_START_BIT | SLUR_END_BIT)) {
                DEBUG("invalid slur %d\n", data[1]);
                return false;
            }
            if (0 > data[2] || data[2] > 127) {
                DEBUG("invalid on note pressure %d\n", data[2]);
                return false;
            }
            if (0 > data[3] || data[3] > 127) {
                DEBUG("invalid off note pressure %d\n", data[3]);
                return false;
            }
        break;
        case REST_TYPE:
            if (channel > CHANNEL_COUNT) {
                DEBUG("invalid note channel %d\n", channel);
                return false;
            }
        break;
        case MIDI_HEADER:
            if (0 > data[0] || data[0] > 2) {
                DEBUG("invalid midi format %d\n", data[0]);
                return false;
            }
            if (1 != data[1] && (!data[0] || 0 > data[1])) {
                DEBUG("invalid midi tracks %d (format=%d)\n", data[1], data[0]);
                return false;
            }
            if (1 > data[2]) {
                DEBUG("invalid beats per quarter note %d\n", data[2]);
                return false;
            }
        break;
        case KEY_SIGNATURE:
            if (0 > data[0] || data[0] > 14) {
                DEBUG("invalid key %d\n", data[0]);
                return false;
            }
            if (0 != data[1] && 1 != data[1]) {
                DEBUG("invalid minor %d\n", data[1]);
                return false;
            }
        break;
        case TIME_SIGNATURE: {
            int num = data[0];
            int denom = data[1];
            if ((unsigned) num > 99 || (unsigned) denom > 99) {
                DEBUG("invalid time signature %d/%d\n", data[0], data[1]);
                return false;
            }
#if 0   // we're going to ignore these values anyway (like all other MIDI software)
            if (data[2] != 24) {
                DEBUG("invalid clocks/click %d\n", data[2]);
            }
            if (data[3] != 8) {
                DEBUG("invalid 32nds/quarter note %d\n", data[3]);
            }
#endif
            } break;
        case MIDI_TEMPO: {
            int ticks = data[0];
            if (ticks <= 0) {
                DEBUG("invalid ticks per second %d\n", data[0]);
                return false;
            }
            break;
        }
        case TRACK_END:
        break;
        default:
            DEBUG("to do: validate %d\n", type);
            return _schmickle_false();
    }
    return true;
}

json_t* DisplayNote::dataToJson() const {
    json_t* root = json_object();
    json_object_set_new(root, "startTime", json_integer(startTime));
    json_object_set_new(root, "duration", json_integer(duration));
    json_t* noteData = json_array();
    for (int unsigned index = 0; index < sizeof(data) / sizeof(data[0]); index++) {
        json_array_append_new(noteData, json_integer(data[index]));
    }
    json_object_set_new(root, "data", noteData);
    json_object_set_new(root, "channel", json_integer(channel));
    json_object_set_new(root, "type", json_integer(type));
    return root;
}

void DisplayNote::dataFromJson(json_t* root) {
    cache = nullptr;
    INT_FROM_JSON(startTime);
    INT_FROM_JSON(duration);
    json_t* noteData = json_object_get(root, "data");
    size_t index;
    json_t* value;
    json_array_foreach(noteData, index, value) {
        data[index] = json_integer_value(value);
    }
    INT_FROM_JSON(channel);
    INT_FROM_JSON(type);
}
