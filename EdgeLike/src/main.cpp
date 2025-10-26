#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <new>

// Simple RNG for noise
static uint32_t rngState = 0x12345678u;
static inline float frand() {
    rngState = 1664525u * rngState + 1013904223u;
    return ((rngState >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f; // ~[-1,1)
}

// Pink noise (Paul Kellet one-pole approx)
struct Pink {
    float b0=0, b1=0, b2=0;
    float process(float white) {
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        float y = b0 + b1 + b2 + white * 0.1848f;
        return 0.05f * y; // scale
    }
};

// PolyBLEP helper
static inline float polyblep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Simple DPW triangle from saw
struct TriDPW {
    float z = 0.0f;
    float process(float saw) {
        float x = saw - 0.0f;
        float y = x * x;
        float h = y - z;
        z = y;
        return 2.0f * h; // scale to ~[-1,1]
    }
};

struct Osc {
    float phase = 0.0f;
    float lastPhase = 0.0f;
    float triMix = 0.5f; // 0=pulse,1=tri
    float pulseWidth = 0.5f;
    TriDPW tri;
    float process(float freq, float fm, float syncIn, bool syncEnable) {
        float fs = NT_globals.sampleRate;
        float inc = freq / fs + fm; // small linear FM term
        if (inc < 0) inc = 0; if (inc > 0.5f) inc = 0.5f; // guard
        lastPhase = phase;
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        // hard sync: if syncEnable and syncIn crossed 0, reset
        if (syncEnable && syncIn < 0.0f && (syncIn + inc) >= 0.0f) {
            phase = 0.0f;
        }
        float t = phase;
        float dt = inc;
        // saw (-1..1)
        float saw = 2.0f * t - 1.0f;
        // pulse with polyBLEP correction
        float s = t;
        float pulse = (s < pulseWidth) ? 1.0f : -1.0f;
        pulse += polyblep(s, dt);
        float s2 = s - pulseWidth;
        if (s2 < 0) s2 += 1.0f;
        pulse -= polyblep(s2, dt);
        // tri via DPW from saw
        float triSig = tri.process(saw);
        // shape mix
        return triMix * triSig + (1.0f - triMix) * pulse;
    }
};

// 4x one-pole cascade as a placeholder for ladder (fast, musical). Mode: 0=LP,1=HP
struct Cascade4Pole {
    float z1=0, z2=0, z3=0, z4=0;
    float cutoff=1000.0f, resonance=0.0f; // Hz, 0..~1
    int mode = 0; // 0 LP, 1 HP
    float drive = 0.0f;
    inline float sat(float x) const { return x * (1.0f + 0.5f * x * x * drive); }
    void set(float c, float r, int m, float d) { cutoff=c; resonance=r; mode=m; drive=d; }
    float process(float in) {
        float fs = NT_globals.sampleRate;
        // TPT one-pole
        float g = std::tan(3.14159265f * cutoff / fs);
        if (!std::isfinite(g)) g = 1.0f;
        float a = g / (1.0f + g);
        float x = sat(in);
        // simple resonance feedback on input for LP; for HP invert at end
        x -= resonance * z4;
        z1 += a * (x - z1);
        z2 += a * (z1 - z2);
        z3 += a * (z2 - z3);
        z4 += a * (z3 - z4);
        float lp = z4;
        float out = (mode == 0) ? lp : (x - lp); // crude HP via spectral inversion
        return out;
    }
};

struct DecayEnv {
    float v = 0.0f;
    float coeff = 0.999f;
    void setDecay(float seconds) {
        float fs = NT_globals.sampleRate;
        if (seconds < 0.001f) seconds = 0.001f;
        coeff = std::exp(-1.0f / (seconds * fs));
    }
    void trigger(float level=1.0f) { v = level; }
    float process() { v *= coeff; return v; }
};

// --- Algorithm ---
struct EdgeLike : public _NT_algorithm {
    // Oscillators
    Osc vco1, vco2;
    float baseF1 = 110.0f, baseF2 = 110.0f; // Hz
    float fmDepth = 0.0f; // small linear term
    int syncMode = 0; // 0 off, 1 2->1, 2 1->2

    // Noise
    Pink pink;
    float noiseColor = 0.0f; // 0 white .. 1 pink
    float noiseLevel = 0.0f;

    // Filter
    Cascade4Pole filt;
    float cutoff = 2000.0f;
    float resonance = 0.2f;
    int filterMode = 0; // 0 LP, 1 HP

    // Envelopes
    DecayEnv envPitch, envVCF, envVCA;
    float amtPitch = 0.0f; // semitones
    float amtVCF = 0.0f;   // Hz or normalized mapping

    // IO cache
    int trigInIdx = 0;
    int audioOutIdx = 0;

    // State
    float trigPrev = 0.0f;

        // ...existing code...

    // Scrooge auxiliary filters (simple 1-pole LP/HP)
    float bdLPz = 0.0f;
    float hhLPz = 0.0f;

    // Scrooge chaotic wavetable state (deterministic per pitch)
    uint32_t chaosSeed = 0u;
    float chaosTable[256];
    float chaosIndex = 0.0f;
    float chaosBaseStep = 2.0f;
};

// --- Parameters ---
enum {
    // OSC
    pVoiceModel, pVCO1_Tune, pVCO2_Tune, pTriMix1, pTriMix2, pPW1, pPW2, pFMDepth, pSyncMode,
    // NOISE
    pNoiseColor, pNoiseLevel,
    // FILTER
    pFilterMode, pCutoff, pResonance, pDrive, pEnvVCFAmt,
    // ENVS
    pPitchDec, pPitchAmt, pVCFDec, pVCA_Dec,
    // IO
    pTrigIn, pAudioOut,
    kNumParams
};

static const char* const syncStrings[] = { "Off", "2->1", "1->2", NULL };
static const char* const voiceStrings[] = { "EDGE", "SCROOGE", "DFAM", NULL };
static const char* const modeStrings[] = { "LP", "HP", NULL };

static const _NT_parameter params[kNumParams] = {
    // OSC
    { .name="Voice Model", .min=0, .max=2, .def=0, .unit=kNT_unitEnum, .scaling=0, .enumStrings=voiceStrings },
    { .name="VCO1 Tune", .min=-24, .max=24, .def=0 },
    { .name="VCO2 Tune", .min=-24, .max=24, .def=0 },
    { .name="VCO1 TriMix", .min=0, .max=100, .def=50 },
    { .name="VCO2 TriMix", .min=0, .max=100, .def=50 },
    { .name="VCO1 PW", .min=5, .max=95, .def=50 },
    { .name="VCO2 PW", .min=5, .max=95, .def=50 },
    { .name="FM Depth", .min=0, .max=100, .def=0 },
    { .name="Sync", .min=0, .max=2, .def=0, .enumStrings=syncStrings },
    // NOISE
    { .name="Noise Color", .min=0, .max=100, .def=0 },
    { .name="Noise Level", .min=0, .max=100, .def=0 },
    // FILTER
    { .name="Filter Mode", .min=0, .max=1, .def=0, .enumStrings=modeStrings },
    { .name="Cutoff", .min=20, .max=12000, .def=2000 },
    { .name="Resonance", .min=0, .max=95, .def=20 },
    { .name="Drive", .min=0, .max=100, .def=0 },
    { .name="Env->VCF", .min=-100, .max=100, .def=30 },
    // ENVS
    { .name="Pitch Decay", .min=1, .max=2000, .def=80 },
    { .name="Pitch Amt", .min=-2400, .max=2400, .def=600 }, // cents
    { .name="VCF Decay", .min=1, .max=2000, .def=150 },
    { .name="VCA Decay", .min=1, .max=2000, .def=120 },
    // IO (0=none)
    NT_PARAMETER_CV_INPUT("Trig In", 0, 0)
    NT_PARAMETER_CV_OUTPUT("Audio Out", 0, 0)
};

static const uint8_t pgOSC[]   = { pVoiceModel,pVCO1_Tune,pVCO2_Tune,pTriMix1,pTriMix2,pPW1,pPW2,pFMDepth,pSyncMode };
static const uint8_t pgNOISE[] = { pNoiseColor,pNoiseLevel };
static const uint8_t pgFILT[]  = { pFilterMode,pCutoff,pResonance,pDrive,pEnvVCFAmt };
static const uint8_t pgENVS[]  = { pPitchDec,pPitchAmt,pVCFDec,pVCA_Dec };
static const uint8_t pgIO[]    = { pTrigIn,pAudioOut };

static const _NT_parameterPage pageArr[] = {
    { "OSC",    (uint8_t)ARRAY_SIZE(pgOSC),   pgOSC },
    { "NOISE",  (uint8_t)ARRAY_SIZE(pgNOISE), pgNOISE },
    { "FILTER", (uint8_t)ARRAY_SIZE(pgFILT),  pgFILT },
    { "ENVS",   (uint8_t)ARRAY_SIZE(pgENVS),  pgENVS },
    { "IO",     (uint8_t)ARRAY_SIZE(pgIO),    pgIO },
};

static const _NT_parameterPages pages = { .numPages=(uint8_t)ARRAY_SIZE(pageArr), .pages=pageArr };

// --- Core API ---
void calculateRequirements(_NT_algorithmRequirements& r, const int32_t*) {
    r.numParameters = kNumParams;
    r.sram = sizeof(EdgeLike);
    r.dtc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    EdgeLike* a = reinterpret_cast<EdgeLike*>(ptrs.sram);
    ::new (static_cast<void*>(a)) EdgeLike;
    a->parameters = params;
    a->parameterPages = &pages;
    return a;
}

void parameterChanged(_NT_algorithm* self, int p) {
    EdgeLike* a = (EdgeLike*)self;
    switch (p) {
        case pTrigIn:   a->trigInIdx = a->v[pTrigIn]; break;
        case pAudioOut: a->audioOutIdx = a->v[pAudioOut]; break;
        default: break;
    }
}

static inline float midiNoteToHz(float semi) {
    return 440.0f * std::pow(2.0f, (semi - 69.0f) / 12.0f);
}

void step(_NT_algorithm* self, float* bus, int nBy4) {
    EdgeLike* a = (EdgeLike*)self;
    const int n = nBy4 * 4;
    float* trig = (a->trigInIdx > 0) ? bus + (a->trigInIdx - 1) * n : nullptr;
    float* out  = (a->audioOutIdx > 0) ? bus + (a->audioOutIdx - 1) * n : nullptr;
    if (!out) return;

    // Read parameters per block
    float triMix1 = a->v[pTriMix1] * 0.01f; a->vco1.triMix = triMix1;
    float triMix2 = a->v[pTriMix2] * 0.01f; a->vco2.triMix = triMix2;
    a->vco1.pulseWidth = a->v[pPW1] * 0.01f;
    a->vco2.pulseWidth = a->v[pPW2] * 0.01f;
    a->fmDepth = a->v[pFMDepth] * 0.0005f; // small
    a->syncMode = a->v[pSyncMode];
    int voiceModel = a->v[pVoiceModel]; // 0 EDGE,1 SCROOGE,2 DFAM
    a->noiseColor = a->v[pNoiseColor] * 0.01f;
    a->noiseLevel = a->v[pNoiseLevel] * 0.01f;
    a->cutoff = (float)a->v[pCutoff];
    a->resonance = a->v[pResonance] * 0.01f;
    int mode = a->v[pFilterMode];
    float drive = a->v[pDrive] * 0.01f;
    a->amtVCF = a->v[pEnvVCFAmt] * 0.01f * 3000.0f; // map to Hz span
    a->envPitch.setDecay(a->v[pPitchDec] * 0.001f);
    a->envVCF.setDecay(a->v[pVCFDec]  * 0.001f);
    a->envVCA.setDecay(a->v[pVCA_Dec] * 0.001f);
    a->amtPitch = a->v[pPitchAmt] * 0.01f; // cents -> semitones

    float f1Semi = 36.0f + a->v[pVCO1_Tune]; // C2-ish
    float f2Semi = 36.0f + a->v[pVCO2_Tune];
    a->baseF1 = midiNoteToHz(f1Semi);
    a->baseF2 = midiNoteToHz(f2Semi);

    a->filt.set(a->cutoff, a->resonance, mode, drive);

    for (int i = 0; i < n; ++i) {
        float tr = trig ? trig[i] : 0.0f;
        if (tr >= 1.0f && a->trigPrev < 1.0f) {
            a->envPitch.trigger(1.0f);
            a->envVCF.trigger(1.0f);
            a->envVCA.trigger(1.0f);
        }
        a->trigPrev = tr;

        float ePitch = a->envPitch.process();
        float eVCF   = a->envVCF.process();
        float eVCA   = a->envVCA.process();

        float pitchSemiMod = a->amtPitch * ePitch;
        float f1 = a->baseF1 * std::pow(2.0f, pitchSemiMod / 12.0f);
        float f2 = a->baseF2;

        float w = frand();
        float p = a->pink.process(w);
        float noise = (1.0f - a->noiseColor) * w + a->noiseColor * p;
        if (voiceModel == 2) {
            a->vco1.triMix = 0.9f; a->vco2.triMix = 0.9f;
        }
        float v1 = a->vco1.process(f1, a->fmDepth, 0.0f, false);
        float v2 = a->vco2.process(f2, 0.0f, 0.0f, false);

        float mix = 0.5f * v1 + 0.5f * v2 + a->noiseLevel * noise;
        int mdlMode = (voiceModel==2) ? 0 : mode;
        float cutoffNow = a->cutoff + a->amtVCF * eVCF;
        if (cutoffNow < 20.0f) cutoffNow = 20.0f; if (cutoffNow > 12000.0f) cutoffNow = 12000.0f;
        a->filt.set(cutoffNow, a->resonance, mdlMode, drive);
        float fOut = a->filt.process(mix);
        float vca = std::pow(eVCA, 1.5f);
        float y = fOut * vca;
        y = y / (1.0f + 0.5f * y * y);
        out[i] = y;
    }
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('E','D','G','E'),
    .name = "EdgeLike",
    .description = "Edge-inspired percussive synth voice",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagInstrument,
    .midiSysEx = NULL
};

uintptr_t pluginEntry(_NT_selector s, uint32_t d) {
    switch (s) {
        case kNT_selector_version: return kNT_apiVersionCurrent;
        case kNT_selector_numFactories: return 1;
        case kNT_selector_factoryInfo: return (uintptr_t)((d==0)?&factory:NULL);
    }
    return 0;
}
