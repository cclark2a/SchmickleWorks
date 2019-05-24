#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

void NoteTakerWidget::setHorizontalWheelRange() {
    if (runButton->ledOn) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - .001f); // tempo relative to quarter note
        horizontalWheel->setValue(NoteDurations::FromStd(stdTimePerQuarterNote));
        return;
    }
    if (fileButton->ledOn) {
        // to do : here, and in other places: set speed proportional to limits
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        return;
    }
    PartButton* pb = partButton;
    DEBUG("pb %p", pb);
    if (partButton->ledOn) {
        horizontalWheel->setLimits(-1, CV_OUTPUTS);
        horizontalWheel->setValue(this->unlockedChannel());
        return;
    }
    if (sustainButton->ledOn) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - 1);
        int sustainMinDuration = nt()->channels[this->unlockedChannel()].sustainMin;
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainMinDuration, n().ppq));
        return;
    }
    if (tieButton->ledOn) {
        horizontalWheel->setLimits(0, 2);
        // to do : if already slurred, set value to zero
        //         if already triplet, set value to two
        // to do : create circle : 
        //         trip-slur slur norm trip trip-slur ...
        horizontalWheel->setValue(1);
        return;
    }
    // horizontal wheel range and value
    int value = INT_MAX;
    auto& n = this->n();
    if (!selectButton->ledOn) {
        const DisplayNote* note = &n.notes[n.selectStart];
        if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
            switch (note->type) {
                case TIME_SIGNATURE:
                    horizontalWheel->setDenominator(*note);
                    return;
                case KEY_SIGNATURE:
                    // nothing to do : horizontal wheel doesn't affect key signature
                    return;
                case MIDI_TEMPO: {
                    horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
                    float fValue = NoteDurations::FromStd(note->tempo() * stdTimePerQuarterNote / 500000);
                    horizontalWheel->setValue(fValue);
                    return;
                }
                default:
                    assert(0); // incomplete
            }
        } else {
            unsigned start = n.selectStart;
            while (note->isSignature() && start < n.selectEnd) {
                note = &n.notes[++start];
            }
            if (note->isNoteOrRest()) {
                value = NoteDurations::FromMidi(note->duration, n.ppq);
            }
            if (INT_MAX != value) {
                horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
            }
        }
    } else {
        int wheelMin = selectButton->editStart() ? 0 : 1;
        float wheelMax = this->horizontalCount() + .999f;
        if (debugVerbose) DEBUG("horizontalWheel->setLimits wheelMin %d wheelMax %g", wheelMin, wheelMax);
        horizontalWheel->setLimits(wheelMin, wheelMax);
        if (this->isEmpty()) {
            value = 0;
        } else {
            const DisplayNote* note = &n.notes[n.selectStart];
            value = this->noteToWheel(*note);
            if (value < wheelMin || value > wheelMax) {
                DEBUG("! note type %d value %d wheelMin %d wheelMax %g",
                        note->type, value, wheelMin, wheelMax);
                this->debugDump();
                assert(0);
            }
        }
    }
    if (INT_MAX != value) {
        horizontalWheel->setValue(value);
    }
    return;
}

void NoteTakerWidget::setVerticalWheelRange() {
    if (runButton->ledOn) {
        verticalWheel->setLimits(0, 127);    // v/oct transpose (5 octaves up, down)
        verticalWheel->setValue(60);
        return;
    }
    if (fileButton->ledOn || partButton->ledOn) {
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        return;
    }
    if (sustainButton->ledOn) {
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->setValue(2.5f); // sustain min
        return;
    }
    if (this->isEmpty()) {
        return;
    }
    auto& n = this->n();
    const DisplayNote* note = &n.notes[n.selectStart];
    if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                verticalWheel->setLimits(-7, 7);
                verticalWheel->setValue(note->key());
                break;
            case TIME_SIGNATURE:
                verticalWheel->setLimits(0, 1.999f);
                verticalWheel->setValue(0);
                break;
            case MIDI_TEMPO:
                // nothing to do : tempo is note affected by vertical wheel
                break;
            default:
                assert(0); // incomplete
        }
        return;
    }
    unsigned start = n.selectStart;
    while (start < n.selectEnd && !note->isSelectable(selectChannels)) {
        note = &n.notes[++start];
    }
    bool validNote = start < n.selectEnd && NOTE_ON == note->type;
    verticalWheel->setLimits(0, 127);  // range for midi pitch
    verticalWheel->setValue(validNote ? note->pitch() : 60);
}

void NoteTakerWidget::setWheelRange() {
    this->setHorizontalWheelRange();
    this->setVerticalWheelRange();
    if (debugVerbose) DEBUG("setWheelRange %s %s", 
            horizontalWheel->debugString().c_str(),
            verticalWheel->debugString().c_str());
    displayBuffer->fb->dirty = true;
}

void NoteTakerWidget::updateHorizontal() {
    auto& n = this->n();
    
    displayBuffer->fb->dirty |= this->menuButtonOn();
    if (fileButton->ledOn) {
        return;
    }
    if (partButton->ledOn) {
        return;
    }
    if (sustainButton->ledOn) {
        NoteTakerChannel& channel = nt()->channels[this->unlockedChannel()];
        channel.setLimit((NoteTakerChannel::Limit) verticalWheel->getValue(),
                NoteDurations::ToMidi(horizontalWheel->getValue(), n.ppq));
        return;
    }
    if (runButton->ledOn) {
        return;
    }
    if (tieButton->ledOn) {
        TieButton::State prev = tieButton->state;
        if (horizontalWheel->getValue() < .25f) {
            // to do : disallow choosing slur if either selection is one note
            //       : or if all of selection is not notes
            if (TieButton::State::slur != tieButton->state) {
                this->makeSlur();
                tieButton->state = TieButton::State::slur;
            }
        } else if (horizontalWheel->getValue() > .75f && horizontalWheel->getValue() < 1.25f) {
            if (TieButton::State::normal != tieButton->state) {
                this->makeNormal();
                tieButton->state = TieButton::State::normal;
            }
        } else if (horizontalWheel->getValue() > 1.75f) {
            if (TieButton::State::tuplet != tieButton->state) {
                this->makeTuplet();
                tieButton->state = TieButton::State::tuplet;
            }
        }
        if (prev != tieButton->state) {
            display->invalidateCache();
        }
        return;
    }
    if (this->isEmpty()) {
        return;
    }
    bool selectOne = !selectButton->ledOn && n.selectStart + 1 == n.selectEnd;
    DisplayNote& oneNote = n.notes[n.selectStart];
    if (selectOne && MIDI_TEMPO == oneNote.type) {
        oneNote.setTempo(nt()->wheelToTempo(horizontalWheel->getValue()) * 500000);
        displayBuffer->fb->dirty = true;
        return;
    }
    if (!horizontalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = horizontalWheel->wheelValue();
    if (selectOne && TIME_SIGNATURE == oneNote.type) {
        if ((int) verticalWheel->getValue()) {
            oneNote.setNumerator(wheelValue);
        } else {
            oneNote.setDenominator(wheelValue);
        }
        display->invalidateCache();
        return;
    }
    bool noteChanged = false;
    bool displayChanged = false;
    if (!selectButton->ledOn) {
        // for now, if selection includes signature, do nothing
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            DisplayNote& note = n.notes[index];
            if (note.isSignature()) {
                return;
            }
        }
        array<int, CHANNEL_COUNT> diff;
        diff.fill(0);
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            DisplayNote& note = n.notes[index];
            note.startTime += diff[note.channel];
            if (!this->isSelectable(note)) {
                continue;
            }
            assert(note.isNoteOrRest());
            assert((unsigned) wheelValue < NoteDurations::Count());
            int duration = NoteDurations::ToMidi(wheelValue, n.ppq);
            diff[note.channel] += duration - note.duration;
            note.duration = duration;
        }
        for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
            if (diff[chan]) {
                NoteTaker::ShiftNotes(n.notes, n.selectEnd, diff[chan], 1 << chan);
                noteChanged = true;
            }
        }
        if (noteChanged) {
            NoteTaker::Sort(n.notes);
            display->invalidateCache();
            display->rangeInvalid = true;
        }
    } else {
        unsigned start, end;
        if (selectButton->editEnd()) {
            clipboardInvalid = true;
            int wheelStart = this->noteToWheel(
                    selectButton->selStart - (int) selectButton->saveZero) + 1;
            start =  this->wheelToNote(std::min(wheelValue, wheelStart));
            end =  this->wheelToNote(std::max(wheelValue + 1, wheelStart));
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d wheelStart %d",
                    start, end, wheelValue, wheelStart);
        } else {
            start =  this->wheelToNote(wheelValue);
            selectButton->saveZero = SelectButton::State::single == selectButton->state
                    && !start;
            end = nt()->nextAfter(start, 1);
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d", start, end, wheelValue);
        }
        assert(start < end);
        if (start != n.selectStart || end != n.selectEnd) {
            nt()->setSelect(start, end);
            this->setVerticalWheelRange();
            displayChanged = true;
            if (start + 1 == end) {
                const auto& note = n.notes[start];
                if (KEY_SIGNATURE == note.type && !note.key()) {
                    display->dynamicSelectTimer = nt()->realSeconds
                            + display->fadeDuration;
                    display->dynamicSelectAlpha = 0xFF;
                }
            }
        }
    }
    if (noteChanged) {
        display->invalidateCache();
    }
    if (noteChanged || displayChanged) {
        display->invalidateRange();
        nt()->playSelection();
    }
}
    
void NoteTakerWidget::updateVertical() {
    auto& n = this->n();
    if (runButton->ledOn) {
        return;
    }
    displayBuffer->fb->dirty |= this->menuButtonOn();
    if (!verticalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = verticalWheel->wheelValue();
    if (fileButton->ledOn || partButton->ledOn) {
        if (verticalWheel->getValue() <= 2) {
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
        if (verticalWheel->getValue() >= 8) {
            display->upSelected = true;
            if (fileButton->ledOn && (unsigned) horizontalWheel->getValue() < storage.size()) {
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
        NoteTakerChannel& channel = nt()->channels[this->unlockedChannel()];
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
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainDuration, n.ppq));
        return;
    }
    // transpose selection
    // loop below computes diff of first note, and adds diff to subsequent notes in select
    int diff = 0;
    for (unsigned index = n.selectStart ; index < n.selectEnd; ++index) {
        DisplayNote& note = n.notes[index];
        switch (note.type) {
            case KEY_SIGNATURE:
                if (n.selectStart + 1 == n.selectEnd) {
                    note.setKey(wheelValue);
                    display->invalidateCache();
                }
                break;
            case TIME_SIGNATURE:
                if (n.selectStart + 1 == n.selectEnd && !selectButton->ledOn) {
                    if (!wheelValue) {
                        horizontalWheel->setDenominator(note);
                    } else {
                        horizontalWheel->setLimits(1, 99.99f);   // numer limit 0 to 99
                        horizontalWheel->setValue(note.numerator());
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
                break;
            }
            default:
                ;
        }
    }
    displayBuffer->fb->dirty = true;
    nt()->playSelection();
}
