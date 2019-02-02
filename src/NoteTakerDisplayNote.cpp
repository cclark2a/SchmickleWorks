#include "NoteTakerDisplayNote.hpp"
#include "NoteTakerMakeMidi.hpp"

static_assert(((NOTE_OFF << 4) | 0x80) == midiNoteOff);
static_assert(((NOTE_ON << 4) | 0x80) == midiNoteOn);
static_assert(((KEY_PRESSURE << 4) | 0x80) == midiKeyPressure);
static_assert(((CONTROL_CHANGE << 4) | 0x80) == midiControlChange);
static_assert(((PROGRAM_CHANGE << 4) | 0x80) == midiProgramChange);
static_assert(((CHANNEL_PRESSURE << 4) | 0x80) == midiChannelPressure);
static_assert(((PITCH_WHEEL << 4) | 0x80) == midiPitchWheel);
static_assert(((MIDI_SYSTEM << 4) | 0x80) == midiSystem);
