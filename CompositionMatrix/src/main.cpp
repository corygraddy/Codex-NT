#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cstddef>

const size_t DEBUG_LOG_SIZE = 32; // Keep this small to conserve memory

// --- Data To Continue (DTC) Struct ---
// This struct holds the state of the algorithm that needs to persist when the
// preset is saved. It's stored in a special memory region (DTC) that is
// fast and non-volatile within a session.
struct CompositionMatrixAlgorithm_DTC {
    // The 32-bit integer that represents the saved musical pattern.
    uint32_t savedShiftRegister = 0;
    // A log of all the patterns saved during the session.
    uint32_t debugLog[DEBUG_LOG_SIZE];
    // The number of entries currently in the debug log.
    size_t debugLogCount = 0;
};

// --- Algorithm State ---
// This enum defines the two main states of the algorithm.
enum AlgorithmState {
    STATE_EXPLORE, // The algorithm is generating new patterns.
    STATE_REFINE   // The user has frozen a pattern to work with it.
};

// --- Main Algorithm Struct ---
// This struct holds the main runtime state of the algorithm. It inherits
// from the base _NT_algorithm class provided by the API.
struct CompositionMatrixAlgorithm : public _NT_algorithm {
    // A pointer to our persistent state.
    CompositionMatrixAlgorithm_DTC* dtc;

    // The core of the generative engine, a 32-bit Linear Feedback Shift Register (LFSR).
    uint32_t shiftRegister = 0xDEADBEEF; // Initial seed value.

    // The current pitch CV output value.
    float pitchCv = 0.0f;

    // A counter to manage the duration of the gate output pulse.
    int gateCounter = 0;

    // The current operational state of the algorithm (Explore or Refine).
    AlgorithmState state = STATE_EXPLORE;

    // The currently selected slot for saving/loading patterns. (For future use)
    int currentSlot = 0;

    // Store the previous value of the Save parameter for rising-edge detection.
    int16_t lastSaveValue = 0;

    // Holds the value of the clock input from the previous frame for edge detection.
    float lastClockValue = 0.0f;

    // Constructor: Initializes the algorithm and links the DTC state.
    CompositionMatrixAlgorithm(CompositionMatrixAlgorithm_DTC* dtc_ptr) : dtc(dtc_ptr) {}
};

// --- Parameters ---
// Enum for identifying the algorithm's parameters.
enum {
    // Page 1: SYSTEM
    kParamGlobalKey,
    kParamGlobalScale,
    kParamPolyphonyMode,
    kParamNumSupportVoices,

    // Page 2: PATTERN
    kParamPatternSlot,
    kParamMode,
    kParamSave,
    kParamDuplicate,

    // Page 3: HARMONY
    kParamMusicalMode,
    kParamChordExtension,
    kParamHarmonicMovement,
    kParamBeatsPerMeasure,

    // Page 4: RHYTHM
    kParamDensityLead,
    kParamDensityBass,
    kParamDensitySupport,
    kParamGateLength,

    // Page 5: DYNAMICS
    kParamBaseVelocity,
    kParamVelocityDynamics,

    // Page 6: PITCH
    kParamLeadOctaveSpread,
    kParamLeadOctaveOffset,
    kParamBassOctaveSpread,
    kParamBassOctaveOffset,
    kParamSupportOctaveSpread,
    kParamSupportOctaveOffset,

    // Page 7: CHAOS
    kParamVelocityChaos,
    kParamGateChaos,
    kParamNoteChaos,

    // Page 8: SONG - EDIT
    kParamSongSlot,
    kParamAssignPattern,
    kParamRepeatCount,
    kParamInsertStep,
    kParamDeleteStep,

    // Page 9: SONG - PERFORM
    kParamLiveAudition,
    kParamStepVoicing,
    kParamStepNumVoices,
    kParamRestartSong,
    kParamStopSong,

    // Page 10: INPUTS
    kParamClockIn,
    kParamFreezeIn,

    // Page 11: OUTPUTS
    kParamPitchOut,
    kParamGateOut,

    kParamStateVersion, // Hidden parameter to track state changes
    kNumParameters
};

// Static array defining the parameters available in the UI.
static const _NT_parameter parameters[kNumParameters] = {
    // Page 1: SYSTEM
    { .name = "Global Key", .min = 0, .max = 11, .def = 0, .enumStrings = "C,C#,D,D#,E,F,F#,G,G#,A,A#,B" },
    { .name = "Global Scale", .min = 0, .max = 6, .def = 0, .enumStrings = "Major,Minor,Dorian,Phrygian,Lydian,Mixolydian,Locrian" },
    { .name = "Polyphony Mode", .min = 0, .max = 2, .def = 0, .enumStrings = "Lead Only,All Voices,Headless" },
    { .name = "Num Support Voices", .min = 1, .max = 3, .def = 1 },

    // Page 2: PATTERN
    { .name = "Pattern Slot", .min = 0, .max = 99, .def = 0 },
    { .name = "Mode", .min = 0, .max = 1, .def = 0, .enumStrings = "Explore,Refine" },
    { .name = "Save", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Duplicate", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 3: HARMONY
    { .name = "Musical Mode", .min = 0, .max = 6, .def = 0, .enumStrings = "Ionian,Dorian,Phrygian,Lydian,Mixolydian,Locrian,Aeolian" },
    { .name = "Chord Extension", .min = 0, .max = 2, .def = 0, .enumStrings = "Triad,7th,9th" },
    { .name = "Harmonic Movement", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Beats Per Measure", .min = 2, .max = 13, .def = 4 },

    // Page 4: RHYTHM
    { .name = "Density: Lead", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Density: Bass", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Density: Support", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Gate Length", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },

    // Page 5: DYNAMICS
    { .name = "Base Velocity", .min = 0, .max = 127, .def = 100 },
    { .name = "Velocity Dynamics", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },

    // Page 6: PITCH
    { .name = "Lead Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Lead Octave Offset", .min = -3, .max = 3, .def = 0 },
    { .name = "Bass Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Bass Octave Offset", .min = -3, .max = 3, .def = -1 },
    { .name = "Support Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Support Octave Offset", .min = -3, .max = 3, .def = 0 },

    // Page 7: CHAOS
    { .name = "Velocity Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },
    { .name = "Gate Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },
    { .name = "Note Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },

    // Page 8: SONG - EDIT
    { .name = "Song Slot", .min = 0, .max = 49, .def = 0 },
    { .name = "Assign Pattern", .min = 0, .max = 99, .def = 0 },
    { .name = "Repeat Count", .min = 1, .max = 16, .def = 1 },
    { .name = "Insert Step", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Delete Step", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 9: SONG - PERFORM
    { .name = "Live Audition", .min = 0, .max = 99, .def = 0 },
    { .name = "Step Voicing", .min = 0, .max = 2, .def = 0, .enumStrings = "Lead Only,All Voices,Headless" },
    { .name = "Step Num Voices", .min = 1, .max = 3, .def = 1 },
    { .name = "Restart Song", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Stop Song", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 10: INPUTS
    NT_PARAMETER_CV_INPUT("Clock In", 1, 0),
    NT_PARAMETER_CV_INPUT("Freeze In", 2, 0),

    // Page 11: OUTPUTS
    NT_PARAMETER_CV_OUTPUT("Pitch Out", 1, 0),
    NT_PARAMETER_CV_OUTPUT("Gate Out", 2, 0),

    // This parameter is not on any page, so it's hidden from the user.
    { .name = "State Version", .min = 0, .max = 32767, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
};

static const uint8_t page1_params[] = { kParamGlobalKey, kParamGlobalScale, kParamPolyphonyMode, kParamNumSupportVoices };
static const _NT_parameterPage page1_system = { "SYSTEM", page1_params, 4 };
static const uint8_t page2_params[] = { kParamPatternSlot, kParamMode, kParamSave, kParamDuplicate };
static const _NT_parameterPage page2_pattern = { "PATTERN", page2_params, 4 };
static const uint8_t page3_params[] = { kParamMusicalMode, kParamChordExtension, kParamHarmonicMovement, kParamBeatsPerMeasure };
static const _NT_parameterPage page3_harmony = { "HARMONY", page3_params, 4 };
static const uint8_t page4_params[] = { kParamDensityLead, kParamDensityBass, kParamDensitySupport, kParamGateLength };
static const _NT_parameterPage page4_rhythm = { "RHYTHM", page4_params, 4 };
static const uint8_t page5_params[] = { kParamBaseVelocity, kParamVelocityDynamics };
static const _NT_parameterPage page5_dynamics = { "DYNAMICS", page5_params, 2 };
static const uint8_t page6_params[] = { kParamLeadOctaveSpread, kParamLeadOctaveOffset, kParamBassOctaveSpread, kParamBassOctaveOffset, kParamSupportOctaveSpread, kParamSupportOctaveOffset };
static const _NT_parameterPage page6_pitch = { "PITCH", page6_params, 6 };
static const uint8_t page7_params[] = { kParamVelocityChaos, kParamGateChaos, kParamNoteChaos };
static const _NT_parameterPage page7_chaos = { "CHAOS", page7_params, 3 };
static const uint8_t page8_params[] = { kParamSongSlot, kParamAssignPattern, kParamRepeatCount, kParamInsertStep, kParamDeleteStep };
static const _NT_parameterPage page8_song_edit = { "SONG - EDIT", page8_params, 5 };
static const uint8_t page9_params[] = { kParamLiveAudition, kParamStepVoicing, kParamStepNumVoices, kParamRestartSong, kParamStopSong };
static const _NT_parameterPage page9_song_perform = { "SONG - PERFORM", page9_params, 5 };
static const uint8_t page10_params[] = { kParamClockIn, kParamFreezeIn };
static const _NT_parameterPage page10_inputs = { "INPUTS", page10_params, 2 };
static const uint8_t page11_params[] = { kParamPitchOut, kParamGateOut };
static const _NT_parameterPage page11_outputs = { "OUTPUTS", page11_params, 2 };

static const _NT_parameterPages pages = {
    11,
    { &page1_system, &page2_pattern, &page3_harmony, &page4_rhythm, &page5_dynamics, &page6_pitch, &page7_chaos, &page8_song_edit, &page9_song_perform, &page10_inputs, &page11_outputs }
};

// --- Hard-coded Scale (C Major Pentatonic) ---
// An array of voltage values corresponding to the notes in the scale.
const float cMajorPentatonicVolts[] = {
    0.0f, 0.1667f, 0.3333f, 0.5833f, 0.75f, // C, D, E, G, A
    1.0f, 1.1667f, 1.3333f, 1.5833f, 1.75f  // C, D, E, G, A (Octave up)
};

// --- Core API Functions ---

// calculateRequirements: Called by the system to determine memory needs.
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(CompositionMatrixAlgorithm);
    // Request memory for our persistent state struct in the DTC region.
    req.dtc = sizeof(CompositionMatrixAlgorithm_DTC);
}

// construct: Called by the system to create an instance of the algorithm.
_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    // Initialize the DTC struct using placement new.
    CompositionMatrixAlgorithm_DTC* dtc = new (ptrs.dtc) CompositionMatrixAlgorithm_DTC();
    // Initialize the main algorithm struct,.
    CompositionMatrixAlgorithm* alg = new (ptrs.sram) CompositionMatrixAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

// parameterChanged: Callback for when a parameter is changed in the UI.
void parameterChanged(_NT_algorithm* self, int p) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;

    if (p == kParamMode) {
        pThis->state = (AlgorithmState)pThis->v[kParamMode];
    } else if (p == kParamSave) {
        // Rising-edge detection for the Save trigger
        if (pThis->v[kParamSave] > 0 && pThis->lastSaveValue == 0) {
            // Save the current "live" pattern into our persistent DTC struct.
            pThis->dtc->savedShiftRegister = pThis->shiftRegister;

            // Add to the debug log
            if (pThis->dtc->debugLogCount < DEBUG_LOG_SIZE) {
                pThis->dtc->debugLog[pThis->dtc->debugLogCount++] = pThis->shiftRegister;
            }

            // Increment the hidden state version parameter to mark the preset as dirty.
            int16_t nextVersion = (pThis->v[kParamStateVersion] + 1) % 32767;
            NT_setParameterFromUi(NT_algorithmIndex(self), kParamStateVersion + NT_parameterOffset(), nextVersion);
        }
        pThis->lastSaveValue = pThis->v[kParamSave];
    }
}

// step: The main audio processing loop.
void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;
    int numFrames = numFramesBy4 * 4;
    const int gatePulseDuration = 480; // 10ms pulse at 48kHz

    // --- Get Bus Pointers ---
    int clockInBus = pThis->v[kParamClockIn];
    float* clockIn = (clockInBus > 0) ? busFrames + (clockInBus - 1) * numFrames : nullptr;

    int freezeInBus = pThis->v[kParamFreezeIn];
    float* freezeIn = (freezeInBus > 0) ? busFrames + (freezeInBus - 1) * numFrames : nullptr;

    int pitchOutBus = pThis->v[kParamPitchOut];
    float* pitchOut = (pitchOutBus > 0) ? busFrames + (pitchOutBus - 1) * numFrames : nullptr;

    int gateOutBus = pThis->v[kParamGateOut];
    float* gateOut = (gateOutBus > 0) ? busFrames + (gateOutBus - 1) * numFrames : nullptr;

    // --- Process Audio Frames ---
    if (clockIn) {
        for (int i = 0; i < numFrames; ++i) {
            // Simple rising-edge detection for the clock input.
            if (clockIn[i] >= 1.0f && pThis->lastClockValue < 1.0f) {
                // --- CLOCK TRIGGERED ---

                bool frozen = (pThis->state == STATE_REFINE);
                if (freezeIn) {
                    if (freezeIn[i] > 1.0f) {
                        frozen = true;
                    }
                }

                // If not frozen, advance the shift register using a Galois LFSR algorithm.
                if (!frozen) {
                    pThis->shiftRegister = (pThis->shiftRegister >> 1) ^ (-(pThis->shiftRegister & 1) & 0xD0000040u);
                }

                // Use the lower bits of the register to select a note from the scale.
                uint8_t noteIndex = pThis->shiftRegister & 0x0F;
                pThis->pitchCv = cMajorPentatonicVolts[noteIndex % ARRAY_SIZE(cMajorPentatonicVolts)];
                
                // Start the gate pulse.
                pThis->gateCounter = gatePulseDuration;
            }
            pThis->lastClockValue = clockIn[i];

            // Manage the gate output.
            float gateValue = 0.0f;
            if (pThis->gateCounter > 0) {
                gateValue = 5.0f; // 5V gate signal.
                pThis->gateCounter--;
            }

            // --- Set Output Buffers (per sample) ---
            if (pitchOut) {
                pitchOut[i] = pThis->pitchCv;
            }
            if (gateOut) {
                gateOut[i] = gateValue;
            }
        }
    } else {
        // If no clock is connected, ensure outputs are silent.
        if (pitchOut) {
            for (int i = 0; i < numFrames; ++i) { pitchOut[i] = 0.0f; }
        }
        if (gateOut) {
            for (int i = 0; i < numFrames; ++i) { gateOut[i] = 0.0f; }
        }
    }
}

// serialise: Saves the persistent state to the preset JSON.
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;

    // Save the 32-bit pattern. This is the primary data.
    stream.addMemberName("savedShiftRegister");
    stream.addNumber((int)pThis->dtc->savedShiftRegister);

    // Save the state version for debugging, to confirm the dirty flag is working.
    stream.addMemberName("stateVersion");
    stream.addNumber(pThis->v[kParamStateVersion]);

    // --- Enhanced Debugging Context ---
    stream.addMemberName("debug_context");
    stream.openObject();
    stream.addMemberName("live_shift_register");
    stream.addNumber((int)pThis->shiftRegister);
    stream.addMemberName("algorithm_state");
    stream.addString(pThis->state == STATE_EXPLORE ? "Explore" : "Refine");
    stream.closeObject();


    // Regression Test
    stream.addMemberName("regression_test");
    stream.openObject();
    stream.addMemberName("test_string");
    stream.addString("hello");
    stream.addMemberName("test_int");
    stream.addNumber(123);
    stream.addMemberName("test_float");
    stream.addNumber(45.67f);
    stream.closeObject();

    // Debug Log
    stream.addMemberName("debug_log");
    stream.openArray();
    for (size_t i = 0; i < pThis->dtc->debugLogCount; ++i) {
        stream.addNumber((int)pThis->dtc->debugLog[i]);
    }
    stream.closeArray();
}

// deserialise: Loads the persistent state from the preset JSON.
bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;
    int numMembers = 0;
    if (parse.numberOfObjectMembers(numMembers)) {
        for (int i = 0; i < numMembers; ++i) {
            if (parse.matchName("savedShiftRegister")) {
                int temp_val = 0;
                if (parse.number(temp_val)) {
                    pThis->dtc->savedShiftRegister = (uint32_t)temp_val;
                }
            } else if (parse.matchName("debug_log")) {
                int arraySize = 0;
                if (parse.numberOfArrayElements(arraySize)) {
                    pThis->dtc->debugLogCount = 0;
                    for (int j = 0; j < arraySize && j < DEBUG_LOG_SIZE; ++j) {
                        int temp_val = 0;
                        if (parse.number(temp_val)) {
                            pThis->dtc->debugLog[pThis->dtc->debugLogCount++] = (uint32_t)temp_val;
                        }
                    }
                }
            } else {
                parse.skipMember();
            }
        }
    }
    return true;
}

// --- Plugin Registration ---
// The factory struct defines the algorithm and its capabilities.
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('C', 'M', 'P', 'X'),
    .name = "CompositionMatrix",
    .description = "Generative Harmony & Rhythm Engine.",
    .numSpecifications = 0,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .tags = kNT_tagInstrument,
    .serialise = serialise,
    .deserialise = deserialise,
};

// The main entry point that the Disting OS calls to interact with the plugin.
uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}