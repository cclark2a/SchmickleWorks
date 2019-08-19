#include "Button.hpp"
#include "Display.hpp"
#include "Taker.hpp"
#include "Wheel.hpp"
#include "Widget.hpp"

void NoteTakerWidget::setHorizontalNoteLimits() {
    const auto& n = this->n();
    const DisplayNote* note = &n.notes[n.selectStart];
    if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
        switch (note->type) {
            case TIME_SIGNATURE:
                horizontalWheel->setDenominator(*note); // sets limits, value
                return;
            case KEY_SIGNATURE:
                horizontalWheel->setLimits(0, 1.999f);
                horizontalWheel->setValue(note->minor());
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
    } else if (edit.horizontalNote) {
        // to do : choose shortest note at starting time
        horizontalWheel->setValue(edit.horizontalValue);
        horizontalWheel->setLimits(0, NoteDurations::Count() - 0.001f);
    } else {
        horizontalWheel->setValue(0);
        horizontalWheel->setLimits(0, 0);
    }
}
void NoteTakerWidget::setHorizontalWheelRange() {
    if (runButton->ledOn()) {
        horizontalWheel->setLimits(0, NoteDurations::Count() - .001f); // tempo relative to quarter note
        horizontalWheel->setValue(NoteDurations::FromStd(stdTimePerQuarterNote));
        return;
    }
    if (fileButton->ledOn()) {
        // to do : here, and in other places: set speed proportional to limits
        horizontalWheel->setLimits(0, storage.size() - 1);
        horizontalWheel->setValue(this->getSlot());
        return;
    }
    if (partButton->ledOn()) {
        horizontalWheel->setLimits(-1, nt()->outputCount());
        horizontalWheel->setValue(this->unlockedChannel());
        return;
    }
    if (slotButton->ledOn()) {
        edit.init(storage);
        if (selectButton->isOff()) {
            const auto& slotPlay = storage.playback[storage.selectStart];
            horizontalWheel->setLimits(0, storage.size() - 1);
            horizontalWheel->setValue(slotPlay.index);
        } else if (selectButton->isSingle()) {
            horizontalWheel->setLimits(0, storage.playback.size());
            horizontalWheel->setValue(storage.selectStart);
        } else {
            SCHMICKLE(selectButton->isExtend());
            horizontalWheel->setLimits(0, storage.playback.size() - 1);
            horizontalWheel->setValue(storage.selectStart);
        }
        edit.horizontalValue = (int) horizontalWheel->getValue();
        return;
    }
    if (sustainButton->ledOn()) {
        // use first unlocked channel as guide
        horizontalWheel->setLimits(0, NoteDurations::Count() - 1);
        int sustainMinDuration = nt()->slot->channels[this->unlockedChannel()].sustainMin;
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainMinDuration, n().ppq));
        return;
    }
    if (tieButton->ledOn()) {
        horizontalWheel->setLimits(-4, 7.999f); // one set more in either direction for scrolling
        bool trips = this->isTriplet();
        bool slur = this->isSlur();
        int index = trips ? 2 + slur : !slur;
        horizontalWheel->setValue(index + 0.5f);
        display->xControlOffset = (index + 3) % 4;
        if (debugVerbose) DEBUG("tieButton trips %d slur %d setHorz val %g xControlOffset %g",
                trips, slur, index + 0.5f, display->xControlOffset);
        return;
    }
    auto& n = this->n();
    edit.init(n, selectChannels);  // set up for horz and vert
    // horizontal wheel range and value
    int value = INT_MAX;
    if (selectButton->isOff()) {
        this->setHorizontalNoteLimits();
    } else {
        int wheelMin = selectButton->editStart() ? 0 : 1;
        float wheelMax = n.horizontalCount(selectChannels) + .999f;
        if (debugVerbose) DEBUG("horizontalWheel->setLimits selButtonVal %d wheelMin %d wheelMax %g",
                selectButton->getValue(), wheelMin, wheelMax);
        horizontalWheel->setLimits(wheelMin, wheelMax);
        if (n.isEmpty(selectChannels)) {
            value = wheelMin;
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
    if (runButton->ledOn()) {
        verticalWheel->setLimits(0, 127.999f);    // v/oct transpose (5 octaves up, down)
        verticalWheel->setValue(60);
        return;
    }
    if (fileButton->ledOn() || partButton->ledOn()) {
        verticalWheel->setLimits(0, 10);
        verticalWheel->setValue(5);
        return;
    }
    if (slotButton->ledOn()) {
        if (selectButton->isOff()) {
            verticalWheel->setLimits(0, 2.999f);
            verticalWheel->setValue(1.5f); // choose slot
            this->updateSlotVertical(SlotButton::Select::slot);
        } else {
            verticalWheel->setLimits(0, 0);
            verticalWheel->setValue(0);
        }
        return;
    }
    if (sustainButton->ledOn()) {
        verticalWheel->setLimits(0, 3.999f);
        verticalWheel->setValue(2.5f); // sustain min
        return;
    }
    if (tieButton->ledOn()) {
        verticalWheel->setLimits(0, 0);
        verticalWheel->setValue(0);
        return;
    }
    auto& n = this->n();
    if (n.isEmpty(selectChannels)) {
        return;
    }
    SCHMICKLE(edit.base.size());
    if (selectButton->isOff()) {
        if (edit.verticalNote) {
            verticalWheel->setLimits(0, 127.999f);  // range for midi pitch
            verticalWheel->setValue(edit.verticalValue);
        } else {
            const DisplayNote* note = &n.notes[n.selectStart];
            if (n.selectStart + 1 == n.selectEnd && note->isSignature()) {
                switch (note->type) {
                    case KEY_SIGNATURE:
                        verticalWheel->setLimits(-7.999f, 7.999f);
                        verticalWheel->setValue(note->key());
                        return;
                    case TIME_SIGNATURE:
                        verticalWheel->setLimits(0, 1.999f);
                        verticalWheel->setValue(0);
                        return;
                    case MIDI_TEMPO:
                        // tempo is not affected by vertical wheel
                        verticalWheel->setLimits(0, 0);
                        verticalWheel->setValue(0);
                        return;
                    default:
                        _schmickled(); // incomplete
                }
            } else {
                verticalWheel->setLimits(0, 0);
                verticalWheel->setValue(0);
            }
        }
    } else if (edit.verticalNote) {
        SCHMICKLE(selectButton->editStart() || selectButton->editEnd());
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
                SCHMICKLE(edit.voices[0] < n.notes.size());
                const DisplayNote& note = n.notes[edit.voices[0]];
                verticalWheel->setValue(NOTE_ON == note.type ? note.pitch() : 60); // to do : add const
            }
        }
    } else {
        verticalWheel->setLimits(0, 0);
        verticalWheel->setValue(0);
    }
}

void NoteTakerWidget::setWheelRange() {
    edit.clear();
    this->setHorizontalWheelRange();
    this->setVerticalWheelRange();
    if (debugVerbose) DEBUG("updateWheelRange %s %s", 
            horizontalWheel->debugString().c_str(),
            verticalWheel->debugString().c_str());
}

void NoteTakerWidget::updateHorizontal() {
    auto& n = this->n();
    if (runButton->ledOn()) {
        return;
    }
    if (this->menuButtonOn()) {
        displayBuffer->redraw();
    }
    if (fileButton->ledOn()) {
        return;
    }
    if (partButton->ledOn()) {
        return;
    }
    if (slotButton->ledOn()) {
        // move selection in storage playback, or change slot #, repeat, stage
        int wheelValue = (int) horizontalWheel->getValue();
        if (selectButton->isOff()) {
            for (unsigned index = storage.selectStart; index < storage.selectEnd; ++index) {
                // note that changes are relative to their original values
                auto& orig = edit.pbase[index - edit.originalStart];
                auto& slot = storage.playback[index];
                int diff = wheelValue - edit.horizontalValue;
                switch ((SlotButton::Select) verticalWheel->getValue()) {
                    case SlotButton::Select::repeat:
                        slot.repeat = (int) orig.repeat + diff < 1 ? 1 :
                                std::min(orig.repeat + diff, 99U);
                    break;
                    case SlotButton::Select::slot:
                        // change current slot in nt as well
                        slot.index = (int) orig.index + diff < 0 ? 0 :
                                std::min(orig.index + diff, SLOT_COUNT - 1);
                        if (storage.selectStart == index) {
                            this->nt()->stageSlot(slot.index);
                        }
                    break;
                    case SlotButton::Select::end:
                        slot.stage = (int) orig.stage + diff < (int) SlotPlay::Stage::step ?
                                SlotPlay::Stage::step :
                                (SlotPlay::Stage) std::min((int) orig.stage + diff,
                                (int) SlotPlay::Stage::never);
                    break;
                    default:
                        _schmickled();
                }
            }
        } else if (selectButton->isSingle()) {
            storage.selectStart = wheelValue;
        #if 0
            if (storage.selectStart) {
                storage.selectStart -= 1;
            } else {
                storage.saveZero = true;
            }
        #endif
            storage.selectEnd = storage.selectStart + 1;
            static unsigned debugStart = INT_MAX;
            if (debugVerbose && debugStart != storage.selectStart) {
                DEBUG("updateHorizontal %u", storage.selectStart);
                debugStart = storage.selectStart;
            }
        } else {
            SCHMICKLE(selectButton->isExtend());
            storage.selectStart = std::min((unsigned) wheelValue, edit.originalStart);
            storage.selectEnd = std::max((unsigned) wheelValue + 1, edit.originalStart + 1);
        }
        return;
    }
    if (sustainButton->ledOn()) {
        // to do : worth having some lambda-thing to abstract and reuse this loop?
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            if (!(selectChannels & (1 << index))) {
                continue;
            }
            NoteTakerChannel& channel = nt()->slot->channels[index];
            channel.setLimit((NoteTakerChannel::Limit) verticalWheel->getValue(),
                    NoteDurations::ToMidi(horizontalWheel->getValue(), n.ppq));
        }
        return;
    }
    if (tieButton->ledOn()) {
        TieButton::State prev = tieButton->state;
        float value = horizontalWheel->getValue();
        if (value < 0 || value >= 4) {
            value += value < 0 ? 4 : -4;
            horizontalWheel->setValue(value); 
            if (debugVerbose) DEBUG("tie updateHorz value %g", value);
        }
        TieButton::State next = (TieButton::State) value;
        if (prev == next) {
            return;
        }
        switch (next) {
            case TieButton::State::slur:
            // to do : disallow choosing slur if either selection is one note
            //       : or if all of selection is not notes?
                this->makeNormal();
                this->makeSlur();
            break;
            case TieButton::State::normal:
                this->makeNormal();
            break;
            case TieButton::State::tuplet:
                this->makeNormal();
                this->makeTuplet();
            break;
            case TieButton::State::both:
                this->makeSlur();
                this->makeTuplet();
            break;
        }
        tieButton->state = next;
        this->invalidateAndPlay(Inval::change);
        return;
    }
    if (n.isEmpty(selectChannels)) {
        return;
    }
    bool selectOne = !selectButton->ledOn() && n.selectStart + 1 == n.selectEnd;
    DisplayNote& oneNote = n.notes[n.selectStart];
    if (selectOne && MIDI_TEMPO == oneNote.type) {
        oneNote.setTempo(nt()->wheelToTempo(horizontalWheel->getValue()) * 500000);
        displayBuffer->redraw();
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
        this->invalidateAndPlay(Inval::change);
        return;
    }
    if (selectOne && KEY_SIGNATURE == oneNote.type) {
        oneNote.setMinor(wheelValue);
        this->invalidateAndPlay(Inval::change);
        return;
    }
    Inval inval = Inval::none;
    if (!selectButton->ledOn()) {
        if (Wheel::vertical == edit.wheel) {
            edit.clear();
            edit.init(n, selectChannels);
        }
        edit.wheel = Wheel::horizontal;
        SCHMICKLE((unsigned) wheelValue < NoteDurations::Count());
        int wheelChange = wheelValue - edit.horizontalValue;
        int startTime = edit.base[n.selectStart].startTime;
        int selectMaxEnd = 0;
        int maxEnd = 0;
        vector<std::pair<int, int>> overlaps; // orig end time, adj end time
        // proportionately adjust start times and durations of all in selection
        // additionally, compute the insertedTime as n.nextStart - selectStart.start mod wheel chang
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            const auto& test = edit.base[index];
            if (!test.isSelectable(selectChannels)) {
                continue;
            }
            auto* note = &n.notes[index];
            int startDiff = test.startTime - startTime;
            if (startDiff) {
                if (debugVerbose) DEBUG("old note start %s", note->debugString().c_str());
                if (wheelChange) {
                    int index = NoteDurations::FromMidi(startDiff, n.ppq);
                    note->startTime = startTime + NoteDurations::ToMidi(index + wheelChange, n.ppq);
                } else {
                    note->startTime = test.startTime;
                }
                if (debugVerbose) DEBUG("new note start %s", note->debugString().c_str());
            }
            if (test.isNoteOrRest()) {
                if (debugVerbose) DEBUG("old note duration %s", note->debugString().c_str());
                if (wheelChange) {
                    int oldDurationIndex = NoteDurations::FromMidi(test.duration, n.ppq);
                    note->duration = NoteDurations::ToMidi(oldDurationIndex + wheelChange, n.ppq);
                } else {
                    note->duration = test.duration;
                }
                if (debugVerbose) DEBUG("new note duration %s", note->debugString().c_str());
                if (edit.voice) {
                    n.setDuration(note);
                } else if (test.endTime() > edit.selectMaxEnd) {
                    overlaps.emplace_back(std::pair<int, int>(test.endTime(), note->endTime()));
                    if (debugVerbose) DEBUG("add overlap base %d note %d",
                            test.endTime(), note->endTime());
                }
            }
            if (test.endTime() <= edit.nextStart) {
                if (debugVerbose) DEBUG("selectMaxEnd %d note end time %d", selectMaxEnd,
                        note->endTime());
                selectMaxEnd = std::max(selectMaxEnd, note->endTime());
            }
            if (debugVerbose) DEBUG("maxEnd %d note end time %d", maxEnd, note->endTime());
            maxEnd = std::max(maxEnd, note->endTime());
        }
        overlaps.emplace_back(std::pair<int, int>(edit.selectMaxEnd, selectMaxEnd));
        if (!edit.voice) {
            std::sort(overlaps.begin(), overlaps.end(),
                    [](const std::pair<int,int>& left, const std::pair<int,int>& right) {
                return left.second < right.second;
            });
            auto* overPtr = &overlaps.front();
            int insertedTime = overPtr->second - overPtr->first;
            if (debugVerbose) DEBUG("insertedTime %d", insertedTime);
            // note subtract one skips track end
            const unsigned last = n.notes.size() - 1;
            SCHMICKLE(TRACK_END == edit.base[last].type);
            SCHMICKLE(TRACK_END == n.notes[last].type);
            for (unsigned index = n.selectEnd; index < last; ++index) {
                const auto& base = edit.base[index];
                auto* note = &n.notes[index];
                if (base.isSelectable(selectChannels)) {
                    while (overPtr < &overlaps.back()) {
                        auto overTest = overPtr + 1;
                        if (overTest->first > base.startTime) {
                            break;
                        }
                        overPtr = overTest;
                        insertedTime = std::max(insertedTime, overPtr->second - overPtr->first);
                        if (debugVerbose) DEBUG("over insertedTime %d", insertedTime);
                    }
                    if (debugVerbose) DEBUG("old post start %s", note->debugString().c_str());
                    note->startTime = base.startTime + insertedTime;
                    if (debugVerbose) DEBUG("new post start %s", note->debugString().c_str());
                }
                if (debugVerbose) DEBUG("post maxEnd %d note end time %d", maxEnd, note->endTime());
                maxEnd = std::max(maxEnd, note->endTime());
            }
        }
        // make sure track end is adjusted as necessary
        SCHMICKLE(TRACK_END == n.notes.back().type);
        n.notes.back().startTime = maxEnd;
        inval = Inval::note;
        NoteTaker::Sort(n.notes);
    } else {
        unsigned start, end;
        edit.voice = false;
        if (selectButton->editEnd()) {
            clipboardInvalid = true;
            int wheelStart = this->noteToWheel(
                    selectButton->selStart - (int) selectButton->saveZero) + 1;
            start = this->wheelToNote(std::min(wheelValue, wheelStart));
            end =  this->wheelToNote(std::max(wheelValue + 1, wheelStart));
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d wheelStart %d",
                    start, end, wheelValue, wheelStart);
        } else {
            start = this->wheelToNote(wheelValue);
            selectButton->saveZero = selectButton->isSingle() && !start;
            end = nt()->nextAfter(start, 1);
            if (debugVerbose) DEBUG("start %u end %u wheelValue %d", start, end, wheelValue);
        }
        SCHMICKLE(start < end);
        if (start != n.selectStart || end != n.selectEnd) {
            nt()->setSelect(start, end);
            edit.init(n, selectChannels);
            this->setVerticalWheelRange();
            inval = Inval::display;
            if (start + 1 == end) {
                const auto& note = n.notes[start];
                if (KEY_SIGNATURE == note.type && !note.key()) {
                    display->dynamicCNaturalTimer = nt()->realSeconds + display->fadeDuration;
                    display->dynamicCNaturalAlpha = 0xFF;
                }
            }
        }
    }
    if (debugVerbose) n.validate(); // find buggy shifts sooner?
    this->invalidateAndPlay(inval);
}

void NoteTakerWidget::updateSlotVertical(SlotButton::Select select) {
    SlotPlay& slot = storage.playback[storage.selectStart];
    int slotParam = INT_MAX;
    int repMin = INT_MAX;
    int repMax = 0;
    int slotMax = 0;
    switch (select) {
        case SlotButton::Select::repeat:
            // find range of repeat counts in all existing slots
            for (const auto& s : edit.pbase) {
                repMin = std::min(repMin, (int) s.repeat);
                repMax = std::max(repMax, (int) s.repeat);
            }
            slotParam = slot.repeat;
            slotMax = 99;
            break;
        case SlotButton::Select::slot:
            for (const auto& s : edit.pbase) {
                repMin = std::min(repMin, (int) s.index);
                repMax = std::max(repMax, (int) s.index);
            }
            slotParam = slot.index;
            slotMax = storage.size() - 1;
            break;
        case SlotButton::Select::end:
            for (const auto& s : edit.pbase) {
                repMin = std::min(repMin, (int) s.stage);
                repMax = std::max(repMax, (int) s.stage);
            }
            slotParam = (int) slot.stage;
            slotMax = (int) SlotPlay::Stage::never;
            break;
        default:
            _schmickled();
    }
    horizontalWheel->setLimits(slotParam - repMax, slotMax + slotParam - repMin);
    horizontalWheel->setValue(slotParam);
    edit.horizontalValue = slotParam;
    return;
}

void NoteTakerWidget::updateVertical() {
    auto& n = this->n();
    if (runButton->ledOn()) {
        return;
    }
    if (this->menuButtonOn()) {
        displayBuffer->redraw();
    }
    if (!verticalWheel->hasChanged()) {
        return;
    }
    const int wheelValue = verticalWheel->wheelValue();
    if (fileButton->ledOn() || partButton->ledOn()) {
        if (verticalWheel->getValue() <= 2) {
            display->downSelected = true;
            if (fileButton->ledOn()) {
                unsigned val = (unsigned) horizontalWheel->getValue();
                SCHMICKLE(val < storage.size());
                this->copyToSlot(val);
                this->nt()->stageSlot(val);
            } else {
                SCHMICKLE(partButton->ledOn());
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
            if (fileButton->ledOn() && (unsigned) horizontalWheel->getValue() < storage.size()) {
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
    if (slotButton->ledOn()) {
        if (selectButton->isOff()) {
            this->updateSlotVertical((SlotButton::Select) wheelValue);
        }
        return;
    }
    if (sustainButton->ledOn()) {
        // use first unlocked channel as guide
        NoteTakerChannel& channel = nt()->slot->channels[this->unlockedChannel()];
        int sustainDuration = INT_MAX;
        switch ((NoteTakerChannel::Limit) wheelValue) {
            case NoteTakerChannel::Limit::sustainMax:
                sustainDuration = channel.sustainMax;
                break;
            case NoteTakerChannel::Limit::sustainMin:
                sustainDuration = channel.sustainMin;
                break;
            case NoteTakerChannel::Limit::releaseMin:
                sustainDuration = channel.releaseMin;
                break;
            case NoteTakerChannel::Limit::releaseMax:
                sustainDuration = channel.releaseMax;
                break;
            default:
                _schmickled();
        }
        horizontalWheel->setValue(NoteDurations::FromMidi(sustainDuration, n.ppq));
        return;
    }
    if (!selectButton->ledOn()) {
        if (Wheel::horizontal == edit.wheel) {
            edit.clear();
            edit.init(n, selectChannels);
        }
        edit.wheel = Wheel::vertical;
        // transpose selection
        // loop below computes diff of first note, and adds diff to subsequent notes in select
        bool playNotes = false;
        for (unsigned index = n.selectStart ; index < n.selectEnd; ++index) {
            DisplayNote& note = n.notes[index];
            switch (note.type) {
                case KEY_SIGNATURE:
                    if (n.selectStart + 1 == n.selectEnd) {
                        note.setKey(wheelValue);
                        this->invalidateAndPlay(Inval::cut);
                        return;
                    }
                    break;
                case TIME_SIGNATURE:
                    if (n.selectStart + 1 == n.selectEnd && !selectButton->ledOn()) {
                        if (!wheelValue) {
                            horizontalWheel->setDenominator(note);
                        } else {
                            horizontalWheel->setLimits(1, 99.99f);   // numer limit 0 to 99
                            horizontalWheel->setValue(note.numerator());
                        }
                        this->invalidateAndPlay(Inval::cut);
                        return;
                    }
                    break;
                case MIDI_TEMPO:
                    // tempo unaffected by vertical wheel
                    break;
                case NOTE_ON: {
                    if (!note.isSelectable(selectChannels)) {
                        continue;
                    }
                    int value = edit.base[index].pitch() + wheelValue - edit.verticalValue;
                    value = std::max(0, std::min(127, value));
                    if (!n.pitchCollision(note, value)) {
                        note.setPitch(value);
                        // if changing the pitch causes the this pitch to run into the same
                        // pitch on the same channel later, shorten its duration. But, restore the
                        // duration if there is no longer a pitch collision
                        const DisplayNote& base = edit.base[index];
                        note.duration = base.duration;
                        n.setDuration(&note);
                        playNotes = true;
                    }
                }
                default:
                    ;
            }
        }
        if (playNotes) {
            this->invalidateAndPlay(Inval::change);
        }
        return;
    } else if (selectButton->editEnd()) {
        if (!wheelValue) {
            edit.voice = false;
            n.selectStart = edit.originalStart;
            n.selectEnd = edit.originalEnd;
            DEBUG("selStart %u / selEnd %u", n.selectStart, n.selectEnd);
        } else {
            edit.voice = true;
            n.selectStart = edit.voices[wheelValue - 1];
            n.selectEnd = n.selectStart + 1;
            DEBUG("selStart %u / selEnd %u wheel %d", n.selectStart, n.selectEnd, wheelValue);
        }
        this->invalidateAndPlay(Inval::display);
    } else {
        SCHMICKLE(selectButton->editStart());
        for (unsigned index = n.selectStart; index < n.selectEnd; ++index) {
            if (NOTE_ON != n.notes[index].type) {
                return;  // give up early if selection contains something other than a note
            }
        }
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
        if (!edit.voice) {
            int duration = base ? base->duration : n.ppq;
            int startTime = base ? base->startTime : n.notes[n.selectStart].startTime;
            DisplayNote iNote(NOTE_ON, startTime, duration, (uint8_t) this->unlockedChannel());
            iNote.setPitch(pitch);
            n.selectStart += 1;
            n.selectEnd = n.selectStart + 1;
            n.notes.insert(n.notes.begin() + n.selectStart, iNote);
            edit.voice = true;
            this->invalidateAndPlay(Inval::note);
        } else {
            n.notes[n.selectStart].setPitch(pitch);
            this->invalidateAndPlay(Inval::change);
        }
    }
    if (debugVerbose) n.validate(); // find buggy shifts sooner?
}
