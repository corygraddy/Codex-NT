#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

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

    // Sequencer state
    int seqNumSteps = 8;                 // 1..16
    int seqSelectedIndex = 1;            // 0 = step count pseudo-knob, then pairs (pitch,vel)
    int seqDirection = 0;                // 0=FWD,1=BWD,2=PINGPONG,3=ODD_EVEN
    int seqProbability = 100;            // 0..100 percent
    int seqPlayIdx = 0;                  // current step index [0..seqNumSteps-1]
    int seqPingDir = +1;                 // +1 forward, -1 backward
    bool seqOddPhase = true;             // for odd-even mode
    int seqPhasePos = 0;                 // position within phase
    int seqPitch[16];                    // 0..100 UI value
    int seqVel[16];                      // 0..100 UI value
    float seqPitchOffsetSemi = 0.0f;     // semitone offset from current step

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
    EdgeLike* a = new (ptrs.sram) EdgeLike();
    a->parameters = params;
    a->parameterPages = &pages;
    // Init envelopes
    a->envPitch.setDecay(0.08f);
    a->envVCF.setDecay(0.15f);
    a->envVCA.setDecay(0.12f);
    a->filt.set(2000.0f, 0.2f, 0, 0.0f);
    // Init sequencer arrays
    a->seqNumSteps = 8;
    a->seqSelectedIndex = 1;
    a->seqDirection = 0;
    a->seqProbability = 100;
    a->seqPlayIdx = 0;
    a->seqPingDir = +1;
    a->seqOddPhase = true;
    a->seqPhasePos = 0;
    for (int i=0;i<16;++i) { a->seqPitch[i] = 50; a->seqVel[i] = 100; }
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
        // Trigger detect
        float tr = trig ? trig[i] : 0.0f;
        if (tr >= 1.0f && a->trigPrev < 1.0f) {
            // Advance sequencer step according to direction
            auto advanceFwd = [&](){ a->seqPlayIdx = (a->seqPlayIdx + 1) % a->seqNumSteps; };
            auto advanceBwd = [&](){ a->seqPlayIdx = (a->seqPlayIdx + a->seqNumSteps - 1) % a->seqNumSteps; };
            if (a->seqDirection == 0) {
                advanceFwd();
            } else if (a->seqDirection == 1) {
                advanceBwd();
            } else if (a->seqDirection == 2) {
                // ping-pong between [0..numSteps-1]
                if (a->seqPingDir > 0) {
                    if (a->seqPlayIdx >= a->seqNumSteps - 1) { a->seqPingDir = -1; a->seqPlayIdx = a->seqNumSteps - 2; }
                    else a->seqPlayIdx++;
                } else {
                    if (a->seqPlayIdx <= 0) { a->seqPingDir = +1; a->seqPlayIdx = 1; }
                    else a->seqPlayIdx--;
                }
            } else {
                // odd-even: iterate odds (1-based: 1,3,5..) then evens (2,4,...)
                int base = a->seqOddPhase ? 0 : 1; // index base (0-based index for step 1 is 0)
                a->seqPhasePos++;
                int next = base + 2 * a->seqPhasePos;
                if (next >= a->seqNumSteps) {
                    // flip phase and restart
                    a->seqOddPhase = !a->seqOddPhase;
                    a->seqPhasePos = 0;
                    base = a->seqOddPhase ? 0 : 1;
                    next = base;
                }
                a->seqPlayIdx = next;
            }

            // Probability gating
            float p = a->seqProbability * 0.01f;
            float r = (frand() * 0.5f + 0.5f); // ~[0,1)
            bool fire = (r <= p);
            if (fire) {
                // Compute per-step pitch offset: map 0..100 to -24..+24 semis
                int ui = a->seqPitch[a->seqPlayIdx];
                float semi = (ui - 50) * (48.0f / 100.0f);
                a->seqPitchOffsetSemi = semi;
                int vel = a->seqVel[a->seqPlayIdx];
                float level = vel * 0.01f;
                a->envPitch.trigger(1.0f);
                a->envVCF.trigger(1.0f);
                a->envVCA.trigger(level);

                // If SCROOGE: deterministically seed chaos by pitch to get repeatable timbre
                if (voiceModel == 1) {
                    uint32_t pitchKey = (uint32_t)ui & 0xFFu;
                    // Derive a seed from pitch value (Knuth constant)
                    a->chaosSeed = 0x9E3779B9u ^ (pitchKey * 2654435761u);
                    rngState = a->chaosSeed; // re-seed noise RNG so noise evolution is repeatable for this pitch
                    // Build chaotic table using logistic map (chaotic but deterministic)
                    float x = 0.111f + (pitchKey / 255.0f) * 0.777f; // in (0,1)
                    const float rlog = 3.98f; // chaos parameter
                    for (int k=0;k<256;++k) {
                        x = rlog * x * (1.0f - x);
                        a->chaosTable[k] = 2.0f * x - 1.0f; // [-1,1]
                    }
                    a->chaosIndex = 0.0f;
                    a->chaosBaseStep = 1.5f + 4.0f * (pitchKey / 255.0f); // faster for higher pitch knobs
                }
            }
        }
        a->trigPrev = tr;

        float ePitch = a->envPitch.process();
        float eVCF   = a->envVCF.process();
        float eVCA   = a->envVCA.process();

        // Instantaneous frequencies (semitone sweep)
        float pitchSemiMod = a->amtPitch * ePitch + a->seqPitchOffsetSemi; // semitones (env + seq)
        float f1 = a->baseF1 * std::pow(2.0f, pitchSemiMod / 12.0f);
        float f2 = a->baseF2;

        // Noise
        float w = frand();
        float p = a->pink.process(w);
        float noise = (1.0f - a->noiseColor) * w + a->noiseColor * p;
        // DFAM: prefer triangle, no sync, minimal FM
        if (voiceModel == 2) {
            a->vco1.triMix = 0.9f; a->vco2.triMix = 0.9f; fmFrom2 *= 0.2f;
        }
        float v1 = a->vco1.process(f1, (voiceModel==2)?fmFrom2:fmFrom2, (voiceModel==2)?0.0f:((a->syncMode==1)?syncSig2:0.0f), (voiceModel==2)?false:(a->syncMode==1));
        float v2 = a->vco2.process(f2, 0.0f, (voiceModel==2)?0.0f:((a->syncMode==2)?syncSig1:0.0f), (voiceModel==2)?false:(a->syncMode==2));

        // Noise
            // Advance chaos index with envelope-dependent step (simulates voltage sag)
            float cStep = a->chaosBaseStep * (0.25f + 0.75f * eVCA);
            a->chaosIndex += cStep;
            int cIdx = ((int)a->chaosIndex) & 255;
            float c = a->chaosTable[cIdx]; // [-1,1]
            // Use chaotic value to shape noise amplitude and gating (more dropouts as env decays)
            float gateThresh = 0.7f - 0.5f * eVCA; // threshold rises as VCA decays
            float gate = (c > gateThresh) ? 1.0f : 0.0f;
            float noiseShaped = noise * (0.6f + 0.4f * (0.5f * (c + 1.0f))) * gate;
        float w = frand();
        float p = a->pink.process(w);
            float bd = 0.6f * v1; // use VCO1 as body

            float bdCut = 200.0f + 1000.0f * eVCF + 150.0f * c; // chaotic wobble
            // SCROOGE-like: blend BD (low) and HH (high) layers by step pitch UI (0..100)
            float blend = 0.5f;
            if (a->seqNumSteps > 0) {
                int s = a->seqPlayIdx; if (s < 0) s = 0; if (s >= a->seqNumSteps) s = a->seqNumSteps - 1;
                blend = a->seqPitch[s] * 0.01f; // 0 = BD, 1 = HH
            }
            // BD layer: low freq thump with LP
            float bd = 0.6f * v1; // use VCO1 as body
            float hhCut = 6000.0f - 4000.0f * eVCF + 500.0f * c; if (hhCut < 1000.0f) hhCut = 1000.0f;
            float bdCut = 200.0f + 1000.0f * eVCF;
            float gbd = std::tan(3.14159265f * bdCut / NT_globals.sampleRate);
            a->hhLPz += ahh * (noiseShaped - a->hhLPz);
            float hh = (noiseShaped - a->hhLPz) * 0.8f + 0.2f * v2; // bright noise with a bit of VCO2
            bd = a->bdLPz;
            // add slight drive
            bd = bd / (1.0f + 0.8f * bd * bd);

            // HH layer: bright noise via HP (x - LP)
            float hhCut = 6000.0f - 4000.0f * eVCF; if (hhCut < 1000.0f) hhCut = 1000.0f;
            float ghh = std::tan(3.14159265f * hhCut / NT_globals.sampleRate);
            float ahh = ghh / (1.0f + ghh);
            a->hhLPz += ahh * (noise - a->hhLPz);
            float hh = (noise - a->hhLPz) * 0.8f + 0.2f * v2; // bright noise with a bit of VCO2

            float scMix = (1.0f - blend) * bd + blend * hh;
            float vca = std::pow(eVCA, 1.4f);
            float y = scMix * vca;
            y = y / (1.0f + 0.5f * y * y);
            out[i] = y;
        } else {
            // EDGE/DFAM path through main filter
            float mix = 0.5f * v1 + 0.5f * v2 + a->noiseLevel * noise;
            // DFAM prefers LP
            int mdlMode = (voiceModel==2) ? 0 : mode;
            // Filter cutoff mod
            float cutoffNow = a->cutoff + a->amtVCF * eVCF;
            if (cutoffNow < 20.0f) cutoffNow = 20.0f; if (cutoffNow > 12000.0f) cutoffNow = 12000.0f;
            a->filt.set(cutoffNow, a->resonance, mdlMode, drive);
            float fOut = a->filt.process(mix);
            // VCA
            float vca = std::pow(eVCA, 1.5f);
            float y = fOut * vca;
            y = y / (1.0f + 0.5f * y * y);
            out[i] = y;
        }
    }
}

// --- UI helpers ---
static inline float deg2rad(float d){ return d * 3.14159265f / 180.0f; }

static void drawKnob(int cx, int cy, int radius, int uiVal, bool grey, bool selected, int colourBase=12) {
    int col = grey ? 4 : (selected ? 15 : colourBase);
    NT_drawShapeI(kNT_circle, cx, cy, radius, 0, col);
    if (grey) return;
    float v = std::fmax(0.0f, std::fmin(100.0f, (float)uiVal)) / 100.0f;
    // Map 0..1 to angles 210°..300° (7 o'clock to 5 o'clock)
    float a0 = deg2rad(210.0f);
    float a1 = deg2rad(300.0f);
    float a = a0 + (a1 - a0) * v;
    float dx = std::cos(a);
    float dy = std::sin(a);
    int x0 = cx;
    int y0 = cy;
    int x1 = cx + (int)std::round(dx * (radius + 4));
    int y1 = cy + (int)std::round(dy * (radius + 4));
    // 2 px wide: draw two lines with +/- 1px normal offset
    float nx = -dy, ny = dx;
    int ox = (int)std::round(nx);
    int oy = (int)std::round(ny);
    NT_drawShapeI(kNT_line, x0+ox, y0+oy, x1+ox, y1+oy, col);
    NT_drawShapeI(kNT_line, x0-ox, y0-oy, x1-ox, y1-oy, col);
}

// --- Custom UI ---
static const char* dirName(int d){
    switch(d){ case 0: return "FWD"; case 1: return "BWD"; case 2: return "PING"; default: return "ODD/EVN"; }
}

uint32_t hasCustomUi(_NT_algorithm* self) {
    // Handle both encoders and two pots (probability + direction)
    return (uint32_t)(kNT_encoderL | kNT_encoderR | kNT_potC | kNT_potR);
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    EdgeLike* a = (EdgeLike*)self;
    // Left encoder: selection
    int8_t dl = data.encoders[0];
    if (dl) {
        int maxSel = 1 + 2 * a->seqNumSteps - 1; // last selectable index
        a->seqSelectedIndex += dl;
        if (a->seqSelectedIndex < 0) a->seqSelectedIndex = maxSel;
        if (a->seqSelectedIndex > maxSel) a->seqSelectedIndex = 0;
    }
    // Right encoder: value adjust
    int8_t dr = data.encoders[1];
    if (dr) {
        if (a->seqSelectedIndex == 0) {
            a->seqNumSteps += dr;
            if (a->seqNumSteps < 1) a->seqNumSteps = 1;
            if (a->seqNumSteps > 16) a->seqNumSteps = 16;
            if (a->seqPlayIdx >= a->seqNumSteps) a->seqPlayIdx = a->seqNumSteps - 1;
        } else {
            int pair = a->seqSelectedIndex - 1;
            int step = pair / 2; // 0..15
            int row  = pair % 2; // 0 pitch, 1 vel
            if (step >= a->seqNumSteps) step = a->seqNumSteps - 1;
            int* arr = (row==0) ? a->seqPitch : a->seqVel;
            int val = arr[step] + dr;
            if (val < 0) val = 0; if (val > 100) val = 100;
            arr[step] = val;
        }
    }
    // Pots: probability (C) and direction (R)
    float prob = std::fmax(0.0f, std::fmin(1.0f, data.pots[1]));
    a->seqProbability = (int)std::round(prob * 100.0f);
    float dirp = std::fmax(0.0f, std::fmin(1.0f, data.pots[2]));
    int dir = (int)std::floor(dirp * 4.0f);
    if (dir > 3) dir = 3; if (dir < 0) dir = 0;
    if (dir != a->seqDirection) {
        a->seqDirection = dir;
        // reset helper state for new mode
        a->seqPingDir = +1; a->seqOddPhase = true; a->seqPhasePos = -1; // so first advance goes to base
        a->seqPlayIdx = 0;
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    EdgeLike* a = (EdgeLike*)self;
    pots[1] = a->seqProbability / 100.0f;
    pots[2] = a->seqDirection / 3.0f;
}

bool draw(_NT_algorithm* self) {
    EdgeLike* a = (EdgeLike*)self;
    // Top info line
    char buf[64];
    NT_drawText(0, 0, "Sequencer", 15);
    int n = 0;
    n += NT_intToString(buf, a->seqProbability); buf[n++]='%'; buf[n]=0;
    NT_drawText(120, 0, "Prob:", 12);
    NT_drawText(156, 0, buf, 15);
    NT_drawText(0, 10, "Dir:", 12);
    NT_drawText(28, 10, dirName(a->seqDirection), 15);
    NT_drawText(120, 10, "Steps:", 12);
    char b2[8];
    NT_intToString(b2, a->seqNumSteps);
    NT_drawText(168, 10, b2, 15);

    // Knob grid bottom: 2 rows x up to 16 columns
    const int radius = 6;
    const int yPitch = 36; // higher row
    const int yVel   = 55; // lower row
    for (int col=0; col<16; ++col) {
        int cx = 8 + col * 16;
        bool grey = (col >= a->seqNumSteps);
        bool selPitch=false, selVel=false;
        if (a->seqSelectedIndex > 0) {
            int pair = a->seqSelectedIndex - 1;
            int s = pair / 2;
            int r = pair % 2;
            selPitch = (s==col && r==0);
            selVel   = (s==col && r==1);
        }
        // Highlight current playing step slightly brighter
        int colBase = (col == a->seqPlayIdx) ? 14 : 10;
        drawKnob(cx, yPitch, radius, a->seqPitch[col], grey, selPitch, colBase);
        drawKnob(cx, yVel,   radius, a->seqVel[col],   grey, selVel,   colBase);
    }
    return true; // suppress default header
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
    .draw = draw,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagInstrument,
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
    .serialise = []( _NT_algorithm* self, _NT_jsonStream& stream ) {
        EdgeLike* a = (EdgeLike*)self;
        // Basic fields
        stream.addMemberName("seqSteps");
        stream.addNumber(a->seqNumSteps);
        stream.addMemberName("seqDir");
        stream.addNumber(a->seqDirection);
        stream.addMemberName("seqProb");
        stream.addNumber(a->seqProbability);
        // Steps data
        char key[16];
        for (int i=0;i<16;++i) {
            std::snprintf(key, sizeof(key), "seqPitch%d", i);
            stream.addMemberName(key);
            stream.addNumber(a->seqPitch[i]);
        }
        for (int i=0;i<16;++i) {
            std::snprintf(key, sizeof(key), "seqVel%d", i);
            stream.addMemberName(key);
            stream.addNumber(a->seqVel[i]);
        }
    },
    .deserialise = []( _NT_algorithm* self, _NT_jsonParse& parse ) -> bool {
        EdgeLike* a = (EdgeLike*)self;
        int members = 0;
        if (!parse.numberOfObjectMembers(members)) return false;
        for (int idx=0; idx<members; ++idx) {
            int val = 0;
            if (parse.matchName("seqSteps")) {
                if (parse.number(val)) {
                    if (val < 1) val = 1; if (val > 16) val = 16; a->seqNumSteps = val;
                    if (a->seqPlayIdx >= a->seqNumSteps) a->seqPlayIdx = a->seqNumSteps - 1;
                }
            } else if (parse.matchName("seqDir")) {
                if (parse.number(val)) { if (val<0) val=0; if (val>3) val=3; a->seqDirection = val; }
            } else if (parse.matchName("seqProb")) {
                if (parse.number(val)) { if (val<0) val=0; if (val>100) val=100; a->seqProbability = val; }
            } else {
                // Try step keys
                bool matched = false;
                char key[16];
                for (int i=0;i<16 && !matched; ++i) {
                    std::snprintf(key, sizeof(key), "seqPitch%d", i);
                    if (parse.matchName(key)) {
                        matched = true;
                        int v=0; if (parse.number(v)) { if (v<0) v=0; if (v>100) v=100; a->seqPitch[i] = v; }
                        break;
                    }
                }
                if (!matched) {
                    for (int i=0;i<16 && !matched; ++i) {
                        std::snprintf(key, sizeof(key), "seqVel%d", i);
                        if (parse.matchName(key)) {
                            matched = true;
                            int v=0; if (parse.number(v)) { if (v<0) v=0; if (v>100) v=100; a->seqVel[i] = v; }
                            break;
                        }
                    }
                }
                if (!matched) {
                    parse.skipMember();
                }
            }
        }
        return true;
    },
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
