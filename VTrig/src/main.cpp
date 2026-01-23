#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

// VTrig: 6-track trigger/gate sequencer
// - Shared Clock and Reset inputs
// - 6 independent trigger tracks
// - 32 steps per track
// - Direction control: Forward, Backward, Pingpong
// - Clock division/multiplication (31 options)
// - Swing (0-100%)
// - Section looping with configurable repeats
// - Fill feature

struct VTrig : public _NT_algorithm {
    // Trigger data: 6 tracks × 32 steps
    bool steps[6][32];
    
    // Track state (6 tracks)
    int currentStep[6];         // Current step for each track (0-31)
    bool pingpongForward[6];    // Direction state for pingpong mode
    int swingCounter[6];        // Counter for swing timing
    int section1Counter[6];     // Track section 1 repeat count
    int section2Counter[6];     // Track section 2 repeat count
    bool inSection2[6];         // Which section is currently playing
    bool inFill[6];             // Whether we're in the fill section
    int triggerCounter[6];      // Countdown for trigger pulse duration
    bool triggered[6];          // Whether gate was just triggered this step
    int clockCounter[6];        // Clock division counter for tracks
    int internalClockCounter[6]; // Internal subdivision counter for multiplication
    int lastClockPeriod[6];     // Samples between last two clocks (for multiplication)
    int samplesSinceLastClock[6]; // Sample counter since last clock
    
    // Edge detection
    float lastClockIn;
    float lastResetIn;
    
    // UI state
    int selectedStep;           // 0-31
    int selectedTrack;          // 0-5
    int lastSelectedStep;       // Track when step changes to update pots
    uint16_t lastButton4State;  // For debouncing button 4
    uint16_t lastEncoderRButton; // For debouncing right encoder button
    float lastPotLValue;        // Track left pot position for relative movement
    bool trackPotCaught;        // Track if left pot has caught track position
    
    VTrig() {
        // Initialize trigger data
        for (int track = 0; track < 6; track++) {
            for (int step = 0; step < 32; step++) {
                steps[track][step] = false;
            }
            currentStep[track] = 0;
            pingpongForward[track] = true;
            swingCounter[track] = 0;
            section1Counter[track] = 0;
            section2Counter[track] = 0;
            inSection2[track] = false;
            inFill[track] = false;
            triggerCounter[track] = 0;
            clockCounter[track] = 0;
            triggered[track] = false;
            internalClockCounter[track] = 0;
            lastClockPeriod[track] = 4800;  // Default ~10Hz at 48kHz
            samplesSinceLastClock[track] = 0;
        }
        
        lastClockIn = 0.0f;
        lastResetIn = 0.0f;
        selectedStep = 0;
        selectedTrack = 0;
        lastSelectedStep = 0;
        lastButton4State = 0;
        lastEncoderRButton = 0;
        lastPotLValue = 0.5f;
        trackPotCaught = false;
    }
    
    // Advance trigger track - will implement in Phase 2
    void advanceTrack(int track, int direction, int trackLength, int splitPoint, 
                      int sec1Reps, int sec2Reps, int fillStart);
};

// =============================================================================
// Parameters
// =============================================================================

enum {
    kParamClockIn,
    kParamResetIn,
    
    // Track outputs (6 tracks, 2 params each: Out + CC)
    kParamTrack1Out,
    kParamTrack1CC,
    kParamTrack2Out,
    kParamTrack2CC,
    kParamTrack3Out,
    kParamTrack3CC,
    kParamTrack4Out,
    kParamTrack4CC,
    kParamTrack5Out,
    kParamTrack5CC,
    kParamTrack6Out,
    kParamTrack6CC,
    
    // Track parameters (6 tracks, 9 params each)
    kParamTrack1Run,
    kParamTrack1Length,
    kParamTrack1Direction,
    kParamTrack1ClockDiv,
    kParamTrack1Swing,
    kParamTrack1SplitPoint,
    kParamTrack1Section1Reps,
    kParamTrack1Section2Reps,
    kParamTrack1FillStart,
    
    kParamTrack2Run,
    kParamTrack2Length,
    kParamTrack2Direction,
    kParamTrack2ClockDiv,
    kParamTrack2Swing,
    kParamTrack2SplitPoint,
    kParamTrack2Section1Reps,
    kParamTrack2Section2Reps,
    kParamTrack2FillStart,
    
    kParamTrack3Run,
    kParamTrack3Length,
    kParamTrack3Direction,
    kParamTrack3ClockDiv,
    kParamTrack3Swing,
    kParamTrack3SplitPoint,
    kParamTrack3Section1Reps,
    kParamTrack3Section2Reps,
    kParamTrack3FillStart,
    
    kParamTrack4Run,
    kParamTrack4Length,
    kParamTrack4Direction,
    kParamTrack4ClockDiv,
    kParamTrack4Swing,
    kParamTrack4SplitPoint,
    kParamTrack4Section1Reps,
    kParamTrack4Section2Reps,
    kParamTrack4FillStart,
    
    kParamTrack5Run,
    kParamTrack5Length,
    kParamTrack5Direction,
    kParamTrack5ClockDiv,
    kParamTrack5Swing,
    kParamTrack5SplitPoint,
    kParamTrack5Section1Reps,
    kParamTrack5Section2Reps,
    kParamTrack5FillStart,
    
    kParamTrack6Run,
    kParamTrack6Length,
    kParamTrack6Direction,
    kParamTrack6ClockDiv,
    kParamTrack6Swing,
    kParamTrack6SplitPoint,
    kParamTrack6Section1Reps,
    kParamTrack6Section2Reps,
    kParamTrack6FillStart,
    
    kParamTriggerMidiChannel,
    
    kNumParameters
};

// Parameter name strings
static char clockInName[] = "Clock In";
static char resetInName[] = "Reset In";

static char track1OutName[] = "Track 1 Out";
static char track1CCName[] = "Track 1 CC";
static char track2OutName[] = "Track 2 Out";
static char track2CCName[] = "Track 2 CC";
static char track3OutName[] = "Track 3 Out";
static char track3CCName[] = "Track 3 CC";
static char track4OutName[] = "Track 4 Out";
static char track4CCName[] = "Track 4 CC";
static char track5OutName[] = "Track 5 Out";
static char track5CCName[] = "Track 5 CC";
static char track6OutName[] = "Track 6 Out";
static char track6CCName[] = "Track 6 CC";

static char track1RunName[] = "Track 1 Run";
static char track1LenName[] = "Track 1 Length";
static char track1DirName[] = "Track 1 Direction";
static char track1DivName[] = "Track 1 Clock Div";
static char track1SwingName[] = "Track 1 Swing";
static char track1SplitName[] = "Track 1 Split Point";
static char track1Sec1Name[] = "Track 1 Sec1 Reps";
static char track1Sec2Name[] = "Track 1 Sec2 Reps";
static char track1FillName[] = "Track 1 Fill Start";

static char track2RunName[] = "Track 2 Run";
static char track2LenName[] = "Track 2 Length";
static char track2DirName[] = "Track 2 Direction";
static char track2DivName[] = "Track 2 Clock Div";
static char track2SwingName[] = "Track 2 Swing";
static char track2SplitName[] = "Track 2 Split Point";
static char track2Sec1Name[] = "Track 2 Sec1 Reps";
static char track2Sec2Name[] = "Track 2 Sec2 Reps";
static char track2FillName[] = "Track 2 Fill Start";

static char track3RunName[] = "Track 3 Run";
static char track3LenName[] = "Track 3 Length";
static char track3DirName[] = "Track 3 Direction";
static char track3DivName[] = "Track 3 Clock Div";
static char track3SwingName[] = "Track 3 Swing";
static char track3SplitName[] = "Track 3 Split Point";
static char track3Sec1Name[] = "Track 3 Sec1 Reps";
static char track3Sec2Name[] = "Track 3 Sec2 Reps";
static char track3FillName[] = "Track 3 Fill Start";

static char track4RunName[] = "Track 4 Run";
static char track4LenName[] = "Track 4 Length";
static char track4DirName[] = "Track 4 Direction";
static char track4DivName[] = "Track 4 Clock Div";
static char track4SwingName[] = "Track 4 Swing";
static char track4SplitName[] = "Track 4 Split Point";
static char track4Sec1Name[] = "Track 4 Sec1 Reps";
static char track4Sec2Name[] = "Track 4 Sec2 Reps";
static char track4FillName[] = "Track 4 Fill Start";

static char track5RunName[] = "Track 5 Run";
static char track5LenName[] = "Track 5 Length";
static char track5DirName[] = "Track 5 Direction";
static char track5DivName[] = "Track 5 Clock Div";
static char track5SwingName[] = "Track 5 Swing";
static char track5SplitName[] = "Track 5 Split Point";
static char track5Sec1Name[] = "Track 5 Sec1 Reps";
static char track5Sec2Name[] = "Track 5 Sec2 Reps";
static char track5FillName[] = "Track 5 Fill Start";

static char track6RunName[] = "Track 6 Run";
static char track6LenName[] = "Track 6 Length";
static char track6DirName[] = "Track 6 Direction";
static char track6DivName[] = "Track 6 Clock Div";
static char track6SwingName[] = "Track 6 Swing";
static char track6SplitName[] = "Track 6 Split Point";
static char track6Sec1Name[] = "Track 6 Sec1 Reps";
static char track6Sec2Name[] = "Track 6 Sec2 Reps";
static char track6FillName[] = "Track 6 Fill Start";

static char triggerMidiChannelName[] = "Trigger MIDI Ch";

static const char* const divisionStrings[] = {
    "/16", "/15", "/14", "/13", "/12", "/11", "/10", "/9", "/8", "/7", "/6", "/5", "/4", "/3", "/2",
    "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", NULL
};

static const char* const directionStrings[] = {
    "Forward", "Backward", "Pingpong", NULL
};

static _NT_parameter parameters[kNumParameters];

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
    
    // Track outputs and MIDI CC parameters (6 tracks, 2 parameters each)
    const char* outNames[] = {track1OutName, track2OutName, track3OutName, track4OutName, track5OutName, track6OutName};
    const char* ccNames[] = {track1CCName, track2CCName, track3CCName, track4CCName, track5CCName, track6CCName};
    
    for (int track = 0; track < 6; track++) {
        int outParam = kParamTrack1Out + (track * 2);
        int ccParam = kParamTrack1CC + (track * 2);
        
        parameters[outParam].name = outNames[track];
        parameters[outParam].min = 0;
        parameters[outParam].max = 28;
        parameters[outParam].def = 0;
        parameters[outParam].unit = kNT_unitCvOutput;
        parameters[outParam].scaling = kNT_scalingNone;
        
        parameters[ccParam].name = ccNames[track];
        parameters[ccParam].min = 0;
        parameters[ccParam].max = 127;
        parameters[ccParam].def = 0;
        parameters[ccParam].unit = kNT_unitNone;
        parameters[ccParam].scaling = kNT_scalingNone;
    }
    
    // Track parameters (6 tracks, 9 parameters each)
    const char* runNames[] = {track1RunName, track2RunName, track3RunName, track4RunName, track5RunName, track6RunName};
    const char* lenNames[] = {track1LenName, track2LenName, track3LenName, track4LenName, track5LenName, track6LenName};
    const char* dirNames[] = {track1DirName, track2DirName, track3DirName, track4DirName, track5DirName, track6DirName};
    const char* divNames[] = {track1DivName, track2DivName, track3DivName, track4DivName, track5DivName, track6DivName};
    const char* swingNames[] = {track1SwingName, track2SwingName, track3SwingName, track4SwingName, track5SwingName, track6SwingName};
    const char* splitNames[] = {track1SplitName, track2SplitName, track3SplitName, track4SplitName, track5SplitName, track6SplitName};
    const char* sec1Names[] = {track1Sec1Name, track2Sec1Name, track3Sec1Name, track4Sec1Name, track5Sec1Name, track6Sec1Name};
    const char* sec2Names[] = {track1Sec2Name, track2Sec2Name, track3Sec2Name, track4Sec2Name, track5Sec2Name, track6Sec2Name};
    const char* fillNames[] = {track1FillName, track2FillName, track3FillName, track4FillName, track5FillName, track6FillName};
    
    for (int track = 0; track < 6; track++) {
        int runParam = kParamTrack1Run + (track * 9);
        int lenParam = kParamTrack1Length + (track * 9);
        int dirParam = kParamTrack1Direction + (track * 9);
        int divParam = kParamTrack1ClockDiv + (track * 9);
        int swingParam = kParamTrack1Swing + (track * 9);
        int splitParam = kParamTrack1SplitPoint + (track * 9);
        int sec1Param = kParamTrack1Section1Reps + (track * 9);
        int sec2Param = kParamTrack1Section2Reps + (track * 9);
        int fillParam = kParamTrack1FillStart + (track * 9);
        
        parameters[runParam].name = runNames[track];
        parameters[runParam].min = 0;
        parameters[runParam].max = 1;
        parameters[runParam].def = 0;
        parameters[runParam].unit = kNT_unitNone;
        parameters[runParam].scaling = kNT_scalingNone;
        
        parameters[lenParam].name = lenNames[track];
        parameters[lenParam].min = 1;
        parameters[lenParam].max = 32;
        parameters[lenParam].def = 16;
        parameters[lenParam].unit = kNT_unitNone;
        parameters[lenParam].scaling = kNT_scalingNone;
        
        parameters[dirParam].name = dirNames[track];
        parameters[dirParam].min = 0;
        parameters[dirParam].max = 2;
        parameters[dirParam].def = 0;
        parameters[dirParam].unit = kNT_unitEnum;
        parameters[dirParam].scaling = kNT_scalingNone;
        parameters[dirParam].enumStrings = directionStrings;
        
        parameters[divParam].name = divNames[track];
        parameters[divParam].min = 0;
        parameters[divParam].max = 30;
        parameters[divParam].def = 14;  // Default to /2
        parameters[divParam].unit = kNT_unitEnum;
        parameters[divParam].scaling = kNT_scalingNone;
        parameters[divParam].enumStrings = divisionStrings;
        
        parameters[swingParam].name = swingNames[track];
        parameters[swingParam].min = 0;
        parameters[swingParam].max = 100;
        parameters[swingParam].def = 0;
        parameters[swingParam].unit = kNT_unitNone;
        parameters[swingParam].scaling = kNT_scalingNone;
        
        parameters[splitParam].name = splitNames[track];
        parameters[splitParam].min = 0;
        parameters[splitParam].max = 31;
        parameters[splitParam].def = 0;
        parameters[splitParam].unit = kNT_unitNone;
        parameters[splitParam].scaling = kNT_scalingNone;
        
        parameters[sec1Param].name = sec1Names[track];
        parameters[sec1Param].min = 1;
        parameters[sec1Param].max = 99;
        parameters[sec1Param].def = 1;
        parameters[sec1Param].unit = kNT_unitNone;
        parameters[sec1Param].scaling = kNT_scalingNone;
        
        parameters[sec2Param].name = sec2Names[track];
        parameters[sec2Param].min = 1;
        parameters[sec2Param].max = 99;
        parameters[sec2Param].def = 1;
        parameters[sec2Param].unit = kNT_unitNone;
        parameters[sec2Param].scaling = kNT_scalingNone;
        
        parameters[fillParam].name = fillNames[track];
        parameters[fillParam].min = 1;
        parameters[fillParam].max = 32;
        parameters[fillParam].def = 1;
        parameters[fillParam].unit = kNT_unitNone;
        parameters[fillParam].scaling = kNT_scalingNone;
    }
    
    // Trigger MIDI Channel
    parameters[kParamTriggerMidiChannel].name = triggerMidiChannelName;
    parameters[kParamTriggerMidiChannel].min = 0;
    parameters[kParamTriggerMidiChannel].max = 16;
    parameters[kParamTriggerMidiChannel].def = 0;
    parameters[kParamTriggerMidiChannel].unit = kNT_unitNone;
    parameters[kParamTriggerMidiChannel].scaling = kNT_scalingNone;
    
    self->parameters = parameters;
}

// =============================================================================
// Construction
// =============================================================================

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VTrig);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VTrig* alg = new (ptrs.sram) VTrig();
    initParameters(alg);
    return alg;
}

// =============================================================================
// Stub functions for Phase 1 - will implement in later phases
// =============================================================================

void VTrig::advanceTrack(int track, int direction, int trackLength, int splitPoint, 
                         int sec1Reps, int sec2Reps, int fillStart) {
    // If no sections (splitPoint >= trackLength), use simple wrapping logic
    if (splitPoint >= trackLength) {
        if (direction == 0) {
            // Forward
            currentStep[track]++;
            if (currentStep[track] >= trackLength) {
                currentStep[track] = 0;
            }
        } else if (direction == 1) {
            // Backward
            currentStep[track]--;
            if (currentStep[track] < 0) {
                currentStep[track] = trackLength - 1;
            }
        } else if (direction == 2) {
            // Pingpong
            if (pingpongForward[track]) {
                currentStep[track]++;
                if (currentStep[track] >= trackLength) {
                    currentStep[track] = trackLength - 2;
                    if (currentStep[track] < 0) currentStep[track] = 0;
                    pingpongForward[track] = false;
                }
            } else {
                currentStep[track]--;
                if (currentStep[track] < 0) {
                    currentStep[track] = 1;
                    if (currentStep[track] >= trackLength) currentStep[track] = trackLength - 1;
                    pingpongForward[track] = true;
                }
            }
        }
        return;
    }
    
    // Section-based logic
    // Determine section boundaries
    int section1End = (splitPoint > 0 && splitPoint < trackLength) ? splitPoint : trackLength;
    
    if (direction == 0) {  // Forward
        currentStep[track]++;
        
        // Check for fill trigger on last repetition of section 1
        // Only if sections are enabled (splitPoint < trackLength) AND fill is enabled (fillStart > 0)
        // AND we're actually repeating section 1 (sec1Reps > 1)
        if (!inSection2[track] && 
            splitPoint > 0 && 
            splitPoint < trackLength &&
            fillStart > 0 &&
            fillStart < splitPoint &&
            sec1Reps > 1 &&
            section1Counter[track] == sec1Reps - 1 &&
            currentStep[track] >= fillStart) {
            // Fill triggered! Jump to section 2
            section1Counter[track] = 0;
            inSection2[track] = true;
            currentStep[track] = splitPoint;
        }
        // Check if we've crossed a section boundary
        else if (!inSection2[track] && currentStep[track] >= section1End) {
            // Completed section 1
            section1Counter[track]++;
            if (section1Counter[track] >= sec1Reps) {
                // Move to section 2
                section1Counter[track] = 0;
                inSection2[track] = true;
                if (splitPoint > 0) {
                    currentStep[track] = splitPoint;
                } else {
                    currentStep[track] = 0;
                }
            } else {
                // Repeat section 1
                currentStep[track] = 0;
            }
        } else if (inSection2[track] && currentStep[track] >= trackLength) {
            // Completed section 2
            section2Counter[track]++;
            if (section2Counter[track] >= sec2Reps) {
                // Back to section 1
                section2Counter[track] = 0;
                inSection2[track] = false;
            }
            currentStep[track] = (splitPoint > 0) ? splitPoint : 0;
            if (!inSection2[track]) {
                currentStep[track] = 0;
            }
        }
    } else if (direction == 1) {  // Backward
        currentStep[track]--;
        
        if (inSection2[track] && currentStep[track] < splitPoint) {
            section2Counter[track]++;
            if (section2Counter[track] >= sec2Reps) {
                section2Counter[track] = 0;
                inSection2[track] = false;
                currentStep[track] = section1End - 1;
            } else {
                currentStep[track] = trackLength - 1;
            }
        } else if (!inSection2[track] && currentStep[track] < 0) {
            section1Counter[track]++;
            if (section1Counter[track] >= sec1Reps) {
                section1Counter[track] = 0;
                inSection2[track] = true;
                currentStep[track] = trackLength - 1;
            } else {
                currentStep[track] = section1End - 1;
            }
        }
    } else if (direction == 2) {  // Pingpong
        if (pingpongForward[track]) {
            currentStep[track]++;
            if (currentStep[track] >= trackLength) {
                currentStep[track] = trackLength - 2;
                if (currentStep[track] < 0) currentStep[track] = 0;
                pingpongForward[track] = false;
            }
        } else {
            currentStep[track]--;
            if (currentStep[track] < 0) {
                currentStep[track] = 1;
                if (currentStep[track] >= trackLength) currentStep[track] = trackLength - 1;
                pingpongForward[track] = true;
            }
        }
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VTrig* a = static_cast<VTrig*>(self);
    int numFrames = numFramesBy4 * 4;
    
    // Read clock and reset inputs
    int clockInput = self->v[kParamClockIn];     // 0 = none, 1-28 = bus
    int resetInput = self->v[kParamResetIn];     // 0 = none, 1-28 = bus
    
    float clockIn = 0.0f;
    float resetIn = 0.0f;
    
    if (clockInput > 0 && clockInput <= 28) {
        float* clockBus = busFrames + ((clockInput - 1) * numFrames);
        clockIn = clockBus[0];  // Sample first frame
    }
    
    if (resetInput > 0 && resetInput <= 28) {
        float* resetBus = busFrames + ((resetInput - 1) * numFrames);
        resetIn = resetBus[0];  // Sample first frame
    }
    
    // Detect clock and reset edges (rising edge = > 2.5V)
    bool clockTrig = (clockIn > 2.5f && a->lastClockIn <= 2.5f);
    bool resetTrig = (resetIn > 2.5f && a->lastResetIn <= 2.5f);
    
    a->lastClockIn = clockIn;
    a->lastResetIn = resetIn;
    
    // Process each track
    for (int track = 0; track < 6; track++) {
        int outParam = kParamTrack1Out + (track * 2);
        int runParam = kParamTrack1Run + (track * 9);
        int lenParam = kParamTrack1Length + (track * 9);
        int dirParam = kParamTrack1Direction + (track * 9);
        int divParam = kParamTrack1ClockDiv + (track * 9);
        int swingParam = kParamTrack1Swing + (track * 9);
        int splitParam = kParamTrack1SplitPoint + (track * 9);
        int sec1Param = kParamTrack1Section1Reps + (track * 9);
        int sec2Param = kParamTrack1Section2Reps + (track * 9);
        int fillParam = kParamTrack1FillStart + (track * 9);
        
        int outputBus = self->v[outParam];
        int isRunning = self->v[runParam];
        int trackLength = self->v[lenParam];
        int direction = self->v[dirParam];
        int clockDiv = self->v[divParam];
        int swing = self->v[swingParam];
        int splitPoint = self->v[splitParam];
        int sec1Reps = self->v[sec1Param];
        int sec2Reps = self->v[sec2Param];
        int fillStart = self->v[fillParam];
        
        // Map clockDiv parameter to divisor/multiplier
        int divisor = 1;
        int multiplier = 1;
        bool isDivision = (clockDiv < 15);
        
        if (isDivision) {
            divisor = 16 - clockDiv;
        } else {
            multiplier = clockDiv - 14;
        }
        
        // Skip if not running
        if (isRunning == 0) continue;
        
        // Reset handling
        if (resetTrig) {
            a->currentStep[track] = 0;
            a->pingpongForward[track] = true;
            a->swingCounter[track] = 0;
            a->section1Counter[track] = 0;
            a->section2Counter[track] = 0;
            a->inSection2[track] = false;
            a->inFill[track] = false;
            a->clockCounter[track] = 0;
            a->internalClockCounter[track] = 0;
            a->samplesSinceLastClock[track] = 0;
        }
        
        // Track samples for multiplication
        a->samplesSinceLastClock[track] += numFrames;
        
        // Clock handling with division/multiplication
        bool stepped = false;
        if (clockTrig) {
            // Measure clock period for multiplication
            if (a->samplesSinceLastClock[track] > 100 && a->samplesSinceLastClock[track] < 96000) {
                a->lastClockPeriod[track] = a->samplesSinceLastClock[track];
            }
            a->samplesSinceLastClock[track] = 0;
            a->internalClockCounter[track] = 0;
            
            if (isDivision) {
                // Division: count clocks before advancing
                a->clockCounter[track]++;
                if (a->clockCounter[track] >= divisor) {
                    a->clockCounter[track] = 0;
                    a->advanceTrack(track, direction, trackLength, splitPoint, sec1Reps, sec2Reps, fillStart);
                    stepped = true;
                }
            } else {
                // Multiplication: step on external clock
                a->advanceTrack(track, direction, trackLength, splitPoint, sec1Reps, sec2Reps, fillStart);
                stepped = true;
            }
        }
        
        // Internal clock multiplication - generate additional steps between external clocks
        if (!isDivision && multiplier > 1 && !clockTrig && a->lastClockPeriod[track] > 0) {
            int subdivisionPeriod = a->lastClockPeriod[track] / multiplier;
            
            if (subdivisionPeriod > numFrames && a->samplesSinceLastClock[track] >= subdivisionPeriod * (a->internalClockCounter[track] + 1)) {
                a->internalClockCounter[track]++;
                if (a->internalClockCounter[track] < multiplier) {
                    a->advanceTrack(track, direction, trackLength, splitPoint, sec1Reps, sec2Reps, fillStart);
                    stepped = true;
                }
            }
        }
        
        // After potential advancement, mark if current step should trigger
        if (stepped) {
            int currentStep = a->currentStep[track];
            if (currentStep >= 0 && currentStep < 32 && a->steps[track][currentStep]) {
                // Apply swing: delay odd-numbered steps
                bool isOddStep = (currentStep % 2) == 1;
                int swingDelay = 0;
                
                if (isOddStep && swing > 0 && a->lastClockPeriod[track] > 0) {
                    // Swing delays by percentage of half the clock period
                    swingDelay = (a->lastClockPeriod[track] * swing) / 200;
                }
                
                // If no swing delay, trigger immediately
                if (swingDelay == 0) {
                    a->triggerCounter[track] = 240;  // ~5ms at 48kHz
                } else {
                    // Set swing counter to delay the trigger
                    a->swingCounter[track] = swingDelay;
                }
            }
        }
        
        // Handle swing delay countdown
        if (a->swingCounter[track] > 0) {
            a->swingCounter[track] -= numFrames;
            if (a->swingCounter[track] <= 0) {
                a->swingCounter[track] = 0;
                a->triggerCounter[track] = 240;  // Trigger now after swing delay
            }
        }
        
        // Countdown trigger pulses every buffer
        if (a->triggerCounter[track] > 0) {
            a->triggerCounter[track] -= numFrames;
            if (a->triggerCounter[track] < 0) {
                a->triggerCounter[track] = 0;
            }
        }
        
        // Output trigger pulse
        if (outputBus > 0 && outputBus <= 28) {
            bool triggerActive = a->triggerCounter[track] > 0;
            
            // Write to all frames in the output bus
            float* outBus = busFrames + ((outputBus - 1) * numFrames);
            for (int frame = 0; frame < numFrames; frame++) {
                outBus[frame] = triggerActive ? 5.0f : 0.0f;  // 5V trigger
            }
        }
    }
}

void parameterChanged(_NT_algorithm* self, int parameterIndex) {
    // Phase 3: implement parameter change handling
}

bool draw(_NT_algorithm* self) {
    // Phase 4: implement display
    return false;
}

// =============================================================================
// Plugin Factory
// =============================================================================

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'T', 'R', 'G'),
    .name = "VTrig",
    .description = "6-Track Trigger Sequencer",
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
    .hasCustomUi = nullptr,
    .customUi = nullptr,
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
