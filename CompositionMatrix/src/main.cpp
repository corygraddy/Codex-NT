#include <distingnt/api.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// --- Logging Utility ---
void logMessage(const std::string& message) {
    std::ofstream logfile("/mnt/sdcard/engine_log.txt", std::ios_base::app);
    if (logfile.is_open()) {
        logfile << message << std::endl;
    }
}

// --- Algorithm State ---
struct CompositionMatrixAlgorithm : public _NT_algorithm {
    uint32_t shiftRegister = 0xDEADBEEF;
    bool frozen = false;
    float lastClockValue = 0.0f;
    float gateValue = 0.0f;
    float pitchCv = 0.0f; // Hold the current pitch CV
};

// --- Parameters ---
enum {
    kParamClockIn,
    kParamFreezeIn,
    kParamPitchOut,
    kParamGateOut,
};

static const _NT_parameter parameters[] = {
    NT_PARAMETER_CV_INPUT("Clock In", 1, 0),
    NT_PARAMETER_CV_INPUT("Freeze In", 2, 0),
    NT_PARAMETER_CV_OUTPUT("Pitch Out", 1, 0),
    NT_PARAMETER_CV_OUTPUT("Gate Out", 2, 0),
};

// --- Hard-coded Scale (C Major Pentatonic) ---
const float cMajorPentatonicVolts[] = {
    0.0f, 0.1667f, 0.3333f, 0.5833f, 0.75f,
    1.0f, 1.1667f, 1.3333f, 1.5833f, 1.75f
};

// --- Core API Functions ---

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(CompositionMatrixAlgorithm);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    CompositionMatrixAlgorithm* alg = new (ptrs.sram) CompositionMatrixAlgorithm();
    alg->parameters = parameters;
    logMessage("CompositionMatrix Initialized. Seed: " + std::to_string(alg->shiftRegister));
    return alg;
}

void parameterChanged(_NT_algorithm* self, int p) {
    // Nothing to do for the MVP
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;
    int numFrames = numFramesBy4 * 4;

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
    pThis->gateValue = 0.0f; // Lower the gate at the start of the block

    if (clockIn) {
        for (int i = 0; i < numFrames; ++i) {
            // Rising-edge detection
            if (clockIn[i] >= 1.0f && pThis->lastClockValue < 1.0f) {
                // CLOCK TRIGGERED
                if (freezeIn) {
                    pThis->frozen = (freezeIn[i] > 1.0f);
                }

                if (!pThis->frozen) {
                    pThis->shiftRegister = (pThis->shiftRegister >> 1) ^ (-(pThis->shiftRegister & 1) & 0xD0000040u);
                }

                uint8_t noteIndex = pThis->shiftRegister & 0x0F;
                pThis->pitchCv = cMajorPentatonicVolts[noteIndex % ARRAY_SIZE(cMajorPentatonicVolts)];
                pThis->gateValue = 5.0f; // Fire the gate (5V)

                logMessage("Clock. Frozen: " + std::to_string(pThis->frozen) + ". Val: " + std::to_string(pThis->shiftRegister) + ". CV: " + std::to_string(pThis->pitchCv));
            }
            pThis->lastClockValue = clockIn[i];
        }
    }

    // --- Set Output Buffers ---
    if (pitchOut) {
        for (int i = 0; i < numFrames; ++i) {
            pitchOut[i] = pThis->pitchCv;
        }
    }
    if (gateOut) {
        for (int i = 0; i < numFrames; ++i) {
            gateOut[i] = pThis->gateValue;
        }
    }
}

bool draw(_NT_algorithm* self) {
    NT_drawText(8, 32, "CompositionMatrix");
    NT_drawText(10, 48, "MVP");
    return false;
}

// --- Plugin Registration ---
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('C', 'M', 'P', 'X'),
    .name = "CompositionMatrix",
    .description = "Generative Harmony & Rhythm Engine.",
    .numSpecifications = 0,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .tags = kNT_tagInstrument,
};

// The main entry point that the Disting OS calls.
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