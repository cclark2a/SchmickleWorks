#include "NoteTakerDisplayNote.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"

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
            case NOTE_ON:
                if (0 > data[0] || data[0] > 127) {
                    debug("invalid note pitch %d\n", data[0]);
                    return false;
                }
                if (channel > channels) {
                    debug("invalid note channel %d\n", channel);
                    return false;
                }
                if (NOTE_ON == type && (unsigned) data[1] >= noteDurations.size()) {
                    debug("invalid on note index %d\n", data[1]);
                    return false;
                }
                if (NOTE_ON == type && (0 > data[2] || data[2] > 127)) {
                    debug("invalid on note pressure %d\n", data[2]);
                    return false;
                }
                if (0 > data[3] || data[3] > 127) {
                    debug("invalid off note pressure %d\n", data[3]);
                    return false;
                }
            break;
            case REST_TYPE:
                if (0 > data[0] || data[0] > 127) {  // todo: should be more restrictive
                    debug("invalid rest pitch %d\n", data[0]);
                    return false;
                }
                if (channel > channels) {
                    debug("invalid rest channel %d\n", channel);
                    return false;
                }
            break;
            case MIDI_HEADER:
                if (0 > data[0] || data[0] > 2) {
                    debug("invalid midi format %d\n", data[0]);
                    return false;
                }
                if (1 != data[1] && (!data[0] || 0 > data[1])) {
                    debug("invalid midi tracks %d (format=%d)\n", data[1], data[0]);
                    return false;
                }
                if (1 > data[2]) {
                    debug("invalid beats per quarter note %d\n", data[2]);
                    return false;
                }
            break;
            case KEY_SIGNATURE:
                if (-7 <= data[0] || data[0] <= 7) {
                    debug("invalid key %d\n", data[0]);
                    return false;
                }
                if (0 != data[1] && 1 != data[1]) {
                    debug("invalid minor %d\n", data[1]);
                    return false;
                }
            break;
            case TIME_SIGNATURE: {
                // although midi doesn't prohibit weird time signatures, look for
                // common ones to help validate that the file is parsed correctly
                // allowed: 2/2 n/4 (n = 2 3 4 5 6 7 9 11) m/8 (m = 3 5 6 7 9 11 12)
                int num = data[0];
                int denom = data[1];
                if ((denom == 1 && num != 2)  // allow 2/2
                        || (denom == 2 && (num < 2 || num > 11 || (num > 7 && !(num & 1))))
                        || (denom == 3 && (num < 3 || num > 12 || num == 4 || num == 8))) {
                    debug("invalid time signature %d/%d\n", data[0], data[1]);
                    return false;
                }
                if (data[2] != 24) {
                    debug("invalid clocks/click %d\n", data[2]);
                }
                if (data[3] != 8) {
                    debug("invalid 32nds/quarter ntoe %d\n", data[3]);
                }
                } break;
            default:
                debug("to do: validate %d\n", type);
                assert(0);
                return false;
        }
        return true;
    }
