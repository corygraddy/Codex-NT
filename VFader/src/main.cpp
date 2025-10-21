#include <distingnt/api.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>

#define VFADER_BUILD 16  // Increment with each build

// VFader: 64 virtual faders (params). I2C mappings set params. On change, emit 14-bit CC pairs over USB.
// UI: 8 columns per page (8 pages), encL = page, encR = selection, pots L/C/R = left/selected/right faders.

struct VFader : public _NT_algorithm {
    // state
    uint8_t page = 1;    // 1..8
    uint8_t sel = 1;     // 1..64
    bool uiActive = false; // true only while algorithm UI is being drawn
    uint8_t uiActiveTicks = 0; // small countdown after draw to absorb immediate UI events

    // change tracking (kept for UI display)
    float last[64] = {0};
    bool everSet[64] = {false};
    bool dirtyFaders[64] = {false};  // true = needs MIDI send from step()

    // config (budget retained as a parameter/display only)
    uint8_t sendBudgetPerStep = 8; // display only in minimal mode
    // pot throttling
    float potLast[3] = { -1.0f, -1.0f, -1.0f };
    uint32_t potLastStep[3] = { 0, 0, 0 };
    uint8_t minStepsBetweenPotWrites = 2;
    // step counter retained for potential future use
    uint32_t stepCounter = 0;
};

// parameters
enum {
    kParamMidiChannel = 0,   // 1..16
    kParamMidiDest,          // enum: Breakout, USB, Both, Internal, Select Bus, All
    kParamCCOrder,           // enum: High first, Low first
    kParamSendBudget,        // 1..32, default 8
    kParamFaderBase,         // 64 params follow: Fader 1..64 (0..1 scaled by 1000)
    kParamCVDestBase = kParamFaderBase + 64,  // 64 CV destination params follow
    kNumParameters = kParamCVDestBase + 64
};

static const char* const destStrings[] = { "Breakout", "USB", "Both", "Internal", "Select Bus", "All", NULL };
static const char* const orderStrings[] = { "High first", "Low first", NULL };
static const char* const cvDestStrings[] = { 
    "Off", 
    "Input 1", "Input 2", "Input 3", "Input 4", "Input 5", "Input 6", "Input 7", "Input 8",
    "Input 9", "Input 10", "Input 11", "Input 12",
    "Output 1", "Output 2", "Output 3", "Output 4", "Output 5", "Output 6", "Output 7", "Output 8",
    "Aux 1", "Aux 2", "Aux 3", "Aux 4", "Aux 5", "Aux 6", "Aux 7", "Aux 8",
    NULL 
};

static _NT_parameter parameters[kNumParameters] = {
    { .name = "MIDI channel", .min = 1, .max = 16, .def = 1, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "MIDI Dest", .min = 0, .max = 5, .def = 1, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = destStrings },
    { .name = "14-bit Order", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = orderStrings },
    { .name = "Send Budget", .min = 1, .max = 8, .def = 2, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL }, // default kept at 2 for stability tests
};

// Fader names for mapping pages
static char faderNames[64][12];
// CV destination names
static char cvDestNames[64][16];

// Initialize fader parameter definitions
static void initFaderParameters() {
    for (int i = 0; i < 64; ++i) {
        int idx = kParamFaderBase + i;
        snprintf(faderNames[i], sizeof(faderNames[i]), "Fader %02d", i + 1);
        parameters[idx].name = faderNames[i]; // used in mapping pages
        parameters[idx].min = 0;
        parameters[idx].max = 1000;
        parameters[idx].def = 0;
        parameters[idx].unit = kNT_unitNone;
        parameters[idx].scaling = kNT_scaling1000;
        parameters[idx].enumStrings = NULL;
    }
    
    // Initialize CV destination parameters
    for (int i = 0; i < 64; ++i) {
        int idx = kParamCVDestBase + i;
        snprintf(cvDestNames[i], sizeof(cvDestNames[i]), "F%02d CV Dest", i + 1);
        parameters[idx].name = cvDestNames[i];
        parameters[idx].min = 0;
        parameters[idx].max = 28;  // 0=Off, 1-12=Input1-12, 13-20=Output1-8, 21-28=Aux1-8
        parameters[idx].def = 0;   // Default to Off
        parameters[idx].unit = kNT_unitEnum;
        parameters[idx].scaling = kNT_scalingNone;
        parameters[idx].enumStrings = cvDestStrings;
    }
}

// Pages: one page for core MIDI settings + 8 pages exposing faders for I2C mapping + 8 pages for CV routing
static const uint8_t page_config_params[] = { kParamMidiChannel, kParamMidiDest, kParamCCOrder, kParamSendBudget };
static uint8_t faderPages[8][8];
static uint8_t cvDestPages[8][8];
static _NT_parameterPage page_array[17];  // 1 config + 8 fader + 8 CV dest
static _NT_parameterPages pages;

static void initPages() {
    int pageIdx = 0;
    
    // config page first
    page_array[pageIdx].name = "VFADER";
    page_array[pageIdx].numParams = (uint8_t)ARRAY_SIZE(page_config_params);
    page_array[pageIdx].params = page_config_params;
    pageIdx++;
    
    // 8 fader pages
    static char faderPageNames[8][12];
    for (int p = 0; p < 8; ++p) {
        for (int i = 0; i < 8; ++i) {
            faderPages[p][i] = (uint8_t)(kParamFaderBase + p * 8 + i);
        }
        int start = p * 8 + 1;
        int end = p * 8 + 8;
        snprintf(faderPageNames[p], sizeof(faderPageNames[p]), "FDR %02d-%02d", start, end);
        page_array[pageIdx].name = faderPageNames[p];
        page_array[pageIdx].numParams = 8;
        page_array[pageIdx].params = faderPages[p];
        pageIdx++;
    }
    
    // 8 CV destination pages
    static char cvDestPageNames[8][12];
    for (int p = 0; p < 8; ++p) {
        for (int i = 0; i < 8; ++i) {
            cvDestPages[p][i] = (uint8_t)(kParamCVDestBase + p * 8 + i);
        }
        int start = p * 8 + 1;
        int end = p * 8 + 8;
        snprintf(cvDestPageNames[p], sizeof(cvDestPageNames[p]), "CV %02d-%02d", start, end);
        page_array[pageIdx].name = cvDestPageNames[p];
        page_array[pageIdx].numParams = 8;
        page_array[pageIdx].params = cvDestPages[p];
        pageIdx++;
    }
    
    pages.numPages = pageIdx;
    pages.pages = page_array;
}

// helpers
static inline uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)(v < lo ? lo : v > hi ? hi : v); }
static inline int faderIndex(uint8_t page, uint8_t col) { return (page - 1) * 8 + col; } // 1..64

// queue removed in minimal mode

static inline uint32_t destMaskFromParam(int destParamVal) {
    switch (destParamVal) {
        case 0: return kNT_destinationBreakout;
        case 1: return kNT_destinationUSB;
        case 2: return (kNT_destinationBreakout | kNT_destinationUSB);
        case 3: return kNT_destinationInternal;
        case 4: return kNT_destinationSelectBus;
        default: return (kNT_destinationBreakout | kNT_destinationUSB | kNT_destinationInternal | kNT_destinationSelectBus);
    }
}

static void sendCCPair(uint32_t destMask, uint8_t channel1based, uint8_t faderIdx1based, float norm01, bool highFirst) {
    uint8_t status = (uint8_t)(0xB0 + ((channel1based - 1) & 0x0F));
    int full = (int)(norm01 * 16383.0f + 0.5f);
    uint8_t msb = (uint8_t)(full >> 7);
    uint8_t lsb = (uint8_t)(full & 0x7F);
    // Map as observed/requested:
    // Fader N -> MSB CC = N, LSB CC = 31+N (N<=32) or 63+N (N>32)
    uint8_t msbCC = faderIdx1based;
    uint8_t lsbCC = (faderIdx1based <= 32) ? (uint8_t)(31 + faderIdx1based) : (uint8_t)(63 + faderIdx1based);
    if (highFirst) {
        NT_sendMidi3ByteMessage(destMask, status, msbCC, msb);
        NT_sendMidi3ByteMessage(destMask, status, lsbCC, lsb);
    } else {
        NT_sendMidi3ByteMessage(destMask, status, lsbCC, lsb);
        NT_sendMidi3ByteMessage(destMask, status, msbCC, msb);
    }
}

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(VFader);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VFader* alg = new (ptrs.sram) VFader();
    initFaderParameters();
    initPages();
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VFader* a = (VFader*)self;
    int numFrames = numFramesBy4 * 4;
    
    // advance step counter and keep minimal uiActive ticking
    a->stepCounter++;
    if (a->uiActiveTicks > 0) { a->uiActive = true; --a->uiActiveTicks; }
    else { a->uiActive = false; }
    
    // CV Output: write fader values to configured output channels
    for (int i = 0; i < 64; ++i) {
        int cvDest = self->v[kParamCVDestBase + i];
        if (cvDest > 0 && cvDest <= 28) {
            // cvDest: 1-12=Input1-12, 13-20=Output1-8, 21-28=Aux1-8
            // Simple lookup table - map each destination to its physical bus
            // Determined by systematic oscilloscope testing
            
            // HARDCODED EXACT MAPPING from build 14 test
            // Build 14 used buses: In7=18, In8=19, In9=6, In10=7, In11=8, In12=9, Out1=10, Out2=11, Out3=12, Out4=13, Out5=14, Out6=15, Out7=16, Out8=17
            // Test results: In7→Out7, In8→Out8, In9→In7, In10→In8, In11→In9, In12→In10, Out1→In11, Out2→In12, Out3→Out1, Out4→Out2, Out5→Out3, Out6→Out4, Out7→Out5, Out8→Out6
            // Reverse mapping: In7 needs bus from In9(6), In8 needs bus from In10(7), In9 needs bus from In11(8), In10 needs bus from In12(9), In11 needs bus from Out1(10), In12 needs bus from Out2(11)
            //                  Out1 needs bus from Out3(12), Out2 needs bus from Out4(13), Out3 needs bus from Out5(14), Out4 needs bus from Out6(15), Out5 needs bus from Out7(16), Out6 needs bus from Out8(17), Out7 needs bus from In7(18), Out8 needs bus from In8(19)
            static const int busMap[29] = {
                -1,  // 0: unused
                0, 1, 2, 3, 4, 5,  // 1-6: Input 1-6 (PASS)
                6, 7, 8, 9, 10, 11,  // 7-12: In7, In8, In9, In10, In11, In12
                12, 13, 14, 15, 16, 17, 18, 19,  // 13-20: Out1, Out2, Out3, Out4, Out5, Out6, Out7, Out8
                20, 21, 22, 23, 24, 25, 26, 27  // 21-28: Aux 1-8 (PASS)
            };
            
            int busIndex = busMap[cvDest];
            
            if (busIndex < 28) {  // Total 28 busses
                // Get fader value (0.0-1.0) and scale to voltage (0-10V)
                float voltage = a->last[i] * 10.0f;
                
                // Write to all frames in this buffer
                for (int frame = 0; frame < numFrames; ++frame) {
                    busFrames[busIndex * numFrames + frame] = voltage;
                }
            }
        }
    }
    
    // Deferred MIDI sending: process dirty faders, throttled by sendBudgetPerStep
    // TEMPORARILY DISABLED to test UI
    /*
    uint8_t budget = (uint8_t)clampU8(self->v[kParamSendBudget], 1, 8);
    uint8_t sent = 0;
    
    for (int i = 0; i < 64 && sent < budget; ++i) {
        if (a->dirtyFaders[i]) {
            // Send the 14-bit MIDI CC pair for this fader
            uint8_t ch = (uint8_t)clampU8(self->v[kParamMidiChannel], 1, 16);
            uint32_t destMask = destMaskFromParam((int)self->v[kParamMidiDest]);
            bool highFirst = (self->v[kParamCCOrder] < 0.5f);
            sendCCPair(destMask, ch, (uint8_t)(i + 1), a->last[i], highFirst);
            
            a->dirtyFaders[i] = false;
            sent++;
        }
    }
    */
}

bool draw(_NT_algorithm* self) {
    VFader* a = (VFader*)self;
    a->uiActive = true;
    a->uiActiveTicks = 2; // keep active for a couple of steps to capture immediate controls
    // header
    if (a->page < 1 || a->page > 8) a->page = clampU8(a->page, 1, 8);
    if (a->sel < 1 || a->sel > 64) a->sel = clampU8(a->sel, 1, 64);
    int sel = a->sel;
    char header[96];
    int off = 0;
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "VF.%03d  ", VFADER_BUILD);
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "Fader %02d", sel);
    NT_drawText(8, 8, header);

    // page indicators: draw shorter boxes
    {
        int indicator_height = 6; // shorter
        int indicator_width = 32;
        int y_offset = 12; // moved up 2px
        for (int i = 1; i <= 8; ++i) {
            int x1 = (i - 1) * indicator_width;
            int x2 = x1 + indicator_width - 3;
            if (i == a->page) {
                NT_drawShapeI(kNT_rectangle, x1, y_offset, x2, indicator_height + y_offset, 15);
            } else {
                NT_drawShapeI(kNT_box, x1, y_offset, x2, indicator_height + y_offset, 7);
            }
        }
    }

    // names & numbers rows
    const int colWidth = 32;
    const int yOdd = 32, yEven = 44, yNum = 58;
    const int nameShift[8] = { 8, 8, 5, 3, -3, -5, -8, -10 };

    int localSel = ((a->sel - 1) % 8) + 1;
    int leftLocal = (localSel > 1) ? (localSel - 1) : 1;
    int rightLocal = (localSel < 8) ? (localSel + 1) : 8;

    int baseIndex = (a->page - 1) * 8;
    for (int i = 1; i <= 8; ++i) {
        int idx = baseIndex + i; // 1..64
        int colStart = (i - 1) * colWidth;
        int colEnd = i * colWidth;
        int xCenter = colStart + (colWidth / 2);

    float v = self->v[kParamFaderBase + (idx - 1)] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
    int valuePct = (int)(v * 100.0f + 0.5f);
    const char* nameStr = "FADER"; // simple label

    int shift = nameShift[i - 1];
    char numBuf[8];
    snprintf(numBuf, sizeof(numBuf), "%d", valuePct);
    NT_drawText(xCenter + shift, yNum, numBuf, 15, kNT_textCentre);

        int yName = (i % 2 == 1) ? yOdd : yEven;
        bool isSel = (i == localSel);
        int nameColour = isSel ? 15 : 7;
        if (isSel) {
            NT_drawText(colStart + (colWidth / 2) + shift, yName + 3, nameStr, 15, kNT_textCentre);
        } else {
            if (i <= 4) NT_drawText(colStart + 2 + shift, yName, nameStr, nameColour);
            else NT_drawText(colEnd - 2 + shift, yName, nameStr, nameColour, kNT_textRight);
        }

        if (i % 2 == 1) {
            int xLine = xCenter + shift;
            int yMid = ((yName + yNum) / 2) - 3; // moved down
            int halfLen = 3;
            NT_drawShapeI(kNT_line, xLine, yMid - halfLen, xLine, yMid + halfLen, 6);
        }
    }

    // triple underline under left/selected/right (centered on column centres, independent of text shift)
    auto centerXFor = [&](int iCol){ return (iCol - 1) * colWidth + (colWidth / 2); };
    int yUL = yNum + 2;
    if (yUL <= 63) {
        // Left dotted
        {
            int cx = centerXFor(leftLocal);
            int xL = cx - 12; int xR = cx + 12;
            if (xL < 0) xL = 0; if (xR > 255) xR = 255;
            for (int x = xL; x < xR; x += 4) {
                int xsegR = x + 2; if (xsegR > xR) xsegR = xR;
                NT_drawShapeI(kNT_line, x, yUL, xsegR, yUL, 10);
            }
        }
        // Middle solid, centred with a subtle +1 px nudge to the right
        {
            int cx = centerXFor(localSel) + 1;
            int xL = cx - 12; int xR = cx + 12;
            if (xL < 0) xL = 0; if (xR > 255) xR = 255;
            NT_drawShapeI(kNT_line, xL, yUL, xR, yUL, 15);
        }
        // Right dotted
        {
            int cx = centerXFor(rightLocal);
            int xL = cx - 12; int xR = cx + 12;
            if (xL < 0) xL = 0; if (xR > 255) xR = 255;
            for (int x = xL; x < xR; x += 4) {
                int xsegR = x + 2; if (xsegR > xR) xsegR = xR;
                NT_drawShapeI(kNT_line, x, yUL, xsegR, yUL, 10);
            }
        }
    }
    return true; // keep suppressing default header; change to false if needed in next step
}

uint32_t hasCustomUi(_NT_algorithm* self) { 
    (void)self; 
    return kNT_potL | kNT_potC | kNT_potR | kNT_encoderL | kNT_encoderR; 
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    VFader* a = (VFader*)self;
    
    // Get current column (0-7) within the page
    int currentCol = (a->sel - 1) % 8;
    
    // Handle encoder left (page selection)
    if (data.encoders[0] != 0) {
        int newPage = (int)a->page + data.encoders[0];
        if (newPage < 1) newPage = 1;
        if (newPage > 8) newPage = 8;
        a->page = (uint8_t)newPage;
        
        // Update sel to maintain column position on new page
        a->sel = (uint8_t)((a->page - 1) * 8 + currentCol + 1);
    }
    
    // Handle encoder right (column selection within page)
    if (data.encoders[1] != 0) {
        int newCol = currentCol + data.encoders[1];
        if (newCol < 0) newCol = 0;
        if (newCol > 7) newCol = 7;
        
        // Update sel based on current page and new column
        a->sel = (uint8_t)((a->page - 1) * 8 + newCol + 1);
    }
    
    // Handle pots (control faders relative to selected fader)
    int selectedFaderIdx = a->sel - 1;  // sel is 1-based (1-64), convert to 0-based (0-63)
    int pageBase = (a->page - 1) * 8;   // 0, 8, 16, ... 56
    int colInPage = selectedFaderIdx - pageBase;  // 0-7 column within current page
    
    // Left pot controls fader to the LEFT of selected (or disabled if selected is first on page)
    if (data.controls & kNT_potL) {
        if (colInPage > 0) {  // Not the first fader on page
            int faderIdx = selectedFaderIdx - 1;
            if (faderIdx >= 0 && faderIdx < 64) {
                int paramIdx = kParamFaderBase + faderIdx;
                int16_t newVal = (int16_t)(data.pots[0] * 1000.0f);
                NT_setParameterFromUi(NT_algorithmIndex(self), paramIdx + NT_parameterOffset(), newVal);
            }
        }
    }
    
    // Center pot always controls the SELECTED fader
    if (data.controls & kNT_potC) {
        if (selectedFaderIdx >= 0 && selectedFaderIdx < 64) {
            int paramIdx = kParamFaderBase + selectedFaderIdx;
            int16_t newVal = (int16_t)(data.pots[1] * 1000.0f);
            NT_setParameterFromUi(NT_algorithmIndex(self), paramIdx + NT_parameterOffset(), newVal);
        }
    }
    
    // Right pot controls fader to the RIGHT of selected (or disabled if selected is last on page)
    if (data.controls & kNT_potR) {
        if (colInPage < 7) {  // Not the last fader on page
            int faderIdx = selectedFaderIdx + 1;
            if (faderIdx >= 0 && faderIdx < 64) {
                int paramIdx = kParamFaderBase + faderIdx;
                int16_t newVal = (int16_t)(data.pots[2] * 1000.0f);
                NT_setParameterFromUi(NT_algorithmIndex(self), paramIdx + NT_parameterOffset(), newVal);
            }
        }
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) { 
    VFader* a = (VFader*)self;
    int pageBase = (a->page - 1) * 8;
    
    // Sync pots to current fader values
    pots[0] = a->last[pageBase + 0];  // Left pot → left-most fader
    int selCol = (a->sel - 1) % 8;
    pots[1] = a->last[pageBase + selCol];  // Center pot → selected fader
    pots[2] = a->last[pageBase + 7];  // Right pot → right-most fader
}

void parameterChanged(_NT_algorithm* self, int p) {
    VFader* a = (VFader*)self;
    if (p >= kParamFaderBase && p < (kParamFaderBase + 64)) {
        int i = p - kParamFaderBase; // 0..63
        float v = self->v[p] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
        // Cache value and mark dirty for deferred MIDI send from step()
        a->last[i] = v;
        a->everSet[i] = true;
        a->dirtyFaders[i] = true;  // Will be sent from step()
    }
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V','F','D','R'),
    .name = "VFader",
    .description = "64 virtual faders with 14-bit MIDI CC out.",
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
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
    .serialise = NULL,
    .deserialise = NULL,
    .midiSysEx = NULL
};

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version: return kNT_apiVersionCurrent;
        case kNT_selector_numFactories: return 1;
        case kNT_selector_factoryInfo: return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}
