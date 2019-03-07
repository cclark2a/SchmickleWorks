#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"

void NoteTaker::setVerticalWheelRange() {
    if (!runButton) {
        return;
    }
    if (this->isRunning()) {
        verticalWheel->setLimits(0, 127);    // v/oct transpose (5 octaves up, down)
        verticalWheel->setValue(60);
        verticalWheel->speed = .1;
        return;
    }
    if (fileButton->ledOn) {
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        verticalWheel->speed = 1;
        return;
    }
    if (sustainButton->ledOn) {
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->setValue(2.5f); // sustain min
        verticalWheel->speed = 1;
        return;
    }
    if (partButton->ledOn) {
        verticalWheel->setLimits(0, CV_OUTPUTS);
        verticalWheel->setValue(partButton->lastChannels);
        verticalWheel->speed = 1;
    }
    if (isEmpty()) {
        return;
    }
    DisplayNote* note = &allNotes[selectStart];
    if (selectStart + 1 == selectEnd) {
        if (KEY_SIGNATURE == note->type) {
            verticalWheel->setLimits(-7, 7);
            verticalWheel->setValue(note->key());
            verticalWheel->speed = 1;
            return;
        }
        if (TIME_SIGNATURE == note->type) {
            verticalWheel->setLimits(0, 1);
            verticalWheel->setValue(0);
            verticalWheel->speed = 1;
            return;
        }
    }
    unsigned start = selectStart;
    while (start < selectEnd && (NOTE_ON != note->type || !note->isSelectable(selectChannels))) {
        note->selected = false;
        note = &allNotes[++start];
    }
    bool validNote = start < selectEnd && NOTE_ON == note->type;
    if (!partButton->ledOn) {
        verticalWheel->setLimits(0, 127);  // range for midi pitch
        verticalWheel->setValue(validNote ? note->pitch() : 60);
        verticalWheel->speed = .1;
    } else if (!selectButton->ledOn) {
        verticalWheel->setValue(validNote ? note->channel : 1);
        while (start < selectEnd) {
            note->selected = NOTE_ON == note->type && note->isSelectable(selectChannels);
            note = &allNotes[++start];
        }
    }
}

void NoteTaker::setHorizontalWheelRange() {
    if (!runButton) {
        return;
    }
    if (this->isRunning()) {
        horizontalWheel->setLimits(0, (noteDurations.size() - 1) - .001f); // tempo relative to quarter note
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(stdTimePerQuarterNote));
        horizontalWheel->speed = 1;
        return;
    }
    if (fileButton->ledOn) {
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        horizontalWheel->speed = 1;
        return;
    }
    if (sustainButton->ledOn) {
        horizontalWheel->setLimits(0, noteDurations.size() - 1);
        int sustainMinDuration = channels[this->firstChannel()].sustainMin;
        horizontalWheel->setValue(NoteTakerDisplay::DurationIndex(sustainMinDuration));
        horizontalWheel->speed = 1;
        return;
    }
    if (isEmpty()) {
        return;
    }
    const DisplayNote* note = &allNotes[selectStart];
    // horizontal wheel range and value
    float horzSpeed = 1;
    int value = INT_MAX;
    if (!selectButton->ledOn) {
        if (selectStart + 1 == selectEnd
                && (KEY_SIGNATURE == note->type || TIME_SIGNATURE == note->type)) {
            if (TIME_SIGNATURE == note->type) {
                horizontalWheel->setLimits(1, 99.99);   // denom limit 0 to 6 (2^N, 1 to 64)
                horzSpeed = .1;
                value = note->numerator();
            }
            // otherwise, do nothing for key signature
        } else {
            unsigned start = selectStart;
            while ((KEY_SIGNATURE == note->type || TIME_SIGNATURE == note->type)
                    && start < selectEnd) {
                note = &allNotes[++start];
            }
            if (NOTE_ON == note->type) {
                value = note->note();
            } else if (REST_TYPE == note->type) {
                value = note->rest();
            }
            if (INT_MAX != value) {
                horizontalWheel->setLimits(0, noteDurations.size() - 1);
            }
        }
    } else {
        int wheelMin = selectButton->editStart() ? -1 : 0;
        int wheelMax = (int) this->horizontalCount() + wheelMin;
        horizontalWheel->setLimits(wheelMin, wheelMax);
        value = this->noteToWheel(*note);
        if (value < wheelMin || value > wheelMax) {
            debug("! note type %d value %d wheelMin %d wheelMax %d",
                    note->type, value, wheelMin, wheelMax);
            this->debugDump();
            assert(0);
        }
    }
    if (INT_MAX != value) {
        horizontalWheel->setValue(value);
        horizontalWheel->speed = horzSpeed;
    }
    return;
}

void NoteTaker::setWheelRange() {
    this->setHorizontalWheelRange();
    this->setVerticalWheelRange();
    debug("setWheelRange %s %s", horizontalWheel->debugString().c_str(),
            verticalWheel->debugString().c_str());
}

void NoteTaker::updateHorizontal() {
    if (this->isRunning()) {
        return;
    }
    if (fileButton->ledOn) {
        // to do : nudge to next valid value once I figure out how that should work, exactly
        return;
    }
    if (!horizontalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = horizontalWheel->wheelValue();
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
                switch (note.type) {
                    case NOTE_ON:
                        assert((unsigned) wheelValue < noteDurations.size());
                        note.setNote(wheelValue);
                        break;
                    case REST_TYPE:
                        note.setRest(wheelValue);
                        break;
                    case KEY_SIGNATURE:
                        break;
                    case TIME_SIGNATURE:
                        if (selectStart + 1 == selectEnd) {
                            if ((int) verticalWheel->value) {
                                note.setNumerator(wheelValue);
                            } else {
                                note.setDenominator(wheelValue);
                            }
                            display->xPositionsInvalid = true;
                            display->updateXPosition();
                        }
                        break;
                    default:
                        assert(0);
                }
                if (KEY_SIGNATURE != note.type && TIME_SIGNATURE != note.type) {
                    int duration = noteDurations[wheelValue];
                    diff += duration - note.duration;
                    note.duration = duration;
                }
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
        if (noteChanged) {
            this->setVerticalWheelRange();
        }
    }
    if (noteChanged) {
        this->playSelection();
    }
}
    
void NoteTaker::updateVertical() {
    if (this->isRunning()) {
        return;
    }
    if (!verticalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = verticalWheel->wheelValue();
    if (fileButton->ledOn) {
        if (verticalWheel->value <= 2) {
            display->saving = true;
            this->saveScore();
        }
        if (verticalWheel->value >= 8 && (unsigned) horizontalWheel->value < storage.size()) {
            debug("updateVertical loadScore");
            display->loading = true;
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
        switch (note.type) {
            case KEY_SIGNATURE:
                if (selectStart + 1 == selectEnd && !partButton->ledOn) {
                    note.setKey(wheelValue);
                    display->xPositionsInvalid = true;
                    display->updateXPosition();
                }
                break;
            case TIME_SIGNATURE:
                if (selectStart + 1 == selectEnd && !partButton->ledOn && !selectButton->ledOn) {
                    if (!wheelValue) {
                        horizontalWheel->setLimits(0, 6.99);   // denom limit 0 to 6 (2^N, 1 to 64)
                        horizontalWheel->value = note.numerator();
                        horizontalWheel->speed = 1;
                    } else {
                        horizontalWheel->setLimits(1, 99.99);   // numer limit 0 to 99
                        horizontalWheel->value = note.denominator();
                        horizontalWheel->speed = .1;
                    }
                }
                break;
            case NOTE_ON: {
                if (partButton->ledOn ? !note.selected : !note.isSelectable(selectChannels)) {
                    continue;
                }
                if (partButton->ledOn) {
                    note.setChannel(wheelValue);
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
                break;
            }
            default:
                ;
        }
    }
    this->playSelection();
}
