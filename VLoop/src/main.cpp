// VLoop B1101-0900 - Event-Driven MIDI Looper for Disting NT
// Professional architecture with relative timestamps and sorted events
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <cstddef>
#include <new>
#include <algorithm>  // For std::sort

#define VLOOP_BUILD_NUMBER "B1101-0900"  // Nov 1, 09:00 - Event-Driven Architecture

// --- CORE ARCHITECTURE ---

// State Machine: Clean separation of concerns
enum LooperState {
    STOPPED = 0,    // Idle, ready to start recording
    RECORDING,      // Capturing MIDI events with relative timestamps
    PLAYING         // Playing back recorded loop with efficient sorted lookup
};

// MIDI Event with Relative Timestamp
struct LoopEvent {
    uint32_t timeDelta;     // Time relative to loop start (in clock ticks)
    uint8_t midiData[3];    // MIDI message bytes
    
    LoopEvent() : timeDelta(0) {
        midiData[0] = midiData[1] = midiData[2] = 0;
    }
    
    LoopEvent(uint32_t delta, uint8_t b0, uint8_t b1, uint8_t b2) : timeDelta(delta) {
        midiData[0] = b0;
        midiData[1] = b1;
        midiData[2] = b2;
    }
    
    // Comparison operator for sorting by timeDelta
    bool operator<(const LoopEvent& other) const {
        return timeDelta < other.timeDelta;
    }
};

#define MAX_LOOP_EVENTS 2560  // 20x larger buffer for long sequences at fast tempos

// Data-To-Core: Persistent state
struct VLoopDTC {
    // Core timing
    uint32_t globalTime;           // Absolute time since plugin start
    uint32_t loopStartTime;        // Absolute time when recording started
    uint32_t loopLength;           // Length of recorded loop in clock ticks
    
    // Event storage
    LoopEvent loopEvents[MAX_LOOP_EVENTS];
    uint32_t eventCount;
    
    // Playback state
    uint32_t currentPlaybackTime;  // Current position in loop (0 to loopLength-1)
    uint32_t playbackIndex;        // Index of next event to check (optimization)
    
    // DEBUG: Event logging and analysis
    uint32_t totalMidiEventsReceived;   // Total MIDI events captured
    uint32_t lastTimeDelta;             // Previous event's timeDelta for comparison
    uint32_t deltaGaps[32];             // Store last 32 gap sizes between events
    uint32_t gapIndex;                  // Current position in deltaGaps array
    
    VLoopDTC() {
        globalTime = 0;
        loopStartTime = 0;
        loopLength = 0;
        eventCount = 0;
        currentPlaybackTime = 0;
        playbackIndex = 0;
        totalMidiEventsReceived = 0;
        lastTimeDelta = 0;
        gapIndex = 0;
        // Initialize gap tracking
        for (int i = 0; i < 32; i++) {
            deltaGaps[i] = 0;
        }
    }
};

// Parameters
enum {
    kParamClockInput = 0,   // Clock input bus (1-28)
    kParamResetInput,       // Reset input bus (1-28)
    kParamRecord,           // Record button
    kParamPlay,             // Play button
    kParamClear,            // Clear recorded events
    kNumParams
};

// Main algorithm class
class VLoopAlgorithm : public _NT_algorithm {
public:
    VLoopDTC* dtc;
    LooperState currentState;
    bool pendingStopRecording;  // Flag to stop recording on next clock
    
    // Edge detection for inputs
    float lastClockValue;
    float lastResetValue;
    float lastRecordValue;
    float lastPlayValue;
    float lastClearValue;
    
    VLoopAlgorithm(VLoopDTC* dtc_ptr) : dtc(dtc_ptr), currentState(STOPPED), pendingStopRecording(false) {
        lastClockValue = 0.0f;
        lastResetValue = 0.0f;
        lastRecordValue = 0.0f;
        lastPlayValue = 0.0f;
        lastClearValue = 0.0f;
    }
    
    // --- CORE ENGINE FUNCTIONS ---
    
    void recordMidiEvent(uint8_t byte0, uint8_t byte1, uint8_t byte2) {
        // SAFETY: Double-check recording state
        if (currentState != RECORDING) return;
        if (dtc->eventCount >= MAX_LOOP_EVENTS) {
            return;
        }
        
        // DEBUG: Count total MIDI events received
        dtc->totalMidiEventsReceived++;
        
        // FIXED: QUANTIZE to current clock position for consistent timing
        // This ensures all events snap to clock grid regardless of arrival time
        uint32_t timeDelta;
        if (dtc->globalTime >= dtc->loopStartTime) {
            timeDelta = dtc->globalTime - dtc->loopStartTime;
        } else {
            timeDelta = 0;  // MIDI arrived before recording officially started
        }
        
        // DEBUG: Log delta gaps between consecutive events
        if (dtc->eventCount > 0) {
            uint32_t gap = (timeDelta > dtc->lastTimeDelta) ? 
                          (timeDelta - dtc->lastTimeDelta) : 0;
            dtc->deltaGaps[dtc->gapIndex % 32] = gap;
            dtc->gapIndex++;
        }
        dtc->lastTimeDelta = timeDelta;
        
        // QUANTIZATION: All events get snapped to current clock tick
        // This means KeyStep sync offset won't affect rhythm!
        
        // Store event with quantized timestamp
        dtc->loopEvents[dtc->eventCount] = LoopEvent(timeDelta, byte0, byte1, byte2);
        dtc->eventCount++;
    }
    
    void startRecording() {
        if (currentState != STOPPED) return;
        
        currentState = RECORDING;
        // FIXED: Start recording on NEXT clock tick for proper sync
        dtc->loopStartTime = dtc->globalTime + 1;
        dtc->eventCount = 0;  // Clear previous recording
        
        // DEBUG: Reset logging counters for new recording
        dtc->totalMidiEventsReceived = 0;
        dtc->lastTimeDelta = 0;
        dtc->gapIndex = 0;
        for (int i = 0; i < 32; i++) {
            dtc->deltaGaps[i] = 0;
        }
    }
    
    void stopRecording() {
        if (currentState != RECORDING) return;
        
        // FIXED: Quantize loop end to next clock boundary for clean looping
        uint32_t rawLoopLength = (dtc->globalTime - dtc->loopStartTime) + 1;
        
        // QUANTIZE: Round up to next clock tick to ensure clean loop boundaries
        // This prevents "halfway between notes" loop restart issues
        dtc->loopLength = rawLoopLength;  // Already quantized since we use clock ticks
        
        if (dtc->loopLength == 0) dtc->loopLength = 1;  // Prevent zero-length loops
        
        // Sort events by timeDelta for efficient playback
        if (dtc->eventCount > 1) {
            std::sort(dtc->loopEvents, dtc->loopEvents + dtc->eventCount);
        }
        
        currentState = STOPPED;
    }
    
    void startPlayback() {
        if (currentState != STOPPED || dtc->eventCount == 0) return;
        
        currentState = PLAYING;
        dtc->currentPlaybackTime = 0;
        dtc->playbackIndex = 0;
    }
    
    void stopPlayback() {
        if (currentState != PLAYING) return;
        currentState = STOPPED;
    }
    
    void clearLoop() {
        currentState = STOPPED;
        dtc->eventCount = 0;
        dtc->loopLength = 0;
        dtc->currentPlaybackTime = 0;
        dtc->playbackIndex = 0;
    }
    
    // Single update() function - THE ENGINE
    void update() {
        if (currentState != PLAYING) return;
        if (dtc->eventCount == 0 || dtc->loopLength == 0) return;
        
        // Check for events at current playback time
        while (dtc->playbackIndex < dtc->eventCount) {
            LoopEvent& event = dtc->loopEvents[dtc->playbackIndex];
            
            if (event.timeDelta == dtc->currentPlaybackTime) {
                // Fire this event
                uint32_t allDestinations = kNT_destinationBreakout | kNT_destinationSelectBus | 
                                           kNT_destinationUSB | kNT_destinationInternal;
                NT_sendMidi3ByteMessage(allDestinations, event.midiData[0], event.midiData[1], event.midiData[2]);
                dtc->playbackIndex++;
            }
            else if (event.timeDelta > dtc->currentPlaybackTime) {
                // Future event, stop checking
                break;
            }
            else {
                // Should not happen with sorted events, but safety check
                dtc->playbackIndex++;
            }
        }
        
        // Advance playback time
        dtc->currentPlaybackTime++;
        
        // FIXED: Wrap-around with proper playback index reset
        if (dtc->currentPlaybackTime >= dtc->loopLength) {
            dtc->currentPlaybackTime = 0;
            dtc->playbackIndex = 0;  // This is actually correct - we restart from beginning of sorted array
        }
    }
};

// --- DISTING NT API IMPLEMENTATION ---

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    // Get input bus indices
    int clockBusIndex = (int)pThis->v[kParamClockInput] - 1;  // 0-27
    int resetBusIndex = (int)pThis->v[kParamResetInput] - 1;  // 0-27
    
    // Process each frame
    for (int frame = 0; frame < numFramesBy4 * 4; frame++) {
        // Read inputs
        float clockIn = (clockBusIndex >= 0 && clockBusIndex < 28) ? 
                        busFrames[clockBusIndex * numFramesBy4 * 4 + frame] : 0.0f;
        float resetIn = (resetBusIndex >= 0 && resetBusIndex < 28) ? 
                        busFrames[resetBusIndex * numFramesBy4 * 4 + frame] : 0.0f;
        float recordParam = pThis->v[kParamRecord];
        float playParam = pThis->v[kParamPlay];
        float clearParam = pThis->v[kParamClear];
        
        // Detect edges
        bool clockRising = (clockIn >= 1.0f && pThis->lastClockValue < 1.0f);
        bool resetRising = (resetIn >= 1.0f && pThis->lastResetValue < 1.0f);
        bool recordRising = (recordParam >= 0.5f && pThis->lastRecordValue < 0.5f);
        bool recordFalling = (recordParam < 0.5f && pThis->lastRecordValue >= 0.5f);
        bool playRising = (playParam >= 0.5f && pThis->lastPlayValue < 0.5f);
        bool playFalling = (playParam < 0.5f && pThis->lastPlayValue >= 0.5f);
        bool clearRising = (clearParam >= 0.5f && pThis->lastClearValue < 0.5f);
        
        // Update global time on clock
        if (clockRising) {
            pThis->dtc->globalTime++;
        }
        
        // Reset functionality - restart loop playback from beginning
        if (resetRising) {
            if (pThis->currentState == PLAYING) {
                pThis->dtc->currentPlaybackTime = 0;
                pThis->dtc->playbackIndex = 0;
            }
        }
        
        // State machine control - IMPROVED: More robust state handling
        if (recordRising) {
            pThis->startRecording();
        }
        // FIXED: Only stop recording on explicit button release, not spurious signals
        else if (recordFalling && pThis->currentState == RECORDING) {
            pThis->stopRecording();
        }
        
        if (playRising) {
            pThis->startPlayback();
        }
        else if (playFalling) {
            pThis->stopPlayback();
        }
        
        if (clearRising) {
            pThis->clearLoop();
        }
        
        // THE ENGINE: Update playback on each clock tick
        if (clockRising) {
            pThis->update();
        }
        
        // Store last values for edge detection
        pThis->lastClockValue = clockIn;
        pThis->lastResetValue = resetIn;
        pThis->lastRecordValue = recordParam;
        pThis->lastPlayValue = playParam;
        pThis->lastClearValue = clearParam;
    }
}

// MIDI input callback - EVENT-DRIVEN RECORDING
void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    // Only record MIDI when in RECORDING state
    if (pThis->currentState == RECORDING) {
        // FILTER: Only record Note On/Off messages to prevent MIDI flood
        uint8_t messageType = byte0 & 0xF0;
        if (messageType == 0x80 || messageType == 0x90) {  // Note Off or Note On
            pThis->recordMidiEvent(byte0, byte1, byte2);
        }
        // Ignore CC, Pitch Bend, etc. for now to debug the timing
    }
}

// --- API FUNCTIONS ---

// Parameter definitions  
static const _NT_parameter parameters[kNumParams] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Reset In", .min = 1, .max = 28, .def = 2, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Play", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone }
};

static const uint8_t page1_params[] = { kParamClockInput, kParamResetInput };
static const uint8_t page2_params[] = { kParamRecord, kParamPlay, kParamClear };

static const _NT_parameterPage pageArray[] = {
    { .name = "INPUTS", .numParams = 2, .params = page1_params },
    { .name = "CONTROL", .numParams = 3, .params = page2_params }
};

static const _NT_parameterPages pages = { .numPages = 2, .pages = pageArray };

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParams;
    req.sram = sizeof(VLoopAlgorithm);
    req.dtc = sizeof(VLoopDTC);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    VLoopDTC* dtc = new (ptrs.dtc) VLoopDTC();
    VLoopAlgorithm* alg = new (ptrs.sram) VLoopAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

// Serialization - EXPORT DEBUG DATA
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VLoopAlgorithm* pThis = (VLoopAlgorithm*)self;
    
    stream.openObject();
    stream.addMemberName("vloop_debug_v2");
    
    // Core stats
    stream.openObject();
    stream.addMemberName("recording_stats");
    stream.openObject();
    stream.addMemberName("totalMidiEvents");
    stream.addNumber((int)pThis->dtc->totalMidiEventsReceived);
    stream.addMemberName("storedEvents");
    stream.addNumber((int)pThis->dtc->eventCount);
    stream.addMemberName("loopLength");
    stream.addNumber((int)pThis->dtc->loopLength);
    stream.closeObject();
    
    // Delta gap analysis - CRITICAL for debugging KeyStep timing
    stream.addMemberName("delta_gaps");
    stream.openArray();
    uint32_t gapsToShow = (pThis->dtc->gapIndex < 32) ? pThis->dtc->gapIndex : 32;
    for (uint32_t i = 0; i < gapsToShow; i++) {
        stream.addNumber((int)pThis->dtc->deltaGaps[i]);
    }
    stream.closeArray();
    
    // Individual event deltas for pattern analysis
    stream.addMemberName("event_deltas");
    stream.openArray();
    for (uint32_t i = 0; i < pThis->dtc->eventCount && i < 64; i++) {  // Show first 64 events
        stream.addNumber((int)pThis->dtc->loopEvents[i].timeDelta);
    }
    stream.closeArray();
    
    stream.closeObject();
    stream.closeObject();
}

// Factory definition
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'L', 'P', '2'),
    .name = "VLoop2",
    .description = "Event-Driven MIDI Looper " VLOOP_BUILD_NUMBER,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .step = step,
    .midiMessage = midiMessage,
    .serialise = serialise,
    .deserialise = nullptr
};
extern "C" {
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
}