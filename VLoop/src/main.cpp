#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <new>

struct MidiEvent {
    uint16_t clockPulse;
    uint8_t data[3];
    
    MidiEvent() : clockPulse(0) {
        data[0] = data[1] = data[2] = 0;
    }
};

#define MAX_EVENTS 2048
#define MAX_LOOP_LENGTH 512

struct VLoop : public _NT_algorithm {
    MidiEvent events[MAX_EVENTS];
    uint16_t eventCount;
    uint16_t currentPulse;
    uint16_t loopLength;
    uint16_t playbackIndex;
    bool isRecording;
    bool isPlaying;
    float lastClockCV;
    bool lastRecord;
    bool lastPlay;
    bool lastClear;
    
    VLoop() {
        eventCount = 0;
        currentPulse = 0;
        loopLength = 0;
        playbackIndex = 0;
        isRecording = false;
        isPlaying = false;
        lastClockCV = 0.0f;
        lastRecord = false;
        lastPlay = false;
        lastClear = false;
    }
};

enum {
    kParamClockInput = 0,
    kParamRecord,
    kParamPlay,
    kParamClear,
    kNumParams
};

static const _NT_parameter parameters[] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Play", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone }
};

static const uint8_t page1[] = { kParamClockInput, kParamRecord, kParamPlay, kParamClear };
static const _NT_parameterPage pages[] = {
    { .name = "VLoop", .numParams = ARRAY_SIZE(page1), .params = page1 }
};
static const _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages
};

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParams;
    req.sram = sizeof(VLoop);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VLoop* loop = new (ptrs.sram) VLoop();
    loop->parameters = parameters;
    loop->parameterPages = &parameterPages;
    return loop;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoop* loop = (VLoop*)self;
    
    // Get parameter values
    int clockBusIndex = (int)loop->v[kParamClockInput] - 1;
    bool recordActive = loop->v[kParamRecord] > 0.5f;
    bool playActive = loop->v[kParamPlay] > 0.5f;
    bool clearActive = loop->v[kParamClear] > 0.5f;
    
    // Handle Clear knob
    if (clearActive && !loop->lastClear) {
        // Send All Notes Off on all channels
        for (uint8_t ch = 0; ch < 16; ch++) {
            NT_sendMidi3ByteMessage(~0, 0xB0 | ch, 123, 0); // All Notes Off
        }
        loop->eventCount = 0;
        loop->loopLength = 0;
        loop->currentPulse = 0;
        loop->playbackIndex = 0;
        loop->isRecording = false;
        loop->isPlaying = false;
    }
    loop->lastClear = clearActive;
    
    // Handle Record knob changes
    if (recordActive && !loop->lastRecord) {
        // Knob turned up - enter record mode
        loop->isRecording = true;
        loop->currentPulse = 0;
        loop->eventCount = 0;
        loop->loopLength = 0;
    } else if (!recordActive && loop->lastRecord && loop->isRecording) {
        // Knob turned down - stop recording, save loop length
        loop->isRecording = false;
        loop->loopLength = loop->currentPulse;
    }
    loop->lastRecord = recordActive;
    
    // Handle Play knob changes
    if (playActive && !loop->lastPlay) {
        // Knob turned up - start playback
        if (loop->loopLength > 0) {
            loop->isPlaying = true;
            loop->currentPulse = 0;
            loop->playbackIndex = 0;
        }
    } else if (!playActive && loop->lastPlay) {
        // Knob turned down - stop playback and send All Notes Off
        loop->isPlaying = false;
        for (uint8_t ch = 0; ch < 16; ch++) {
            NT_sendMidi3ByteMessage(~0, 0xB0 | ch, 123, 0); // All Notes Off
        }
    }
    loop->lastPlay = playActive;
    
    // Process all frames looking for clock edge
    const int totalFrames = numFramesBy4 * 4;
    for (int frame = 0; frame < totalFrames; frame++) {
        // Read clock CV from selected bus
        float clockCV = busFrames[clockBusIndex * totalFrames + frame];
        
        // Detect rising edge (crosses 2.5V threshold)
        bool clockEdge = (clockCV > 2.5f && loop->lastClockCV <= 2.5f);
        loop->lastClockCV = clockCV;
        
        if (clockEdge) {
            if (loop->isRecording) {
                // Increment pulse counter BEFORE recording more events
                // This ensures events recorded before next clock get the next pulse number
                loop->currentPulse++;
                if (loop->currentPulse >= MAX_LOOP_LENGTH) {
                    loop->currentPulse = MAX_LOOP_LENGTH - 1;
                }
            } else if (loop->isPlaying && loop->loopLength > 0) {
                // Playback mode - send events at NEXT pulse (currentPulse + 1)
                // Limit to 8 events per clock edge to prevent overflow
                uint16_t nextPulse = loop->currentPulse + 1;
                if (nextPulse >= loop->loopLength) {
                    nextPulse = 0;
                }
                
                // Send up to 8 events at nextPulse
                uint16_t eventsSent = 0;
                while (loop->playbackIndex < loop->eventCount && eventsSent < 8) {
                    uint16_t eventPulse = loop->events[loop->playbackIndex].clockPulse;
                    
                    // Safety check: ensure event pulse is within loop bounds
                    if (eventPulse >= loop->loopLength) {
                        loop->playbackIndex++;
                        continue;
                    }
                    
                    if (eventPulse == nextPulse) {
                        // Send this MIDI event
                        NT_sendMidi3ByteMessage(
                            ~0,
                            loop->events[loop->playbackIndex].data[0],
                            loop->events[loop->playbackIndex].data[1],
                            loop->events[loop->playbackIndex].data[2]
                        );
                        loop->playbackIndex++;
                        eventsSent++;
                    } else if (eventPulse > nextPulse) {
                        // Events should be sorted, so we can stop
                        break;
                    } else {
                        // Event in the past - skip it
                        loop->playbackIndex++;
                    }
                }
                
                // Increment pulse and wrap around
                loop->currentPulse++;
                if (loop->currentPulse >= loop->loopLength) {
                    loop->currentPulse = 0;
                    loop->playbackIndex = 0;
                }
            }
        }
    }
}

void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoop* loop = (VLoop*)self;
    
    // Only record when in recording mode
    if (!loop->isRecording) return;
    
    // Don't record system messages
    if ((byte0 & 0xF0) == 0xF0) return;
    
    // Check if we have space
    if (loop->eventCount >= MAX_EVENTS) return;
    
    // Store event at current clock pulse
    loop->events[loop->eventCount].clockPulse = loop->currentPulse;
    loop->events[loop->eventCount].data[0] = byte0;
    loop->events[loop->eventCount].data[1] = byte1;
    loop->events[loop->eventCount].data[2] = byte2;
    loop->eventCount++;
}

void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VLoop* loop = (VLoop*)self;
    
    // Write debug info
    stream.addMemberName("eventCount");
    stream.addNumber((int)loop->eventCount);
    
    stream.addMemberName("loopLength");
    stream.addNumber((int)loop->loopLength);
    
    stream.addMemberName("currentPulse");
    stream.addNumber((int)loop->currentPulse);
    
    stream.addMemberName("playbackIndex");
    stream.addNumber((int)loop->playbackIndex);
    
    stream.addMemberName("isRecording");
    stream.addBoolean(loop->isRecording);
    
    stream.addMemberName("isPlaying");
    stream.addBoolean(loop->isPlaying);
    
    // Write all events
    stream.addMemberName("events");
    stream.openArray();
    for (uint16_t i = 0; i < loop->eventCount; i++) {
        stream.openObject();
        stream.addMemberName("pulse");
        stream.addNumber((int)loop->events[i].clockPulse);
        stream.addMemberName("status");
        stream.addNumber((int)loop->events[i].data[0]);
        stream.addMemberName("data1");
        stream.addNumber((int)loop->events[i].data[1]);
        stream.addMemberName("data2");
        stream.addNumber((int)loop->events[i].data[2]);
        stream.closeObject();
    }
    stream.closeArray();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VLoop* loop = (VLoop*)self;
    
    // Reset state when loading preset
    loop->eventCount = 0;
    loop->loopLength = 0;
    loop->currentPulse = 0;
    loop->playbackIndex = 0;
    loop->isRecording = false;
    loop->isPlaying = false;
    
    // Load the data
    int num;
    if (parse.matchName("eventCount")) {
        parse.number(num);
        loop->eventCount = num;
    }
    if (parse.matchName("loopLength")) {
        parse.number(num);
        loop->loopLength = num;
    }
    
    // Load events array
    if (parse.matchName("events")) {
        int arraySize = 0;
        if (parse.numberOfArrayElements(arraySize)) {
            for (int i = 0; i < arraySize && i < MAX_EVENTS; i++) {
                int objSize = 0;
                if (parse.numberOfObjectMembers(objSize)) {
                    for (int j = 0; j < objSize; j++) {
                        if (parse.matchName("pulse")) {
                            parse.number(num);
                            loop->events[i].clockPulse = num;
                        } else if (parse.matchName("status")) {
                            parse.number(num);
                            loop->events[i].data[0] = num;
                        } else if (parse.matchName("data1")) {
                            parse.number(num);
                            loop->events[i].data[1] = num;
                        } else if (parse.matchName("data2")) {
                            parse.number(num);
                            loop->events[i].data[2] = num;
                        } else {
                            parse.skipMember();
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'L', 'O', 'P'),
    .name = "VLoop",
    .description = "Simple MIDI Looper",
    .numSpecifications = 0,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .step = step,
    .midiMessage = midiMessage,
    .tags = kNT_tagUtility,
    .hasCustomUi = 0,
    .customUi = 0,
    .setupUi = 0,
    .serialise = serialise,
    .deserialise = deserialise
};

extern "C" {
    uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
        switch (selector) {
            case kNT_selector_version:
                return kNT_apiVersionCurrent;
            case kNT_selector_numFactories:
                return 1;
            case kNT_selector_factoryInfo:
                return (uintptr_t)((data == 0) ? &factory : 0);
        }
        return 0;
    }
}
