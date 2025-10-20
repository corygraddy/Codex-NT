#include <distingnt/api.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>

// VFader: 64 virtual faders (params). I2C mappings set params. On change, emit 14-bit CC pairs over USB.
// UI: 8 columns per page (8 pages), encL = page, encR = selection, pots L/C/R = left/selected/right faders.

struct VFader : public _NT_algorithm {
    // state
    uint8_t page = 1;    // 1..8
    uint8_t sel = 1;     // 1..64
    bool uiActive = false; // true only while algorithm UI is being drawn
    uint8_t uiActiveTicks = 0; // small countdown after draw to absorb immediate UI events

    // change tracking and throttled MIDI queue
    float last[64] = {0};
    bool everSet[64] = {false};
    bool queued[64] = {false};
    uint8_t queue[64] = {0};
    uint8_t qHead = 0, qTail = 0; // circular queue of 1..64 fader indices

    // config
    uint8_t sendBudgetPerStep = 8; // CC pairs per step
    // pot throttling
    float potLast[3] = { -1.0f, -1.0f, -1.0f };
    uint32_t potLastStep[3] = { 0, 0, 0 };
    uint8_t minStepsBetweenPotWrites = 2;
    // rate limit for enqueues per fader
    uint32_t stepCounter = 0;
    uint32_t lastEnqueueStep[64] = {0};
    uint8_t minStepsBetweenEnqueues = 3; // only enqueue a given fader once per N steps
};

// parameters
enum {
    kParamMidiChannel = 0,   // 1..16
    kParamMidiDest,          // enum: Breakout, USB, Both, Internal, Select Bus, All
    kParamCCOrder,           // enum: High first, Low first
    kParamSendBudget,        // 1..32, default 8
    kParamFaderBase,         // 64 params follow: Fader 1..64 (0..1 scaled by 1000)
    kNumParameters = kParamFaderBase + 64
};

static const char* const destStrings[] = { "Breakout", "USB", "Both", "Internal", "Select Bus", "All", NULL };
static const char* const orderStrings[] = { "High first", "Low first", NULL };

static _NT_parameter parameters[kNumParameters] = {
    { .name = "MIDI channel", .min = 1, .max = 16, .def = 1, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "MIDI Dest", .min = 0, .max = 5, .def = 1, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = destStrings },
    { .name = "14-bit Order", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = orderStrings },
    { .name = "Send Budget", .min = 1, .max = 8, .def = 2, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL }, // default kept at 2 for stability tests
};

// Fader names for mapping pages
static char faderNames[64][12];

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
}

// Pages: one page for core MIDI settings + 8 pages exposing faders for I2C mapping
static const uint8_t page_config_params[] = { kParamMidiChannel, kParamMidiDest, kParamCCOrder, kParamSendBudget };
static uint8_t faderPages[8][8];
static _NT_parameterPage page_array[9];
static _NT_parameterPages pages;

static void initPages() {
    // config page first
    page_array[0].name = "VFADER";
    page_array[0].numParams = (uint8_t)ARRAY_SIZE(page_config_params);
    page_array[0].params = page_config_params;
    // 8 fader pages
    static char pageNames[8][12];
    for (int p = 0; p < 8; ++p) {
        for (int i = 0; i < 8; ++i) {
            faderPages[p][i] = (uint8_t)(kParamFaderBase + p * 8 + i);
        }
        char* name;
        // short static names like "FDR 01-08"
        int start = p * 8 + 1;
        int end = start + 7;
        snprintf(pageNames[p], sizeof(pageNames[p]), "FDR %02d-%02d", start, end);
        name = pageNames[p];
        page_array[p + 1].name = name;
        page_array[p + 1].numParams = 8;
        page_array[p + 1].params = faderPages[p];
    }
    pages.numPages = 9;
    pages.pages = page_array;
}

// helpers
static inline uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)(v < lo ? lo : v > hi ? hi : v); }
static inline int faderIndex(uint8_t page, uint8_t col) { return (page - 1) * 8 + col; } // 1..64

static void enqueue(VFader* a, uint8_t idx1based) {
    uint8_t bit = (uint8_t)(idx1based - 1);
    if (a->queued[bit]) return; // already queued
    // check full (next tail equals head)
    uint8_t nextTail = (uint8_t)((a->qTail + 1) & 63);
    if (nextTail == a->qHead) {
        // drop oldest to make room
        uint8_t old = a->queue[a->qHead];
        if (old >= 1 && old <= 64) a->queued[old - 1] = false;
        a->qHead = (uint8_t)((a->qHead + 1) & 63);
    }
    a->queue[a->qTail] = idx1based;
    a->qTail = nextTail;
    a->queued[bit] = true;
}

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
    // notify host of parameter definition changes for named faders
    for (int i = 0; i < 64; ++i) NT_updateParameterDefinition(NT_algorithmIndex(alg), kParamFaderBase + i);
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    (void)busFrames; (void)numFramesBy4;
    VFader* a = (VFader*)self;
    // advance step counter
    a->stepCounter++;
    // tick down UI active window; draw() will refresh it
    if (a->uiActiveTicks > 0) {
        a->uiActive = true;
        --a->uiActiveTicks;
    } else {
        a->uiActive = false;
    }

    // drain queue (budgeted)
    uint8_t ch = (uint8_t)clampU8(self->v[kParamMidiChannel], 1, 16);
    uint32_t destMask = destMaskFromParam((int)self->v[kParamMidiDest]);
    bool highFirst = (self->v[kParamCCOrder] < 0.5f);
    uint8_t budget = (uint8_t)clampU8((int)self->v[kParamSendBudget], 1, 8);
    a->sendBudgetPerStep = budget;
    // per-step de-dupe mask to avoid resending same fader within the same step
    bool sentThisStep[64] = {false};
    while (budget && a->qHead != a->qTail) {
        uint8_t idx1 = a->queue[a->qHead];
        a->qHead = (uint8_t)((a->qHead + 1) & 63);
        if (idx1 >= 1 && idx1 <= 64) a->queued[idx1 - 1] = false;
        if (idx1 >= 1 && idx1 <= 64 && !sentThisStep[idx1 - 1]) {
            sentThisStep[idx1 - 1] = true;
            float v = a->last[idx1 - 1];
            sendCCPair(destMask, ch, idx1, v, highFirst);
            --budget;
        }
    }
}

bool draw(_NT_algorithm* self) {
    VFader* a = (VFader*)self;
    a->uiActive = true;
    a->uiActiveTicks = 2; // keep active for a couple of steps to capture immediate controls
    // header (defensive clamp + safe formatting; omit page number to avoid confusion)
    if (a->page < 1 || a->page > 8) a->page = clampU8(a->page, 1, 8);
    if (a->sel < 1 || a->sel > 64) a->sel = clampU8(a->sel, 1, 64);
    int sel = a->sel;
    int msbCC = sel;
    int lsbCC = (sel <= 32) ? (31 + sel) : (63 + sel);
    int chDisp = (int)clampU8(self->v[kParamMidiChannel], 1, 16);
    char header[96];
    int off = 0;
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "F%02d  CC %d/", sel, msbCC);
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "%d  Ch ", lsbCC);
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "%d", chDisp);
    // diagnostics: queue size and budget
    uint8_t qSize = (uint8_t)((a->qTail - a->qHead) & 63);
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "  qS %u b %u", (unsigned)qSize, (unsigned)a->sendBudgetPerStep);
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
    return true;
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    // Advertise controls only when the GUI is visible or within grace (pre-edge-fix behavior).
    VFader* a = (VFader*)self;
    if (a->uiActive || a->uiActiveTicks > 0) {
        return (uint32_t)(kNT_encoderL | kNT_encoderR | kNT_potL | kNT_potC | kNT_potR);
    }
    return 0;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    VFader* a = (VFader*)self;
    // Only handle controls when our GUI is active
    if (!a->uiActive) return;
    // encoders
    if (data.encoders[0]) { // left encoder: page
        int delta = data.encoders[0];
        if (delta > 8) delta = 8; else if (delta < -8) delta = -8; // cap
        int p = a->page;
        if (delta > 0) {
            while (delta--) { ++p; if (p > 8) p = 1; }
        } else if (delta < 0) {
            while (delta++) { --p; if (p < 1) p = 8; }
        }
        a->page = (uint8_t)p;
        // keep selection aligned within page
        int localIdx = ((a->sel - 1) % 8) + 1;
        a->sel = (uint8_t)faderIndex(a->page, localIdx);
    }
    if (data.encoders[1]) { // right encoder: column within page
        int localIdx = ((a->sel - 1) % 8) + 1;
        int delta = data.encoders[1];
        if (delta > 8) delta = 8; else if (delta < -8) delta = -8; // cap
        // avoid negative modulo: wrap manually
        if (delta > 0) {
            while (delta--) { ++localIdx; if (localIdx > 8) localIdx = 1; }
        } else if (delta < 0) {
            while (delta++) { --localIdx; if (localIdx < 1) localIdx = 8; }
        }
        a->sel = (uint8_t)faderIndex(a->page, localIdx);
    }

    // pots: write directly to parameters (0..1 mapped by scaling1000)
    auto setParam01 = [&](int paramIndex, float value01){
        int16_t scaled = (int16_t)(value01 * 1000.0f + 0.5f);
        // avoid redundant writes if value unchanged
        int16_t cur = self->v[paramIndex];
        if (cur != scaled) {
            NT_setParameterFromUi(NT_algorithmIndex(self), paramIndex + NT_parameterOffset(), scaled);
        }
    };

    int base = kParamFaderBase - 0;
    int localSel = ((a->sel - 1) % 8) + 1;
    // Default neighbor mapping
    int colL = localSel - 1;
    int colC = localSel;
    int colR = localSel + 1;
    bool enableL = (colL >= 1);
    bool enableC = true;
    bool enableR = (colR <= 8);
    // Edge rules per user request
    if (localSel == 1) { enableL = false; colL = 1; }
    if (localSel == 8) { enableR = false; colR = 8; }

    if (a->uiActive && (data.controls & kNT_potL) && enableL) {
        int idx = faderIndex(a->page, colL);
        float v = data.pots[0];
        uint32_t elapsed = a->stepCounter - a->potLastStep[0];
    if ((a->potLast[0] < 0.0f || std::fabs(v - a->potLast[0]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[0] = v;
            a->potLastStep[0] = a->stepCounter;
        }
    }
    if (a->uiActive && (data.controls & kNT_potC) && enableC) {
        int idx = faderIndex(a->page, colC);
        float v = data.pots[1];
        uint32_t elapsed = a->stepCounter - a->potLastStep[1];
    if ((a->potLast[1] < 0.0f || std::fabs(v - a->potLast[1]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[1] = v;
            a->potLastStep[1] = a->stepCounter;
        }
    }
    if (a->uiActive && (data.controls & kNT_potR) && enableR) {
        int idx = faderIndex(a->page, colR);
        float v = data.pots[2];
        uint32_t elapsed = a->stepCounter - a->potLastStep[2];
    if ((a->potLast[2] < 0.0f || std::fabs(v - a->potLast[2]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[2] = v;
            a->potLastStep[2] = a->stepCounter;
        }
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    VFader* a = (VFader*)self;
        // Mark UI as active and give a grace window so hasCustomUi() engages immediately
        a->uiActive = true;
        a->uiActiveTicks = 8;
    int localSel = ((a->sel - 1) % 8) + 1;
    int colL = localSel - 1;
    int colC = localSel;
    int colR = localSel + 1;
    bool enableL = (colL >= 1);
    bool enableC = true;
    bool enableR = (colR <= 8);
    if (localSel == 1) { colL = 1; colC = 2; enableL = true; enableR = false; }
    else if (localSel == 8) { colC = 7; colR = 8; enableL = false; enableR = true; }
    auto val01 = [&](int col){
        int idx = faderIndex(a->page, col);
        float v = self->v[kParamFaderBase + (idx - 1)] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
        return v;
    };
        if (enableL) { pots[0] = val01(colL); a->potLast[0] = pots[0]; } else { a->potLast[0] = -1.0f; }
        if (enableC) { pots[1] = val01(colC); a->potLast[1] = pots[1]; } else { a->potLast[1] = -1.0f; }
        if (enableR) { pots[2] = val01(colR); a->potLast[2] = pots[2]; } else { a->potLast[2] = -1.0f; }
}

void parameterChanged(_NT_algorithm* self, int p) {
    VFader* a = (VFader*)self;
    if (p >= kParamFaderBase && p < (kParamFaderBase + 64)) {
        int i = p - kParamFaderBase; // 0..63
        float v = self->v[p] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
        // enqueue only if changed by at least 1 step of 1000-scale
        if (!a->everSet[i] || v != a->last[i]) {
            // rate-limit: only enqueue if enough steps elapsed since last enqueue for this fader
            if (a->everSet[i]) {
                uint32_t elapsed = a->stepCounter - a->lastEnqueueStep[i];
                if (elapsed < a->minStepsBetweenEnqueues) {
                    a->last[i] = v; // update cached value but skip enqueue
                    return;
                }
            }
            a->last[i] = v;
            a->everSet[i] = true;
            enqueue(a, (uint8_t)(i + 1));
            a->lastEnqueueStep[i] = a->stepCounter;
        }
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
#include <distingnt/api.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>

// VFader: 64 virtual faders (params). I2C mappings set params. On change, emit 14-bit CC pairs over USB.
// UI: 8 columns per page (8 pages), encL = page, encR = selection, pots L/C/R = left/selected/right faders.

struct VFader : public _NT_algorithm {
    // state
    uint8_t page = 1;    // 1..8
    uint8_t sel = 1;     // 1..64
    bool uiActive = false; // true only while algorithm UI is being drawn
    uint8_t uiActiveTicks = 0; // small countdown after draw to absorb immediate UI events

    // change tracking and throttled MIDI queue
    float last[64] = {0};
    bool everSet[64] = {false};
    bool queued[64] = {false};
    uint8_t queue[64] = {0};
    uint8_t qHead = 0, qTail = 0; // circular queue of 1..64 fader indices

    // config
    uint8_t sendBudgetPerStep = 8; // CC pairs per step
    // pot throttling
    float potLast[3] = { -1.0f, -1.0f, -1.0f };
    uint32_t potLastStep[3] = { 0, 0, 0 };
    uint8_t minStepsBetweenPotWrites = 2;
    // rate limit for enqueues per fader
    uint32_t stepCounter = 0;
    uint32_t lastEnqueueStep[64] = {0};
    uint8_t minStepsBetweenEnqueues = 3; // only enqueue a given fader once per N steps
};

// parameters
enum {
    kParamMidiChannel = 0,   // 1..16
    kParamMidiDest,          // enum: Breakout, USB, Both, Internal, Select Bus, All
    kParamCCOrder,           // enum: High first, Low first
    kParamSendBudget,        // 1..32, default 8
    kParamFaderBase,         // 64 params follow: Fader 1..64 (0..1 scaled by 1000)
    kNumParameters = kParamFaderBase + 64
};

static const char* const destStrings[] = { "Breakout", "USB", "Both", "Internal", "Select Bus", "All", NULL };
static const char* const orderStrings[] = { "High first", "Low first", NULL };

static _NT_parameter parameters[kNumParameters] = {
    { .name = "MIDI channel", .min = 1, .max = 16, .def = 1, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL },
    { .name = "MIDI Dest", .min = 0, .max = 5, .def = 1, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = destStrings },
    { .name = "14-bit Order", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = orderStrings },
    { .name = "Send Budget", .min = 1, .max = 8, .def = 2, .unit = kNT_unitNone, .scaling = kNT_scalingNone, .enumStrings = NULL }, // default kept at 2 for stability tests
};

// Fader names for mapping pages
static char faderNames[64][12];

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
}

// Pages: one page for core MIDI settings + 8 pages exposing faders for I2C mapping
static const uint8_t page_config_params[] = { kParamMidiChannel, kParamMidiDest, kParamCCOrder, kParamSendBudget };
static uint8_t faderPages[8][8];
static _NT_parameterPage page_array[9];
static _NT_parameterPages pages;

static void initPages() {
    // config page first
    page_array[0].name = "VFADER";
    page_array[0].numParams = (uint8_t)ARRAY_SIZE(page_config_params);
    page_array[0].params = page_config_params;
    // 8 fader pages
    static char pageNames[8][12];
    for (int p = 0; p < 8; ++p) {
        for (int i = 0; i < 8; ++i) {
            faderPages[p][i] = (uint8_t)(kParamFaderBase + p * 8 + i);
        }
        char* name;
        // short static names like "FDR 01-08"
        int start = p * 8 + 1;
        int end = start + 7;
        snprintf(pageNames[p], sizeof(pageNames[p]), "FDR %02d-%02d", start, end);
        name = pageNames[p];
        page_array[p + 1].name = name;
        page_array[p + 1].numParams = 8;
        page_array[p + 1].params = faderPages[p];
    }
    pages.numPages = 9;
    pages.pages = page_array;
}

// helpers
static inline uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)(v < lo ? lo : v > hi ? hi : v); }
static inline int faderIndex(uint8_t page, uint8_t col) { return (page - 1) * 8 + col; } // 1..64

static void enqueue(VFader* a, uint8_t idx1based) {
    uint8_t bit = (uint8_t)(idx1based - 1);
    if (a->queued[bit]) return; // already queued
    // check full (next tail equals head)
    uint8_t nextTail = (uint8_t)((a->qTail + 1) & 63);
    if (nextTail == a->qHead) {
        // drop oldest to make room
        uint8_t old = a->queue[a->qHead];
        if (old >= 1 && old <= 64) a->queued[old - 1] = false;
        a->qHead = (uint8_t)((a->qHead + 1) & 63);
    }
    a->queue[a->qTail] = idx1based;
    a->qTail = nextTail;
    a->queued[bit] = true;
}

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
    // notify host of parameter definition changes for named faders
    for (int i = 0; i < 64; ++i) NT_updateParameterDefinition(NT_algorithmIndex(alg), kParamFaderBase + i);
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    (void)busFrames; (void)numFramesBy4;
    VFader* a = (VFader*)self;
    // advance step counter
    a->stepCounter++;
    // tick down UI active window; draw() will refresh it
    if (a->uiActiveTicks > 0) {
        a->uiActive = true;
        --a->uiActiveTicks;
    } else {
        a->uiActive = false;
    }

    // drain queue (budgeted)
    uint8_t ch = (uint8_t)clampU8(self->v[kParamMidiChannel], 1, 16);
    uint32_t destMask = destMaskFromParam((int)self->v[kParamMidiDest]);
    bool highFirst = (self->v[kParamCCOrder] < 0.5f);
    uint8_t budget = (uint8_t)clampU8((int)self->v[kParamSendBudget], 1, 8);
    a->sendBudgetPerStep = budget;
    // per-step de-dupe mask to avoid resending same fader within the same step
    bool sentThisStep[64] = {false};
    while (budget && a->qHead != a->qTail) {
        uint8_t idx1 = a->queue[a->qHead];
        a->qHead = (uint8_t)((a->qHead + 1) & 63);
        if (idx1 >= 1 && idx1 <= 64) a->queued[idx1 - 1] = false;
        if (idx1 >= 1 && idx1 <= 64 && !sentThisStep[idx1 - 1]) {
            sentThisStep[idx1 - 1] = true;
            float v = a->last[idx1 - 1];
            sendCCPair(destMask, ch, idx1, v, highFirst);
            --budget;
        }
    }
}

bool draw(_NT_algorithm* self) {
    VFader* a = (VFader*)self;
    a->uiActive = true;
    a->uiActiveTicks = 2; // keep active for a couple of steps to capture immediate controls
    // header (defensive clamp + safe formatting; omit page number to avoid confusion)
    if (a->page < 1 || a->page > 8) a->page = clampU8(a->page, 1, 8);
    if (a->sel < 1 || a->sel > 64) a->sel = clampU8(a->sel, 1, 64);
    int sel = a->sel;
    int msbCC = sel;
    int lsbCC = (sel <= 32) ? (31 + sel) : (63 + sel);
    int chDisp = (int)clampU8(self->v[kParamMidiChannel], 1, 16);
    char header[64];
    int off = 0;
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "F%02d  CC %d/", sel, msbCC);
    off += snprintf(header + off, (size_t)(sizeof(header) - off), "%d  Ch ", lsbCC);
    snprintf(header + off, (size_t)(sizeof(header) - off), "%d", chDisp);
    NT_drawText(8, 8, header);
    // (TEST label removed per request)

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
    return true;
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    // Advertise controls only when the GUI is visible or within grace (pre-edge-fix behavior).
    VFader* a = (VFader*)self;
    if (a->uiActive || a->uiActiveTicks > 0) {
        return (uint32_t)(kNT_encoderL | kNT_encoderR | kNT_potL | kNT_potC | kNT_potR);
    }
    return 0;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    VFader* a = (VFader*)self;
    // Only handle controls when our GUI is active
    if (!a->uiActive) return;
    // encoders
    if (data.encoders[0]) { // left encoder: page
        int delta = data.encoders[0];
        if (delta > 8) delta = 8; else if (delta < -8) delta = -8; // cap
        int p = a->page;
        if (delta > 0) {
            while (delta--) { ++p; if (p > 8) p = 1; }
        } else if (delta < 0) {
            while (delta++) { --p; if (p < 1) p = 8; }
        }
        a->page = (uint8_t)p;
        // keep selection aligned within page
        int localIdx = ((a->sel - 1) % 8) + 1;
        a->sel = (uint8_t)faderIndex(a->page, localIdx);
    }
    if (data.encoders[1]) { // right encoder: column within page
        int localIdx = ((a->sel - 1) % 8) + 1;
        int delta = data.encoders[1];
        if (delta > 8) delta = 8; else if (delta < -8) delta = -8; // cap
        // avoid negative modulo: wrap manually
        if (delta > 0) {
            while (delta--) { ++localIdx; if (localIdx > 8) localIdx = 1; }
        } else if (delta < 0) {
            while (delta++) { --localIdx; if (localIdx < 1) localIdx = 8; }
        }
        a->sel = (uint8_t)faderIndex(a->page, localIdx);
    }

    // pots: write directly to parameters (0..1 mapped by scaling1000)
    auto setParam01 = [&](int paramIndex, float value01){
        int16_t scaled = (int16_t)(value01 * 1000.0f + 0.5f);
        // avoid redundant writes if value unchanged
        int16_t cur = self->v[paramIndex];
        if (cur != scaled) {
            NT_setParameterFromUi(NT_algorithmIndex(self), paramIndex + NT_parameterOffset(), scaled);
        }
    };

    int base = kParamFaderBase - 0;
    int localSel = ((a->sel - 1) % 8) + 1;
    // Default neighbor mapping
    int colL = localSel - 1;
    int colC = localSel;
    int colR = localSel + 1;
    bool enableL = (colL >= 1);
    bool enableC = true;
    bool enableR = (colR <= 8);
    // Edge rules per user request
    if (localSel == 1) { enableL = false; colL = 1; }
    if (localSel == 8) { enableR = false; colR = 8; }

    if (a->uiActive && (data.controls & kNT_potL) && enableL) {
        int idx = faderIndex(a->page, colL);
        float v = data.pots[0];
        uint32_t elapsed = a->stepCounter - a->potLastStep[0];
    if ((a->potLast[0] < 0.0f || std::fabs(v - a->potLast[0]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[0] = v;
            a->potLastStep[0] = a->stepCounter;
        }
    }
    if (a->uiActive && (data.controls & kNT_potC) && enableC) {
        int idx = faderIndex(a->page, colC);
        float v = data.pots[1];
        uint32_t elapsed = a->stepCounter - a->potLastStep[1];
    if ((a->potLast[1] < 0.0f || std::fabs(v - a->potLast[1]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[1] = v;
            a->potLastStep[1] = a->stepCounter;
        }
    }
    if (a->uiActive && (data.controls & kNT_potR) && enableR) {
        int idx = faderIndex(a->page, colR);
        float v = data.pots[2];
        uint32_t elapsed = a->stepCounter - a->potLastStep[2];
    if ((a->potLast[2] < 0.0f || std::fabs(v - a->potLast[2]) >= 0.005f) && (elapsed >= a->minStepsBetweenPotWrites)) {
            setParam01(base + (idx - 1), v);
            a->potLast[2] = v;
            a->potLastStep[2] = a->stepCounter;
        }
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    VFader* a = (VFader*)self;
        // Mark UI as active and give a grace window so hasCustomUi() engages immediately
        a->uiActive = true;
        a->uiActiveTicks = 8;
    int localSel = ((a->sel - 1) % 8) + 1;
    int colL = localSel - 1;
    int colC = localSel;
    int colR = localSel + 1;
    bool enableL = (colL >= 1);
    bool enableC = true;
    bool enableR = (colR <= 8);
    if (localSel == 1) { colL = 1; colC = 2; enableL = true; enableR = false; }
    else if (localSel == 8) { colC = 7; colR = 8; enableL = false; enableR = true; }
    auto val01 = [&](int col){
        int idx = faderIndex(a->page, col);
        float v = self->v[kParamFaderBase + (idx - 1)] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
        return v;
    };
        if (enableL) { pots[0] = val01(colL); a->potLast[0] = pots[0]; } else { a->potLast[0] = -1.0f; }
        if (enableC) { pots[1] = val01(colC); a->potLast[1] = pots[1]; } else { a->potLast[1] = -1.0f; }
        if (enableR) { pots[2] = val01(colR); a->potLast[2] = pots[2]; } else { a->potLast[2] = -1.0f; }
}

void parameterChanged(_NT_algorithm* self, int p) {
    VFader* a = (VFader*)self;
    if (p >= kParamFaderBase && p < (kParamFaderBase + 64)) {
        int i = p - kParamFaderBase; // 0..63
        float v = self->v[p] * 0.001f;
        if (v < 0) v = 0; else if (v > 1) v = 1;
        // enqueue only if changed by at least 1 step of 1000-scale
        if (!a->everSet[i] || v != a->last[i]) {
            // rate-limit: only enqueue if enough steps elapsed since last enqueue for this fader
            if (a->everSet[i]) {
                uint32_t elapsed = a->stepCounter - a->lastEnqueueStep[i];
                if (elapsed < a->minStepsBetweenEnqueues) {
                    a->last[i] = v; // update cached value but skip enqueue
                    return;
                }
            }
            a->last[i] = v;
            a->everSet[i] = true;
            enqueue(a, (uint8_t)(i + 1));
            a->lastEnqueueStep[i] = a->stepCounter;
        }
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
