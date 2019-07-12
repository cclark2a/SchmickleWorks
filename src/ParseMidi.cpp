#include "ParseMidi.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "Taker.hpp"

bool NoteTakerParseMidi::parseMidi() {
    if (debugVerbose) DEBUG("parseMidi start");
    vector<DisplayNote> parsedNotes;
    if (midi.size() < 14) {
        DEBUG("MIDI file too small size=%llu", midi.size());
        return false;
    }
    vector<uint8_t>::const_iterator iter = midi.begin();
    if (!match_midi(iter, MThd)) {
        for (auto iter = MThd.begin(); iter != MThd.end(); ++iter) {
            DEBUG("%c", *iter);
        }
        DEBUG("expect MIDI header, got %c%c%c%c (0x%02x%02x%02x%02x)", 
                midi[0], midi[1], midi[2], midi[3],
                midi[0], midi[1], midi[2], midi[3]);
        return false;
    }
    int MThd_length = 0;
    if (!midi_size32(iter, &MThd_length) || 6 != MThd_length) {
        DEBUG("expect MIDI header size == 6, got (0x%02x%02x%02x%02x)", 
                midi[4], midi[5], midi[6], midi[7]);
        return false;
    }
    DisplayNote displayNote(MIDI_HEADER);
    for (int i = 0; i < 3; ++i) {
        read_midi16(iter, &displayNote.data[i]);
    }
    if (!displayNote.isValid()) {
        DEBUG("invalid %s", displayNote.debugString().c_str());
        debug_out(iter);
        return false;
    }
    int midiFormat = displayNote.format();
    int ppq = displayNote.ppq();
    int trackCount = -1;
    int midiTime;
    parsedNotes.push_back(displayNote);
    bool channelUsed[CHANNEL_COUNT];
    memset(channelUsed, 0, sizeof(channelUsed));
    DisplayNote trackEnd(TRACK_END);  // for missing end
    do {
        bool trackEnded = false;
        vector<uint8_t>::const_iterator trk = iter;
        // parse track header before parsing channel voice messages
        if (!match_midi(iter, MTrk)) {
            DEBUG("expect MIDI track, got %c%c%c%c (0x%02x%02x%02x%02x)", 
                    trk[0], trk[1], trk[2], trk[3],
                    trk[0], trk[1], trk[2], trk[3]);
            debug_out(iter);
            return false;
        }
        int trackLength;
        if (!midi_size32(iter, &trackLength)) {
            DEBUG("invalid track length");
            debug_out(iter);
            return false;
        }
        ++trackCount;
        midiTime = 0;
        if (debugVerbose) DEBUG("trackLength %d", trackLength);
        unsigned lowNibble;
        unsigned runningStatus = -1;
        // don't allow notes with the same pitch and channel to overlap
        // to detect this economically, store last pitch/channel combo in table
        unsigned pitched[CHANNEL_COUNT][128];
        memset(pitched, 0, sizeof(pitched));
        unsigned last[CHANNEL_COUNT];
        memset(last, 0, sizeof(last));
        // find next note; note on followed by note off, within a short distance
        // don't allow delta time to include next note
        while (!trackEnded && trackLength) {
            const auto messageStart = iter;
            int delta;
            if (!midi_size8(iter, &delta) || delta < 0) {
                DEBUG("invalid midi time");
                debug_out(iter);
                return false;
            }
            midiTime += delta;
            if (0 == (*iter & 0x80)) {
                if (0 == (runningStatus & 0x80)) {
                    DEBUG("%d expected running status 0x%02x hi bit set", midiTime, runningStatus);
                    debug_out(iter);
                    return false;
                }
            } else {
                runningStatus = *iter++;
            }
            displayNote.cache = nullptr;
            displayNote.startTime = midiTime;
            displayNote.duration = -1;  // not known yet
            memset(displayNote.data, 0, sizeof(displayNote.data));
            displayNote.channel = runningStatus & 0x0F;
            lowNibble = displayNote.channel;
            if (!displayNote.channel && trackCount > 0) {
                displayNote.channel = trackCount;
            }
            displayNote.type = (DisplayType) ((runningStatus >> 4) & 0x7);
            displayNote.selected = false;
            if (NOTE_ON == displayNote.type || NOTE_OFF == displayNote.type) {
                if (!midi_check7bits(iter, "pitch", midiTime)) {
                    return false;
                }
                displayNote.setPitch(*iter++);
                if (!midi_check7bits(iter, "velocity", midiTime)) {
                    return false;
                }
                displayNote.setOnVelocity(*iter++);
                if (displayNote.onVelocity() == 0) {
                    displayNote.type = NOTE_OFF;
                }
            }
            switch(displayNote.type) {
                case NOTE_OFF: {
                    auto noteOnIndex = pitched[displayNote.channel][displayNote.pitch()];
                    DisplayNote* noteOn = nullptr;
                    if (noteOnIndex) {
                        noteOn = &(&parsedNotes.front())[noteOnIndex];
                        if (noteOn->duration < 0) {
                            noteOn->duration =
                                    std::max(NoteDurations::SmallestMidi(ppq), midiTime - noteOn->startTime);
                            noteOn->setOffVelocity(displayNote.onVelocity());
                            DEBUG("assign vel %s", noteOn->debugString().c_str());
                        } else {
                            DEBUG("unexpected note off old %s new %s",
                                 noteOn->debugString().c_str(), displayNote.debugString().c_str());
                        }
                    } else {
                        DEBUG("missing note on: off %s", displayNote.debugString().c_str());
                    }
                    auto lastOnChannelIndex = last[displayNote.channel];
                    if (lastOnChannelIndex) {
                        SCHMICKLE(noteOn);
                        DisplayNote* lastOnChannel = &(&parsedNotes.front())[lastOnChannelIndex];
                        if (lastOnChannel->endTime() == noteOn->startTime + 1) {
                            lastOnChannel->setSlur(true);
                            noteOn->setSlur(true);
                        }
                    }
                    trackLength -= iter - messageStart;
                    continue;  // do not store note off
                };
                case NOTE_ON: {
                    auto noteOnIndex = pitched[displayNote.channel][displayNote.pitch()];
                    if (noteOnIndex) {
                        DisplayNote* noteOn = &(&parsedNotes.front())[noteOnIndex];
                        if (midiTime == noteOn->startTime) {
                            DEBUG("duplicate note on: old %s new %s",
                                  noteOn->debugString().c_str(), displayNote.debugString().c_str());
                            continue;  // don't add the same note on twice
                        }
                        if (noteOn->duration < 0) {
                            DEBUG("missing note off old %s new %s",
                                  noteOn->debugString().c_str(), displayNote.debugString().c_str());
                            noteOn->duration = midiTime - noteOn->startTime;
                        }
                    }
                    pitched[displayNote.channel][displayNote.pitch()] = parsedNotes.size();
                    last[displayNote.channel] = parsedNotes.size();
                }
                break;
                case KEY_PRESSURE:
                case CONTROL_CHANGE:
                case PITCH_WHEEL:
                    for (int i = 0; i < 2; i++) {
                        if (!midi_check7bits(iter, "pitch wheel", midiTime)) {
                            return false;
                        }
                        displayNote.data[i] = *iter++;
                    }
                break;
                case PROGRAM_CHANGE:
                    if (!midi_check7bits(iter, "program change", midiTime)) {
                        return false;
                    }
                    displayNote.data[0] = *iter++;
                break;
                case CHANNEL_PRESSURE:
                    if (!midi_check7bits(iter, "channel pressure", midiTime)) {
                        return false;
                    }
                    displayNote.data[0] = *iter++;
                break;
                case MIDI_SYSTEM:
                    displayNote.channel = 0;
                    if (debugVerbose) DEBUG("system message 0x%02x", lowNibble);
                    switch (lowNibble) {
                        case 0x0:  // system exclusive
                            displayNote.data[0] = iter - midi.begin();  // offset of message start
                            while (++iter != midi.end() && 0 == (*iter & 0x80))
                                ;
                            displayNote.data[1] = iter - midi.begin();  // offset of message end
                            if (0xF7 != *iter++) {
                                DEBUG("expected system exclusive terminator %02x", *iter);
                            }
                            break;
                        case 0x1: // undefined
                            break;
                        case 0x2: // song position pointer
                            for (int i = 0; i < 2; i++) {
                                if (!midi_check7bits(iter, "song position pointer", midiTime)) {
                                    return false;
                                }
                                displayNote.data[i] = *iter++;
                            }
                            break;
                        case 0x3: // song select
                            if (!midi_check7bits(iter, "song select", midiTime)) {
                                return false;
                            }
                            displayNote.data[0] = *iter++;
                        break;
                        case 0x4: // undefined
                        case 0x5: // undefined
                        case 0x6: // tune request
                        break;
                        case 0x7: // end of exclusive
                            DEBUG("end without beginning");
                        break;
                        case 0xF: // meta event
                            if (!midi_check7bits(iter, "meta event", midiTime)) {
                                return false;
                            }
                            displayNote.data[0] = *iter++;
                            if (!midi_size8(iter, &displayNote.data[1])) {
                                DEBUG("expected meta event length");
                                return false;
                            }
                            if (debugVerbose) DEBUG("meta event 0x%02x", displayNote.data[0]);
                            switch (displayNote.data[0]) {
                                case 0x00:  // sequence number: 
                                            // http://midi.teragonaudio.com/tech/midifile/seq.htm                                   
                                    if (2 == displayNote.data[1]) { // two bytes for # follow
                                        for (int i = 2; i < 4; i++) {
                                            if (!midi_check7bits(iter, "sequence #", midiTime)) {
                                                return false;
                                            }
                                            displayNote.data[i] = *iter++;
                                        }
                                    } else if (0 != displayNote.data[1]) {
                                        DEBUG("expected sequence number length of 0 or 2: %d",
                                                displayNote.data[1]);
                                        debug_out(iter);
                                        return false;
                                    }
                                break;
                                case 0x01: // text event
                                case 0x02: // copyright notice
                                case 0x03: // sequence/track name
                                case 0x04: // instument name
                                case 0x05: // lyric
                                case 0x06: // marker
                                case 0x07: // cue point
                                case 0x08: // reserved for text event
                                case 0x09: // reserved for text event
                                case 0x0A: // reserved for text event
                                case 0x0B: // reserved for text event
                                case 0x0C: // reserved for text event
                                case 0x0D: // reserved for text event
                                case 0x0E: // reserved for text event
                                case 0x0F: // reserved for text event
                                 { 
                                    displayNote.data[2] = iter - midi.begin();
                                    if (midi.end() - iter < displayNote.data[1]) {
                                        DEBUG("meta text length %d exceeds file:", 
                                                displayNote.data[1]);
                                    }
                                    std::advance(iter, displayNote.data[1]);
                                } break;
                                case 0x20: // channel prefix
                                    if (1 != displayNote.data[1]) {
                                        DEBUG("expected channel prefix length == 1 %d",
                                                displayNote.data[1]);
                                        debug_out(iter);
                                        return false;
                                    }
                                    if (!midi_check7bits(iter, "channel prefix", midiTime)) {
                                        return false;
                                    }
                                    displayNote.data[2] = *iter++;
                                break;
                            #if 01  // not in the formal midi spec?
                                case 0x21: // port prefix
                                    if (1 != displayNote.data[1]) {
                                        DEBUG("expected port prefix length == 1 %d",
                                                displayNote.data[1]);
                                        debug_out(iter);
                                        return false;
                                    }
                                    if (!midi_check7bits(iter, "port prefix", midiTime)) {
                                        return false;
                                    }
                                    displayNote.data[2] = *iter++;
                                break;
                            #endif
                                case midiEndOfTrack: // (required)
                                // keep track of the last track end, and write that one
                                // note that track end sets duration of all active notes later
                                    displayNote.type = TRACK_END;
                                    if (0 != displayNote.data[1]) {
                                        DEBUG("expected end of track length == 0 %d",
                                                displayNote.data[1]);
                                        debug_out(iter);
                                        return false;
                                    }
                                    if (displayNote.startTime > trackEnd.startTime) {
                                        trackEnd = displayNote;
                                    }
                                    trackEnded = true;
                                break;
                                case midiSetTempo:
                                    displayNote.type = MIDI_TEMPO;
                                    displayNote.duration = 0;
                                    if (3 != displayNote.data[1]) {
                                        DEBUG("expected set tempo length == 3 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    if (!midi_size24(iter, &displayNote.data[0])) {
                                        DEBUG("midi_size24");
                                        debug_out(iter);
                                        return false;
                                    }
                                    DEBUG("tempo %d", displayNote.data[0]);
                                break;
                                case 0x54: // SMPTE offset
                                    if (5 != displayNote.data[1]) {
                                        DEBUG("expected SMPTE offset length == 5 %d",
                                                displayNote.data[1]);
                                        debug_out(iter);
                                        return false;
                                    }
                                    displayNote.data[2] = iter - midi.begin();
                                    std::advance(iter, displayNote.data[1]);
                                break;
                                case midiTimeSignature:
                                    displayNote.type = TIME_SIGNATURE;
                                    displayNote.duration = 0;
                                    for (int i = 0; i < 4; ++i) {
                                        if (!midi_check7bits(iter, "time signature", midiTime)) {
                                            return false;
                                        }
                                        displayNote.data[i] = *iter++;
                                    }
                                    if (!displayNote.isValid()) {
                                        DEBUG("invalid %s 2", displayNote.debugString().c_str());
                                        debug_out(iter);
                                        return false;
                                    }
                                break;
                                case midiKeySignature:
                                    displayNote.type = KEY_SIGNATURE;
                                    displayNote.duration = 0;
                                    displayNote.data[0] = (signed char) *iter++;
                                    displayNote.data[1] = *iter++;
                                    if (!displayNote.isValid()) {
                                        DEBUG("invalid %s 3", displayNote.debugString().c_str());
                                        debug_out(iter);
                                        return false;
                                    }
                                break;
                                case 0x7F: // sequencer specific meta event
                                    displayNote.data[2] = iter - midi.begin();
                                    if (midi.end() - iter < displayNote.data[1]) {
                                        DEBUG("meta text length %d exceeds file:", 
                                                displayNote.data[1]);
                                    }
                                    std::advance(iter, displayNote.data[1]);
                                break;
                                default:
                                    DEBUG("unexpected meta: 0x%02x", displayNote.data[0]);
                                    std::advance(iter, displayNote.data[1]);
                            }

                        break;
                        default:    
                            DEBUG("unexpected real time message 0x%02x", 0xF0 | lowNibble);
                            debug_out(iter);
                            return false;
                    }
                break;
                default:
                    DEBUG("unexpected byte %d", *iter);
                    debug_out(iter);
                    return false;
            }
            trackLength -= iter - messageStart;
            // hijack 0x00 0xBx 0x57-0x5A 0xXX to set sustain and release parameters
            // only intercepts if it occurs at time zero, and 0xXX value is in range
            if (CONTROL_CHANGE == displayNote.type && 0 == displayNote.startTime &&
                    midiReleaseMax <= displayNote.data[0] && displayNote.data[0] <= midiSustainMax &&
                    (unsigned) displayNote.data[1] < NoteDurations::Count()) {
                channels[lowNibble].setLimit(
                        (NoteTakerChannel::Limit) (displayNote.data[0] - midiReleaseMax),
                        NoteDurations::ToMidi(displayNote.data[1], ppq));
                continue;
            }
            // to do : support tracking midi system, control change, etc. in display notes
            if (KEY_PRESSURE <= displayNote.type && displayNote.type <= MIDI_SYSTEM) {
                continue;
            }
            if ((!midiFormat || !trackCount || NOTE_ON == displayNote.type)
                    && TRACK_END != displayNote.type) {
                DEBUG("push %s", displayNote.debugString().c_str());
                parsedNotes.push_back(displayNote);
            }
        }
        // if there are missing note off, set note on duration to current time
        int lastTime = midiTime;
        for (auto ri = parsedNotes.rbegin(); ri != parsedNotes.rend(); ++ri) {
            if (ri->startTime > lastTime) {
                break;
            }
            lastTime = ri->startTime;
            if (0 <= ri->duration) {
                continue;
            }
            if (ri->type != NOTE_ON) {
                ri->duration = 0;
                continue;
            }
            ri->duration = midiTime - ri->startTime;
            DEBUG("missing note off for %s", ri->debugString().c_str());
        }
        // track channels used
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            if (!last[index]) {
                continue;
            }
            channelUsed[index] = true;
        }
    } while (iter != midi.end());
    NoteTaker::Sort(parsedNotes);
    if (trackEnd.startTime < 0) {
        trackEnd.startTime = midiTime;
    }
    if (trackEnd.duration < 0) {
        trackEnd.duration = 0;
    }
    parsedNotes.push_back(trackEnd);
    // insert rests
    int lastNoteEnd = 0;
    vector<DisplayNote> withRests;
    withRests.reserve(parsedNotes.size());
    /* track to channel assignment strategy
       see if track has any notes
       if so, and if channel 10 or 11, assume it is percussion and leave there
       otherwise, assign to the next unused channel, and reuse channels without notes
     */
    uint8_t reassign[CHANNEL_COUNT];
    memset(reassign, 0xFF, sizeof(reassign));
    for (auto& note : parsedNotes) {
        unsigned chan = note.channel;
        if (NOTE_ON == note.type && chan >= CV_OUTPUTS && 10 != chan && 11 != chan) {
            // map to unused channel, if any
            if (0xFF == reassign[chan]) {
                reassign[chan] = 0xFE;      // assume we won't find a free channel
                for (unsigned index = 0; index < CV_OUTPUTS; ++index) {
                    if (!channelUsed[index]) {
                        channelUsed[index] = true;
                        reassign[chan] = index;
                        break;
                    }
                }
            }
            if (reassign[chan] < CV_OUTPUTS) {
                note.channel = reassign[chan];
            }
        }
        if (NOTE_ON == note.type || TRACK_END == note.type) {
            int duration = note.startTime - lastNoteEnd;
            if (duration > 0 && NoteDurations::InStd(duration, ppq)) {
                withRests.emplace_back(REST_TYPE, lastNoteEnd, duration, note.channel);
            }
        }
        withRests.push_back(note);
        lastNoteEnd = std::max(lastNoteEnd, note.endTime());
    }
    int lastTime = -1;
    for (const auto& note : withRests) {
        // to do : shouldn't allow zero either, let it slide for now to debug 9.mid
        if (NOTE_ON == note.type && 0 >= note.duration) {
            DEBUG("non-positive note on duration %s", note.debugString().c_str());
            if (0) NoteTaker::DebugDump(withRests);  // to do : abbreviate output?
            return false;
        }
        if (note.startTime < lastTime) {
            DEBUG("notes out of time order %d lastTime %d note %s",
                    note.startTime, lastTime, note.debugString().c_str());
            if (0) NoteTaker::DebugDump(withRests); // to do : abbreviate output?
            return false;
        }
        lastTime = note.startTime;
    }
    if (debugVerbose) NoteTaker::DebugDump(withRests);
    displayNotes->swap(withRests);
    if (ntPpq) {
        *ntPpq = ppq;
    }
    if (debugVerbose && ntPpq) DEBUG("ntPpq %d", *ntPpq);
    return true;
}

#if 0
int NoteTakerParseMidi::safeMidi_size8(vector<uint8_t>::const_iterator& limit,
        vector<uint8_t>::const_iterator& iter, int ppq) {
    int value;
    // if value is not well-formed, add a zero byte
    if (!Midi_Size8(limit, iter, &value) && iter[-1] & 0x80) {
        value <<= 7;
        value = NoteDurations::LtOrEq(value, ppq);
    }
    return value;
}
#endif
