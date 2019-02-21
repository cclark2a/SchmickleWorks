#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerParseMidi.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTaker.hpp"

static void Press(MomentarySwitch* ms) {
    EventDragStart evs;
    ms->onDragStart(evs);
    EventDragEnd evd;
    ms->onDragEnd(evd);
}

static void ResetButtons(NoteTaker* n) {
    n->selectButton->setOff();
    n->partButton->setOff();
    n->setWheelRange();
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

static void SetScoreEmpty(NoteTaker* n) {
    n->initialize();
    vector<uint8_t> emptyMidi;
    NoteTakerMakeMidi makeMidi;
    makeMidi.createEmpty(emptyMidi);
    NoteTakerParseMidi emptyParser(emptyMidi, n->allNotes);
    emptyParser.parseMidi();
    ResetButtons(n);
}

static void AddTwoNotes(NoteTaker* n) {
    SetScoreEmpty(n);
    ResetButtons(n);
    Press(n->insertButton);
    Press(n->insertButton);
    unsigned note1 = n->wheelToNote(0);
    WheelUp(n, n->allNotes[note1].pitch() + 1);
}

void UnitTest(NoteTaker* n) {
    SetScoreEmpty(n);

    debug("delete a note with empty score");
    Press(n->cutButton);

    debug("add a note with empty score, delete same");
    Press(n->insertButton);
    assert(!n->isEmpty());
    Press(n->cutButton);
    assert(n->isEmpty());

    debug("add two notes with empty score, delete same");
    Press(n->insertButton);
    Press(n->insertButton);
    Press(n->cutButton);
    assert(!n->isEmpty());
    Press(n->cutButton);
    assert(n->isEmpty());

    debug("add two notes with empty score, check order");
    AddTwoNotes(n);
    unsigned note1 = n->wheelToNote(0);
    assert(4 == n->allNotes.size());
    assert(2 == n->horizontalCount());
    unsigned note2 = n->wheelToNote(1);
    assert(n->allNotes[note1].pitch() < n->allNotes[note2].pitch());
    Press(n->cutButton);
    assert(!n->isEmpty());
    Press(n->cutButton);
    assert(n->isEmpty());

    debug("press select button with empty score");
    ResetButtons(n);
    assert(!n->selectButton->ledOn);
    ExerciseWheels(n);
    Press(n->selectButton);
    assert(n->selectButton->editStart());
    ExerciseWheels(n);
    Press(n->selectButton);
    assert(n->selectButton->editEnd());
    ExerciseWheels(n);
    Press(n->selectButton);
    assert(!n->selectButton->ledOn && SelectButton::State::ledOff == n->selectButton->state);
    ExerciseWheels(n);

    debug("press part button with empty score");
    ResetButtons(n);
    assert(!n->partButton->ledOn);
    ExerciseWheels(n);
    Press(n->partButton);
    assert(n->partButton->ledOn);
    ExerciseWheels(n);
    Press(n->partButton);
    assert(!n->partButton->ledOn);
    Press(n->partButton);
    Press(n->selectButton);
    ExerciseWheels(n);
    Press(n->cutButton);
    ExerciseWheels(n);

    debug("duplicate");
    AddTwoNotes(n);
    Press(n->selectButton);
    WheelLeft(n, -1);
    Press(n->selectButton);
    WheelLeft(n, 2);
    Press(n->insertButton);
    assert(6 == n->allNotes.size());
    assert(4 == n->horizontalCount());

    debug("copy and paste");
    AddTwoNotes(n);
    Press(n->selectButton);
    WheelLeft(n, 0);
    debug("cnp wheel left 0");
    n->debugDump();
    Press(n->selectButton);
    WheelLeft(n, 2);
    debug("cnp wheel left 2");
    n->debugDump();
    Press(n->selectButton);
    Press(n->selectButton);
    WheelLeft(n, 0);
    debug("cnp wheel left 0");
    n->debugDump();
    Press(n->insertButton);
    n->debugDump();
    assert(5 == n->allNotes.size());
    assert(3 == n->horizontalCount());

    debug("restore defaults");
    n->initialize();
    n->setUpSampleNotes();
    ResetButtons(n);
}