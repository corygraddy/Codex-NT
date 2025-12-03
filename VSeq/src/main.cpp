#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

#define VFADER_BUILD 49  // Full 14-bit resolution: parameters 0-16383

// VFader: Simple paging architecture with MIDI output
// - 8 FADER parameters (external controls, what F8R maps to)
// - 1 PAGE parameter (selects which 8 of 32 internal faders to control)
// - 1 MIDI MODE parameter (7-bit or 14-bit MIDI CC output)
// - 32 internal faders stored in state (0.0-1.0)
// - Outputs: MIDI CC 1-32 (7-bit) or CC 0-31 (14-bit with standard pairing)
// - Parameters accept 0-16383 for full 14-bit MIDI resolution
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
    float lastPhysicalPos[32] = {0};   // Previous physical position to detect direction
    float pickupPivot[32] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};  // Physical position when entering pickup mode
    float pickupStartValue[32] = {0};  // Value when entering pickup mode
    bool inPickupMode[32] = {false};  // Whether fader is in pickup/relative mode
    bool inSlewMode[32] = {false};    // Whether fader is slewing to catch target
    float slewTarget[32] = {0};       // Target value for slewing
    uint8_t pickupSettleFrames[32] = {0};  // Frames to wait after exiting pickup before re-entering (prevent jitter)
    
    // For 14-bit: alternate between sending MSB and LSB across steps
    bool send14bitPhase = false;  // false = send MSB, true = send LSB
    
    // Sticky endpoints: hold min/max values for a few frames to ensure they register
    uint8_t endpointHoldCounter[32] = {0};  // Frames remaining to hold endpoint value
    
    // UI state
    uint8_t page = 1;    // 1..4 (for display)
    uint8_t lastPage = 1; // Track when page changes
    uint8_t sel = 1;     // 1..32 (for display)
    bool uiActive = false;
    uint8_t uiActiveTicks = 0;
    
    // Flag to trigger FADER parameter updates on next step()
    bool needsFaderUpdate = false;
    
    // Name editing state
    char faderNames[32][13] = {{0}};  // 32 faders, 12 chars + null terminator (6 name + 5 category + 1 unused)
    bool nameEditMode = false;       // Whether we're currently editing a name
    uint8_t nameEditPos = 0;         // Current character position being edited (0-10: 0-5 for name, 6-10 for category)
    uint8_t nameEditFader = 0;       // Which fader's name is being edited (0-31)
    uint8_t nameEditPage = 0;        // Which edit page: 0=name/category, 1=settings
    uint8_t nameEditSettingPos = 0;  // Which setting being edited: 0=displayMode, 1=sharpFlat, 2=bottomNote, 3=bottomOctave, 4=topNote, 5=topOctave
    float lastPotR = -1.0f;          // Last pot R value for page detection in name edit mode
    uint16_t lastButtonState = 0;    // Track last button state for debouncing
    bool namesModified = false;      // Whether names have been edited since last preset save
    
    // Per-fader note settings
    struct FaderNoteSettings {
        uint8_t displayMode;         // 0=Number (0-100), 1=Note
        uint8_t sharpFlat;           // 0=Sharp, 1=Flat
        uint8_t bottomMidi;          // 0-127 (MIDI note number, C-1 = 0, G9 = 127)
        uint8_t topMidi;             // 0-127 (MIDI note number)
        uint8_t bottomValue;         // 0-100 (for Number mode)
        uint8_t topValue;            // 0-100 (for Number mode)
        uint8_t chromaticScale[12];  // 0=off, 1=on for each note (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
        uint8_t controlAllCount;     // 0-31: number of faders to the right to control (0=disabled)
        uint8_t controlAllMode;      // 0=Absolute (offset), 1=Relative (proportional)
    };
    FaderNoteSettings faderNoteSettings[32];  // Settings for all 32 faders
    
    // Gang fader reference values - the "50%" position for each fader
    float faderReferenceValues[32] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                       0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                       0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                       0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    
    // Track last gang fader values to detect changes
    float lastGangValues[32] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
                                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    
    // Initialize note settings with defaults
    void initializeNoteSettings() {
        for (int i = 0; i < 32; i++) {
            faderNoteSettings[i].displayMode = 0;     // Default to Number mode
            faderNoteSettings[i].sharpFlat = 0;       // Default to Sharps
            faderNoteSettings[i].bottomMidi = 36;     // C1 (MIDI 36)
            faderNoteSettings[i].topMidi = 96;        // C6 (MIDI 96)
            faderNoteSettings[i].controlAllCount = 0; // Gang fader disabled by default
            faderNoteSettings[i].controlAllMode = 0;  // Default to Absolute mode
            faderNoteSettings[i].bottomValue = 0;     // 0% for Number mode
            faderNoteSettings[i].topValue = 100;      // 100% for Number mode
            // Initialize chromatic scale - all notes ON by default
            for (int j = 0; j < 12; j++) {
                faderNoteSettings[i].chromaticScale[j] = 1;
            }
        }
    }
    
    // Helper: Get note name string from MIDI note number (0-127)
    void getMidiNoteName(uint8_t midiNote, uint8_t sharpFlat, char* buffer, int bufSize) {
        static const char* noteNamesSharp[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        static const char* noteNamesFlat[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
        
        if (midiNote > 127) midiNote = 127;
        
        int octave = (midiNote / 12) - 1;  // MIDI 0-11 = octave -1, 12-23 = octave 0, etc.
        int noteInOctave = midiNote % 12;
        const char* noteName = (sharpFlat == 0) ? noteNamesSharp[noteInOctave] : noteNamesFlat[noteInOctave];
        
        // Manually build string: note + octave
        int idx = 0;
        buffer[idx++] = noteName[0];
        if (noteName[1] != '\0') {
            buffer[idx++] = noteName[1];
        }
        // Handle octave (can be -1 to 9)
        if (octave < 0) {
            buffer[idx++] = '-';
            buffer[idx++] = '1';
        } else {
            buffer[idx++] = '0' + (char)octave;
        }
        buffer[idx] = '\0';
    }
    
    // Helper: Snap fader value to active note in chromatic scale
    // Returns the MIDI note number to display/send
    int snapToActiveNote(float faderValue, const FaderNoteSettings& settings) {
        // Build list of active MIDI notes in range
        int activeNotes[128];
        int numActive = 0;
        
        for (int midi = settings.bottomMidi; midi <= settings.topMidi && midi <= 127; midi++) {
            int noteInOctave = midi % 12;
            if (settings.chromaticScale[noteInOctave] == 1) {
                activeNotes[numActive++] = midi;
            }
        }
        
        if (numActive == 0) return settings.bottomMidi; // Safety fallback
        if (numActive == 1) return activeNotes[0];      // Only one note
        
        // More aggressive handling for extremes - expand the edge zones
        // Bottom 5% always maps to first note, top 5% always maps to last note
        if (faderValue <= 0.05f) return activeNotes[0];
        if (faderValue >= 0.95f) return activeNotes[numActive - 1];
        
        // Map fader value to index in active notes array
        // Adjust the range to account for the edge zones we handled above
        // Map 0.05-0.95 range to 0-(numActive-1) indices
        float adjustedValue = (faderValue - 0.05f) / 0.9f;  // Normalize 0.05-0.95 to 0-1
        float floatIndex = adjustedValue * (numActive - 1);
        int index = (int)(floatIndex + 0.5f);  // Round to nearest
        
        if (index < 0) index = 0;
        if (index >= numActive) index = numActive - 1;
        
        return activeNotes[index];
    }
    
    // Helper: Map fader value to value range (for Number mode)
    // Returns the scaled value (0-100) to display/send
    int snapToValueRange(float faderValue, const FaderNoteSettings& settings) {
        // More aggressive handling for extremes - expand the edge zones
        // Bottom 5% always maps to bottom value, top 5% always maps to top value
        if (faderValue <= 0.05f) return settings.bottomValue;
        if (faderValue >= 0.95f) return settings.topValue;
        
        // Map fader value 0.05-0.95 to bottomValue-topValue range
        float adjustedValue = (faderValue - 0.05f) / 0.9f;  // Normalize 0.05-0.95 to 0-1
        int range = settings.topValue - settings.bottomValue;
        int scaledValue = settings.bottomValue + (int)(adjustedValue * range + 0.5f);
        
        // Clamp to ensure we stay within bounds
        if (scaledValue < settings.bottomValue) scaledValue = settings.bottomValue;
        if (scaledValue > settings.topValue) scaledValue = settings.topValue;
        
        return scaledValue;
    }

    // Pot throttling and deadband
    float potLast[3] = { -1.0f, -1.0f, -1.0f };
    uint32_t potLastStep[3] = { 0, 0, 0 };
    uint8_t minStepsBetweenPotWrites = 2;
    float potDeadband = 0.03f;  // Minimum change required (3%) to update fader - increased from 1.5% for better touch sensitivity
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
        
        // UI interaction debug tracking
        uint16_t lastButtonState;
        bool nameEditModeActive;
        uint8_t nameEditFaderIdx;
        uint8_t nameEditCursorPos;
        int encoderLCount;
        int encoderRCount;
        uint8_t currentPage;
        uint8_t currentSel;
        uint8_t nameEditPageNum;
        uint8_t nameEditSettingIdx;
        int uiFreezeCounter;
        
        // Note mode debug tracking for selected fader
        uint8_t selectedFaderDisplayMode;
        uint8_t selectedFaderBottomMidi;
        uint8_t selectedFaderTopMidi;
        uint8_t selectedFaderBottomValue;
        uint8_t selectedFaderTopValue;
        uint8_t lastSentMidiValue;
        float lastSentFaderValue;
        uint8_t snappedNoteValue;
        uint8_t scaledNumberValue;  // The 0-100 value after snapToValueRange
        
        // Pickup indicator debug - for all 32 faders
        bool pickupModeActive[32];
        float internalFaderValue[32];
        float physicalFaderValue[32];
        float pickupPivotValue[32];
        float pickupStartValueArray[32];
    };
    
    DebugSnapshot debugSnapshot = {};  // Zero-initialize all members
    
    // Constructor
    VFader() {}
};

// parameters - 8 FADER + 1 PAGE + 1 MIDI MODE + 1 PICKUP MODE + 1 DEBUG = 12 total
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
    kParamPickupMode,    // Pickup mode: 0=Scaled, 1=Catch
    kParamPotControl,    // Pot control: 0=On, 1=Off
    kParamDriftControl,  // Drift control: 0=Off, 1=Low, 2=High
    // CV Output parameters (output bus selection)
    kParamCvOut1,
    kParamCvOut1Mode,
    kParamCvOut2,
    kParamCvOut2Mode,
    kParamCvOut3,
    kParamCvOut3Mode,
    kParamCvOut4,
    kParamCvOut4Mode,
    kParamCvOut5,
    kParamCvOut5Mode,
    kParamCvOut6,
    kParamCvOut6Mode,
    kParamCvOut7,
    kParamCvOut7Mode,
    kParamCvOut8,
    kParamCvOut8Mode,
    // CV Output fader mappings (which fader controls each CV output)
    kParamCvOut1Map,
    kParamCvOut2Map,
    kParamCvOut3Map,
    kParamCvOut4Map,
    kParamCvOut5Map,
    kParamCvOut6Map,
    kParamCvOut7Map,
    kParamCvOut8Map,
    kNumParameters
};

static const char* const pageStrings[] = { "Page 1", "Page 2", "Page 3", "Page 4", NULL };
static const char* const midiModeStrings[] = { "7-bit CC", "14-bit CC", NULL };
static const char* const pickupModeStrings[] = { "Scaled", "Catch", NULL };
static const char* const potControlStrings[] = { "On", "Off", NULL };
static const char* const driftControlStrings[] = { "Off", "Low", "High", NULL };

// Fader mapping strings for CV outputs: None, Fader 1-32
static const char* const faderMapStrings[] = {
    "None", "Fader 1", "Fader 2", "Fader 3", "Fader 4", "Fader 5", "Fader 6", "Fader 7", "Fader 8",
    "Fader 9", "Fader 10", "Fader 11", "Fader 12", "Fader 13", "Fader 14", "Fader 15", "Fader 16",
    "Fader 17", "Fader 18", "Fader 19", "Fader 20", "Fader 21", "Fader 22", "Fader 23", "Fader 24",
    "Fader 25", "Fader 26", "Fader 27", "Fader 28", "Fader 29", "Fader 30", "Fader 31", "Fader 32",
    NULL
};

static _NT_parameter parameters[kNumParameters] = {};

// Fader names for mapping
static char faderNames[8][12];

// Initialize parameter definitions
static void initParameters() {
    // FADER 1-8 parameters (0-16383 for full 14-bit MIDI resolution)
    for (int i = 0; i < 8; ++i) {
        snprintf(faderNames[i], sizeof(faderNames[i]), "FADER %d", i + 1);
        parameters[kParamFader1 + i].name = faderNames[i];
        parameters[kParamFader1 + i].min = 0;
        parameters[kParamFader1 + i].max = 16383;  // Full 14-bit range
        parameters[kParamFader1 + i].def = 0;
        parameters[kParamFader1 + i].unit = kNT_unitNone;
        parameters[kParamFader1 + i].scaling = kNT_scalingNone;
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
    parameters[kParamMidiMode].def = 1;  // Default to 14-bit
    parameters[kParamMidiMode].unit = kNT_unitEnum;
    parameters[kParamMidiMode].scaling = kNT_scalingNone;
    parameters[kParamMidiMode].enumStrings = midiModeStrings;
    
    // PICKUP MODE parameter
    parameters[kParamPickupMode].name = "Pickup Mode";
    parameters[kParamPickupMode].min = 0;
    parameters[kParamPickupMode].max = 1;  // 0=Scaled, 1=Catch
    parameters[kParamPickupMode].def = 1;  // Default to Catch mode
    parameters[kParamPickupMode].unit = kNT_unitEnum;
    parameters[kParamPickupMode].scaling = kNT_scalingNone;
    parameters[kParamPickupMode].enumStrings = pickupModeStrings;
    
    // POT CONTROL parameter
    parameters[kParamPotControl].name = "Pot Control";
    parameters[kParamPotControl].min = 0;
    parameters[kParamPotControl].max = 1;  // 0=On, 1=Off
    parameters[kParamPotControl].def = 0;  // Default to On
    parameters[kParamPotControl].unit = kNT_unitEnum;
    parameters[kParamPotControl].scaling = kNT_scalingNone;
    parameters[kParamPotControl].enumStrings = potControlStrings;
    
    // DRIFT CONTROL parameter
    parameters[kParamDriftControl].name = "Drift Ctrl";
    parameters[kParamDriftControl].min = 0;
    parameters[kParamDriftControl].max = 2;  // 0=Off, 1=Low, 2=High
    parameters[kParamDriftControl].def = 0;  // Default to Off
    parameters[kParamDriftControl].unit = kNT_unitEnum;
    parameters[kParamDriftControl].scaling = kNT_scalingNone;
    parameters[kParamDriftControl].enumStrings = driftControlStrings;
    
    // CV Output parameters (8 CV outputs with mode and fader mapping)
    // Using NT_PARAMETER_CV_OUTPUT_WITH_MODE pattern manually
    for (int i = 0; i < 8; ++i) {
        // CV Output bus selection (which output 1-28)
        int cvOutParam = kParamCvOut1 + (i * 2);
        parameters[cvOutParam].name = (i == 0) ? "CV Out 1" :
                                       (i == 1) ? "CV Out 2" :
                                       (i == 2) ? "CV Out 3" :
                                       (i == 3) ? "CV Out 4" :
                                       (i == 4) ? "CV Out 5" :
                                       (i == 5) ? "CV Out 6" :
                                       (i == 6) ? "CV Out 7" : "CV Out 8";
        parameters[cvOutParam].min = 1;
        parameters[cvOutParam].max = 28;
        parameters[cvOutParam].def = i + 1;  // Default to outputs 1-8
        parameters[cvOutParam].unit = kNT_unitCvOutput;
        parameters[cvOutParam].scaling = kNT_scalingNone;
        parameters[cvOutParam].enumStrings = NULL;
        
        // CV Output mode (add/replace)
        int cvModeParam = kParamCvOut1Mode + (i * 2);
        parameters[cvModeParam].name = (i == 0) ? "CV Out 1 mode" :
                                        (i == 1) ? "CV Out 2 mode" :
                                        (i == 2) ? "CV Out 3 mode" :
                                        (i == 3) ? "CV Out 4 mode" :
                                        (i == 4) ? "CV Out 5 mode" :
                                        (i == 5) ? "CV Out 6 mode" :
                                        (i == 6) ? "CV Out 7 mode" : "CV Out 8 mode";
        parameters[cvModeParam].min = 0;
        parameters[cvModeParam].max = 1;
        parameters[cvModeParam].def = 0;
        parameters[cvModeParam].unit = kNT_unitOutputMode;
        parameters[cvModeParam].scaling = kNT_scalingNone;
        parameters[cvModeParam].enumStrings = NULL;
        
        // Fader mapping (which fader 0-32 controls this CV output)
        int mapParam = kParamCvOut1Map + i;
        parameters[mapParam].name = (i == 0) ? "CV Out 1 Fader" :
                                     (i == 1) ? "CV Out 2 Fader" :
                                     (i == 2) ? "CV Out 3 Fader" :
                                     (i == 3) ? "CV Out 4 Fader" :
                                     (i == 4) ? "CV Out 5 Fader" :
                                     (i == 5) ? "CV Out 6 Fader" :
                                     (i == 6) ? "CV Out 7 Fader" : "CV Out 8 Fader";
        parameters[mapParam].min = 0;
        parameters[mapParam].max = 32;  // 0=None, 1-32=Fader 1-32
        parameters[mapParam].def = 0;   // Default to None
        parameters[mapParam].unit = kNT_unitEnum;
        parameters[mapParam].scaling = kNT_scalingNone;
        parameters[mapParam].enumStrings = faderMapStrings;
    }
}

// Parameter pages: Single FADER page with all parameters
static uint8_t faderPageParams[12];  // FADER 1-8 + MIDI Mode + Pickup Mode + Pot Control + Drift Control
static _NT_parameterPage page_array[1];
static _NT_parameterPages pages;

static void initPages() {
    // FADER page: FADER 1-8, MIDI Mode, Pickup Mode, Pot Control, Drift Control
    for (int i = 0; i < 8; ++i) {
        faderPageParams[i] = kParamFader1 + i;
    }
    faderPageParams[8] = kParamMidiMode;
    faderPageParams[9] = kParamPickupMode;
    faderPageParams[10] = kParamPotControl;
    faderPageParams[11] = kParamDriftControl;
    
    page_array[0].name = "VFADER";
    page_array[0].numParams = 12;
    page_array[0].params = faderPageParams;
    
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
    
    // Initialize default fader names (12 chars max)
    for (int i = 0; i < 32; i++) {
        snprintf(alg->faderNames[i], 13, "FADER%02d", i + 1);
    }
    
    // Initialize note settings for all faders
    alg->initializeNoteSettings();
    
    return alg;
}

// Helper function to apply drift control
// Locks fader value until input changes by more than threshold
static inline float applyDriftControl(float newValue, float lockedValue, int driftLevel) {
    if (driftLevel == 0) {
        return newValue;  // No drift control
    }
    
    // Drift thresholds: Low=0.5%, High=1%
    float threshold;
    switch (driftLevel) {
        case 1: threshold = 0.005f; break;  // Low
        case 2: threshold = 0.01f; break;   // High
        default: threshold = 0.0f; break;
    }
    
    // If locked value is negative, this is first read - use new value
    if (lockedValue < 0.0f) {
        return newValue;
    }
    
    // Check if new value differs from locked value by more than threshold
    if (fabsf(newValue - lockedValue) > threshold) {
        return newValue;  // Update to new value
    }
    
    return lockedValue;  // Stay locked at current value
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VFader* a = (VFader*)self;
    
    // Get drift control setting
    int driftLevel = self->v[kParamDriftControl];
    
    // Advance step counter and UI ticking
    a->stepCounter++;
    if (a->uiActiveTicks > 0) { 
        a->uiActive = true; 
        --a->uiActiveTicks; 
    } else { 
        a->uiActive = false; 
    }
    
    
    // Apply gang fader transformations
    // Only apply when the gang fader itself has changed
    for (int i = 0; i < 32; i++) {
        if (a->faderNoteSettings[i].controlAllCount > 0) {
            float gangValue = a->internalFaders[i];  // 0.0 to 1.0
            float lastGangValue = a->lastGangValues[i];
            
            // Only update children if gang fader changed
            bool gangChanged = (lastGangValue < 0.0f) || (fabsf(gangValue - lastGangValue) > 0.001f);
            
            if (gangChanged) {
                int childCount = a->faderNoteSettings[i].controlAllCount;
                uint8_t mode = a->faderNoteSettings[i].controlAllMode;
                
                // For Absolute mode, calculate min/max reference values ONCE before processing children
                float minRef = 1.0f;
                float maxRef = 0.0f;
                if (mode == 0) {
                    for (int k = 1; k <= childCount && (i + k) < 32; k++) {
                        int checkIdx = i + k;
                        if (a->faderNoteSettings[checkIdx].controlAllCount > 0) continue; // Skip nested macros
                        float checkRef = a->faderReferenceValues[checkIdx];
                        if (checkRef < minRef) minRef = checkRef;
                        if (checkRef > maxRef) maxRef = checkRef;
                    }
                }
                
                // Calculate gang logical value for Absolute mode (used by all children)
                float gangLogical = 0.0f;
                if (mode == 0) {
                    float gangLogicalMin = 0.5f - maxRef;
                    float gangLogicalMax = 0.5f + (1.0f - minRef);
                    gangLogical = gangLogicalMin + (gangValue * (gangLogicalMax - gangLogicalMin));
                }
                
                // Process each child fader
                for (int j = 1; j <= childCount && (i + j) < 32; j++) {
                    int childIdx = i + j;
                    
                    // Skip if child is also a gang fader
                    if (a->faderNoteSettings[childIdx].controlAllCount > 0) continue;
                    
                    float refValue = a->faderReferenceValues[childIdx];
                    float newValue;
                    
                    if (mode == 0) {
                        // Absolute mode: children move in parallel using pre-calculated gangLogical
                        float shift = gangLogical - 0.5f;
                        newValue = refValue + shift;
                    } else {
                        // Relative mode: proportional scaling from reference position
                        // Reference value is the child's position when macro is at 0.5 (center)
                        // As macro moves from 0.5, children scale proportionally
                        
                        if (gangValue <= 0.5f) {
                            // Macro is in lower half [0.0 to 0.5]
                            // Map: macro 0.0→child reaches 0.0, macro 0.5→child stays at refValue
                            float scaleFactor = gangValue / 0.5f;  // 0.0 to 1.0
                            newValue = refValue * scaleFactor;
                        } else {
                            // Macro is in upper half [0.5 to 1.0]
                            // Map: macro 0.5→child stays at refValue, macro 1.0→child reaches 1.0
                            float t = (gangValue - 0.5f) / 0.5f;  // 0.0 to 1.0
                            newValue = refValue + ((1.0f - refValue) * t);
                        }
                    }
                    
                    // Clamp to valid range
                    if (newValue < 0.0f) newValue = 0.0f;
                    if (newValue > 1.0f) newValue = 1.0f;
                    
                    // Update child fader
                    a->internalFaders[childIdx] = newValue;
                    
                    // In Relative mode, update reference to track current child position
                    // This allows manual child adjustments to be preserved
                    if (mode == 1) {
                        a->faderReferenceValues[childIdx] = newValue;
                    }
                }
                
                // Update last gang value
                a->lastGangValues[i] = gangValue;
            }
        }
    }
    
    // Get MIDI mode (0=7-bit, 1=14-bit)
    int midiMode = (int)(self->v[kParamMidiMode] + 0.5f);
    
    // MIDI settings
    uint8_t midiChannel = 1;  // Hardcoded to channel 1 for now
    uint32_t midiDest = kNT_destinationUSB | kNT_destinationInternal;  // Send to USB and Internal
    
    if (midiMode == 0) {
        // 7-bit mode: send all changed faders
        for (int i = 0; i < 32; ++i) {
            float rawValue = a->internalFaders[i];
            float lastValue = a->lastMidiValues[i];
            
            // Apply drift control - lock value unless change exceeds threshold
            float currentValue = applyDriftControl(rawValue, lastValue, driftLevel);
            
            // Note: Soft takeover removed for external I2C control
            // External controllers always have immediate control
            // This could be re-enabled for onboard pot control if needed
            
            // Send MIDI if value changed (or first time)
            bool firstSend = (lastValue < 0.0f);
            bool valueChanged = fabsf(currentValue - lastValue) > 0.001f;
            
            if (firstSend || valueChanged) {
                uint8_t midiValue;
                int scaledValue = 0;  // Track the scaled value for debug
                
                // Check if this fader is in Note mode
                if (a->faderNoteSettings[i].displayMode == 1) {
                    // Note mode: snap to active note and send that MIDI note number
                    midiValue = (uint8_t)a->snapToActiveNote(currentValue, a->faderNoteSettings[i]);
                } else {
                    // Number mode: snap to value range (0-100), then scale to MIDI (0-127)
                    scaledValue = a->snapToValueRange(currentValue, a->faderNoteSettings[i]);
                    // Map 0-100 to 0-127
                    midiValue = (uint8_t)((scaledValue * 127) / 100);
                    if (midiValue > 127) midiValue = 127;
                }
                
                
                // Send MIDI CC (CC number is i+1, so fader 0 → CC #1)
                uint8_t ccNumber = (uint8_t)(i + 1);
                uint8_t status = 0xB0 | (midiChannel - 1);  // Control Change on channel
                NT_sendMidi3ByteMessage(midiDest, status, ccNumber, midiValue);
                
                // Update last sent value
                a->lastMidiValues[i] = currentValue;
                
            }
        }
    } else {
        // 14-bit mode: send MSB and LSB on alternate steps to avoid conflicts
        uint8_t status = 0xB0 | (midiChannel - 1);
        
        for (int i = 0; i < 32; ++i) {
            float rawValue = a->internalFaders[i];
            float lastValue = a->lastMidiValues[i];
            
            // Apply drift control - lock value unless change exceeds threshold
            float currentValue = applyDriftControl(rawValue, lastValue, driftLevel);
            
            // Note: Soft takeover removed for external I2C control
            // External controllers always have immediate control
            
            // Check if value changed (or first time)
            bool firstSend = (lastValue < 0.0f);
            bool valueChanged = fabsf(currentValue - lastValue) > 0.001f;
            
            if (firstSend || valueChanged) {
                int full;
                
                // Check if this fader is in Note mode
                if (a->faderNoteSettings[i].displayMode == 1) {
                    // Note mode: snap to active note and use that as the value (0-127)
                    uint8_t noteValue = (uint8_t)a->snapToActiveNote(currentValue, a->faderNoteSettings[i]);
                    full = noteValue << 7;  // Shift to MSB position for 14-bit
                } else {
                    // Number mode: use full 0.0-1.0 resolution, scale directly to 14-bit (0-16383)
                    // Add deadzone at min/max for reliable endpoint hitting
                    float scaledValue = currentValue;
                    bool isAtEndpoint = false;
                    
                    if (scaledValue < 0.01f) {
                        scaledValue = 0.0f;      // Snap to min
                        isAtEndpoint = true;
                    }
                    if (scaledValue > 0.99f) {
                        scaledValue = 1.0f;      // Snap to max
                        isAtEndpoint = true;
                    }
                    
                    // Sticky endpoints: hold min/max for 3 frames to ensure registration
                    if (isAtEndpoint) {
                        a->endpointHoldCounter[i] = 3;
                    }
                    
                    full = (int)(scaledValue * 16383.0f + 0.5f);
                    if (full > 16383) full = 16383;
                    if (full < 0) full = 0;
                }
                
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
            } else if (a->endpointHoldCounter[i] > 0) {
                // Not changed, but still holding endpoint - keep sending
                a->endpointHoldCounter[i]--;
                
                // Re-send the last value
                float heldValue = a->lastMidiValues[i];
                int full = (int)(heldValue * 16383.0f + 0.5f);
                if (full > 16383) full = 16383;
                if (full < 0) full = 0;
                
                uint8_t msb = (uint8_t)(full >> 7);
                uint8_t lsb = (uint8_t)(full & 0x7F);
                
                uint8_t msbCC = i;
                uint8_t lsbCC = i + 32;
                
                if (msb > 127) msb = 127;
                if (lsb > 127) lsb = 127;
                
                if (a->send14bitPhase) {
                    NT_sendMidi3ByteMessage(midiDest, status, lsbCC, lsb);
                } else {
                    NT_sendMidi3ByteMessage(midiDest, status, msbCC, msb);
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
    
    // NAME EDIT MODE DISPLAY
    if (a->nameEditMode) {
        // Bounds check nameEditFader to prevent display corruption
        if (a->nameEditFader > 31) a->nameEditFader = 0;
        
        VFader::FaderNoteSettings& settings = a->faderNoteSettings[a->nameEditFader];
        
        if (a->nameEditPage == 0) {
            // PAGE 1: NAME/CATEGORY EDITING
            
            // Title centered
            NT_drawText(128, 8, "EDIT NAME", 15, kNT_textCentre);
            
            char* name = a->faderNames[a->nameEditFader];
            int yName = 28;
            int yCat = 42;
            int xStart = 40;
            
            // "Name" label
            NT_drawText(8, yName, "Name", 15);
            
            // Name field (chars 0-5)
            for (int i = 0; i < 6; i++) {
                char c = name[i];
                if (c == 0) c = ' ';
                char buf[2] = {c, 0};
                int x = xStart + i * 10;  // 8px char width + 2px spacing
                NT_drawText(x, yName, buf, 15);
                if (i == a->nameEditPos) {
                    NT_drawShapeI(kNT_line, x, yName + 3, x + 7, yName + 3, 15);
                }
            }
            
            // "Cat" label
            NT_drawText(8, yCat, "Cat", 15);
            
            // Category field (chars 6-10, displayed as positions 0-4)
            for (int i = 6; i < 11; i++) {
                char c = name[i];
                if (c == 0) c = ' ';
                char buf[2] = {c, 0};
                int x = xStart + (i - 6) * 10;
                NT_drawText(x, yCat, buf, 15);
                if (i == a->nameEditPos) {
                    NT_drawShapeI(kNT_line, x, yCat + 3, x + 7, yCat + 3, 15);
                }
            }
        } else if (a->nameEditPage == 1) {
            // PAGE 2: FADER FUNCTION EDITING
            
            // Title centered
            NT_drawText(128, 8, "FADER FUNCTION EDIT", 15, kNT_textCentre);
            
            // Left side: Note parameters
            int xLabel = 8;
            int xValue = 79;  // Moved 3px to the right (was 76)
            int yPos = 20;
            int yStep = 10;
            
            // Display Mode
            NT_drawText(xLabel, yPos, "Display", (a->nameEditSettingPos == 0) ? 15 : 5);
            const char* displayStr = (settings.displayMode == 0) ? "Number" : "Note";
            NT_drawText(xValue, yPos, displayStr, (a->nameEditSettingPos == 0) ? 15 : 5);
            yPos += yStep;
            
            // Sharp/Flat - dim to 1 when Display is Number mode
            int accidentalLabelColor = (a->nameEditSettingPos == 1) ? 15 : 5;
            int accidentalValueColor = (a->nameEditSettingPos == 1) ? 15 : 5;
            // Override to very dark if Display is Number mode (not applicable)
            if (settings.displayMode == 0) {
                accidentalLabelColor = 1;
                accidentalValueColor = 1;
            }
            NT_drawText(xLabel, yPos, "Accidental", accidentalLabelColor);
            const char* sharpFlatStr = (settings.sharpFlat == 0) ? "Sharp" : "Flat";
            NT_drawText(xValue, yPos, sharpFlatStr, accidentalValueColor);
            yPos += yStep;
            
            // Top Value (displays as note name in Note mode, number in Number mode)
            NT_drawText(xLabel, yPos, "Top Value", (a->nameEditSettingPos == 2) ? 15 : 5);
            char topValStr[8];
            if (settings.displayMode == 1) {
                // Note mode: show note name
                a->getMidiNoteName(settings.topMidi, settings.sharpFlat, topValStr, sizeof(topValStr));
            } else {
                // Number mode: show value 0-100
                snprintf(topValStr, sizeof(topValStr), "%d", settings.topValue);
            }
            NT_drawText(xValue, yPos, topValStr, (a->nameEditSettingPos == 2) ? 15 : 5);
            yPos += yStep;
            
            // Bottom Value (displays as note name in Note mode, number in Number mode)
            NT_drawText(xLabel, yPos, "Bottom Value", (a->nameEditSettingPos == 3) ? 15 : 5);
            char botValStr[8];
            if (settings.displayMode == 1) {
                // Note mode: show note name
                a->getMidiNoteName(settings.bottomMidi, settings.sharpFlat, botValStr, sizeof(botValStr));
            } else {
                // Number mode: show value 0-100
                snprintf(botValStr, sizeof(botValStr), "%d", settings.bottomValue);
            }
            NT_drawText(xValue, yPos, botValStr, (a->nameEditSettingPos == 3) ? 15 : 5);
            
            // Right side: Note Mask (3 rows of 4 notes)
            static const char* noteNamesSharp[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            static const char* noteNamesFlat[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
            const char** noteNames = (settings.sharpFlat == 0) ? noteNamesSharp : noteNamesFlat;
            
            NT_drawText(140, 20, "Mask:", 15);
            
            int xMaskStart = 140;
            int yMaskStart = 30;
            int xSpacing = 18;
            int ySpacing = 10;
            
            // 3 rows of 4 notes each
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 4; col++) {
                    int noteIdx = row * 4 + col;
                    int x = xMaskStart + col * xSpacing;
                    int y = yMaskStart + row * ySpacing;
                    
                    bool isActive = (settings.chromaticScale[noteIdx] == 1);
                    bool isSelected = (a->nameEditSettingPos == 4 + noteIdx);  // Mask starts at position 4
                    
                    if (isActive) {
                        NT_drawText(x, y, noteNames[noteIdx], isSelected ? 15 : 5);
                    } else {
                        NT_drawText(x, y, "-", isSelected ? 15 : 5);
                    }
                }
            }
        } else if (a->nameEditPage == 2) {
            // PAGE 3: MACRO FADER SETTINGS
            
            // Title centered
            NT_drawText(128, 8, "MACRO FADER", 15, kNT_textCentre);
            
            int xLabel = 8;
            int xValue = 89;  // Moved 10px to the right (was 79)
            int yPos = 25;
            int yStep = 12;
            
            // Control Count (0-31, with dynamic max)
            NT_drawText(xLabel, yPos, "Control Count", (a->nameEditSettingPos == 0) ? 15 : 5);
            char countStr[8];
            if (settings.controlAllCount == 0) {
                snprintf(countStr, sizeof(countStr), "Off");
            } else {
                snprintf(countStr, sizeof(countStr), "%d", settings.controlAllCount);
            }
            NT_drawText(xValue, yPos, countStr, (a->nameEditSettingPos == 0) ? 15 : 5);
            yPos += yStep;
            
            // Control Mode (Absolute/Relative)
            NT_drawText(xLabel, yPos, "Control Mode", (a->nameEditSettingPos == 1) ? 15 : 5);
            const char* modeStr = (settings.controlAllMode == 0) ? "Absolute" : "Relative";
            NT_drawText(xValue, yPos, modeStr, (a->nameEditSettingPos == 1) ? 15 : 5);
            yPos += yStep;
            
            // Help text
            NT_drawText(8, yPos + 5, "Controls faders to the right", 5, kNT_textLeft, kNT_textTiny);
            NT_drawText(8, yPos + 12, "At 50% = reference values", 5, kNT_textLeft, kNT_textTiny);
        }
        
        // Page indicator and exit on right side, close together
        const char* pageStr;
        if (a->nameEditPage == 0) pageStr = "Page 1/3";
        else if (a->nameEditPage == 1) pageStr = "Page 2/3";
        else pageStr = "Page 3/3";
        NT_drawText(250, 61, pageStr, 5, kNT_textRight, kNT_textTiny);
        NT_drawText(250, 55, "R:Exit", 5, kNT_textRight, kNT_textTiny);
        
        return true;
    }
    
    // NORMAL MODE DISPLAY
    
    // Fader bank layout: 8 vertical "faders" with names and values
    const int colWidth = 28;     // Squeeze columns closer together
    const int faderHeight = 45;  // Shorter fader
    const int faderTop = 12;     // Start lower to fit numbers on top
    const int faderBottom = 57;  // Stop earlier

    int localSel = ((a->sel - 1) % 8) + 1;
    int leftLocal = (localSel > 1) ? (localSel - 1) : 1;
    int rightLocal = (localSel < 8) ? (localSel + 1) : 8;

    int baseIndex = (a->page - 1) * 8;
    for (int i = 1; i <= 8; ++i) {
        int idx = baseIndex + i; // 1..64
        int colStart = (i - 1) * colWidth;
        int xCenter = colStart + (colWidth / 2);

        // Read value from internal faders array (0-indexed, so idx-1)
        float v = a->internalFaders[idx - 1];
        if (v < 0.0f) v = 0.0f; 
        else if (v > 1.0f) v = 1.0f;
        
        // Check if fader is in pickup mode (for visual indication)
        bool isPickup = a->inPickupMode[idx - 1];
        
        bool isSel = (i == localSel);
        
        // Draw vertical fader bar (skinnier - 12px wide, centered in column)
        int faderX = colStart + 8;  // Left edge of fader (leave space for name on left)
        int faderWidth = 12;        // Skinnier fader width
        
        // Calculate filled height based on value
        int fillHeight = (int)(v * faderHeight);
        
        // Draw fader background (empty part)
        NT_drawShapeI(kNT_box, faderX, faderTop, faderX + faderWidth, faderBottom, 7);
        
        // Calculate tick mark positions
        int faderMidY = faderTop + (faderHeight / 2);
        int fader25Y = faderTop + (faderHeight * 3 / 4);  // 25% from top = 75% from bottom
        int fader75Y = faderTop + (faderHeight / 4);      // 75% from top = 25% from bottom
        
        // Draw filled part of fader as solid box
        int fillTop = faderBottom;
        if (fillHeight > 0) {
            int fillColor = isSel ? 15 : 10;
            fillTop = faderBottom - fillHeight;
            // Draw solid filled rectangle
            NT_drawShapeI(kNT_rectangle, faderX + 1, fillTop, faderX + faderWidth - 1, faderBottom - 1, fillColor);
        }
        
        // Draw tick marks at 25%, 50%, 75% - invert color if covered by fill
        // 50% mark - lines (4px) on left and right
        int tick50Color = (faderMidY >= fillTop) ? 0 : 10;  // Black if filled, bright if empty
        NT_drawShapeI(kNT_line, faderX, faderMidY, faderX + 3, faderMidY, tick50Color);
        NT_drawShapeI(kNT_line, faderX + faderWidth - 3, faderMidY, faderX + faderWidth, faderMidY, tick50Color);
        
        // 25% mark - same length lines (4px)
        int tick25Color = (fader25Y >= fillTop) ? 0 : 10;
        NT_drawShapeI(kNT_line, faderX, fader25Y, faderX + 3, fader25Y, tick25Color);
        NT_drawShapeI(kNT_line, faderX + faderWidth - 3, fader25Y, faderX + faderWidth, fader25Y, tick25Color);
        
        // 75% mark - same length lines (4px)
        int tick75Color = (fader75Y >= fillTop) ? 0 : 10;
        NT_drawShapeI(kNT_line, faderX, fader75Y, faderX + 3, fader75Y, tick75Color);
        NT_drawShapeI(kNT_line, faderX + faderWidth - 3, fader75Y, faderX + faderWidth, fader75Y, tick75Color);

        
        // Value at TOP - 1px above fader bar
        int nameColor = isSel ? 15 : 7;
        VFader::FaderNoteSettings& faderSettings = a->faderNoteSettings[idx - 1];
        
        if (faderSettings.displayMode == 1) {
            // Note mode - display note name with scale snapping
            int midiNote = a->snapToActiveNote(v, faderSettings);
            
            // Get note name
            char noteBuf[8];
            a->getMidiNoteName(midiNote, faderSettings.sharpFlat, noteBuf, sizeof(noteBuf));
            NT_drawText(xCenter + 3, faderTop - 2, noteBuf, nameColor, kNT_textCentre, kNT_textNormal);
        } else {
            // Number mode - display scaled value (respecting bottomValue/topValue range)
            int scaledValue = a->snapToValueRange(v, faderSettings);
            char valBuf[4];
            snprintf(valBuf, sizeof(valBuf), "%d", scaledValue);
            
            // Draw main value
            NT_drawText(xCenter + 3, faderTop - 2, valBuf, nameColor, kNT_textCentre, kNT_textNormal);
            
            // Add tiny decimal digit (.0-.9) to the right using pixels
            // Calculate decimal part from full resolution value
            float fullValue = v * 100.0f;  // 0.0 - 100.0
            int decimalDigit = ((int)(fullValue * 10.0f)) % 10;  // Extract tenths place
            
            // Position for decimal digit - to the right of main value
            int mainWidth = (scaledValue >= 100) ? 18 : (scaledValue >= 10) ? 12 : 6;
            int decX = xCenter + 3 + (mainWidth / 2) + 1;  // Right edge + 1px spacing
            int decY = faderTop - 1;  // Align with top of main value
            
            // Draw the decimal digit as a tiny 3x5 pixel pattern
            // Simple patterns for 0-9
            switch (decimalDigit) {
                case 0: // O shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_point, decX, decY+1, decX, decY+1, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+1, decX+2, decY+1, nameColor);
                    NT_drawShapeI(kNT_point, decX, decY+2, decX, decY+2, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_line, decX, decY+3, decX+2, decY+3, nameColor);
                    break;
                case 1: // I shape
                    NT_drawShapeI(kNT_line, decX+1, decY, decX+1, decY+3, nameColor);
                    break;
                case 2: // 2 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+1, decX+2, decY+1, nameColor);
                    NT_drawShapeI(kNT_line, decX, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_point, decX, decY+3, decX, decY+3, nameColor);
                    break;
                case 3: // 3 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_line, decX+1, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+1, decX+2, decY+1, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+3, decX+2, decY+3, nameColor);
                    break;
                case 4: // 4 shape
                    NT_drawShapeI(kNT_point, decX, decY, decX, decY+1, nameColor);
                    NT_drawShapeI(kNT_line, decX, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_line, decX+2, decY, decX+2, decY+3, nameColor);
                    break;
                case 5: // 5 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_point, decX, decY+1, decX, decY+1, nameColor);
                    NT_drawShapeI(kNT_line, decX, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+3, decX+2, decY+3, nameColor);
                    break;
                case 6: // 6 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX, decY+3, nameColor);
                    NT_drawShapeI(kNT_line, decX, decY+2, decX+2, decY+2, nameColor);
                    NT_drawShapeI(kNT_point, decX+2, decY+3, decX+2, decY+3, nameColor);
                    break;
                case 7: // 7 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_line, decX+2, decY, decX+2, decY+3, nameColor);
                    break;
                case 8: // 8 shape
                    NT_drawShapeI(kNT_rectangle, decX, decY, decX+2, decY+3, nameColor);
                    break;
                case 9: // 9 shape
                    NT_drawShapeI(kNT_line, decX, decY, decX+2, decY, nameColor);
                    NT_drawShapeI(kNT_point, decX, decY+1, decX, decY+1, nameColor);
                    NT_drawShapeI(kNT_line, decX+2, decY, decX+2, decY+3, nameColor);
                    break;
            }
        }
        
        // Pickup mode indicator - small line sticking out right side at locked value position (2px long, 3px tall)
        // Only show if there's actually a mismatch (physical != internal)
        if (isPickup) {
            float lockedValue = a->internalFaders[idx - 1];
            float physicalPos = a->physicalFaderPos[idx - 1];
            float mismatch = fabsf(physicalPos - lockedValue);
            
            // Only draw the line if there's a meaningful mismatch (>2%)
            if (mismatch > 0.02f) {
                int lockY = faderBottom - 1 - (int)(lockedValue * faderHeight);
                int lineStartX = faderX + faderWidth;
                int lineEndX = lineStartX + 2;  // 2px line extending to the right
                // Draw 3 lines for 3px height (centered)
                NT_drawShapeI(kNT_line, lineStartX, lockY - 1, lineEndX, lockY - 1, 15);
                NT_drawShapeI(kNT_line, lineStartX, lockY, lineEndX, lockY, 15);
                NT_drawShapeI(kNT_line, lineStartX, lockY + 1, lineEndX, lockY + 1, 15);
            }
        }
        
        // Draw underline indicators below fader (thicker 3px and 2px wider each direction = 4px total)
        int underlineY = faderBottom + 2;
        int underlineStartX = faderX - 4;
        int underlineEndX = faderX + faderWidth + 4;
        
        // Check pot control setting
        int potControl = (int)(self->v[kParamPotControl] + 0.5f);
        
        if (isSel) {
            // Solid line for active fader (3px thick)
            NT_drawShapeI(kNT_line, underlineStartX, underlineY, underlineEndX, underlineY, 15);
            NT_drawShapeI(kNT_line, underlineStartX, underlineY + 1, underlineEndX, underlineY + 1, 15);
            NT_drawShapeI(kNT_line, underlineStartX, underlineY + 2, underlineEndX, underlineY + 2, 15);
        } else if ((i == localSel - 1 || i == localSel + 1) && potControl == 0) {
            // Dotted line for adjacent faders (3px thick, draw every other pixel)
            // Only show if pot control is ON (0)
            for (int dotX = underlineStartX; dotX <= underlineEndX; dotX += 2) {
                NT_drawShapeI(kNT_line, dotX, underlineY, dotX, underlineY, 7);
                NT_drawShapeI(kNT_line, dotX, underlineY + 1, dotX, underlineY + 1, 7);
                NT_drawShapeI(kNT_line, dotX, underlineY + 2, dotX, underlineY + 2, 7);
            }
        }
        
        // Macro/Child indicator on right side of fader
        if (faderSettings.controlAllCount > 0) {
            // This is a macro fader - show "M"
            int indicatorX = faderX + faderWidth + 2;
            int indicatorY = faderTop + 4;
            NT_drawText(indicatorX, indicatorY, "M", 10, kNT_textLeft, kNT_textTiny);
        } else {
            // Check if this is a child of any macro fader
            for (int m = 0; m < idx - 1; m++) {
                if (a->faderNoteSettings[m].controlAllCount > 0) {
                    int childCount = a->faderNoteSettings[m].controlAllCount;
                    int firstChild = m + 1;
                    int lastChild = m + childCount;
                    if ((idx - 1) >= firstChild && (idx - 1) <= lastChild) {
                        // This fader is a child - show "C"
                        int indicatorX = faderX + faderWidth + 2;
                        int indicatorY = faderTop + 4;
                        NT_drawText(indicatorX, indicatorY, "C", 10, kNT_textLeft, kNT_textTiny);
                        break;
                    }
                }
            }
        }
        
        // Draw name vertically on LEFT side - 0px spacing between chars (tighter fit for 6 chars)
        const char* nameStr = a->faderNames[idx - 1];
        int nameLen = 0;
        for (int j = 0; j < 12 && nameStr[j] != 0; j++) nameLen++;
        if (nameLen > 6) nameLen = 6;  // Limit to 6 chars for display
        
        if (nameLen > 0) {
            int nameX = colStart + 1;  // Position name on left side of column
            int nameStartY = faderTop + 5;  // Moved down 6px from previous position (was -1, now +5)
            
            for (int charIdx = 0; charIdx < nameLen; charIdx++) {
                char buf[2] = {nameStr[charIdx], 0};
                int charY = nameStartY + charIdx * 8;  // 8px char height + 0px spacing
                if (charY >= -2 && charY <= 63) {  // Allow full range to bottom of screen
                    NT_drawText(nameX, charY, buf, nameColor, kNT_textLeft, kNT_textNormal);
                }
            }
        }
    }

    // Right side display area (no box, just content)
    int rightAreaX = 224;
    
    // Top: Large page number with "P" prefix - aligned with F below
    char pageBuf[4];
    snprintf(pageBuf, sizeof(pageBuf), "P%d", a->page);
    NT_drawText(rightAreaX, 20, pageBuf, 15, kNT_textLeft, kNT_textLarge);  // Was rightAreaX + 8, now rightAreaX to align with F
    
    // Fader number under page number - large with "F" prefix, moved down 6px
    int selectedFaderIdx = a->sel - 1;  // 0-31
    int faderNumber = selectedFaderIdx + 1;  // 1-32
    char ccBuf[8];
    snprintf(ccBuf, sizeof(ccBuf), "F%d", faderNumber);
    NT_drawText(rightAreaX, 41, ccBuf, 15, kNT_textLeft, kNT_textLarge);  // Was 38, now 38 + 3 = 41
    
    // Category (chars 6-10) - moved left 3px and down 3px from previous position
    const char* selectedName = a->faderNames[selectedFaderIdx];
    int catY = 53;  // Was 50, now 50 + 3 = 53
    
    // Display category with normal font
    char catBuf[6] = {0};
    for (int i = 0; i < 5; i++) {
        catBuf[i] = selectedName[6 + i];
        if (catBuf[i] == 0) catBuf[i] = ' ';
    }
    NT_drawText(rightAreaX - 5, catY, catBuf, 15, kNT_textLeft, kNT_textNormal);  // Changed back to kNT_textNormal
    
    // Build number in bottom right corner (tiny font)
    NT_drawText(236, 60, "B48", 15, kNT_textLeft, kNT_textTiny);
    
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
    int currentFader = (a->sel - 1);  // 0-31 index
    
    // Detect button press (not hold) - only trigger on rising edge
    bool leftButtonPressed = (data.controls & kNT_encoderButtonL) && !(a->lastButtonState & kNT_encoderButtonL);
    bool rightButtonPressed = (data.controls & kNT_encoderButtonR) && !(a->lastButtonState & kNT_encoderButtonR);
    a->lastButtonState = data.controls;
    
    
    // NAME EDIT MODE
    if (a->nameEditMode) {
        // Right encoder: navigate between pages or edit values
        // Limit encoder delta to prevent freeze
        int encoderDelta = data.encoders[1];
        if (encoderDelta > 1) encoderDelta = 1;
        if (encoderDelta < -1) encoderDelta = -1;
        
        if (encoderDelta != 0) {
            if (a->nameEditPage == 0) {
                // PAGE 1: Editing name/category characters
                char* name = a->faderNames[a->nameEditFader];
                char c = name[a->nameEditPos];
                
                // Character set: A-Z (65-90), 0-9 (48-57), space (32)
                // Array: space, 0-9, A-Z (37 total characters)
                const char charset[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                const int charsetLen = 37;
                
                // Find current position in charset
                int currentIdx = 0;
                if (c == 0) c = 'A';  // Initialize if null
                for (int i = 0; i < charsetLen; i++) {
                    if (charset[i] == c) {
                        currentIdx = i;
                        break;
                    }
                }
                
                // Move to next/previous character
                currentIdx += encoderDelta;
                if (currentIdx < 0) currentIdx = charsetLen - 1;
                if (currentIdx >= charsetLen) currentIdx = 0;
                
                name[a->nameEditPos] = charset[currentIdx];
            } else if (a->nameEditPage == 1) {
                // PAGE 2: Editing settings or mask
                VFader::FaderNoteSettings& settings = a->faderNoteSettings[a->nameEditFader];
                bool settingsChanged = false;
                
                if (a->nameEditSettingPos < 4) {
                    // Editing settings (0-3)
                    switch (a->nameEditSettingPos) {
                        case 0: // Display Mode (Number/Note)
                            settings.displayMode = (settings.displayMode == 0) ? 1 : 0;
                            settingsChanged = true;
                            break;
                        case 1: // Sharp/Flat
                            settings.sharpFlat = (settings.sharpFlat == 0) ? 1 : 0;
                            settingsChanged = true;
                            break;
                        case 2: // Top Value (MIDI note in Note mode, 0-100 value in Number mode)
                            if (settings.displayMode == 1) {
                                // Note mode: edit MIDI note (0-127)
                                int newMidi = (int)settings.topMidi + encoderDelta;
                                if (newMidi < 0) newMidi = 0;
                                if (newMidi > 127) newMidi = 127;
                                settings.topMidi = (uint8_t)newMidi;
                                
                                // Validate: top can't be lower than bottom
                                if (settings.topMidi < settings.bottomMidi) {
                                    settings.bottomMidi = settings.topMidi;
                                }
                            } else {
                                // Number mode: edit value (0-100)
                                int newValue = (int)settings.topValue + encoderDelta;
                                if (newValue < 0) newValue = 0;
                                if (newValue > 100) newValue = 100;
                                
                                // Validate: top must be greater than bottom (not equal)
                                if (newValue <= settings.bottomValue) {
                                    newValue = settings.bottomValue + 1;
                                    if (newValue > 100) newValue = 100;
                                }
                                settings.topValue = (uint8_t)newValue;
                            }
                            settingsChanged = true;
                            break;
                        case 3: // Bottom Value (MIDI note in Note mode, 0-100 value in Number mode)
                            if (settings.displayMode == 1) {
                                // Note mode: edit MIDI note (0-127)
                                int newMidi = (int)settings.bottomMidi + encoderDelta;
                                if (newMidi < 0) newMidi = 0;
                                if (newMidi > 127) newMidi = 127;
                                settings.bottomMidi = (uint8_t)newMidi;
                                
                                // Validate: bottom can't be higher than top
                                if (settings.bottomMidi > settings.topMidi) {
                                    settings.topMidi = settings.bottomMidi;
                                }
                            } else {
                                // Number mode: edit value (0-100)
                                int newValue = (int)settings.bottomValue + encoderDelta;
                                if (newValue < 0) newValue = 0;
                                if (newValue > 100) newValue = 100;
                                
                                // Validate: bottom must be less than top (not equal)
                                if (newValue >= settings.topValue) {
                                    newValue = settings.topValue - 1;
                                    if (newValue < 0) newValue = 0;
                                }
                                settings.bottomValue = (uint8_t)newValue;
                            }
                            settingsChanged = true;
                            break;
                    }
                } else {
                    // Editing mask (4-15 maps to notes 0-11)
                    int maskIdx = a->nameEditSettingPos - 4;
                    if (maskIdx >= 0 && maskIdx < 12) {
                        // Check if we're trying to turn off the last active note
                        if (settings.chromaticScale[maskIdx] == 1) {
                            // Count active notes
                            int activeCount = 0;
                            for (int i = 0; i < 12; i++) {
                                if (settings.chromaticScale[i] == 1) activeCount++;
                            }
                            // Only allow turning off if at least 2 notes are active
                            if (activeCount > 1) {
                                settings.chromaticScale[maskIdx] = 0;
                                settingsChanged = true;
                            }
                        } else {
                            // Turning on is always allowed
                            settings.chromaticScale[maskIdx] = 1;
                            settingsChanged = true;
                        }
                    }
                }
                
                if (settingsChanged) {
                    a->namesModified = true;  // Mark settings as modified
                    // Invalidate MIDI cache for this fader to force re-send with new settings
                    a->lastMidiValues[a->nameEditFader] = -1.0f;
                }
            } else if (a->nameEditPage == 2) {
                // PAGE 3: Gang fader settings
                VFader::FaderNoteSettings& settings = a->faderNoteSettings[a->nameEditFader];
                bool settingsChanged = false;
                
                switch (a->nameEditSettingPos) {
                    case 0: // Control Count (0-31)
                        {
                            uint8_t oldCount = settings.controlAllCount;
                            int newCount = (int)settings.controlAllCount + encoderDelta;
                            if (newCount < 0) newCount = 0;
                            if (newCount > 31) newCount = 31;
                            
                            // Calculate max based on fader position and next gang fader
                            int faderIdx = a->nameEditFader;  // 0-31
                            int maxPossible = 31 - faderIdx;  // Can't control beyond fader 31
                            
                            // Find next gang fader to the right (if any)
                            for (int i = faderIdx + 1; i < 32; i++) {
                                if (a->faderNoteSettings[i].controlAllCount > 0) {
                                    maxPossible = i - faderIdx - 1;
                                    break;
                                }
                            }
                            
                            if (newCount > maxPossible) newCount = maxPossible;
                            settings.controlAllCount = (uint8_t)newCount;
                            
                            // If gang fader was just created (0 -> non-zero), initialize children's reference values
                            if (oldCount == 0 && newCount > 0) {
                                for (int j = 1; j <= newCount && (faderIdx + j) < 32; j++) {
                                    int childIdx = faderIdx + j;
                                    // Set reference to current position (don't snap!)
                                    a->faderReferenceValues[childIdx] = a->internalFaders[childIdx];
                                }
                                // Initialize lastGangValues to current macro position (not -1.0!)
                                // This prevents the first macro movement from causing incorrect transforms
                                a->lastGangValues[faderIdx] = a->internalFaders[faderIdx];
                            }
                            
                            settingsChanged = true;
                        }
                        break;
                    case 1: // Control Mode (Absolute/Relative)
                        settings.controlAllMode = (settings.controlAllMode == 0) ? 1 : 0;
                        settingsChanged = true;
                        break;
                }
                
                if (settingsChanged) {
                    a->namesModified = true;
                }
            }
        }
        
        // Left encoder: move cursor position (page 1) or setting selection (page 2/3)
        if (data.encoders[0] != 0) {
            if (a->nameEditPage == 0) {
                // PAGE 1: Move character position
                int newPos = (int)a->nameEditPos + data.encoders[0];
                if (newPos < 0) newPos = 0;
                if (newPos > 10) newPos = 10;  // 0-10 for 11 characters (6 name + 5 category)
                a->nameEditPos = (uint8_t)newPos;
            } else if (a->nameEditPage == 1) {
                // PAGE 2: Move between settings (4 settings + 12 mask notes = 16 total)
                int newSettingPos = (int)a->nameEditSettingPos + data.encoders[0];
                if (newSettingPos < 0) newSettingPos = 0;
                if (newSettingPos > 15) newSettingPos = 15;  // 0-3: settings, 4-15: mask notes
                a->nameEditSettingPos = (uint8_t)newSettingPos;
            } else if (a->nameEditPage == 2) {
                // PAGE 3: Move between gang fader settings (2 settings)
                int newSettingPos = (int)a->nameEditSettingPos + data.encoders[0];
                if (newSettingPos < 0) newSettingPos = 0;
                if (newSettingPos > 1) newSettingPos = 1;  // 0-1: Control Count, Control Mode
                a->nameEditSettingPos = (uint8_t)newSettingPos;
            }
        }
        
        // Top right pot (pot R): switch between edit pages (3 pages)
        // Pot value: < 0.33 = Page 1, 0.33-0.66 = Page 2, >= 0.66 = Page 3
        if (data.controls & kNT_potR) {
            float potValue = data.pots[2];
            // Check if pot moved significantly (to avoid jitter)
            if (a->lastPotR < 0.0f || fabsf(potValue - a->lastPotR) > 0.1f) {
                if (potValue < 0.33f) {
                    a->nameEditPage = 0;
                } else if (potValue < 0.66f) {
                    a->nameEditPage = 1;
                } else {
                    a->nameEditPage = 2;
                }
                a->lastPotR = potValue;
            }
        }
        
        // Right encoder button: exit name edit mode (only on press, not hold)
        if (rightButtonPressed) {
            a->nameEditMode = false;
            a->namesModified = true;  // Mark that names have been changed
            a->lastPotR = -1.0f;  // Reset pot tracking
        }
        
        return;  // Don't process normal UI in name edit mode
    }
    
    // NORMAL MODE
    
    // Right encoder button: Enter name edit mode for selected fader (only on press, not hold)
    if (rightButtonPressed) {
        a->nameEditMode = true;
        a->nameEditFader = currentFader;
        // Bounds check
        if (a->nameEditFader > 31) a->nameEditFader = 0;
        a->nameEditPos = 0;
        a->nameEditPage = 0;  // Start on page 1 (name/category)
        a->nameEditSettingPos = 0;
        a->lastPotR = -1.0f;  // Reset pot tracking for page switching
        return;
    }
    
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
    
    // Check if pot control is enabled (0=On, 1=Off)
    int potControl = (int)(self->v[kParamPotControl] + 0.5f);
    
    // Only process pot input if pot control is ON
    if (potControl == 0) {
        // Left pot controls fader to the LEFT of selected (does nothing if first fader selected)
        if (data.controls & kNT_potL) {
            if (colInPage > 0) {  // Only active if not first fader
                float potValue = data.pots[0];
                // Apply min/max deadzones to pot values (3% to match parameter logic)
                if (potValue < 0.03f) potValue = 0.0f;
                else if (potValue > 0.97f) potValue = 1.0f;
                // Quantize pot to 2500 steps to reduce jitter/backtracking
                potValue = floorf(potValue * 2500.0f + 0.5f) / 2500.0f;
                // Apply deadband - only update if change is significant
                if (a->potLast[0] < 0.0f || fabsf(potValue - a->potLast[0]) > a->potDeadband) {
                    int targetCol = colInPage - 1;
                    int faderParam = targetCol;  // FADER 1-8 maps to 0-7
                    int16_t value = (int16_t)(potValue * 16383.0f + 0.5f);  // Scale to 0-16383
                    NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
                    a->potLast[0] = potValue;
                }
            }
        }
        
        // Center pot always controls the SELECTED fader's column
        if (data.controls & kNT_potC) {
            float potValue = data.pots[1];
            // Apply min/max deadzones to pot values (3% to match parameter logic)
            if (potValue < 0.03f) potValue = 0.0f;
            else if (potValue > 0.97f) potValue = 1.0f;
            // Quantize pot to 2500 steps to reduce jitter/backtracking
            potValue = floorf(potValue * 2500.0f + 0.5f) / 2500.0f;
            // Apply deadband - only update if change is significant
            if (a->potLast[1] < 0.0f || fabsf(potValue - a->potLast[1]) > a->potDeadband) {
                int faderParam = colInPage;  // FADER 1-8 maps to 0-7
                int16_t value = (int16_t)(potValue * 16383.0f + 0.5f);  // Scale to 0-16383
                NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
                a->potLast[1] = potValue;
            }
        }
        
        // Right pot controls fader to the RIGHT of selected (does nothing if last fader selected)
        if (data.controls & kNT_potR) {
            if (colInPage < 7) {  // Only active if not last fader
                float potValue = data.pots[2];
                // Apply min/max deadzones to pot values (3% to match parameter logic)
                if (potValue < 0.03f) potValue = 0.0f;
                else if (potValue > 0.97f) potValue = 1.0f;
                // Quantize pot to 2500 steps to reduce jitter/backtracking
                potValue = floorf(potValue * 2500.0f + 0.5f) / 2500.0f;
                // Apply deadband - only update if change is significant
                if (a->potLast[2] < 0.0f || fabsf(potValue - a->potLast[2]) > a->potDeadband) {
                    int targetCol = colInPage + 1;
                    int faderParam = targetCol;  // FADER 1-8 maps to 0-7
                    int16_t value = (int16_t)(potValue * 16383.0f + 0.5f);  // Scale to 0-16383
                    NT_setParameterFromUi(algIndex, kParamFader1 + faderParam + paramOffset, value);
                    a->potLast[2] = potValue;
                }
            }
        }
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
        
        // Scale parameter (0-16383) to internal value (0.0-1.0)
        float v = self->v[p] / 16383.0f;
        if (v < 0.0f) v = 0.0f;
        else if (v > 1.0f) v = 1.0f;
        
        // Apply min/max deadzones BEFORE pickup mode logic to prevent false catches at endpoints
        // Larger deadzones (3%) at min/max to prevent getting stuck
        if (v < 0.03f) {
            v = 0.0f;
        } else if (v > 0.97f) {
            v = 1.0f;
        }
        
        // Get pickup mode setting (0=Scaled, 1=Catch)
        int pickupMode = (int)(self->v[kParamPickupMode] + 0.5f);
        
        // Pickup mode logic: relative scaling when physical position ≠ value
        float currentValue = a->internalFaders[internalIdx];
        
        // Also apply deadzones to current value for consistent comparison
        float currentValueWithDeadzone = currentValue;
        if (currentValueWithDeadzone < 0.03f) {
            currentValueWithDeadzone = 0.0f;
        } else if (currentValueWithDeadzone > 0.97f) {
            currentValueWithDeadzone = 1.0f;
        }
        
        // Use coarse resolution (500 steps) for pickup mode detection to reduce jitter sensitivity
        // Still maintain full 16k resolution for actual output
        int coarsePhysical = (int)(v * 500.0f + 0.5f);
        int coarseCurrent = (int)(currentValueWithDeadzone * 500.0f + 0.5f);
        int coarseMismatch = abs(coarsePhysical - coarseCurrent);
        
        // Track movement direction to detect continuous fast sweeps
        float movementDelta = v - a->lastPhysicalPos[internalIdx];
        // Use threshold that accounts for coarse resolution (500 steps = 0.002 per step)
        bool movingContinuously = fabsf(movementDelta) > 0.001f;  // ~0.5 coarse steps
        bool sameDirection = (movementDelta > 0 && (v - a->physicalFaderPos[internalIdx]) > 0) ||
                            (movementDelta < 0 && (v - a->physicalFaderPos[internalIdx]) < 0);
        
        // Update tracking
        a->lastPhysicalPos[internalIdx] = a->physicalFaderPos[internalIdx];
        a->physicalFaderPos[internalIdx] = v;
        
        // Debug tracking for fader 0
        if (internalIdx == 0) {
        }
        
        // Decide whether to use pickup mode or absolute mode
        if (!a->inPickupMode[internalIdx]) {
            // Decrement settle counter if active AND not actively moving
            if (a->pickupSettleFrames[internalIdx] > 0) {
                if (!movingContinuously) {
                    a->pickupSettleFrames[internalIdx]--;
                } else {
                    // Active movement clears settle period immediately
                    a->pickupSettleFrames[internalIdx] = 0;
                }
            }
            
            // Currently in absolute mode - check if we should enter pickup
            // Enter pickup mode if:
            //   1. Mismatch > 1% (coarse) AND
            //   2. Either:
            //      a) Big mismatch (>10%) - always enter pickup, OR
            //      b) Small mismatch (1-10%) but NOT continuous same-direction movement (allows fast sweeps)
            //   3. Settle period has expired
            bool bigMismatch = coarseMismatch > 50;  // >10% - always use pickup
            bool smallMismatch = coarseMismatch > 5;  // >1% - use pickup unless fast sweep
            bool shouldEnterPickup = a->pickupSettleFrames[internalIdx] == 0 && 
                                    (bigMismatch || (smallMismatch && !(movingContinuously && sameDirection)));
            
            if (shouldEnterPickup) {
                // Enter pickup mode
                a->inPickupMode[internalIdx] = true;
                a->pickupPivot[internalIdx] = v;
                a->pickupStartValue[internalIdx] = currentValue;
                
                // Debug tracking for fader 0
                if (internalIdx == 0) {
                }
                // Don't change value yet - will calculate on next call
            } else {
                // Small mismatch with fast sweep OR in settle period - update directly
                a->internalFaders[internalIdx] = v;
            }
        } else {
            // Already in pickup mode
            if (pickupMode == 1) {
                // CATCH MODE: Only exit on continuous movement if already close (within 10%)
                // This prevents jumps when starting from a big mismatch
                bool closeEnoughToExit = coarseMismatch < 50;  // Within 10%
                
                if (movingContinuously && sameDirection && closeEnoughToExit) {
                    // Continuous movement AND already close - exit pickup and follow directly
                    a->internalFaders[internalIdx] = v;
                    a->inPickupMode[internalIdx] = false;
                    a->inSlewMode[internalIdx] = false;
                    a->pickupPivot[internalIdx] = -1.0f;
                    a->pickupSettleFrames[internalIdx] = 0;  // No settle when actively moving
                } else if (coarseMismatch < 25) {
                    // Within 5% (coarse) - enter slew mode for smooth catch
                    if (!a->inSlewMode[internalIdx]) {
                        a->inSlewMode[internalIdx] = true;
                        a->slewTarget[internalIdx] = v;
                    }
                    
                    // Slew toward target over ~0.5 seconds
                    float slewRate = 0.005333f;
                    float delta = v - currentValue;
                    
                    // Exit when within 2 coarse steps (0.2%)
                    if (coarseMismatch < 2) {
                        // Close enough - snap and exit pickup mode
                        a->internalFaders[internalIdx] = v;
                        a->inPickupMode[internalIdx] = false;
                        a->inSlewMode[internalIdx] = false;
                        a->pickupPivot[internalIdx] = -1.0f;
                        a->pickupSettleFrames[internalIdx] = 5;
                    } else {
                        // Slew toward target
                        if (delta > 0) {
                            a->internalFaders[internalIdx] += slewRate;
                        } else {
                            a->internalFaders[internalIdx] -= slewRate;
                        }
                    }
                } else {
                    // Still > 5% (coarse) away - use scaled pickup behavior like Scaled mode
                    // Calculate scaled value based on pivot
                    float pivotPos = a->pickupPivot[internalIdx];
                    float startValue = a->pickupStartValue[internalIdx];
                    float physicalDelta = v - pivotPos;
                    
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
                        targetValue = startValue;
                    }
                    
                    if (targetValue < 0.0f) targetValue = 0.0f;
                    if (targetValue > 1.0f) targetValue = 1.0f;
                    
                    a->internalFaders[internalIdx] = targetValue;
                }
            } else {
                // SCALED MODE: Calculate scaled value based on pivot
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
                }
                
                if (caughtUp) {
                    // Physical position has caught up with output - exit pickup mode
                    a->inPickupMode[internalIdx] = false;
                    a->pickupPivot[internalIdx] = -1.0f;
                    a->internalFaders[internalIdx] = v;  // Now use absolute
                    
                    // Debug tracking for fader 0
                    if (internalIdx == 0) {
                    }
                } else {
                    // Still in pickup - use scaled value
                    a->internalFaders[internalIdx] = targetValue;
                }
            }
        }
        
        // Update physical position tracking
        a->physicalFaderPos[internalIdx] = v;
        
        // Update reference value for gang fader system
        // If this fader is a child of a gang fader, update its reference
        bool isChild = false;
        for (int g = 0; g < internalIdx; g++) {
            if (a->faderNoteSettings[g].controlAllCount > 0) {
                int childStart = g + 1;
                int childEnd = g + a->faderNoteSettings[g].controlAllCount;
                if (internalIdx >= childStart && internalIdx <= childEnd) {
                    isChild = true;
                    break;
                }
            }
        }
        
        // If this is a child fader and it was manually adjusted, update its reference
        if (isChild && !a->inPickupMode[internalIdx]) {
            a->faderReferenceValues[internalIdx] = a->internalFaders[internalIdx];
        }
        
        // Track debug info for FADER 1 (internalIdx 0 on page 0)
        if (internalIdx == 0) {
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
    
    // Clear the modified flag when saving
    a->namesModified = false;
    
    // Write display layout info for debugging/screenshots
    stream.addMemberName("displayLayout");
    stream.openObject();
        stream.addMemberName("buildVersion");
        stream.addNumber(VFADER_BUILD);
        
        stream.addMemberName("currentPage");
        stream.addNumber(a->page);
        
        stream.addMemberName("selectedFader");
        stream.addNumber(a->sel);
        
        stream.addMemberName("nameEditMode");
        stream.addBoolean(a->nameEditMode);
        
        stream.addMemberName("nameEditFader");
        stream.addNumber(a->nameEditFader);
        
        stream.addMemberName("namesModified");
        stream.addBoolean(a->namesModified);
        
        // Store visible fader values for this page
        stream.addMemberName("visibleFaders");
        stream.openArray();
        int baseIndex = (a->page - 1) * 8;
        for (int i = 0; i < 8; i++) {
            stream.openObject();
                stream.addMemberName("index");
                stream.addNumber(baseIndex + i);
                stream.addMemberName("value");
                stream.addNumber(a->internalFaders[baseIndex + i]);
                stream.addMemberName("name");
                stream.addString(a->faderNames[baseIndex + i]);
                stream.addMemberName("inPickup");
                stream.addBoolean(a->inPickupMode[baseIndex + i]);
            stream.closeObject();
        }
        stream.closeArray();
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
    
    // Write fader names
    stream.addMemberName("faderNames");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.addString(a->faderNames[i]);
    }
    stream.closeArray();
    
    // Write fader note settings
    stream.addMemberName("noteSettings");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.openObject();
        stream.addMemberName("displayMode");
        stream.addNumber(a->faderNoteSettings[i].displayMode);
        stream.addMemberName("sharpFlat");
        stream.addNumber(a->faderNoteSettings[i].sharpFlat);
        stream.addMemberName("bottomMidi");
        stream.addNumber(a->faderNoteSettings[i].bottomMidi);
        stream.addMemberName("topMidi");
        stream.addNumber(a->faderNoteSettings[i].topMidi);
        stream.addMemberName("bottomValue");
        stream.addNumber(a->faderNoteSettings[i].bottomValue);
        stream.addMemberName("topValue");
        stream.addNumber(a->faderNoteSettings[i].topValue);
        stream.addMemberName("chromaticScale");
        stream.openArray();
        for (int j = 0; j < 12; j++) {
            stream.addNumber(a->faderNoteSettings[i].chromaticScale[j]);
        }
        stream.closeArray();
        stream.addMemberName("controlAllCount");
        stream.addNumber(a->faderNoteSettings[i].controlAllCount);
        stream.addMemberName("controlAllMode");
        stream.addNumber(a->faderNoteSettings[i].controlAllMode);
        stream.closeObject();
    }
    stream.closeArray();
    
    // Save fader reference values
    stream.addMemberName("faderReferenceValues");
    stream.openArray();
    for (int i = 0; i < 32; i++) {
        stream.addNumber(a->faderReferenceValues[i]);
    }
    stream.closeArray();
}

// Deserialization - restore fader names and values
static bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VFader* a = static_cast<VFader*>(self);
    
    // Clear the modified flag when loading a preset
    a->namesModified = false;
    
    int numMembers;
    if (!parse.numberOfObjectMembers(numMembers))
        return false;
    
    for (int i = 0; i < numMembers; i++) {
        // Check for fader values
        if (parse.matchName("faders")) {
            int arraySize;
            if (!parse.numberOfArrayElements(arraySize))
                return false;
            for (int j = 0; j < arraySize && j < 32; j++) {
                float value;
                if (!parse.number(value))
                    return false;
                a->internalFaders[j] = value;
            }
        }
        // Check for fader names
        else if (parse.matchName("faderNames")) {
            int arraySize;
            if (!parse.numberOfArrayElements(arraySize))
                return false;
            for (int j = 0; j < arraySize && j < 32; j++) {
                const char* str = NULL;
                if (!parse.string(str))
                    return false;
                if (str != NULL) {
                    strncpy(a->faderNames[j], str, 12);
                    a->faderNames[j][12] = 0;  // Ensure null termination (12 chars max)
                }
            }
        }
        // Load note settings
        else if (parse.matchName("noteSettings")) {
            int arraySize;
            if (!parse.numberOfArrayElements(arraySize))
                return false;
            for (int j = 0; j < arraySize && j < 32; j++) {
                int numFields;
                if (!parse.numberOfObjectMembers(numFields))
                    return false;
                for (int k = 0; k < numFields; k++) {
                    if (parse.matchName("displayMode")) {
                        float val;
                        if (!parse.number(val)) return false;
                        a->faderNoteSettings[j].displayMode = (uint8_t)val;
                    }
                    else if (parse.matchName("sharpFlat")) {
                        float val;
                        if (!parse.number(val)) return false;
                        a->faderNoteSettings[j].sharpFlat = (uint8_t)val;
                    }
                    else if (parse.matchName("bottomMidi")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int midi = (int)val;
                        if (midi < 0) midi = 0;
                        if (midi > 127) midi = 127;
                        a->faderNoteSettings[j].bottomMidi = (uint8_t)midi;
                    }
                    else if (parse.matchName("topMidi")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int midi = (int)val;
                        if (midi < 0) midi = 0;
                        if (midi > 127) midi = 127;
                        a->faderNoteSettings[j].topMidi = (uint8_t)midi;
                    }
                    else if (parse.matchName("bottomValue")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int value = (int)val;
                        if (value < 0) value = 0;
                        if (value > 100) value = 100;
                        a->faderNoteSettings[j].bottomValue = (uint8_t)value;
                    }
                    else if (parse.matchName("topValue")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int value = (int)val;
                        if (value < 0) value = 0;
                        if (value > 100) value = 100;
                        a->faderNoteSettings[j].topValue = (uint8_t)value;
                    }
                    // Backward compatibility with old format
                    else if (parse.matchName("bottomNote") || parse.matchName("bottomOctave") || 
                             parse.matchName("topNote") || parse.matchName("topOctave")) {
                        float val;
                        if (!parse.number(val)) return false;
                        // Ignore old format - will use defaults
                    }
                    else if (parse.matchName("chromaticScale")) {
                        int scaleSize;
                        if (!parse.numberOfArrayElements(scaleSize)) return false;
                        for (int m = 0; m < scaleSize && m < 12; m++) {
                            float val;
                            if (!parse.number(val)) return false;
                            a->faderNoteSettings[j].chromaticScale[m] = (uint8_t)val;
                        }
                    }
                    else if (parse.matchName("controlAllCount")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int count = (int)val;
                        if (count < 0) count = 0;
                        if (count > 31) count = 31;
                        a->faderNoteSettings[j].controlAllCount = (uint8_t)count;
                    }
                    else if (parse.matchName("controlAllMode")) {
                        float val;
                        if (!parse.number(val)) return false;
                        int mode = (int)val;
                        if (mode < 0) mode = 0;
                        if (mode > 1) mode = 1;
                        a->faderNoteSettings[j].controlAllMode = (uint8_t)mode;
                    }
                    else {
                        if (!parse.skipMember()) return false;
                    }
                }
            }
        }
        else if (parse.matchName("faderReferenceValues")) {
            int refCount;
            if (!parse.numberOfArrayElements(refCount)) return false;
            for (int i = 0; i < refCount && i < 32; i++) {
                float val;
                if (!parse.number(val)) return false;
                a->faderReferenceValues[i] = val;
            }
        }
        // Skip other members (debug data, etc.)
        else {
            if (!parse.skipMember())
                return false;
        }
    }
    
    return true;
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V','F','D','R'),
    .name = "VFader",
    .description = "32 virtual faders, 7/14-bit MIDI CC, CV outputs, F8R control",
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
