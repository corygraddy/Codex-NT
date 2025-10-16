Of course. Here is a summary of our current task. You can paste this back to me after you restart.

Project: CompositionMatrix File: /Users/corygraddy/Documents/Codex-NT/CompositionMatrix/src/main.cpp Goal: Debug a UI issue. Problem: The saved pattern value in "Slot 0" always displays as "0" and doesn't update after saving. Current Step: We are testing a fix by changing how the value is formatted for the display. We are modifying the snprintf line inside the draw function to use the decimal format specifier (%u) instead of hexadecimal (%lX) to see if that resolves the display problem.


all i see in the ui is this ...

CompositionMatrix

Mode: Explore
Live: 12345678


----------------------------------------
| UI: 0x2021eb50                            |
| CompositionMatrix                       |  
|  Mode: Refine                               |
| Slot 0: 0                                        |
| Live: 12345678                             |
----------------------------------------

----------------------------------------
| 
| All      Bypass <--- the word off is superimposed ontop of "Bypass"
| Algorithm         <--- off should appear here
| 
| 
----------------------------------------


context generated 10/13 10:01pm ---

Of course. Here is a summary of our current session to help you pick up where you left off:

Project: CompositionMatrix Goal: Debug an issue where a saved pattern value is not updating on the UI.

Current Status: We've beenunable to get the "CompositionMatrix" plugin to load on your Disting NT, even after confirming the build process and file transfer are correct. We've tried several debugging steps, including creating minimal test algorithms, but none have appeared on the device.

Next Steps: My primary theory is that the plugin is exceeding a memory limit on the Disting NT, causing it to be rejected. The next step is to drastically simplify the CompositionMatrixAlgorithm struct in main.cpp to its bare essentials, removing large data structures like the patterns array. We will then build this minimal version and see if it loads on your device. If it does, we can incrementally add back features to pinpoint the cause of the issue.

# Disting NT Algorithm Development Reference Guide

This guide provides a reference for developing algorithms for the Disting NT, based on the examples in the `distingNT_API/examples` directory.

## 1. Plugin Entry Point (`pluginEntry`)

The entry point of a Disting NT plugin is the `pluginEntry` function. It is responsible for responding to requests from the host environment.

-   **Signature**: `uintptr_t pluginEntry(_NT_selector selector, uint32_t data)`
-   **Selectors**:
    -   `kNT_selector_version`: Return `kNT_apiVersionCurrent`.
    -   `kNT_selector_numFactories`: Return the number of factories in the plugin.
    -   `kNT_selector_factoryInfo`: Return a pointer to the `_NT_factory` struct for the factory specified by the `data` index.

**Example (`multiple.cpp`):**

```c++
uintptr_t pluginEntry( _NT_selector selector, uint32_t data )
{
	switch ( selector )
	{
	case kNT_selector_version:
		return kNT_apiVersionCurrent;
	case kNT_selector_numFactories:
		return 2;
	case kNT_selector_factoryInfo:
		if ( data == 0 )
			return (uintptr_t)&factory1;
		if ( data == 1 )
			return (uintptr_t)&factory2;
		return 0;
	}
	return 0;
}
```

## 2. Factory (`_NT_factory`)

A factory defines a single algorithm. A plugin can contain multiple factories.

-   **Structure**: `_NT_factory`
-   **Key Members**:
    -   `.guid`: A unique 4-character identifier for the algorithm. Use `NT_MULTICHAR()`.
    -   `.name`: The name of the algorithm.
    -   `.description`: A short description of the algorithm.
    -   `.numSpecifications`: The number of specifications (configurable options at algorithm creation).
    -   `.specifications`: An array of `_NT_specification` structs.
    -   `.calculateRequirements`: A function pointer to calculate memory requirements.
    -   `.construct`: A function pointer to create an instance of the algorithm.
    -   `.parameterChanged`: A function pointer that is called when a parameter value changes.
    -   `.step`: The main DSP processing function.
    -   `.draw`: A function pointer for drawing to the screen.
    -   `.midiMessage`: A function pointer for handling MIDI messages.
    -   `.hasCustomUi`, `.customUi`, `.setupUi`: For custom UI handling.
    -   `.serialise`, `.deserialise`: For saving and loading state.

## 3. Algorithm Structure

The core of your plugin is the algorithm struct, which inherits from `_NT_algorithm`.

-   **Main Struct**: Inherits from `_NT_algorithm`. This struct holds the algorithm's state that does *not* need to persist across power cycles or preset loads.
-   **DTC (Data To Continue) Struct**: If you need to store data that persists, define a separate struct for it and allocate it in the `dtc` memory region. The `_monosynthAlgorithm` and `_midiCcConverterAlgorithm` examples demonstrate this.

**Example (`gain.cpp`):**

```c++
struct _gainAlgorithm : public _NT_algorithm
{
	_gainAlgorithm() {}
	~_gainAlgorithm() {}

	float gain;
};
```

**Example with DTC (`monosynth.cpp`):**

```c++
struct _monosynthAlgorithm_DTC
{
	uint32_t	phase;
	uint32_t	inc;
	float 		env;
	bool		gate;
};

struct _monosynthAlgorithm : public _NT_algorithm
{
	_monosynthAlgorithm( _monosynthAlgorithm_DTC* dtc_ ) : dtc( dtc_ ) {}
	~_monosynthAlgorithm() {}

	_monosynthAlgorithm_DTC*	dtc;
};
```

## 4. Memory Management

-   **`calculateRequirements`**: This function is called to determine the memory needs of your algorithm.
    -   **Signature**: `void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications)`
    -   **`req` members**:
        -   `numParameters`: The total number of parameters.
        -   `sram`: The size of your main algorithm struct.
        -   `dram`: Additional fast memory.
        -   `dtc`: The size of your persistent data struct. Can be used for arrays of objects, as seen in `th_tinear.cpp`.
        -   `itc`: Instruction cache memory.
-   **`construct`**: This function is called to initialize an instance of your algorithm.
    -   **Signature**: `_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications)`
    -   Use placement new to initialize your algorithm struct in the provided `sram`.
    -   If you use a DTC struct, the pointer to it is in `ptrs.dtc`.
    -   Assign the `parameters` and `parameterPages` members of your algorithm struct.
-   **`calculateStaticRequirements` and `initialise`**: For one-time setup when the plugin is loaded, as seen in `nt_enosc.cpp`.

## 5. Parameters

-   **`_NT_parameter`**: Defines a single parameter.
    -   **Key Members**: `.name`, `.min`, `.max`, `.def`, `.unit`, `.scaling`, `.enumStrings`.
    -   The API provides macros for common parameter types like `NT_PARAMETER_AUDIO_INPUT` and `NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE`.
-   **`_NT_parameterPage`**: Groups parameters into pages for the UI.
-   **`_NT_parameterPages`**: A collection of parameter pages.
-   **`parameterChanged`**: A callback function that is invoked when a parameter's value changes. Use this to update cached values in your algorithm struct.
-   **Dynamic Parameters**: The `th_tinear.cpp` example shows how to dynamically generate parameters and pages in the constructor, which is useful for algorithms with a variable number of components (like emitters).

## 6. DSP (`step`)

The `step` function is the heart of your algorithm's audio processing.

-   **Signature**: `void step(_NT_algorithm* self, float* busFrames, int numFramesBy4)`
-   `busFrames`: A pointer to the buffer containing all audio and CV busses.
-   `numFramesBy4`: The number of frames to process, divided by 4. The total number of frames is `numFramesBy4 * 4`.
-   Access input and output busses by getting the bus index from the parameter values (`pThis->v[kParamInput]`) and calculating the offset in the `busFrames` buffer.

## 7. MIDI Handling (`midiMessage`)

-   **Signature**: `void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2)`
-   This function is called for each incoming MIDI message.
-   The `monosynth.cpp` and `midiLFO.cpp` examples show how to handle incoming MIDI and send outgoing MIDI messages (`NT_sendMidi3ByteMessage`, etc.).

## 8. UI (`draw`, Custom UI)

-   **`draw`**:
    -   **Signature**: `bool draw(_NT_algorithm* self)`
    -   Called to draw the algorithm's UI on the screen.
    -   Use `NT_drawText`, `NT_drawShapeI`, `NT_drawShapeF`, etc. to draw.
    -   The screen buffer is available at `NT_screen`.
    -   The `dnb_seq.cpp` example shows a complex graphical UI with a piano-roll style grid.
-   **Custom UI**:
    -   `hasCustomUi`: Return a bitmask of the pots and buttons your UI uses (e.g., `kNT_potC`).
    -   `customUi`: Handle changes from the custom controls.
    -   `setupUi`: Initialize the state of the custom controls.
    -   The `gainCustomUI.cpp` and `dnb_seq.cpp` examples demonstrate this.

## 9. Serialization (`serialise`, `deserialise`)

-   **`serialise`**:
    -   **Signature**: `void serialise(_NT_algorithm* self, _NT_jsonStream& stream)`
    -   Write the algorithm's state to a JSON stream.
-   **`deserialise`**:
    -   **Signature**: `bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse)`
    -   Read the algorithm's state from a JSON parser.
-   The `gainCustomUI.cpp` example shows how to use these functions.

## 10. Advanced Topics

### Parameter Smoothing

To avoid audible clicks and pops when parameters change, it is crucial to smooth the parameter values over time. The `nt_enosc.cpp` example demonstrates this with a `Smoother` class that uses a simple one-pole filter to interpolate between the current and target parameter values.

### Integrating External Libraries

The `nt_enosc.cpp` and `th_tinear.cpp` examples show how to include and use external C/C++ code in your project. You can add the source files to your project and include the headers in your main plugin file.

### A Note on Conflicting Strategies

As you explore different example projects, you may notice different approaches to solving similar problems. This often reflects the evolution of the Disting NT API and the discovery of new best practices. When in doubt, look to the most recent or most complex examples for modern and robust solutions. For instance, the use of parameter smoothing in `nt_enosc.cpp` is a more advanced and desirable technique than directly using parameter values in the `step` function.

## Final Summary & Unresolved UI Redraw Issue (2025-10-14)

**Goal:** Fix a UI bug where a saved value is not updated on the screen until the algorithm is reloaded.

**Summary of Progress:**

1.  **Preset Saving:** We successfully fixed an issue where the Disting NT would not save the preset because it didn't detect any changes. We solved this by programmatically incrementing a hidden "State Version" parameter (`kParamStateVersion`) whenever a change occurred, correctly marking the algorithm's state as "dirty."

2.  **Core Logic:** We confirmed that the algorithm's core generative logic (`shiftRegister`) is working correctly, driven by an external clock signal.

3.  **Display Rendering:** We identified and fixed a major bug where the saved value was always displayed as "0". The cause was an unreliable `snprintf` implementation in the Disting's embedded environment. We successfully bypassed this by creating a manual `uint32_to_hex_string` helper function. The UI now correctly displays the saved value when the algorithm is first loaded.

**The Unresolved Problem:**

The final remaining issue is that the custom UI does not update immediately after a new value is saved. When the user presses the button to save the current pattern, the `draw()` function is not called again by the host environment, so the screen continues to display the old value until the entire algorithm is reloaded.

**What We Tried:**

To solve this, we tried to force the host to call `draw()` again. Our attempts included:
-   **Changing a hidden parameter:** We programmatically changed our hidden `kParamStateVersion` parameter in the `customUi()` function. This did not trigger a redraw.
-   **Adding a second dummy parameter:** We added another hidden parameter (`kParamDummy`) and toggled its value. This also failed.
-   **Forcing periodic updates:** We added a counter to the `step()` function to toggle the dummy parameter automatically every second. This also failed.
-   **Using a Parameter Page:** Based on an analysis of the `nt_grids` example, we placed our `kParamStateVersion` parameter onto a defined parameter page. This also failed to trigger a redraw.

**Conclusion:**

We have exhausted all reasonable, documented, and experimental methods for forcing a custom UI redraw from within the `customUi()` callback. The issue seems to be a fundamental, undocumented aspect of the Disting NT's UI lifecycle, or a bug in the firmware. The fact that other example algorithms (like `dnb_seq`) also show inconsistent UI behavior supports this conclusion.

The code has been cleaned of all failed experiments, leaving it in a state where the core logic and preset handling are fully functional, and the initial display of the saved value is correct. The next step is likely to contact the Expert Sleepers developers with this detailed summary.

---
*Date: 2025-10-15*

## CompositionMatrix Design Pivot: Parameter-Driven Workflow

To bypass potential UI bugs in the Disting NT firmware, the CompositionMatrix project is temporarily pivoting to a UI-less, parameter-driven design.

- **Mode Switching:** A parameter `pMode` will control the state.
  - `0`: Explore Mode (generates new patterns).
  - `1`: Refine Mode (freezes the current pattern for tweaking).

- **Save Action:** A parameter `pSave` will act as a trigger.
  - The save operation is executed only on a rising edge transition (from `0` to `1`).
  - This requires storing the parameter's previous state to detect the change, preventing continuous saves.

## CompositionMatrix Parameter Grouping Design (In Work)

This design organizes all parameters from the v5.0 design document into a structure suitable for the Disting NT's paged UI.

### Page 1: `SYSTEM` (Global settings)
-   `Global Key`: (C, C#, D, ...)
-   `Global Scale`: (Major, Minor, Dorian, ...)
-   `Polyphony Mode`: (Lead Only, All Voices, Headless, ...)
-   `Num Support Voices`: (1, 2, 3)

### Page 2: `PATTERN` (Core workflow)
-   `Pattern Slot`: (Selector: 0-99)
-   `Mode`: (Switch: 0=Explore, 1=Refine)
-   `Save`: (Trigger: 0->1)
-   `Duplicate`: (Trigger: 0->1)

### Page 3: `HARMONY` (Musical theory)
-   `Musical Mode`: (Ionian, Dorian, ...)
-   `Chord Extension`: (Triad, 7th, 9th, ...)
-   `Harmonic Movement`: (Probability %)
-   `Beats Per Measure`: (2-13)

### Page 4: `RHYTHM` (Gate and timing)
-   `Density: Lead`: (Probability %)
-   `Density: Bass`: (Probability %)
-   `Density: Support`: (Probability %)
-   `Gate Length`: (Macro: Staccato -> Legato)

### Page 5: `DYNAMICS` (Velocity and expression)
-   `Base Velocity`: (0-127)
-   `Velocity Dynamics`: (Range %)

### Page 6: `PITCH` (Octave and range)
-   `Lead Octave Spread`: (0-7)
-   `Lead Octave Offset`: (-3 to +3)
-   `Bass Octave Spread`: (0-7)
-   `Bass Octave Offset`: (-3 to +3)
-   `Support Octave Spread`: (0-7)
-   `Support Octave Offset`: (-3 to +3)

### Page 7: `CHAOS` (Deterministic "accidents")
-   `Velocity Chaos`: (Probability %)
-   `Gate Chaos`: (Probability %)
-   `Note Chaos`: (Probability %)

### Page 8: `SONG - EDIT` (Arrangement building)
-   `Song Slot`: (Selector: 0-49)
-   `Assign Pattern`: (Selector: 0-99)
-   `Repeat Count`: (1-16)
-   `Insert Step`: (Trigger: 0->1)
-   `Delete Step`: (Trigger: 0->1)

### Page 9: `SONG - PERFORM` (Live song playback)
-   `Live Audition`: (Selector: 0-99)
-   `Step Voicing`: (Per-step override for Polyphony Mode)
-   `Step Num Voices`: (Per-step override for Num Support Voices)
-   `Restart Song`: (Trigger: 0->1)
-   `Stop Song`: (Trigger: 0->1)

### Page 10: ---- don't forget to add a inputs and outputs pages

---
*Date: 2025-10-16*

## CompositionMatrix Build Fix

The last build attempt failed with two main errors:
1.  **`cannot convert 'const char*' to 'const char* const*'`**: This is for the `enumStrings` member of the `_NT_parameter` struct. It seems that the `enumStrings` member expects an array of strings, not a single comma-separated string.
2.  **`invalid conversion from 'const uint8_t*' to 'uint8_t'` and `invalid conversion from 'int' to 'const uint8_t*'`**: This is for the `_NT_parameterPage` initialization. The arguments for the `_NT_parameterPage` struct were in the wrong order.

The proposed change is to fix these issues by:
1.  Defining static arrays of C-style strings for each enum and assigning them to the `enumStrings` member of the corresponding parameters.
2.  Correcting the order of arguments in the `_NT_parameterPage` initialization to `(name, numParams, params)`.

The following code will be used to fix the issues:

```cpp
static const char* const globalKeyStrings[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", NULL };
static const char* const globalScaleStrings[] = { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", NULL };
static const char* const polyphonyModeStrings[] = { "Lead Only", "All Voices", "Headless", NULL };
static const char* const modeStrings[] = { "Explore", "Refine", NULL };
static const char* const musicalModeStrings[] = { "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Aeolian", NULL };
static const char* const chordExtensionStrings[] = { "Triad", "7th", "9th", NULL };
static const char* const stepVoicingStrings[] = { "Lead Only", "All Voices", "Headless", NULL };

// Static array defining the parameters available in the UI.
static const _NT_parameter parameters[kNumParameters] = {
    // Page 1: SYSTEM
    { .name = "Global Key", .min = 0, .max = 11, .def = 0, .enumStrings = globalKeyStrings },
    { .name = "Global Scale", .min = 0, .max = 6, .def = 0, .enumStrings = globalScaleStrings },
    { .name = "Polyphony Mode", .min = 0, .max = 2, .def = 0, .enumStrings = polyphonyModeStrings },
    { .name = "Num Support Voices", .min = 1, .max = 3, .def = 1 },

    // Page 2: PATTERN
    { .name = "Pattern Slot", .min = 0, .max = 99, .def = 0 },
    { .name = "Mode", .min = 0, .max = 1, .def = 0, .enumStrings = modeStrings },
    { .name = "Save", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Duplicate", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 3: HARMONY
    { .name = "Musical Mode", .min = 0, .max = 6, .def = 0, .enumStrings = musicalModeStrings },
    { .name = "Chord Extension", .min = 0, .max = 2, .def = 0, .enumStrings = chordExtensionStrings },
    { .name = "Harmonic Movement", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Beats Per Measure", .min = 2, .max = 13, .def = 4 },

    // Page 4: RHYTHM
    { .name = "Density: Lead", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Density: Bass", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Density: Support", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },
    { .name = "Gate Length", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },

    // Page 5: DYNAMICS
    { .name = "Base Velocity", .min = 0, .max = 127, .def = 100 },
    { .name = "Velocity Dynamics", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent },

    // Page 6: PITCH
    { .name = "Lead Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Lead Octave Offset", .min = -3, .max = 3, .def = 0 },
    { .name = "Bass Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Bass Octave Offset", .min = -3, .max = 3, .def = -1 },
    { .name = "Support Octave Spread", .min = 0, .max = 7, .def = 1 },
    { .name = "Support Octave Offset", .min = -3, .max = 3, .def = 0 },

    // Page 7: CHAOS
    { .name = "Velocity Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },
    { .name = "Gate Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },
    { .name = "Note Chaos", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent },

    // Page 8: SONG - EDIT
    { .name = "Song Slot", .min = 0, .max = 49, .def = 0 },
    { .name = "Assign Pattern", .min = 0, .max = 99, .def = 0 },
    { .name = "Repeat Count", .min = 1, .max = 16, .def = 1 },
    { .name = "Insert Step", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Delete Step", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 9: SONG - PERFORM
    { .name = "Live Audition", .min = 0, .max = 99, .def = 0 },
    { .name = "Step Voicing", .min = 0, .max = 2, .def = 0, .enumStrings = stepVoicingStrings },
    { .name = "Step Num Voices", .min = 1, .max = 3, .def = 1 },
    { .name = "Restart Song", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Stop Song", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },

    // Page 10: INPUTS
    NT_PARAMETER_CV_INPUT("Clock In", 1, 0),
    NT_PARAMETER_CV_INPUT("Freeze In", 2, 0),

    // Page 11: OUTPUTS
    NT_PARAMETER_CV_OUTPUT("Pitch Out", 1, 0),
    NT_PARAMETER_CV_OUTPUT("Gate Out", 2, 0),

    // This parameter is not on any page, so it's hidden from the user.
    { .name = "State Version", .min = 0, .max = 32767, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
};

static const uint8_t page1_params[] = { kParamGlobalKey, kParamGlobalScale, kParamPolyphonyMode, kParamNumSupportVoices };
static const _NT_parameterPage page1_system = { "SYSTEM", 4, page1_params };
static const uint8_t page2_params[] = { kParamPatternSlot, kParamMode, kParamSave, kParamDuplicate };
static const _NT_parameterPage page2_pattern = { "PATTERN", 4, page2_params };
static const uint8_t page3_params[] = { kParamMusicalMode, kParamChordExtension, kParamHarmonicMovement, kParamBeatsPerMeasure };
static const _NT_parameterPage page3_harmony = { "HARMONY", 4, page3_params };
static const uint8_t page4_params[] = { kParamDensityLead, kParamDensityBass, kParamDensitySupport, kParamGateLength };
static const _NT_parameterPage page4_rhythm = { "RHYTHM", 4, page4_params };
static const uint8_t page5_params[] = { kParamBaseVelocity, kParamVelocityDynamics };
static const _NT_parameterPage page5_dynamics = { "DYNAMICS", 2, page5_params };
static const uint8_t page6_params[] = { kParamLeadOctaveSpread, kParamLeadOctaveOffset, kParamBassOctaveSpread, kParamBassOctaveOffset, kParamSupportOctaveSpread, kParamSupportOctaveOffset };
static const _NT_parameterPage page6_pitch = { "PITCH", 6, page6_params };
static const uint8_t page7_params[] = { kParamVelocityChaos, kParamGateChaos, kParamNoteChaos };
static const _NT_parameterPage page7_chaos = { "CHAOS", 3, page7_params };
static const uint8_t page8_params[] = { kParamSongSlot, kParamAssignPattern, kParamRepeatCount, kParamInsertStep, kParamDeleteStep };
static const _NT_parameterPage page8_song_edit = { "SONG - EDIT", 5, page8_params };
static const uint8_t page9_params[] = { kParamLiveAudition, kParamStepVoicing, kParamStepNumVoices, kParamRestartSong, kParamStopSong };
static const _NT_parameterPage page9_song_perform = { "SONG - PERFORM", 5, page9_params };
static const uint8_t page10_params[] = { kParamClockIn, kParamFreezeIn };
static const _NT_parameterPage page10_inputs = { "INPUTS", 2, page10_params };
static const uint8_t page11_params[] = { kParamPitchOut, kParamGateOut };
static const _NT_parameterPage page11_outputs = { "OUTPUTS", 2, page11_params };

static const _NT_parameterPages pages = {
    11,
    { &page1_system, &page2_pattern, &page3_harmony, &page4_rhythm, &page5_dynamics, &page6_pitch, &page7_chaos, &page8_song_edit, &page9_song_perform, &page10_inputs, &page11_outputs }
};
```

---
*Date: 2025-10-16*

## Standard Operating Procedures

1.  **Activity Monitoring:** I will monitor my activity (e.g., file reads, edits). If you indicate that I am running slow or that you need to restart the environment, I will make a note of my recent activity levels and suggest breaking down tasks into smaller, more manageable chunks to improve performance and stability.
