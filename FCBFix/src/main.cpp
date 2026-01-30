#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

// FCBFix: MIDI Program Change to CV Gate converter
// - 10 programmable slots
// - Each slot: program change number (0-127) + CV output assignment
// - When matching program change is received, fires CV gate on assigned output
// - Gate duration: 100ms fixed

struct FCBFix : public _NT_algorithm {
    // Gate state for 10 slots
    int gateCounter[10];        // Countdown timer for gate pulse (in samples)
    float gateOutputs[10];      // Current gate voltage for each slot
    
    // UI state
    int selectedSlot;           // 0-9
    uint16_t lastEncoderLButton; // For debouncing left encoder button
    
    FCBFix() {
        for (int i = 0; i < 10; i++) {
            gateCounter[i] = 0;
            gateOutputs[i] = 0.0f;
        }
        selectedSlot = 0;
        lastEncoderLButton = 0;
    }
    
    void triggerGate(int slot) {
        // Set gate duration: 100ms at 48kHz = 4800 samples
        gateCounter[slot] = 4800;
        gateOutputs[slot] = 10.0f;  // 10V gate
    }
};

// =============================================================================
// Parameters
// =============================================================================

enum {
    // 10 slots × 2 parameters each (program number + output)
    kParamSlot1Program,
    kParamSlot1Output,
    kParamSlot2Program,
    kParamSlot2Output,
    kParamSlot3Program,
    kParamSlot3Output,
    kParamSlot4Program,
    kParamSlot4Output,
    kParamSlot5Program,
    kParamSlot5Output,
    kParamSlot6Program,
    kParamSlot6Output,
    kParamSlot7Program,
    kParamSlot7Output,
    kParamSlot8Program,
    kParamSlot8Output,
    kParamSlot9Program,
    kParamSlot9Output,
    kParamSlot10Program,
    kParamSlot10Output,
    kNumParameters
};

static _NT_parameter parameters[kNumParameters];

// Parameter name strings
static char slot1ProgramName[] = "Slot 1 Program";
static char slot1OutputName[] = "Slot 1 Output";
static char slot2ProgramName[] = "Slot 2 Program";
static char slot2OutputName[] = "Slot 2 Output";
static char slot3ProgramName[] = "Slot 3 Program";
static char slot3OutputName[] = "Slot 3 Output";
static char slot4ProgramName[] = "Slot 4 Program";
static char slot4OutputName[] = "Slot 4 Output";
static char slot5ProgramName[] = "Slot 5 Program";
static char slot5OutputName[] = "Slot 5 Output";
static char slot6ProgramName[] = "Slot 6 Program";
static char slot6OutputName[] = "Slot 6 Output";
static char slot7ProgramName[] = "Slot 7 Program";
static char slot7OutputName[] = "Slot 7 Output";
static char slot8ProgramName[] = "Slot 8 Program";
static char slot8OutputName[] = "Slot 8 Output";
static char slot9ProgramName[] = "Slot 9 Program";
static char slot9OutputName[] = "Slot 9 Output";
static char slot10ProgramName[] = "Slot 10 Program";
static char slot10OutputName[] = "Slot 10 Output";

void initParameters(_NT_algorithm* self) {
    // Slot 1
    parameters[kParamSlot1Program].name = slot1ProgramName;
    parameters[kParamSlot1Program].min = 0;
    parameters[kParamSlot1Program].max = 127;
    parameters[kParamSlot1Program].def = 0;
    parameters[kParamSlot1Program].unit = kNT_unitNone;
    parameters[kParamSlot1Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot1Output].name = slot1OutputName;
    parameters[kParamSlot1Output].min = 0;
    parameters[kParamSlot1Output].max = 28;
    parameters[kParamSlot1Output].def = 0;
    parameters[kParamSlot1Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot1Output].scaling = kNT_scalingNone;
    
    // Slot 2
    parameters[kParamSlot2Program].name = slot2ProgramName;
    parameters[kParamSlot2Program].min = 0;
    parameters[kParamSlot2Program].max = 127;
    parameters[kParamSlot2Program].def = 1;
    parameters[kParamSlot2Program].unit = kNT_unitNone;
    parameters[kParamSlot2Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot2Output].name = slot2OutputName;
    parameters[kParamSlot2Output].min = 0;
    parameters[kParamSlot2Output].max = 28;
    parameters[kParamSlot2Output].def = 0;
    parameters[kParamSlot2Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot2Output].scaling = kNT_scalingNone;
    
    // Slot 3
    parameters[kParamSlot3Program].name = slot3ProgramName;
    parameters[kParamSlot3Program].min = 0;
    parameters[kParamSlot3Program].max = 127;
    parameters[kParamSlot3Program].def = 2;
    parameters[kParamSlot3Program].unit = kNT_unitNone;
    parameters[kParamSlot3Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot3Output].name = slot3OutputName;
    parameters[kParamSlot3Output].min = 0;
    parameters[kParamSlot3Output].max = 28;
    parameters[kParamSlot3Output].def = 0;
    parameters[kParamSlot3Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot3Output].scaling = kNT_scalingNone;
    
    // Slot 4
    parameters[kParamSlot4Program].name = slot4ProgramName;
    parameters[kParamSlot4Program].min = 0;
    parameters[kParamSlot4Program].max = 127;
    parameters[kParamSlot4Program].def = 3;
    parameters[kParamSlot4Program].unit = kNT_unitNone;
    parameters[kParamSlot4Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot4Output].name = slot4OutputName;
    parameters[kParamSlot4Output].min = 0;
    parameters[kParamSlot4Output].max = 28;
    parameters[kParamSlot4Output].def = 0;
    parameters[kParamSlot4Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot4Output].scaling = kNT_scalingNone;
    
    // Slot 5
    parameters[kParamSlot5Program].name = slot5ProgramName;
    parameters[kParamSlot5Program].min = 0;
    parameters[kParamSlot5Program].max = 127;
    parameters[kParamSlot5Program].def = 4;
    parameters[kParamSlot5Program].unit = kNT_unitNone;
    parameters[kParamSlot5Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot5Output].name = slot5OutputName;
    parameters[kParamSlot5Output].min = 0;
    parameters[kParamSlot5Output].max = 28;
    parameters[kParamSlot5Output].def = 0;
    parameters[kParamSlot5Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot5Output].scaling = kNT_scalingNone;
    
    // Slot 6
    parameters[kParamSlot6Program].name = slot6ProgramName;
    parameters[kParamSlot6Program].min = 0;
    parameters[kParamSlot6Program].max = 127;
    parameters[kParamSlot6Program].def = 5;
    parameters[kParamSlot6Program].unit = kNT_unitNone;
    parameters[kParamSlot6Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot6Output].name = slot6OutputName;
    parameters[kParamSlot6Output].min = 0;
    parameters[kParamSlot6Output].max = 28;
    parameters[kParamSlot6Output].def = 0;
    parameters[kParamSlot6Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot6Output].scaling = kNT_scalingNone;
    
    // Slot 7
    parameters[kParamSlot7Program].name = slot7ProgramName;
    parameters[kParamSlot7Program].min = 0;
    parameters[kParamSlot7Program].max = 127;
    parameters[kParamSlot7Program].def = 6;
    parameters[kParamSlot7Program].unit = kNT_unitNone;
    parameters[kParamSlot7Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot7Output].name = slot7OutputName;
    parameters[kParamSlot7Output].min = 0;
    parameters[kParamSlot7Output].max = 28;
    parameters[kParamSlot7Output].def = 0;
    parameters[kParamSlot7Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot7Output].scaling = kNT_scalingNone;
    
    // Slot 8
    parameters[kParamSlot8Program].name = slot8ProgramName;
    parameters[kParamSlot8Program].min = 0;
    parameters[kParamSlot8Program].max = 127;
    parameters[kParamSlot8Program].def = 7;
    parameters[kParamSlot8Program].unit = kNT_unitNone;
    parameters[kParamSlot8Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot8Output].name = slot8OutputName;
    parameters[kParamSlot8Output].min = 0;
    parameters[kParamSlot8Output].max = 28;
    parameters[kParamSlot8Output].def = 0;
    parameters[kParamSlot8Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot8Output].scaling = kNT_scalingNone;
    
    // Slot 9
    parameters[kParamSlot9Program].name = slot9ProgramName;
    parameters[kParamSlot9Program].min = 0;
    parameters[kParamSlot9Program].max = 127;
    parameters[kParamSlot9Program].def = 8;
    parameters[kParamSlot9Program].unit = kNT_unitNone;
    parameters[kParamSlot9Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot9Output].name = slot9OutputName;
    parameters[kParamSlot9Output].min = 0;
    parameters[kParamSlot9Output].max = 28;
    parameters[kParamSlot9Output].def = 0;
    parameters[kParamSlot9Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot9Output].scaling = kNT_scalingNone;
    
    // Slot 10
    parameters[kParamSlot10Program].name = slot10ProgramName;
    parameters[kParamSlot10Program].min = 0;
    parameters[kParamSlot10Program].max = 127;
    parameters[kParamSlot10Program].def = 9;
    parameters[kParamSlot10Program].unit = kNT_unitNone;
    parameters[kParamSlot10Program].scaling = kNT_scalingNone;
    
    parameters[kParamSlot10Output].name = slot10OutputName;
    parameters[kParamSlot10Output].min = 0;
    parameters[kParamSlot10Output].max = 28;
    parameters[kParamSlot10Output].def = 0;
    parameters[kParamSlot10Output].unit = kNT_unitCvOutput;
    parameters[kParamSlot10Output].scaling = kNT_scalingNone;
    
    self->parameters = parameters;
}

// =============================================================================
// Parameter Pages
// =============================================================================

static const uint8_t slot1Params[] = { kParamSlot1Program, kParamSlot1Output };
static const uint8_t slot2Params[] = { kParamSlot2Program, kParamSlot2Output };
static const uint8_t slot3Params[] = { kParamSlot3Program, kParamSlot3Output };
static const uint8_t slot4Params[] = { kParamSlot4Program, kParamSlot4Output };
static const uint8_t slot5Params[] = { kParamSlot5Program, kParamSlot5Output };
static const uint8_t slot6Params[] = { kParamSlot6Program, kParamSlot6Output };
static const uint8_t slot7Params[] = { kParamSlot7Program, kParamSlot7Output };
static const uint8_t slot8Params[] = { kParamSlot8Program, kParamSlot8Output };
static const uint8_t slot9Params[] = { kParamSlot9Program, kParamSlot9Output };
static const uint8_t slot10Params[] = { kParamSlot10Program, kParamSlot10Output };

static const _NT_parameterPage parameterPages[] = {
    { .name = "Slot 1", .numParams = 2, .params = slot1Params },
    { .name = "Slot 2", .numParams = 2, .params = slot2Params },
    { .name = "Slot 3", .numParams = 2, .params = slot3Params },
    { .name = "Slot 4", .numParams = 2, .params = slot4Params },
    { .name = "Slot 5", .numParams = 2, .params = slot5Params },
    { .name = "Slot 6", .numParams = 2, .params = slot6Params },
    { .name = "Slot 7", .numParams = 2, .params = slot7Params },
    { .name = "Slot 8", .numParams = 2, .params = slot8Params },
    { .name = "Slot 9", .numParams = 2, .params = slot9Params },
    { .name = "Slot 10", .numParams = 2, .params = slot10Params },
};

static const _NT_parameterPages pages = {
    .numPages = 10,
    .pages = parameterPages
};

// =============================================================================
// Factory Functions
// =============================================================================

static void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(FCBFix);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

static _NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, 
                                 const _NT_algorithmRequirements& req,
                                 const int32_t* specifications) {
    FCBFix* a = new (ptrs.sram) FCBFix();
    initParameters(a);
    a->parameterPages = &pages;
    return a;
}

static void parameterChanged(_NT_algorithm* self, int p) {
    // No action needed for parameter changes in this plugin
}

static void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    FCBFix* a = (FCBFix*)self;
    const int numFrames = numFramesBy4 * 4;
    
    // Process gate counters and output gates
    for (int slot = 0; slot < 10; slot++) {
        if (a->gateCounter[slot] > 0) {
            a->gateCounter[slot] -= numFrames;
            if (a->gateCounter[slot] <= 0) {
                a->gateCounter[slot] = 0;
                a->gateOutputs[slot] = 0.0f;  // Gate off
            }
        }
        
        // Output gate voltage to assigned bus
        int outputBus = a->v[kParamSlot1Output + (slot * 2)];
        if (outputBus > 0 && outputBus <= 28) {
            float* bus = busFrames + ((outputBus - 1) * numFrames);
            float voltage = a->gateOutputs[slot];
            for (int i = 0; i < numFrames; i++) {
                bus[i] = voltage;
            }
        }
    }
}

static void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    FCBFix* a = (FCBFix*)self;
    
    // Check if it's a program change message (0xC0-0xCF)
    if ((byte0 & 0xF0) == 0xC0) {
        uint8_t program = byte1;  // Program number (0-127)
        
        // Check all 10 slots for a match
        for (int slot = 0; slot < 10; slot++) {
            int assignedProgram = a->v[kParamSlot1Program + (slot * 2)];
            if (program == assignedProgram) {
                a->triggerGate(slot);
            }
        }
    }
}

static bool draw(_NT_algorithm* self) {
    FCBFix* a = (FCBFix*)self;
    
    // Draw title
    NT_drawText(0, 2, "FCBFix: MIDI PC->CV Gate", 15, kNT_textLeft, kNT_textNormal);
    
    // Draw slot status (2 rows of 5 slots each)
    char buffer[32];
    int y = 20;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 5; col++) {
            int slot = row * 5 + col;
            int x = col * 51;
            
            // Slot number
            snprintf(buffer, sizeof(buffer), "%d", slot + 1);
            NT_drawText(x + 2, y, buffer, 15, kNT_textLeft, kNT_textNormal);
            
            // Program number
            int program = a->v[kParamSlot1Program + (slot * 2)];
            snprintf(buffer, sizeof(buffer), "P%d", program);
            NT_drawText(x + 12, y, buffer, 15, kNT_textLeft, kNT_textNormal);
            
            // Output assignment
            int output = a->v[kParamSlot1Output + (slot * 2)];
            if (output > 0) {
                snprintf(buffer, sizeof(buffer), "O%d", output);
                NT_drawText(x + 30, y, buffer, 15, kNT_textLeft, kNT_textNormal);
            } else {
                NT_drawText(x + 30, y, "--", 7, kNT_textLeft, kNT_textNormal);
            }
            
            // Gate indicator (bright when active)
            int color = (a->gateCounter[slot] > 0) ? 15 : 7;
            NT_drawShapeI(kNT_circle, x + 45, y + 4, x + 48, y + 7, color);
        }
        y += 20;
    }
    
    // Draw selected slot indicator
    int selectedRow = a->selectedSlot / 5;
    int selectedCol = a->selectedSlot % 5;
    int indicatorX = selectedCol * 51;
    int indicatorY = 20 + (selectedRow * 20) - 2;
    NT_drawShapeI(kNT_line, indicatorX, indicatorY, indicatorX + 48, indicatorY, 15);
    
    return true;  // Suppress default parameter line
}

static uint32_t hasCustomUi(_NT_algorithm* self) {
    return kNT_encoderL;  // Use left encoder for slot selection
}

static void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    FCBFix* a = (FCBFix*)self;
    
    // Left encoder: select slot (0-9)
    if (data.encoders[0] != 0) {
        a->selectedSlot += data.encoders[0];
        if (a->selectedSlot < 0) a->selectedSlot = 9;
        if (a->selectedSlot > 9) a->selectedSlot = 0;
    }
    
    // Left encoder button: cycle through slots
    if ((data.controls & kNT_encoderButtonL) && !(data.lastButtons & kNT_encoderButtonL)) {
        a->selectedSlot++;
        if (a->selectedSlot > 9) a->selectedSlot = 0;
    }
}

// =============================================================================
// Factory Definition
// =============================================================================

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('F','C','B','F'),
    .name = "FCBFix",
    .description = "MIDI Program Change to CV Gate converter (10 slots)",
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
    .parameterUiPrefix = nullptr,
    .parameterString = nullptr
};

// =============================================================================
// Plugin Entry Point
// =============================================================================

extern "C" {
    uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
        switch (selector) {
            case kNT_selector_version:
                return kNT_apiVersionCurrent;
            case kNT_selector_numFactories:
                return 1;
            case kNT_selector_factoryInfo:
                return (uintptr_t)&factory;
            default:
                return 0;
        }
    }
}
