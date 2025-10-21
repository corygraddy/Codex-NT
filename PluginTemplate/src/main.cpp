/*
 * Disting NT Plugin Template
 * 
 * This file provides a basic template for creating a new Disting NT plugin.
 */

#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <cstdio>
#include <new>

// --- Data To Continue (DTC) Struct ---
// This struct holds data that needs to be saved with a preset.
// We'll use it to store our debug information.
struct MyPluginAlgorithm_DTC {
    uint32_t stepCounter = 0;
    int32_t lastButtonPressed = -1;
    uint32_t magicNumber = 0xDEADBEEF; // To confirm serialisation is working
};

// --- Main Algorithm Struct ---
// This struct holds the live state of your algorithm.
struct MyPluginAlgorithm : public _NT_algorithm {
    MyPluginAlgorithm_DTC* dtc; // Pointer to our persistent data

    // Constructor
    MyPluginAlgorithm(MyPluginAlgorithm_DTC* dtc_ptr) : dtc(dtc_ptr) {}
};

// Forward declaration for the serialization functions
void serialise(_NT_algorithm* self, _NT_jsonStream& stream);
bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse);

// --- Parameters ---
enum {
    kParamKnob1, kParamKnob2, kParamKnob3,
    kParamButton1, kParamButton2, kParamButton3,
    kParamCVInput1, kParamAudioInput1,
    kParamCVOutput1, kParamAudioOutput1,
    kNumParameters
};

static const _NT_parameter parameters[kNumParameters] = {
    { .name = "Knob 1", .min = 0, .max = 99, .def = 0 },
    { .name = "Knob 2", .min = 0, .max = 99, .def = 50 },
    { .name = "Knob 3", .min = 0, .max = 99, .def = 99 },
    { .name = "Button 1", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Button 2", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Button 3", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "CV In 1", .min = 0, .max = 28, .def = 0, .unit = kNT_unitCvInput },
    { .name = "Audio In 1", .min = 0, .max = 28, .def = 0, .unit = kNT_unitAudioInput },
    { .name = "CV Out 1", .min = 0, .max = 28, .def = 0, .unit = kNT_unitCvOutput },
    { .name = "Audio Out 1", .min = 0, .max = 28, .def = 0, .unit = kNT_unitAudioOutput }
};

// --- Parameter Pages ---
static const uint8_t page1_params[] = { kParamKnob1, kParamKnob2, kParamKnob3, kParamButton1, kParamButton2, kParamButton3 };
static const uint8_t page2_params[] = { kParamCVInput1, kParamAudioInput1 };
static const uint8_t page3_params[] = { kParamCVOutput1, kParamAudioOutput1 };

static const _NT_parameterPage page_array[] = {
    { "MAIN", ARRAY_SIZE(page1_params), page1_params },
    { "INPUTS", ARRAY_SIZE(page2_params), page2_params },
    { "OUTPUTS", ARRAY_SIZE(page3_params), page3_params },
};

static const _NT_parameterPages pages = {
    .numPages = ARRAY_SIZE(page_array),
    .pages = page_array
};

// --- Core API Functions ---
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(MyPluginAlgorithm);
    req.dtc = sizeof(MyPluginAlgorithm_DTC); // Allocate memory for our debug data
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    MyPluginAlgorithm_DTC* dtc = new (ptrs.dtc) MyPluginAlgorithm_DTC();
    MyPluginAlgorithm* alg = new (ptrs.sram) MyPluginAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

void parameterChanged(_NT_algorithm* self, int p) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;

    // Check if one of our buttons was pressed
    if (p >= kParamButton1 && p <= kParamButton3) {
        if (pThis->v[p] == 1) {
            // Record which button was pressed in our debug data
            pThis->dtc->lastButtonPressed = p;
            // Reset the button state to make it momentary
            NT_setParameterFromUi(NT_algorithmIndex(self), p + NT_parameterOffset(), 0);
        }
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;
    int numFrames = numFramesBy4 * 4;

    // Increment our step counter for debugging
    pThis->dtc->stepCounter++;

    int audioInBus = pThis->v[kParamAudioInput1];
    int audioOutBus = pThis->v[kParamAudioOutput1];

    float* audioIn = (audioInBus > 0) ? busFrames + (audioInBus - 1) * numFrames : nullptr;
    float* audioOut = (audioOutBus > 0) ? busFrames + (audioOutBus - 1) * numFrames : nullptr;

    if (audioIn && audioOut) {
        for (int i = 0; i < numFrames; ++i) {
            audioOut[i] = audioIn[i];
        }
    } else if (audioOut) {
        for (int i = 0; i < numFrames; ++i) {
            audioOut[i] = 0.0f;
        }
    }
}

bool draw(_NT_algorithm* self) {
    NT_drawText(0, 0, "My Plugin Template");
    return false;
}

// --- Serialization (for Debugging) ---
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;
    
    // Write our debug data to the JSON preset file
    stream.addMemberName("debug_info");
    stream.openObject();
    stream.addMemberName("magicNumber");
    stream.addNumber((int)pThis->dtc->magicNumber);
    stream.addMemberName("stepCounter");
    stream.addNumber((int)pThis->dtc->stepCounter);
    stream.addMemberName("lastButtonPressed");
    stream.addNumber((int)pThis->dtc->lastButtonPressed);
    stream.closeObject();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;
    int numMembers = 0;
    if (parse.numberOfObjectMembers(numMembers)) {
        for (int i = 0; i < numMembers; ++i) {
            if (parse.matchName("debug_info")) {
                int numDebugMembers = 0;
                if (parse.numberOfObjectMembers(numDebugMembers)) {
                    for (int j = 0; j < numDebugMembers; ++j) {
                        int temp_val = 0;
                        if (parse.matchName("magicNumber")) {
                            if (parse.number(temp_val)) pThis->dtc->magicNumber = temp_val;
                        } else if (parse.matchName("stepCounter")) {
                            if (parse.number(temp_val)) pThis->dtc->stepCounter = temp_val;
                        } else if (parse.matchName("lastButtonPressed")) {
                            if (parse.number(temp_val)) pThis->dtc->lastButtonPressed = temp_val;
                        } else {
                            parse.skipMember();
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
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'P', 'L', 'T'),
    .name = "My Plugin",
    .description = "A template for creating new plugins.",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagUtility,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
    .serialise = serialise,
    .deserialise = deserialise,
    .midiSysEx = NULL
}; 

// The entry point that the Disting host calls.
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