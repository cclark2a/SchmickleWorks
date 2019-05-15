#include "NoteTaker.hpp"
#include "NoteTakerButton.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerWheel.hpp"
#include "NoteTakerWidget.hpp"

void NoteTakerWidget::setHorizontalWheelRange() {
    HorizontalWheel* horizontalWheel = this->widget<HorizontalWheel>();
    if (this->widget<RunButton>()->ledOn) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - .001f); // tempo relative to quarter note
        horizontalWheel->setValue(NoteDurations::FromStd(stdTimePerQuarterNote));
        horizontalWheel->speed = 1;
        return;
    }
    if (this->widget<FileButton>()->ledOn) {
        // to do : here, and in other places: set speed proportional to limits
        horizontalWheel->setLimits(0, storage.size());
        horizontalWheel->setValue(0);
        horizontalWheel->speed = 1;
        return;
    }
    if (this->widget<PartButton>()->ledOn) {
        horizontalWheel->setLimits(-1, CV_OUTPUTS);
        horizontalWheel->setValue(this->unlockedChannel());
        horizontalWheel->speed = 1;
        return;
    }
    if (this->widget<SustainButton>()->ledOn) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - 1);
        int sustainMinDuration = nt()->channels[this->unlockedChannel()].sustainMin;
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainMinDuration, nt()->ppq));
        horizontalWheel->speed = 1;
        return;
    }
    if (this->widget<TieButton>()->ledOn) {
        horizontalWheel->setLimits(0, 2);
        // to do : if already slurred, set value to zero
        //         if already triplet, set value to two
        // to do : create circle : 
        //         trip-slur slur norm trip trip-slur ...
        horizontalWheel->setValue(1);
        horizontalWheel->speed = 1;
        return;
    }
    // horizontal wheel range and value
    float horzSpeed = 1;
    int value = INT_MAX;
    if (!this->widget<SelectButton>()->ledOn) {
        const DisplayNote* note = &nt()->notes()[nt()->selStart()];
        if (nt()->selStart() + 1 == nt()->selEnd() && note->isSignature()) {
            switch (note->type) {
                case TIME_SIGNATURE:
                    horizontalWheel->setLimits(1, 6.99f);   // denom limit 0 to 6 (2^N, 1 to 64)
                    horzSpeed = 1;
                    value = note->denominator();
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
            unsigned start = nt()->selStart();
            while (note->isSignature() && start < nt()->selEnd()) {
                note = &nt()->notes()[++start];
            }
            if (note->isNoteOrRest()) {
                value = NoteDurations::FromMidi(note->duration, nt()->ppq);
            }
            if (INT_MAX != value) {
                horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
            }
        }
    } else {
        int wheelMin = this->widget<SelectButton>()->editStart() ? 0 : 1;
        float wheelMax = this->horizontalCount() + .999f;
        if (debugVerbose) debug("horizontalWheel->setLimits wheelMin %d wheelMax %g", wheelMin, wheelMax);
        horizontalWheel->setLimits(wheelMin, wheelMax);
        if (this->isEmpty()) {
            value = 0;
        } else {
            const DisplayNote* note = &nt()->notes()[nt()->selStart()];
            value = this->noteToWheel(*note);
            if (value < wheelMin || value > wheelMax) {
                debug("! note type %d value %d wheelMin %d wheelMax %g",
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

void NoteTakerWidget::setVerticalWheelRange() {
    VerticalWheel* verticalWheel = this->widget<VerticalWheel>();
    if (this->widget<RunButton>()->ledOn) {
        verticalWheel->setLimits(0, 127);    // v/oct transpose (5 octaves up, down)
        verticalWheel->setValue(60);
        verticalWheel->speed = .1;
        return;
    }
    if (this->widget<FileButton>()->ledOn || this->widget<PartButton>()->ledOn) {
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        verticalWheel->speed = 2;
        return;
    }
    if (this->widget<SustainButton>()->ledOn) {
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->setValue(2.5f); // sustain min
        verticalWheel->speed = 1;
        return;
    }
    if (this->isEmpty()) {
        return;
    }
    DisplayNote* note = &nt()->notes()[nt()->selStart()];
    if (nt()->selStart() + 1 == nt()->selEnd() && note->isSignature()) {
        switch (note->type) {
            case KEY_SIGNATURE:
                verticalWheel->setLimits(-7, 7);
                verticalWheel->setValue(note->key());
                verticalWheel->speed = 1;
                break;
            case TIME_SIGNATURE:
                verticalWheel->setLimits(0, 1.999f);
                verticalWheel->setValue(0);
                verticalWheel->speed = 5;
                break;
            case MIDI_TEMPO:
                // nothing to do : tempo is note affected by vertical wheel
                break;
            default:
                assert(0); // incomplete
        }
        return;
    }
    unsigned start = nt()->selStart();
    while (start < nt()->selEnd() && !note->isSelectable(selectChannels)) {
        note = &nt()->notes()[++start];
    }
    bool validNote = start < nt()->selEnd() && NOTE_ON == note->type;
    verticalWheel->setLimits(0, 127);  // range for midi pitch
    verticalWheel->setValue(validNote ? note->pitch() : 60);
    verticalWheel->speed = .1;
}

void NoteTakerWidget::setWheelRange() {
    this->setHorizontalWheelRange();
    this->setVerticalWheelRange();
    if (debugVerbose) debug("setWheelRange %s %s", 
            this->widget<HorizontalWheel>()->debugString().c_str(),
            this->widget<VerticalWheel>()->debugString().c_str());
    this->widget<NoteTakerDisplay>()->fb->dirty = true;
}

void NoteTakerWidget::updateHorizontal() {
    this->widget<NoteTakerDisplay>()->fb->dirty |= this->menuButtonOn();
    if (this->widget<FileButton>()->ledOn) {
        return;
    }
    if (this->widget<PartButton>()->ledOn) {
        return;
    }
    if (this->widget<SustainButton>()->ledOn) {
        NoteTakerChannel& channel = nt()->channels[this->unlockedChannel()];
        channel.setLimit((NoteTakerChannel::Limit) this->widget<VerticalWheel>()->getValue(),
                NoteDurations::ToMidi(this->widget<HorizontalWheel>()->getValue(), nt()->ppq));
        return;
    }
    if (this->widget<RunButton>()->ledOn) {
        return;
    }
    if (this->widget<TieButton>()->ledOn) {
        TieButton::State prev = this->widget<TieButton>()->state;
        auto horizontalWheel = this->widget<HorizontalWheel>();
        if (horizontalWheel->getValue() < .25f) {
            // to do : disallow choosing slur if either selection is one note
            //       : or if all of selection is not notes
            if (TieButton::State::slur != this->widget<TieButton>()->state) {
                this->makeSlur();
                this->widget<TieButton>()->state = TieButton::State::slur;
            }
        } else if (horizontalWheel->getValue() > .75f && horizontalWheel->getValue() < 1.25f) {
            if (TieButton::State::normal != this->widget<TieButton>()->state) {
                this->makeNormal();
                this->widget<TieButton>()->state = TieButton::State::normal;
            }
        } else if (horizontalWheel->getValue() > 1.75f) {
            if (TieButton::State::tuplet != this->widget<TieButton>()->state) {
                this->makeTuplet();
                this->widget<TieButton>()->state = TieButton::State::tuplet;
            }
        }
        if (prev != this->widget<TieButton>()->state) {
            this->widget<NoteTakerDisplay>()->invalidateCache();
        }
        return;
    }
    if (this->isEmpty()) {
        return;
    }
    bool selectOne = !this->widget<SelectButton>()->ledOn && nt()->selStart() + 1 == nt()->selEnd();
    DisplayNote& oneNote = nt()->notes()[nt()->selStart()];
    if (selectOne && MIDI_TEMPO == oneNote.type) {
        oneNote.setTempo(nt()->wheelToTempo(this->widget<HorizontalWheel>()->getValue()) * 500000);
        this->widget<NoteTakerDisplay>()->fb->dirty = true;
        return;
    }
    if (!this->widget<HorizontalWheel>()->hasChanged()) {
        return;
    }
    const int wheelValue = this->widget<HorizontalWheel>()->wheelValue();
    if (selectOne && TIME_SIGNATURE == oneNote.type) {
        if ((int) this->widget<VerticalWheel>()->getValue()) {
            oneNote.setNumerator(wheelValue);
        } else {
            oneNote.setDenominator(wheelValue);
        }
        this->widget<NoteTakerDisplay>()->invalidateCache();
        return;
    }
    bool noteChanged = false;
    if (!this->widget<SelectButton>()->ledOn) {
        // for now, if selection includes signature, do nothing
        for (unsigned index = nt()->selStart(); index < nt()->selEnd(); ++index) {
            DisplayNote& note = nt()->notes()[index];
            if (note.isSignature()) {
                return;
            }
        }
        array<int, CHANNEL_COUNT> diff;
        diff.fill(0);
        for (unsigned index = nt()->selStart(); index < nt()->selEnd(); ++index) {
            DisplayNote& note = nt()->notes()[index];
            note.startTime += diff[note.channel];
            if (!this->isSelectable(note)) {
                continue;
            }
            assert(note.isNoteOrRest());
            assert((unsigned) wheelValue < NoteDurations::Count());
            int duration = NoteDurations::ToMidi(wheelValue, nt()->ppq);
            diff[note.channel] += duration - note.duration;
            note.duration = duration;
        }
        for (unsigned chan = 0; chan < CHANNEL_COUNT; ++chan) {
            if (diff[chan]) {
                NoteTaker::ShiftNotes(nt()->notes(), nt()->selEnd(), diff[chan], 1 << chan);
                noteChanged = true;
            }
        }
        if (noteChanged) {
            NoteTaker::Sort(nt()->notes());
            this->widget<NoteTakerDisplay>()->invalidateCache();
            this->widget<NoteTakerDisplay>()->rangeInvalid = true;
        }
    } else {
        unsigned start, end;
        if (this->widget<SelectButton>()->editEnd()) {
            clipboardInvalid = true;
            int wheelStart = this->noteToWheel(
                    this->widget<SelectButton>()->selStart - (int) this->widget<SelectButton>()->saveZero) + 1;
            start =  this->wheelToNote(std::min(wheelValue, wheelStart));
            end =  this->wheelToNote(std::max(wheelValue + 1, wheelStart));
            if (debugVerbose) debug("start %u end %u wheelValue %d wheelStart %d",
                    start, end, wheelValue, wheelStart);
        } else {
            start =  this->wheelToNote(wheelValue);
            this->widget<SelectButton>()->saveZero = SelectButton::State::single == this->widget<SelectButton>()->state
                    && !start;
            end = nt()->nextAfter(start, 1);
            if (debugVerbose) debug("start %u end %u wheelValue %d", start, end, wheelValue);
        }
        assert(start < end);
        if (start != nt()->selStart() || end != nt()->selEnd()) {
            nt()->setSelect(start, end);
            this->setVerticalWheelRange();
            noteChanged = true;
            if (start + 1 == end) {
                const auto& note = nt()->notes()[start];
                if (KEY_SIGNATURE == note.type && !note.key()) {
                    this->widget<NoteTakerDisplay>()->dynamicSelectTimer = nt()->realSeconds
                            + this->widget<NoteTakerDisplay>()->fadeDuration;
                    this->widget<NoteTakerDisplay>()->dynamicSelectAlpha = 0xFF;
                }
            }
        }
    }
    if (noteChanged) {
        this->widget<NoteTakerDisplay>()->invalidateCache();
        nt()->playSelection();
    }
}
    
void NoteTakerWidget::updateVertical() {
    if (this->widget<RunButton>()->ledOn) {
        return;
    }
    this->widget<NoteTakerDisplay>()->fb->dirty |= this->menuButtonOn();
    auto verticalWheel = this->widget<VerticalWheel>();
    if (!verticalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = verticalWheel->wheelValue();
    auto horizontalWheel = this->widget<HorizontalWheel>();
    auto display = this->widget<NoteTakerDisplay>();
    if (this->widget<FileButton>()->ledOn || this->widget<PartButton>()->ledOn) {
        if (verticalWheel->getValue() <= 2) {
            display->downSelected = true;
            if (this->widget<FileButton>()->ledOn) {
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
            if (this->widget<FileButton>()->ledOn && (unsigned) horizontalWheel->getValue() < storage.size()) {
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
    if (this->widget<SustainButton>()->ledOn) {
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
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainDuration, nt()->ppq));
        return;
    }
    // transpose selection
    // loop below computes diff of first note, and adds diff to subsequent notes in select
    int diff = 0;
    for (unsigned index = nt()->selStart() ; index < nt()->selEnd(); ++index) {
        DisplayNote& note = nt()->notes()[index];
        switch (note.type) {
            case KEY_SIGNATURE:
                if (nt()->selStart() + 1 == nt()->selEnd()) {
                    note.setKey(wheelValue);
                    display->invalidateCache();
                }
                break;
            case TIME_SIGNATURE:
                if (nt()->selStart() + 1 == nt()->selEnd() && !this->widget<SelectButton>()->ledOn) {
                    if (!wheelValue) {
                        horizontalWheel->setLimits(0, 6.99f);   // denom limit 0 to 6 (2^N, 1 to 64)
                        horizontalWheel->setValue(note.denominator());
                        // to do : override setLimits to set speed based on limit range
                        horizontalWheel->speed = 1;
                    } else {
                        horizontalWheel->setLimits(1, 99.99f);   // numer limit 0 to 99
                        horizontalWheel->setValue(note.numerator());
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
                break;
            }
            default:
                ;
        }
    }
    display->fb->dirty = true;
    nt()->playSelection();
}
