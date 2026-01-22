# Codex-NT Development Context
**Last Updated:** January 22, 2026

## Current Session Work

### Just Completed
1. **VSeq restored from git** - Was accidentally replaced with VFader code in commit `3893a73`
   - Restored from working commit `355e014`
   - Saved broken version as `main.cpp.vfader_accident`
   
2. **VSeq CV output voltage fixed** - Was outputting 0-1V instead of 0-10V
   - Issue: Code converted int16_t to 0.0-1.0 float instead of 0-10V
   - Fix: Changed conversion from `(value + 32768) / 65535.0f` to `((value + 32768) / 65535.0f) * 10.0f`
   - Committed as: "Fix VSeq CV output voltage range (0-10V instead of 0-1V)"

### Currently Working On
- **VSeq Clock Division Not Working** - Parameter exists but has no effect
  - Clock div parameter: 0-8 (values: /16, /8, /4, /2, x1, x2, x4, x8, x16)
  - Problem: No `clockCounter[3]` array in struct, no division logic in step()
  - Need to add: Clock counter tracking and division/multiplication logic
  - Status: Partially started - added clockCounter array to struct, need to finish implementation

## Plugin Status Overview

### ✅ VFader (Working - Build 50)
- **Purpose:** 32 virtual faders → MIDI CC (7-bit/14-bit) + CV outputs
- **Location:** `/VFader/src/main.cpp`
- **Status:** Stable, released
- **Known Issues:**
  - Fader wobble above 0 (drift compensation incomplete)
  - Top/Bottom value bugs in Number mode (25/75 settings don't scale range correctly)
  - See: `VFader/REMAINING_FEATURES.md` and `CHANGELOG.txt`

### ⚠️ VSeq (Partially Working - Build 1)
- **Purpose:** 3 CV sequencers (9 outputs) + 6 trigger sequencers
- **Location:** `/VSeq/src/main.cpp` (1766 lines)
- **GUID:** `VSEQ`
- **Build Output:** `build/1VSeq.o`
- **What Works:**
  - ✅ Basic sequencing (32 steps per sequencer)
  - ✅ CV output (0-10V range - JUST FIXED)
  - ✅ Direction modes (Forward, Backward, Pingpong)
  - ✅ Section looping with repeat counts
  - ✅ Gate sequencer (6 tracks)
  - ✅ Swing and fill features for triggers
  
- **Known Issues:**
  - ❌ Clock division/multiplication not working (IN PROGRESS)
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
- Last good VSeq: commit `355e014` (Fix final gate track parameter stride issue)
- Multiple MIDI-related commits that led to bugs

### Current Branch State
```bash
git log --oneline -- VSeq/src/main.cpp | head -5
# 9cfb1f4 Fix VSeq CV output voltage range (0-10V instead of 0-1V)
# 3893a73 VLoop2 v0.4.0 - Phase 4: Basic Playback
# 355e014 Fix final gate track parameter stride issue in VSeq
# f90c6b5 Fix gate track step selection in VSeq customUi
# e0ab1b3 Update VSeq parameter defaults and fix gate track display bug
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

1. **Finish VSeq Clock Division Fix**
   - Add clock counter initialization in constructor
   - Implement division logic in step() function
   - Test with all 9 division values (/16 through x16)
   - Commit when working

2. **Debug VSeq Parameter Change Issue**
   - "Parameter changes reset patterns" (from CHANGELOG)
   - Need to compare debug snapshots before/after parameter change
   - Use `VSeq/analyze_debug.sh` to extract debug data

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
