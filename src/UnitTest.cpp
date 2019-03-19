#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

static void Press(NoteTaker* n, MomentarySwitch* ms) {
    EventDragStart evs;
    ms->onDragStart(evs);
    EventDragEnd evd;
    ms->onDragEnd(evd);
    n->debugDump();
}

static void WheelUp(NoteTaker* n, float value) {
    n->verticalWheel->value = value;
    EventDragMove evm;
    n->verticalWheel->onDragMove(evm);
}

static void WheelLeft(NoteTaker* n, float value) {
    n->horizontalWheel->value = value;
    EventDragMove evm;
    n->horizontalWheel->onDragMove(evm);
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

static void AddTwoNotes(NoteTaker* n) {
    n->reset();
    Press(n, n->insertButton);
    Press(n, n->insertButton);
    unsigned note1 = n->wheelToNote(1);
    WheelUp(n, n->allNotes[note1].pitch() + 1);
}

void UnitTest(NoteTaker* n) {
    if (true) return;  // to do : remove, temporary, while debugging
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
    assert(4 == n->allNotes.size());
    assert(2 == n->horizontalCount());
    unsigned note2 = n->wheelToNote(2);
    assert(n->allNotes[note1].pitch() < n->allNotes[note2].pitch());
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
    assert(6 == n->allNotes.size());
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
    assert(6 == n->allNotes.size());
    assert(4 == n->horizontalCount());

    debug("restore defaults");
    n->reset();
}
