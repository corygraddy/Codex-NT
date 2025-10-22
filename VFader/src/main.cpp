#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

#define VFADER_BUILD 25  // 32 faders, 4 pages, MIDI-only, soft takeover

// VFader: Simple paging architecture with MIDI output
// - 8 FADER parameters (external controls, what F8R maps to)
// - 1 PAGE parameter (selects which 8 of 32 internal faders to control)
// - 1 MIDI MODE parameter (7-bit or 14-bit MIDI CC output)
// - 32 internal faders stored in state (not as parameters)
// - Outputs: MIDI CC 1-32 (7-bit) or CC 0-31 (14-bit with standard pairing)
// - Users can route MIDI to CV using disting's MIDI→CV converter

struct VFader : public _NT_algorithm {
    // The 32 internal virtual faders (0.0-1.0)
    float internalFaders[32] = {0};
    
    // MIDI change tracking (last values sent - initialize to -1 to force first send)
    float lastMidiValues[32] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                 -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                 -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                 -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    
    // Pickup mode: track physical fader position for relative control
    float physicalFaderPos[32] = {0};  // Last known physical position (0.0-1.0 from parameter)
    float pickupPivot[32] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};  // Physical position when entering pickup mode
    float pickupStartValue[32] = {0};  // Value when entering pickup mode
    bool inPickupMode[32] = {false};  // Whether fader is in pickup/relative mode
    
    // For 14-bit: alternate between sending MSB and LSB across steps
    bool send14bitPhase = false;  // false = send MSB, true = send LSB
    
    // UI state
    uint8_t page = 1;    // 1..4 (for display)
    uint8_t lastPage = 1; // Track when page changes
    uint8_t sel = 1;     // 1..32 (for display)
    bool uiActive = false;
    uint8_t uiActiveTicks = 0;
    
    // Flag to trigger FADER parameter updates on next step()
    bool needsFaderUpdate = false;

    // Pot throttling
    float potLast[3] = { -1.0f, -1.0f, -1.0f };
    uint32_t potLastStep[3] = { 0, 0, 0 };
    uint8_t minStepsBetweenPotWrites = 2;
    uint32_t stepCounter = 0;
    
    // DEBUG tracking - captures state for JSON export
    struct DebugSnapshot {
        uint32_t stepCount;
        float fader0Value;
        float lastMidiValue0;
        bool hasControl0;
        int paramChangedCount;
        int midiSentCount;
        float lastParamChangedValue;
        uint32_t lastParamChangedStep;
        
        // Pickup mode debug tracking
        int pickupEnterCount;
        int pickupExitCount;
        float lastPhysicalPos;
        float lastPickupPivot;
        float lastPickupStartValue;
        float lastMismatch;
        bool lastCaughtUpUp;
        bool lastCaughtUpDown;
    } debugSnapshot = {0, 0.0f, -1.0f, true, 0, 0, 0.0f, 0, 0, 0, 0.0f, -1.0f, 0.0f, 0.0f, false, false};
};

// parameters - 8 FADER + 1 PAGE + 1 MIDI MODE = 10 total
enum {
    kParamFader1 = 0,    // External control faders (0-1000 scaled)
    kParamFader2,
    kParamFader3,
    kParamFader4,
    kParamFader5,
    kParamFader6,
    kParamFader7,
    kParamFader8,
    kParamPage,          // Page selector (0-7, displayed as Page 1-8)
    kParamMidiMode,      // MIDI mode: 0=7-bit, 1=14-bit
    kNumParameters
};

static const char* const pageStrings[] = { "Page 1", "Page 2", "Page 3", "Page 4", NULL };
static const char* const midiModeStrings[] = { "7-bit CC", "14-bit CC", NULL };

static _NT_parameter parameters[kNumParameters] = {};

// Fader names for mapping
static char faderNames[8][12];

// Initialize parameter definitions
static void initParameters() {
    // FADER 1-8 parameters
    for (int i = 0; i < 8; ++i) {
        snprintf(faderNames[i], sizeof(faderNames[i]), "FADER %d", i + 1);
        parameters[kParamFader1 + i].name = faderNames[i];
        parameters[kParamFader1 + i].min = 0;
        parameters[kParamFader1 + i].max = 1000;
        parameters[kParamFader1 + i].def = 0;
        parameters[kParamFader1 + i].unit = kNT_unitNone;
        parameters[kParamFader1 + i].scaling = kNT_scaling1000;
        parameters[kParamFader1 + i].enumStrings = NULL;
    }
    
    // PAGE parameter
    parameters[kParamPage].name = "PAGE";
    parameters[kParamPage].min = 0;
    parameters[kParamPage].max = 3;  // 4 pages (0-3)
    parameters[kParamPage].def = 0;
    parameters[kParamPage].unit = kNT_unitEnum;
    parameters[kParamPage].scaling = kNT_scalingNone;
    parameters[kParamPage].enumStrings = pageStrings;
    
    // MIDI MODE parameter
    parameters[kParamMidiMode].name = "MIDI Mode";
    parameters[kParamMidiMode].min = 0;
    parameters[kParamMidiMode].max = 1;  // 0=7-bit, 1=14-bit
    parameters[kParamMidiMode].def = 0;  // Default to 7-bit
    parameters[kParamMidiMode].unit = kNT_unitEnum;
    parameters[kParamMidiMode].scaling = kNT_scalingNone;
    parameters[kParamMidiMode].enumStrings = midiModeStrings;
}

// Parameter page with FADER 1-8 and MIDI Mode visible (PAGE hidden)
static uint8_t visibleParams[9];  // FADER 1-8 + MIDI Mode
static _NT_parameterPage page_array[1];
static _NT_parameterPages pages;

static void initPages() {
    // Show FADER 1-8 and MIDI Mode (hide PAGE to avoid confusion)
    for (int i = 0; i < 8; ++i) {
        visibleParams[i] = kParamFader1 + i;
    }
    visibleParams[8] = kParamMidiMode;
    
    page_array[0].name = "VFADER";
    page_array[0].numParams = 9;
    page_array[0].params = visibleParams;
    
    pages.numPages = 1;
    pages.pages = page_array;
}

// helpers
static inline uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)(v < lo ? lo : v > hi ? hi : v); }
static inline int faderIndex(uint8_t page, uint8_t col) { return (page - 1) * 8 + col; } // 1..32

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
    
    // Standard 14-bit MIDI CC mapping for 32 faders:
    // Faders 1-32: CC 0-31 (MSB) paired with CC 32-63 (LSB)
    uint8_t msbCC = faderIdx1based - 1;  // CC 0-31
    uint8_t lsbCC = msbCC + 32;          // CC 32-63
    
    // Clamp values to valid MIDI range
    if (msb > 127) msb = 127;
    if (lsb > 127) lsb = 127;
    if (msbCC > 127) msbCC = 127;
    if (lsbCC > 127) lsbCC = 127;
    
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
    initParameters();
    initPages();
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VFader* a = (VFader*)self;
    (void)busFrames;  // Not using CV output
    (void)numFramesBy4;
    
    // Advance step counter and UI ticking
    a->stepCounter++;
    if (a->uiActiveTicks > 0) { 
        a->uiActive = true; 
        --a->uiActiveTicks; 
    } else { 
        a->uiActive = false; 
    }
    
    // Update debug snapshot every 2000 steps (~40ms)
    if (a->stepCounter % 2000 == 0) {
        a->debugSnapshot.stepCount = a->stepCounter;
        a->debugSnapshot.fader0Value = a->internalFaders[0];
        a->debugSnapshot.lastMidiValue0 = a->lastMidiValues[0];
        a->debugSnapshot.hasControl0 = !a->inPickupMode[0];  // inverted: true = normal, false = pickup
    }
    
    // Get MIDI mode (0=7-bit, 1=14-bit)
    int midiMode = (int)(self->v[kParamMidiMode] + 0.5f);
    
    // MIDI settings
    uint8_t midiChannel = 1;  // Hardcoded to channel 1 for now
    uint32_t midiDest = kNT_destinationUSB | kNT_destinationInternal;  // Send to USB and Internal
    
    if (midiMode == 0) {
        // 7-bit mode: send all changed faders
        for (int i = 0; i < 32; ++i) {
            float currentValue = a->internalFaders[i];
            float lastValue = a->lastMidiValues[i];
            
            // Note: Soft takeover removed for external I2C control
            // External controllers always have immediate control
            // This could be re-enabled for onboard pot control if needed
            
            // Send MIDI if value changed (or first time)
            bool firstSend = (lastValue < 0.0f);
            bool valueChanged = fabsf(currentValue - lastValue) > 0.001f;
            
            if (firstSend || valueChanged) {
                // Convert 0.0-1.0 to 0-127 (7-bit MIDI CC)
                uint8_t midiValue = (uint8_t)(currentValue * 127.0f + 0.5f);
                if (midiValue > 127) midiValue = 127;
                
                // Send MIDI CC (CC number is i+1, so fader 0 → CC #1)
                uint8_t ccNumber = (uint8_t)(i + 1);
                uint8_t status = 0xB0 | (midiChannel - 1);  // Control Change on channel
                NT_sendMidi3ByteMessage(midiDest, status, ccNumber, midiValue);
                
                // Update last sent value
                a->lastMidiValues[i] = currentValue;
                
                // Track debug info for fader 0
                if (i == 0) {
                    a->debugSnapshot.midiSentCount++;
                }
            }
        }
    } else {
        // 14-bit mode: send MSB and LSB on alternate steps to avoid conflicts
        uint8_t status = 0xB0 | (midiChannel - 1);
        
        for (int i = 0; i < 32; ++i) {
            float currentValue = a->internalFaders[i];
            float lastValue = a->lastMidiValues[i];
            
            // Note: Soft takeover removed for external I2C control
            // External controllers always have immediate control
            
            // Check if value changed (or first time)
            bool firstSend = (lastValue < 0.0f);
            bool valueChanged = fabsf(currentValue - lastValue) > 0.001f;
            
            if (firstSend || valueChanged) {
                // Calculate 14-bit value
                int full = (int)(currentValue * 16383.0f + 0.5f);
                uint8_t msb = (uint8_t)(full >> 7);
                uint8_t lsb = (uint8_t)(full & 0x7F);
                
                // CC mapping: fader N (1-32) -> CC (N-1) for MSB, CC (N-1+32) for LSB
                uint8_t msbCC = i;           // CC 0-31
                uint8_t lsbCC = i + 32;      // CC 32-63
                
                // Clamp
                if (msb > 127) msb = 127;
                if (lsb > 127) lsb = 127;
                
                // Send MSB or LSB based on phase (order doesn't matter to disting)
                if (a->send14bitPhase) {
                    NT_sendMidi3ByteMessage(midiDest, status, lsbCC, lsb);
                } else {
                    NT_sendMidi3ByteMessage(midiDest, status, msbCC, msb);
                }
                
                // Update last sent value after both phases complete
                if (a->send14bitPhase) {
                    a->lastMidiValues[i] = currentValue;
                }
            }
        }
        
        // Toggle phase for next step
        a->send14bitPhase = !a->send14bitPhase;
    }
}

bool draw(_NT_algorithm* self) {
    VFader* a = (VFader*)self;
    a->uiActive = true;
    a->uiActiveTicks = 2; // keep active for a couple of steps to capture immediate controls
    
    // Validate bounds
    if (a->page < 1 || a->page > 4) a->page = clampU8(a->page, 1, 4);
    if (a->sel < 1 || a->sel > 32) a->sel = clampU8(a->sel, 1, 32);
    
    // Header with version
    char header[32];
    snprintf(header, sizeof(header), "VF.%03d", VFADER_BUILD);
    NT_drawText(4, 4, header, 15);

    // page indicators: draw 4 boxes (wider for 4 pages)
    {
        int indicator_height = 8;
        int indicator_width = 64;
        int y_offset = 12;
        for (int i = 1; i <= 4; ++i) {
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
    const int yOdd = 28, yEven = 40, yNum = 54;
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

        // Read value from internal faders array (0-indexed, so idx-1)
        float v = a->internalFaders[idx - 1];
        if (v < 0.0f) v = 0.0f; 
        else if (v > 1.0f) v = 1.0f;
        
        // Check if fader is in pickup mode (for visual indication)
        bool isPickup = a->inPickupMode[idx - 1];
        
        // Always display current fader value
        float displayValue = v;
        
        int valuePct = (int)(displayValue * 100.0f + 0.5f);
        const char* nameStr = "FADER";
        
        int shift = nameShift[i - 1];
        char numBuf[8];
        snprintf(numBuf, sizeof(numBuf), "%d", valuePct);
        
        // Always draw the value
        NT_drawText(xCenter + shift, yNum, numBuf, 15, kNT_textCentre);
        
        // If in pickup mode, draw a small indicator
        if (isPickup) {
            // Draw "P" next to the value to indicate pickup mode
            NT_drawText(xCenter + shift - 18, yNum, "P", 15, kNT_textLeft);
        }

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
        if (newPage > 4) newPage = 4;  // 4 pages max
        a->page = (uint8_t)newPage;
        
        // Update PAGE parameter to stay in sync
        uint32_t algIndex = NT_algorithmIndex(self);
        uint32_t paramOffset = NT_parameterOffset();
        NT_setParameterFromUi(algIndex, kParamPage + paramOffset, (int16_t)(newPage - 1));
        
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
    
    // Handle pots - write to FADER parameters to trigger pickup mode logic
    int pageBase = (a->page - 1) * 8;   // 0, 8, 16, ... 56
    int colInPage = (a->sel - 1) - pageBase;  // 0-7 column within current page
    
    uint32_t algIndex = NT_algorithmIndex(self);
    uint32_t paramOffset = NT_parameterOffset();
    
    // Left pot controls fader to the LEFT of selected (or first on page if selected is first)
    if (data.controls & kNT_potL) {
        int targetCol = (colInPage > 0) ? (colInPage - 1) : 0;
        int faderParam = targetCol;  // FADER 1-8 maps to 0-7
        int16_t value = (int16_t)(data.pots[0] * 1000.0f + 0.5f);  // Scale to 0-1000
        NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
    }
    
    // Center pot always controls the SELECTED fader's column
    if (data.controls & kNT_potC) {
        int faderParam = colInPage;  // FADER 1-8 maps to 0-7
        int16_t value = (int16_t)(data.pots[1] * 1000.0f + 0.5f);  // Scale to 0-1000
        NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
    }
    
    // Right pot controls fader to the RIGHT of selected (or last on page if selected is last)
    if (data.controls & kNT_potR) {
        int targetCol = (colInPage < 7) ? (colInPage + 1) : 7;
        int faderParam = targetCol;  // FADER 1-8 maps to 0-7
        int16_t value = (int16_t)(data.pots[2] * 1000.0f + 0.5f);  // Scale to 0-1000
        NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) { 
    VFader* a = (VFader*)self;
    int pageBase = (a->page - 1) * 8;
    int selCol = (a->sel - 1) % 8;
    
    // Sync pots to current internal fader values on this page
    // Left pot → leftmost fader on page
    // Center pot → selected fader
    // Right pot → rightmost fader on page
    pots[0] = a->internalFaders[pageBase + 0];
    pots[1] = a->internalFaders[pageBase + selCol];
    pots[2] = a->internalFaders[pageBase + 7];
}

void parameterChanged(_NT_algorithm* self, int p) {
    VFader* a = (VFader*)self;
    
    // Handle FADER 1-8 changes: write to internal faders on current page
    if (p >= kParamFader1 && p <= kParamFader8) {
        int faderIdx = p - kParamFader1;  // 0-7
        int currentPage = (int)self->v[kParamPage];  // 0-3
        int internalIdx = currentPage * 8 + faderIdx;  // 0-31
        
        // Scale parameter (0-1000) to internal value (0.0-1.0)
        float v = self->v[p] * 0.001f;
        if (v < 0.0f) v = 0.0f;
        else if (v > 1.0f) v = 1.0f;
        
        // Pickup mode logic: relative scaling when physical position ≠ value
        float currentValue = a->internalFaders[internalIdx];
        float mismatch = fabsf(v - currentValue);
        
        // Debug tracking for fader 0
        if (internalIdx == 0) {
            a->debugSnapshot.lastPhysicalPos = v;
            a->debugSnapshot.lastMismatch = mismatch;
        }
        
        // Decide whether to use pickup mode or absolute mode
        if (!a->inPickupMode[internalIdx]) {
            // Currently in absolute mode - check if we should enter pickup
            if (mismatch > 0.1f) {
                // Significant mismatch - enter pickup mode
                a->inPickupMode[internalIdx] = true;
                a->pickupPivot[internalIdx] = v;
                a->pickupStartValue[internalIdx] = currentValue;
                
                // Debug tracking for fader 0
                if (internalIdx == 0) {
                    a->debugSnapshot.pickupEnterCount++;
                    a->debugSnapshot.lastPickupPivot = v;
                    a->debugSnapshot.lastPickupStartValue = currentValue;
                }
                // Don't change value yet - will calculate on next call
            } else {
                // Close enough - use absolute value
                a->internalFaders[internalIdx] = v;
            }
        } else {
            // Already in pickup mode - calculate scaled value
            float pivotPos = a->pickupPivot[internalIdx];
            float startValue = a->pickupStartValue[internalIdx];
            float physicalDelta = v - pivotPos;
            
            // Calculate scaled target value
            float targetValue;
            if (physicalDelta > 0) {
                // Moving up from pivot
                float physicalRange = 1.0f - pivotPos;
                float valueRange = 1.0f - startValue;
                if (physicalRange > 0.001f) {
                    float ratio = physicalDelta / physicalRange;
                    if (ratio > 1.0f) ratio = 1.0f;
                    targetValue = startValue + ratio * valueRange;
                } else {
                    targetValue = 1.0f;
                }
            } else if (physicalDelta < 0) {
                // Moving down from pivot
                float physicalRange = pivotPos;
                float valueRange = startValue;
                if (physicalRange > 0.001f) {
                    float ratio = -physicalDelta / physicalRange;
                    if (ratio > 1.0f) ratio = 1.0f;
                    targetValue = startValue - ratio * valueRange;
                } else {
                    targetValue = 0.0f;
                }
            } else {
                // No movement from pivot yet
                targetValue = startValue;
            }
            
            // Clamp
            if (targetValue < 0.0f) targetValue = 0.0f;
            if (targetValue > 1.0f) targetValue = 1.0f;
            
            // Check if we should exit pickup mode
            // Exit when physical position is close to the OUTPUT value (not start value)
            float outputMismatch = fabsf(v - targetValue);
            bool caughtUp = (outputMismatch < 0.02f);  // Within 2% of output
            
            // Debug tracking for fader 0
            if (internalIdx == 0) {
                a->debugSnapshot.lastCaughtUpUp = (physicalDelta > 0 && caughtUp);
                a->debugSnapshot.lastCaughtUpDown = (physicalDelta < 0 && caughtUp);
            }
            
            if (caughtUp) {
                // Physical position has caught up with output - exit pickup mode
                a->inPickupMode[internalIdx] = false;
                a->pickupPivot[internalIdx] = -1.0f;
                a->internalFaders[internalIdx] = v;  // Now use absolute
                
                // Debug tracking for fader 0
                if (internalIdx == 0) {
                    a->debugSnapshot.pickupExitCount++;
                }
            } else {
                // Still in pickup - use scaled value
                a->internalFaders[internalIdx] = targetValue;
            }
        }
        
        // Update physical position tracking
        a->physicalFaderPos[internalIdx] = v;
        
        // Track debug info for FADER 1 (internalIdx 0 on page 0)
        if (internalIdx == 0) {
            a->debugSnapshot.paramChangedCount++;
            a->debugSnapshot.lastParamChangedValue = v;
            a->debugSnapshot.lastParamChangedStep = a->stepCounter;
        }
    }
    // Handle PAGE changes: just update display value and set flag
    else if (p == kParamPage) {
        int newPage = (int)self->v[kParamPage];  // 0-3
        a->page = (uint8_t)(newPage + 1);  // Update display value (1-4)
        
        // Don't call NT_setParameterFromUi here - it causes deadlock
        // Instead, we won't update FADER parameters at all (simpler, no freeze)
    }
}

// Serialization - write debug data to preset JSON
static void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VFader* a = static_cast<VFader*>(self);
    
    // Write debug snapshot
    stream.addMemberName("debug");
    stream.openObject();
        stream.addMemberName("stepCount");
        stream.addNumber((int)a->debugSnapshot.stepCount);
        
        stream.addMemberName("fader0Value");
        stream.addNumber(a->debugSnapshot.fader0Value);
        
        stream.addMemberName("lastMidiValue0");
        stream.addNumber(a->debugSnapshot.lastMidiValue0);
        
        stream.addMemberName("hasControl0");
        stream.addBoolean(a->debugSnapshot.hasControl0);
        
        stream.addMemberName("paramChangedCount");
        stream.addNumber(a->debugSnapshot.paramChangedCount);
        
        stream.addMemberName("midiSentCount");
        stream.addNumber(a->debugSnapshot.midiSentCount);
        
        stream.addMemberName("lastParamChangedValue");
        stream.addNumber(a->debugSnapshot.lastParamChangedValue);
        
        stream.addMemberName("lastParamChangedStep");
        stream.addNumber((int)a->debugSnapshot.lastParamChangedStep);
        
        stream.addMemberName("pickupEnterCount");
        stream.addNumber(a->debugSnapshot.pickupEnterCount);
        
        stream.addMemberName("pickupExitCount");
        stream.addNumber(a->debugSnapshot.pickupExitCount);
        
        stream.addMemberName("lastPhysicalPos");
        stream.addNumber(a->debugSnapshot.lastPhysicalPos);
        
        stream.addMemberName("lastPickupPivot");
        stream.addNumber(a->debugSnapshot.lastPickupPivot);
        
        stream.addMemberName("lastPickupStartValue");
        stream.addNumber(a->debugSnapshot.lastPickupStartValue);
        
        stream.addMemberName("lastMismatch");
        stream.addNumber(a->debugSnapshot.lastMismatch);
        
        stream.addMemberName("lastCaughtUpUp");
        stream.addBoolean(a->debugSnapshot.lastCaughtUpUp);
        
        stream.addMemberName("lastCaughtUpDown");
        stream.addBoolean(a->debugSnapshot.lastCaughtUpDown);
    stream.closeObject();
    
    // Write current state of all 32 faders
    stream.addMemberName("faders");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.addNumber(a->internalFaders[i]);
    }
    stream.closeArray();
    
    // Write soft takeover/pickup state
    stream.addMemberName("inPickupMode");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.addBoolean(a->inPickupMode[i]);
    }
    stream.closeArray();
    
    // Write last MIDI values
    stream.addMemberName("lastMidiValues");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.addNumber(a->lastMidiValues[i]);
    }
    stream.closeArray();
}

// Deserialization - we don't need to restore debug data, just return success
static bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    // Skip all members - we don't restore debug data
    int num;
    if (!parse.numberOfObjectMembers(num))
        return false;
    for (int i = 0; i < num; i++) {
        if (!parse.skipMember())
            return false;
    }
    return true;
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V','F','D','R'),
    .name = "VFader",
    .description = "VF.025 - 32 virtual faders, 7/14-bit MIDI CC, F8R control",
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
    .serialise = serialise,
    .deserialise = deserialise,
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
