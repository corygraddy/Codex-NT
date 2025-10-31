#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <cstddef>
#include <new>

// VLoop - MIDI Looper for Disting NT
// MVP: Basic recording logic with state tracking and logging

// Recording states
enum RecordingState {
    IDLE = 0,           // Not recording, waiting for record button
    WAITING_FOR_RESET,  // Record pressed, waiting for reset to sync recording start
    ACTIVELY_RECORDING  // Recording and capturing MIDI events until next reset
};

// MIDI Event structure for logging
struct MidiEvent {
    uint32_t timestamp;    // Relative to loop start (in sub-ticks)
    uint8_t status;        // MIDI status byte
    uint8_t data1;         // MIDI data byte 1
    uint8_t data2;         // MIDI data byte 2
    
    MidiEvent() : timestamp(0), status(0), data1(0), data2(0) {}
    MidiEvent(uint32_t t, uint8_t s, uint8_t d1, uint8_t d2) 
        : timestamp(t), status(s), data1(d1), data2(d2) {}
};

// Maximum events we can store for logging
#define MAX_EVENTS 64

// Data To Continue (DTC) - persistent data
struct VLoopAlgorithm_DTC {
    // State tracking for analysis
    uint32_t totalClockTicks;
    uint32_t totalResetTicks;
    uint32_t recordingStartClockTick;
    uint32_t recordingEndClockTick;
    uint32_t loopLengthTicks;
    uint32_t recordingState;
    
    // Event logging
    uint32_t eventCount;
    MidiEvent events[MAX_EVENTS];
    
    // State transition logging
    uint32_t stateTransitionCount;
    uint32_t lastTransitionTick;
    uint32_t lastTransitionFromState;
    uint32_t lastTransitionToState;
    
    VLoopAlgorithm_DTC() : 
        totalClockTicks(0), totalResetTicks(0), recordingStartClockTick(0),
        recordingEndClockTick(0), loopLengthTicks(0), recordingState(IDLE),
        eventCount(0), stateTransitionCount(0), lastTransitionTick(0),
        lastTransitionFromState(0), lastTransitionToState(0) {}
};

// Main algorithm structure
struct VLoopAlgorithm : public _NT_algorithm {
    VLoopAlgorithm_DTC* dtc;
    
    // Timing state
    uint32_t currentClockTick;
    uint32_t currentSubTick;
    uint32_t loopStartTick;
    RecordingState currentState;
    
    // Input tracking for edge detection
    float lastClockValue;
    float lastResetValue;
    float lastRecordValue;
    
    VLoopAlgorithm(VLoopAlgorithm_DTC* dtc_ptr) : 
        dtc(dtc_ptr), currentClockTick(0), currentSubTick(0), loopStartTick(0),
        currentState(IDLE), lastClockValue(0.0f), lastResetValue(0.0f), lastRecordValue(0.0f) {}
};

// Parameters
enum {
    kParamClockInput,
    kParamResetInput,
    kParamRecord,
    kParamPrecision,
    kNumParameters
};

static const _NT_parameter parameters[kNumParameters] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Reset In", .min = 1, .max = 28, .def = 2, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Precision", .min = 1, .max = 16, .def = 4 }
};

static const uint8_t page1_params[] = { kParamClockInput, kParamResetInput };
static const uint8_t page2_params[] = { kParamRecord, kParamPrecision };

static const _NT_parameterPage pageArray[] = {
    { .name = "INPUTS", .numParams = 2, .params = page1_params },
    { .name = "CONTROL", .numParams = 2, .params = page2_params }
};

static const _NT_parameterPages pages = {
    .numPages = 2, 
    .pages = pageArray
};

// Helper function to log state transitions
void logStateTransition(VLoopAlgorithm* pThis, RecordingState fromState, RecordingState toState) {
    pThis->dtc->stateTransitionCount++;
    pThis->dtc->lastTransitionTick = pThis->currentClockTick;
    pThis->dtc->lastTransitionFromState = fromState;
    pThis->dtc->lastTransitionToState = toState;
    pThis->currentState = toState;
    pThis->dtc->recordingState = toState;
}

// Helper function to log MIDI event
void logMidiEvent(VLoopAlgorithm* pThis, uint8_t status, uint8_t data1, uint8_t data2) {
    if (pThis->dtc->eventCount < MAX_EVENTS) {
        uint32_t timestamp = (pThis->currentClockTick - pThis->loopStartTick) * pThis->v[kParamPrecision] + pThis->currentSubTick;
        pThis->dtc->events[pThis->dtc->eventCount] = MidiEvent(timestamp, status, data1, data2);
        pThis->dtc->eventCount++;
    }
}

// Memory requirements
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VLoopAlgorithm);
    req.dtc = sizeof(VLoopAlgorithm_DTC);
}

// Construction
_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    VLoopAlgorithm_DTC* dtc = new (ptrs.dtc) VLoopAlgorithm_DTC();
    VLoopAlgorithm* alg = new (ptrs.sram) VLoopAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

// MIDI message handler
void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    // Only log MIDI events when actively recording
    if (pThis->currentState == ACTIVELY_RECORDING) {
        logMidiEvent(pThis, byte0, byte1, byte2);
    }
}

// Main DSP step function
void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    // Get input bus indices (convert from 1-28 parameter range to 0-27 bus array indices)
    int clockBusIndex = (int)pThis->v[kParamClockInput] - 1;  // 0-27 (parameter is 1-28)
    int resetBusIndex = (int)pThis->v[kParamResetInput] - 1;  // 0-27 (parameter is 1-28)
    int precision = (int)pThis->v[kParamPrecision];
    
    // Process each frame
    for (int frame = 0; frame < numFramesBy4 * 4; frame++) {
        // Read inputs (with bounds checking)
        float clockIn = (clockBusIndex >= 0 && clockBusIndex < 28) ? 
                        busFrames[clockBusIndex * numFramesBy4 * 4 + frame] : 0.0f;
        float resetIn = (resetBusIndex >= 0 && resetBusIndex < 28) ? 
                        busFrames[resetBusIndex * numFramesBy4 * 4 + frame] : 0.0f;
        float recordParam = pThis->v[kParamRecord];
        
        // Detect clock rising edge
        bool clockRising = (clockIn >= 1.0f && pThis->lastClockValue < 1.0f);
        bool resetRising = (resetIn >= 1.0f && pThis->lastResetValue < 1.0f);
        bool recordRising = (recordParam >= 0.5f && pThis->lastRecordValue < 0.5f);
        bool recordFalling = (recordParam < 0.5f && pThis->lastRecordValue >= 0.5f);
        
        // Update sub-tick counter on every sample (for precision timing)
        pThis->currentSubTick++;
        if (pThis->currentSubTick >= (uint32_t)precision) {
            pThis->currentSubTick = 0;
        }
        
        // Process clock tick
        if (clockRising) {
            pThis->currentClockTick++;
            pThis->dtc->totalClockTicks++;
            pThis->currentSubTick = 0; // Reset sub-tick on clock
        }
        
        // Process reset tick
        if (resetRising) {
            pThis->dtc->totalResetTicks++;
            
            // If we're waiting for reset to start recording
            if (pThis->currentState == WAITING_FOR_RESET) {
                pThis->loopStartTick = pThis->currentClockTick;
                pThis->dtc->recordingStartClockTick = pThis->currentClockTick;
                logStateTransition(pThis, WAITING_FOR_RESET, ACTIVELY_RECORDING);
            }
            // If we're actively recording, this could end the loop
            else if (pThis->currentState == ACTIVELY_RECORDING) {
                pThis->dtc->recordingEndClockTick = pThis->currentClockTick;
                pThis->dtc->loopLengthTicks = pThis->currentClockTick - pThis->loopStartTick;
                
                // Complete the recording and return to IDLE
                logStateTransition(pThis, ACTIVELY_RECORDING, IDLE);
            }
        }
        
        // Process record parameter changes
        if (recordRising) {
            if (pThis->currentState == IDLE) {
                logStateTransition(pThis, IDLE, WAITING_FOR_RESET);
            }
        }
        else if (recordFalling) {
            // IMPROVED LOGIC: Only cancel from WAITING_FOR_RESET if no recording has started
            // Once recording begins, button release doesn't interrupt the process
            if (pThis->currentState == WAITING_FOR_RESET) {
                logStateTransition(pThis, WAITING_FOR_RESET, IDLE);
            }
            else if (pThis->currentState == ACTIVELY_RECORDING) {
                // Record release time but stay in recording until next reset
                // This allows for "punch out" style recording where button release marks intent to stop
                pThis->dtc->recordingEndClockTick = pThis->currentClockTick;
            }
        }
        
        // Update previous values for edge detection
        pThis->lastClockValue = clockIn;
        pThis->lastResetValue = resetIn;
        pThis->lastRecordValue = recordParam;
    }
}

// Serialization for logging analysis
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    stream.addMemberName("vloop_analysis");
    stream.openObject();
    
    // Timing statistics
    stream.addMemberName("timing");
    stream.openObject();
    stream.addMemberName("totalClockTicks");
    stream.addNumber((int)pThis->dtc->totalClockTicks);
    stream.addMemberName("totalResetTicks");
    stream.addNumber((int)pThis->dtc->totalResetTicks);
    stream.addMemberName("recordingStartTick");
    stream.addNumber((int)pThis->dtc->recordingStartClockTick);
    stream.addMemberName("recordingEndTick");
    stream.addNumber((int)pThis->dtc->recordingEndClockTick);
    stream.addMemberName("loopLengthTicks");
    stream.addNumber((int)pThis->dtc->loopLengthTicks);
    stream.closeObject();
    
    // State information
    stream.addMemberName("state");
    stream.openObject();
    stream.addMemberName("currentState");
    stream.addNumber((int)pThis->dtc->recordingState);
    stream.addMemberName("transitionCount");
    stream.addNumber((int)pThis->dtc->stateTransitionCount);
    stream.addMemberName("lastTransitionTick");
    stream.addNumber((int)pThis->dtc->lastTransitionTick);
    stream.addMemberName("lastTransitionFrom");
    stream.addNumber((int)pThis->dtc->lastTransitionFromState);
    stream.addMemberName("lastTransitionTo");
    stream.addNumber((int)pThis->dtc->lastTransitionToState);
    stream.closeObject();
    
    // MIDI events
    stream.addMemberName("midiEvents");
    stream.openArray();
    for (uint32_t i = 0; i < pThis->dtc->eventCount; i++) {
        stream.openObject();
        stream.addMemberName("timestamp");
        stream.addNumber((int)pThis->dtc->events[i].timestamp);
        stream.addMemberName("status");
        stream.addNumber((int)pThis->dtc->events[i].status);
        stream.addMemberName("data1");
        stream.addNumber((int)pThis->dtc->events[i].data1);
        stream.addMemberName("data2");
        stream.addNumber((int)pThis->dtc->events[i].data2);
        stream.closeObject();
    }
    stream.closeArray();
    
    stream.closeObject();
}

// Deserialization
bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    if (parse.matchName("vloop_analysis")) {
        // For now, just restore basic state
        if (parse.matchName("timing")) {
            if (parse.matchName("totalClockTicks")) {
                int val; parse.number(val); pThis->dtc->totalClockTicks = val;
            }
            if (parse.matchName("totalResetTicks")) {
                int val; parse.number(val); pThis->dtc->totalResetTicks = val;
            }
            if (parse.matchName("recordingStartTick")) {
                int val; parse.number(val); pThis->dtc->recordingStartClockTick = val;
            }
            if (parse.matchName("recordingEndTick")) {
                int val; parse.number(val); pThis->dtc->recordingEndClockTick = val;
            }
            if (parse.matchName("loopLengthTicks")) {
                int val; parse.number(val); pThis->dtc->loopLengthTicks = val;
            }
        }
        
        if (parse.matchName("state")) {
            if (parse.matchName("currentState")) {
                int val; parse.number(val); pThis->dtc->recordingState = val;
            }
            if (parse.matchName("transitionCount")) {
                int val; parse.number(val); pThis->dtc->stateTransitionCount = val;
            }
            if (parse.matchName("lastTransitionTick")) {
                int val; parse.number(val); pThis->dtc->lastTransitionTick = val;
            }
            if (parse.matchName("lastTransitionFrom")) {
                int val; parse.number(val); pThis->dtc->lastTransitionFromState = val;
            }
            if (parse.matchName("lastTransitionTo")) {
                int val; parse.number(val); pThis->dtc->lastTransitionToState = val;
            }
        }
    }
    
    return true;
}

// Factory definition
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'L', 'P', '1'),
    .name = "VLoop",
    .description = "MIDI Looper with Recording Logic",
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .step = step,
    .midiMessage = midiMessage,
    .serialise = serialise,
    .deserialise = deserialise
};

// Plugin entry point
uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : nullptr);
    }
    return 0;
}