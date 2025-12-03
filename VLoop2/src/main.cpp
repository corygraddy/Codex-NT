/*
 * VLoop2 - Single track MIDI looper with 8 loop slots
 * Version: 0.4.1
 * 
 * Features:
 * - 8 independent loop slots
 * - Record/Overdub with Add or Overwrite modes
 * - Undo function for overdubs
 * - MIDI channel filtering and remapping
 * - 128KB DRAM for ~16K MIDI events
 */

#define VLOOP2_VERSION "0.4.1"

#include <distingnt/api.h>
#include <new>
#include <string.h>

// =============================================================================
// Data Structures
// =============================================================================

struct MidiEvent {
    uint32_t timestamp;  // Sample count from loop start
    uint8_t byte0;       // Status byte
    uint8_t byte1;       // Data byte 1
    uint8_t byte2;       // Data byte 2
};

struct Loop {
    uint32_t startIndex;   // Index in event pool where this loop starts
    uint32_t eventCount;   // Number of events in this loop
    uint32_t loopLength;   // Loop length in samples
    bool isEmpty;
};

struct VLoop2 : public _NT_algorithm {
    // Active loops
    Loop loops[8];
    MidiEvent* eventPool;      // Allocated in DRAM
    uint32_t poolCapacity;     // Total events available
    uint32_t poolUsed;         // Events currently used
    
    // Undo buffer (same size as active)
    Loop undoLoops[8];
    MidiEvent* undoEventPool;  // Allocated in DRAM
    uint32_t undoPoolUsed;
    int undoLoopIndex;         // Which loop has undo data (-1 = none)
    bool canUndo;
    
    // State
    int currentLoop;           // Active loop slot (0-7)
    uint32_t playhead;         // Current playback position in samples
    uint32_t playbackIndex;    // Index of next event to check for playback
    uint32_t recordStart;      // Sample count when recording started
    bool isRecording;
    bool isPlaying;
    
    VLoop2() : 
        eventPool(nullptr),
        poolCapacity(0),
        poolUsed(0),
        undoEventPool(nullptr),
        undoPoolUsed(0),
        undoLoopIndex(-1),
        canUndo(false),
        currentLoop(0),
        playhead(0),
        playbackIndex(0),
        recordStart(0),
        isRecording(false),
        isPlaying(false)
    {
        for (int i = 0; i < 8; i++) {
            loops[i].startIndex = 0;
            loops[i].eventCount = 0;
            loops[i].loopLength = 0;
            loops[i].isEmpty = true;
            
            undoLoops[i].startIndex = 0;
            undoLoops[i].eventCount = 0;
            undoLoops[i].loopLength = 0;
            undoLoops[i].isEmpty = true;
        }
    }
};

// =============================================================================
// Parameter Definitions
// =============================================================================

enum {
    kParamLoopSelect,
    kParamRecord,
    kParamOverdubMode,
    kParamPlayStop,
    kParamClear,
    kParamUndo,
    kParamMidiIn,
    kParamMidiOut,
    kNumParameters
};

static const char* const overdubModeStrings[] = {
    "Add",
    "Overwrite"
};

static const _NT_parameter parameters[] = {
    { .name = "Loop Select", .min = 0, .max = 7, .def = 0, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "Overdub Mode", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = overdubModeStrings },
    { .name = "Play/Stop", .min = 0, .max = 1, .def = 0, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "Undo", .min = 0, .max = 1, .def = 0, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "MIDI In", .min = 1, .max = 16, .def = 1, .unit = 0, .scaling = 0, .enumStrings = nullptr },
    { .name = "MIDI Out", .min = 1, .max = 16, .def = 2, .unit = 0, .scaling = 0, .enumStrings = nullptr },
};

static const uint8_t page1[] = { kParamLoopSelect, kParamRecord, kParamOverdubMode, kParamPlayStop };
static const uint8_t page2[] = { kParamClear, kParamUndo, kParamMidiIn, kParamMidiOut };

static const _NT_parameterPage pages[] = {
    { .name = "Main", .numParams = 4, .params = page1 },
    { .name = "Edit", .numParams = 4, .params = page2 },
};

static const _NT_parameterPages parameterPages = {
    .numPages = 2,
    .pages = pages,
};

// =============================================================================
// Custom UI
// =============================================================================

uint32_t hasCustomUi(_NT_algorithm* self) {
    // We want to draw custom display
    return 0;  // Return 0 to use standard UI for now, we'll just add to draw callback
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    // Not used yet
}

bool draw(_NT_algorithm* self) {
    VLoop2* pThis = static_cast<VLoop2*>(self);
    
    // Draw version in bottom right corner
    NT_drawText(250, 58, VLOOP2_VERSION, 8, kNT_textRight, kNT_textTiny);
    
    // Draw recording/playing status
    if (pThis->isRecording) {
        NT_drawText(10, 58, "REC", 15, kNT_textLeft, kNT_textTiny);
    }
    if (pThis->isPlaying) {
        NT_drawText(30, 58, "PLAY", 15, kNT_textLeft, kNT_textTiny);
    }
    
    return false;  // Don't suppress standard parameter display
}

// =============================================================================
// Helper Functions
// =============================================================================

// Will be implemented in later phases
static void saveUndoState(VLoop2* self);
static void restoreUndo(VLoop2* self);
static void clearLoop(VLoop2* self, int loopIndex);
static void addEvent(VLoop2* self, int loopIndex, uint32_t timestamp, uint8_t b0, uint8_t b1, uint8_t b2);
static void deleteEventsAt(VLoop2* self, int loopIndex, uint32_t timestamp);

// =============================================================================
// API Callbacks
// =============================================================================

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VLoop2);
    req.dram = 131072;  // 128KB: 64KB active + 64KB undo
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    VLoop2* self = new (ptrs.sram) VLoop2();
    
    // Allocate event pools in DRAM
    // Split 128KB in half: 64KB for active, 64KB for undo
    const uint32_t totalEvents = 16384;  // 128KB / 8 bytes per event
    self->poolCapacity = totalEvents / 2;  // 8192 events for active pool
    
    self->eventPool = reinterpret_cast<MidiEvent*>(ptrs.dram);
    self->undoEventPool = reinterpret_cast<MidiEvent*>(ptrs.dram + (totalEvents / 2) * sizeof(MidiEvent));
    
    self->parameters = parameters;
    self->parameterPages = &parameterPages;
    
    return self;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoop2* pThis = static_cast<VLoop2*>(self);
    
    // Early return if nothing to do
    if (!pThis->isPlaying && !pThis->isRecording) {
        return;
    }
    
    Loop* currentLoop = &pThis->loops[pThis->currentLoop];
    
    // Only process playback if we have data and are playing
    if (pThis->isPlaying && !currentLoop->isEmpty && currentLoop->loopLength > 0 && currentLoop->eventCount > 0) {
        int outputChannel = pThis->v[kParamMidiOut];
        int numSamples = numFramesBy4 * 4;
        
        // Process this buffer of samples
        for (int i = 0; i < numSamples; i++) {
            // Check if the next event should fire at this playhead position
            // Events are recorded in chronological order, so we only scan forward
            while (pThis->playbackIndex < currentLoop->eventCount) {
                uint32_t poolIndex = currentLoop->startIndex + pThis->playbackIndex;
                MidiEvent* event = &pThis->eventPool[poolIndex];
                
                // If this event is at current playhead, send it
                if (event->timestamp == pThis->playhead) {
                    uint8_t remappedByte0 = (event->byte0 & 0xF0) | ((outputChannel - 1) & 0x0F);
                    NT_sendMidi3ByteMessage(~0, remappedByte0, event->byte1, event->byte2);
                    pThis->playbackIndex++;
                }
                // If this event is in the future, stop checking
                else if (event->timestamp > pThis->playhead) {
                    break;
                }
                // If this event is in the past (shouldn't happen), skip it
                else {
                    pThis->playbackIndex++;
                }
            }
            
            // Increment playhead
            pThis->playhead++;
            
            // Loop wrap - reset both playhead and index
            if (pThis->playhead >= currentLoop->loopLength) {
                pThis->playhead = 0;
                pThis->playbackIndex = 0;
            }
        }
    } else if (pThis->isPlaying) {
        // Playing but no loop - just advance playhead for empty loops
        int numSamples = numFramesBy4 * 4;
        pThis->playhead += numSamples;
        if (currentLoop->loopLength > 0 && pThis->playhead >= currentLoop->loopLength) {
            pThis->playhead = pThis->playhead % currentLoop->loopLength;
        }
    }
}

void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoop2* pThis = static_cast<VLoop2*>(self);
    
    // Phase 2: MIDI pass-through with channel filtering and remapping
    
    // Extract incoming MIDI channel (1-16)
    int incomingChannel = (byte0 & 0x0F) + 1;
    int inputChannel = pThis->v[kParamMidiIn];
    int outputChannel = pThis->v[kParamMidiOut];
    
    // Only process messages on our input channel
    if (incomingChannel == inputChannel) {
        // Remap to output channel
        uint8_t remappedByte0 = (byte0 & 0xF0) | ((outputChannel - 1) & 0x0F);
        
        // Pass through to MIDI output
        // ~0 means send to all destinations (Breakout, Select Bus, USB, Internal)
        NT_sendMidi3ByteMessage(~0, remappedByte0, byte1, byte2);
        
        // Phase 3: Recording logic
        if (pThis->isRecording) {
            // Store original MIDI event (before channel remapping)
            addEvent(pThis, pThis->currentLoop, pThis->playhead, byte0, byte1, byte2);
        }
    }
}

void parameterChanged(_NT_algorithm* self, int param) {
    VLoop2* pThis = static_cast<VLoop2*>(self);
    int32_t value = pThis->v[param];
    
    switch (param) {
        case kParamLoopSelect:
            // Phase 5: Loop switching logic
            // For now, just update current loop
            pThis->currentLoop = value;
            pThis->playhead = 0;  // Reset playhead
            break;
            
        case kParamRecord:
            // Phase 3: Recording logic
            if (value == 1 && !pThis->isRecording) {
                // Start recording
                pThis->isRecording = true;
                pThis->recordStart = pThis->playhead;
            } else if (value == 0 && pThis->isRecording) {
                // Stop recording
                pThis->isRecording = false;
                
                Loop* loop = &pThis->loops[pThis->currentLoop];
                if (!loop->isEmpty && loop->loopLength == 0) {
                    // First recording - set loop length
                    loop->loopLength = pThis->playhead - pThis->recordStart;
                    if (loop->loopLength == 0) {
                        loop->loopLength = 1;  // Minimum length
                    }
                    
                    // Auto-play after first recording
                    pThis->isPlaying = true;
                    pThis->playhead = 0;  // Reset to start
                    pThis->playbackIndex = 0;  // Reset playback position
                }
            }
            break;
            
        case kParamPlayStop:
            // Phase 4: Playback logic
            pThis->isPlaying = (value == 1);
            break;
            
        case kParamClear:
            // Phase 8: Clear function
            if (value == 1) {
                // clearLoop(pThis, pThis->currentLoop);
            }
            break;
            
        case kParamUndo:
            // Phase 9: Undo function
            if (value == 1 && pThis->canUndo) {
                // restoreUndo(pThis);
            }
            break;
            
        case kParamMidiIn:
            // Validate: MIDI In must differ from MIDI Out
            // Note: Can't modify other parameters from here, will handle in UI
            break;
            
        case kParamMidiOut:
            // Validate: MIDI Out must differ from MIDI In
            // Note: Can't modify other parameters from here, will handle in UI
            break;
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

void saveUndoState(VLoop2* self) {
    // Phase 9: Save current state to undo buffer
}

void restoreUndo(VLoop2* self) {
    // Phase 9: Restore from undo buffer
}

void clearLoop(VLoop2* self, int loopIndex) {
    // Phase 8: Clear loop data
}

void addEvent(VLoop2* self, int loopIndex, uint32_t timestamp, uint8_t b0, uint8_t b1, uint8_t b2) {
    // Phase 3: Add MIDI event to loop
    if (self->poolUsed >= self->poolCapacity) {
        // Memory full - stop recording
        self->isRecording = false;
        return;
    }
    
    Loop* loop = &self->loops[loopIndex];
    
    // If this is the first event in the loop, initialize start index
    if (loop->isEmpty) {
        loop->startIndex = self->poolUsed;
        loop->isEmpty = false;
    }
    
    // Add event to pool
    uint32_t poolIndex = loop->startIndex + loop->eventCount;
    self->eventPool[poolIndex].timestamp = timestamp;
    self->eventPool[poolIndex].byte0 = b0;
    self->eventPool[poolIndex].byte1 = b1;
    self->eventPool[poolIndex].byte2 = b2;
    
    loop->eventCount++;
    self->poolUsed++;
}

void deleteEventsAt(VLoop2* self, int loopIndex, uint32_t timestamp) {
    // Phase 7: Delete events at timestamp (for overwrite mode)
}

// =============================================================================
// Plugin Factory
// =============================================================================

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'L', 'P', '2'),
    .name = "VLoop2",
    .description = "MIDI Looper",
    .numSpecifications = 0,
    .specifications = nullptr,
    .calculateStaticRequirements = nullptr,
    .initialise = nullptr,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .midiRealtime = nullptr,
    .midiMessage = midiMessage,
    .tags = kNT_tagUtility,
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = nullptr,
    .serialise = nullptr,
    .deserialise = nullptr,
    .midiSysEx = nullptr,
};

extern "C" {
    uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
        switch (selector) {
            case kNT_selector_version:
                return kNT_apiVersionCurrent;
            case kNT_selector_numFactories:
                return 1;
            case kNT_selector_factoryInfo:
                return reinterpret_cast<uintptr_t>(&factory);
            default:
                return 0;
        }
    }
}
