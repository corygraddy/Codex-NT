# C++ Lesson: A Disting NT Plugin Template

This document breaks down a template for a Disting NT plugin. We'll explore how it's structured and what each part does, with explanations aimed at someone learning C++.

## 1. Includes

We start by including the necessary "header files." These files provide access to the core functionalities of the Disting NT API and standard C++ features.

```cpp
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <cstdio>
#include <new>
```

- **`<distingnt/api.h>`**: The main API header, providing all the basic building blocks for an algorithm.
- **`<distingnt/serialisation.h>`**: Provides the tools (`_NT_jsonStream`, `_NT_jsonParse`) for saving and loading custom data with presets.
- **`<cstdint>`**: A standard C++ header for using integer types with specific widths, like `uint32_t` (a 32-bit unsigned integer).
- **`<cstdio>`**: Provides standard C input/output functions. Not used in this template, but good to have for debugging.
- **`<new>`**: Required for "placement new," a special way of constructing objects in memory that the Disting NT API uses.

---

## 2. Data Structures

We use `structs` to group related variables. This template uses two: one for persistent data and one for the live algorithm state.

```cpp
// --- Data To Continue (DTC) Struct ---
struct MyPluginAlgorithm_DTC {
    uint32_t stepCounter = 0;
    int32_t lastButtonPressed = -1;
    uint32_t magicNumber = 0xDEADBEEF;
};

// --- Main Algorithm Struct ---
struct MyPluginAlgorithm : public _NT_algorithm {
    MyPluginAlgorithm_DTC* dtc; // Pointer to our persistent data

    MyPluginAlgorithm(MyPluginAlgorithm_DTC* dtc_ptr) : dtc(dtc_ptr) {}
};
```

- **`MyPluginAlgorithm_DTC`**: The "Data To Continue" struct holds data that must be saved with a preset. For this template, we've created some variables for debugging:
    - `stepCounter`: To see if our DSP loop is running.
    - `lastButtonPressed`: To check if button presses are detected.
    - `magicNumber`: A unique value to confirm that our data is being saved and loaded correctly.

- **`MyPluginAlgorithm`**: This is the main struct for our algorithm's live state.
    - **`: public _NT_algorithm`**: This is **inheritance**. Our struct gains all the properties of the base `_NT_algorithm`.
    - **`dtc`**: A pointer to our DTC struct, linking the live algorithm to its saved data.
    - **`MyPluginAlgorithm(...)`**: This is the **constructor**. It runs when the object is created and sets up the `dtc` pointer.

---

## 3. Parameters

Parameters are the user-configurable settings. We define them with an `enum` for easy access and an array of `_NT_parameter` structs.

```cpp
enum {
    kParamKnob1, kParamKnob2, kParamKnob3,
    kParamButton1, kParamButton2, kParamButton3,
    kParamCVInput1, kParamAudioInput1,
    kParamCVOutput1, kParamAudioOutput1,
    kNumParameters
};

static const _NT_parameter parameters[kNumParameters] = {
    { .name = "Knob 1", .min = 0, .max = 99, .def = 0 },
    { .name = "Knob 2", .min = 0, .max = 99, .def = 50 },
    { .name = "Knob 3", .min = 0, .max = 99, .def = 99 },
    { .name = "Button 1", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    // ... other parameters
};
```

- **`enum`**: An enumeration that creates named integer constants for our parameter indices (e.g., `kParamKnob1` is `0`, `kParamKnob2` is `1`).
- **`static const _NT_parameter parameters[]`**: A constant array where each element is a struct that defines a single parameter: its name, min/max values, and default value.
- **`.scaling = kNT_scalingNone`**: This is used for the buttons. It makes them behave like momentary triggers rather than knobs, so they don't require the user to "confirm" a new value after turning a pot.

---

## 4. Parameter Pages

To keep the UI tidy, we group parameters into pages.

```cpp
static const uint8_t page1_params[] = { kParamKnob1, /*...*/ };
static const uint8_t page2_params[] = { kParamCVInput1, /*...*/ };
// ...

static const _NT_parameterPage page_array[] = {
    { "MAIN", ARRAY_SIZE(page1_params), page1_params },
    { "INPUTS", ARRAY_SIZE(page2_params), page2_params },
    { "OUTPUTS", ARRAY_SIZE(page3_params), page3_params },
};

static const _NT_parameterPages pages = {
    .numPages = ARRAY_SIZE(page_array),
    .pages = page_array
};
```

This structure is straightforward: we define arrays of parameter indices for each page, then create an array of `_NT_parameterPage` structs to define the pages themselves, and finally wrap it all in a single `_NT_parameterPages` struct.

---

## 5. Core API Functions: `calculateRequirements` & `construct`

These two functions are essential for memory management.

```cpp
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(MyPluginAlgorithm);
    req.dtc = sizeof(MyPluginAlgorithm_DTC);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    MyPluginAlgorithm_DTC* dtc = new (ptrs.dtc) MyPluginAlgorithm_DTC();
    MyPluginAlgorithm* alg = new (ptrs.sram) MyPluginAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}
```

- **`calculateRequirements`**: The host calls this to ask how much memory the algorithm needs. We provide the size of our main struct (`sram`) and our persistent data struct (`dtc`).
- **`construct`**: The host calls this to build the algorithm. We use **placement new** to create our `DTC` and `MyPluginAlgorithm` objects in the specific memory locations (`ptrs.dtc`, `ptrs.sram`) provided by the host.

---

## 6. `parameterChanged` Function

This function is a callback that the host executes whenever the user changes a parameter.

```cpp
void parameterChanged(_NT_algorithm* self, int p) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;

    if (p >= kParamButton1 && p <= kParamButton3) {
        if (pThis->v[p] == 1) {
            pThis->dtc->lastButtonPressed = p;
            NT_setParameterFromUi(NT_algorithmIndex(self), p + NT_parameterOffset(), 0);
        }
    }
}
```

- We check if the changed parameter `p` is one of our buttons.
- If a button was pressed (`pThis->v[p] == 1`), we record its index in our debug data.
- **`NT_setParameterFromUi(...)`**: We immediately set the button's parameter value back to `0`. This makes the button a **momentary trigger**; it's only `1` for a single moment before being reset.

---

## 7. The DSP Loop: `step`

This is the main audio processing function, called thousands of times per second.

```cpp
void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;
    pThis->dtc->stepCounter++;
    // ... DSP logic ...
}
```

- **`pThis->dtc->stepCounter++;`**: For debugging, we simply increment a counter in our persistent data struct each time `step` is called. When we save a preset, we can look at this value to confirm the DSP loop is running.
- The rest of the function gets pointers to the input/output buffers and passes audio through, serving as a placeholder for your own DSP code.

---

## 8. Serialization (for Debugging)

These functions handle saving and loading our debug data to the preset file.

```cpp
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    MyPluginAlgorithm* pThis = (MyPluginAlgorithm*)self;
    
    stream.addMemberName("debug_info");
    stream.openObject();
    stream.addMemberName("magicNumber");
    stream.addNumber((int)pThis->dtc->magicNumber);
    // ... more data ...
    stream.closeObject();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    // ... code to read the data back ...
}
```

- **`serialise`**: When saving a preset, this function is called. We use the `stream` object to write a JSON object called `"debug_info"` containing our `magicNumber`, `stepCounter`, and `lastButtonPressed`.
- **`deserialise`**: When loading a preset, this function is called to read the `"debug_info"` object and restore the values to our `DTC` struct.

---

## 9. Plugin Registration and Entry Point

Finally, we register our plugin with the Disting host.

```cpp
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'P', 'L', 'T'),
    .name = "My Plugin",
    // ... function pointers ...
    .serialise = serialise,
    .deserialise = deserialise,
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    // ...
}
```

- **`_NT_factory`**: This `factory` struct is a "business card" for our plugin. It contains the plugin's name, a unique ID (`guid`), and, most importantly, **function pointers** to all the functions we've written (`construct`, `step`, `serialise`, etc.).
- **`pluginEntry`**: This is the very first function the host calls. It simply directs the host to our `factory` struct.
