#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

// VSeq: 4-channel 16-step sequencer
// - Clock and Reset inputs
// - 4 sequencers × 3 CV outputs = 12 total outputs
// - Each sequencer has 16 steps × 3 values
// - Clock division/multiplication
// - Direction control: Forward, Backward, Pingpong
// - Step count: 1-16 steps

struct VSeq : public _NT_algorithm {
    // Sequencer data: 4 sequencers × 16 steps × 3 outputs
    int16_t stepValues[4][16][3];
    
    // Sequencer state
    int currentStep[4];         // Current step for each sequencer (0-15)
    bool pingpongForward[4];    // Direction state for pingpong mode
    int clockDivCounter[4];     // Counter for clock division
    
    // Edge detection
    float lastClockIn;
    float lastResetIn;
    
    // UI state
    int selectedStep;           // 0-15
    int selectedSeq;            // 0-3 (which sequencer to view/edit)
    bool editMode;              // true = edit mode (pots control values)
    int lastSelectedStep;       // Track when step changes to update pots
    uint16_t lastButtonState;   // For debouncing encoder button
    
    // Debug: track actual output bus assignments
    int debugOutputBus[12];
    
    VSeq() {
        // Initialize step values to test patterns (visible voltages)
        // Each sequencer gets different voltage levels for testing
        for (int seq = 0; seq < 4; seq++) {
            for (int step = 0; step < 16; step++) {
                for (int out = 0; out < 3; out++) {
                    // Create test patterns: different voltages for each output
                    // seq 0: 2V, 4V, 6V
                    // seq 1: 1V, 3V, 5V
                    // seq 2: 3V, 5V, 7V
                    // seq 3: 2.5V, 4.5V, 6.5V
                    float voltage = 2.0f + (seq * 1.0f) + (out * 2.0f);
                    if (seq == 1) voltage -= 1.0f;
                    if (seq == 3) voltage += 0.5f;
                    
                    // Convert voltage (0-10V range) to int16_t (-32768 to 32767)
                    // 0V = -32768, 10V = 32767
                    float normalized = voltage / 10.0f;  // 0.0-1.0
                    stepValues[seq][step][out] = (int16_t)((normalized * 65535.0f) - 32768.0f);
                }
            }
            currentStep[seq] = 0;
            pingpongForward[seq] = true;
            clockDivCounter[seq] = 0;
        }
        
        lastClockIn = 0.0f;
        lastResetIn = 0.0f;
        selectedStep = 0;
        selectedSeq = 0;
        editMode = false;
        lastSelectedStep = 0;
        lastButtonState = 0;
        
        for (int i = 0; i < 12; i++) {
            debugOutputBus[i] = 0;
        }
    }
    
    // Advance sequencer to next step based on direction
    void advanceSequencer(int seq, int direction) {
        if (direction == 0) {
            // Forward
            currentStep[seq]++;
            if (currentStep[seq] >= 16) currentStep[seq] = 0;
        } else if (direction == 1) {
            // Backward
            currentStep[seq]--;
            if (currentStep[seq] < 0) currentStep[seq] = 15;
        } else {
            // Pingpong
            if (pingpongForward[seq]) {
                currentStep[seq]++;
                if (currentStep[seq] >= 15) {
                    currentStep[seq] = 15;
                    pingpongForward[seq] = false;
                }
            } else {
                currentStep[seq]--;
                if (currentStep[seq] <= 0) {
                    currentStep[seq] = 0;
                    pingpongForward[seq] = true;
                }
            }
        }
    }
    
    void resetSequencer(int seq) {
        int direction = 0;  // Will be set from parameters in process()
        if (direction == 1) {
            // Backward: start at last step
            currentStep[seq] = 15;
        } else {
            // Forward and Pingpong: start at step 0
            currentStep[seq] = 0;
        }
        pingpongForward[seq] = true;
        clockDivCounter[seq] = 0;
    }
};

// Parameter indices
enum {
    kParamClockIn = 0,
    kParamResetIn,
    // Sequencer 1 outputs
    kParamSeq1Out1,
    kParamSeq1Out2,
    kParamSeq1Out3,
    // Sequencer 2 outputs
    kParamSeq2Out1,
    kParamSeq2Out2,
    kParamSeq2Out3,
    // Sequencer 3 outputs
    kParamSeq3Out1,
    kParamSeq3Out2,
    kParamSeq3Out3,
    // Sequencer 4 outputs
    kParamSeq4Out1,
    kParamSeq4Out2,
    kParamSeq4Out3,
    // Per-sequencer parameters
    kParamSeq1ClockDiv,
    kParamSeq1Direction,
    kParamSeq1StepCount,
    kParamSeq2ClockDiv,
    kParamSeq2Direction,
    kParamSeq2StepCount,
    kParamSeq3ClockDiv,
    kParamSeq3Direction,
    kParamSeq3StepCount,
    kParamSeq4ClockDiv,
    kParamSeq4Direction,
    kParamSeq4StepCount,
    kNumParameters
};

// String arrays for enum parameters
static const char* const divisionStrings[] = {
    "/16", "/8", "/4", "/2", "x1", "x2", "x4", "x8", "x16", NULL
};

static const char* const directionStrings[] = {
    "Forward", "Backward", "Pingpong", NULL
};

// Global parameter array
static _NT_parameter parameters[kNumParameters];

// Initialize parameter definitions
static void initParameters() {
    // Clock and Reset inputs
    parameters[kParamClockIn].name = "Clock in";
    parameters[kParamClockIn].min = 0;
    parameters[kParamClockIn].max = 28;
    parameters[kParamClockIn].def = 1;
    parameters[kParamClockIn].unit = kNT_unitCvInput;
    parameters[kParamClockIn].scaling = kNT_scalingNone;
    
    parameters[kParamResetIn].name = "Reset in";
    parameters[kParamResetIn].min = 0;
    parameters[kParamResetIn].max = 28;
    parameters[kParamResetIn].def = 2;
    parameters[kParamResetIn].unit = kNT_unitCvInput;
    parameters[kParamResetIn].scaling = kNT_scalingNone;
    
    // CV Outputs (12 total)
    const char* outNames[] = {
        "Seq 1 Out 1", "Seq 1 Out 2", "Seq 1 Out 3",
        "Seq 2 Out 1", "Seq 2 Out 2", "Seq 2 Out 3",
        "Seq 3 Out 1", "Seq 3 Out 2", "Seq 3 Out 3",
        "Seq 4 Out 1", "Seq 4 Out 2", "Seq 4 Out 3"
    };
    
    for (int i = 0; i < 12; i++) {
        int paramIdx = kParamSeq1Out1 + i;
        parameters[paramIdx].name = outNames[i];
        parameters[paramIdx].min = 1;
        parameters[paramIdx].max = 28;
        parameters[paramIdx].def = i + 1;
        parameters[paramIdx].unit = kNT_unitCvOutput;
        parameters[paramIdx].scaling = kNT_scalingNone;
    }
    
    // Sequencer configuration parameters
    for (int seq = 0; seq < 4; seq++) {
        int divParam = kParamSeq1ClockDiv + (seq * 3);
        int dirParam = kParamSeq1Direction + (seq * 3);
        int stepParam = kParamSeq1StepCount + (seq * 3);
        
        char divName[20], dirName[20], stepName[20];
        snprintf(divName, sizeof(divName), "Seq %d Clock Div", seq + 1);
        snprintf(dirName, sizeof(dirName), "Seq %d Direction", seq + 1);
        snprintf(stepName, sizeof(stepName), "Seq %d Steps", seq + 1);
        
        // Clock Division parameter
        parameters[divParam].name = divName;
        parameters[divParam].min = 0;
        parameters[divParam].max = 8;  // /16, /8, /4, /2, x1, x2, x4, x8, x16
        parameters[divParam].def = 4;  // x1
        parameters[divParam].unit = kNT_unitEnum;
        parameters[divParam].scaling = kNT_scalingNone;
        parameters[divParam].enumStrings = divisionStrings;
        
        // Direction parameter
        parameters[dirParam].name = dirName;
        parameters[dirParam].min = 0;
        parameters[dirParam].max = 2;  // Forward, Backward, Pingpong
        parameters[dirParam].def = 0;  // Forward
        parameters[dirParam].unit = kNT_unitEnum;
        parameters[dirParam].scaling = kNT_scalingNone;
        parameters[dirParam].enumStrings = directionStrings;
        
        // Step Count parameter
        parameters[stepParam].name = stepName;
        parameters[stepParam].min = 1;
        parameters[stepParam].max = 16;
        parameters[stepParam].def = 16;
        parameters[stepParam].unit = kNT_unitNone;
        parameters[stepParam].scaling = kNT_scalingNone;
    }
}

// Parameter pages
static uint8_t paramPageInputs[] = { kParamClockIn, kParamResetIn, 0 };
static uint8_t paramPageSeq1Out[] = { kParamSeq1Out1, kParamSeq1Out2, kParamSeq1Out3, 0 };
static uint8_t paramPageSeq2Out[] = { kParamSeq2Out1, kParamSeq2Out2, kParamSeq2Out3, 0 };
static uint8_t paramPageSeq3Out[] = { kParamSeq3Out1, kParamSeq3Out2, kParamSeq3Out3, 0 };
static uint8_t paramPageSeq4Out[] = { kParamSeq4Out1, kParamSeq4Out2, kParamSeq4Out3, 0 };
static uint8_t paramPageSeq1Params[] = { kParamSeq1ClockDiv, kParamSeq1Direction, kParamSeq1StepCount, 0 };
static uint8_t paramPageSeq2Params[] = { kParamSeq2ClockDiv, kParamSeq2Direction, kParamSeq2StepCount, 0 };
static uint8_t paramPageSeq3Params[] = { kParamSeq3ClockDiv, kParamSeq3Direction, kParamSeq3StepCount, 0 };
static uint8_t paramPageSeq4Params[] = { kParamSeq4ClockDiv, kParamSeq4Direction, kParamSeq4StepCount, 0 };

static _NT_parameterPage pageArray[] = {
    { .name = "Inputs", .numParams = 2, .params = paramPageInputs },
    { .name = "Seq 1 Outs", .numParams = 3, .params = paramPageSeq1Out },
    { .name = "Seq 2 Outs", .numParams = 3, .params = paramPageSeq2Out },
    { .name = "Seq 3 Outs", .numParams = 3, .params = paramPageSeq3Out },
    { .name = "Seq 4 Outs", .numParams = 3, .params = paramPageSeq4Out },
    { .name = "Seq 1 Params", .numParams = 3, .params = paramPageSeq1Params },
    { .name = "Seq 2 Params", .numParams = 3, .params = paramPageSeq2Params },
    { .name = "Seq 3 Params", .numParams = 3, .params = paramPageSeq3Params },
    { .name = "Seq 4 Params", .numParams = 3, .params = paramPageSeq4Params }
};

static _NT_parameterPages pages = {
    .numPages = 9,
    .pages = pageArray
};

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VSeq);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VSeq* alg = new (ptrs.sram) VSeq();
    initParameters();
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    
    // Initialize debug output bus array from default parameter values
    for (int i = 0; i < 12; i++) {
        alg->debugOutputBus[i] = parameters[kParamSeq1Out1 + i].def;
    }
    
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VSeq* a = (VSeq*)self;
    
    // Get input bus indices from parameters
    int clockBus = self->v[kParamClockIn] - 1;  // 0-27 (parameter is 1-28)
    int resetBus = self->v[kParamResetIn] - 1;
    
    // Calculate number of actual frames
    int numFrames = numFramesBy4 * 4;
    
    // Get first sample from each input bus (for edge detection)
    float clockIn = (clockBus >= 0 && clockBus < 28) ? busFrames[clockBus * numFrames] : 0.0f;
    float resetIn = (resetBus >= 0 && resetBus < 28) ? busFrames[resetBus * numFrames] : 0.0f;
    
    // Clock edge detection (rising edge)
    bool clockTrig = (clockIn > 0.5f && a->lastClockIn <= 0.5f);
    bool resetTrig = (resetIn > 0.5f && a->lastResetIn <= 0.5f);
    
    a->lastClockIn = clockIn;
    a->lastResetIn = resetIn;
    
    // Process each sequencer
    for (int seq = 0; seq < 4; seq++) {
        int divParam = kParamSeq1ClockDiv + (seq * 3);
        int dirParam = kParamSeq1Direction + (seq * 3);
        int stepParam = kParamSeq1StepCount + (seq * 3);
        
        int clockDiv = self->v[divParam];   // 0-8 (/16 to x16)
        int direction = self->v[dirParam];  // 0=Forward, 1=Backward, 2=Pingpong
        int stepCount = self->v[stepParam]; // 1-16
        
        // Reset handling
        if (resetTrig) {
            a->resetSequencer(seq);
        }
        
        // Clock handling with division/multiplication
        if (clockTrig) {
            int divFactor = 1;
            if (clockDiv == 0) divFactor = 16;      // /16
            else if (clockDiv == 1) divFactor = 8;  // /8
            else if (clockDiv == 2) divFactor = 4;  // /4
            else if (clockDiv == 3) divFactor = 2;  // /2
            else if (clockDiv == 4) divFactor = 1;  // x1
            else if (clockDiv == 5) divFactor = -2; // x2
            else if (clockDiv == 6) divFactor = -4; // x4
            else if (clockDiv == 7) divFactor = -8; // x8
            else if (clockDiv == 8) divFactor = -16;// x16
            
            if (divFactor > 0) {
                // Division: increment counter, advance when reaching divFactor
                a->clockDivCounter[seq]++;
                if (a->clockDivCounter[seq] >= divFactor) {
                    a->clockDivCounter[seq] = 0;
                    a->advanceSequencer(seq, direction);
                }
            } else {
                // Multiplication: advance multiple times per clock
                int multFactor = -divFactor;
                for (int i = 0; i < multFactor; i++) {
                    a->advanceSequencer(seq, direction);
                }
            }
        }
        
        // Clamp current step to step count
        if (a->currentStep[seq] >= stepCount) {
            a->currentStep[seq] = stepCount - 1;
        }
        
        // Output current step values to all output buses
        int step = a->currentStep[seq];
        for (int out = 0; out < 3; out++) {
            int paramIdx = kParamSeq1Out1 + (seq * 3) + out;
            int outputBus = self->v[paramIdx] - 1;  // 0-27 (parameter is 1-28)
            
            // Store actual bus assignment for debug
            int debugIdx = seq * 3 + out;
            a->debugOutputBus[debugIdx] = outputBus + 1;  // Store as 1-28 for readability
            
            if (outputBus >= 0 && outputBus < 28) {
                int16_t value = a->stepValues[seq][step][out];
                // Convert from int16_t range to 0.0-1.0
                float outputValue = (value + 32768) / 65535.0f;
                
                // Write to all frames in the output bus
                float* outBus = busFrames + (outputBus * numFrames);
                for (int frame = 0; frame < numFrames; frame++) {
                    outBus[frame] = outputValue;
                }
            }
        }
    }
}

bool draw(_NT_algorithm* self) {
    VSeq* a = (VSeq*)self;
    
    // Draw based on current page (0-8: Inputs, 4 seq outputs, 4 seq params)
    // For now, focus on step view UI for sequencer pages
    
    // Clear screen
    NT_drawShapeI(kNT_rectangle, 0, 0, 256, 64, 0);  // Black background
    
    // Display step view for selected sequencer
    int seq = a->selectedSeq;  // 0-3
    
    // Draw title
    char title[16];
    snprintf(title, sizeof(title), "SEQ %d", seq + 1);
    NT_drawText(0, 0, title, 255);
    
    // Draw 16 steps in 2 rows of 8
    // Each step gets 3 skinny bars for 3 outputs
    // Screen is 256 wide, divided into 2 rows of 8 steps
    
    int barWidth = 3;   // Width of each bar
    int barSpacing = 1; // Space between bars
    int stepWidth = 16; // Total width per step (3 bars + spacing)
    int startY = 10;    // Start below title
    int rowHeight = 26; // Height of each row
    int maxBarHeight = 22; // Maximum bar height
    
    for (int step = 0; step < 16; step++) {
        int row = step / 8;      // 0 or 1
        int col = step % 8;      // 0-7
        
        int x = col * stepWidth;
        int y = startY + (row * rowHeight);
        
        // Draw 3 vertical bars for this step
        for (int out = 0; out < 3; out++) {
            int16_t value = a->stepValues[seq][step][out];
            // Convert int16_t (-32768 to 32767) to 0.0-1.0
            float normalized = (value + 32768.0f) / 65535.0f;
            // Convert to bar height (1 to maxBarHeight pixels)
            int barHeight = (int)(normalized * maxBarHeight);
            if (barHeight < 1) barHeight = 1;
            
            int barX = x + (out * (barWidth + barSpacing));
            int barBottomY = y + maxBarHeight;
            int barTopY = barBottomY - barHeight;
            
            // Draw bar (filled rectangle from top to bottom)
            NT_drawShapeI(kNT_rectangle, barX, barTopY, barX + barWidth - 1, barBottomY - 1, 255);
        }
        
        // Draw step indicator dot if this is the current step
        if (step == a->currentStep[seq]) {
            // Draw dot above the middle bar (bar 1)
            int dotX = x + (barWidth + barSpacing);  // Above middle bar
            NT_drawShapeI(kNT_rectangle, dotX, y - 2, dotX + barWidth - 1, y - 1, 255);
        }
        
        // Draw selection bar if this is the selected step (for editing)
        if (a->editMode && step == a->selectedStep) {
            NT_drawShapeI(kNT_line, x, y + maxBarHeight + 1, x + 13, y + maxBarHeight + 1, 255);
        }
    }
    
    // Draw page number on right side
    char pageNum[8];
    snprintf(pageNum, sizeof(pageNum), "%d", seq + 1);
    NT_drawText(240, 0, pageNum, 255);
    
    // Draw mode indicator
    if (a->editMode) {
        NT_drawText(200, 56, "EDIT", 128);
    }
    
    return true;  // Suppress default parameter line
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    return kNT_potL | kNT_potC | kNT_potR | kNT_encoderR | kNT_encoderButtonR;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    VSeq* a = (VSeq*)self;
    
    // Right encoder: select step (0-15)
    if (data.encoders[1] != 0) {
        int delta = data.encoders[1];
        a->selectedStep += delta;
        if (a->selectedStep < 0) a->selectedStep = 15;
        if (a->selectedStep > 15) a->selectedStep = 0;
    }
    
    // Right encoder button: toggle edit mode (with debouncing)
    uint16_t currentButton = data.controls & kNT_encoderButtonR;
    uint16_t lastButton = a->lastButtonState & kNT_encoderButtonR;
    if (currentButton && !lastButton) {  // Rising edge only
        a->editMode = !a->editMode;
    }
    a->lastButtonState = data.controls;
    
    // Pots control the 3 values for the selected step (only if pot changed in edit mode)
    if (a->editMode && (data.controls & (kNT_potL | kNT_potC | kNT_potR))) {
        if (data.controls & kNT_potL) {
            float potValue = data.pots[0];
            a->stepValues[a->selectedSeq][a->selectedStep][0] = (int16_t)((potValue * 65535.0f) - 32768);
        }
        if (data.controls & kNT_potC) {
            float potValue = data.pots[1];
            a->stepValues[a->selectedSeq][a->selectedStep][1] = (int16_t)((potValue * 65535.0f) - 32768);
        }
        if (data.controls & kNT_potR) {
            float potValue = data.pots[2];
            a->stepValues[a->selectedSeq][a->selectedStep][2] = (int16_t)((potValue * 65535.0f) - 32768);
        }
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    VSeq* a = (VSeq*)self;
    
    // Only update pot positions when step changes or entering edit mode
    if (a->editMode && a->selectedStep != a->lastSelectedStep) {
        a->lastSelectedStep = a->selectedStep;
        for (int i = 0; i < 3; i++) {
            int16_t value = a->stepValues[a->selectedSeq][a->selectedStep][i];
            // Convert from int16_t to 0.0-1.0
            pots[i] = (value + 32768) / 65535.0f;
        }
    }
}

void parameterChanged(_NT_algorithm* self, int parameterIndex) {
    VSeq* a = (VSeq*)self;
    
    // Update debug output bus tracking when output parameters change
    if (parameterIndex >= kParamSeq1Out1 && parameterIndex <= kParamSeq4Out3) {
        int debugIdx = parameterIndex - kParamSeq1Out1;
        a->debugOutputBus[debugIdx] = self->v[parameterIndex];  // Store parameter value (1-28)
    }
}

void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VSeq* a = (VSeq*)self;
    
    // Save all step values as 3D array
    stream.addMemberName("stepValues");
    stream.openArray();
    for (int seq = 0; seq < 4; seq++) {
        stream.openArray();
        for (int step = 0; step < 16; step++) {
            stream.openArray();
            for (int out = 0; out < 3; out++) {
                stream.addNumber((int)a->stepValues[seq][step][out]);
            }
            stream.closeArray();
        }
        stream.closeArray();
    }
    stream.closeArray();
    
    // Save debug output bus assignments
    stream.addMemberName("debugOutputBus");
    stream.openArray();
    for (int i = 0; i < 12; i++) {
        stream.addNumber(a->debugOutputBus[i]);
    }
    stream.closeArray();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VSeq* a = (VSeq*)self;
    
    // Match "stepValues"
    if (parse.matchName("stepValues")) {
        int numSeqs = 0;
        if (parse.numberOfArrayElements(numSeqs) && numSeqs == 4) {
            for (int seq = 0; seq < 4; seq++) {
                int numSteps = 0;
                if (parse.numberOfArrayElements(numSteps) && numSteps == 16) {
                    for (int step = 0; step < 16; step++) {
                        int numOuts = 0;
                        if (parse.numberOfArrayElements(numOuts) && numOuts == 3) {
                            for (int out = 0; out < 3; out++) {
                                int value;
                                if (parse.number(value)) {
                                    a->stepValues[seq][step][out] = (int16_t)value;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Match "debugOutputBus" (optional)
    if (parse.matchName("debugOutputBus")) {
        int numBuses = 0;
        if (parse.numberOfArrayElements(numBuses)) {
            for (int i = 0; i < numBuses && i < 12; i++) {
                int bus;
                if (parse.number(bus)) {
                    a->debugOutputBus[i] = bus;
                }
            }
        }
    }
    
    // After deserialization, sync debug array from current parameter values
    // (in case parameters were loaded but custom data wasn't)
    for (int i = 0; i < 12; i++) {
        a->debugOutputBus[i] = self->v[kParamSeq1Out1 + i];
    }
    
    return true;
}

// Factory
extern "C" {

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V','S','E','Q'),
    .name = "VSeq",
    .description = "4-channel 16-step sequencer with clock/reset",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,  // Note: step callback processes audio
    .draw = draw,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagUtility,
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
    .serialise = serialise,
    .deserialise = deserialise,
    .midiSysEx = NULL
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : NULL);
        default:
            return 0;
    }
}

} // extern "C"
