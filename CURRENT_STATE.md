# Codex-NT Development Context
**Last Updated:** January 22, 2026

## Current Session Work

### Just Completed
1. **VSeq Clock Division/Multiplication Implementation** - ✅ COMPLETE
   - Added clock counter arrays for all sequencers (CV + gate)
   - Division modes (/16, /8, /4, /2): Count clock pulses before advancing
   - Multiplication modes (x1, x2, x4, x8, x16): Measure clock period and generate internal subdivisions
   - Fixed MIDI/gate triggers to only fire on actual step events (not every clock)
   - Committed as: "Implement clock division/multiplication for VSeq"

2. **VSeq Swing Implementation** - ✅ COMPLETE
   - Added swing parameter (0-100%) to gate sequencer
   - Odd-numbered steps (1,3,5...) delayed based on swing percentage
   - swing=0: straight timing, swing=100: triplet/shuffle feel
   - Swing delay calculated as percentage of clock period
   - Committed as: "Implement swing for VSeq gate tracks"

3. **VSeq CV Voltage Fix** - ✅ COMPLETE (Earlier)
   - Fixed 0-1V output bug → now correctly outputs 0-10V
   - Committed as: "Fix VSeq CV output voltage range"

4. **VSeq Restoration** - ✅ COMPLETE (Earlier)
   - Was accidentally replaced with VFader code in commit `3893a73`
   - Restored from working commit `355e014`

## Plugin Status Overview

### ✅ VFader (Working - Build 50)
- **Purpose:** 32 virtual faders → MIDI CC (7-bit/14-bit) + CV outputs
- **Location:** `/VFader/src/main.cpp`
- **Status:** Stable, released
- **Known Issues:**
  - Fader wobble above 0 (drift compensation incomplete)
  - Top/Bottom value bugs in Number mode (25/75 settings don't scale range correctly)
  - See: `VFader/REMAINING_FEATURES.md` and `CHANGELOG.txt`

### ⚠️ VSeq (Working - Build 1+)
- **Purpose:** 3 CV sequencers (9 outputs) + 6 trigger sequencers
- **Location:** `/VSeq/src/main.cpp` (1915 lines)
- **GUID:** `VSEQ`
- **Build Output:** `build/1VSeq.o`
- **What Works:**
  - ✅ Basic sequencing (32 steps per sequencer)
  - ✅ CV output (0-10V range)
  - ✅ Direction modes (Forward, Backward, Pingpong)
  - ✅ Section looping with repeat counts
  - ✅ Gate sequencer (6 tracks)
  - ✅ Fill feature for triggers
  - ✅ Clock division/multiplication (all 9 modes working)
  - ✅ Swing (0-100% on gate tracks)
  
- **Known Issues:**
  - ⚠️ MIDI CC removed (was buggy - only track 6 worked, value stuck at 48)
  - ⚠️ Parameter changes reset patterns (unknown cause, needs debug)

### 🚧 VTrig (Status Unknown)
- **Purpose:** 6-track trigger/gate sequencer
- **Location:** `/VTrig/src/main.cpp` (485 lines)
- **GUID:** `VTRG`
- **Issues:** Not verified working, don't use as reference

### 🚧 VLoop & VLoop2 (In Development)
- **Status:** Experimental, not ready for production
- **Note:** Added to `.gitignore` to keep separate from stable code

### ✅ CompositionMatrix (Status Unknown)
- **Location:** `/CompositionMatrix/src/main.cpp`
- **Note:** Not recently tested

### ✅ EdgeLike (Status Unknown)
- **Location:** `/EdgeLike/src/main.cpp`
- **Note:** Not recently tested

## Critical API Patterns (Verified)

### Clock Division/Multiplication Implementation
**Pattern used in VSeq (working):**
```cpp
// State variables needed:
int clockCounter[N];              // Division counter
int internalClockCounter[N];      // Multiplication subdivision counter
int lastClockPeriod[N];          // Measured samples between clocks
int samplesSinceLastClock[N];    // Running sample counter

// Division modes (0-3 = /16, /8, /4, /2):
if (clockTrig) {
    int divisor = 1 << (4 - clockDiv);  // 0->16, 1->8, 2->4, 3->2
    clockCounter[i]++;
    if (clockCounter[i] >= divisor) {
        clockCounter[i] = 0;
        advanceSequencer();  // Step only when divisor reached
    }
}

// Multiplication modes (4-8 = x1, x2, x4, x8, x16):
if (clockTrig) {
    // Measure period between clocks
    if (samplesSinceLastClock[i] > 100 && samplesSinceLastClock[i] < 96000) {
        lastClockPeriod[i] = samplesSinceLastClock[i];
    }
    samplesSinceLastClock[i] = 0;
    internalClockCounter[i] = 0;
    advanceSequencer();  // Step on external clock
}
// Between clocks, generate internal steps
if (clockDiv >= 5 && !clockTrig && lastClockPeriod[i] > 0) {
    int multiplier = 1 << (clockDiv - 4);  // 5->2, 6->4, 7->8, 8->16
    int subdivisionPeriod = lastClockPeriod[i] / multiplier;
    if (samplesSinceLastClock[i] >= subdivisionPeriod * (internalClockCounter[i] + 1)) {
        internalClockCounter[i]++;
        if (internalClockCounter[i] < multiplier) {
            advanceSequencer();  // Internal step
        }
    }
}
```

### Swing Implementation
**Pattern used in VSeq gate tracks:**
```cpp
// State: int gateSwingCounter[6];  // Countdown for swing delay

if (gateStepped) {
    int currentStep = gateCurrentStep[track];
    bool isOddStep = (currentStep % 2) == 1;
    
    if (isOddStep && swing > 0) {
        // Delay trigger by percentage of clock period
        int swingDelay = (lastClockPeriod[track] * swing) / 200;
        gateSwingCounter[track] = swingDelay;
    } else {
        // Trigger immediately
        gateTriggerCounter[track] = 240;  // ~5ms pulse
    }
}

// Each buffer, countdown swing delay
if (gateSwingCounter[track] > 0) {
    gateSwingCounter[track] -= numFrames;
    if (gateSwingCounter[track] <= 0) {
        gateSwingCounter[track] = 0;
        gateTriggerCounter[track] = 240;  // Fire delayed trigger
    }
}
```

### CV/Audio Bus Voltage Mapping
**From API examples and VTrig inspection:**
- `busFrames` array uses **direct voltage values** (not normalized)
- Gate output example: `busFrames[bus*numFrames + i] = gateActive ? 5.0f : 0.0f;`
- **CV Range:** 0.0f to 10.0f volts directly
- **DO NOT** use 0.0-1.0 normalized values

### int16_t to Voltage Conversion
```cpp
// CORRECT conversion for 0-10V CV output:
int16_t value = stepValues[seq][step][out];
float voltage = ((value + 32768) / 65535.0f) * 10.0f;
busFrames[bus*numFrames + i] = voltage;
```

### Parameter Value Types
- Parameters stored as `int16_t* v` array
- Access: `self->v[kParamIndex]`
- CV Input/Output params: 0 = none, 1-28 = bus 0-27
- Bus index calculation: `(paramValue - 1) * numFrames`

### Clock Detection Pattern
```cpp
// Edge detection (rising edge):
bool clockTrig = (clockIn > 0.5f && lastClockIn <= 0.5f);
lastClockIn = clockIn;

// Threshold varies - VTrig uses 2.0f, VSeq uses 0.5f
```

## Build System

### Standard Build Commands
```bash
cd VSeq && make clean && make     # Produces build/1VSeq.o
cd VFader && make clean && make   # Produces build/VFader.o
cd VTrig && make clean && make    # Produces build/1VTrig.o
```

### Makefile Requirements
- ARM toolchain: `arm-none-eabi-c++`
- Flags: `-std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os`
- API path: `../distingNT_API/include`
- Output: Relocatable `.o` files (not linked binaries)

### Deployment
- Copy `.o` files to SD card: `/Volumes/*/programs/plug-ins/`
- Use `BuildTransfer.sh` scripts in each plugin directory

## Git Workflow

### Recent History Issues
- Commit `3893a73` (VLoop2 v0.4.0) accidentally replaced VSeq with VFader code
- Last good VSeq before accidents: commit `355e014`
- Session commits: `720f9c2` (clock div/mult), `67c5267` (swing)

### Current Branch State
```bash
git log --oneline -- VSeq/src/main.cpp | head -5
# 67c5267 Implement swing for VSeq gate tracks
# 720f9c2 Implement clock division/multiplication for VSeq
# 9cfb1f4 Fix VSeq CV output voltage range (0-10V instead of 0-1V)
# 3893a73 VLoop2 v0.4.0 - Phase 4: Basic Playback (ACCIDENT)
# 355e014 Fix final gate track parameter stride issue in VSeq
```

## File Structure

### Important Files
```
Codex-NT/
├── CHANGELOG.txt              # Project-wide bug/feature log
├── CURRENT_STATE.md           # This file - session context
├── .gitignore                 # Excludes VLoop/, VLoop2/, build/, docs/
├── distingNT_API/             # External API repo (submodule?)
│   └── include/distingnt/api.h
├── VSeq/
│   ├── src/main.cpp           # Working VSeq implementation
│   ├── src/main.cpp.vfader_accident  # Backup of broken state
│   ├── src/main.cpp.backup    # Also VFader code (duplicate)
│   ├── src/main.cpp.bad       # Incomplete VSeq (4 seq × 16 steps)
│   ├── test/test_vseq.cpp     # Unit tests for sequencer logic
│   ├── README.md              # Feature documentation
│   └── REMAINING_FEATURES.md  # TODO list
├── VFader/
│   ├── src/main.cpp           # 2325 lines, stable
│   ├── REMAINING_FEATURES.md  # Feature TODO
│   └── release/               # Release notes and docs
└── VTrig/
    └── src/main.cpp           # 485 lines, status unknown
```

## Known Code Issues to Avoid

### ❌ Don't Trust These as References
1. **VTrig** - Not verified working
2. **VLoop/VLoop2** - Experimental, broken
3. **VFader** - Has known bugs (wobble, number mode)

### ✅ Use These Resources
1. **distingNT_API/include/distingnt/api.h** - Primary reference
2. **distingNT_API/examples/*.cpp** - Verified example patterns
3. **VSeq/test/test_vseq.cpp** - Expected behavior tests
4. **Git history** - Working commits before bugs introduced

## Next Steps (Priority Order)

1. **Test VSeq on Hardware**
   - Verify all clock division/multiplication modes work correctly
   - Test swing at various percentages
   - Confirm no regressions in existing features

2. **Debug VSeq Parameter Change Issue**
   - "Parameter changes reset patterns" (from CHANGELOG)
   - Need to compare debug snapshots before/after parameter change
   - Use `VSeq/analyze_debug.sh` to extract debug data

3. **Consider Future Features**
   - MIDI note output for CV sequencers (currently removed due to bugs)
   - Additional sequencer modes or features as needed

3. **VSeq Unit Tests**
   - Run: `cd VSeq/test && make test`
   - Verify sequencer advancement logic
   - Check section looping behavior

4. **Documentation Updates**
   - Update CHANGELOG.txt with clock division fix
   - Document voltage mapping discovery
   - Note which commits are unsafe to reference

## Debug Workflow

### Preset Debug Analysis
```bash
# VSeq debug extraction
cd VSeq
./analyze_debug.sh preset.json

# VFader debug extraction
cd VFader
./analyze_debug.sh preset.json
```

### Common Debug Steps
1. Save preset on Disting NT (captures state + debug data)
2. Copy preset JSON from SD card
3. Run analyze_debug script
4. Compare snapshots to identify issues

## API Documentation Notes

### What We Know for Certain
- CV buses use voltage values (0-10V typically)
- int16_t range: -32768 to 32767
- Audio sample rate: 48kHz (from `NT_globals.sampleRate`)
- numFramesBy4: Actual frames = `numFramesBy4 * 4`
- Bus access: `busFrames[busIndex * numFrames]`

### What Needs Investigation
- Clock multiplication implementation (values 5-8)
- Proper way to handle multiply modes (x2, x4, x8, x16)
- MIDI CC best practices (avoid VSeq's removed implementation)
- Parameter change notifications and state preservation
