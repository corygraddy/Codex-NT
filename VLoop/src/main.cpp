#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <new>

// Debug mode - set to true to enable debug counters and logging
#define VLOOP_DEBUG false

struct MidiEvent {
    uint8_t data[3];
    
    MidiEvent() {
        data[0] = data[1] = data[2] = 0;
    }
};

#define MAX_EVENTS_PER_PULSE 16
#define MAX_LOOP_LENGTH 512

// Simple fixed-size vector for events at each pulse
struct EventBucket {
    MidiEvent events[MAX_EVENTS_PER_PULSE];
    uint8_t count;
    
    EventBucket() : count(0) {}
    
    void clear() {
        count = 0;
    }
    
    bool add(const MidiEvent& event) {
        if (count < MAX_EVENTS_PER_PULSE) {
            events[count] = event;
            count++;
            return true;
        }
        return false;
    }
};

struct VLoop : public _NT_algorithm {
    EventBucket eventBuckets[MAX_LOOP_LENGTH];
    uint16_t currentPulse;
    uint16_t loopLength;
    bool isRecording;
    bool isPlaying;
    bool recordArmed;         // Record mode armed, waiting for first MIDI
    uint16_t recordStartPulse; // Pulse where recording actually started
    float lastClockCV;
    bool lastRecord;
    bool lastPlay;
    bool lastClear;
    
#if VLOOP_DEBUG
    // Debug counters (only when VLOOP_DEBUG is true)
    uint32_t totalClockEdges;
    uint32_t totalMidiSent;
    uint32_t totalMidiReceived;
    uint32_t stepCallCount;
    uint16_t lastPulseWithMidi;
    uint8_t lastMidiStatus;
    uint8_t lastMidiData1;
    uint8_t lastMidiData2;
#endif
    
    VLoop() {
        currentPulse = 0;
        loopLength = 0;
        isRecording = false;
        isPlaying = false;
        recordArmed = false;
        recordStartPulse = 0;
        lastClockCV = 0.0f;
        lastRecord = false;
        lastPlay = false;
        lastClear = false;
#if VLOOP_DEBUG
        totalClockEdges = 0;
        totalMidiSent = 0;
        totalMidiReceived = 0;
        stepCallCount = 0;
        lastPulseWithMidi = 0;
        lastMidiStatus = 0;
        lastMidiData1 = 0;
        lastMidiData2 = 0;
#endif
    }
};

enum {
    kParamClockInput = 0,
    kParamRecord,
    kParamPlay,
    kParamClear,
    kParamQuantize,
    kParamAutoStop,
    kNumParams
};

static const _NT_parameter parameters[] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Play", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Quantize", .min = 0, .max = 2, .def = 0, .scaling = kNT_scalingNone },  // 0=1/32, 1=1/16, 2=1/8
    { .name = "Auto Stop", .min = 0, .max = 16, .def = 0, .scaling = kNT_scalingNone } // 0=off, 1-16=beats
};

static const uint8_t page1[] = { kParamClockInput, kParamRecord, kParamPlay, kParamClear, kParamQuantize, kParamAutoStop };
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
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VLoop* loop = new (ptrs.sram) VLoop();
    loop->parameters = parameters;
    loop->parameterPages = &parameterPages;
    return loop;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoop* loop = (VLoop*)self;
    
#if VLOOP_DEBUG
    loop->stepCallCount++;
#endif
    
    // Get parameter values
    int clockBusIndex = (int)loop->v[kParamClockInput] - 1;
    bool recordActive = loop->v[kParamRecord] > 0.5f;
    bool playActive = loop->v[kParamPlay] > 0.5f;
    bool clearActive = loop->v[kParamClear] > 0.5f;
#if VLOOP_DEBUG
    int quantize = (int)loop->v[kParamQuantize];  // 0=1/32, 1=1/16, 2=1/8
#endif
    int autoStop = (int)loop->v[kParamAutoStop];   // 0=off, 1-16=beats
    
    // Assuming clock input is 1/32 notes (8 pulses per beat)
    int pulsesPerBeat = 8;  // 1/32 notes, 8 per quarter note
    
    // Handle Clear knob
    if (clearActive && !loop->lastClear) {
        // Clear all event buckets
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->eventBuckets[i].clear();
        }
        loop->loopLength = 0;
        loop->currentPulse = 0;
        loop->isRecording = false;
        loop->isPlaying = false;
    }
    loop->lastClear = clearActive;
    
    // Handle Record knob changes
    if (recordActive && !loop->lastRecord) {
        // Knob turned up - ARM record mode (wait for first MIDI)
        loop->isPlaying = false;
        loop->isRecording = false;
        loop->recordArmed = true;
        loop->currentPulse = 0;
        // Clear all buckets
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->eventBuckets[i].clear();
        }
        loop->loopLength = 0;
    } else if (!recordActive && loop->lastRecord) {
        // Knob turned down - stop recording, save loop length
        if (loop->isRecording) {
            // Quantize end to beat boundary (1/4 note = 8 pulses)
            loop->loopLength = ((loop->currentPulse + pulsesPerBeat - 1) / pulsesPerBeat) * pulsesPerBeat;
        }
        loop->isRecording = false;
        loop->recordArmed = false;
    }
    loop->lastRecord = recordActive;
    
    // Handle Play knob changes
    if (playActive && !loop->lastPlay) {
        // Knob turned up - start playback
        // Don't allow playback while recording
        if (loop->loopLength > 0 && !loop->isRecording) {
            loop->isPlaying = true;
            loop->currentPulse = 0;
        }
    } else if (!playActive && loop->lastPlay) {
        // Knob turned down - stop playback
        loop->isPlaying = false;
    }
    loop->lastPlay = playActive;
    
    // Process all frames looking for clock edge (detect only ONE edge per step)
    const int totalFrames = numFramesBy4 * 4;
    bool clockEdgeDetected = false;
    for (int frame = 0; frame < totalFrames && !clockEdgeDetected; frame++) {
        // Read clock CV from selected bus
        float clockCV = busFrames[clockBusIndex * totalFrames + frame];
        
        // Detect rising edge (crosses 2.5V threshold)
        bool clockEdge = (clockCV > 2.5f && loop->lastClockCV <= 2.5f);
        loop->lastClockCV = clockCV;
        
        if (clockEdge) {
            clockEdgeDetected = true;
#if VLOOP_DEBUG
            loop->totalClockEdges++;
#endif
            
            if (loop->isRecording) {
                // Already recording - increment pulse counter
                loop->currentPulse++;
                if (loop->currentPulse >= MAX_LOOP_LENGTH) {
                    loop->currentPulse = MAX_LOOP_LENGTH - 1;
                }
                
                // Check for auto-stop
                if (autoStop > 0) {
                    uint16_t pulsesRecorded = loop->currentPulse - loop->recordStartPulse;
                    uint16_t targetPulses = autoStop * pulsesPerBeat;  // beats * pulses per beat
                    
                    if (pulsesRecorded >= targetPulses) {
                        // Quantize end to beat boundary (1/4 note)
                        loop->loopLength = ((loop->currentPulse + pulsesPerBeat - 1) / pulsesPerBeat) * pulsesPerBeat;
                        loop->isRecording = false;
                        loop->recordArmed = false;
                        
                        // Auto-start playback
                        loop->isPlaying = true;
                        loop->currentPulse = 0;
                    }
                }
            } else if (loop->recordArmed) {
                // Armed but not recording - stay at pulse 0, waiting for first MIDI
                // Do nothing, let midiMessage() start the recording
            } else if (loop->isPlaying && loop->loopLength > 0) {
                // Send ALL events at current pulse
                if (loop->currentPulse < loop->loopLength && loop->currentPulse < MAX_LOOP_LENGTH) {
                    EventBucket& bucket = loop->eventBuckets[loop->currentPulse];
                    
                    // Send all events in this bucket
                    for (uint8_t i = 0; i < bucket.count; i++) {
#if VLOOP_DEBUG
                        loop->lastPulseWithMidi = loop->currentPulse;
                        loop->lastMidiStatus = bucket.events[i].data[0];
                        loop->lastMidiData1 = bucket.events[i].data[1];
                        loop->lastMidiData2 = bucket.events[i].data[2];
                        loop->totalMidiSent++;
#endif
                        
                        // Send only to Internal (for routing to other slots)
                        NT_sendMidi3ByteMessage(
                            kNT_destinationInternal,
                            bucket.events[i].data[0],
                            bucket.events[i].data[1],
                            bucket.events[i].data[2]
                        );
                    }
                }
                
                // Advance to next pulse
                loop->currentPulse++;
                if (loop->currentPulse >= loop->loopLength) {
                    loop->currentPulse = 0; // Loop wrap
                }
            }
        }
    }
}

void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoop* loop = (VLoop*)self;
    
#if VLOOP_DEBUG
    loop->totalMidiReceived++;
#endif
    
    // Always pass through incoming MIDI to output
    NT_sendMidi3ByteMessage(byte0, byte1, byte2, kNT_destinationInternal);
    
    // Don't record system messages
    if ((byte0 & 0xF0) == 0xF0) return;
    
    // If armed, start recording on first MIDI message (quantized start)
    if (loop->recordArmed && !loop->isRecording) {
        loop->isRecording = true;
        loop->recordArmed = false;
        loop->currentPulse = 0;
        loop->recordStartPulse = 0;
        // First event goes to pulse 0
    }
    
    // Only record when in recording mode
    if (!loop->isRecording) return;
    
    // Apply quantization - only record on quantized pulses
    int quantize = (int)loop->v[kParamQuantize];
    int quantizeDivisor = 1 << quantize;  // 1, 2, or 4
    
    if (loop->currentPulse % quantizeDivisor != 0) {
        // Not on a quantized pulse, skip recording this event
        return;
    }
    
    // Add event to current pulse's bucket
    if (loop->currentPulse < MAX_LOOP_LENGTH) {
        MidiEvent event;
        event.data[0] = byte0;
        event.data[1] = byte1;
        event.data[2] = byte2;
        loop->eventBuckets[loop->currentPulse].add(event);
    }
}

void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VLoop* loop = (VLoop*)self;
    
    // Write loop metadata
    stream.addMemberName("loopLength");
    stream.addNumber((int)loop->loopLength);
    
    stream.addMemberName("currentPulse");
    stream.addNumber((int)loop->currentPulse);
    
    stream.addMemberName("isRecording");
    stream.addBoolean(loop->isRecording);
    
    stream.addMemberName("recordArmed");
    stream.addBoolean(loop->recordArmed);
    
    stream.addMemberName("isPlaying");
    stream.addBoolean(loop->isPlaying);
    
#if VLOOP_DEBUG
    // Write debug counters
    stream.addMemberName("debug");
    stream.openObject();
    stream.addMemberName("totalClockEdges");
    stream.addNumber((int)loop->totalClockEdges);
    stream.addMemberName("totalMidiSent");
    stream.addNumber((int)loop->totalMidiSent);
    stream.addMemberName("totalMidiReceived");
    stream.addNumber((int)loop->totalMidiReceived);
    stream.addMemberName("stepCallCount");
    stream.addNumber((int)loop->stepCallCount);
    stream.addMemberName("lastPulseWithMidi");
    stream.addNumber((int)loop->lastPulseWithMidi);
    stream.addMemberName("lastMidiSent");
    stream.openObject();
    stream.addMemberName("status");
    stream.addNumber((int)loop->lastMidiStatus);
    stream.addMemberName("data1");
    stream.addNumber((int)loop->lastMidiData1);
    stream.addMemberName("data2");
    stream.addNumber((int)loop->lastMidiData2);
    stream.closeObject();
    stream.closeObject();
#endif
    
    // Write event buckets (only non-empty ones for efficiency)
    stream.addMemberName("buckets");
    stream.openArray();
    for (uint16_t pulse = 0; pulse < MAX_LOOP_LENGTH; pulse++) {
        if (loop->eventBuckets[pulse].count > 0) {
            stream.openObject();
            stream.addMemberName("pulse");
            stream.addNumber((int)pulse);
            stream.addMemberName("events");
            stream.openArray();
            for (uint8_t i = 0; i < loop->eventBuckets[pulse].count; i++) {
                stream.openObject();
                stream.addMemberName("status");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[0]);
                stream.addMemberName("data1");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[1]);
                stream.addMemberName("data2");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[2]);
                stream.closeObject();
            }
            stream.closeArray();
            stream.closeObject();
        }
    }
    stream.closeArray();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VLoop* loop = (VLoop*)self;
    
    // Reset state when loading preset
    for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
        loop->eventBuckets[i].clear();
    }
    loop->loopLength = 0;
    loop->currentPulse = 0;
    loop->isRecording = false;
    loop->isPlaying = false;
    
    // Load the data
    int num;
    if (parse.matchName("loopLength")) {
        parse.number(num);
        loop->loopLength = num;
    }
    
    // Load event buckets array
    if (parse.matchName("buckets")) {
        int arraySize = 0;
        if (parse.numberOfArrayElements(arraySize)) {
            for (int i = 0; i < arraySize; i++) {
                int bucketObjSize = 0;
                if (parse.numberOfObjectMembers(bucketObjSize)) {
                    uint16_t pulse = 0;
                    for (int j = 0; j < bucketObjSize; j++) {
                        if (parse.matchName("pulse")) {
                            parse.number(num);
                            pulse = num;
                        } else if (parse.matchName("events")) {
                            int eventsArraySize = 0;
                            if (parse.numberOfArrayElements(eventsArraySize)) {
                                for (int k = 0; k < eventsArraySize && k < MAX_EVENTS_PER_PULSE; k++) {
                                    int eventObjSize = 0;
                                    if (parse.numberOfObjectMembers(eventObjSize)) {
                                        MidiEvent event;
                                        for (int m = 0; m < eventObjSize; m++) {
                                            if (parse.matchName("status")) {
                                                parse.number(num);
                                                event.data[0] = num;
                                            } else if (parse.matchName("data1")) {
                                                parse.number(num);
                                                event.data[1] = num;
                                            } else if (parse.matchName("data2")) {
                                                parse.number(num);
                                                event.data[2] = num;
                                            } else {
                                                parse.skipMember();
                                            }
                                        }
                                        if (pulse < MAX_LOOP_LENGTH) {
                                            loop->eventBuckets[pulse].add(event);
                                        }
                                    }
                                }
                            }
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
