#include "SchmickleWorks.hpp"

#if RUN_UNIT_TEST

#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"
#include "NoteTaker.hpp"

static void Press(NoteTakerWidget* n, ParamWidget* ms) {
    event::DragStart evs;
    ms->onDragStart(evs);
    event::DragEnd evd;
    ms->onDragEnd(evd);
    if (n->debugVerbose) n->debugDump();
}

static HorizontalWheel* HWheel(NoteTakerWidget* n) {
    return n->getFirstDescendantOfType<HorizontalWheel>();
}

static VerticalWheel* VWheel(NoteTakerWidget* n) {
    return n->getFirstDescendantOfType<VerticalWheel>();
}

static void WheelUp(NoteTakerWidget* n, float value) {
    VWheel(n)->lastValue = INT_MAX;
    assert(VWheel(n)->paramQuantity->minValue <= VWheel(n)->paramQuantity->maxValue);
    VWheel(n)->setValue(
            std::max(VWheel(n)->paramQuantity->minValue,
            std::min(VWheel(n)->paramQuantity->maxValue, value)));
    n->updateVertical();
}

static void WheelLeft(NoteTakerWidget* n, float value) {
    HWheel(n)->lastValue = INT_MAX;
    assert(HWheel(n)->paramQuantity->minValue <= HWheel(n)->paramQuantity->maxValue);
    HWheel(n)->setValue(
         std::max(HWheel(n)->paramQuantity->minValue,
         std::min(HWheel(n)->paramQuantity->maxValue, value)));
    n->updateHorizontal();
}

static void ExerciseWheels(NoteTakerWidget* n) {
    for (float v : { VWheel(n)->paramQuantity->minValue, VWheel(n)->paramQuantity->maxValue, 
            (VWheel(n)->paramQuantity->minValue + VWheel(n)->paramQuantity->maxValue) / 2 }) {
        WheelUp(n, v);
    }
    for (float v : { HWheel(n)->paramQuantity->minValue, HWheel(n)->paramQuantity->maxValue, 
            (HWheel(n)->paramQuantity->minValue + HWheel(n)->paramQuantity->maxValue) / 2 }) {
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
            Press(n, n->widget<RunButton>());
        break;
        case NoteTaker::EXTEND_BUTTON:
            Press(n, n->widget<SelectButton>());
        break;
        case NoteTaker::INSERT_BUTTON:
            Press(n, n->widget<InsertButton>());
        break;
        case NoteTaker::CUT_BUTTON:
            Press(n, n->widget<CutButton>());
        break;
        case NoteTaker::REST_BUTTON:
            Press(n, n->widget<RestButton>());
        break;
        case NoteTaker::PART_BUTTON:
            Press(n, n->widget<PartButton>());
        break;
        case NoteTaker::FILE_BUTTON:
            Press(n, n->widget<FileButton>());
        break;
        case NoteTaker::SUSTAIN_BUTTON:
            Press(n, n->widget<SustainButton>());
        break;
        case NoteTaker::TIME_BUTTON:
            Press(n, n->widget<TimeButton>());
        break;
        case NoteTaker::KEY_BUTTON:
            Press(n, n->widget<KeyButton>());
        break;
        case NoteTaker::TIE_BUTTON:
            Press(n, n->widget<TieButton>());
        break;
        case NoteTaker::TRILL_BUTTON:
            Press(n, n->widget<TrillButton>());
        break;
        case NoteTaker::TEMPO_BUTTON:
            Press(n, n->widget<TempoButton>());
        break;
        case NoteTaker::VERTICAL_WHEEL: {
            float value = VWheel(n)->paramQuantity->minValue + ((float) rand() / RAND_MAX
                    * (VWheel(n)->paramQuantity->maxValue - VWheel(n)->paramQuantity->minValue));
            WheelUp(n, value);  
        } break;
        case NoteTaker::HORIZONTAL_WHEEL: {
            float value = HWheel(n)->paramQuantity->minValue + ((float) rand() / RAND_MAX
                    * (HWheel(n)->paramQuantity->maxValue - HWheel(n)->paramQuantity->minValue));
            WheelLeft(n, value);
        } break;
        case NoteTaker::NUM_PARAMS + NoteTaker::V_OCT_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::CLOCK_INPUT: 
        case NoteTaker::NUM_PARAMS + NoteTaker::RESET_INPUT: {
            float value = -10 + (float) rand() / RAND_MAX * 20;
            n->nt()->inputs[control - NoteTaker::NUM_PARAMS].value = value;
        } break;
        default:
            assert(0);  // shouldn't land here
    }
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
            n->nt()->resetState();
            n->resetControls();
            for (index = 0; index < digits; ++index) {
                LowLevelAction(n, counters[index]);
                n->validate();
            }
        } while (true);
    }
}

static void LowLevelTestDigitsSolo(NoteTakerWidget* n) {
    int counters[] = {2, 5, 3, 4, 0, 0};
    int digits = 5;
    n->nt()->resetState();
    n->resetControls();
    for (int index = 0; index < digits; ++index) {
        LowLevelAction(n, counters[index]);
    }
}

static void LowLevelRandom(NoteTakerWidget* n, unsigned seed, int steps) {
    srand(seed);
    n->nt()->resetState();
    n->resetControls();
    for (int index = 0; index < steps; ++index) {
        int control = rand() % (NoteTaker::NUM_PARAMS + NoteTaker::NUM_INPUTS);
        LowLevelAction(n, control);
    }
}

static void AddTwoNotes(NoteTakerWidget* n) {
    n->nt()->resetState();
    Press(n, n->widget<InsertButton>());
    Press(n, n->widget<InsertButton>());
    unsigned note1 = n->wheelToNote(1);
    WheelUp(n, n->nt()->notes[note1].pitch() + 1);
}

static void Expected(NoteTakerWidget* n) {
    json_t* saveState = n->toJson();
    n->nt()->onReset();
    debug("delete a note with empty score");
    Press(n, n->widget<CutButton>());

    debug("add a note with empty score, delete same");
    Press(n, n->widget<InsertButton>());
    assert(!n->isEmpty());
    Press(n, n->widget<CutButton>());
    assert(n->isEmpty());

    debug("add two notes with empty score, delete same");
    Press(n, n->widget<InsertButton>());
    Press(n, n->widget<InsertButton>());
    Press(n, n->widget<CutButton>());
    assert(!n->isEmpty());
    Press(n, n->widget<CutButton>());
    assert(n->isEmpty());

    debug("add two notes with empty score, check order");
    AddTwoNotes(n);
    unsigned note1 = n->wheelToNote(1);
    assert(4 == n->nt()->notes.size());
    assert(2 == n->horizontalCount());
    unsigned note2 = n->wheelToNote(2);
    assert(n->nt()->notes[note1].pitch() < n->nt()->notes[note2].pitch());
    Press(n, n->widget<CutButton>());
    assert(!n->isEmpty());
    Press(n, n->widget<CutButton>());
    assert(n->isEmpty());

    debug("press select button with empty score");
    n->resetControls();
    assert(n->widget<SelectButton>()->editStart());
    ExerciseWheels(n);
    Press(n, n->widget<SelectButton>());
    assert(n->widget<SelectButton>()->editStart());
    ExerciseWheels(n);
    Press(n, n->widget<SelectButton>());
    assert(n->widget<SelectButton>()->editStart());
    ExerciseWheels(n);
    Press(n, n->widget<SelectButton>());
    assert(n->widget<SelectButton>()->editStart());
    ExerciseWheels(n);
    n->validate();

    debug("press part button with empty score");
    n->resetControls();
    assert(!n->widget<PartButton>()->ledOn);
    ExerciseWheels(n);
    Press(n, n->widget<PartButton>());
    assert(n->widget<PartButton>()->ledOn);
    ExerciseWheels(n);
    Press(n, n->widget<PartButton>());
    assert(!n->widget<PartButton>()->ledOn);
    Press(n, n->widget<PartButton>());
    Press(n, n->widget<SelectButton>());
    ExerciseWheels(n);
    Press(n, n->widget<CutButton>());
    ExerciseWheels(n);
    n->validate();

    debug("duplicate");
    AddTwoNotes(n);
    Press(n, n->widget<SelectButton>());
    WheelLeft(n, 0);
    Press(n, n->widget<SelectButton>());
    WheelLeft(n, 2);
    Press(n, n->widget<InsertButton>());
    assert(6 == n->nt()->notes.size());
    assert(4 == n->horizontalCount());

    debug("copy and paste");
    AddTwoNotes(n);
    Press(n, n->widget<SelectButton>());
    WheelLeft(n, 0);
    debug("cnp wheel left 0");
    n->debugDump();
    Press(n, n->widget<SelectButton>());
    WheelLeft(n, 2);
    debug("cnp wheel left 2");
    n->debugDump();
    Press(n, n->widget<SelectButton>());
    Press(n, n->widget<SelectButton>());
    WheelLeft(n, 1);
    debug("cnp wheel left 1");
    n->debugDump();
    Press(n, n->widget<InsertButton>());
    assert(6 == n->nt()->notes.size());
    assert(4 == n->horizontalCount());

    debug("restore defaults");
    n->nt()->resetState();
    n->resetControls();
    n->fromJson(saveState);
    json_decref(saveState);
    n->widget<NoteTakerDisplay>()->invalidateCache();
    n->widget<NoteTakerDisplay>()->rangeInvalid = true;
}

void UnitTest(NoteTakerWidget* n, TestType test) {
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
