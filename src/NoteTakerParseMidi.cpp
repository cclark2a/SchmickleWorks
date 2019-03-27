#include "NoteTakerParseMidi.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

bool NoteTakerParseMidi::parseMidi() {
    debug("parseMidi start");
    vector<DisplayNote> parsedNotes;
    if (midi.size() < 14) {
        debug("MIDI file too small size=%llu", midi.size());
        return false;
    }
    vector<uint8_t>::const_iterator iter = midi.begin();
    if (!match_midi(iter, MThd)) {
        for (auto iter = MThd.begin(); iter != MThd.end(); ++iter) {
            debug("%c", *iter);
        }
        debug("expect MIDI header, got %c%c%c%c (0x%02x%02x%02x%02x)", 
                midi[0], midi[1], midi[2], midi[3],
                midi[0], midi[1], midi[2], midi[3]);
        return false;
    }
    if (!match_midi(iter, MThd_length)) {
        debug("expect MIDI header size == 6, got (0x%02x%02x%02x%02x)", 
                midi[4], midi[5], midi[6], midi[7]);
        return false;
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
        return false;
    }
    int midiFormat = displayNote.data[0];
    int trackCount = -1;
    parsedNotes.push_back(displayNote);
    do {
        bool trackEnded = false;
        bool skipFirstDeltaTime = false;
        vector<uint8_t>::const_iterator trk = iter;
        // parse track header before parsing channel voice messages
        if (!match_midi(iter, MTrk)) {
            debug("expect MIDI track, got %c%c%c%c (0x%02x%02x%02x%02x)", 
                    trk[0], trk[1], trk[2], trk[3],
                    trk[0], trk[1], trk[2], trk[3]);
            return false;
        }
        int trackLength;
        if (!midi_size32(iter, &trackLength)) {
            debug("invalid track length");
            return false;
        }
        ++trackCount;
        int midiTime = 0;
        debug("trackLength %d", trackLength);
        while (!trackEnded && trackLength) {
            const auto messageStart = iter;
            if (skipFirstDeltaTime) {
                midiTime = 0;
                skipFirstDeltaTime = false;
            } else if (!midi_delta(iter, &midiTime)) {
                return false;
            }
            debug("midiTime %d", midiTime);
            if (0 == (*iter & 0x80)) {
                debug("expected high bit set on channel voice message: %02x", *iter);
                return false;
            }
            displayNote.startTime = midiTime;
            displayNote.duration = -1;  // not known yet
            displayNote.type = (DisplayType) ((*iter >> 4) & 0x7);
            displayNote.channel = *iter++ & 0x0F;
            if (!displayNote.channel && trackCount > 0) {
                displayNote.channel = trackCount;
            }
            memset(displayNote.data, 0, sizeof(displayNote.data));
            debug ("displayNote.type 0x%02x", displayNote.type);
            switch(displayNote.type) {
                case UNUSED: {  // midi note off
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 1");
                        return false;
                    }
                    int pitch = *iter++;
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 2");
                        return false;
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
                            debug("unexpected note duration start=%d end=%d",
                                    ri->startTime, midiTime);
                        }
                        ri->duration = midiTime - ri->startTime;
                        ri->setOffVelocity(velocity);
                        found = true;
                        break;
                    }
                    if (!found) {
                        debug("note: note off channel=%d pitch=%d not found",
                                displayNote.channel, pitch);
                    }
                    trackLength -= iter - messageStart;
                    continue;  // do not store note off
                } break;
                case NOTE_ON: {
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 3");
                        return false;
                    }
                    displayNote.setPitch(*iter++);
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 4");
                        return false;
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
                        parsedNotes.push_back(rest);
                    }
                } break;
                case KEY_PRESSURE:
                case CONTROL_CHANGE:
                case PITCH_WHEEL:
                    for (int i = 0; i < 2; i++) {
                        if (!midi_check7bits(iter)) {
                            debug("midi_check7bits 5");
                            return false;
                        }
                        displayNote.data[i] = *iter++;
                    }
                break;
                case PROGRAM_CHANGE:
                case CHANNEL_PRESSURE:
                    if (!midi_check7bits(iter)) {
                        debug("midi_check7bits 6");
                        return false;
                    }
                    displayNote.data[0] = *iter++;
                break;
                case MIDI_SYSTEM:
                    debug("system message 0x%02x", displayNote.channel);
                    switch (displayNote.channel) {
                        case 0x0:  // system exclusive
                            displayNote.data[0] = iter - midi.begin();  // offset of message start
                            while (++iter != midi.end() && 0 == (*iter & 0x80))
                                ;
                            displayNote.data[1] = iter - midi.begin();  // offset of message end
                            if (0xF7 != *iter++) {
                                debug("expected system exclusive terminator %02x", *iter);
                            }
                            break;
                        case 0x1: // undefined
                            break;
                        case 0x2: // song position pointer
                            for (int i = 0; i < 2; i++) {
                                if (!midi_check7bits(iter)) {
                                    debug("midi_check7bits 7");
                                    return false;
                                }
                                displayNote.data[i] = *iter++;
                            }
                            break;
                        case 0x3: // song select
                            if (!midi_check7bits(iter)) {
                                debug("midi_check7bits 8");
                                return false;
                            }
                            displayNote.data[0] = *iter++;
                        break;
                        case 0x4: // undefined
                        case 0x5: // undefined
                        case 0x6: // tune request
                        break;
                        case 0x7: // end of exclusive
                            debug("end without beginning");
                        break;
                        case 0xF: // meta event
                            if (!midi_check7bits(iter)) {
                                debug("midi_check7bits 9");
                                return false;
                            }
                            displayNote.data[0] = *iter++;
                            if (!midi_size8(iter, &displayNote.data[1])) {
                                debug("expected meta event length");
                                return false;
                            }
                            debug("meta event 0x%02x", displayNote.data[0]);
                            switch (displayNote.data[0]) {
                                case 0x00:  // sequence number: 
                                            // http://midi.teragonaudio.com/tech/midifile/seq.htm                                   
                                    if (2 == displayNote.data[1]) { // two bytes for # follow
                                        for (int i = 2; i < 4; i++) {
                                            if (!midi_check7bits(iter)) {
                                                debug("midi_check7bits 10");
                                                return false;
                                            }
                                            displayNote.data[i] = *iter++;
                                        }
                                    } else if (0 != displayNote.data[1]) {
                                        debug("expected sequence number length of 0 or 2: %d",
                                                displayNote.data[1]);
                                        return false;
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
                                        debug("meta text length %d exceeds file:", 
                                                displayNote.data[1]);
                                    }
                                    std::advance(iter, displayNote.data[1]);
                                } break;
                                case 0x20: // channel prefix
                                    if (1 != displayNote.data[1]) {
                                        debug("expected channel prefix length == 1 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    if (!midi_check7bits(iter)) {
                                        debug("midi_check7bits 11");
                                        return false;
                                    }
                                    displayNote.data[2] = *iter++;
                                break;
                                case 0x2F: // end of track (required)
                                    displayNote.type = TRACK_END;
                                    if (0 != displayNote.data[1]) {
                                        debug("expected end of track length == 0 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    trackEnded = true;
                                break;
                                case 0x51:  // set tempo
                                    displayNote.type = MIDI_TEMPO;
                                    if (3 != displayNote.data[1]) {
                                        debug("expected set tempo length == 3 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    if (!midi_size24(iter, &displayNote.data[2])) {
                                        debug("midi_size24");
                                        return false;
                                    }
                                    skipFirstDeltaTime = !displayNote.startTime && 1 == midiFormat;
                                    debug("tempo %d", displayNote.data[2]);
                                break;
                                case 0x54: // SMPTE offset
                                    if (5 != displayNote.data[1]) {
                                        debug("expected SMPTE offset length == 5 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    displayNote.data[2] = iter - midi.begin();
                                    std::advance(iter, displayNote.data[1]);
                                break;
                                case 0x58: // time signature
                                    displayNote.type = TIME_SIGNATURE;
                                    displayNote.duration = 0;
                                    for (int i = 0; i < 4; ++i) {
                                        if (!midi_check7bits(iter)) {
                                            debug("midi_check7bits 12");
                                            return false;
                                        }
                                        displayNote.data[i] = *iter++;
                                    }
                                    if (!displayNote.isValid()) {
                                        debug("invalid %s 2", displayNote.debugString().c_str());
                                        return false;
                                    }
                                break;
                                case 0x59:  // key signature
                                    displayNote.type = KEY_SIGNATURE;
                                    displayNote.duration = 0;
                                    displayNote.data[0] = (signed char) *iter++;
                                    displayNote.data[1] = *iter++;
                                    if (!displayNote.isValid()) {
                                        debug("invalid %s 3", displayNote.debugString().c_str());
                                        return false;
                                    }
                                break;
                                case 0x7F: // sequencer specific meta event
                                    displayNote.data[2] = iter - midi.begin();
                                    if (midi.end() - iter < displayNote.data[1]) {
                                        debug("meta text length %d exceeds file:", 
                                                displayNote.data[1]);
                                    }
                                    std::advance(iter, displayNote.data[1]);
                                break;
                                default:
                                    debug("unexpected meta: 0x%02x", displayNote.data[0]);
                            }

                        break;
                        default:    
                            debug("unexpected real time message 0x%02x",
                                    0xF0 | displayNote.channel);
                            return false;
                    }
                    displayNote.channel = 0xFF;
                break;
                default:
                    debug("unexpected byte %d", *iter);
                    return false;
            }
            debug("length used %d", iter - messageStart);
            trackLength -= iter - messageStart;
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
            if (!midiFormat || !trackCount || displayNote.isNoteOrRest()) {
                parsedNotes.push_back(displayNote);
            }
        }
    } while (iter != midi.end());
    for (auto it = parsedNotes.begin(), end = parsedNotes.end(); it != end; ++it) {
        auto const insertion_point = std::upper_bound(parsedNotes.begin(), it, *it);
        std::rotate(insertion_point, it, it + 1);
    }
    int lastTime = -1;
    for (const auto& note : parsedNotes) {
        // to do : shouldn't allow zero either, let it slide for now to debug 9.mid
        if (NOTE_ON == note.type && 0 > note.duration) {
            debug("negative note on duration");
            NoteTaker::DebugDump(parsedNotes);
            return false;
        }
        if (note.startTime < lastTime) {
            debug("notes out of time order");
            NoteTaker::DebugDump(parsedNotes);
            return false;
        }
        lastTime = note.startTime;
    }
    NoteTaker::DebugDump(parsedNotes);
    displayNotes.swap(parsedNotes);
    return true;
}
