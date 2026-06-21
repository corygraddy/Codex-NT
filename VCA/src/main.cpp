/*
 * Simple VCA Plugin for Disting NT
 */

#include <distingnt/api.h>
#include <cmath>
#include <new>

// --- Main Algorithm Struct ---
struct VCAAlgorithm : public _NT_algorithm {
    // No extra state needed for this simple VCA
};

// --- Parameters ---
enum {
    kParamLevel,
    kParamCurve,
    kParamCvIn,
    kParamCvAmt,
    kParamAudioIn,
    kParamAudioOut,
    kNumParameters
};

// Parameter name strings
static char levelName[] = "Level";
static char curveName[] = "Curve";
static char cvInName[] = "CV In";
static char cvAmtName[] = "CV Amt";
static char audioInName[] = "Audio In";
static char audioOutName[] = "Audio Out";

static const char* const curveStrings[] = { "Linear", "Exponential", NULL };

static _NT_parameter parameters[kNumParameters];

void initParameters(_NT_algorithm* self) {
    parameters[kParamLevel].name = levelName;
    parameters[kParamLevel].min = 0;
    parameters[kParamLevel].max = 100;
    parameters[kParamLevel].def = 100;
    parameters[kParamLevel].unit = kNT_unitPercent;
    parameters[kParamLevel].scaling = kNT_scalingNone;
    
    parameters[kParamCurve].name = curveName;
    parameters[kParamCurve].min = 0;
    parameters[kParamCurve].max = 1;
    parameters[kParamCurve].def = 0;
    parameters[kParamCurve].unit = kNT_unitEnum;
    parameters[kParamCurve].scaling = kNT_scalingNone;
    parameters[kParamCurve].enumStrings = curveStrings;
    
    parameters[kParamCvIn].name = cvInName;
    parameters[kParamCvIn].min = 0;
    parameters[kParamCvIn].max = kNT_lastBus;
    parameters[kParamCvIn].def = 0;
    parameters[kParamCvIn].unit = kNT_unitCvInput;
    parameters[kParamCvIn].scaling = kNT_scalingNone;
    
    parameters[kParamCvAmt].name = cvAmtName;
    parameters[kParamCvAmt].min = -100;
    parameters[kParamCvAmt].max = 100;
    parameters[kParamCvAmt].def = 100;
    parameters[kParamCvAmt].unit = kNT_unitPercent;
    parameters[kParamCvAmt].scaling = kNT_scalingNone;
    
    parameters[kParamAudioIn].name = audioInName;
    parameters[kParamAudioIn].min = 1;
    parameters[kParamAudioIn].max = kNT_lastBus;
    parameters[kParamAudioIn].def = 1;
    parameters[kParamAudioIn].unit = kNT_unitAudioInput;
    parameters[kParamAudioIn].scaling = kNT_scalingNone;
    
    parameters[kParamAudioOut].name = audioOutName;
    parameters[kParamAudioOut].min = 1;
    parameters[kParamAudioOut].max = kNT_lastBus;
    parameters[kParamAudioOut].def = 1;
    parameters[kParamAudioOut].unit = kNT_unitAudioOutput;
    parameters[kParamAudioOut].scaling = kNT_scalingNone;
}

// --- Parameter Pages ---
static const uint8_t page1_params[] = { kParamLevel, kParamCurve, kParamCvIn, kParamCvAmt, kParamAudioIn, kParamAudioOut };

static _NT_parameterPage page_array[] = {
    { .name = (char*)"VCA", .numParams = ARRAY_SIZE(page1_params), .params = page1_params },
};

static _NT_parameterPages pages = {
    .numPages = ARRAY_SIZE(page_array),
    .pages = page_array
};

// --- Core API Functions ---
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VCAAlgorithm);
    req.dtc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    VCAAlgorithm* alg = new (ptrs.sram) VCAAlgorithm();
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    initParameters(alg);
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VCAAlgorithm* pThis = (VCAAlgorithm*)self;
    int numFrames = numFramesBy4 * 4;

    // Get parameters once
    float level = pThis->v[kParamLevel] * 0.01f; // 0.0 to 1.0
    bool isExponential = (pThis->v[kParamCurve] == 1);
    int cvInBus = pThis->v[kParamCvIn];
    float cvAmt = pThis->v[kParamCvAmt] * 0.01f; // -1.0 to 1.0
    int audioInBus = pThis->v[kParamAudioIn];
    int audioOutBus = pThis->v[kParamAudioOut];

    // Safety check: if input and output are the same bus, do nothing.
    if (audioInBus == audioOutBus) {
        return;
    }

    // Get bus pointers
    float* audioIn = nullptr;
    float* audioOut = nullptr;
    float* cvIn = nullptr;
    
    if (audioInBus > 0 && audioInBus <= kNT_lastBus) {
        audioIn = busFrames + ((audioInBus - 1) * numFrames);
    }
    
    if (audioOutBus > 0 && audioOutBus <= kNT_lastBus) {
        audioOut = busFrames + ((audioOutBus - 1) * numFrames);
    }
    
    if (cvInBus > 0 && cvInBus <= kNT_lastBus) {
        cvIn = busFrames + ((cvInBus - 1) * numFrames);
    }

    // If no output bus is assigned, do nothing
    if (!audioOut) {
        return;
    }

    // If no input is patched, zero the output buffer.
    if (!audioIn) {
        for (int i = 0; i < numFrames; ++i) {
            audioOut[i] = 0.0f;
        }
        return;
    }

    // Main processing loop
    for (int i = 0; i < numFrames; ++i) {
        float finalLevel = level;

        // Apply CV modulation if connected
        if (cvIn) {
            finalLevel += cvIn[i] * cvAmt * 0.1f;  // 10V CV = 100% modulation
        }

        // Clamp final level to a safe range [0, 1]
        if (finalLevel < 0.0f) finalLevel = 0.0f;
        else if (finalLevel > 1.0f) finalLevel = 1.0f;

        // Apply curve for gain calculation
        float gain = isExponential ? finalLevel * finalLevel : finalLevel;

        // Apply gain
        audioOut[i] = audioIn[i] * gain;
    }
}

// --- Factory ---
static const _NT_factory factory = {
    NT_MULTICHAR('V', 'C', 'A', '_'),
    "VCA",
    "A simple Voltage Controlled Amplifier.",
    0,
    nullptr,
    nullptr,
    nullptr,
    calculateRequirements,
    construct,
    nullptr,
    step,
    nullptr,
    nullptr,
    nullptr,
    kNT_tagUtility,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

// --- Plugin Entry Point ---
extern "C" {
    uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
        switch (selector) {
            case kNT_selector_version:
                return kNT_apiVersionCurrent;
            case kNT_selector_numFactories:
                return 1;
            case kNT_selector_factoryInfo:
                return (uintptr_t)&factory;
        }
        return 0;
    }
}
