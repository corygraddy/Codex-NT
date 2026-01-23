#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

// V3Seq: Single 3-output CV sequencer
// - Clock and Reset inputs
// - 3 CV outputs
// - 3 MIDI CC outputs
// - 32 steps with 3 values per step
// - Direction control: Forward, Backward, Pingpong
// - Section looping with configurable repeats
// - Clock division and multiplication

struct V3Seq : public _NT_algorithm {
    // Sequencer data: 32 steps × 3 outputs
    int16_t stepValues[32][3];
    
    // Sequencer state
    int currentStep;            // Current step (0-31)
    bool pingpongForward;       // Direction state for pingpong mode
    int section1Counter;        // Track section 1 repeat count
    int section2Counter;        // Track section 2 repeat count
    bool inSection2;            // Which section is currently playing
    int clockCounter;           // Clock division counter
    int internalClockCounter;   // Internal subdivision counter for multiplication
    int lastClockPeriod;        // Samples between last two clocks (for multiplication)
    int samplesSinceLastClock;  // Sample counter since last clock
    
    // Edge detection
    float lastClockIn;
    float lastResetIn;
    
    // UI state
    int selectedStep;           // 0-31
    int lastSelectedStep;       // Track when step changes to update pots
    uint16_t lastEncoderRButton; // For debouncing right encoder button
    bool potCaught[3];          // Track if each pot has caught the step value
    
    V3Seq() {
        // Initialize step values to middle voltage (0V)
        for (int step = 0; step < 32; step++) {
            for (int out = 0; out < 3; out++) {
                stepValues[step][out] = 0;  // 0V = middle of -5V to +5V range
            }
        }
        
        // Initialize sequencer state
        currentStep = 0;
        pingpongForward = true;
        section1Counter = 0;
        section2Counter = 0;
        inSection2 = false;
        clockCounter = 0;
        internalClockCounter = 0;
        lastClockPeriod = 0;
        samplesSinceLastClock = 0;
        
        // Initialize edge detection
        lastClockIn = 0.0f;
        lastResetIn = 0.0f;
        
        // Initialize UI state
        selectedStep = 0;
        lastSelectedStep = -1;
        lastEncoderRButton = 0;
        for (int i = 0; i < 3; i++) {
            potCaught[i] = false;
        }
    }
    
    void advanceSequencer(int direction, int stepCount, int splitPoint, 
                          int sec1Reps, int sec2Reps);
};

// =============================================================================
// Parameters
// =============================================================================

enum {
    kParamClockIn,
    kParamResetIn,
    kParamOut1,
    kParamMidi1,
    kParamOut2,
    kParamMidi2,
    kParamOut3,
    kParamMidi3,
    kParamMidiChannel,
    kParamClockDiv,
    kParamDirection,
    kParamStepCount,
    kParamSplitPoint,
    kParamSection1Reps,
    kParamSection2Reps,
    kNumParameters
};

static _NT_parameter parameters[kNumParameters];

// Parameter name strings
static char clockInName[] = "Clock In";
static char resetInName[] = "Reset In";
static char out1Name[] = "CV Out 1";
static char midi1Name[] = "MIDI CC 1";
static char out2Name[] = "CV Out 2";
static char midi2Name[] = "MIDI CC 2";
static char out3Name[] = "CV Out 3";
static char midi3Name[] = "MIDI CC 3";
static char midiChannelName[] = "MIDI Channel";
static char clockDivName[] = "Clock Div/Mult";
static char directionName[] = "Direction";
static char stepCountName[] = "Step Count";
static char splitPointName[] = "Split Point";
static char section1RepsName[] = "Section 1 Reps";
static char section2RepsName[] = "Section 2 Reps";

// Clock division/multiplication strings (31 options: /16-/2, x1-x16)
static const char* const divisionStrings[] = {
    "/16", "/15", "/14", "/13", "/12", "/11", "/10", "/9", "/8", "/7", "/6", "/5", "/4", "/3", "/2",
    "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", NULL
};

static const char* const directionStrings[] = {
    "Forward", "Backward", "Pingpong", NULL
};

void initParameters(_NT_algorithm* self) {
    // Clock and Reset inputs
    parameters[kParamClockIn].name = clockInName;
    parameters[kParamClockIn].min = 0;
    parameters[kParamClockIn].max = 28;
    parameters[kParamClockIn].def = 0;
    parameters[kParamClockIn].unit = kNT_unitCvInput;
    parameters[kParamClockIn].scaling = kNT_scalingNone;
    
    parameters[kParamResetIn].name = resetInName;
    parameters[kParamResetIn].min = 0;
    parameters[kParamResetIn].max = 28;
    parameters[kParamResetIn].def = 0;
    parameters[kParamResetIn].unit = kNT_unitCvInput;
    parameters[kParamResetIn].scaling = kNT_scalingNone;
    
    // CV Outputs (1-28, 0 = off)
    parameters[kParamOut1].name = out1Name;
    parameters[kParamOut1].min = 0;
    parameters[kParamOut1].max = 28;
    parameters[kParamOut1].def = 0;
    parameters[kParamOut1].unit = kNT_unitCvOutput;
    parameters[kParamOut1].scaling = kNT_scalingNone;
    
    parameters[kParamMidi1].name = midi1Name;
    parameters[kParamMidi1].min = 0;
    parameters[kParamMidi1].max = 127;
    parameters[kParamMidi1].def = 1;
    parameters[kParamMidi1].unit = kNT_unitNone;
    parameters[kParamMidi1].scaling = kNT_scalingNone;
    
    parameters[kParamOut2].name = out2Name;
    parameters[kParamOut2].min = 0;
    parameters[kParamOut2].max = 28;
    parameters[kParamOut2].def = 0;
    parameters[kParamOut2].unit = kNT_unitCvOutput;
    parameters[kParamOut2].scaling = kNT_scalingNone;
    
    parameters[kParamMidi2].name = midi2Name;
    parameters[kParamMidi2].min = 0;
    parameters[kParamMidi2].max = 127;
    parameters[kParamMidi2].def = 2;
    parameters[kParamMidi2].unit = kNT_unitNone;
    parameters[kParamMidi2].scaling = kNT_scalingNone;
    
    parameters[kParamOut3].name = out3Name;
    parameters[kParamOut3].min = 0;
    parameters[kParamOut3].max = 28;
    parameters[kParamOut3].def = 0;
    parameters[kParamOut3].unit = kNT_unitCvOutput;
    parameters[kParamOut3].scaling = kNT_scalingNone;
    
    parameters[kParamMidi3].name = midi3Name;
    parameters[kParamMidi3].min = 0;
    parameters[kParamMidi3].max = 127;
    parameters[kParamMidi3].def = 3;
    parameters[kParamMidi3].unit = kNT_unitNone;
    parameters[kParamMidi3].scaling = kNT_scalingNone;
    
    // MIDI Channel (0 = off, 1-16 = MIDI channels)
    parameters[kParamMidiChannel].name = midiChannelName;
    parameters[kParamMidiChannel].min = 0;
    parameters[kParamMidiChannel].max = 16;
    parameters[kParamMidiChannel].def = 0;
    parameters[kParamMidiChannel].unit = kNT_unitNone;
    parameters[kParamMidiChannel].scaling = kNT_scalingNone;
    
    // Sequencer parameters
    parameters[kParamClockDiv].name = clockDivName;
    parameters[kParamClockDiv].min = 0;
    parameters[kParamClockDiv].max = 30;
    parameters[kParamClockDiv].def = 15;  // x1
    parameters[kParamClockDiv].unit = kNT_unitEnum;
    parameters[kParamClockDiv].scaling = kNT_scalingNone;
    parameters[kParamClockDiv].enumStrings = divisionStrings;
    
    parameters[kParamDirection].name = directionName;
    parameters[kParamDirection].min = 0;
    parameters[kParamDirection].max = 2;
    parameters[kParamDirection].def = 0;
    parameters[kParamDirection].unit = kNT_unitEnum;
    parameters[kParamDirection].scaling = kNT_scalingNone;
    parameters[kParamDirection].enumStrings = directionStrings;
    
    parameters[kParamStepCount].name = stepCountName;
    parameters[kParamStepCount].min = 1;
    parameters[kParamStepCount].max = 32;
    parameters[kParamStepCount].def = 16;
    parameters[kParamStepCount].unit = kNT_unitNone;
    parameters[kParamStepCount].scaling = kNT_scalingNone;
    
    parameters[kParamSplitPoint].name = splitPointName;
    parameters[kParamSplitPoint].min = 0;
    parameters[kParamSplitPoint].max = 31;
    parameters[kParamSplitPoint].def = 0;
    parameters[kParamSplitPoint].unit = kNT_unitNone;
    parameters[kParamSplitPoint].scaling = kNT_scalingNone;
    
    parameters[kParamSection1Reps].name = section1RepsName;
    parameters[kParamSection1Reps].min = 1;
    parameters[kParamSection1Reps].max = 99;
    parameters[kParamSection1Reps].def = 1;
    parameters[kParamSection1Reps].unit = kNT_unitNone;
    parameters[kParamSection1Reps].scaling = kNT_scalingNone;
    
    parameters[kParamSection2Reps].name = section2RepsName;
    parameters[kParamSection2Reps].min = 1;
    parameters[kParamSection2Reps].max = 99;
    parameters[kParamSection2Reps].def = 1;
    parameters[kParamSection2Reps].unit = kNT_unitNone;
    parameters[kParamSection2Reps].scaling = kNT_scalingNone;
    
    self->parameters = parameters;
}

// =============================================================================
// Construction
// =============================================================================

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(V3Seq);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    V3Seq* alg = new (ptrs.sram) V3Seq();
    initParameters(alg);
    return alg;
}

// =============================================================================
// Core Functions - Stubs for Phase 1
// =============================================================================

void V3Seq::advanceSequencer(int direction, int stepCount, int splitPoint, 
                              int sec1Reps, int sec2Reps) {
    // If no sections (splitPoint >= stepCount), use simple wrapping logic
    if (splitPoint >= stepCount) {
        if (direction == 0) {
            // Forward
            currentStep++;
            if (currentStep >= stepCount) {
                currentStep = 0;
            }
        } else if (direction == 1) {
            // Backward
            currentStep--;
            if (currentStep < 0) {
                currentStep = stepCount - 1;
            }
        } else {
            // Pingpong
            if (pingpongForward) {
                currentStep++;
                if (currentStep >= stepCount) {
                    currentStep = stepCount - 1;
                    pingpongForward = false;
                }
            } else {
                currentStep--;
                if (currentStep < 0) {
                    currentStep = 0;
                    pingpongForward = true;
                }
            }
        }
        return;
    }
    
    // Section-based logic
    if (direction == 0) {
        // Forward
        currentStep++;
        
        // Check if we've reached the end of a section
        if (!inSection2) {
            // In section 1
            if (currentStep >= splitPoint) {
                section1Counter++;
                if (section1Counter >= sec1Reps) {
                    // Move to section 2
                    inSection2 = true;
                    section1Counter = 0;
                } else {
                    // Repeat section 1
                    currentStep = 0;
                }
            }
        } else {
            // In section 2
            if (currentStep >= stepCount) {
                section2Counter++;
                if (section2Counter >= sec2Reps) {
                    // Loop back to section 1
                    inSection2 = false;
                    section2Counter = 0;
                    currentStep = 0;
                } else {
                    // Repeat section 2
                    currentStep = splitPoint;
                }
            }
        }
    } else if (direction == 1) {
        // Backward
        currentStep--;
        
        // Check if we've reached the start of a section
        if (inSection2) {
            // In section 2
            if (currentStep < splitPoint) {
                section2Counter++;
                if (section2Counter >= sec2Reps) {
                    // Move to section 1
                    inSection2 = false;
                    section2Counter = 0;
                } else {
                    // Repeat section 2
                    currentStep = stepCount - 1;
                }
            }
        } else {
            // In section 1
            if (currentStep < 0) {
                section1Counter++;
                if (section1Counter >= sec1Reps) {
                    // Move to section 2
                    inSection2 = true;
                    section1Counter = 0;
                    currentStep = stepCount - 1;
                } else {
                    // Repeat section 1
                    currentStep = splitPoint - 1;
                }
            }
        }
    } else {
        // Pingpong
        if (pingpongForward) {
            currentStep++;
            if (currentStep >= stepCount) {
                currentStep = stepCount - 1;
                pingpongForward = false;
            }
        } else {
            currentStep--;
            if (currentStep <= 0) {
                currentStep = 0;
                pingpongForward = true;
            }
        }
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    V3Seq* a = static_cast<V3Seq*>(self);
    int numFrames = numFramesBy4 * 4;
    
    // Get input bus indices from parameters
    int clockBus = self->v[kParamClockIn] - 1;  // 0-27 (parameter is 1-28)
    int resetBus = self->v[kParamResetIn] - 1;
    
    // Get first sample from each input bus (for edge detection)
    float clockIn = (clockBus >= 0 && clockBus < 28) ? busFrames[clockBus * numFrames] : 0.0f;
    float resetIn = (resetBus >= 0 && resetBus < 28) ? busFrames[resetBus * numFrames] : 0.0f;
    
    // Clock edge detection (rising edge > 0.5V)
    bool clockTrig = (clockIn > 0.5f && a->lastClockIn <= 0.5f);
    bool resetTrig = (resetIn > 0.5f && a->lastResetIn <= 0.5f);
    
    a->lastClockIn = clockIn;
    a->lastResetIn = resetIn;
    
    // Get sequencer parameters
    int clockDiv = self->v[kParamClockDiv];
    int direction = self->v[kParamDirection];
    int stepCount = self->v[kParamStepCount];
    int splitPoint = self->v[kParamSplitPoint];
    int sec1Reps = self->v[kParamSection1Reps];
    int sec2Reps = self->v[kParamSection2Reps];
    
    // Map clockDiv parameter to actual divisor/multiplier
    int divisor = 1;
    int multiplier = 1;
    bool isDivision = (clockDiv < 15);
    
    if (isDivision) {
        divisor = 16 - clockDiv;  // 0->16, 1->15, ..., 14->2
    } else {
        multiplier = clockDiv - 14;  // 15->1, 16->2, ..., 30->16
    }
    
    // Reset handling
    if (resetTrig) {
        a->currentStep = 0;
        a->pingpongForward = true;
        a->section1Counter = 0;
        a->section2Counter = 0;
        a->inSection2 = false;
        a->clockCounter = 0;
        a->internalClockCounter = 0;
        a->samplesSinceLastClock = 0;
    }
    
    // Track samples for multiplication modes
    a->samplesSinceLastClock += numFrames;
    
    // Clock handling with division/multiplication
    bool stepped = false;
    if (clockTrig) {
        // Measure clock period for multiplication
        if (a->samplesSinceLastClock > 100 && a->samplesSinceLastClock < 96000) {
            a->lastClockPeriod = a->samplesSinceLastClock;
        }
        a->samplesSinceLastClock = 0;
        a->internalClockCounter = 0;
        
        if (isDivision) {
            // Division mode: count clocks before advancing
            a->clockCounter++;
            if (a->clockCounter >= divisor) {
                a->clockCounter = 0;
                a->advanceSequencer(direction, stepCount, splitPoint, sec1Reps, sec2Reps);
                stepped = true;
            }
        } else {
            // Multiplication mode: step on external clock
            a->advanceSequencer(direction, stepCount, splitPoint, sec1Reps, sec2Reps);
            stepped = true;
        }
    }
    
    // Internal clock multiplication - generate additional steps between external clocks
    if (!isDivision && multiplier > 1 && !clockTrig && a->lastClockPeriod > 0) {
        int subdivisionPeriod = a->lastClockPeriod / multiplier;
        
        if (subdivisionPeriod > numFrames && a->samplesSinceLastClock >= subdivisionPeriod * (a->internalClockCounter + 1)) {
            a->internalClockCounter++;
            if (a->internalClockCounter < multiplier) {
                a->advanceSequencer(direction, stepCount, splitPoint, sec1Reps, sec2Reps);
                stepped = true;
            }
        }
    }
    
    // Clamp current step to step count (safety check)
    if (a->currentStep >= stepCount) {
        a->currentStep = stepCount - 1;
    }
    
    // Output current step values to all output buses
    int step = a->currentStep;
    for (int out = 0; out < 3; out++) {
        int outputBus = self->v[kParamOut1 + out];  // 0 = none, 1-28 = bus 0-27
        
        if (outputBus > 0 && outputBus <= 28) {
            int16_t value = a->stepValues[step][out];
            // Convert from int16_t range (-32768 to 32767) to voltage (0.0V to 10.0V)
            float outputValue = ((value + 32768) / 65535.0f) * 10.0f;
            
            // Write to all frames in the output bus
            float* outBus = busFrames + ((outputBus - 1) * numFrames);
            for (int frame = 0; frame < numFrames; frame++) {
                outBus[frame] = outputValue;
            }
        }
        
        // Send MIDI CC if channel is configured and sequencer just stepped
        if (stepped) {
            int midiChannel = self->v[kParamMidiChannel];  // 0 = off, 1-16 = MIDI channels
            
            if (midiChannel > 0 && midiChannel <= 16) {
                int ccNumber = self->v[kParamMidi1 + out];  // 0-127
                
                // Convert CV value to CC value (0-127)
                int16_t value = a->stepValues[step][out];
                float normalized = (value + 32768) / 65535.0f;  // 0.0-1.0
                uint8_t ccValue = (uint8_t)(normalized * 127.0f);
                if (ccValue > 127) ccValue = 127;
                
                uint8_t channel = (midiChannel - 1) & 0x0F;
                
                // Send CC message
                NT_sendMidi3ByteMessage(
                    kNT_destinationInternal,
                    0xB0 | channel,  // CC message
                    ccNumber,
                    ccValue
                );
            }
        }
    }
}

void parameterChanged(_NT_algorithm* self, int parameterIndex) {
    // Minimal parameter handling
    (void)self;
    (void)parameterIndex;
}

bool draw(_NT_algorithm* self) {
    // Phase 5: implement display
    return false;
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    // Phase 5: enable custom UI
    (void)self;
    return 0;
}

void handleUi(_NT_algorithm* self, const _NT_uiData& data) {
    // Phase 5: implement UI handling
    (void)self;
    (void)data;
}

// =============================================================================
// Plugin Factory
// =============================================================================

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', '3', 'S', 'Q'),
    .name = "V3Seq",
    .description = "3-Output CV Sequencer",
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
    .midiMessage = nullptr,
    .tags = kNT_tagUtility,
    .hasCustomUi = hasCustomUi,
    .customUi = handleUi,
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
            case kNT_selector_factoryInfo:
                return (uintptr_t)&factory;
            default:
                return 0;
        }
    }
}
