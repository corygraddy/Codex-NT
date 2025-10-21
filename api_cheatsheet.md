# Disting NT API Cheatsheet

This is a summary of the Disting NT C++ API, based on the header files and examples.

## 1. Plugin Entry Point (`pluginEntry`)

The entry point for your plugin. It's a function that the Disting NT host calls to get information about your plugin.

```c++
uintptr_t pluginEntry(_NT_selector selector, uint32_t data);
```

- **`kNT_selector_version`**: Return `kNT_apiVersionCurrent`.
- **`kNT_selector_numFactories`**: Return the number of algorithm factories in your plugin.
- **`kNT_selector_factoryInfo`**: Return a pointer to an `_NT_factory` struct.

## 2. Algorithm Factory (`_NT_factory`)

Defines a single algorithm. A plugin can have multiple factories.

```c++
struct _NT_factory {
    uint32_t guid; // 4-char unique ID using NT_MULTICHAR()
    const char* name;
    const char* description;
    // ... function pointers ...
};
```

### Key Function Pointers:

- **`calculateRequirements`**: Calculate memory needs for an algorithm instance.
- **`construct`**: Create an instance of your algorithm.
- **`parameterChanged`**: Called when a parameter's value changes.
- **`step`**: The main DSP processing function.
- **`draw`**: For drawing a custom UI to the screen.
- **`serialise` / `deserialise`**: For saving and loading custom data with presets.
- **`midiMessage`**: For handling MIDI messages.

## 3. Algorithm Instance (`_NT_algorithm`)

This is the main struct for your algorithm. You'll typically create your own struct that inherits from this.

```c++
struct MyAlgorithm : public _NT_algorithm {
    // Your algorithm's state data here
    float myValue;
};
```

- **`parameters`**: Pointer to your array of `_NT_parameter`s.
- **`parameterPages`**: Pointer to your `_NT_parameterPages` struct.
- **`v`**: Pointer to an array of your algorithm's parameter values.

## 4. Memory Management

1.  **`calculateRequirements`**: Tell the host how much memory you need.
    -   `req.sram`: For your main algorithm struct.
    -   `req.dram`: For large, non-critical data.
    -   `req.dtc`: For performance-critical data.
2.  **`construct`**: The host gives you pointers to the allocated memory. Use placement `new` to create your algorithm instance in the `sram`.

```c++
// In calculateRequirements:
req.sram = sizeof(MyAlgorithm);

// In construct:
MyAlgorithm* alg = new (ptrs.sram) MyAlgorithm();
```

## 5. Parameters

- **`_NT_parameter`**: Defines a single parameter (name, min, max, default, unit, etc.).
- **`_NT_parameterPage`**: Groups parameters into pages for the UI.
- **`parameterChanged`**: A callback that fires when a parameter is changed. Use this to update cached values in your algorithm struct.

## 6. DSP (`step`)

The audio processing callback.

```c++
void step(_NT_algorithm* self, float* busFrames, int numFramesBy4);
```

- **`busFrames`**: A buffer containing all audio/CV busses.
- **`numFramesBy4`**: The number of frames to process is `numFramesBy4 * 4`.
- Get input/output bus indices from your parameter values (`self->v[kParamInput]`).

## 7. UI (`draw`)

Draw a custom UI on the screen.

```c++
bool draw(_NT_algorithm* self);
```

- Return `true` to hide the default parameter display.
- Use `NT_drawText()`, `NT_drawShapeI()`, `NT_drawShapeF()` to draw.
- `NT_screen` is a pointer to the screen buffer if you need direct pixel access.

## 8. Preset Saving & Loading (`serialise` / `deserialise`)

The correct way to save and load custom data with your presets.

- **`serialise(_NT_algorithm* self, _NT_jsonStream& stream)`**: Write your algorithm's state to the JSON stream.
- **`deserialise(_NT_algorithm* self, _NT_jsonParse& parse)`**: Read your state from the JSON parser.

```c++
// In serialise:
stream.addMemberName("myValue");
stream.addNumber(pThis->myValue);

// In deserialise:
if (parse.matchName("myValue")) {
    parse.number(pThis->myValue);
}
```

## 9. MIDI

- **`midiMessage`**: Handles channel voice messages (Note On/Off, CC, etc.).
- **`midiRealtime`**: Handles system realtime messages (Clock, Start, Stop).
- Use `NT_sendMidi3ByteMessage()`, etc. to send MIDI out.

## 10. Key Utility Functions

- **`NT_setParameterFromUi()`**: Set a parameter value.
- **`NT_intToString()` / `NT_floatToString()`**: Safe and efficient string conversion for display.
- **`NT_algorithmIndex()`**: Get the index of the current algorithm instance.
