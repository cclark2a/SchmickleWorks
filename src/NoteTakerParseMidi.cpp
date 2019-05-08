#include "NoteTakerParseMidi.hpp"
#include "NoteTakerDisplay.hpp"
#include "NoteTakerMakeMidi.hpp"
#include "NoteTaker.hpp"

// At least in the case of Google's Bach Doodle, Midi may be missing some or all
// of the delta time. To work around this, scan forward after the delta time to 
// see if the next part is a note on or note off. Use the location of this to
// limit the number of bytes delta time can consume.
static vector<uint8_t>::const_iterator find_next_note(const vector<uint8_t>& midi,
        vector<uint8_t>::const_iterator& iter) {
    // look for note on or note off within 4 bytes
    vector<uint8_t>::const_iterator noteIter = midi.end();
    vector<uint8_t>::const_iterator limit = midi.begin()
            + std::min((int) midi.size() - 3, (int) (iter - midi.begin() + 4));
    for (auto testIter = iter; testIter < limit; ++testIter) {
        if ((testIter[0] & 0xE0) == 0x80  // tests for note on and note off
                && !(testIter[1] & 0x80) && !(testIter[2] & 0x80)) {
            noteIter = testIter;
            break;
        }
    }
    // there is not necessarily any note on / off info in the midi file
    if (noteIter + 2 >= midi.end()) {
        return midi.end();
    }
    if ((*noteIter & midiCVMask) == midiNoteOn) { // if note on, look for note off within 16 bytes
        limit = midi.begin() + std::min((int) midi.size() - 3, (int) (noteIter - midi.begin() + 16));
        for (auto offIter = noteIter + 3; offIter < limit; ++offIter) {
            if (offIter[0] == (noteIter[0] & 0xEF) && offIter[1] == noteIter[1]
                    && !(offIter[2] & 0x80)) {
                return noteIter;
            }
        }
    } else { // if note off, look for note on in previous 16 bytes
        assert((*noteIter & midiCVMask) == midiNoteOff);
        limit = midi.begin() + std::max(0, (int) (noteIter - midi.begin() - 16));
        for (auto onIter = noteIter - 3; onIter >= limit; --onIter) {
            if (onIter[0] == (noteIter[0] | 0x10) && onIter[1] == noteIter[1]
                    && !(onIter[2] & 0x80)) {
                return noteIter;
            }
        }
    }
    debug ("no note after delta 3");
    return midi.end();
}

bool NoteTakerParseMidi::parseMidi() {
    if (debugVerbose) debug("parseMidi start");
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
    int MThd_length = 0;
    if (!midi_size32(iter, &MThd_length) || 6 != MThd_length) {
        debug("expect MIDI header size == 6, got (0x%02x%02x%02x%02x)", 
                midi[4], midi[5], midi[6], midi[7]);
        return false;
    }
    DisplayNote displayNote;
    displayNote.cache = nullptr;
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
    int midiFormat = displayNote.format();
    int ppq = displayNote.ppq();
    int trackCount = -1;
    int midiTime;
    parsedNotes.push_back(displayNote);
    DisplayNote trackEnd = {nullptr, -1, 0, {0, 0, 0, 0}, 0xFF, TRACK_END, false};  // for missing end
    do {
        bool trackEnded = false;
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
        midiTime = 0;
        if (debugVerbose) debug("trackLength %d", trackLength);
        unsigned lowNibble;
        // find next note; note on followed by note off, within a short distance
        // don't allow delta time to include next note
        while (!trackEnded && trackLength) {
            const auto messageStart = iter;
            vector<uint8_t>::const_iterator limit = trackLength > 4 ? find_next_note(midi, iter) : midi.end();
            midiTime += this->safeMidi_size8(limit, iter, ppq);
            if (0 == (*iter & 0x80)) {
                debug("%d expected high bit set on channel voice message: %02x", midiTime, *iter);
                std::string s;
                auto start = std::max(&midi.front(), &*iter - 5);
                auto end = std::min(&midi.back(), &*iter + 5);
                const char hex[] = "0123456789ABCDEF";
                for (auto i = start; i <= end; ++i) {
                    if (i == &*iter) s += "[";
                    s += "0x";
                    s += hex[*i >> 4];
                    s += hex[*i & 0xf];
                    if (i == &*iter) s += "]";
                    s += ", ";
                }
                debug("%s", s.c_str());
                return false;
            }
            displayNote.cache = nullptr;
            displayNote.startTime = midiTime;
            displayNote.duration = -1;  // not known yet
            memset(displayNote.data, 0, sizeof(displayNote.data));
            displayNote.channel = *iter & 0x0F;
            lowNibble = displayNote.channel;
            if (!displayNote.channel && trackCount > 0) {
                displayNote.channel = trackCount;
            }
            displayNote.type = (DisplayType) ((*iter++ >> 4) & 0x7);
            displayNote.selected = false;
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
                    bool setSlur = false;
                    int maxMidiTime = midiTime;
                    for (auto ri = parsedNotes.rbegin(); ri != parsedNotes.rend(); ++ri) {
                        if (ri->type != NOTE_ON) {
                            continue;
                        }
                        if (displayNote.channel != ri->channel) {
                            continue;
                        }
                        int duration = maxMidiTime - ri->startTime;
                        if (pitch != ri->pitch()) {
                            setSlur |= duration > 0;
                            maxMidiTime = ri->startTime;
                            continue;
                        }
                        if (duration <= 0) {
                            continue;
                        }
                        if (ri->duration > 0) {
                            debug("unexpected note on %s", ri->debugString().c_str());
                            return false;
                        }
                        debug("%u note off %s", &*ri - &parsedNotes.front(), 
                                ri->debugString().c_str());
                        ri->duration = duration;
                        ri->setSlur(setSlur);
                        ri->setOffVelocity(velocity);
                        break;
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
                    displayNote.channel = 0xFF;
                    if (debugVerbose) debug("system message 0x%02x", lowNibble);
                    switch (lowNibble) {
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
                            if (debugVerbose) debug("meta event 0x%02x", displayNote.data[0]);
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
                                case midiEndOfTrack: // (required)
                                // keep track of the last track end, and write that one
                                // note that track end sets duration of all active notes later
                                    displayNote.type = TRACK_END;
                                    if (0 != displayNote.data[1]) {
                                        debug("expected end of track length == 0 %d",
                                                displayNote.data[1]);
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
                                        debug("expected set tempo length == 3 %d",
                                                displayNote.data[1]);
                                        return false;
                                    }
                                    if (!midi_size24(iter, &displayNote.data[0])) {
                                        debug("midi_size24");
                                        return false;
                                    }
                                    debug("tempo %d", displayNote.data[0]);
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
                                case midiTimeSignature:
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
                                case midiKeySignature:
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
                            debug("unexpected real time message 0x%02x", 0xF0 | lowNibble);
                            return false;
                    }
                break;
                default:
                    debug("unexpected byte %d", *iter);
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
            if ((!midiFormat || !trackCount || NOTE_ON == displayNote.type)
                    && TRACK_END != displayNote.type) {
                debug("push %s", displayNote.debugString().c_str());
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
            if (ri->type != NOTE_ON) {
                continue;
            }
            if (0 > ri->duration) {
                ri->duration = midiTime - ri->startTime;
                debug("missing note off for %d", ri->debugString().c_str());
            }
        }
    } while (iter != midi.end());
    for (auto it = parsedNotes.begin(), end = parsedNotes.end(); it != end; ++it) {
        auto const insertion_point = std::upper_bound(parsedNotes.begin(), it, *it);
        std::rotate(insertion_point, it, it + 1);
    }
    if (trackEnd.startTime < 0) {
        trackEnd.startTime = midiTime;
    }
    parsedNotes.push_back(trackEnd);
    // insert rests
    int lastNoteEnd = 0;
    vector<DisplayNote> withRests;
    withRests.reserve(parsedNotes.size());
    for (const auto& note : parsedNotes) {
        if (NOTE_ON == note.type && lastNoteEnd < note.startTime) {
            // to do : create constructor so this can call emplace_back ?
            DisplayNote rest;
            rest.startTime = lastNoteEnd;
            rest.duration = note.startTime - lastNoteEnd;
            memset(rest.data, 0, sizeof(rest.data));
            rest.channel = 0xFF;
            rest.type = REST_TYPE;
            rest.selected = false;
            withRests.push_back(rest);
        }
        withRests.push_back(note);
        lastNoteEnd = std::max(lastNoteEnd, note.endTime());
    }
    int lastTime = -1;
    for (const auto& note : withRests) {
        // to do : shouldn't allow zero either, let it slide for now to debug 9.mid
        if (NOTE_ON == note.type && 0 >= note.duration) {
            debug("non-positive note on duration");
            NoteTaker::DebugDump(withRests);
            return false;
        }
        if (note.startTime < lastTime) {
            debug("notes out of time order");
            NoteTaker::DebugDump(withRests);
            return false;
        }
        lastTime = note.startTime;
    }
    if (debugVerbose) NoteTaker::DebugDump(withRests);
    displayNotes.swap(withRests);
    ntPpq = ppq;
    if (debugVerbose) debug("ntPpq %d", ntPpq);
    return true;
}

int NoteTakerParseMidi::safeMidi_size8(vector<uint8_t>::const_iterator& limit,
        vector<uint8_t>::const_iterator& iter, int ppq) {
    int value;
    // if value is not well-formed, add a zero byte
    if (!Midi_Size8(limit, iter, &value) && iter[-1] & 0x80) {
        value <<= 7;
        value = NoteDurations::Closest(value, ppq);
    }
    return value;
}
