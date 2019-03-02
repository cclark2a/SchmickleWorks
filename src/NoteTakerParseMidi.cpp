#include "NoteTakerParseMidi.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

void NoteTakerParseMidi::parseMidi() {
    debug("parseMidi start");
    vector<DisplayNote> parsedNotes;
    int midiTime = 0;
    if (midi.size() < 14) {
        debug("MIDI file too small size=%llu\n", midi.size());
        return;
    }
    vector<uint8_t>::const_iterator iter = midi.begin();
    if (!match_midi(iter, MThd)) {
        for (auto iter = MThd.begin(); iter != MThd.end(); ++iter) {
            debug("%c\n", *iter);
        }
        debug("expect MIDI header, got %c%c%c%c (0x%02x%02x%02x%02x)\n", 
                midi[0], midi[1], midi[2], midi[3],
                midi[0], midi[1], midi[2], midi[3]);
        return;
    }
    if (!match_midi(iter, MThd_length)) {
        debug("expect MIDI header size == 6, got (0x%02x%02x%02x%02x)\n", 
                midi[4], midi[5], midi[6], midi[7]);
        return;
    }
    DisplayNote displayNote;
    displayNote.startTime = 0;
    displayNote.duration = 0;
    displayNote.type = MIDI_HEADER;
    displayNote.channel = 0xFF;
    for (int i = 0; i < 3; ++i) {
        read_midi16(iter, &displayNote.data[i]);
    }
    if (!displayNote.isValid()) {
        debug("invalid %s", displayNote.debugString().c_str());
        return;
    }
    parsedNotes.push_back(displayNote);
    vector<uint8_t>::const_iterator trk = iter;
    // parse track header before parsing channel voice messages
    if (!match_midi(iter, MTrk)) {
        debug("expect MIDI track, got %c%c%c%c (0x%02x%02x%02x%02x)\n", 
                trk[0], trk[1], trk[2], trk[3],
                trk[0], trk[1], trk[2], trk[3]);
        return;
    }
    int trackLength;
    if (!midi_size32(iter, &trackLength)) {
        debug("invalid track length");
        return;
    }
    while (iter != midi.end()) {
        if (!midi_delta(iter, &midiTime)) {
            return;
        }
        if (0 == (*iter & 0x80)) {
            debug("expected high bit set on channel voice message: %02x\n", *iter);
            return;
        }
        displayNote.startTime = midiTime;
        displayNote.duration = -1;  // not known yet
        displayNote.type = (DisplayType) ((*iter >> 4) & 0x7);
        displayNote.channel = *iter++ & 0x0F;
        memset(displayNote.data, 0, sizeof(displayNote.data));
        switch(displayNote.type) {
            case UNUSED: {  // midi note off
                if (!midi_check7bits(iter)) {
                    debug("midi_check7bits 1");
                    return;
                }
                int pitch = *iter++;
                if (!midi_check7bits(iter)) {
                    debug("midi_check7bits 2");
                    return;
                }
                int velocity = *iter++;
                bool found = false;
                for (auto ri = parsedNotes.rbegin(); ri != parsedNotes.rend(); ++ri) {
                    if (ri->type != NOTE_ON) {
                        continue;
                    }
                    if (ri->duration != -1) {
                        continue;
                    }
                    if (displayNote.channel != ri->channel) {
                        continue;
                    }
                    if (pitch != ri->pitch()) {
                        continue;
                    }
                    if (midiTime <= ri->startTime) {
                        debug("unexpected note duration start=%d end=%d\n",
                                ri->startTime, midiTime);
                    }
                    ri->duration = midiTime - ri->startTime;
                    ri->setNote(NoteTakerDisplay::DurationIndex(ri->duration));
                    ri->setOffVelocity(velocity);
                    found = true;
                    break;
                }
                if (!found) {
                    debug("note: note off channel=%d pitch=%d not found\n",
                            displayNote.channel, pitch);
                }
                continue;  // do not store note off
            } break;
            case NOTE_ON: {
                if (!midi_check7bits(iter)) {
                    debug("midi_check7bits 3");
                    return;
                }
                displayNote.setPitch(*iter++);
                if (!midi_check7bits(iter)) {
                    debug("midi_check7bits 4");
                    return;
                }
                displayNote.setOnVelocity(*iter++);
                DisplayNote& prior = parsedNotes.back();
                if (prior.duration >= 0 && prior.endTime() < displayNote.startTime) {
                    // to do : create constructor so this can call emplace_back ?
                    DisplayNote rest;
                    rest.startTime = prior.endTime();
                    rest.duration = displayNote.startTime - rest.startTime;
                    rest.type = REST_TYPE;
                    rest.channel = 0xFF;
                    memset(rest.data, 0, sizeof(rest.data));
                    rest.setRest(NoteTakerDisplay::DurationIndex(rest.duration));
                    parsedNotes.push_back(rest);
                }
            } break;
            case KEY_PRESSURE:
            case CONTROL_CHANGE:
            case PITCH_WHEEL:
                for (int i = 0; i < 2; i++) {
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 5");
                        return;
                    }
                    displayNote.data[i] = *iter++;
                }
            break;
            case PROGRAM_CHANGE:
            case CHANNEL_PRESSURE:
                if (!midi_check7bits(iter)) {
                    debug("midi_check7bits 6");
                    return;
                }
                displayNote.data[0] = *iter++;
            break;
            case MIDI_SYSTEM:
                switch (displayNote.channel) {
                    case 0x0:  // system exclusive
                        displayNote.data[0] = iter - midi.begin();  // offset of message start
                        while (++iter != midi.end() && 0 == (*iter & 0x80))
                            ;
                        displayNote.data[1] = iter - midi.begin();  // offset of message end
                        if (0xF7 != *iter++) {
                            debug("expected system exclusive terminator %02x\n", *iter);
                        }
                        break;
                    case 0x1: // undefined
                        break;
                    case 0x2: // song position pointer
                        for (int i = 0; i < 2; i++) {
                            if (!midi_check7bits(iter)) {
                                debug("midi_check7bits 7");
                                return;
                            }
                            displayNote.data[i] = *iter++;
                        }
                        break;
                    case 0x3: // song select
                        if (!midi_check7bits(iter)) {
                            debug("midi_check7bits 8");
                            return;
                        }
                        displayNote.data[0] = *iter++;
                    break;
                    case 0x4: // undefined
                    case 0x5: // undefined
                    case 0x6: // tune request
                    break;
                    case 0x7: // end of exclusive
                        debug("end without beginning\n");
                    break;
                    case 0xF: // meta event
                        if (!midi_check7bits(iter)) {
                            debug("midi_check7bits 9");
                            return;
                        }
                        displayNote.data[0] = *iter++;
                        if (!midi_size8(iter, &displayNote.data[1])) {
                            debug("expected meta event length\n");
                            return;
                        }
                        switch (displayNote.data[0]) {
                            case 0x00:  // sequence number: 
                                        // http://midi.teragonaudio.com/tech/midifile/seq.htm                                   
                                if (2 == displayNote.data[1]) { // two bytes for # follow
                                    for (int i = 2; i < 4; i++) {
                                        if (!midi_check7bits(iter)) {
                                            debug("midi_check7bits 10");
                                            return;
                                        }
                                        displayNote.data[i] = *iter++;
                                    }
                                } else if (0 != displayNote.data[1]) {
                                    debug("expected sequence number length of 0 or 2: %d\n",
                                            displayNote.data[1]);
                                    return;
                                }
                            break;
                            case 0x01: // text event
                            case 0x02: // copyright notice
                            case 0x03: // sequence/track name
                            case 0x04: // instument name
                            case 0x05: // lyric
                            case 0x06: // marker
                            case 0x07: { // cue point
                                displayNote.data[2] = iter - midi.begin();
                                if (midi.end() - iter < displayNote.data[1]) {
                                    debug("meta text length %d exceeds file:\n", 
                                            displayNote.data[1]);
                                }
                                std::advance(iter, displayNote.data[1]);
                            } break;
                            case 0x20: // channel prefix
                                if (1 != displayNote.data[1]) {
                                    debug("expected channel prefix length == 1 %d\n",
                                            displayNote.data[1]);
                                    return;
                                }
                                if (!midi_check7bits(iter)) {
                                    debug("midi_check7bits 11");
                                    return;
                                }
                                displayNote.data[2] = *iter++;
                            break;
                            case 0x2F: // end of track (required)
                                displayNote.type = TRACK_END;
                                if (0 != displayNote.data[1]) {
                                    debug("expected end of track length == 0 %d\n",
                                            displayNote.data[1]);
                                    return;
                                }
                            break;
                            case 0x51:  // set tempo
                                displayNote.type = MIDI_TEMPO;
                                if (3 != displayNote.data[1]) {
                                    debug("expected set tempo length == 3 %d\n",
                                            displayNote.data[1]);
                                    return;
                                }
                                if (!midi_size24(iter, &displayNote.data[2])) {
                                    debug("midi_size24");
                                    return;
                                }
                            break;
                            case 0x54: // SMPTE offset
                                if (5 != displayNote.data[1]) {
                                    debug("expected SMPTE offset length == 5 %d\n",
                                            displayNote.data[1]);
                                    return;
                                }
                                displayNote.data[2] = iter - midi.begin();
                                std::advance(iter, displayNote.data[1]);
                            break;
                            case 0x58: // time signature
                                displayNote.type = TIME_SIGNATURE;
                                for (int i = 0; i < 4; ++i) {
                                    if (!midi_check7bits(iter)) {
                                        debug("midi_check7bits 12");
                                        return;
                                    }
                                    displayNote.data[i] = *iter++;
                                }
                                if (!displayNote.isValid()) {
                                    debug("invalid %s 2", displayNote.debugString().c_str());
                                    return;
                                }
                            break;
                            case 0x59:  // key signature
                                displayNote.type = KEY_SIGNATURE;
                                for (int i = 0; i < 2; ++i) {
                                    if (!midi_check7bits(iter)) {
                                        debug("midi_check7bits 13");
                                        return;
                                    }
                                    displayNote.data[i] = *iter++;
                                }
                                if (!displayNote.isValid()) {
                                    debug("invalid %s 3", displayNote.debugString().c_str());
                                    return;
                                }
                            break;
                            case 0x7F: // sequencer specific meta event
                                displayNote.data[2] = iter - midi.begin();
                                if (midi.end() - iter < displayNote.data[1]) {
                                    debug("meta text length %d exceeds file:\n", 
                                            displayNote.data[1]);
                                }
                                std::advance(iter, displayNote.data[1]);
                            break;
                            default:
                                debug("unexpected meta: 0x%02x\n", displayNote.data[0]);
                        }

                    break;
                    default:    
                        debug("unexpected real time message 0x%02x\n",
                                0xF0 | displayNote.channel);
                        return;
                }
                displayNote.channel = 0xFF;
            break;
            default:
                debug("unexpected byte %d\n", *iter);
                return;
        }
        // hijack 0x00 0xBx 0x57-0x5A 0xXX to set sustain and release parameters
        // only intercepts if it occurs at time zero, and 0xXX value is in range
        if (CONTROL_CHANGE == displayNote.type && 0 == displayNote.startTime &&
                midiReleaseMax <= displayNote.data[0] && displayNote.data[0] <= midiSustainMax &&
                (unsigned) displayNote.data[1] < noteDurations.size()) {
            channels[displayNote.channel].setLimit(
                    (NoteTakerChannel::Limit) (displayNote.data[0] - midiReleaseMax),
                    noteDurations[displayNote.data[1]]);
            continue;
        }
        parsedNotes.push_back(displayNote);
    }
    NoteTaker::DebugDump(parsedNotes);
    displayNotes.swap(parsedNotes);
}
