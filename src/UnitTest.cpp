#include "SchmickleWorks.hpp"

#if RUN_UNIT_TEST

#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

static void Press(NoteTaker* n, MomentarySwitch* ms) {
    EventDragStart evs;
    ms->onDragStart(evs);
    EventDragEnd evd;
    ms->onDragEnd(evd);
    if (n->debugVerbose) n->debugDump();
}

static void WheelUp(NoteTaker* n, float value) {
    n->verticalWheel->lastValue = INT_MAX;
    assert(n->verticalWheel->minValue <= n->verticalWheel->maxValue);
    n->verticalWheel->value = 
            std::max(n->verticalWheel->minValue, std::min(n->verticalWheel->maxValue, value));
    n->updateVertical();
}

static void WheelLeft(NoteTaker* n, float value) {
    n->horizontalWheel->lastValue = INT_MAX;
    assert(n->horizontalWheel->minValue <= n->horizontalWheel->maxValue);
    n->horizontalWheel->value =
         std::max(n->horizontalWheel->minValue, std::min(n->horizontalWheel->maxValue, value));
    n->updateHorizontal();
}

static void ExerciseWheels(NoteTaker* n) {
    for (float v : { n->verticalWheel->minValue, n->verticalWheel->maxValue, 
            (n->verticalWheel->minValue + n->verticalWheel->maxValue) / 2 }) {
        WheelUp(n, v);
    }
    for (float v : { n->horizontalWheel->minValue, n->horizontalWheel->maxValue, 
            (n->horizontalWheel->minValue + n->horizontalWheel->maxValue) / 2 }) {
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
static void LowLevelAction(NoteTaker* n, int control) {
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
        case NoteTaker::TRILL_BUTTON:
            Press(n, n->trillButton);
        break;
        case NoteTaker::TEMPO_BUTTON:
            Press(n, n->tempoButton);
        break;
        case NoteTaker::VERTICAL_WHEEL: {
            float value = n->verticalWheel->minValue + ((float) rand() / RAND_MAX
                    * (n->verticalWheel->maxValue - n->verticalWheel->minValue));
            WheelUp(n, value);  
        } break;
        case NoteTaker::HORIZONTAL_WHEEL: {
            float value = n->horizontalWheel->minValue + ((float) rand() / RAND_MAX
                    * (n->horizontalWheel->maxValue - n->horizontalWheel->minValue));
            WheelLeft(n, value);
        } break;
        case NoteTaker::NUM_PARAMS + NoteTaker::V_OCT_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::CLOCK_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::RESET_INPUT: {
            float value = -10 + (float) rand() / RAND_MAX * 20;
            n->inputs[control - NoteTaker::NUM_PARAMS].value = value;
        } break;
        default:
            assert(0);  // shouldn't land here
    }
}

static void LowLevelTestDigits(NoteTaker* n) {
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
            n->resetState();
            for (index = 0; index < digits; ++index) {
                LowLevelAction(n, counters[index]);
                n->validate();
            }
        } while (true);
    }
}

static void LowLevelTestDigitsSolo(NoteTaker* n) {
    int counters[] = {2, 5, 3, 4, 0, 0};
    int digits = 5;
    n->resetState();
    for (int index = 0; index < digits; ++index) {
        LowLevelAction(n, counters[index]);
    }
}

static void LowLevelRandom(NoteTaker* n, unsigned seed, int steps) {
    srand(seed);
    n->resetState();
    for (int index = 0; index < steps; ++index) {
        int control = rand() % (NoteTaker::NUM_PARAMS + NoteTaker::NUM_INPUTS);
        LowLevelAction(n, control);
    }
}

static void AddTwoNotes(NoteTaker* n) {
    n->resetState();
    Press(n, n->insertButton);
    Press(n, n->insertButton);
    unsigned note1 = n->wheelToNote(1);
    WheelUp(n, n->notes[note1].pitch() + 1);
}

static void Expected(NoteTaker* n) {
    json_t* saveState = n->toJson();
    n->reset();
    debug("delete a note with empty score");
    Press(n, n->cutButton);

    debug("add a note with empty score, delete same");
    Press(n, n->insertButton);
    assert(!n->isEmpty());
    Press(n, n->cutButton);
    assert(n->isEmpty());

    debug("add two notes with empty score, delete same");
    Press(n, n->insertButton);
    Press(n, n->insertButton);
    Press(n, n->cutButton);
    assert(!n->isEmpty());
    Press(n, n->cutButton);
    assert(n->isEmpty());

    debug("add two notes with empty score, check order");
    AddTwoNotes(n);
    unsigned note1 = n->wheelToNote(1);
    assert(4 == n->notes.size());
    assert(2 == n->horizontalCount());
    unsigned note2 = n->wheelToNote(2);
    assert(n->notes[note1].pitch() < n->notes[note2].pitch());
    Press(n, n->cutButton);
    assert(!n->isEmpty());
    Press(n, n->cutButton);
    assert(n->isEmpty());

    debug("press select button with empty score");
    n->resetControls();
    assert(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    assert(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    assert(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n, n->selectButton);
    assert(n->selectButton->editStart());
    ExerciseWheels(n);
    n->validate();

    debug("press part button with empty score");
    n->resetControls();
    assert(!n->partButton->ledOn);
    ExerciseWheels(n);
    Press(n, n->partButton);
    assert(n->partButton->ledOn);
    ExerciseWheels(n);
    Press(n, n->partButton);
    assert(!n->partButton->ledOn);
    Press(n, n->partButton);
    Press(n, n->selectButton);
    ExerciseWheels(n);
    Press(n, n->cutButton);
    ExerciseWheels(n);
    n->validate();

    debug("duplicate");
    AddTwoNotes(n);
    Press(n, n->selectButton);
    WheelLeft(n, 0);
    Press(n, n->selectButton);
    WheelLeft(n, 2);
    Press(n, n->insertButton);
    assert(6 == n->notes.size());
    assert(4 == n->horizontalCount());

    debug("copy and paste");
    AddTwoNotes(n);
    Press(n, n->selectButton);
    WheelLeft(n, 0);
    debug("cnp wheel left 0");
    n->debugDump();
    Press(n, n->selectButton);
    WheelLeft(n, 2);
    debug("cnp wheel left 2");
    n->debugDump();
    Press(n, n->selectButton);
    Press(n, n->selectButton);
    WheelLeft(n, 1);
    debug("cnp wheel left 1");
    n->debugDump();
    Press(n, n->insertButton);
    assert(6 == n->notes.size());
    assert(4 == n->horizontalCount());

    debug("restore defaults");
    n->resetState();
    n->fromJson(saveState);
    json_decref(saveState);
    n->display->invalidateCache();
    n->display->rangeInvalid = true;
}

void UnitTest(NoteTaker* n, TestType test) {
    n->unitTestRunning = true;
    switch (test) {
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
            assert(0);
    }
    n->unitTestRunning = false;
}

#endif
