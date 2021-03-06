#include "ParseMidi.hpp"
#include "Display.hpp"
#include "MakeMidi.hpp"
#include "Taker.hpp"

// to do : not sure what the rules are if note on follows note on without note off... 
// to do : only worry about GM 1 until GM 2 test appears..
// to do : GM 2 suggests that chan 10 and 11 can be used for percussion or not; how to tell?
#define DEBUG_NOTE_OFF 0

struct TrackUsage {
    array<bool, CHANNEL_COUNT> noteCount;
    std::string sequenceName;
    std::string instrumentName;

    TrackUsage() {
        noteCount.fill(false);
    }
};

static void AddTrackName(std::string* dest, std::string add) {
    if (add.empty()) {
        return;
    }
    if (!dest->empty()) {
        *dest += "/";
    }
    *dest += add;
}

bool NoteTakerParseMidi::parseMidi() {
#if DEBUG_PARSE
    if (debugVerbose) DEBUG("parseMidi start");
#endif
    array<NoteTakerChannel, CHANNEL_COUNT> parsedChannels;
    array<uint8_t, CHANNEL_COUNT> reassign;
    for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
        reassign[index] = index;
    }
    vector<DisplayNote> parsedNotes;
    vector<TrackUsage> parsedTracks;
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
        parsedTracks.emplace_back();
        auto curTrack = &parsedTracks.back();
        midiTime = 0;
#if DEBUG_PARSE
        if (debugVerbose) DEBUG("trackLength %d", trackLength);
#endif
        unsigned lowNibble;
        unsigned runningStatus = -1;
        unsigned defaultChannel = 0;
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
            lowNibble = runningStatus & 0x0F;
            displayNote.type = (DisplayType) ((runningStatus >> 4) & 0x7);
            displayNote.channel = 
                    reassign[MIDI_SYSTEM == displayNote.type ? defaultChannel : lowNibble];
            if (NOTE_ON == displayNote.type || NOTE_OFF == displayNote.type) {
                if (!midi_check7bits(iter, "pitch", midiTime)) {
                    return false;
                }
                displayNote.setPitchData(*iter++);
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
                                    std::max(NoteDurations::Smallest(ppq),
                                    midiTime - noteOn->startTime);
                            noteOn->setOffVelocity(displayNote.onVelocity());
                            if (DEBUG_NOTE_OFF) DEBUG("assign vel %s", noteOn->debugString().c_str());
                        } else {
                            if (DEBUG_NOTE_OFF) DEBUG("unexpected note off old %s new %s",
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
                            lastOnChannel->setSlurStart(true);
                            noteOn->setSlurEnd(true);
#if DEBUG_SLUR
                            if (debugVerbose) {
                                DEBUG("set slur start %d %s ; set slur end %d %s",
                                        lastOnChannel - &parsedNotes.front(),
                                        lastOnChannel->debugString().c_str(),
                                        noteOn - &parsedNotes.front(),
                                        noteOn->debugString().c_str());
                            }
#endif
                        } else if (lastOnChannel->startTime == noteOn->startTime
                                && lastOnChannel->slurEnd()) {
                            noteOn->setSlurEnd(true);  // set slur end for all notes in chord
#if DEBUG_SLUR
                            if (debugVerbose) {
                                DEBUG("set slur end %d %s",
                                        noteOn - &parsedNotes.front(),
                                        noteOn->debugString().c_str());
                            }
#endif
                        }
                    }
                    trackLength -= iter - messageStart;
                }
                break;
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
                            if (DEBUG_NOTE_OFF) DEBUG("missing note off old %s new %s",
                                  noteOn->debugString().c_str(), displayNote.debugString().c_str());
                            noteOn->duration = midiTime - noteOn->startTime;
                        }
                    }
                    pitched[displayNote.channel][displayNote.pitch()] = parsedNotes.size();
                    last[displayNote.channel] = parsedNotes.size();
                    curTrack->noteCount[displayNote.channel] = true; 
                }
                break;
                case KEY_PRESSURE:
                    for (int i = 0; i < 2; i++) {
                        if (!midi_check7bits(iter, "key pressure", midiTime)) {
                            return false;
                        }
                        displayNote.data[i] = *iter++;
                    }
                    if (debugVerbose) DEBUG("key pressure [chan %d] %d %d", displayNote.channel,
                            displayNote.data[0], displayNote.data[1]);
                break;
                case CONTROL_CHANGE:
                    for (int i = 0; i < 2; i++) {
                        if (!midi_check7bits(iter, "control change", midiTime)) {
                            return false;
                        }
                        displayNote.data[i] = *iter++;
                    }
                    if (2 != displayNote.data[0] && debugVerbose) {
                            // to do : 2 is 'breath control' -- see it a lot, don't know what it does
                            DEBUG("control change [chan %d] %d %d", displayNote.channel,
                                displayNote.data[0], displayNote.data[1]);
                    }
                break;
                case PITCH_WHEEL:
                    for (int i = 0; i < 2; i++) {
                        if (!midi_check7bits(iter, "pitch wheel", midiTime)) {
                            return false;
                        }
                        displayNote.data[i] = *iter++;
                    }
                    if (debugVerbose) DEBUG("pitch wheel [chan %d] %d %d", displayNote.channel,
                            displayNote.data[0], displayNote.data[1]);
                break;
                case PROGRAM_CHANGE:
                    if (!midi_check7bits(iter, "program change", midiTime)) {
                        return false;
                    }
                    displayNote.data[0] = *iter++;
                    if (debugVerbose) DEBUG("program change [chan %d] %s", displayNote.channel,
                            NoteTakerDisplay::GMInstrumentName(displayNote.data[0]));
                    parsedChannels[displayNote.channel].gmInstrument = displayNote.data[0];
                    // if same instrument on multiple channels, and pref is set, remap to same chan
                    if (groupByGMInstrument) {
                        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
                            if (parsedChannels[index].gmInstrument == displayNote.data[0]) {
                                reassign[displayNote.channel] = index;
                                break;
                            }
                        }
                    }
                break;
                case CHANNEL_PRESSURE:
                    if (!midi_check7bits(iter, "channel pressure", midiTime)) {
                        return false;
                    }
                    displayNote.data[0] = *iter++;
#if DEBUG_PARSE
                    if (debugVerbose) DEBUG("channel pressure [chan %d] %d", displayNote.channel,
                            displayNote.data[0]);
#endif
                break;
                case MIDI_SYSTEM:
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
#if DEBUG_PARSE
                            if (debugVerbose) DEBUG("meta event 0x%02x", displayNote.data[0]);
#endif
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
                                    std::string text((char*) &midi.front() + displayNote.data[2],
                                            displayNote.data[1]);
                                    if (0x03 == displayNote.data[0]) {
                                        curTrack->sequenceName = text;
                                    } else if (0x04 == displayNote.data[0]) {
                                        curTrack->instrumentName = text;
                                    } 
                                    if (debugVerbose) {
                                        static const char* textType[] = { "text event",
                                                "copyright notice", "sequence/track name",
                                                "instrument name", "lyric", "marker",
                                                "cue point"};
                            DEBUG("track %d channel %u %s: %s", trackCount, displayNote.channel,
                                    1 <= displayNote.data[0] && displayNote.data[0] <= 7 ?
                                    textType[displayNote.data[0] - 1] : "(unknown)", text.c_str());
                                    }
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
                                    defaultChannel = displayNote.data[2];
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
                                    displayNote.data[0] = 7 + (signed char) *iter++;
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
                parsedChannels[lowNibble].setLimit(
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
                if (DEBUG_NOTE_OFF) DEBUG("push %s", displayNote.debugString().c_str());
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
    } while (iter != midi.end());
    array<bool, CHANNEL_COUNT> usedChannels;
    usedChannels.fill(false);
    usedChannels[9] = true;  // don't assign non-drum channels to drums
    for (auto& track : parsedTracks) {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            usedChannels[index] |= track.noteCount[index];
        }
    }
    for (auto& track : parsedTracks) {
        if (track.sequenceName.empty() && track.instrumentName.empty()) {
            continue;
        }
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            if (!track.noteCount[index]) {
                continue;
            }
            if (debugVerbose) DEBUG("%s track:%u chan:%u seq:\"%s\" inst:\"%s\"",
                    __func__, &track - &parsedTracks.front(), index, 
                    track.sequenceName.c_str(), track.instrumentName.c_str());
            AddTrackName(&parsedChannels[index].sequenceName, track.sequenceName);
            AddTrackName(&parsedChannels[index].instrumentName, track.instrumentName);
        }
    }
#if DEBUG_PARSE
    if (debugVerbose) {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            int instrument = parsedChannels[index].gmInstrument;
            if (instrument < 0) {
                continue;
            }
            const TrackUsage* track = nullptr;
            for (auto& test : parsedTracks) {
                if (test.noteCount[index]) {
                    track = &test;
                    break;
                }
            }
            if (track) {
                std::string seq = track->sequenceName.empty() ? "" :
                        "seq: " + track->sequenceName + " ";
                std::string inst = track->instrumentName.empty() ? "" :
                        "inst: " + track->instrumentName + " ";
                std::string seqInst = seq.empty() && inst.empty() ? "" : "( " + seq + inst + ") ";
                DEBUG("%s GM %d \"%s\" %s", __func__, index, 
                        NoteTakerDisplay::GMInstrumentName(instrument), seqInst.c_str());
            }
        }
    }
#endif
    // reassign channels if possible
    // to do : if channel is unused, shift all down
    reassign.fill(0);
    reassign[9] = 9;
    unsigned unused = 0;
    for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
        if (usedChannels[index] && 9 != index) {
            reassign[index] = unused;
#if DEBUG_PARSE
            if (debugVerbose) DEBUG("reassign %u to %u", index, unused);
#endif
            ++unused;
        }
    }
    for (unsigned index = 0; index < parsedNotes.size(); ++index) {
        auto& note = parsedNotes[index];
        note.channel = reassign[note.channel];
    }
    std::sort(parsedNotes.begin(), parsedNotes.end());
    if (trackEnd.startTime < 0) {
        trackEnd.startTime = midiTime;
    }
    if (trackEnd.duration < 0) {
        trackEnd.duration = 0;
    }
    parsedNotes.push_back(trackEnd);
    // insert rests
    vector<DisplayNote> withRests;
    withRests.reserve(parsedNotes.size());
#if DEBUG_PARSE
    array<int , CHANNEL_COUNT> counts;
    counts.fill(0);
#endif
    array<int , CHANNEL_COUNT> ends;
    ends.fill(0);
    for (auto& note : parsedNotes) {
        unsigned chan = note.channel;
#if DEBUG_PARSE
        if (NOTE_ON == note.type) {
            counts[chan]++;
        }
#endif
        if (NOTE_ON == note.type || TRACK_END == note.type) {
            int duration = note.startTime - ends[chan];
            if (duration > 0 && NoteDurations::InStd(duration, ppq)) {
                withRests.emplace_back(REST_TYPE, ends[chan], duration, chan);
            }
        }
        withRests.push_back(note);
        ends[chan] = std::max(ends[chan], note.endTime());
    }
    if (withRests.size() > parsedNotes.size()) {
        std::sort(withRests.begin(), withRests.end());
    }
#if DEBUG_PARSE
    if (debugVerbose) {
        for (unsigned index = 0; index < CHANNEL_COUNT; ++index) {
            if (counts[index]) {
                DEBUG("[%u] note count %d", index, counts[index]);
            }
        }
    }
#endif
    int lastTime = -1;
    for (const auto& note : withRests) {
        // to do : shouldn't allow zero either, let it slide for now to debug 9.mid
        if (NOTE_ON == note.type && 0 >= note.duration) {
            DEBUG("non-positive note on duration %s", note.debugString().c_str());
            if (0) Notes::DebugDump(withRests);  // to do : abbreviate output?
            return false;
        }
        if (note.startTime < lastTime) {
            DEBUG("notes out of time order %d lastTime %d note %s",
                    note.startTime, lastTime, note.debugString().c_str());
            if (0) Notes::DebugDump(withRests); // to do : abbreviate output?
            return false;
        }
        lastTime = note.startTime;
    }
#if DEBUG_PARSE
    if (debugVerbose) Notes::DebugDump(withRests);
#endif
    displayNotes->swap(withRests);
    if (ntPpq) {
        *ntPpq = ppq;
    }
#if DEBUG_PARSE
    if (debugVerbose && ntPpq) DEBUG("ntPpq %d", *ntPpq);
#endif
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
