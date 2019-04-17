#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"

void NoteTaker::setHorizontalWheelRange() {
    if (!runButton) {
        return;
    }
    if (this->isRunning()) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - .001f); // tempo relative to quarter note
        horizontalWheel->setValue(NoteDurations::FromStd(stdTimePerQuarterNote));
        horizontalWheel->speed = 1;
        return;
    }
    if (fileButton->ledOn) {
        // to do : here, and in other places: set speed proportional to limits
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        horizontalWheel->speed = 1;
        return;
    }
    if (partButton->ledOn) {
        horizontalWheel->setLimits(-1, CV_OUTPUTS);
        horizontalWheel->setValue(partButton->addChannel);
        horizontalWheel->speed = 1;
        return;
    }
    if (sustainButton->ledOn) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - 1);
        int sustainMinDuration = channels[partButton->addChannel].sustainMin;
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainMinDuration, ppq));
        horizontalWheel->speed = 1;
        return;
    }
    if (tieButton->ledOn) {
        horizontalWheel->setLimits(0, 2);
        horizontalWheel->setValue(1);
        horizontalWheel->speed = 1;
        return;
    }
    // horizontal wheel range and value
    float horzSpeed = 1;
    int value = INT_MAX;
    if (!selectButton->ledOn) {
        const DisplayNote* note = &allNotes[selectStart];
        if (selectStart + 1 == selectEnd && note->isSignature()) {
            switch (note->type) {
                case TIME_SIGNATURE:
                    horizontalWheel->setLimits(1, 99.999f);   // denom limit 0 to 6 (2^N, 1 to 64)
                    horzSpeed = .1;
                    value = note->numerator();
                    break;
                case KEY_SIGNATURE:
                    // nothing to do : horizontal wheel doesn't affect key signature
                    break;
                case MIDI_TEMPO: {
                    horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
                    float fValue = NoteDurations::FromStd(note->tempo() * stdTimePerQuarterNote / 500000);
                    horizontalWheel->setValue(fValue);
                    horizontalWheel->speed = horzSpeed;
                    return;
                }
                default:
                    assert(0); // incomplete
            }
        } else {
            unsigned start = selectStart;
            while (note->isSignature() && start < selectEnd) {
                note = &allNotes[++start];
            }
            if (note->isNoteOrRest()) {
                value = NoteDurations::FromMidi(note->duration, ppq);
            }
            if (INT_MAX != value) {
                horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
            }
        }
    } else {
        int wheelMin = selectButton->editStart() ? 0 : 1;
        float wheelMax = this->horizontalCount() + .999f;
        horizontalWheel->setLimits(wheelMin, wheelMax);
        if (this->isEmpty()) {
            value = 0;
        } else {
            const DisplayNote* note = &allNotes[selectStart];
            value = this->noteToWheel(*note);
            if (value < wheelMin || value > wheelMax) {
                debug("! note type %d value %d wheelMin %d wheelMax %d",
                        note->type, value, wheelMin, wheelMax);
                this->debugDump();
                assert(0);
            }
        }
    }
    if (INT_MAX != value) {
        horizontalWheel->setValue(value);
        horizontalWheel->speed = horzSpeed;
    }
    return;
}

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
    if (fileButton->ledOn || partButton->ledOn) {
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        verticalWheel->speed = 2;
        return;
    }
    if (sustainButton->ledOn) {
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->setValue(2.5f); // sustain min
        verticalWheel->speed = 1;
        return;
    }
    if (isEmpty()) {
        return;
    }
    DisplayNote* note = &allNotes[selectStart];
    if (selectStart + 1 == selectEnd && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                verticalWheel->setLimits(-7, 7);
                verticalWheel->setValue(note->key());
                verticalWheel->speed = 1;
                break;
            case TIME_SIGNATURE:
                verticalWheel->setLimits(0, 1);
                verticalWheel->setValue(0);
                verticalWheel->speed = 1;
                break;
            case MIDI_TEMPO:
                // nothing to do : tempo is note affected by vertical wheel
                break;
            default:
                assert(0); // incomplete
        }
        return;
    }
    unsigned start = selectStart;
    while (start < selectEnd && !note->isSelectable(selectChannels)) {
        note = &allNotes[++start];
    }
    bool validNote = start < selectEnd && NOTE_ON == note->type;
    verticalWheel->setLimits(0, 127);  // range for midi pitch
    verticalWheel->setValue(validNote ? note->pitch() : 60);
    verticalWheel->speed = .1;
}

void NoteTaker::setWheelRange() {
    this->setHorizontalWheelRange();
    this->setVerticalWheelRange();
    debug("setWheelRange %s %s", horizontalWheel->debugString().c_str(),
            verticalWheel->debugString().c_str());
}

void NoteTaker::updateHorizontal() {
    if (fileButton->ledOn) {
        return;
    }
    if (partButton->ledOn) {
        int part = horizontalWheel->part();
        partButton->allChannels = part < 0;
        if (!partButton->allChannels) {
            partButton->addChannel = (uint8_t) part;
            // to do : update selectChannels before part button is turned off ?
        }
        return;
    }
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[partButton->addChannel];
        channel.setLimit((NoteTakerChannel::Limit) verticalWheel->value,
                NoteDurations::ToMidi(horizontalWheel->value, ppq));
        return;
    }
    if (this->isRunning()) {
        return;
    }
    if (tieButton->ledOn) {
        TieButton::State prev = tieButton->state;
        if (horizontalWheel->value < .25f) {
            if (TieButton::State::slur != tieButton->state) {
                this->makeSlur();
                tieButton->state = TieButton::State::slur;
            }
        } else if (horizontalWheel->value > .75f && horizontalWheel->value < 1.25f) {
            if (TieButton::State::normal != tieButton->state) {
                this->makeNormal();
                tieButton->state = TieButton::State::normal;
            }
        } else if (horizontalWheel->value > 1.75f) {
            if (TieButton::State::tuplet != tieButton->state) {
                this->makeTuplet();
                tieButton->state = TieButton::State::tuplet;
            }
        }
        if (prev != tieButton->state) {
            display->invalidateCache();
            display->updateXPosition();
        }
        return;
    }

    if (!selectButton->ledOn && selectStart + 1 == selectEnd) {
        DisplayNote& note = allNotes[selectStart];
        if (MIDI_TEMPO == note.type) {
            note.setTempo(this->wheelToTempo(horizontalWheel->value) * 500000);
            return;
        }
    }

    if (!horizontalWheel->hasChanged()) {
        return;
    }
    if (isEmpty()) {
        return;
    }
    const int wheelValue = horizontalWheel->wheelValue();
    bool noteChanged = false;
    if (!selectButton->ledOn) {
        array<int, CHANNEL_COUNT> diff;
        diff.fill(0);
        for (unsigned index = selectStart; index < selectEnd; ++index) {
            DisplayNote& note = allNotes[index];
            note.startTime += diff[note.channel];
            if (this->isSelectable(note)) {
                switch (note.type) {
                    case NOTE_ON:
                    case REST_TYPE: {
                        assert((unsigned) wheelValue < NoteDurations::Count());
                        int duration = NoteDurations::ToMidi(wheelValue, ppq);
                        diff[note.channel] += duration - note.duration;
                        note.duration = duration;
                        } break;
                    case KEY_SIGNATURE:
                        // nothing to do : key signature is not affected by horizontal wheel
                        break;
                    case TIME_SIGNATURE:
                        if (selectStart + 1 == selectEnd) {
                            if ((int) verticalWheel->value) {
                                note.setNumerator(wheelValue);
                            } else {
                                note.setDenominator(wheelValue);
                            }
                            display->invalidateCache();
                            display->updateXPosition();
                        }
                        break;
                    case MIDI_TEMPO:
                        // handled above, to get full wheel resolution
                        break;
                    default:
                        assert(0);
                }
            }
        }
        for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
            if (diff[chan]) {
                ShiftNotes(allNotes, selectEnd, diff[chan], 1 << chan);
                noteChanged = true;
            }
        }
        if (noteChanged) {
            display->invalidateCache();
            this->setSelect(selectStart, selectEnd);
        }
    } else {
        unsigned start, end;
        if (selectButton->editEnd()) {
            int wheelStart = this->noteToWheel(
                    selectButton->selStart - (int) selectButton->saveZero) + 1;
            start = this->wheelToNote(std::min(wheelValue, wheelStart));
            end = this->wheelToNote(std::max(wheelValue + 1, wheelStart));
            debug("start %u end %u wheelValue %d wheelStart %d",
                    start, end, wheelValue, wheelStart);
        } else {
            start = this->wheelToNote(wheelValue);
            end = this->nextAfter(start, 1);
            debug("start %u end %u wheelValue %d", start, end, wheelValue);
        }
        assert(start < end);
        if (start != selectStart || end != selectEnd) {
            this->setSelect(start, end);
            this->setVerticalWheelRange();
            noteChanged = true;
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
    if (fileButton->ledOn || partButton->ledOn) {
        if (verticalWheel->value <= 2) {
            display->downSelected = true;
            if (fileButton->ledOn) {
                this->saveScore();
            } else {
                int part = horizontalWheel->part();
                if (part < 0) {
                    selectChannels = ALL_CHANNELS;
                } else {
                    selectChannels |= 1 << part;
                }
            }
        }
        if (verticalWheel->value >= 8) {
            display->upSelected = true;
            if (fileButton->ledOn && (unsigned) horizontalWheel->value < storage.size()) {
                this->loadScore();
            } else {
                int part = horizontalWheel->part();
                if (part < 0) {
                    selectChannels = 0;
                } else {
                    selectChannels &= ~(1 << part);
                }
            }
        }
        return;
    }
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = channels[partButton->addChannel];
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
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainDuration, ppq));
        return;
    }
    // transpose selection
    // loop below computes diff of first note, and adds diff to subsequent notes in select
    int diff = 0;
    for (unsigned index = selectStart ; index < selectEnd; ++index) {
        DisplayNote& note = allNotes[index];
        switch (note.type) {
            case KEY_SIGNATURE:
                if (selectStart + 1 == selectEnd) {
                    note.setKey(wheelValue);
                    display->invalidateCache();
                    display->updateXPosition();
                }
                break;
            case TIME_SIGNATURE:
                if (selectStart + 1 == selectEnd && !selectButton->ledOn) {
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
            case MIDI_TEMPO:
                // tempo unaffected by vertical wheel
                break;
            case NOTE_ON: {
                if (!note.isSelectable(selectChannels)) {
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
                display->invalidateCache();
                display->updateXPosition();
                break;
            }
            default:
                ;
        }
    }
    this->playSelection();
}
