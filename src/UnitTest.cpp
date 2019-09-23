#include "SchmickleWorks.hpp"

#if RUN_UNIT_TEST

#include "Button.hpp"
#include "Display.hpp"
#include "ParseMidi.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

static void Press(NoteTakerWidget* n, ParamWidget* ms) {
    event::DragStart evs;
    ms->onDragStart(evs);
    event::DragEnd evd;
    ms->onDragEnd(evd);
    if (debugVerbose) n->debugDump();
}

static void WheelUp(NoteTakerWidget* n, float value) {
    n->verticalWheel->lastValue = INT_MAX;
    auto vpm = n->verticalWheel->paramQuantity;
    SCHMICKLE(vpm->minValue <= vpm->maxValue);
    n->verticalWheel->setValue(std::max(vpm->minValue, std::min(vpm->maxValue, value)));
    n->updateVertical();
}

static void WheelLeft(NoteTakerWidget* n, float value) {
    n->horizontalWheel->lastValue = INT_MAX;
    auto hpm = n->horizontalWheel->paramQuantity;
    SCHMICKLE(hpm->minValue <= hpm->maxValue);
    n->horizontalWheel->setValue(std::max(hpm->minValue, std::min(hpm->maxValue, value)));
    n->updateHorizontal();
}

static void ExerciseWheels(NoteTakerWidget* n) {
    auto vpm = n->verticalWheel->paramQuantity;
    for (float v : { vpm->minValue, vpm->maxValue, (vpm->minValue + vpm->maxValue) / 2 }) {
        WheelUp(n, v);
    }
    auto hpm = n->horizontalWheel->paramQuantity;
    for (float v : { hpm->minValue, hpm->maxValue, (hpm->minValue + hpm->maxValue) / 2 }) {
        WheelLeft(n, v);
    }
}

// construct unit test out of parts that can be called independently
// difficult to test for correctness, but should find crashes
/*  high level
    add note / part / pitch / duration
    add rest / duration
    + low level
    
    low level
    press button
    move wheel
    pulse input
 */

// file button + vertical wheel potentially reads/writes to file system
// Unit test shouldn't modify files.
// Note NoteTaker loadScore, saveScore override midi dir while test is running
//  to read/write files from test directory instead of midi directory
static void LowLevelAction(NoteTakerWidget* n, int control) {
    switch (control) {
        case NoteTaker::RUN_BUTTON:
            Press(n, n->runButton);
        break;
        case NoteTaker::EXTEND_BUTTON:
            Press(n, n->selectButton);
        break;
        case NoteTaker::INSERT_BUTTON:
            Press(n, n->insertButton);
        break;
        case NoteTaker::CUT_BUTTON:
            Press(n, n->cutButton);
        break;
        case NoteTaker::REST_BUTTON:
            Press(n, n->restButton);
        break;
        case NoteTaker::PART_BUTTON:
            Press(n, n->partButton);
        break;
        case NoteTaker::FILE_BUTTON:
            Press(n, n->fileButton);
        break;
        case NoteTaker::SUSTAIN_BUTTON:
            Press(n, n->sustainButton);
        break;
        case NoteTaker::TIME_BUTTON:
            Press(n, n->timeButton);
        break;
        case NoteTaker::KEY_BUTTON:
            Press(n, n->keyButton);
        break;
        case NoteTaker::TIE_BUTTON:
            Press(n, n->tieButton);
        break;
        case NoteTaker::SLOT_BUTTON:
            Press(n, n->slotButton);
        break;
        case NoteTaker::TEMPO_BUTTON:
            Press(n, n->tempoButton);
        break;
        case NoteTaker::VERTICAL_WHEEL: {
            float value = n->verticalWheel->paramQuantity->minValue + ((float) rand() / RAND_MAX
                    * (n->verticalWheel->paramQuantity->maxValue - n->verticalWheel->paramQuantity->minValue));
            WheelUp(n, value);  
        } break;
        case NoteTaker::HORIZONTAL_WHEEL: {
            float value = n->horizontalWheel->paramQuantity->minValue + ((float) rand() / RAND_MAX
                    * (n->horizontalWheel->paramQuantity->maxValue - n->horizontalWheel->paramQuantity->minValue));
            WheelLeft(n, value);
        } break;
        case NoteTaker::NUM_PARAMS + NoteTaker::V_OCT_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::CLOCK_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::RESET_INPUT: {
            float value = -10 + (float) rand() / RAND_MAX * 20;
            n->nt()->inputs[control - NoteTaker::NUM_PARAMS].setVoltage(value);
        } break;
        default:
            _schmickled();  // shouldn't land here
    }
}

static void ResetState(NoteTakerWidget* n) {
    n->nt()->resetState(false);
    n->display->range.reset();
}

static void LowLevelTestDigits(NoteTakerWidget* n) {
    int counters[6];
    for (int digits = 1; digits <= 5; ++digits) {
        for (int index = 0; index < digits; ++ index) {
            counters[index] = 0;
        }
        do {
            int index = 0;
            while (++counters[index] == NoteTaker::NUM_PARAMS + NoteTaker::NUM_INPUTS) {
                counters[index] = 0;
                ++index;
            }
            if (counters[digits]) {
                break;
            }
            ResetState(n);
            n->resetControls();
            for (index = 0; index < digits; ++index) {
                LowLevelAction(n, counters[index]);
                n->n().validate();
            }
        } while (true);
    }
}

static void LowLevelTestDigitsSolo(NoteTakerWidget* n) {
    int counters[] = {2, 5, 3, 4, 0, 0};
    int digits = 5;
    ResetState(n);
    n->resetControls();
    for (int index = 0; index < digits; ++index) {
        LowLevelAction(n, counters[index]);
    }
}

static void LowLevelRandom(NoteTakerWidget* n, unsigned seed, int steps) {
    srand(seed);
    ResetState(n);
    n->resetControls();
    for (int index = 0; index < steps; ++index) {
        int control = rand() % (NoteTaker::NUM_PARAMS + NoteTaker::NUM_INPUTS);
        LowLevelAction(n, control);
    }
}

static void AddTwoNotes(NoteTakerWidget* n) {
    ResetState(n);
    Press(n, n->insertButton);
    Press(n, n->insertButton);
    unsigned note1 = n->wheelToNote(1);
    WheelUp(n, n->n().notes[note1].pitch() + 1);
}

static bool IsEmpty(NoteTakerWidget* n) {
    return n->n().isEmpty(n->selectChannels);
}

static void Expected(NoteTakerWidget* n) {
    json_t* saveState = n->toJson();
    n->nt()->requests.push(RequestType::onReset);
    DEBUG("delete a note with empty score");
    Press(n, n->cutButton);

    DEBUG("add a note with empty score, delete same");
    Press(n, n->insertButton);
    SCHMICKLE(!IsEmpty(n));
    Press(n, n->cutButton);
    SCHMICKLE(IsEmpty(n));

    DEBUG("add two notes with empty score, delete same");
    Press(n, n->insertButton);
    Press(n, n->insertButton);
    Press(n, n->cutButton);
    SCHMICKLE(!IsEmpty(n));
    Press(n, n->cutButton);
    SCHMICKLE(IsEmpty(n));

    DEBUG("add two notes with empty score, check order");
    AddTwoNotes(n);
    unsigned note1 = n->wheelToNote(1);
    SCHMICKLE(4 == n->n().notes.size());
    SCHMICKLE(2 == n->n().horizontalCount(n->selectChannels));
    unsigned note2 = n->wheelToNote(2);
    SCHMICKLE(n->n().notes[note1].pitch() < n->n().notes[note2].pitch());
    Press(n, n->cutButton);
    SCHMICKLE(!IsEmpty(n));
    Press(n, n->cutButton);
    SCHMICKLE(IsEmpty(n));

    DEBUG("press select button with empty score");
    n->resetControls();
    SCHMICKLE(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    SCHMICKLE(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    SCHMICKLE(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    SCHMICKLE(n->selectButton->editStart());
    ExerciseWheels(n);
    n->n().validate();

    DEBUG("press part button with empty score");
    n->resetControls();
    SCHMICKLE(!n->partButton->ledOn());
    ExerciseWheels(n);
    Press(n, n->partButton);
    SCHMICKLE(n->partButton->ledOn());
    ExerciseWheels(n);
    Press(n, n->partButton);
    SCHMICKLE(!n->partButton->ledOn());
    Press(n, n->partButton);
    Press(n, n->selectButton);
    ExerciseWheels(n);
    Press(n, n->cutButton);
    ExerciseWheels(n);
    n->n().validate();

    DEBUG("duplicate");
    AddTwoNotes(n);
    Press(n, n->selectButton);
    WheelLeft(n, 0);
    Press(n, n->selectButton);
    WheelLeft(n, 2);
    Press(n, n->insertButton);
    SCHMICKLE(6 == n->n().notes.size());
    SCHMICKLE(4 == n->n().horizontalCount(n->selectChannels));

    DEBUG("copy and paste");
    AddTwoNotes(n);
    Press(n, n->selectButton);
    WheelLeft(n, 0);
    DEBUG("cnp wheel left 0");
    n->debugDump();
    Press(n, n->selectButton);
    WheelLeft(n, 2);
    DEBUG("cnp wheel left 2");
    n->debugDump();
    Press(n, n->selectButton);
    Press(n, n->selectButton);
    WheelLeft(n, 1);
    DEBUG("cnp wheel left 1");
    n->debugDump();
    Press(n, n->insertButton);
    SCHMICKLE(6 == n->n().notes.size());
    SCHMICKLE(4 == n->n().horizontalCount(n->selectChannels));

    DEBUG("restore defaults");
    ResetState(n);
    n->resetControls();
    n->fromJson(saveState);
    json_decref(saveState);
    n->invalAndPlay(Inval::load);
}

static void TestEncode() {
    Notes n;
    n.notes.clear();
    int start = 0;
    for (auto type : { MIDI_HEADER, KEY_SIGNATURE, TIME_SIGNATURE, MIDI_TEMPO, NOTE_ON,
            REST_TYPE, TRACK_END }) {
        DisplayNote note(type, start);
        if (note.isNoteOrRest()) {
            note.duration = n.ppq;
            if (NOTE_ON == note.type) {
                note.setPitch(60);
            }
        }
        start += note.duration;
        n.notes.push_back(note);
    }
    vector<std::string> results;
    for (const auto& note : n.notes) {
        results.push_back(note.debugString());
        DEBUG("n.notes %s", results.back().c_str());
    }
    vector<uint8_t> midi;
    Notes::Serialize(n.notes, midi);
    DEBUG("raw midi");
    NoteTakerParseMidi::DebugDumpRawMidi(midi);
    vector<char> encoded;
    NoteTakerSlot::Encode(midi, &encoded);
    encoded.push_back('\0'); // treat as string
    const char* encodedString = &encoded.front();
    DEBUG("encoded midi %s", encodedString);
    vector<char> encoded2(encodedString, encodedString + strlen(encodedString));
    DEBUG("encoded2     %.*s", encoded2.size(), &encoded2.front());
    NoteTakerSlot::Decode(encoded2, &midi);
    DEBUG("raw midi2");
    NoteTakerParseMidi::DebugDumpRawMidi(midi);
    Notes n2;
    bool result = Notes::Deserialize(midi, &n2.notes, &n2.ppq);
    vector<std::string> results2;
    for (const auto& note : n2.notes) {
        results2.push_back(note.debugString());
        DEBUG("n2.notes %s", results2.back().c_str());
    }
    SCHMICKLE(result);
    SCHMICKLE(results == results2);
}

void UnitTest(NoteTakerWidget* n, TestType test) {
    n->unitTestRunning = true;
    switch (test) {
        case TestType::encode:
            TestEncode();
            break;
        case TestType::digit:
            LowLevelTestDigitsSolo(n);
            LowLevelTestDigits(n);
            break;
        case TestType::random:
            LowLevelRandom(n, 2, 10);
            break;
        case TestType::expected:
            Expected(n);
            break;
        default:
            _schmickled();
    }
    n->unitTestRunning = false;
}

#endif
