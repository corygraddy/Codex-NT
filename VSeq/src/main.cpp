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
    // Sequencer data: 4 sequencers × 32 steps × 3 outputs
    int16_t stepValues[4][32][3];
    
    // Step modes: 0=normal, 1=2-ratchet, 2=3-ratchet, 3=4-ratchet, 4=2-repeat, 5=3-repeat, 6=4-repeat
    uint8_t stepMode[4][32];
    
    // Gate sequencer data (replaces seq 3): 6 tracks × 32 steps
    bool gateSteps[6][32];
    
    // Sequencer state
    int currentStep[4];         // Current step for each sequencer (0-31)
    bool pingpongForward[4];    // Direction state for pingpong mode
    int clockDivCounter[4];     // Counter for clock division
    int clockMultCounter[4];    // Counter for clock multiplication (internal ticks)
    int ratchetCounter[4];      // Counter for ratchets within a step
    int repeatCounter[4];       // Counter for step repeats
    int section1Counter[4];     // Track section 1 repeat count
    int section2Counter[4];     // Track section 2 repeat count
    bool inSection2[4];         // Which section is currently playing
    
    // Gate sequencer state (6 tracks)
    int gateCurrentStep[6];     // Current step for each gate track (0-31)
    bool gatePingpongForward[6]; // Direction state for pingpong mode
    int gateClockDivCounter[6]; // Counter for clock division
    int gateClockMultCounter[6]; // Counter for clock multiplication (internal ticks)
    int gateSwingCounter[6];    // Counter for swing timing
    int gateSection1Counter[6]; // Track section 1 repeat count
    int gateSection2Counter[6]; // Track section 2 repeat count
    bool gateInSection2[6];     // Which section is currently playing
    bool gateInFill[6];         // Whether we're in the fill section
    int gateTriggerCounter[6];  // Countdown for trigger pulse duration
    bool gateTriggered[6];      // Whether gate was just triggered this step
    
    // Edge detection
    float lastClockIn;
    float lastResetIn;
    
    // UI state
    int selectedStep;           // 0-31
    int selectedSeq;            // 0-3 (which sequencer to view/edit)
    int selectedTrack;          // 0-5 (for gate sequencer)
    int lastSelectedStep;       // Track when step changes to update pots
    uint16_t lastButton4State;  // For debouncing button 4
    uint16_t lastEncoderRButton; // For debouncing right encoder button
    float lastPotLValue;        // Track left pot position for relative movement
    bool potCaught[3];          // Track if each pot has caught the step value
    
    // Debug: track actual output bus assignments
    int debugOutputBus[12];
    
    VSeq() {
        // Initialize step values to test patterns (visible voltages)
        // Each sequencer gets different voltage levels for testing
        for (int seq = 0; seq < 4; seq++) {
            for (int step = 0; step < 32; step++) {
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
                stepMode[seq][step] = 0;  // Normal mode
            }
            currentStep[seq] = 0;
            pingpongForward[seq] = true;
            clockDivCounter[seq] = 0;
            clockMultCounter[seq] = 0;
            ratchetCounter[seq] = 0;
            repeatCounter[seq] = 0;
            section1Counter[seq] = 0;
            section2Counter[seq] = 0;
            inSection2[seq] = false;
        }
        
        lastClockIn = 0.0f;
        lastResetIn = 0.0f;
        selectedStep = 0;
        selectedSeq = 0;
        selectedTrack = 0;
        lastSelectedStep = 0;
        lastButton4State = 0;
        lastEncoderRButton = 0;
        lastPotLValue = 0.5f;
        potCaught[0] = false;
        potCaught[1] = false;
        potCaught[2] = false;
        
        // Initialize gate sequencer
        for (int track = 0; track < 6; track++) {
            for (int step = 0; step < 32; step++) {
                gateSteps[track][step] = false;
            }
            gateCurrentStep[track] = 0;
            gatePingpongForward[track] = true;
            gateClockDivCounter[track] = 0;
            gateClockMultCounter[track] = 0;
            gateSwingCounter[track] = 0;
            gateSection1Counter[track] = 0;
            gateSection2Counter[track] = 0;
            gateInSection2[track] = false;
            gateInFill[track] = false;
            gateTriggerCounter[track] = 0;
            gateTriggered[track] = false;
        }
        
        for (int i = 0; i < 12; i++) {
            debugOutputBus[i] = 0;
        }
    }
    
    // Advance sequencer to next step based on direction, with section looping
    void advanceSequencer(int seq, int direction, int stepCount, int splitPoint, int sec1Reps, int sec2Reps) {
        if (direction == 0) {
            // Forward
            currentStep[seq]++;
            
            // Check if we've reached the end of a section
            if (!inSection2[seq]) {
                // In section 1
                if (currentStep[seq] >= splitPoint) {
                    section1Counter[seq]++;
                    if (section1Counter[seq] >= sec1Reps) {
                        // Move to section 2
                        inSection2[seq] = true;
                        section1Counter[seq] = 0;
                    } else {
                        // Repeat section 1
                        currentStep[seq] = 0;
                    }
                }
            } else {
                // In section 2
                if (currentStep[seq] >= stepCount) {
                    section2Counter[seq]++;
                    if (section2Counter[seq] >= sec2Reps) {
                        // Loop back to section 1
                        inSection2[seq] = false;
                        section2Counter[seq] = 0;
                        currentStep[seq] = 0;
                    } else {
                        // Repeat section 2
                        currentStep[seq] = splitPoint;
                    }
                }
            }
        } else if (direction == 1) {
            // Backward
            currentStep[seq]--;
            
            // Check if we've reached the start of a section
            if (inSection2[seq]) {
                // In section 2
                if (currentStep[seq] < splitPoint) {
                    section2Counter[seq]++;
                    if (section2Counter[seq] >= sec2Reps) {
                        // Move to section 1
                        inSection2[seq] = false;
                        section2Counter[seq] = 0;
                    } else {
                        // Repeat section 2
                        currentStep[seq] = stepCount - 1;
                    }
                }
            } else {
                // In section 1
                if (currentStep[seq] < 0) {
                    section1Counter[seq]++;
                    if (section1Counter[seq] >= sec1Reps) {
                        // Move to section 2
                        inSection2[seq] = true;
                        section1Counter[seq] = 0;
                        currentStep[seq] = stepCount - 1;
                    } else {
                        // Repeat section 1
                        currentStep[seq] = splitPoint - 1;
                    }
                }
            }
        } else {
            // Pingpong
            if (pingpongForward[seq]) {
                currentStep[seq]++;
                if (currentStep[seq] >= stepCount) {
                    currentStep[seq] = stepCount - 1;
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
            currentStep[seq] = 31;
        } else {
            // Forward and Pingpong: start at step 0
            currentStep[seq] = 0;
        }
        pingpongForward[seq] = true;
        clockDivCounter[seq] = 0;
        ratchetCounter[seq] = 0;
        repeatCounter[seq] = 0;
        section1Counter[seq] = 0;
        section2Counter[seq] = 0;
        inSection2[seq] = false;
    }
};

// Helper function to set a pixel in NT_screen
// Screen is 256x64, stored as 128x64 bytes (2 pixels per byte, 4-bit grayscale)
inline void setPixel(int x, int y, int brightness) {
    if (x < 0 || x >= 256 || y < 0 || y >= 64) return;
    
    int byteIndex = (y * 128) + (x / 2);
    int pixelShift = (x & 1) ? 0 : 4;  // Even pixels in high nibble, odd in low
    
    // Clear the nibble and set new value
    NT_screen[byteIndex] = (NT_screen[byteIndex] & (0x0F << (4 - pixelShift))) | ((brightness & 0x0F) << pixelShift);
}

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
    // Sequencer 4 outputs (now gate sequencer - reuse for gate outputs)
    kParamSeq4Out1,
    kParamSeq4Out2,
    kParamSeq4Out3,
    // Per-sequencer parameters
    kParamSeq1ClockDiv,
    kParamSeq1Direction,
    kParamSeq1StepCount,
    kParamSeq1SplitPoint,
    kParamSeq1Section1Reps,
    kParamSeq1Section2Reps,
    kParamSeq2ClockDiv,
    kParamSeq2Direction,
    kParamSeq2StepCount,
    kParamSeq2SplitPoint,
    kParamSeq2Section1Reps,
    kParamSeq2Section2Reps,
    kParamSeq3ClockDiv,
    kParamSeq3Direction,
    kParamSeq3StepCount,
    kParamSeq3SplitPoint,
    kParamSeq3Section1Reps,
    kParamSeq3Section2Reps,
    // Gate Track 1 parameters
    kParamGate1Out,
    kParamGate1Run,
    kParamGate1Length,
    kParamGate1Direction,
    kParamGate1ClockDiv,
    kParamGate1Swing,
    kParamGate1SplitPoint,
    kParamGate1Section1Reps,
    kParamGate1Section2Reps,
    kParamGate1FillStart,
    // Gate Track 2 parameters
    kParamGate2Out,
    kParamGate2Run,
    kParamGate2Length,
    kParamGate2Direction,
    kParamGate2ClockDiv,
    kParamGate2Swing,
    kParamGate2SplitPoint,
    kParamGate2Section1Reps,
    kParamGate2Section2Reps,
    kParamGate2FillStart,
    // Gate Track 3 parameters
    kParamGate3Out,
    kParamGate3Run,
    kParamGate3Length,
    kParamGate3Direction,
    kParamGate3ClockDiv,
    kParamGate3Swing,
    kParamGate3SplitPoint,
    kParamGate3Section1Reps,
    kParamGate3Section2Reps,
    kParamGate3FillStart,
    // Gate Track 4 parameters
    kParamGate4Out,
    kParamGate4Run,
    kParamGate4Length,
    kParamGate4Direction,
    kParamGate4ClockDiv,
    kParamGate4Swing,
    kParamGate4SplitPoint,
    kParamGate4Section1Reps,
    kParamGate4Section2Reps,
    kParamGate4FillStart,
    // Gate Track 5 parameters
    kParamGate5Out,
    kParamGate5Run,
    kParamGate5Length,
    kParamGate5Direction,
    kParamGate5ClockDiv,
    kParamGate5Swing,
    kParamGate5SplitPoint,
    kParamGate5Section1Reps,
    kParamGate5Section2Reps,
    kParamGate5FillStart,
    // Gate Track 6 parameters
    kParamGate6Out,
    kParamGate6Run,
    kParamGate6Length,
    kParamGate6Direction,
    kParamGate6ClockDiv,
    kParamGate6Swing,
    kParamGate6SplitPoint,
    kParamGate6Section1Reps,
    kParamGate6Section2Reps,
    kParamGate6FillStart,
    kNumParameters
};

// String arrays for enum parameters
static const char* const divisionStrings[] = {
    "/16", "/8", "/4", "/2", "x1", "x2", "x4", "x8", "x16", NULL
};

static const char* const directionStrings[] = {
    "Forward", "Backward", "Pingpong", NULL
};

// Parameter name strings (must be static to persist)
static char seq1DivName[] = "Seq 1 Clock Div";
static char seq1DirName[] = "Seq 1 Direction";
static char seq1StepName[] = "Seq 1 Steps";
static char seq1SplitName[] = "Seq 1 Split Point";
static char seq1Sec1Name[] = "Seq 1 Sec1 Reps";
static char seq1Sec2Name[] = "Seq 1 Sec2 Reps";
static char seq2DivName[] = "Seq 2 Clock Div";
static char seq2DirName[] = "Seq 2 Direction";
static char seq2StepName[] = "Seq 2 Steps";
static char seq2SplitName[] = "Seq 2 Split Point";
static char seq2Sec1Name[] = "Seq 2 Sec1 Reps";
static char seq2Sec2Name[] = "Seq 2 Sec2 Reps";
static char seq3DivName[] = "Seq 3 Clock Div";
static char seq3DirName[] = "Seq 3 Direction";
static char seq3StepName[] = "Seq 3 Steps";
static char seq3SplitName[] = "Seq 3 Split Point";
static char seq3Sec1Name[] = "Seq 3 Sec1 Reps";
static char seq3Sec2Name[] = "Seq 3 Sec2 Reps";
static char seq4DivName[] = "Seq 4 Clock Div";
static char seq4DirName[] = "Seq 4 Direction";
static char seq4StepName[] = "Seq 4 Steps";
static char seq4SplitName[] = "Seq 4 Split Point";
static char seq4Sec1Name[] = "Seq 4 Sec1 Reps";
static char seq4Sec2Name[] = "Seq 4 Sec2 Reps";

// Gate track parameter names
static char gate1OutName[] = "Gate 1 Out";
static char gate1RunName[] = "Gate 1 Run";
static char gate1LenName[] = "Gate 1 Length";
static char gate1DirName[] = "Gate 1 Direction";
static char gate1DivName[] = "Gate 1 ClockDiv";
static char gate1SwingName[] = "Gate 1 Swing";
static char gate1SplitName[] = "Gate 1 Split";
static char gate1Sec1Name[] = "Gate 1 Sec1 Reps";
static char gate1Sec2Name[] = "Gate 1 Sec2 Reps";
static char gate1FillName[] = "Gate 1 Fill Start";

static char gate2OutName[] = "Gate 2 Out";
static char gate2RunName[] = "Gate 2 Run";
static char gate2LenName[] = "Gate 2 Length";
static char gate2DirName[] = "Gate 2 Direction";
static char gate2DivName[] = "Gate 2 ClockDiv";
static char gate2SwingName[] = "Gate 2 Swing";
static char gate2SplitName[] = "Gate 2 Split";
static char gate2Sec1Name[] = "Gate 2 Sec1 Reps";
static char gate2Sec2Name[] = "Gate 2 Sec2 Reps";
static char gate2FillName[] = "Gate 2 Fill Start";

static char gate3OutName[] = "Gate 3 Out";
static char gate3RunName[] = "Gate 3 Run";
static char gate3LenName[] = "Gate 3 Length";
static char gate3DirName[] = "Gate 3 Direction";
static char gate3DivName[] = "Gate 3 ClockDiv";
static char gate3SwingName[] = "Gate 3 Swing";
static char gate3SplitName[] = "Gate 3 Split";
static char gate3Sec1Name[] = "Gate 3 Sec1 Reps";
static char gate3Sec2Name[] = "Gate 3 Sec2 Reps";
static char gate3FillName[] = "Gate 3 Fill Start";

static char gate4OutName[] = "Gate 4 Out";
static char gate4RunName[] = "Gate 4 Run";
static char gate4LenName[] = "Gate 4 Length";
static char gate4DirName[] = "Gate 4 Direction";
static char gate4DivName[] = "Gate 4 ClockDiv";
static char gate4SwingName[] = "Gate 4 Swing";
static char gate4SplitName[] = "Gate 4 Split";
static char gate4Sec1Name[] = "Gate 4 Sec1 Reps";
static char gate4Sec2Name[] = "Gate 4 Sec2 Reps";
static char gate4FillName[] = "Gate 4 Fill Start";

static char gate5OutName[] = "Gate 5 Out";
static char gate5RunName[] = "Gate 5 Run";
static char gate5LenName[] = "Gate 5 Length";
static char gate5DirName[] = "Gate 5 Direction";
static char gate5DivName[] = "Gate 5 ClockDiv";
static char gate5SwingName[] = "Gate 5 Swing";
static char gate5SplitName[] = "Gate 5 Split";
static char gate5Sec1Name[] = "Gate 5 Sec1 Reps";
static char gate5Sec2Name[] = "Gate 5 Sec2 Reps";
static char gate5FillName[] = "Gate 5 Fill Start";

static char gate6OutName[] = "Gate 6 Out";
static char gate6RunName[] = "Gate 6 Run";
static char gate6LenName[] = "Gate 6 Length";
static char gate6DirName[] = "Gate 6 Direction";
static char gate6DivName[] = "Gate 6 ClockDiv";
static char gate6SwingName[] = "Gate 6 Swing";
static char gate6SplitName[] = "Gate 6 Split";
static char gate6Sec1Name[] = "Gate 6 Sec1 Reps";
static char gate6Sec2Name[] = "Gate 6 Sec2 Reps";
static char gate6FillName[] = "Gate 6 Fill Start";

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
        parameters[paramIdx].min = 0;
        parameters[paramIdx].max = 28;
        parameters[paramIdx].def = 0;
        parameters[paramIdx].unit = kNT_unitCvOutput;
        parameters[paramIdx].scaling = kNT_scalingNone;
    }
    
    // Sequencer configuration parameters (seq 1-3 only now)
    const char* divNames[] = {seq1DivName, seq2DivName, seq3DivName};
    const char* dirNames[] = {seq1DirName, seq2DirName, seq3DirName};
    const char* stepNames[] = {seq1StepName, seq2StepName, seq3StepName};
    const char* splitNames[] = {seq1SplitName, seq2SplitName, seq3SplitName};
    const char* sec1Names[] = {seq1Sec1Name, seq2Sec1Name, seq3Sec1Name};
    const char* sec2Names[] = {seq1Sec2Name, seq2Sec2Name, seq3Sec2Name};
    
    for (int seq = 0; seq < 3; seq++) {
        int divParam = kParamSeq1ClockDiv + (seq * 6);
        int dirParam = kParamSeq1Direction + (seq * 6);
        int stepParam = kParamSeq1StepCount + (seq * 6);
        int splitParam = kParamSeq1SplitPoint + (seq * 6);
        int sec1Param = kParamSeq1Section1Reps + (seq * 6);
        int sec2Param = kParamSeq1Section2Reps + (seq * 6);
        
        // Clock Division parameter
        parameters[divParam].name = divNames[seq];
        parameters[divParam].min = 0;
        parameters[divParam].max = 8;  // /16, /8, /4, /2, x1, x2, x4, x8, x16
        parameters[divParam].def = 4;  // x1
        parameters[divParam].unit = kNT_unitEnum;
        parameters[divParam].scaling = kNT_scalingNone;
        parameters[divParam].enumStrings = divisionStrings;
        
        // Direction parameter
        parameters[dirParam].name = dirNames[seq];
        parameters[dirParam].min = 0;
        parameters[dirParam].max = 2;  // Forward, Backward, Pingpong
        parameters[dirParam].def = 0;  // Forward
        parameters[dirParam].unit = kNT_unitEnum;
        parameters[dirParam].scaling = kNT_scalingNone;
        parameters[dirParam].enumStrings = directionStrings;
        
        // Step Count parameter
        parameters[stepParam].name = stepNames[seq];
        parameters[stepParam].min = 1;
        parameters[stepParam].max = 32;
        parameters[stepParam].def = 32;
        parameters[stepParam].unit = kNT_unitNone;
        parameters[stepParam].scaling = kNT_scalingNone;
        
        // Split Point parameter
        parameters[splitParam].name = splitNames[seq];
        parameters[splitParam].min = 1;
        parameters[splitParam].max = 31;
        parameters[splitParam].def = 16;
        parameters[splitParam].unit = kNT_unitNone;
        parameters[splitParam].scaling = kNT_scalingNone;
        
        // Section 1 Repeats parameter
        parameters[sec1Param].name = sec1Names[seq];
        parameters[sec1Param].min = 1;
        parameters[sec1Param].max = 99;
        parameters[sec1Param].def = 1;
        parameters[sec1Param].unit = kNT_unitNone;
        parameters[sec1Param].scaling = kNT_scalingNone;
        
        // Section 2 Repeats parameter
        parameters[sec2Param].name = sec2Names[seq];
        parameters[sec2Param].min = 1;
        parameters[sec2Param].max = 99;
        parameters[sec2Param].def = 1;
        parameters[sec2Param].unit = kNT_unitNone;
        parameters[sec2Param].scaling = kNT_scalingNone;
    }
    
    // Gate Track parameters (6 tracks, 10 parameters each)
    const char* gateOutNames[] = {gate1OutName, gate2OutName, gate3OutName, gate4OutName, gate5OutName, gate6OutName};
    const char* gateRunNames[] = {gate1RunName, gate2RunName, gate3RunName, gate4RunName, gate5RunName, gate6RunName};
    const char* gateLenNames[] = {gate1LenName, gate2LenName, gate3LenName, gate4LenName, gate5LenName, gate6LenName};
    const char* gateDirNames[] = {gate1DirName, gate2DirName, gate3DirName, gate4DirName, gate5DirName, gate6DirName};
    const char* gateDivNames[] = {gate1DivName, gate2DivName, gate3DivName, gate4DivName, gate5DivName, gate6DivName};
    const char* gateSwingNames[] = {gate1SwingName, gate2SwingName, gate3SwingName, gate4SwingName, gate5SwingName, gate6SwingName};
    const char* gateSplitNames[] = {gate1SplitName, gate2SplitName, gate3SplitName, gate4SplitName, gate5SplitName, gate6SplitName};
    const char* gateSec1Names[] = {gate1Sec1Name, gate2Sec1Name, gate3Sec1Name, gate4Sec1Name, gate5Sec1Name, gate6Sec1Name};
    const char* gateSec2Names[] = {gate1Sec2Name, gate2Sec2Name, gate3Sec2Name, gate4Sec2Name, gate5Sec2Name, gate6Sec2Name};
    const char* gateFillNames[] = {gate1FillName, gate2FillName, gate3FillName, gate4FillName, gate5FillName, gate6FillName};
    
    for (int track = 0; track < 6; track++) {
        int outParam = kParamGate1Out + (track * 10);
        int runParam = kParamGate1Run + (track * 10);
        int lenParam = kParamGate1Length + (track * 10);
        int dirParam = kParamGate1Direction + (track * 10);
        int divParam = kParamGate1ClockDiv + (track * 10);
        int swingParam = kParamGate1Swing + (track * 10);
        int splitParam = kParamGate1SplitPoint + (track * 10);
        int sec1Param = kParamGate1Section1Reps + (track * 10);
        int sec2Param = kParamGate1Section2Reps + (track * 10);
        int fillParam = kParamGate1FillStart + (track * 10);
        
        parameters[outParam].name = gateOutNames[track];
        parameters[outParam].min = 0;
        parameters[outParam].max = 28;
        parameters[outParam].def = 0;
        parameters[outParam].unit = kNT_unitCvOutput;
        parameters[outParam].scaling = kNT_scalingNone;
        
        parameters[runParam].name = gateRunNames[track];
        parameters[runParam].min = 0;
        parameters[runParam].max = 1;
        parameters[runParam].def = 1;
        parameters[runParam].unit = kNT_unitNone;
        parameters[runParam].scaling = kNT_scalingNone;
        
        parameters[lenParam].name = gateLenNames[track];
        parameters[lenParam].min = 1;
        parameters[lenParam].max = 32;
        parameters[lenParam].def = 32;
        parameters[lenParam].unit = kNT_unitNone;
        parameters[lenParam].scaling = kNT_scalingNone;
        
        parameters[dirParam].name = gateDirNames[track];
        parameters[dirParam].min = 0;
        parameters[dirParam].max = 2;
        parameters[dirParam].def = 0;
        parameters[dirParam].unit = kNT_unitEnum;
        parameters[dirParam].scaling = kNT_scalingNone;
        parameters[dirParam].enumStrings = directionStrings;
        
        parameters[divParam].name = gateDivNames[track];
        parameters[divParam].min = 0;
        parameters[divParam].max = 8;
        parameters[divParam].def = 4;
        parameters[divParam].unit = kNT_unitEnum;
        parameters[divParam].scaling = kNT_scalingNone;
        parameters[divParam].enumStrings = divisionStrings;
        
        parameters[swingParam].name = gateSwingNames[track];
        parameters[swingParam].min = 0;
        parameters[swingParam].max = 100;
        parameters[swingParam].def = 0;
        parameters[swingParam].unit = kNT_unitNone;
        parameters[swingParam].scaling = kNT_scalingNone;
        
        parameters[splitParam].name = gateSplitNames[track];
        parameters[splitParam].min = 0;
        parameters[splitParam].max = 31;
        parameters[splitParam].def = 0;
        parameters[splitParam].unit = kNT_unitNone;
        parameters[splitParam].scaling = kNT_scalingNone;
        
        parameters[sec1Param].name = gateSec1Names[track];
        parameters[sec1Param].min = 1;
        parameters[sec1Param].max = 99;
        parameters[sec1Param].def = 1;
        parameters[sec1Param].unit = kNT_unitNone;
        parameters[sec1Param].scaling = kNT_scalingNone;
        
        parameters[sec2Param].name = gateSec2Names[track];
        parameters[sec2Param].min = 1;
        parameters[sec2Param].max = 99;
        parameters[sec2Param].def = 1;
        parameters[sec2Param].unit = kNT_unitNone;
        parameters[sec2Param].scaling = kNT_scalingNone;
        
        parameters[fillParam].name = gateFillNames[track];
        parameters[fillParam].min = 1;
        parameters[fillParam].max = 32;
        parameters[fillParam].def = 32;  // Default: no fill (starts at last step)
        parameters[fillParam].unit = kNT_unitNone;
        parameters[fillParam].scaling = kNT_scalingNone;
    }
}

// Parameter pages
static uint8_t paramPageInputs[] = { kParamClockIn, kParamResetIn, 0 };
static uint8_t paramPageSeq1Out[] = { kParamSeq1Out1, kParamSeq1Out2, kParamSeq1Out3, 0 };
static uint8_t paramPageSeq2Out[] = { kParamSeq2Out1, kParamSeq2Out2, kParamSeq2Out3, 0 };
static uint8_t paramPageSeq3Out[] = { kParamSeq3Out1, kParamSeq3Out2, kParamSeq3Out3, 0 };
static uint8_t paramPageSeq1Params[] = { kParamSeq1ClockDiv, kParamSeq1Direction, kParamSeq1StepCount, kParamSeq1SplitPoint, kParamSeq1Section1Reps, kParamSeq1Section2Reps, 0 };
static uint8_t paramPageSeq2Params[] = { kParamSeq2ClockDiv, kParamSeq2Direction, kParamSeq2StepCount, kParamSeq2SplitPoint, kParamSeq2Section1Reps, kParamSeq2Section2Reps, 0 };
static uint8_t paramPageSeq3Params[] = { kParamSeq3ClockDiv, kParamSeq3Direction, kParamSeq3StepCount, kParamSeq3SplitPoint, kParamSeq3Section1Reps, kParamSeq3Section2Reps, 0 };
static uint8_t paramPageGate1[] = { kParamGate1Out, kParamGate1Run, kParamGate1Length, kParamGate1Direction, kParamGate1ClockDiv, kParamGate1Swing, kParamGate1SplitPoint, kParamGate1Section1Reps, kParamGate1Section2Reps, kParamGate1FillStart, 0 };
static uint8_t paramPageGate2[] = { kParamGate2Out, kParamGate2Run, kParamGate2Length, kParamGate2Direction, kParamGate2ClockDiv, kParamGate2Swing, kParamGate2SplitPoint, kParamGate2Section1Reps, kParamGate2Section2Reps, kParamGate2FillStart, 0 };
static uint8_t paramPageGate3[] = { kParamGate3Out, kParamGate3Run, kParamGate3Length, kParamGate3Direction, kParamGate3ClockDiv, kParamGate3Swing, kParamGate3SplitPoint, kParamGate3Section1Reps, kParamGate3Section2Reps, kParamGate3FillStart, 0 };
static uint8_t paramPageGate4[] = { kParamGate4Out, kParamGate4Run, kParamGate4Length, kParamGate4Direction, kParamGate4ClockDiv, kParamGate4Swing, kParamGate4SplitPoint, kParamGate4Section1Reps, kParamGate4Section2Reps, kParamGate4FillStart, 0 };
static uint8_t paramPageGate5[] = { kParamGate5Out, kParamGate5Run, kParamGate5Length, kParamGate5Direction, kParamGate5ClockDiv, kParamGate5Swing, kParamGate5SplitPoint, kParamGate5Section1Reps, kParamGate5Section2Reps, kParamGate5FillStart, 0 };
static uint8_t paramPageGate6[] = { kParamGate6Out, kParamGate6Run, kParamGate6Length, kParamGate6Direction, kParamGate6ClockDiv, kParamGate6Swing, kParamGate6SplitPoint, kParamGate6Section1Reps, kParamGate6Section2Reps, kParamGate6FillStart, 0 };

static _NT_parameterPage pageArray[] = {
    { .name = "Inputs", .numParams = 2, .params = paramPageInputs },
    { .name = "Seq 1 Outs", .numParams = 3, .params = paramPageSeq1Out },
    { .name = "Seq 2 Outs", .numParams = 3, .params = paramPageSeq2Out },
    { .name = "Seq 3 Outs", .numParams = 3, .params = paramPageSeq3Out },
    { .name = "Seq 1 Params", .numParams = 6, .params = paramPageSeq1Params },
    { .name = "Seq 2 Params", .numParams = 6, .params = paramPageSeq2Params },
    { .name = "Seq 3 Params", .numParams = 6, .params = paramPageSeq3Params },
    { .name = "Trig Track 1", .numParams = 10, .params = paramPageGate1 },
    { .name = "Trig Track 2", .numParams = 10, .params = paramPageGate2 },
    { .name = "Trig Track 3", .numParams = 10, .params = paramPageGate3 },
    { .name = "Trig Track 4", .numParams = 10, .params = paramPageGate4 },
    { .name = "Trig Track 5", .numParams = 10, .params = paramPageGate5 },
    { .name = "Trig Track 6", .numParams = 10, .params = paramPageGate6 }
};

static _NT_parameterPages pages = {
    .numPages = 13,
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
        int divParam = kParamSeq1ClockDiv + (seq * 6);
        int dirParam = kParamSeq1Direction + (seq * 6);
        int stepParam = kParamSeq1StepCount + (seq * 6);
        int splitParam = kParamSeq1SplitPoint + (seq * 6);
        int sec1Param = kParamSeq1Section1Reps + (seq * 6);
        int sec2Param = kParamSeq1Section2Reps + (seq * 6);
        
        int clockDiv = self->v[divParam];   // 0-8 (/16 to x16)
        int direction = self->v[dirParam];  // 0=Forward, 1=Backward, 2=Pingpong
        int stepCount = self->v[stepParam]; // 1-32
        int splitPoint = self->v[splitParam]; // 1-31
        int sec1Reps = self->v[sec1Param];  // 1-99
        int sec2Reps = self->v[sec2Param];  // 1-99
        
        // Reset handling
        if (resetTrig) {
            a->resetSequencer(seq);
        }
        
        // Clock handling - advance one step per clock
        if (clockTrig) {
            a->advanceSequencer(seq, direction, stepCount, splitPoint, sec1Reps, sec2Reps);
        }
        
        // Clamp current step to step count (safety check)
        if (a->currentStep[seq] >= stepCount) {
            a->currentStep[seq] = stepCount - 1;
        }
        
        // Output current step values to all output buses
        int step = a->currentStep[seq];
        for (int out = 0; out < 3; out++) {
            int paramIdx = kParamSeq1Out1 + (seq * 3) + out;
            int outputBus = self->v[paramIdx];  // 0 = none, 1-28 = bus 0-27
            
            // Store actual bus assignment for debug
            int debugIdx = seq * 3 + out;
            a->debugOutputBus[debugIdx] = outputBus;  // Store as 0-28
            
            if (outputBus > 0 && outputBus <= 28) {
                int16_t value = a->stepValues[seq][step][out];
                // Convert from int16_t range to 0.0-1.0
                float outputValue = (value + 32768) / 65535.0f;
                
                // Write to all frames in the output bus
                float* outBus = busFrames + ((outputBus - 1) * numFrames);
                for (int frame = 0; frame < numFrames; frame++) {
                    outBus[frame] = outputValue;
                }
            }
        }
    }
    
    // Process gate sequencer (6 tracks)
    for (int track = 0; track < 6; track++) {
        int outParam = kParamGate1Out + (track * 10);
        int runParam = kParamGate1Run + (track * 10);
        int lenParam = kParamGate1Length + (track * 10);
        int dirParam = kParamGate1Direction + (track * 10);
        int divParam = kParamGate1ClockDiv + (track * 10);
        int splitParam = kParamGate1SplitPoint + (track * 10);
        int sec1Param = kParamGate1Section1Reps + (track * 10);
        int sec2Param = kParamGate1Section2Reps + (track * 10);
        int fillParam = kParamGate1FillStart + (track * 10);
        
        int outputBus = self->v[outParam];     // 0 = none, 1-28 = bus 0-27
        int isRunning = self->v[runParam];     // 0 = stopped, 1 = running
        int trackLength = self->v[lenParam];   // 1-32
        int direction = self->v[dirParam];     // 0=Forward, 1=Backward, 2=Pingpong
        int clockDiv = self->v[divParam];      // 0-8 (/16 to x16)
        int splitPoint = self->v[splitParam];  // 0-31 (0 = no split)
        int sec1Reps = self->v[sec1Param];     // 1-99
        int sec2Reps = self->v[sec2Param];     // 1-99
        int fillStart = self->v[fillParam];    // 1-32 (step where fill replaces section 1 on last rep)
        
        // Skip if not running
        if (isRunning == 0) continue;
        
        // Reset handling
        if (resetTrig) {
            a->gateCurrentStep[track] = 0;
            a->gatePingpongForward[track] = true;
            a->gateClockDivCounter[track] = 0;
            a->gateSwingCounter[track] = 0;
            a->gateSection1Counter[track] = 0;
            a->gateSection2Counter[track] = 0;
            a->gateInSection2[track] = false;
            a->gateInFill[track] = false;
        }
        
        // Clock handling - advance one step per clock
        if (clockTrig) {
            // Determine section boundaries
            int section1End = (splitPoint > 0 && splitPoint < trackLength) ? splitPoint : trackLength;
            
            if (direction == 0) {  // Forward
                        a->gateCurrentStep[track]++;
                        
                        // Check for fill trigger on last repetition of section 1
                        if (!a->gateInSection2[track] && 
                            splitPoint > 0 && 
                            fillStart < splitPoint &&
                            a->gateSection1Counter[track] == sec1Reps - 1 &&
                            a->gateCurrentStep[track] >= fillStart) {
                            // Fill triggered! Jump to section 2
                            a->gateSection1Counter[track] = 0;
                            a->gateInSection2[track] = true;
                            a->gateCurrentStep[track] = splitPoint;
                        }
                        // Check if we've crossed a section boundary
                        else if (!a->gateInSection2[track] && a->gateCurrentStep[track] >= section1End) {
                            // Completed section 1
                            a->gateSection1Counter[track]++;
                            if (a->gateSection1Counter[track] >= sec1Reps) {
                                // Move to section 2
                                a->gateSection1Counter[track] = 0;
                                a->gateInSection2[track] = true;
                                if (splitPoint > 0) {
                                    a->gateCurrentStep[track] = splitPoint;
                                } else {
                                    a->gateCurrentStep[track] = 0;
                                }
                            } else {
                                // Repeat section 1
                                a->gateCurrentStep[track] = 0;
                            }
                        } else if (a->gateInSection2[track] && a->gateCurrentStep[track] >= trackLength) {
                            // Completed section 2
                            a->gateSection2Counter[track]++;
                            if (a->gateSection2Counter[track] >= sec2Reps) {
                                // Back to section 1
                                a->gateSection2Counter[track] = 0;
                                a->gateInSection2[track] = false;
                            }
                            a->gateCurrentStep[track] = (splitPoint > 0) ? splitPoint : 0;
                            if (!a->gateInSection2[track]) {
                                a->gateCurrentStep[track] = 0;
                            }
                        }
                    } else if (direction == 1) {  // Backward
                        a->gateCurrentStep[track]--;
                        
                        if (a->gateInSection2[track] && a->gateCurrentStep[track] < splitPoint) {
                            a->gateSection2Counter[track]++;
                            if (a->gateSection2Counter[track] >= sec2Reps) {
                                a->gateSection2Counter[track] = 0;
                                a->gateInSection2[track] = false;
                                a->gateCurrentStep[track] = section1End - 1;
                            } else {
                                a->gateCurrentStep[track] = trackLength - 1;
                            }
                        } else if (!a->gateInSection2[track] && a->gateCurrentStep[track] < 0) {
                            a->gateSection1Counter[track]++;
                            if (a->gateSection1Counter[track] >= sec1Reps) {
                                a->gateSection1Counter[track] = 0;
                                a->gateInSection2[track] = true;
                                a->gateCurrentStep[track] = trackLength - 1;
                            } else {
                                a->gateCurrentStep[track] = section1End - 1;
                            }
                        }
                    } else if (direction == 2) {  // Pingpong
                        if (a->gatePingpongForward[track]) {
                            a->gateCurrentStep[track]++;
                            if (a->gateCurrentStep[track] >= trackLength) {
                                a->gateCurrentStep[track] = trackLength - 2;
                                if (a->gateCurrentStep[track] < 0) a->gateCurrentStep[track] = 0;
                                a->gatePingpongForward[track] = false;
                            }
                        } else {
                            a->gateCurrentStep[track]--;
                            if (a->gateCurrentStep[track] < 0) {
                                a->gateCurrentStep[track] = 1;
                                if (a->gateCurrentStep[track] >= trackLength) a->gateCurrentStep[track] = trackLength - 1;
                                a->gatePingpongForward[track] = true;
                            }
                        }
                    }
                    
                // After advancing, mark if current step should trigger
                int currentStep = a->gateCurrentStep[track];
                if (currentStep >= 0 && currentStep < 32 && a->gateSteps[track][currentStep]) {
                    // Gate is active on this step - trigger!
                    a->gateTriggerCounter[track] = 240;  // ~5ms at 48kHz
                }
        }
        
        // Countdown trigger pulses every buffer
        if (a->gateTriggerCounter[track] > 0) {
            a->gateTriggerCounter[track] -= numFrames;  // Countdown by buffer size
            if (a->gateTriggerCounter[track] < 0) {
                a->gateTriggerCounter[track] = 0;
            }
        }
        
        // Output trigger pulse
        if (outputBus > 0 && outputBus <= 28) {
            bool triggerActive = a->gateTriggerCounter[track] > 0;
            
            // Write to all frames in the output bus
            float* outBus = busFrames + ((outputBus - 1) * numFrames);
            for (int frame = 0; frame < numFrames; frame++) {
                outBus[frame] = triggerActive ? 5.0f : 0.0f;  // 5V trigger
            }
        }
    }
}

bool draw(_NT_algorithm* self) {
    VSeq* a = (VSeq*)self;
    
    // Clear screen
    NT_drawShapeI(kNT_rectangle, 0, 0, 256, 64, 0);  // Black background
    
    int seq = a->selectedSeq;  // 0-3
    
    // If seq 3 (4th sequencer), draw gate sequencer instead
    if (seq == 3) {
        // Show track and step info
        char info[32];
        snprintf(info, sizeof(info), "T%d S%d", a->selectedTrack + 1, a->selectedStep + 1);
        NT_drawText(0, 0, info, 255);
        
        // Show gate state for current selection
        bool currentGateState = a->gateSteps[a->selectedTrack][a->selectedStep];
        NT_drawText(60, 0, currentGateState ? "ON" : "off", currentGateState ? 255 : 100);
        
        // 6 tracks × 32 steps
        // Screen: 256px wide, 64px tall
        // Step size: 256/32 = 8px per step
        // Track height: (64-8)/6 = ~9px per track (leave 8px for title)
        
        int stepWidth = 8;
        int trackHeight = 9;
        int startY = 8;
        
        // Determine which page we're on based on selectedTrack (0-5 maps to pages 0-3)
        int currentPage = a->selectedTrack / 2;  // 0-1=page0, 2-3=page1, 4-5=page2
        // Actually, let's use step position: steps 0-7=page0, 8-15=page1, 16-23=page2, 24-31=page3
        currentPage = a->selectedStep / 8;
        
        // Draw page indicators at top:
        // - Show which 8-step group (dotted/solid for steps 0-7, 8-15, 16-23, 24-31)
        // - AND which sequencer we're on (brightness for seq 0-3)
        for (int group = 0; group < 4; group++) {
            int groupWidth = 8 * stepWidth;  // 8 steps per group = 64px
            int barStartX = (group * groupWidth) + (stepWidth / 2);
            int barWidth = groupWidth - stepWidth;
            
            // Determine brightness: bright if this is the current sequencer (seq 3 = gate seq)
            int brightness = (group == seq) ? 255 : 80;
            
            if (group == currentPage) {
                // Current 8-step page: solid line
                NT_drawShapeI(kNT_line, barStartX, 4, barStartX + barWidth - 1, 4, brightness);
            } else {
                // Other 8-step pages: dotted line
                for (int x = barStartX; x < barStartX + barWidth; x += 2) {
                    NT_drawShapeI(kNT_rectangle, x, 4, x, 4, brightness);
                }
            }
        }
        
        for (int track = 0; track < 6; track++) {
            int y = startY + (track * trackHeight);
            
            // Get track parameters
            int lenParam = kParamGate1Length + (track * 10);
            int splitParam = kParamGate1SplitPoint + (track * 10);
            int trackLength = self->v[lenParam];
            int splitPoint = self->v[splitParam];
            int currentStep = a->gateCurrentStep[track];
            
            // Highlight selected track with a line on the left
            if (track == a->selectedTrack) {
                NT_drawShapeI(kNT_line, 0, y, 0, y + trackHeight - 1, 255);
                NT_drawShapeI(kNT_line, 1, y, 1, y + trackHeight - 1, 255);
            }
            
            // Draw split point line if active
            if (splitPoint > 0 && splitPoint < trackLength) {
                int splitX = splitPoint * stepWidth;
                NT_drawShapeI(kNT_line, splitX, y, splitX, y + trackHeight - 1, 200);
            }
            
            for (int step = 0; step < 32; step++) {
                int x = step * stepWidth;
                
                // Determine if this step is active (within track length)
                bool isActive = (step < trackLength);
                
                // Only draw steps that are within the track length
                if (!isActive) continue;  // Skip inactive steps entirely
                
                // Get gate state for this track/step
                bool hasGate = a->gateSteps[track][step];
                
                // Calculate center position
                int centerX = x + (stepWidth / 2);
                int centerY = y + (trackHeight / 2);
                
                // If gate is active, draw filled 5x5 square
                if (hasGate) {
                    // Draw filled 5x5 square (very obvious)
                    NT_drawShapeI(kNT_rectangle, centerX - 2, centerY - 2, centerX + 2, centerY + 2, 255);
                } else {
                    // Just draw center pixel for inactive steps
                    NT_drawShapeI(kNT_rectangle, centerX, centerY, centerX, centerY, 255);
                }
                
                // Draw small box below the current playing step
                if (step == currentStep) {
                    NT_drawShapeI(kNT_rectangle, centerX, centerY + 3, centerX + 1, centerY + 3, 255);
                }
                
                // Highlight selected step (for editing)
                if (step == a->selectedStep && track == a->selectedTrack) {
                    NT_drawShapeI(kNT_line, centerX - 3, centerY - 3, centerX + 3, centerY - 3, 200);  // Top
                    NT_drawShapeI(kNT_line, centerX - 3, centerY + 3, centerX + 3, centerY + 3, 200);  // Bottom
                    NT_drawShapeI(kNT_line, centerX - 3, centerY - 3, centerX - 3, centerY + 3, 200);  // Left
                    NT_drawShapeI(kNT_line, centerX + 3, centerY - 3, centerX + 3, centerY + 3, 200);  // Right
                }
            }
        }
        
        return true;  // Suppress default parameter drawing
    }
    
    // Original CV sequencer view for seq 0-2
    // Get parameters for current sequencer
    int stepParam = kParamSeq1StepCount + (seq * 6);
    int splitParam = kParamSeq1SplitPoint + (seq * 6);
    int stepCount = self->v[stepParam];
    int splitPoint = self->v[splitParam];
    
    // Draw step view
    char title[16];
    snprintf(title, sizeof(title), "SEQ %d", seq + 1);
    NT_drawText(0, 0, title, 255);
    
    // Draw 32 steps in 2 rows of 16
    // Each step gets 3 skinny bars for 3 outputs
    // Screen is 256 wide, divided into 2 rows of 16 steps
    
    int barWidth = 3;   // Width of each bar
    int barSpacing = 1; // Space between bars within a step
    int barsWidth = (3 * barWidth) + (2 * barSpacing);  // Width of 3 bars: 3*3 + 2*1 = 11
    int stepGap = 4;    // Gap after each step (reduced to make room for dots)
    int stepWidth = barsWidth + stepGap;  // Total width per step: 11 + 4 = 15
    int startY = 10;    // Start below title
    int rowHeight = 26; // Height of each row
    int maxBarHeight = 22; // Maximum bar height
    
    for (int step = 0; step < 32; step++) {
        int row = step / 16;     // 0 or 1
        int col = step % 16;     // 0-15
        
        int x = col * stepWidth;
        int y = startY + (row * rowHeight);
        
        // Determine if this step is active
        bool isActive = (step < stepCount);
        int brightness = isActive ? 255 : 40;  // Dim inactive steps
        
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
            NT_drawShapeI(kNT_rectangle, barX, barTopY, barX + barWidth - 1, barBottomY, brightness);
        }
        
        // Draw step indicator dot if this is the current step
        if (step == a->currentStep[seq]) {
            // Draw dot above the middle bar (bar 1)
            int dotX = x + (barWidth + barSpacing);  // Above middle bar
            NT_drawShapeI(kNT_rectangle, dotX, y - 2, dotX + barWidth - 1, y - 1, 255);
        }
        
        // Draw selection underline if this is the selected step
        if (step == a->selectedStep) {
            NT_drawShapeI(kNT_line, x, y + maxBarHeight + 1, x + barsWidth - 1, y + maxBarHeight + 1, 255);
        }
        
        // Draw percentage dots in the gap between steps
        if (col < 15) {  // Don't draw after the last step in each row
            int dotX = x + barsWidth + 2;  // Start of gap area (moved 1px right)
            // 4 dots at 25%, 50%, 75%, 100% of bar height
            int dot25Y = y + maxBarHeight - (maxBarHeight / 4);
            int dot50Y = y + maxBarHeight - (maxBarHeight / 2);
            int dot75Y = y + maxBarHeight - (3 * maxBarHeight / 4);
            int dot100Y = y;
            
            NT_drawShapeI(kNT_rectangle, dotX, dot25Y, dotX, dot25Y, 128);
            NT_drawShapeI(kNT_rectangle, dotX, dot50Y, dotX, dot50Y, 128);
            NT_drawShapeI(kNT_rectangle, dotX, dot75Y, dotX, dot75Y, 128);
            NT_drawShapeI(kNT_rectangle, dotX, dot100Y, dotX, dot100Y, 128);
        }
        
        // Draw triangle between last step of first section and first step of second section
        if (step == (splitPoint - 1) && splitPoint > 0 && splitPoint < stepCount) {
            int boxX = x + barsWidth + 1;  // In the gap after this step
            int boxY = y + maxBarHeight + 3;  // Below the bars
            // Draw small 2x2 box
            NT_drawShapeI(kNT_rectangle, boxX, boxY, boxX + 1, boxY + 1, 255);
        }
    }
    
    // Draw short separator lines at top and bottom of screen between groups of 4 steps
    // Between steps 4-5, 8-9, 12-13
    int separatorY1 = 0;  // Very top of screen
    int separatorY2 = 63; // Very bottom of screen
    int x1 = 4 * stepWidth - (stepGap / 2);
    int x2 = 8 * stepWidth - (stepGap / 2);
    int x3 = 12 * stepWidth - (stepGap / 2);
    
    NT_drawShapeI(kNT_line, x1, separatorY1, x1, separatorY1 + 3, 128);
    NT_drawShapeI(kNT_line, x1, separatorY2 - 3, x1, separatorY2, 128);
    NT_drawShapeI(kNT_line, x2, separatorY1, x2, separatorY1 + 3, 128);
    NT_drawShapeI(kNT_line, x2, separatorY2 - 3, x2, separatorY2, 128);
    NT_drawShapeI(kNT_line, x3, separatorY1, x3, separatorY1 + 3, 128);
    NT_drawShapeI(kNT_line, x3, separatorY2 - 3, x3, separatorY2, 128);
    
    // Draw page indicators at the very top (above step view)
    // 4 bars representing 4 sequencers, centered above groups of 4 steps
    int pageBarY = 4;  // Just below the top separator lines
    int groupWidth = 4 * stepWidth;  // Width of 4 steps including gaps
    for (int i = 0; i < 4; i++) {
        int barStartX = (i * groupWidth) + (stepGap / 2);
        int barEndX = ((i + 1) * groupWidth) - (stepGap / 2) - stepGap;
        int brightness = (i == seq) ? 255 : 80;  // Bright if active, dim otherwise
        NT_drawShapeI(kNT_line, barStartX, pageBarY, barEndX, pageBarY, brightness);
    }
    
    // Draw current step number in top right corner
    char stepNum[4];
    snprintf(stepNum, sizeof(stepNum), "%d", a->selectedStep + 1);
    NT_drawText(248, 0, stepNum, 255);
    
    return true;  // Suppress default parameter line
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    return kNT_potL | kNT_potC | kNT_potR | kNT_encoderL | kNT_encoderR | kNT_encoderButtonR | kNT_button4;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    VSeq* a = (VSeq*)self;
    
    // Left encoder: select sequencer (0-3)
    if (data.encoders[0] != 0) {
        int delta = data.encoders[0];
        int oldSeq = a->selectedSeq;
        a->selectedSeq += delta;
        if (a->selectedSeq < 0) a->selectedSeq = 3;
        if (a->selectedSeq > 3) a->selectedSeq = 0;
        
        // If sequencer changed, clamp selectedStep to new sequencer's length
        if (a->selectedSeq != oldSeq) {
            // Determine new sequencer's length
            int newLength;
            if (a->selectedSeq == 3) {
                // Gate sequencer - get current track's length
                int lenParam = kParamGate1Length + (a->selectedTrack * 10);
                newLength = self->v[lenParam];
            } else {
                // CV sequencer - get step count
                int lengthParam;
                if (a->selectedSeq == 0) lengthParam = kParamSeq1StepCount;
                else if (a->selectedSeq == 1) lengthParam = kParamSeq2StepCount;
                else lengthParam = kParamSeq3StepCount;
                newLength = self->v[lengthParam];
            }
            
            // Clamp selectedStep to new length
            if (a->selectedStep >= newLength) {
                a->selectedStep = newLength - 1;
            }
        }
    }
    
    // Gate sequencer mode (seq 3)
    if (a->selectedSeq == 3) {
        // Left pot: select track (0-5) - direct mapping
        if (data.controls & kNT_potL) {
            float potValue = data.pots[0];
            
            // Map pot value (0.0-1.0) to tracks (0-5)
            // Divide into 6 equal regions
            int newTrack = (int)(potValue * 5.999f);  // 0.0->0, 0.999->5
            if (newTrack < 0) newTrack = 0;
            if (newTrack > 5) newTrack = 5;
            
            // If track changed, clamp selectedStep to new track's length
            if (newTrack != a->selectedTrack) {
                a->selectedTrack = newTrack;
                
                // Get new track's length
                int lenParam = kParamGate1Length + (newTrack * 10);
                int trackLength = self->v[lenParam];  // 1-32
                
                // Clamp selectedStep to track length
                if (a->selectedStep >= trackLength) {
                    a->selectedStep = trackLength - 1;
                }
            }
        }
        
        // Get current track length for encoder bounds
        int lenParam = kParamGate1Length + (a->selectedTrack * 10);
        int trackLength = self->v[lenParam];  // 1-32
        
        // Right encoder: select step (0 to trackLength-1)
        if (data.encoders[1] != 0) {
            int delta = data.encoders[1];
            a->selectedStep += delta;
            
            // Wrap around based on track length
            if (a->selectedStep < 0) a->selectedStep = trackLength - 1;
            if (a->selectedStep >= trackLength) a->selectedStep = 0;
        }
        
        // Right encoder button: toggle gate
        uint16_t currentEncoderRButton = data.controls & kNT_encoderButtonR;
        uint16_t lastEncoderRButton = a->lastEncoderRButton & kNT_encoderButtonR;
        if (currentEncoderRButton && !lastEncoderRButton) {  // Rising edge
            // Toggle the gate
            int track = a->selectedTrack;
            int step = a->selectedStep;
            a->gateSteps[track][step] = !a->gateSteps[track][step];
            
            // Force update by incrementing a counter to verify button is being pressed
            a->selectedSeq = 3;  // Force redraw
        }
        a->lastEncoderRButton = data.controls;
        
        // DEBUG: Show encoder button state visually
        if (currentEncoderRButton) {
            // Draw indicator when button is pressed
            NT_drawText(120, 0, "BTN", 255);
        }
        
        // Ignore all other controls in gate mode
        return;  // Skip CV sequencer controls
    }
    
    // CV Sequencer mode (seq 0-2)
    
    // Get current sequencer's length
    int seq = a->selectedSeq;
    int lengthParam;
    if (seq == 0) lengthParam = kParamSeq1StepCount;
    else if (seq == 1) lengthParam = kParamSeq2StepCount;
    else lengthParam = kParamSeq3StepCount;
    
    int seqLength = self->v[lengthParam];  // 1-32
    
    // Right encoder: select step (0 to seqLength-1)
    if (data.encoders[1] != 0) {
        int delta = data.encoders[1];
        a->selectedStep += delta;
        
        // Wrap around based on sequencer length
        if (a->selectedStep < 0) a->selectedStep = seqLength - 1;
        if (a->selectedStep >= seqLength) a->selectedStep = 0;
        
        // Reset pot catch state when step changes
        a->potCaught[0] = false;
        a->potCaught[1] = false;
        a->potCaught[2] = false;
    }
    
    // Button 4: cycle through ratchet/repeat modes for selected step
    uint16_t currentButton4 = data.controls & kNT_button4;
    uint16_t lastButton4 = a->lastButton4State & kNT_button4;
    if (currentButton4 && !lastButton4) {  // Rising edge only
        a->stepMode[a->selectedSeq][a->selectedStep]++;
        if (a->stepMode[a->selectedSeq][a->selectedStep] > 6) {
            a->stepMode[a->selectedSeq][a->selectedStep] = 0;
        }
    }
    a->lastButton4State = data.controls;
    
    // Pots control the 3 values for the selected step with catch logic
    if (data.controls & kNT_potL) {
        float potValue = data.pots[0];
        int16_t currentValue = a->stepValues[a->selectedSeq][a->selectedStep][0];
        float currentNormalized = (currentValue + 32768) / 65535.0f;
        
        // Check if pot has caught the current value (within 2% tolerance)
        if (!a->potCaught[0]) {
            if (fabsf(potValue - currentNormalized) < 0.02f) {
                a->potCaught[0] = true;
            }
        }
        
        // Only update if caught
        if (a->potCaught[0]) {
            a->stepValues[a->selectedSeq][a->selectedStep][0] = (int16_t)((potValue * 65535.0f) - 32768);
        }
    }
    
    if (data.controls & kNT_potC) {
        float potValue = data.pots[1];
        int16_t currentValue = a->stepValues[a->selectedSeq][a->selectedStep][1];
        float currentNormalized = (currentValue + 32768) / 65535.0f;
        
        if (!a->potCaught[1]) {
            if (fabsf(potValue - currentNormalized) < 0.02f) {
                a->potCaught[1] = true;
            }
        }
        
        if (a->potCaught[1]) {
            a->stepValues[a->selectedSeq][a->selectedStep][1] = (int16_t)((potValue * 65535.0f) - 32768);
        }
    }
    
    if (data.controls & kNT_potR) {
        float potValue = data.pots[2];
        int16_t currentValue = a->stepValues[a->selectedSeq][a->selectedStep][2];
        float currentNormalized = (currentValue + 32768) / 65535.0f;
        
        if (!a->potCaught[2]) {
            if (fabsf(potValue - currentNormalized) < 0.02f) {
                a->potCaught[2] = true;
            }
        }
        
        if (a->potCaught[2]) {
            a->stepValues[a->selectedSeq][a->selectedStep][2] = (int16_t)((potValue * 65535.0f) - 32768);
        }
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    VSeq* a = (VSeq*)self;
    
    // Only update pot positions when step changes
    if (a->selectedStep != a->lastSelectedStep) {
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
    
    // Reset split/section parameters when step count changes
    if (parameterIndex == kParamSeq1StepCount || 
        parameterIndex == kParamSeq2StepCount ||
        parameterIndex == kParamSeq3StepCount) {
        
        int seq = 0;
        if (parameterIndex == kParamSeq1StepCount) seq = 0;
        else if (parameterIndex == kParamSeq2StepCount) seq = 1;
        else if (parameterIndex == kParamSeq3StepCount) seq = 2;
        
        int stepCount = self->v[parameterIndex];
        int splitParam = kParamSeq1SplitPoint + (seq * 6);
        int sec1Param = kParamSeq1Section1Reps + (seq * 6);
        int sec2Param = kParamSeq1Section2Reps + (seq * 6);
        
        // Calculate new split point (middle of sequence)
        int newSplit = stepCount / 2;
        if (newSplit < 1) newSplit = 1;
        if (newSplit >= stepCount) newSplit = stepCount - 1;
        
        // Reset parameters using NT_setParameterFromAudio
        int32_t algoIdx = NT_algorithmIndex(self);
        NT_setParameterFromAudio(algoIdx, splitParam + NT_parameterOffset(), newSplit);
        NT_setParameterFromAudio(algoIdx, sec1Param + NT_parameterOffset(), 1);
        NT_setParameterFromAudio(algoIdx, sec2Param + NT_parameterOffset(), 1);
        
        // Reset section counters
        a->section1Counter[seq] = 0;
        a->section2Counter[seq] = 0;
        a->inSection2[seq] = false;
    }
}

void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VSeq* a = (VSeq*)self;
    
    // Save all step values as 3D array
    stream.addMemberName("stepValues");
    stream.openArray();
    for (int seq = 0; seq < 4; seq++) {
        stream.openArray();
        for (int step = 0; step < 32; step++) {
            stream.openArray();
            for (int out = 0; out < 3; out++) {
                stream.addNumber((int)a->stepValues[seq][step][out]);
            }
            stream.closeArray();
        }
        stream.closeArray();
    }
    stream.closeArray();
    
    // Save step modes
    stream.addMemberName("stepModes");
    stream.openArray();
    for (int seq = 0; seq < 4; seq++) {
        stream.openArray();
        for (int step = 0; step < 32; step++) {
            stream.addNumber((int)a->stepMode[seq][step]);
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
    
    // Save gate sequencer data (6 tracks × 32 steps)
    stream.addMemberName("gateSteps");
    stream.openArray();
    for (int track = 0; track < 6; track++) {
        stream.openArray();
        for (int step = 0; step < 32; step++) {
            stream.addNumber(a->gateSteps[track][step] ? 1 : 0);
        }
        stream.closeArray();
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
                if (parse.numberOfArrayElements(numSteps)) {
                    // Support both 16 and 32 step presets
                    int stepsToLoad = (numSteps < 32) ? numSteps : 32;
                    for (int step = 0; step < stepsToLoad; step++) {
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
    
    // Match "stepModes" (optional, for backwards compatibility)
    if (parse.matchName("stepModes")) {
        int numSeqs = 0;
        if (parse.numberOfArrayElements(numSeqs) && numSeqs == 4) {
            for (int seq = 0; seq < 4; seq++) {
                int numSteps = 0;
                if (parse.numberOfArrayElements(numSteps)) {
                    int stepsToLoad = (numSteps < 32) ? numSteps : 32;
                    for (int step = 0; step < stepsToLoad; step++) {
                        int mode;
                        if (parse.number(mode)) {
                            a->stepMode[seq][step] = (uint8_t)mode;
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
    
    // Match "gateSteps" (optional, for gate sequencer)
    if (parse.matchName("gateSteps")) {
        int numTracks = 0;
        if (parse.numberOfArrayElements(numTracks)) {
            int tracksToLoad = (numTracks < 6) ? numTracks : 6;
            for (int track = 0; track < tracksToLoad; track++) {
                int numSteps = 0;
                if (parse.numberOfArrayElements(numSteps)) {
                    int stepsToLoad = (numSteps < 32) ? numSteps : 32;
                    for (int step = 0; step < stepsToLoad; step++) {
                        int value;
                        if (parse.number(value)) {
                            a->gateSteps[track][step] = (value != 0);
                        }
                    }
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
