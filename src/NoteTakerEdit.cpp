#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTakerWheel.hpp"

void NoteTaker::setWheelRange() {
    if (!runButton || runButton->running()) {
        return;
    }
    if (fileButton->ledOn) {
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        verticalWheel->setLimits(0, 10);
        verticalWheel->speed = 1;
        verticalWheel->setValue(5);
        debug("horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
                horizontalWheel->maxValue);
        debug("vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
                verticalWheel->maxValue);
        return;
    }
    if (sustainButton->ledOn) {
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
        int sustainMinDuration = channels[this->firstChannel()].sustainMin;
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(sustainMinDuration));
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->speed = 1;
        verticalWheel->setValue(2.5f); // sustain min
        return;
    }
    if (partButton->ledOn && selectButton->ledOn) {
        verticalWheel->setLimits(0, CV_OUTPUTS);
        verticalWheel->speed = 1;
        verticalWheel->setValue(partButton->lastChannels);
    }
    if (isEmpty()) {
        return;
    }
    const DisplayNote* note = &allNotes[selectStart];
    if (!partButton->ledOn || !selectButton->ledOn) {
        verticalWheel->setLimits(0, 127);  // range for midi pitch
        verticalWheel->speed = .1;
        if (NOTE_ON == note->type) {
            verticalWheel->setValue(note->pitch());
        }
    }
    // horizontal wheel range and value
    int wheelMin = selectButton->editStart() ? -1 : 0;
    int wheelMax = (int) this->horizontalCount() + wheelMin;
    if (!selectButton->ledOn) {
        // range is 0 to NoteTakerDisplay.durations.size(); find value in array values
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
    } else {
        horizontalWheel->setLimits(wheelMin, wheelMax);
    }
    if (!selectButton->ledOn) {
        horizontalWheel->setValue(REST_TYPE == note->type ? note->rest() : note->note());
    } else {
        int index = this->noteToWheel(*note);
        if (index < wheelMin || index > wheelMax) {
            debug("! note type %d index %d wheelMin %d wheelMax %d",
                    note->type, index, wheelMin, wheelMax);
            this->debugDump();
            assert(0);
        }
        horizontalWheel->setValue(index);
    }
    debug("horz %g (%g %g)", horizontalWheel->value, horizontalWheel->minValue,
            horizontalWheel->maxValue);
    debug("vert %g (%g %g)", verticalWheel->value, verticalWheel->minValue,
            verticalWheel->maxValue);
}

void NoteTaker::updateHorizontal() {
    if (runButton->running()) {
        return;
    }
    if (fileButton->ledOn) {
        // to do : nudge to next valid value once I figure out how that should work, exactly
        return;
    }
    const int wheelValue = (int) horizontalWheel->value;
    if (wheelValue == lastHorizontal) {
        return;
    }
    lastHorizontal = wheelValue;
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[this->firstChannel()];
        channel.setLimit((NoteTakerChannel::Limit) verticalWheel->value, noteDurations[wheelValue]);
        return;
    }
    if (isEmpty()) {
        return;
    }
    bool noteChanged = false;
    if (!selectButton->ledOn) {
        int diff = 0;
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff;
            if (this->isSelectable(note)) {
                if (NOTE_ON == note.type) {
                    assert((unsigned) wheelValue < noteDurations.size());
                    note.setNote(wheelValue);
                } else {
                    note.setRest(wheelValue);
                }
                int duration = noteDurations[wheelValue];
                diff += duration - note.duration;
                note.duration = duration;
            }
        }
        if (diff) {
            this->shiftNotes(selectEnd, diff);
            noteChanged = true;
            display->xPositionsInvalid = true;
            this->setSelect(selectStart, selectEnd);
        }
    } else {
    // value should range from 0 to max - 1, where max is number of starts for active channels
    // if insert mode, value ranges from -1 to max - 1
        unsigned index = this->wheelToNote(wheelValue);
        noteChanged = selectButton->editEnd() ? this->setSelectEnd(wheelValue, index) :
                this->setSelectStart(index);
    }
    if (noteChanged) {
        this->playSelection();
    }
}
    
void NoteTaker::updateVertical() {
    if (runButton->running()) {
        return;
    }
    const int wheelValue = (int) verticalWheel->value;
    if (wheelValue == lastVertical) {
        return;
    }
    lastVertical = wheelValue;
    if (fileButton->ledOn) {
        if (verticalWheel->value <= 2) {
            saving = true;
            this->saveScore();
        }
        if (verticalWheel->value >= 8 && (unsigned) horizontalWheel->value < storage.size()) {
            debug("updateVertical loadScore");
            loading = true;
            this->loadScore();
        }
        return;
    }
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[this->firstChannel()];
        int sustainDuration = INT_MAX;
        switch(wheelValue) {
            case 3: // to do : create enum to identify values
                sustainDuration = channel.sustainMax;
                break;
            case 2:
                sustainDuration = channel.sustainMin;
                break;
            case 1:
                sustainDuration = channel.releaseMin;
                break;
            case 0:
                sustainDuration = channel.releaseMax;
                break;
            default:
                assert(0);
        }
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(sustainDuration));
    }
    if (partButton->ledOn && selectButton->ledOn) {
        selectChannels = 1 << wheelValue;
        return;
    }
    // transpose selection
    // loop below computes diff of first note, and adds diff to subsequent notes in select
    int diff = 0;
    for (unsigned index = selectStart ; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        if (!this->isSelectable(note)) {
            continue;
        }
        int value;
        if (!diff) {
            value = wheelValue;
            diff = value - note.pitch();
            if (!diff) {
                return;
            }
        } else {
            value = std::max(0, std::min(127, note.pitch() + diff));
        }
        note.setPitch(value);
    }
    this->playSelection();
}
