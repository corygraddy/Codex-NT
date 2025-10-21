# C++ Lesson: Composition Matrix

This document breaks down the C++ code for the Composition Matrix, a music generation algorithm for the Disting NT modular synthesizer. We'll go through it section by section, explaining the concepts for someone learning C++.

## 1. Includes

Every C++ program starts by including "header files." These files give us access to pre-written code and definitions that we need. It's like telling the program, "I'm going to use tools from these toolboxes."

```cpp
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cstdint>
#include <new>
#include <cstdio>
```

- **`<distingnt/api.h>`**: This is the most important one for us. It's the main "toolbox" for creating Disting NT algorithms. It defines all the basic building blocks we'll be using.
- **`<distingnt/serialisation.h>`**: This header provides tools for saving and loading data, which is how the module remembers settings when you save a preset.
- **`<cstdint>`**: This standard C++ header gives us access to integer types with specific sizes, like `uint32_t` (a 32-bit unsigned integer). This is useful for ensuring our code works the same way on different computer systems.
- **`<new>`**: This header is related to memory management. We use it for a special kind of object creation called "placement new."
- **`<cstdio>`**: This provides standard C input/output functions. We're not using it directly in this simplified version, but it's often included for debugging or string formatting.

---

## 2. Helper Function: `uint32_to_hex_string`

Sometimes, you need a specific tool that isn't in a standard toolbox. In C++, you can write your own "functions" to do these jobs. This function takes a 32-bit number and converts it into a hexadecimal string (like `0xDEADBEEF`).

```cpp
// --- Helper Function ---
void uint32_to_hex_string(uint32_t value, char* out_buffer) {
    const char* hex_chars = "0123456789ABCDEF";
    out_buffer[0] = '0';
    out_buffer[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        out_buffer[9 - i] = hex_chars[value & 0x0F];
        value >>= 4;
    }
    out_buffer[10] = '\0';
}
```

- **`void uint32_to_hex_string(...)`**: This defines a function that doesn't return any value (`void`).
- **`uint32_t value, char* out_buffer`**: These are the function's "parameters" or inputs. It takes an integer `value` to convert and a `out_buffer` (a character array) to store the resulting string.
- **`const char* hex_chars = ...`**: We create a constant array of characters that holds all the possible hexadecimal digits.
- **`for (int i = 0; i < 8; ++i)`**: This is a `for` loop. It runs 8 times to process each of the 8 hexadecimal digits in a 32-bit number.
- **`value & 0x0F`**: This is a "bitwise AND" operation. It's a clever way to get the value of the last 4 bits of the number, which corresponds to one hexadecimal digit.
- **`value >>= 4`**: This is a "bitwise right shift." It shifts the bits of the number four places to the right, effectively getting it ready to read the *next* hexadecimal digit in the next loop iteration.
- **`out_buffer[10] = '\0';`**: We add a special "null terminator" character at the end of the string. This is how C-style strings mark their end.

---

## 3. Data Structures

In C++, we use `structs` (structures) to group related variables together. It's like creating our own custom data type. Here, we define two structs that are the heart of our algorithm.

```cpp
// --- Data To Continue (DTC) Struct ---
struct CompositionMatrixAlgorithm_DTC {
    uint32_t savedShiftRegister = 0;
};

// --- Main Algorithm Struct ---
struct CompositionMatrixAlgorithm : public _NT_algorithm {
    CompositionMatrixAlgorithm_DTC* dtc;
    uint32_t shiftRegister = 0xDEADBEEF;
    float pitchCv = 0.0f;
    int gateCounter = 0;
    float lastClockValue = 0.0f;

    CompositionMatrixAlgorithm(CompositionMatrixAlgorithm_DTC* dtc_ptr) : dtc(dtc_ptr) {}
};
```

- **`CompositionMatrixAlgorithm_DTC`**: The "DTC" stands for "Data To Continue." This struct holds data that needs to be **saved with a preset**. In this case, it's the `savedShiftRegister` value. This data is stored in a special, persistent memory area on the Disting.

- **`CompositionMatrixAlgorithm`**: This is the main struct for our algorithm. It holds the "live" data that the algorithm uses while it's running, but doesn't need to be saved.
    - **`: public _NT_algorithm`**: This is called **inheritance**. It means our struct gets all the features and variables from a basic `_NT_algorithm` struct defined in the API, and we can add our own on top.
    - **`dtc`**: A pointer to our DTC struct. This is how the main algorithm accesses its saved data.
    - **`shiftRegister`**: The core of our music generator. It's a number that we manipulate to create patterns.
    - **`pitchCv`, `gateCounter`, `lastClockValue`**: Variables that control the musical output (pitch, gate length) and track the incoming clock signal.
    - **`CompositionMatrixAlgorithm(...)`**: This is a **constructor**. It's a special function that runs when a `CompositionMatrixAlgorithm` object is created. Here, it takes a pointer to the DTC data and saves it.

---

## 4. Parameters

Parameters are the knobs and settings that the user can change on the Disting NT's screen. We define them in two parts: an `enum` and an array of `_NT_parameter` structs.

```cpp
// --- Parameters ---
enum {
    kParamGlobalKey,
    kParamGlobalScale,
    // ... and so on
    kNumParameters
};

static const _NT_parameter parameters[kNumParameters] = {
    { .name = "Global Key", .min = 0, .max = 11, .def = 0, .enumStrings = globalKeyStrings },
    { .name = "Global Scale", .min = 0, .max = 6, .def = 0, .enumStrings = globalScaleStrings },
    // ... and so on
};
```

- **`enum`**: An enumeration. It's a simple way to create a list of named integer constants. Here, we're just giving easy-to-read names to the index of each parameter (e.g., `kParamGlobalKey` is `0`, `kParamGlobalScale` is `1`, etc.). `kNumParameters` will automatically be the total count.

- **`static const _NT_parameter parameters[...]`**: This is an array of `_NT_parameter` structs. Each element in the array defines one parameter.
    - **`static`**: This means the variable belongs to the file itself, not to any specific instance of our algorithm.
    - **`const`**: This means the array cannot be changed after it's created.
    - **`{ .name = "...", .min = ..., .max = ... }`**: This is called **designated initialization**. It's a clear way to set the values for each member of the struct (its name, minimum value, maximum value, default value, etc.).

---

## 5. Parameter Pages

To keep the UI organized, we group our parameters into pages. This is also done with structs and arrays.

```cpp
static const uint8_t page1_params[] = { kParamGlobalKey, kParamGlobalScale, kParamPolyphonyMode, kParamNumSupportVoices };

static const _NT_parameterPage page_array[] = {
    { "SYSTEM", 4, page1_params },
    // ... other pages
};

static const _NT_parameterPages pages = {
    .numPages = ARRAY_SIZE(page_array),
    .pages = page_array
};
```

1.  First, we create small arrays of `uint8_t` (8-bit unsigned integers) that hold the `enum` values for the parameters we want on each page.
2.  Then, we create an array of `_NT_parameterPage` structs. Each struct defines a page, giving it a name and pointing to the array of parameters for that page.
3.  Finally, we create a single `_NT_parameterPages` struct that tells the Disting how many pages there are in total and where to find the array of pages.

---

## 6. Scale Data

This is a simple array that holds the musical data for our algorithm. It's a C Major Pentatonic scale, represented as voltage values that the Disting will output to control an oscillator.

```cpp
// --- Hard-coded Scale (C Major Pentatonic) ---
const float cMajorPentatonicVolts[] = {
    0.0f, 0.1667f, 0.3333f, 0.5833f, 0.75f,
    1.0f, 1.1667f, 1.3333f, 1.5833f, 1.75f
};
```

- **`const float cMajorPentatonicVolts[]`**: We declare an array of floating-point numbers (`float`). It's `const` because this scale data will never change while the program is running.

---

## 7. Core API Functions: `calculateRequirements` & `construct`

These two functions are mandatory. They handle the setup and memory management for our algorithm.

```cpp
void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = kNumParameters;
    req.sram = sizeof(CompositionMatrixAlgorithm);
    req.dtc = sizeof(CompositionMatrixAlgorithm_DTC);
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements& req, const int32_t* specifications) {
    CompositionMatrixAlgorithm_DTC* dtc = new (ptrs.dtc) CompositionMatrixAlgorithm_DTC();
    CompositionMatrixAlgorithm* alg = new (ptrs.sram) CompositionMatrixAlgorithm(dtc);
    alg->parameters = parameters;
    alg->parameterPages = &pages;
    return alg;
}
```

- **`calculateRequirements`**: Before creating our algorithm, the Disting host calls this function to ask, "How much memory do you need?" We tell it:
    - The total number of parameters we defined (`kNumParameters`).
    - How much **SRAM** we need (the size of our main `CompositionMatrixAlgorithm` struct).
    - How much **DTC** memory we need (the size of our `CompositionMatrixAlgorithm_DTC` struct).

- **`construct`**: After allocating the memory, the host calls this function to actually build our algorithm object.
    - **`new (ptrs.dtc) CompositionMatrixAlgorithm_DTC()`**: This is **placement new**. Instead of asking the system for new memory, it constructs our `DTC` object in the exact memory location (`ptrs.dtc`) that the host already gave us.
    - We do the same for our main `alg` object in the `sram` memory.
    - **`alg->parameters = parameters;`**: We set the `parameters` and `parameterPages` pointers in our algorithm struct to point to the arrays we defined earlier.
    - **`return alg;`**: We return a pointer to the newly created algorithm object.

---

## 8. The DSP Loop: `step`

This is where the magic happens! The `step` function is the digital signal processing (DSP) loop. The Disting host calls this function thousands of times per second. Its job is to read inputs, process data, and write to outputs.

```cpp
void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    // ... code ...
}
```

- **`CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;`**: The `self` pointer is a generic algorithm pointer. We **cast** it to our specific `CompositionMatrixAlgorithm` type so we can access our custom variables like `shiftRegister`.
- **`busFrames`**: This is a large array containing all the audio and CV data for all 28 of the Disting's inputs and outputs.
- **Clock Detection**: The code checks for a "rising edge" on the clock input (`clockIn[i] >= 1.0f && pThis->lastClockValue < 1.0f`). This means it only does something on the exact moment a clock pulse arrives.
- **Music Generation**: When a clock pulse is detected:
    - It updates the `shiftRegister` using bitwise logic. This is a simple way to generate a pseudo-random, repeating sequence.
    - It uses the `shiftRegister`'s value to pick a note from the `cMajorPentatonicVolts` array.
    - It sets the `gateCounter` to create a short gate pulse, making the note sound.
- **Output**: In each frame of the loop, it sets the `pitchOut` and `gateOut` values in the `busFrames` buffer.

---

## 9. Custom User Interface

These functions allow us to draw a custom screen and react to button presses, overriding the default parameter view.

```cpp
bool draw(_NT_algorithm* self) { /* ... */ }
uint32_t hasCustomUi(_NT_algorithm* self) { /* ... */ }
void customUi(_NT_algorithm* self, const _NT_uiData& data) { /* ... */ }
```

- **`hasCustomUi`**: This function tells the host which controls we want to take over. Here, we return `kNT_potButtonC` to say we want to handle the press of the center pot's button.
- **`draw`**: This function is called whenever the screen needs to be redrawn. We use API functions like `NT_drawText()` to display our information. We use our `uint32_to_hex_string` helper to show the `live` and `saved` shift register values.
- **`customUi`**: This function is called when the user interacts with a control we've taken over. We check if the center button was pressed (`data.controls & kNT_potButtonC`). If it was, we copy the live `shiftRegister` value into the `savedShiftRegister` in our DTC struct, so it will be saved with the preset.

---

## 10. Serialization: Saving and Loading

These functions are the correct way to handle saving and loading custom data with presets. They get called by the host when the user saves or loads a preset.

```cpp
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    CompositionMatrixAlgorithm* pThis = (CompositionMatrixAlgorithm*)self;
    stream.addMemberName("savedShiftRegister");
    stream.addNumber((int)pThis->dtc->savedShiftRegister);
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    // ... code to read the value back ...
    return true;
}
```

- **`serialise`**: When saving, this function is called. We use the `stream` object to write our data to the preset file, which is in a format called JSON. We add a member named "savedShiftRegister" and write the value.
- **`deserialise`**: When loading, this function is called. We use the `parse` object to read the data back from the JSON file. We look for a member named "savedShiftRegister" and, if found, read its value and store it back in our `dtc->savedShiftRegister` variable.

---

## 11. Plugin Registration: The Factory

This is the final piece of the puzzle. We create one last `static const` struct, an `_NT_factory`. This struct is like a business card for our algorithm.

```cpp
static const _NT_factory factory = {
    .guid = NT_MULTICHAR('C', 'M', 'P', 'X'),
    .name = "CompositionMatrix",
    .description = "Generative Harmony & Rhythm Engine.",
    // ...
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .step = step,
    .draw = draw,
    // ... and so on
};
```

It contains all the information about our algorithm: its unique ID (`guid`), its name, and, most importantly, **function pointers** to all the key functions we wrote (`construct`, `step`, `draw`, `serialise`, etc.). This is how we tell the Disting host where to find the functions that make our algorithm work.

---

## 12. Plugin Entry Point

This is the function that the Disting NT host calls when it first loads our plugin file. It's the very first point of contact.

```cpp
uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}
```

The host calls this function with different `selector` values to ask questions:
- **`kNT_selector_version`**: "What API version were you built for?"
- **`kNT_selector_numFactories`**: "How many different algorithms are in this file?" (We only have one).
- **`kNT_selector_factoryInfo`**: "Give me the 'business card' (the `_NT_factory` struct) for algorithm number `data`." We return a pointer to our `factory` struct.
