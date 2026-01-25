# Codex-NT Codebase Instructions for AI Agents

## Project Overview

Codex-NT is a collection of C++ plugins for the **Expert Sleepers Disting NT**, a compact hardware synthesizer platform. Each plugin is a standalone algorithm compiled to ARM object files (`.o`) and deployed to the device's SD card at `/programs/plug-ins/`.

**Plugin Lineup:**
- **VFader**: 32 virtual faders (4 pages × 8) → MIDI CC (7-bit/14-bit)
- **VSeq**: 3 CV sequencers (9 outputs) + 6-track trigger sequencer
- **VTrig**: 6-track trigger/gate sequencer with swing & fills
- **VLoop**: Looping recorder (status: under development)
- **CompositionMatrix**: Single-voice sequencer with clocked shift register
- **EdgeLike**: Percussive synth voice (OSC, filter, envelopes)
- **DistingLua**: Lua scripts for Disting workflows

## Architecture & Core Patterns

### Plugin Structure
Each plugin inherits from `_NT_algorithm` (from `distingnt/api.h`) and implements:
- **Constructor**: Parameter setup via enum-indexed array (`parameters[kParamX]`)
- **step()**: Real-time audio/CV processing (called on each sample)
- **parameterChanged()**: Handle UI control updates (pots, encoders, buttons)
- **serialize()/deserialize()**: JSON persistence for presets

**Example Parameter Setup** (VFader):
```cpp
struct VFader : public _NT_algorithm {
  enum { kParamPage, kParamMidiMode, kParamPickupMode, kParamFader1, ... };
  void setup() {
    parameters[kParamPage].unit = kNT_unitEnum;
    parameters[kParamPage].enumStrings = pageStrings; // UI dropdown
  }
};
```

### Hardware Interaction
- **Left Encoder**: Navigation (pages, sequencer selection)
- **Right Encoder**: Selection within current view (faders, steps)
- **Left/Center/Right Pots**: Direct value control
- **Buttons**: Mode toggles, edit entry, fill triggers
- **Display**: Custom text/graphics rendering (pixel-based)
- **CV Buses**: 28 inputs (1-12 audio + aux), 8 outputs
- **MIDI**: CC output (7-bit CC 1-32, or 14-bit pairs)

### Data Persistence & JSON Format
Plugins store state in JSON presets (auto-saved on SD card). Example structure:
```json
{
  "slots": [
    {
      "guid": "VFDR",          // Plugin ID (4 chars)
      "parameters": { ... },   // All parameter states
      "debug": { ... },        // Debug snapshots (for troubleshooting)
      "faders": [ ... ],       // Plugin-specific data arrays
      "presets": [ ... ]
    }
  ]
}
```

**Key patterns:**
- Use `GUID` (4-char code) to identify plugins: `VFDR`, `VSEQ`, `VTRG`, etc.
- Parameters stored as float/int values in JSON
- Complex state (fader names, sequencer patterns) in auxiliary arrays
- Debug sections added voluntarily for crash diagnosis

## Build System

### Compilation
All plugins use **ARM EABI toolchain** with **Cortex-M7** target:
```bash
# Standard Makefile pattern (VFader example)
TARGET_ARCH := arm-none-eabi-
CXX := $(TARGET_ARCH)c++
CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard \
            -mthumb -fno-exceptions -Os -Wall
INCLUDES := -I../distingNT_API/include -Iinclude
```

**Build commands:**
```bash
cd VFader && make         # Produces: build/VFader.o
cd VSeq && make           # Produces: build/1VSeq.o
cd VTrig && make          # Produces: build/1VTrig.o
make clean                # Remove build artifacts
```

**Output format:** Relocatable object files (`.o`), not linked binaries. The Disting NT host links them at runtime.

### Deployment
Each plugin has a `BuildTransfer.sh` script:
```bash
./BuildTransfer.sh        # Copies .o to SD card at /volumes/*/programs/plug-ins/
```
Expects SD card mounted as `/Volumes/Untitled` (or `DISTDEV NT`). Manually edit scripts for different mount points.

## Testing & Debugging

### Unit Tests (VSeq)
Located in [VSeq/test/](VSeq/test/):
```bash
cd VSeq/test && make test
```
Tests validate sequencer logic (forward/backward/pingpong modes, section looping, fill feature). Run on host machine (macOS/Linux), not on ARM target. Sequencer advancement logic is duplicated in test file to avoid complex build dependencies.

**Test framework:** Custom assertion macros (`EXPECT_EQ`, `EXPECT_TRUE`). No external dependencies.

### Debug Analysis Scripts
- **VFader**: [analyze_debug.sh](VFader/analyze_debug.sh) → Parses JSON preset, extracts debug snapshot
- **VSeq**: [analyze_debug.sh](VSeq/analyze_debug.sh) → Similar debug extraction
- **VLoop**: [analyze_debug.py](VLoop/analyze_debug.py) → Analyzes crash data

These extract `debug` section from preset JSON and display state information (MIDI counts, soft takeover status, CV values).

### Preset Workflow
1. Modify plugin code
2. `make && ./BuildTransfer.sh` → Deploy to SD card
3. Save preset on Disting NT (embeds plugin state + debug data)
4. `cd VFader && ./analyze_debug.sh` → Extract and analyze debug info
5. Iterate based on findings

## Project-Specific Conventions

### Parameter Naming
- **CV Sequencers**: `Seq1Out1`, `Seq1ClockDiv`, `Seq1Steps`, `Seq1SplitPoint`, `Seq1Sec1Reps`, `Seq1Sec2Reps`
- **Trigger Tracks**: `Gate1Out`, `Gate1Run`, `Gate1Length`, `Gate1Direction`, `Gate1ClockDiv`, `Gate1Swing`, `Gate1Split`, `Gate1Sec1Reps`, `Gate1Sec2Reps`
- **Global**: `ClockIn`, `ResetIn` (CV bus inputs, 0 = unused, 1-28 = input sources)

### Display Constants
All plugins use fixed display dimensions:
- **Page Indicator**: Top-right corner (page numbers 1-4)
- **Selected Item**: Underlined in display
- **Current Position**: Marked with dot or highlight
- **Color codes**: 15 (bright), 7 (dim) — used for active/inactive note masks

### Sequencer Architecture (VSeq, VTrig)
All sequencers support **section looping**:
- **Split Point**: Divides sequence into Section 1 (steps 0–splitPoint) and Section 2 (rest)
- **Repeat Counts**: Sec1Reps × Sec1, then Sec2Reps × Sec2, then loop
- **Pingpong Mode**: Disables section looping (full sequence bounces)
- **Fill Feature** (VTrig only): On last repeat of Sec1, jump to fillStart in Sec2

### Pickup Modes (VFader)
- **Scaled**: Physical pot position scales the fader's configured range
- **Catch**: Fader value doesn't update until pot reaches current fader value (soft takeover)
- Used to prevent value jumps when resuming control

### Macro Fader Control (VFader)
- **Absolute mode**: Child faders move in parallel (maintains spacing)
- **Relative mode**: Child faders scale proportionally (50% = no change, <50% scales toward 0, >50% scales toward max)

## Integration Points & External Dependencies

### distingNT_API
Located at `../distingNT_API/include/distingnt/`. **Must exist relative to plugin directories.**
- Provides `_NT_algorithm` base class, parameter structures, CV/MIDI send functions
- Makefile validates API path with `check_api` target

### Disting NT Hardware
- **Inputs**: 28 CV buses (audio + aux), external clock, reset, triggers
- **Outputs**: 8 CV buses, gate/trigger outputs, MIDI CC
- **Storage**: SD card with preset JSON files

### Build Environment
- **ARM GCC**: `arm-none-eabi-gcc`, `arm-none-eabi-g++` (must be in PATH)
- **macOS/Linux**: Required for shell scripts and testing
- **Python 3**: For debug analysis scripts

## Known Issues & Workarounds

### VFader
- **Fader wobble above 0**: Drift compensation incomplete
- **Top/Bottom value bugs**: Setting values to 75 or 25 doesn't scale fader range correctly (Number mode may not be fully implemented)
- **Reference**: [CHANGELOG.txt](../CHANGELOG.txt), [REMAINING_FEATURES.md](VFader/REMAINING_FEATURES.md)

### VSeq
- **MIDI CC removed**: Previous implementation had bugs (only track 6 working, stuck value at 48)
- **Parameter changes reset patterns**: Unknown cause—need debug data comparison

### Build Quirks
- Output naming convention: `VFader.o`, `1VSeq.o`, `1VTrig.o` — leading `1` prefixes some plugins (for sort order on SD card)
- `.gitmodules` present but path unclear—distingNT_API is a separate repo

## Key Files & Directories

| Path | Purpose |
|------|---------|
| [VFader/src/main.cpp](VFader/src/main.cpp) | 32-fader MIDI controller (2325 lines) |
| [VSeq/src/main.cpp](VSeq/src/main.cpp) | 3× CV sequencer + 6× trigger sequencer |
| [VSeq/test/test_vseq.cpp](VSeq/test/test_vseq.cpp) | Unit tests for sequencer logic |
| [Oneiroi/include/Oneiroi/](Oneiroi/include/Oneiroi/) | Reusable DSP library (filters, envelopes, etc.) |
| [DistingLuaScripts/](DistingLuaScripts/) | Lua scripts for Disting (separate from NT plugins) |
| [CHANGELOG.txt](../CHANGELOG.txt) | Project-wide change log |
| [distingNT_API/](../distingNT_API/) | (External) NT host API headers |

## Iteration Workflow

**Typical development cycle:**
1. Edit plugin source in `VFader/src/main.cpp` (or equivalent)
2. Run `cd VFader && make` → Verify no ARM compilation errors
3. Run `./BuildTransfer.sh` → Copy to SD card, eject
4. Open Disting NT preset editor, save a test preset
5. Fetch preset via script (e.g., `./FetchPreset.sh`)
6. Run `./analyze_debug.sh` → Inspect debug data for issues
7. If bug found, add test case to `VSeq/test/test_vseq.cpp`, run `make test`
8. Fix code, repeat from step 2

**Red flags:**
- Compilation error about missing `distingnt/api.h` → Fix `NT_API_PATH` in Makefile
- SD card not mounting → Edit `BuildTransfer.sh` with correct volume name
- Preset not found → Ensure preset was saved on device before fetching

## Serialization Patterns (Critical for Preset Persistence)

### The Problem
Custom data arrays (like `steps[6][32]` or `stepValues[32][3]`) are NOT automatically saved by the Disting NT host. Only parameters in the `self->v[]` array persist automatically.

### The Solution: serialize/deserialize Functions

**Key Requirements:**
1. **Add serialise() function** to write custom data to JSON stream
2. **Add deserialise() function** to read custom data from JSON
3. **Update factory** to point to these functions (not nullptr)
4. **Critical:** Call `parse.skipMember()` for unrecognized JSON members in deserialise()
5. **Critical:** Do NOT initialize arrays in constructor (deserialise runs AFTER construct)

**Pattern from VTrig (working implementation):**
```cpp
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VTrig* a = (VTrig*)self;
    stream.addMemberName("steps");
    stream.openArray();
    for (int track = 0; track < 6; track++) {
        stream.openArray();
        for (int step = 0; step < 32; step++) {
            stream.addBoolean(a->steps[track][step]);
        }
        stream.closeArray();
    }
    stream.closeArray();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VTrig* a = (VTrig*)self;
    
    int numMembers = 0;
    if (!parse.numberOfObjectMembers(numMembers))
        return false;
    
    // CRITICAL: Loop through ALL members
    for (int i = 0; i < numMembers; i++) {
        if (parse.matchName("steps")) {
            int numTracks = 0;
            if (parse.numberOfArrayElements(numTracks)) {
                int tracksToLoad = (numTracks < 6) ? numTracks : 6;
                for (int track = 0; track < tracksToLoad; track++) {
                    int numSteps = 0;
                    if (parse.numberOfArrayElements(numSteps)) {
                        int stepsToLoad = (numSteps < 32) ? numSteps : 32;
                        for (int step = 0; step < stepsToLoad; step++) {
                            bool value;
                            if (parse.boolean(value)) {
                                a->steps[track][step] = value;
                            }
                        }
                    }
                }
            }
        } else {
            // CRITICAL: Skip unrecognized members
            parse.skipMember();
        }
    }
    return true;
}

// Update factory
static const _NT_factory factory = {
    // ... other fields ...
    .serialise = serialise,      // NOT nullptr
    .deserialise = deserialise,  // NOT nullptr
    // ... other fields ...
};
```

**Common Mistakes:**
- ❌ Initializing arrays in constructor → Arrays zeroed AFTER deserialise loads data
- ❌ Not calling `parse.numberOfObjectMembers()` at start → Parse state not set up
- ❌ Not calling `parse.skipMember()` for unknown members → Parser gets stuck
- ❌ Leaving factory pointers as nullptr → Serialization never called

**API Methods Available:**
- `stream.addBoolean(bool)`, `stream.addNumber(int/float)`, `stream.addString(const char*)`
- `parse.boolean(bool&)`, `parse.number(int/float&)`, `parse.string(const char*&)`
- `parse.numberOfArrayElements(int&)`, `parse.numberOfObjectMembers(int&)`
- `parse.matchName(const char*)`, `parse.skipMember()`

## Release Process (GitHub)

### 1. Prepare Code
- Remove unused features (MIDI, Random mode, etc.)
- Update release notes in `<plugin>/release/RELEASE_NOTES.md`
- Build: `cd <plugin> && make && cp build/<plugin>.o release/`
- Test on hardware, verify preset persistence

### 2. Commit and Push
```bash
cd /Users/corygraddy/Documents/Codex-NT
git add <plugin>/src/main.cpp <plugin>/release/<plugin>.o
git commit -m "Description of changes"
git push origin main
```

### 3. Update Dedicated Repos (git subtree)
```bash
# VTrig
git subtree push --prefix=VTrig https://github.com/corygraddy/VTrig.git main

# V3Seq
git subtree push --prefix=V3Seq https://github.com/corygraddy/V3Seq.git main
```

### 4. Create Tags (in dedicated repos, not main Codex-NT)
```bash
# Clone dedicated repo to /tmp
cd /tmp
git clone https://github.com/corygraddy/VTrig.git
cd VTrig
git tag v1.0.0
git push origin v1.0.0
```

**Why not tag in main repo?**
- Pushing tags from main repo includes full Codex-NT history (huge upload)
- Tags must be created in dedicated repos to avoid multi-GB transfers

### 5. Create GitHub Release
1. Navigate to `https://github.com/corygraddy/<plugin>/releases`
2. Click "Draft a new release"
3. Choose tag (e.g., v1.0.0)
4. Upload `.o` file from `<plugin>/release/`
5. Copy release notes from `<plugin>/release/RELEASE_NOTES.md`
6. Publish release

### 6. NT Gallery Upload
- Upload `.o` file with matching tag version
- Gallery validator may show errors even if plugin works on hardware
- Unresolved: "no metadata returned for analysis" error

**Plugin Sizes (typical):**
- VTrig: ~11KB
- V3Seq: ~7KB
- VFader: ~19KB

**Released Versions:**
- VTrig v1.0.0: January 24, 2026
- V3Seq v1.0.0: January 24, 2026
