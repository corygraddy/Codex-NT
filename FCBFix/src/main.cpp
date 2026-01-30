#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cmath>
#include <cstring>

// FCBFix: MIDI Program Change to CV Gate + MIDI Note converter
// - 10 programmable slots
// - Each slot: program change number (0-127) + CV output + MIDI note
// - When matching program change is received, fires CV gate on assigned output
// - Also sends MIDI Note On/Off messages
// - Gate duration: 100ms fixed

struct FCBFix : public _NT_algorithm {
    // Gate state for 10 slots
    int gateCounter[10];        // Countdown timer for gate pulse (in samples)
    float gateOutputs[10];      // Current gate voltage for each slot
    bool noteActive[10];        // Track if MIDI note is currently on
    
    // Slot names and states
    char slotNames[10][9];      // 10 slots, 8 chars + null terminator
    bool slotStates[10];        // On/off state for each slot (for display brightness)
    
    // UI state
    int selectedSlot;           // 0-9
    uint16_t lastEncoderLButton; // For debouncing left encoder button
    
    // Name edit mode
    bool nameEditMode;          // Whether we're editing a slot name
    uint8_t nameEditPos;        // Current character position (0-7)
    uint8_t nameEditSlot;       // Which slot is being edited (0-9)
    uint16_t lastButtonState;   // For button debouncing
    
    FCBFix() {
        for (int i = 0; i < 10; i++) {
            gateCounter[i] = 0;
            gateOutputs[i] = 0.0f;
            noteActive[i] = false;
            slotStates[i] = false;  // All start dim
            // Initialize default names
            snprintf(slotNames[i], 9, "Slot %d", i + 1);
        }
        selectedSlot = 0;
        lastEncoderLButton = 0;
        nameEditMode = false;
        nameEditPos = 0;
        nameEditSlot = 0;
        lastButtonState = 0;
    }
};

// =============================================================================
// Parameters
// =============================================================================

enum {
    // Global MIDI settings
    kParamMidiChannel,
    kParamMidiDestination,
    
    // 10 slots × 3 parameters each (program number, output, MIDI note)
    kParamSlot1Program,
    kParamSlot1Output,
    kParamSlot1MidiNote,
    kParamSlot2Program,
    kParamSlot2Output,
    kParamSlot2MidiNote,
    kParamSlot3Program,
    kParamSlot3Output,
    kParamSlot3MidiNote,
    kParamSlot4Program,
    kParamSlot4Output,
    kParamSlot4MidiNote,
    kParamSlot5Program,
    kParamSlot5Output,
    kParamSlot5MidiNote,
    kParamSlot6Program,
    kParamSlot6Output,
    kParamSlot6MidiNote,
    kParamSlot7Program,
    kParamSlot7Output,
    kParamSlot7MidiNote,
    kParamSlot8Program,
    kParamSlot8Output,
    kParamSlot8MidiNote,
    kParamSlot9Program,
    kParamSlot9Output,
    kParamSlot9MidiNote,
    kParamSlot10Program,
    kParamSlot10Output,
    kParamSlot10MidiNote,
    kNumParameters
};

static _NT_parameter parameters[kNumParameters];

// Parameter name strings
static char midiChannelName[] = "MIDI Channel";
static char midiDestinationName[] = "MIDI Destination";
static char slot1ProgramName[] = "Slot 1 Program";
static char slot1OutputName[] = "Slot 1 Output";
static char slot1MidiNoteName[] = "Slot 1 MIDI Note";
static char slot2ProgramName[] = "Slot 2 Program";
static char slot2OutputName[] = "Slot 2 Output";
static char slot2MidiNoteName[] = "Slot 2 MIDI Note";
static char slot3ProgramName[] = "Slot 3 Program";
static char slot3OutputName[] = "Slot 3 Output";
static char slot3MidiNoteName[] = "Slot 3 MIDI Note";
static char slot4ProgramName[] = "Slot 4 Program";
static char slot4OutputName[] = "Slot 4 Output";
static char slot4MidiNoteName[] = "Slot 4 MIDI Note";
static char slot5ProgramName[] = "Slot 5 Program";
static char slot5OutputName[] = "Slot 5 Output";
static char slot5MidiNoteName[] = "Slot 5 MIDI Note";
static char slot6ProgramName[] = "Slot 6 Program";
static char slot6OutputName[] = "Slot 6 Output";
static char slot6MidiNoteName[] = "Slot 6 MIDI Note";
static char slot7ProgramName[] = "Slot 7 Program";
static char slot7OutputName[] = "Slot 7 Output";
static char slot7MidiNoteName[] = "Slot 7 MIDI Note";
static char slot8ProgramName[] = "Slot 8 Program";
static char slot8OutputName[] = "Slot 8 Output";
static char slot8MidiNoteName[] = "Slot 8 MIDI Note";
static char slot9ProgramName[] = "Slot 9 Program";
static char slot9OutputName[] = "Slot 9 Output";
static char slot9MidiNoteName[] = "Slot 9 MIDI Note";
static char slot10ProgramName[] = "Slot 10 Program";
static char slot10OutputName[] = "Slot 10 Output";
static char slot10MidiNoteName[] = "Slot 10 MIDI Note";

// MIDI destination strings
static const char* const midiDestinationStrings[] = {
    "Off", "Breakout", "SelectBus", "USB", "Internal", NULL
};

void initParameters(_NT_algorithm* self) {
    // MIDI Channel (1-16)
    parameters[kParamMidiChannel].name = midiChannelName;
    parameters[kParamMidiChannel].min = 1;
    parameters[kParamMidiChannel].max = 16;
    parameters[kParamMidiChannel].def = 1;
    parameters[kParamMidiChannel].unit = kNT_unitNone;
    parameters[kParamMidiChannel].scaling = kNT_scalingNone;
    
    // MIDI Destination
    parameters[kParamMidiDestination].name = midiDestinationName;
    parameters[kParamMidiDestination].min = 0;
    parameters[kParamMidiDestination].max = 4;
    parameters[kParamMidiDestination].def = 3;  // USB by default
    parameters[kParamMidiDestination].unit = kNT_unitEnum;
    parameters[kParamMidiDestination].scaling = kNT_scalingNone;
    parameters[kParamMidiDestination].enumStrings = midiDestinationStrings;
    
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
    
    parameters[kParamSlot1MidiNote].name = slot1MidiNoteName;
    parameters[kParamSlot1MidiNote].min = 0;
    parameters[kParamSlot1MidiNote].max = 127;
    parameters[kParamSlot1MidiNote].def = 60;  // Middle C
    parameters[kParamSlot1MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot1MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot2MidiNote].name = slot2MidiNoteName;
    parameters[kParamSlot2MidiNote].min = 0;
    parameters[kParamSlot2MidiNote].max = 127;
    parameters[kParamSlot2MidiNote].def = 62;
    parameters[kParamSlot2MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot2MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot3MidiNote].name = slot3MidiNoteName;
    parameters[kParamSlot3MidiNote].min = 0;
    parameters[kParamSlot3MidiNote].max = 127;
    parameters[kParamSlot3MidiNote].def = 64;
    parameters[kParamSlot3MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot3MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot4MidiNote].name = slot4MidiNoteName;
    parameters[kParamSlot4MidiNote].min = 0;
    parameters[kParamSlot4MidiNote].max = 127;
    parameters[kParamSlot4MidiNote].def = 65;
    parameters[kParamSlot4MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot4MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot5MidiNote].name = slot5MidiNoteName;
    parameters[kParamSlot5MidiNote].min = 0;
    parameters[kParamSlot5MidiNote].max = 127;
    parameters[kParamSlot5MidiNote].def = 67;
    parameters[kParamSlot5MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot5MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot6MidiNote].name = slot6MidiNoteName;
    parameters[kParamSlot6MidiNote].min = 0;
    parameters[kParamSlot6MidiNote].max = 127;
    parameters[kParamSlot6MidiNote].def = 69;
    parameters[kParamSlot6MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot6MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot7MidiNote].name = slot7MidiNoteName;
    parameters[kParamSlot7MidiNote].min = 0;
    parameters[kParamSlot7MidiNote].max = 127;
    parameters[kParamSlot7MidiNote].def = 71;
    parameters[kParamSlot7MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot7MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot8MidiNote].name = slot8MidiNoteName;
    parameters[kParamSlot8MidiNote].min = 0;
    parameters[kParamSlot8MidiNote].max = 127;
    parameters[kParamSlot8MidiNote].def = 72;
    parameters[kParamSlot8MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot8MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot9MidiNote].name = slot9MidiNoteName;
    parameters[kParamSlot9MidiNote].min = 0;
    parameters[kParamSlot9MidiNote].max = 127;
    parameters[kParamSlot9MidiNote].def = 74;
    parameters[kParamSlot9MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot9MidiNote].scaling = kNT_scalingNone;
    
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
    
    parameters[kParamSlot10MidiNote].name = slot10MidiNoteName;
    parameters[kParamSlot10MidiNote].min = 0;
    parameters[kParamSlot10MidiNote].max = 127;
    parameters[kParamSlot10MidiNote].def = 76;
    parameters[kParamSlot10MidiNote].unit = kNT_unitMIDINote;
    parameters[kParamSlot10MidiNote].scaling = kNT_scalingNone;
    
    self->parameters = parameters;
}

// =============================================================================
// Parameter Pages
// =============================================================================

static const uint8_t midiParams[] = { kParamMidiChannel, kParamMidiDestination };
static const uint8_t slot1Params[] = { kParamSlot1Program, kParamSlot1Output, kParamSlot1MidiNote };
static const uint8_t slot2Params[] = { kParamSlot2Program, kParamSlot2Output, kParamSlot2MidiNote };
static const uint8_t slot3Params[] = { kParamSlot3Program, kParamSlot3Output, kParamSlot3MidiNote };
static const uint8_t slot4Params[] = { kParamSlot4Program, kParamSlot4Output, kParamSlot4MidiNote };
static const uint8_t slot5Params[] = { kParamSlot5Program, kParamSlot5Output, kParamSlot5MidiNote };
static const uint8_t slot6Params[] = { kParamSlot6Program, kParamSlot6Output, kParamSlot6MidiNote };
static const uint8_t slot7Params[] = { kParamSlot7Program, kParamSlot7Output, kParamSlot7MidiNote };
static const uint8_t slot8Params[] = { kParamSlot8Program, kParamSlot8Output, kParamSlot8MidiNote };
static const uint8_t slot9Params[] = { kParamSlot9Program, kParamSlot9Output, kParamSlot9MidiNote };
static const uint8_t slot10Params[] = { kParamSlot10Program, kParamSlot10Output, kParamSlot10MidiNote };

static const _NT_parameterPage parameterPages[] = {
    { .name = "MIDI", .numParams = 2, .params = midiParams },
    { .name = "Slot 1", .numParams = 3, .params = slot1Params },
    { .name = "Slot 2", .numParams = 3, .params = slot2Params },
    { .name = "Slot 3", .numParams = 3, .params = slot3Params },
    { .name = "Slot 4", .numParams = 3, .params = slot4Params },
    { .name = "Slot 5", .numParams = 3, .params = slot5Params },
    { .name = "Slot 6", .numParams = 3, .params = slot6Params },
    { .name = "Slot 7", .numParams = 3, .params = slot7Params },
    { .name = "Slot 8", .numParams = 3, .params = slot8Params },
    { .name = "Slot 9", .numParams = 3, .params = slot9Params },
    { .name = "Slot 10", .numParams = 3, .params = slot10Params },
};

static const _NT_parameterPages pages = {
    .numPages = 11,
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

// =============================================================================
// Helper Functions
// =============================================================================

static void triggerGate(_NT_algorithm* self, int slot) {
    FCBFix* a = (FCBFix*)self;
    
    // Toggle slot state (for display brightness)
    a->slotStates[slot] = !a->slotStates[slot];
    
    // Set gate duration: 100ms at 48kHz = 4800 samples
    a->gateCounter[slot] = 4800;
    a->gateOutputs[slot] = 10.0f;  // 10V gate
    
    // Send MIDI Note On
    int midiChannel = a->v[kParamMidiChannel];
    int midiDest = a->v[kParamMidiDestination];
    uint32_t destination = 0;
    if (midiDest == 1) destination = kNT_destinationBreakout;
    else if (midiDest == 2) destination = kNT_destinationSelectBus;
    else if (midiDest == 3) destination = kNT_destinationUSB;
    else if (midiDest == 4) destination = kNT_destinationInternal;
    
    if (destination != 0) {
        int noteNum = a->v[kParamSlot1MidiNote + (slot * 3)];
        uint8_t statusByte = 0x90 | ((midiChannel - 1) & 0x0F);  // Note On + channel
        NT_sendMidi3ByteMessage(destination, statusByte, noteNum, 100);  // Velocity 100
        a->noteActive[slot] = true;
    }
}

static void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    FCBFix* a = (FCBFix*)self;
    const int numFrames = numFramesBy4 * 4;
    
    // Get MIDI settings
    int midiChannel = a->v[kParamMidiChannel];
    int midiDest = a->v[kParamMidiDestination];
    uint32_t destination = 0;
    if (midiDest == 1) destination = kNT_destinationBreakout;
    else if (midiDest == 2) destination = kNT_destinationSelectBus;
    else if (midiDest == 3) destination = kNT_destinationUSB;
    else if (midiDest == 4) destination = kNT_destinationInternal;
    
    // Process gate counters and output gates
    for (int slot = 0; slot < 10; slot++) {
        if (a->gateCounter[slot] > 0) {
            a->gateCounter[slot] -= numFrames;
            if (a->gateCounter[slot] <= 0) {
                a->gateCounter[slot] = 0;
                a->gateOutputs[slot] = 0.0f;  // Gate off
                
                // Send MIDI Note Off if note is active
                if (a->noteActive[slot] && destination != 0) {
                    int noteNum = a->v[kParamSlot1MidiNote + (slot * 3)];
                    uint8_t statusByte = 0x80 | ((midiChannel - 1) & 0x0F);  // Note Off + channel
                    NT_sendMidi3ByteMessage(destination, statusByte, noteNum, 0);
                    a->noteActive[slot] = false;
                }
            }
        }
        
        // Output gate voltage to assigned bus
        int outputBus = a->v[kParamSlot1Output + (slot * 3)];
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
            int assignedProgram = a->v[kParamSlot1Program + (slot * 3)];
            if (program == assignedProgram) {
                triggerGate(self, slot);
            }
        }
    }
}

static bool draw(_NT_algorithm* self) {
    FCBFix* a = (FCBFix*)self;
    
    // Popup name editor
    if (a->nameEditMode) {
        // Row 1: "EDIT SLOT"
        NT_drawText(2, 8, "EDIT SLOT", 15, kNT_textLeft, kNT_textNormal);
        
        // Row 2: Slot name - draw each character with different brightness
        for (int i = 0; i < 8; i++) {
            char singleChar[2];
            singleChar[0] = a->slotNames[a->nameEditSlot][i];
            singleChar[1] = '\0';
            int brightness = (i == a->nameEditPos) ? 15 : 7;
            NT_drawText(2 + (i * 8), 24, singleChar, brightness, kNT_textLeft, kNT_textNormal);
        }
        
        // Row 3: "STATE" label
        NT_drawText(2, 40, "STATE", 15, kNT_textLeft, kNT_textNormal);
        
        // Row 4: "on" or "off" - brighter when editing
        const char* stateText = a->slotStates[a->nameEditSlot] ? "on" : "off";
        int stateBrightness = (a->nameEditPos == 8) ? 15 : 7;
        NT_drawText(2, 52, stateText, stateBrightness, kNT_textLeft, kNT_textNormal);
        
        return true;
    }
    
    // Clean 2x5 grid - just slot names
    // Top row: Slots 6-10
    int topRowY = 20;
    int bottomRowY = 40;
    
    for (int i = 0; i < 5; i++) {
        // Top row (slots 6-10)
        int topSlot = i + 5;
        int x = i * 51 + 2;
        int brightness = a->slotStates[topSlot] ? 15 : 4;
        NT_drawText(x, topRowY, a->slotNames[topSlot], brightness, kNT_textLeft, kNT_textNormal);
        
        // Short underline for selected slot in top row
        if (a->selectedSlot == topSlot) {
            NT_drawShapeI(kNT_line, x, topRowY + 8, x + 15, topRowY + 8, 15);
        }
        
        // Bottom row (slots 1-5)
        int bottomSlot = i;
        brightness = a->slotStates[bottomSlot] ? 15 : 4;
        NT_drawText(x, bottomRowY, a->slotNames[bottomSlot], brightness, kNT_textLeft, kNT_textNormal);
        
        // Short underline for selected slot in bottom row
        if (a->selectedSlot == bottomSlot) {
            NT_drawShapeI(kNT_line, x, bottomRowY + 8, x + 15, bottomRowY + 8, 15);
        }
    }
    
    return true;
}

static uint32_t hasCustomUi(_NT_algorithm* self) {
    return kNT_encoderL | kNT_encoderR;  // Use both encoders
}

static void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    FCBFix* a = (FCBFix*)self;
    
    // Detect button presses (rising edge only)
    bool leftButtonPressed = (data.controls & kNT_encoderButtonL) && !(a->lastButtonState & kNT_encoderButtonL);
    bool rightButtonPressed = (data.controls & kNT_encoderButtonR) && !(a->lastButtonState & kNT_encoderButtonR);
    a->lastButtonState = data.controls;
    
    // NAME EDIT MODE
    if (a->nameEditMode) {
        // Right encoder: change characters or toggle state
        int encoderDelta = data.encoders[1];
        if (encoderDelta > 1) encoderDelta = 1;
        if (encoderDelta < -1) encoderDelta = -1;
        
        if (encoderDelta != 0) {
            if (a->nameEditPos < 8) {
                // Editing character
                char* name = a->slotNames[a->nameEditSlot];
                char c = name[a->nameEditPos];
                
                // Character set: space, 0-9, A-Z
                const char charset[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                const int charsetLen = 37;
                
                // Find current position in charset
                int currentIdx = 0;
                if (c == 0) c = ' ';
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
            } else {
                // Editing state (toggle on encoder turn)
                a->slotStates[a->nameEditSlot] = !a->slotStates[a->nameEditSlot];
            }
        }
        
        // Left encoder: move cursor position
        if (data.encoders[0] != 0) {
            a->nameEditPos += data.encoders[0];
            // Wrap around: 0-8 (0-7 for chars, 8 for state)
            if (a->nameEditPos > 8) a->nameEditPos = 0;
            if (a->nameEditPos > 8) a->nameEditPos = 8;  // Handle negative wrap
        }
        
        // Right button: exit edit mode
        if (rightButtonPressed) {
            a->nameEditMode = false;
        }
        
    } else {
        // NORMAL MODE
        
        // Left encoder: select slot (0-9)
        if (data.encoders[0] != 0) {
            a->selectedSlot += data.encoders[0];
            if (a->selectedSlot < 0) a->selectedSlot = 9;
            if (a->selectedSlot > 9) a->selectedSlot = 0;
        }
        
        // Left encoder button: cycle through slots
        if (leftButtonPressed) {
            a->selectedSlot++;
            if (a->selectedSlot > 9) a->selectedSlot = 0;
        }
        
        // Right encoder button: enter edit mode for selected slot
        if (rightButtonPressed) {
            a->nameEditMode = true;
            a->nameEditSlot = a->selectedSlot;
            a->nameEditPos = 0;
        }
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
    .serialise = [](struct _NT_algorithm* self, _NT_jsonStream& stream) {
        FCBFix* a = (FCBFix*)self;
        
        // Serialize slot names
        stream.addMemberName("slotNames");
        stream.openArray();
        for (int i = 0; i < 10; i++) {
            stream.addString(a->slotNames[i]);
        }
        stream.closeArray();
        
        // Serialize slot states
        stream.addMemberName("slotStates");
        stream.openArray();
        for (int i = 0; i < 10; i++) {
            stream.addBoolean(a->slotStates[i]);
        }
        stream.closeArray();
    },
    .deserialise = [](struct _NT_algorithm* self, _NT_jsonParse& parse) -> bool {
        FCBFix* a = (FCBFix*)self;
        
        int numMembers = 0;
        if (!parse.numberOfObjectMembers(numMembers))
            return false;
        
        for (int i = 0; i < numMembers; i++) {
            if (parse.matchName("slotNames")) {
                int numNames = 0;
                if (parse.numberOfArrayElements(numNames)) {
                    int count = (numNames < 10) ? numNames : 10;
                    for (int j = 0; j < count; j++) {
                        const char* str = nullptr;
                        if (parse.string(str) && str) {
                            strncpy(a->slotNames[j], str, 8);
                            a->slotNames[j][8] = 0;  // Ensure null termination
                        }
                    }
                }
            } else if (parse.matchName("slotStates")) {
                int numStates = 0;
                if (parse.numberOfArrayElements(numStates)) {
                    int count = (numStates < 10) ? numStates : 10;
                    for (int j = 0; j < count; j++) {
                        bool state;
                        if (parse.boolean(state)) {
                            a->slotStates[j] = state;
                        }
                    }
                }
            } else {
                parse.skipMember();
            }
        }
        return true;
    },
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
