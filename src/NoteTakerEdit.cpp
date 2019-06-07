#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

void NoteTakerWidget::setHorizontalWheelRange() {
    edit.clear();
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
    auto& n = this->n();
    edit.init(n, selectChannels);  // set up for horz and vert
    // horizontal wheel range and value
    int value = INT_MAX;
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
                    _schmickled(); // incomplete
            }
        } else {
            if (edit.horizontalNote) {
                // to do : choose shortest note at starting time
                value = edit.horizontalValue;
                horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
            }
        }
    } else {
        int wheelMin = selectButton->editStart() ? 0 : 1;
        float wheelMax = n.horizontalCount(selectChannels) + .999f;
        if (debugVerbose) DEBUG("horizontalWheel->setLimits wheelMin %d wheelMax %g", wheelMin, wheelMax);
        horizontalWheel->setLimits(wheelMin, wheelMax);
        if (n.isEmpty(selectChannels)) {
            value = 0;
        } else {
            const DisplayNote* note = &n.notes[n.selectStart];
            value = this->noteToWheel(*note);
            if (value < wheelMin || value > wheelMax) {
                DEBUG("! note type %d value %d wheelMin %d wheelMax %g",
                        note->type, value, wheelMin, wheelMax);
                this->debugDump();
                _schmickled();
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
    if (tieButton->ledOn) {
        verticalWheel->setLimits(0, 0);
        verticalWheel->setValue(0);
        return;
    }
    auto& n = this->n();
    if (n.isEmpty(selectChannels)) {
        return;
    }
    const DisplayNote* note = &n.notes[n.selectStart];
    if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                verticalWheel->setLimits(-7, 7);
                verticalWheel->setValue(note->key());
                return;
            case TIME_SIGNATURE:
                verticalWheel->setLimits(0, 1.999f);
                verticalWheel->setValue(0);
                return;
            case MIDI_TEMPO:
                // nothing to do : tempo is note affected by vertical wheel
                return;
            default:
                _schmickled(); // incomplete
        }
    }
    SCHMICKLE(edit.base.size());
    if (!selectButton->ledOn) {
        if (edit.verticalNote) {
            verticalWheel->setLimits(0, 127);  // range for midi pitch
            verticalWheel->setValue(edit.verticalValue);
        }
    } else {
        bool atStart = selectButton->editStart();
        edit.voices = n.getVoices(selectChannels, atStart);
        int vCount = edit.voices.size();
        edit.originalStart = n.selectStart;
        edit.originalEnd = n.selectEnd;
        if (!atStart) {
            SCHMICKLE(selectButton->editEnd());
            verticalWheel->setLimits(0, vCount + .999f); // 0 : select all ; > 0 : select one
            verticalWheel->setValue(0);
        } else {
            verticalWheel->setLimits(0, 127.999f - vCount);  // allow inserting at any unoccupied pitch
            if (!vCount) {
                verticalWheel->setValue(60); // to do : add const for c4 (middle c)
            } else {
                verticalWheel->setValue(n.notes[edit.voices[0]].pitch());
            }
        }
    }
}

void NoteTakerWidget::setWheelRange() {
    edit.clear();
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
            invalidateCaches();
        }
        return;
    }
    if (n.isEmpty(selectChannels)) {
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
        invalidateCaches();
        return;
    }
    bool noteChanged = false;
    bool displayChanged = false;
    if (!selectButton->ledOn || n.voice) {
        SCHMICKLE((unsigned) wheelValue < NoteDurations::Count());
        int wheelChange = wheelValue - edit.horizontalValue;
        int startTime = edit.base[0].startTime;
        int insertedTime = 0;
        // proportionately adjust start times and durations of all in selection
        for (unsigned index = 0; index < edit.base.size(); ++index) {
            const DisplayNote& base = edit.base[index];
            DisplayNote* note = &n.notes[n.selectStart + index];
            int startDiff = base.startTime - startTime;
            int oldStart = note->startTime;
            if (startDiff) {
                int index = NoteDurations::FromMidi(startDiff, n.ppq);
                note->startTime = startTime + NoteDurations::ToMidi(index + wheelChange, n.ppq);
            }
            if (base.isNoteOrRest() && base.isSelectable(selectChannels)) {
                int oldDurationIndex = NoteDurations::FromMidi(base.duration, n.ppq);
                int oldDuration = note->duration;
                note->duration = NoteDurations::ToMidi(oldDurationIndex + wheelChange, n.ppq);
                int change = (note->startTime - oldStart) + (note->duration - oldDuration);
                if (change > 0) {
                    SCHMICKLE(insertedTime >= 0);
                    insertedTime = std::max(insertedTime, change);
                } else if (change < 0) {
                    SCHMICKLE(insertedTime <= 0);
                    insertedTime = std::min(insertedTime, change);
                }
            }
        }
        if (!n.voice) {
            for (unsigned index = n.selectEnd; index < n.notes.size(); ++index) {
                n.notes[index].startTime += insertedTime;
            }
        }
        noteChanged = true;
        NoteTaker::Sort(n.notes);
    } else {
        unsigned start, end;
        n.voice = false;
        if (selectButton->editEnd()) {
            clipboardInvalid = true;
            int wheelStart = this->noteToWheel(
                    selectButton->selStart - (int) selectButton->saveZero) + 1;
            start =  this->wheelToNote(std::min(wheelValue, wheelStart));
            end =  this->wheelToNote(std::max(wheelValue + 1, wheelStart));
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d wheelStart %d",
                    start, end, wheelValue, wheelStart);
        } else {
            start = this->wheelToNote(wheelValue);
            selectButton->saveZero = SelectButton::State::single == selectButton->state
                    && !start;
            end = nt()->nextAfter(start, 1);
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d", start, end, wheelValue);
        }
        SCHMICKLE(start < end);
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
        invalidateCaches();
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
                _schmickled();
        }
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainDuration, n.ppq));
        return;
    }
    if (!selectButton->ledOn) {
        // transpose selection
        // loop below computes diff of first note, and adds diff to subsequent notes in select
        for (unsigned index = n.selectStart ; index < n.selectEnd; ++index) {
            DisplayNote& note = n.notes[index];
            switch (note.type) {
                case KEY_SIGNATURE:
                    if (n.selectStart + 1 == n.selectEnd) {
                        note.setKey(wheelValue);
                        invalidateCaches();
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
                    int value = edit.base[index - n.selectStart].pitch()
                            + wheelValue - edit.verticalValue;
                    value = std::max(0, std::min(127, value));
                    if (!n.uniquePitch(note, value)) {
                        note.setPitch(value);
                    }
                    invalidateCaches();
                    break;
                }
                default:
                    ;
            }
        }
    } else if (selectButton->editEnd()) {
        if (!wheelValue) {
            n.voice = false;
            n.selectStart = edit.originalStart;
            n.selectEnd = edit.originalEnd;
            DEBUG("selStart %u / selEnd %u", n.selectStart, n.selectEnd);
        } else {
            n.voice = true;
            n.selectStart = edit.voices[wheelValue - 1];
            n.selectEnd = n.selectStart + 1;
            DEBUG("selStart %u / selEnd %u wheel %d", n.selectStart, n.selectEnd, wheelValue);
        }
    } else {
        SCHMICKLE(selectButton->editStart());
        int pitch = wheelValue;
        const DisplayNote* base = nullptr;
        for (unsigned i : edit.voices) {  // skipping existing notes
            const auto& test = n.notes[i];
            if (!base) {
                base = &n.notes[i];
            } else if (test.startTime > base->startTime) {
                break;
            }
            if (test.pitch() <= pitch) {
                ++pitch;
            }
        }
        if (!n.voice) {
            int duration = base ? base->duration : n.ppq;
            int startTime = base ? base->startTime : n.notes[n.selectStart].startTime;
            DisplayNote iNote = { nullptr, startTime, duration,
                    { pitch, 0, stdKeyPressure, stdKeyPressure},
                    (uint8_t) this->unlockedChannel(), NOTE_ON, false };
            n.selectStart += 1;
            n.selectEnd = n.selectStart + 1;
            n.notes.insert(n.notes.begin() + n.selectStart, iNote);
            n.voice = true;
        } else {
            n.notes[n.selectStart].setPitch(pitch);
        }
        
        invalidateCaches();
    }
    displayBuffer->fb->dirty = true;
    nt()->playSelection();
}
