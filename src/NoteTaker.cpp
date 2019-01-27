#include "SchmickleWorks.hpp"

using std::vector;

const vector<uint8_t> MThd = {0x4D, 0x54, 0x68, 0x64}; // MIDI file header
const vector<uint8_t> MThd_length = {0x00, 0x00, 0x00, 0x06}; // number of bytes of data to follow
const vector<uint8_t> MThd_data = {0x00, 0x00,  // format 0
                                   0x00, 0x01,  // one track
                                   0x00, 0x60}; /// 96 per quarter-note
const vector<uint8_t> MTrk = {0x4D, 0x54, 0x72, 0x6B}; // MIDI track header
const vector<uint8_t> midiDefaultTimeSignature = {0x00, // delta time
                                   0xFF, 0x58, // time signature
                                   0x04, // number of bytes of data to follow
                                   0x04, // four beats per measure
                                   0x02, // beat per quarter note
                                   0x18, // clocks per quarter note
                                   0x08}; // number of 32nd notes in a quarter note
const vector<uint8_t> midiDefaultTempo = {0x00, // delta time
                                   0xFF, 0x51, // tempo
                                   0x03, // number of bytes of data to follow
                                   0x07, 0xA1, 0x20}; // 500,000 usec/quarter note (120 beats/min)
const vector<uint8_t> MTrk_end = {0xFF, 0x2F, 00}; // end of track

const uint8_t midiCVMask = 0xF0;
const uint8_t midiNoteOff = 0x80;
const uint8_t midiNoteOn = 0x90;
const uint8_t midiKeyPressure = 0xA0;
const uint8_t midiControl = 0xB0;
const uint8_t midiPatch = 0xC0;
const uint8_t midiOverallPressure = 0xD0;
const uint8_t midiPitchWheel = 0xE0;
const uint8_t midiSystem = 0xF0;

const uint8_t channel1 = 0;
const uint8_t channel2 = 1;
const uint8_t channel3 = 2;
const uint8_t channel4 = 3;

const uint8_t stdKeyPressure = 0x64;
const int stdTimePerQuarterNote = 0x60;

enum DisplayType : uint8_t {
    NOTE_TYPE,
    REST_TYPE,
    HEADER,
    KEY_SIGNATURE,
    TIME_SIGNATURE,
    TEMPO,
    META,               // not implemented yet; placeholder for data bigger than DisplayNote
    NUM_TYPES
};

struct DisplayNote {
    int startTime;
    int duration;
    DisplayType type;
    int data[4];

    int pitch() const {
        assertValid(NOTE_TYPE);
        return data[0];
    }

    int format() const {
        assertValid(HEADER);
        return data[0];
    }

    int key() const {
        assertValid(KEY_SIGNATURE);
        return data[0];
    }

    int numerator() const {
        assertValid(TIME_SIGNATURE);
        return data[0];
    }

    int channel() const {
        assertValid(NOTE_TYPE == type ? NOTE_TYPE : REST_TYPE);
        return data[1];
    }

    int denominator() const {
        assertValid(TIME_SIGNATURE);
        return data[1];
    }

    int minor() const {
        assertValid(KEY_SIGNATURE);
        return data[1];
    }

    int onVelocity() const {
        assertValid(NOTE_TYPE);
        return data[2];
    }

    int clocksPerClick() const {
        assertValid(TIME_SIGNATURE);
        return data[2];
    }

    int notated32NotesPerQuarterNote() const {
        assertValid(TIME_SIGNATURE);
        return data[3];
    }

    int offVelocity() const {
        assertValid(NOTE_TYPE);
        return data[3];
    }

    void assertValid(DisplayType t) const {
        assert(type == t);
        return assert(isValid());
    }

    bool isValid() const {
        switch (type) {
            case NOTE_TYPE:
                if (0 > data[0] || data[0] > 127) {
                    printf("invalid note pitch %d\n", data[0]);
                    return false;
                }
                if (0 > data[1] || data[1] > 15) {
                    printf("invalid note channel %d\n", data[1]);
                    return false;
                }
                if (0 > data[2] || data[2] > 127) {
                    printf("invalid on note pressure %d\n", data[2]);
                    return false;
                }
                if (0 > data[3] || data[3] > 127) {
                    printf("invalid off note pressure %d\n", data[3]);
                    return false;
                }
            break;
            case REST_TYPE:
                if (0 > data[0] || data[0] > 127) {  // todo: should be more restrictive
                    printf("invalid rest pitch %d\n", data[0]);
                    return false;
                }
                if (0 > data[1] || data[1] > 15) {
                    printf("invalid rest channel %d\n", data[1]);
                    return false;
                }
            break;
            case HEADER:
                if (0 > data[0] || data[0] > 2) {
                    printf("invalid midi format %d\n", data[0]);
                    return false;
                }
                if (1 != data[1] && (!data[0] || 0 > data[1])) {
                    printf("invalid midi tracks %d (format=%d)\n", data[1], data[0]);
                    return false;
                }
                if (1 > data[2]) {
                    printf("invalid beats per quarter note %d\n", data[2]);
                    return false;
                }
            break;
            case KEY_SIGNATURE:
                if (-7 <= data[0] || data[0] <= 7) {
                    printf("invalid key %d\n", data[0]);
                    return false;
                }
                if (0 != data[1] && 1 != data[1]) {
                    printf("invalid minor %d\n", data[1]);
                    return false;
                }
            break;
            case TIME_SIGNATURE:
                // to do
                break;
            default:
                assert(0);
                return false;
        }
        return true;
    }

};

struct NoteTaker : Module {
	enum ParamIds {
		PITCH_PARAM,
        PITCH_SLIDER,
        DURATION_SLIDER,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
        PITCH_OUTPUT,
        GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};
    struct TimeNote {
        float time;  // 1 == quarter note
        int note;
    };

	float phase = 0.0;
	float blinkPhase = 0.0;

    vector<uint8_t> midi;
    vector<uint8_t>* target;
    vector<DisplayNote> displayNotes;
    unsigned displayFirst = 0;

	NoteTaker() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        createDefaultAsMidi();
        // start here
        // midi is awkward to parse at run time to draw notes since the duration is some
        // where in the future stream. It is not trival (or maybe not possible) to walk
        // backwards to find the note on given the position of the note off
        
        // better to have a structured array of notes that more closely resembles the
        // data we want to draw
        parseMidi();
    }

    void add_array(const vector<uint8_t>& array) {
        target->insert(target->end(), array.begin(), array.end());
    }

    void add_delta(float time, int* lastTime, int end) {
        int midiTime = (int) (time * stdTimePerQuarterNote) - end;
        int delta = midiTime - *lastTime;
        assert(delta >= 0);
        add_size8(delta);  // time of first note
        *lastTime = midiTime;
    }

    void add_one(unsigned char c) {
        target->push_back(c);
    }

    void add_size32(int size) {
        assert(size > 0);
        int shift = 24;
        do {
            target->push_back((size >> shift) & 0xFF);
        } while ((shift -= 8) >= 0);
    }

    void add_size8(int size) {
        assert(size >= 0);
        int64_t buffer = size & 0x7F;
        while ((size >>= 7)) {
            buffer <<= 8;
            buffer |= 0x80 | (size & 0xFF);
        }
        do {
           target->push_back(buffer & 0xFF);
        } while ((buffer >>= 8));
    }

        // TODO: save and load working data
        // to bootstrap, create a midi file
        // has the downside that if there are mistakes here, there will also
        // be mistakes in the midi parser
    void createDefaultAsMidi() {
        target = &midi;
        add_array(MThd);
        add_array(MThd_length);
        add_array(MThd_data);
        add_array(MTrk);
        // defer adding until size of data is known
        std::vector<uint8_t> temp;
        target = &temp;
        const TimeNote notes[] = { {0, 0}, {1, 4}, {2, 7}, {3, 11}, {4, -1} };
        const int c4 = 0x3C;
        int lastNote = -1;
        int lastTime = 0;
        for (auto n : notes) {
            if (lastNote >= 0) {
                add_delta(n.time, &lastTime, 1);
                add_one(midiNoteOff + channel1);
                add_one(c4 + lastNote);
                add_one(stdKeyPressure);
            }
            if (n.note >= 0) {
                add_delta(n.time, &lastTime, 0);
                add_one(midiNoteOn + channel1);
                add_one(c4 + n.note);
                add_one(stdKeyPressure);
            }
            lastNote = n.note;
        }
        add_size8(0);  // track end delta time
        add_array(MTrk_end);
        target = &midi;
        add_size32(temp.size());
        add_array(temp);
    }

    bool match_midi(vector<uint8_t>::iterator& iter, const vector<uint8_t>& data) {
        for (auto dataIter = data.begin(); dataIter != data.end(); ++dataIter) {
            if (*iter != *dataIter) {
                return false;
            }
            ++iter;
        }
        return true;
    }

    void parseMidi() {
        if (midi.size() < 14) {
            printf("MIDI file too small size=%llu\n", midi.size());
            return;
        }
        auto iter = midi.begin();
        if (!match_midi(iter, MThd)) {
            printf("expect MIDI header, got %c%c%c%c (0x%02x%02x%02x%02x)\n", 
                    midi[0], midi[1], midi[2], midi[3],
                    midi[0], midi[1], midi[2], midi[3]);
            return;
        }
        if (!match_midi(iter, MThd_data)) {
            printf("expect MIDI header size == 6, got (0x%02x%02x%02x%02x)\n", 
                    midi[4], midi[5], midi[6], midi[7]);
            return;
        }
        DisplayNote headerData;
        headerData.type = HEADER;
        headerData.startTime = 0;
        headerData.duration = 0;
        for (int i = 0; i < 3; ++i) {
            read_midi16(iter, &headerData.data[i]);
        }
        if (!headerData.isValid()) {
            return;
        }
        displayNotes.push_back(headerData);
        // start here
    }

    void read_midi16(vector<uint8_t>::iterator& iter, int* store) {
        *store = *iter << 8;
        *store |= *iter;
    }

	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void NoteTaker::step() {
	// Implement a simple sine oscillator
	float deltaTime = engineGetSampleTime();

	// Compute the frequency from the pitch parameter and input
	float pitch = params[PITCH_PARAM].value;
	pitch += inputs[PITCH_INPUT].value;

    bool gate = fmodf(blinkPhase, 0.25) > 0.125;
    outputs[GATE_OUTPUT].value = gate ? 5 : 0;

    // hack to reuse blinkPhase to change pitch
    if (blinkPhase < 0.25) {
        ;
    } else if (blinkPhase < 0.5) {
        pitch += 4/12.f;
    } else if (blinkPhase < 0.75) {
        pitch += 7/12.f;
    } else {
        pitch += 11/12.f;
    }
	pitch = clamp(pitch, -4.0f, 4.0f);
    outputs[PITCH_OUTPUT].value = pitch;

	// The default pitch is C4
	float freq = 261.626f * powf(2.0f, pitch);

	// Accumulate the phase
	phase += freq * deltaTime;
	if (phase >= 1.0f)
		phase -= 1.0f;

	// Compute the sine output
	float sine = sinf(2.0f * M_PI * phase);
	outputs[SINE_OUTPUT].value = 5.0f * sine;

	// Blink light at 1Hz
	blinkPhase += deltaTime;
	if (blinkPhase >= 1.0f)
		blinkPhase -= 1.0f;
	lights[BLINK_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
}

struct NoteTakerDisplay : TransparentWidget {

    void draw(NVGcontext *vg) override {
        // draw staff
	    nvgStrokeWidth(vg, 1.0);
    	nvgBeginPath(vg);
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
	    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
	    nvgStroke(vg);
    	nvgBeginPath(vg);
        for (int staff = 15; staff <= 50; staff += 35) {
            for (int y = staff; y <= staff + 20; y += 5) { 
	            nvgMoveTo(vg, 1, y);
	            nvgLineTo(vg, box.size.x - 1, y);
            }
        }
        nvgStrokeWidth(vg, 0.5);
	    nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
	    nvgStroke(vg);

        // draw notes
        for (unsigned i = module->displayFirst; i < module->displayNotes.size(); ++i) {
            const DisplayNote& note = module->displayNotes[i];
            switch (note.type) {
                case NOTE_TYPE:

                break;
                case REST_TYPE:

                break;
                case HEADER:
                break;
                case KEY_SIGNATURE:

                break;
                case TIME_SIGNATURE:

                break;
                case TEMPO:
                break;
                default:
                    assert(0); // incomplete
            }
        }
    }

    NoteTaker* module;
};

struct NoteTakerWheel : Knob, FramebufferWidget {

    // frame varies from 0 to 1 to rotate the wheel
    void drawGear(NVGcontext *vg, float frame) {
        nvgShapeAntiAlias(vg, 1);
        int topcolor = 0xdf;
        int bottom = size.y - 5;
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, size.x, bottom);
        nvgFillColor(vg, nvgRGB(topcolor, topcolor, topcolor));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, bottom / 2);
        nvgLineTo(vg, size.x / 2, bottom);
        nvgLineTo(vg, size.x, bottom / 2);
        nvgLineTo(vg, size.x, bottom);
        nvgLineTo(vg, 0, bottom);
        nvgFillColor(vg, nvgRGB(0xaf, 0xaf, 0xaf));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, size.x, bottom);
        nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        float frameWidth = 1;
        nvgStrokeWidth(vg, frameWidth);
	    nvgStroke(vg);
        nvgScissor(vg, frameWidth / 2, frameWidth / 2, size.x - frameWidth, size.y);
        const int segments = 40;
        const float depth = 2;  // tooth depth
        const float radius = size.x * 2 / 3;
        const float height = bottom / 2;
        const float slant = 3;
        float endTIx = 0, endTIy = 0, endTOx = 0, endTOy = 0, endBIy = 0, endBOy = 0;
        nvgTranslate(vg, size.x / 2, -radius / slant + 10);
        for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 3; ++i) {
            float angle = (i + frame * 2) * 2 * M_PI / segments;
            float ax = cosf(angle), ay = sinf(angle);
            float scaleI = radius - depth;
            float scaleO = radius + depth;
            float startTIx = endTIx, startTIy = endTIy;
            endTIx = ax * scaleI; endTIy = ay * scaleI / slant;
            float startTOx = endTOx, startTOy = endTOy;
            endTOx = ax * scaleO; endTOy = ay * scaleO / slant;
            float startBIy = endBIy;
            endBIy = endTIy + height;
            float startBOy = endBOy;
            endBOy = endTOy + height;
            if (!startTIx && !startTIy) {
                continue;
            }
            float offcenter = abs(segments / 4 - i - frame * 2) / (float) segments; // <= .25
            int facecolor = 0xF0 - (int) (0x17F * offcenter);
            if (i & 1) {
                // back face of inset tooth
                nvgBeginPath(vg);
                nvgMoveTo(vg, startTIx, startTIy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgLineTo(vg, endTIx, endBIy);
                nvgLineTo(vg, startTIx, startBIy);
                // closest to center is brightest
                nvgFillColor(vg, nvgRGB(facecolor, facecolor, facecolor));
	            nvgFill(vg);
            } else {
                // front face of outset tooth
                nvgBeginPath(vg);
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTOx, endBOy);
                nvgLineTo(vg, startTOx, startBOy);
                nvgFillColor(vg, nvgRGB(facecolor, facecolor, facecolor));
	            nvgFill(vg);
                int edgecolor = 0x80 + (int) (0x1FF * offcenter);
                if (startTIx > startTOx) {
                    // edge on right
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, startTIx, startTIy);
                    nvgLineTo(vg, startTIx, startBIy);
                    nvgLineTo(vg, startTOx, startBOy);
                    nvgLineTo(vg, startTOx, startTOy);
                    nvgFillColor(vg, nvgRGB(edgecolor, edgecolor, edgecolor));
	                nvgFill(vg);
                }
                if (endTIx < endTOx) {
                    // edge on left
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, endTIx, endTIy);
                    nvgLineTo(vg, endTIx, endBIy);
                    nvgLineTo(vg, endTOx, endBOy);
                    nvgLineTo(vg, endTOx, endTOy);
                    nvgFillColor(vg, nvgRGB(edgecolor, edgecolor, edgecolor));
	                nvgFill(vg);
                }
            }
        }
        endTIx = endTIy = 0;
        // accumulate top of outset tooth
        nvgBeginPath(vg);
        for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 3; ++i) {
            float angle = (i + frame * 2) * 2 * M_PI / segments;
            float ax = cosf(angle), ay = sinf(angle);
            float scaleI = radius - depth;
            float scaleO = radius + depth;
            float startTIx = endTIx, startTIy = endTIy;
            endTIx = ax * scaleI; endTIy = ay * scaleI / slant;
            float startTOx = endTOx, startTOy = endTOy;
            endTOx = ax * scaleO; endTOy = ay * scaleO / slant;
            if (i & 1) {
                continue;
            }
            nvgMoveTo(vg, startTIx, startTIy);
            nvgLineTo(vg, startTOx, startTOy);
            nvgLineTo(vg, endTOx, endTOy);
            nvgLineTo(vg, endTIx, endTIy);
        }
        nvgFillColor(vg, nvgRGB(topcolor, topcolor, topcolor));
	    nvgFill(vg);
        endTIx = endTIy = 0;
        // accumulate edge lines
        nvgBeginPath(vg);
        for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 3; ++i) {
            float angle = (i + frame * 2) * 2 * M_PI / segments;
            float ax = cosf(angle), ay = sinf(angle);
            float scaleI = radius - depth;
            float scaleO = radius + depth;
            float startTIx = endTIx, startTIy = endTIy;
            endTIx = ax * scaleI; endTIy = ay * scaleI / slant;
            float startTOx = endTOx, startTOy = endTOy;
            endTOx = ax * scaleO; endTOy = ay * scaleO / slant;
            float startBIy = endBIy;
            endBIy = endTIy + height;
            float startBOy = endBOy;
            endBOy = endTOy + height;
            if (!startTIx && !startTIy) {
                continue;
            }
            if (i & 1) {
                nvgMoveTo(vg, startTIx, startTIy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgLineTo(vg, endTOx, endTOy);
                float clipSx = startTIx, clipSy = startBIy;
                float clipEx = endTIx, clipEy = endBIy;
                float diff = clipSx - startTOx;
                float slopex = clipEx - clipSx, slopey = clipEy - clipSy;
                if (diff > 0) {
                    clipSx = startTOx;
                    clipSy -= slopey * diff / slopex;
                }
                diff = clipEx - endTOx;
                if (diff < 0) {
                   clipEx = endTOx;
                   clipEy -= slopey * diff / slopex;
                }
                nvgMoveTo(vg, clipSx, clipSy);
                nvgLineTo(vg, clipEx, clipEy);
            } else {
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTIx, endTIy);
                nvgMoveTo(vg, startTOx, startTOy);
                nvgLineTo(vg, startTOx, startBOy);
                nvgMoveTo(vg, endTOx, endTOy);
                nvgLineTo(vg, endTOx, endBOy);
                if (startTIx > startTOx) {
                    nvgMoveTo(vg, startTIx, startTIy);
                    nvgLineTo(vg, startTIx, startBIy);
                    nvgLineTo(vg, startTOx, startBOy);
                }
                if (endTIx < endTOx) {
                    nvgMoveTo(vg, endTIx, endTIy);
                    nvgLineTo(vg, endTIx, endBIy);
                    nvgLineTo(vg, endTOx, endBOy);
                }
                nvgMoveTo(vg, startTOx, startBOy);
                nvgLineTo(vg, endTOx, endBOy);
            }
        }
        nvgStrokeColor(vg, nvgRGB(0x7f, 0x7f, 0x7f));
        nvgStrokeWidth(vg, .5f);
	    nvgStroke(vg);
    }

    void draw(NVGcontext *vg) override {
        float v = module->params[NoteTaker::PITCH_PARAM].value + value;
        drawGear(vg, fmodf(v, 1));
    }

    void onMouseDown(EventMouseDown &e) override {
       e.target = this;
       e.consumed = true;
       printf("onMouseDown %s\n", name());
       fflush(stdout);
	   FramebufferWidget::onMouseDown(e);
    }

    void onMouseUp(EventMouseUp &e) override {
       e.target = this;
       e.consumed = true;
       printf("onMouseUp %s\n", name());
       fflush(stdout);
	   FramebufferWidget::onMouseUp(e);
    }

    void onMouseMove(EventMouseMove &e) override {
       e.target = this;
       e.consumed = true;
       printf("onMouseMove %s\n", name());
       fflush(stdout);
	   FramebufferWidget::onMouseMove(e);
    }

    void onScroll(EventScroll &e) override {
       e.consumed = true;
       printf("onScroll %s\n", name());
       fflush(stdout);
	   FramebufferWidget::onScroll(e);
    }

    void onDragMove(EventDragMove& e) override {
       printf("onDragMove %g,%g value=%g %s\n", e.mouseRel.x, e.mouseRel.y, value, name());
       if (isHorizontal) {
          std::swap(e.mouseRel.x, e.mouseRel.y);
       } else {
          e.mouseRel.y = -e.mouseRel.y;
       }
       Knob::onDragMove(e);
    }

    void step() override {
        if (dirty) {

        }
        FramebufferWidget::step();
    }

    void onChange(EventChange &e) override {
        dirty = true;
	    Knob::onChange(e);
    }

    virtual const char* name() = 0;
    Vec size;
    bool isHorizontal;
};

struct HorizontalWheel : NoteTakerWheel {

    HorizontalWheel() {
        size.x = box.size.x = 100;
        size.y = box.size.y = 17;
        speed = .1;
        isHorizontal = true;
    }

    const char* name() override {
        return "horizontal";
    }
};

struct VerticalWheel : NoteTakerWheel {

    VerticalWheel() {
        size.y = box.size.x = 17;
        size.x = box.size.y = 100;
        speed = .1;
        isHorizontal = false;
    }

    void draw(NVGcontext *vg) override {
        nvgTranslate(vg, 0, box.size.y);
        nvgRotate(vg, -M_PI / 2);
        NoteTakerWheel::draw(vg);
    }

    const char* name() override {
        return "vertical";
    }
};

struct NoteTakerWidget : ModuleWidget {
	NoteTakerWidget(NoteTaker *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/NoteTaker.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        NoteTakerDisplay *display = new NoteTakerDisplay();
        display->module = (NoteTaker*) module;
        display->box.pos = Vec(RACK_GRID_WIDTH, RACK_GRID_WIDTH * 2);
        display->box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_WIDTH * 9);
        addChild(display);
        ParamWidget* horizontalWheel = ParamWidget::create<HorizontalWheel>(
                Vec(RACK_GRID_WIDTH * 7 - 50, RACK_GRID_WIDTH * 11.5f),
                module, NoteTaker::DURATION_SLIDER, -100.0, 100.0, 0.0);
		addParam(horizontalWheel);
        ParamWidget* verticalWheel = ParamWidget::create<VerticalWheel>(
                Vec(RACK_GRID_WIDTH * 13.5f, RACK_GRID_WIDTH * 6.5f - 50),
                module, NoteTaker::PITCH_SLIDER, -100.0, 100.0, 0.0);
		addParam(verticalWheel);

#if 0
        addParam(ParamWidget::create<BefacoSlidePot>(Vec(display->box.pos.x + display->box.size.x / 2,
                display->box.pos.y + display->box.size.y + 10), module, NoteTaker::DURATION_SLIDER,
                -3.0, 3.0, 0.0));
#endif
		addInput(Port::create<PJ301MPort>(Vec(16, 226), Port::INPUT, module, NoteTaker::PITCH_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(50, 226), Port::OUTPUT, module, NoteTaker::PITCH_OUTPUT));

		addOutput(Port::create<PJ301MPort>(Vec(16, 315), Port::OUTPUT, module, NoteTaker::SINE_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(50, 315), Port::OUTPUT, module, NoteTaker::GATE_OUTPUT));

		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(141, 269), module, NoteTaker::BLINK_LIGHT));
		addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(128, 317), module, 
                NoteTaker::PITCH_PARAM, -3.0, 3.0, 0.0));
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelNoteTaker = Model::create<NoteTaker, NoteTakerWidget>("NoteTaker", "NoteTakerWidget", "Note Taker", OSCILLATOR_TAG);

// gear drawing : https://fiddle.skia.org/c/cd4e81fc3446c3437bb28de39f5df3a6
// width: 512 ; height : 256 ; animation checked, duration 3
#if 0
    SkPaint p;
    int segments = 40;
    float depth = 4;  // tooth depth
    float radius = 228;
    float height = 45;
    canvas->translate(radius + depth * 2, (radius + depth * 2) / 3);
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    SkPoint endTI = {0, 0}, endTO = endTI, endBI = endTI, endBO = endTI;
    for (int i = segments / 8 - 3; i <= 3 * segments / 8 + 2; ++i) {
        float angle = (i + frame * 2) * 2 * M_PI / segments;
        SkVector a = { cosf(angle), sinf(angle) };
        float scaleI = radius - depth;
        float scaleO = radius + depth;
        SkPoint startTI = endTI;
        endTI = {a.fX * scaleI, a.fY * scaleI / 3};
        SkPoint startTO = endTO;
        endTO = {a.fX * scaleO, a.fY * scaleO / 3};
        SkPoint startBI = endBI;
        endBI = {endTI.fX, endTI.fY + height};
        SkPoint startBO = endBO;
        endBO = {endTO.fX, endTO.fY + height};
        if (startTI.fX != 0 || startTI.fY != 0) {
            if (i & 1) {
                canvas->drawLine(startTI, endTI, p);
                canvas->drawLine(endTI, endTO, p);
                SkPoint clipS = startBI;
                SkPoint clipE = endBI;
                float diff = clipS.fX - startBO.fX;
                SkVector slope = clipE - clipS;
                if (diff > 0) {
                    clipS.fX = startBO.fX;
                    clipS.fY -= slope.fY * diff / slope.fX;
                }
                diff = clipE.fX - endBO.fX;
                if (diff < 0) {
                    clipE.fX = endBO.fX;
                    clipE.fY -= slope.fY * diff / slope.fX;
                }
                canvas->drawLine(clipS, clipE, p);
            } else {
                canvas->drawLine(startTO, endTO, p);
                canvas->drawLine(endTO, endTI, p);
                canvas->drawLine(startTO, startBO, p);
                canvas->drawLine(endTO, endBO, p);
                if (startTI.fX > startTO.fX) {
                    canvas->drawLine(startTI, startBI, p);
                    canvas->drawLine(startBI, startBO, p);
                }
                if (endTI.fX < endTO.fX) {
                    canvas->drawLine(endTI, endBI, p);
                    canvas->drawLine(endBI, endBO, p);
                }
                canvas->drawLine(startBO, endBO, p);
            }
        }
    }
#endif

