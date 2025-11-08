#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <new>

// Debug mode - set to true to enable debug counters and logging
#define VLOOP_DEBUG true

// Test mode - set to true to enable test sequence generation
#define VLOOP_TEST_SEQUENCE false

struct MidiEvent {
    uint8_t data[3];
    uint8_t subPulseOffset;  // Position within the beat (0-31 for PPQN=32)
    
    MidiEvent() {
        data[0] = data[1] = data[2] = 0;
        subPulseOffset = 0;
    }
};

#define MAX_EVENTS_PER_PULSE 16
#define MAX_LOOP_LENGTH 512
#define PPQN 32  // Pulses Per Quarter Note (internal resolution: 32 ticks per 1/4 note clock)

// Simple fixed-size vector for events at each pulse
struct EventBucket {
    MidiEvent events[MAX_EVENTS_PER_PULSE];
    uint8_t count;
    
    EventBucket() : count(0) {}
    
    void clear() {
        count = 0;
    }
    
    bool add(const MidiEvent& event) {
        if (count < MAX_EVENTS_PER_PULSE) {
            events[count] = event;
            count++;
            return true;
        }
        return false;
    }
    
    bool add(const uint8_t* data, uint8_t subPulseOffset) {
        if (count < MAX_EVENTS_PER_PULSE) {
            events[count].data[0] = data[0];
            events[count].data[1] = data[1];
            events[count].data[2] = data[2];
            events[count].subPulseOffset = subPulseOffset;
            count++;
            return true;
        }
        return false;
    }
};

struct VLoop : public _NT_algorithm {
    EventBucket eventBuckets[MAX_LOOP_LENGTH];      // Main "safe" loop buffer (indexed by beat)
    EventBucket committedOverdub[MAX_LOOP_LENGTH];  // Last committed overdub (can be undone)
    EventBucket activeOverdub[MAX_LOOP_LENGTH];     // Current overdub in progress
    uint16_t currentBeat;       // Current beat number (quarter note count)
    uint16_t subPulse;          // Sub-pulse within current beat (0-31, for PPQN=32)
    uint16_t loopLengthBeats;   // Loop length in beats (quarter notes)
    bool isRecording;
    bool isPlaying;
    bool recordArmed;         // Record mode armed, waiting for first MIDI or clock
    uint16_t recordStartPulse; // Pulse where recording actually started
    float lastClockCV;
    bool lastRecord;
    bool lastPlay;
    bool lastClear;
    bool lastUndo;
    uint16_t lastBeatWithEvent;  // Track last beat that had MIDI recorded
    
    // Time tracking for sub-pulse interpolation
    uint32_t samplesSinceLastClock;   // Samples elapsed since last clock edge
    uint32_t samplesPerClock;         // Measured clock period in samples
    uint16_t lastSubPulse;            // Track last sub-pulse to detect when we cross event thresholds
    
#if VLOOP_DEBUG
    // Debug counters (only when VLOOP_DEBUG is true)
    uint32_t totalClockEdges;
    uint32_t totalMidiSent;
    uint32_t totalMidiReceived;
    uint32_t noteOnReceived;
    uint32_t noteOffReceived;
    uint32_t noteOnRecorded;
    uint32_t noteOffRecorded;
    uint32_t noteOnSent;
    uint32_t noteOffSent;
    uint32_t midiSkippedNotRecording;
    uint32_t midiSkippedQuantization;
    uint32_t bucketOverflows;
    uint32_t stepCallCount;
    uint32_t continuousPlaybackChecks;  // How many times we check for events to play
    uint32_t continuousPlaybackEvents;  // How many events actually played
    uint16_t lastPulseWithMidi;
    uint8_t lastMidiStatus;
    uint8_t lastMidiData1;
    uint8_t lastMidiData2;
#endif
    
    VLoop() {
        currentBeat = 0;
        subPulse = 0;
        loopLengthBeats = 0;
        isRecording = false;
        isPlaying = false;
        recordArmed = false;
        recordStartPulse = 0;
        lastClockCV = 0.0f;
        lastRecord = false;
        lastPlay = false;
        lastClear = false;
        lastUndo = false;
        lastBeatWithEvent = 0;
        samplesSinceLastClock = 0;
        samplesPerClock = NT_globals.sampleRate / 2; // Default to 120 BPM (0.5 sec per beat)
        lastSubPulse = 0;
#if VLOOP_DEBUG
        totalClockEdges = 0;
        totalMidiSent = 0;
        totalMidiReceived = 0;
        noteOnReceived = 0;
        noteOffReceived = 0;
        noteOnRecorded = 0;
        noteOffRecorded = 0;
        noteOnSent = 0;
        noteOffSent = 0;
        midiSkippedNotRecording = 0;
        midiSkippedQuantization = 0;
        bucketOverflows = 0;
        stepCallCount = 0;
        lastPulseWithMidi = 0;
        lastMidiStatus = 0;
        lastMidiData1 = 0;
        lastMidiData2 = 0;
#endif
    }
};

enum {
    kParamClockInput = 0,
    kParamRecord,
    kParamPlay,
    kParamClear,
    kParamUndo,
    kParamQuantize,
    kParamLoopEndQuant,
    kParamLoopPreset,        // Pre-set loop length: 0=Off, 1=1bar, 2=2bars, 3=4bars, 4=8bars
    kParamMidiOutChannel,
    kNumParams
};

static const char* const loopPresetStrings[] = {
    "Off",
    "1 bar",
    "2 bars",
    "4 bars",
    "8 bars"
};

static const _NT_parameter parameters[] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Play", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Undo", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Note Quant", .min = 0, .max = 4, .def = 0, .scaling = kNT_scalingNone },  // 0=off, 1=1/32, 2=1/16, 3=1/8, 4=1/4
    { .name = "Loop End Quant", .min = 0, .max = 4, .def = 4, .scaling = kNT_scalingNone },  // Default to 1/4 note
    { .name = "Loop Preset", .min = 0, .max = 4, .def = 0, .unit = kNT_unitEnum, .scaling = kNT_scalingNone, .enumStrings = loopPresetStrings },  // Pre-set loop length
    { .name = "MIDI Out Ch", .min = 1, .max = 16, .def = 2, .scaling = kNT_scalingNone } // Output channel (default ch 2)
};

static const uint8_t page1[] = { kParamClockInput, kParamRecord, kParamPlay, kParamClear, kParamUndo, kParamQuantize, kParamLoopEndQuant, kParamLoopPreset, kParamMidiOutChannel };
static const _NT_parameterPage pages[] = {
    { .name = "VLoop", .numParams = ARRAY_SIZE(page1), .params = page1 }
};
static const _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages
};

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = kNumParams;
    req.sram = sizeof(VLoop);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    VLoop* loop = new (ptrs.sram) VLoop();
    loop->parameters = parameters;
    loop->parameterPages = &parameterPages;
    return loop;
}

#if VLOOP_TEST_SEQUENCE
// Generate a dense chord sequence to stress-test MIDI handling
// Multiple simultaneous notes, chord progressions, overlapping events
void generateTestSequence(VLoop* loop) {
    // Clear existing loop
    for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
        loop->eventBuckets[i].clear();
    }
    
    const int pulsesPerBeat = 8;
    const uint8_t velocity = 100;
    const uint8_t channel = 0;  // MIDI channel 1 (0-indexed)
    
    // Chord progression with varying sizes (3-8 note chords)
    // C Major 7 (4 notes) → D minor 9 (5 notes) → G7#9 (6 notes) → C Major 13 (7 notes) → Dense cluster (8 notes)
    const uint8_t chords[][8] = {
        {48, 52, 55, 59, 0, 0, 0, 0},      // C Major 7 (C, E, G, B) - 4 notes
        {50, 53, 57, 60, 64, 0, 0, 0},     // D minor 9 (D, F, A, C, E) - 5 notes
        {43, 47, 50, 54, 57, 61, 0, 0},    // G7#9 (G, B, D, F, A, C#) - 6 notes
        {48, 52, 55, 59, 62, 64, 69, 0},   // C Major 13 (C, E, G, B, D, E, A) - 7 notes
        {36, 40, 43, 47, 50, 54, 57, 60}   // Dense cluster - 8 notes simultaneously
    };
    const uint8_t chordSizes[] = {4, 5, 6, 7, 8};
    const int numChords = 5;
    
    const int pulsesPerChord = 8;  // Quarter note per chord
    int totalNoteOns = 0;
    int totalNoteOffs = 0;
    
    // Place chords with overlapping notes (hold previous chord while starting next)
    for (int c = 0; c < numChords; c++) {
        uint16_t startPulse = c * pulsesPerChord;
        uint16_t endPulse = startPulse + pulsesPerChord + 2;  // Overlap by 2 pulses
        
        // Add all note-ons for this chord at the same pulse
        for (int n = 0; n < chordSizes[c]; n++) {
            MidiEvent noteOn;
            noteOn.data[0] = 0x90 | channel;
            noteOn.data[1] = chords[c][n];
            noteOn.data[2] = velocity;
            if (loop->eventBuckets[startPulse].add(noteOn)) {
                totalNoteOns++;
            } else {
                // Bucket overflow! Try next pulse
                if (startPulse + 1 < MAX_LOOP_LENGTH) {
                    loop->eventBuckets[startPulse + 1].add(noteOn);
                    totalNoteOns++;
                }
            }
        }
        
        // Add all note-offs for this chord at the same pulse
        if (endPulse < MAX_LOOP_LENGTH) {
            for (int n = 0; n < chordSizes[c]; n++) {
                MidiEvent noteOff;
                noteOff.data[0] = 0x80 | channel;
                noteOff.data[1] = chords[c][n];
                noteOff.data[2] = 0;
                if (loop->eventBuckets[endPulse].add(noteOff)) {
                    totalNoteOffs++;
                } else {
                    // Bucket overflow! Try next pulse
                    if (endPulse + 1 < MAX_LOOP_LENGTH) {
                        loop->eventBuckets[endPulse + 1].add(noteOff);
                        totalNoteOffs++;
                    }
                }
            }
        }
    }
    
    // Set loop length
    loop->lastPulseWithEvent = (numChords * pulsesPerChord) + pulsesPerChord + 2;
    loop->loopLength = ((loop->lastPulseWithEvent + pulsesPerBeat) / pulsesPerBeat) * pulsesPerBeat;
    loop->currentPulse = 0;
    loop->isPlaying = true;
    loop->isRecording = false;
    
#if VLOOP_DEBUG
    // Count the notes we actually added
    loop->noteOnRecorded = totalNoteOns;
    loop->noteOffRecorded = totalNoteOffs;
#endif
}
#endif

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    VLoop* loop = (VLoop*)self;
    
#if VLOOP_DEBUG
    loop->stepCallCount++;
#endif
    
    // Get parameter values
    int clockBusIndex = (int)loop->v[kParamClockInput] - 1;
    bool recordActive = loop->v[kParamRecord] > 0.5f;
    bool playActive = loop->v[kParamPlay] > 0.5f;
    bool clearActive = loop->v[kParamClear] > 0.5f;
    bool undoActive = loop->v[kParamUndo] > 0.5f;
    
    // Handle Clear knob - ALWAYS clears everything
    if (clearActive && !loop->lastClear) {
        // FULL CLEAR: Clear everything
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->eventBuckets[i].clear();
            loop->committedOverdub[i].clear();
            loop->activeOverdub[i].clear();
        }
        loop->loopLengthBeats = 0;
        loop->currentBeat = 0;
        loop->subPulse = 0;
        loop->isRecording = false;
        loop->isPlaying = false;
        loop->lastBeatWithEvent = 0;
        loop->samplesSinceLastClock = 0;
        
#if VLOOP_DEBUG
        // Reset debug counters on clear
        loop->totalClockEdges = 0;
        loop->totalMidiSent = 0;
        loop->totalMidiReceived = 0;
        loop->noteOnReceived = 0;
        loop->noteOffReceived = 0;
        loop->noteOnRecorded = 0;
        loop->noteOffRecorded = 0;
        loop->noteOnSent = 0;
        loop->noteOffSent = 0;
        loop->midiSkippedNotRecording = 0;
        loop->midiSkippedQuantization = 0;
        loop->bucketOverflows = 0;
        loop->stepCallCount = 0;
#endif
    }
    loop->lastClear = clearActive;
    
    // Handle Undo knob - Clears committed overdub only (keeps main loop)
    if (undoActive && !loop->lastUndo) {
        // UNDO: Clear committed and active overdub buffers
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->committedOverdub[i].clear();
            loop->activeOverdub[i].clear();
        }
        // Keep playing the main loop
    }
    loop->lastUndo = undoActive;
    
    // Handle Record knob changes
    if (recordActive && !loop->lastRecord) {
        // Record turned ON
        if (loop->loopLengthBeats > 0 && loop->isPlaying) {
            // OVERDUB MODE: Loop exists and is playing
            // First, commit any previous overdub by merging committed into main
            for (int i = 0; i < loop->loopLengthBeats && i < MAX_LOOP_LENGTH; i++) {
                EventBucket& committed = loop->committedOverdub[i];
                EventBucket& main = loop->eventBuckets[i];
                
                // Merge committed into main
                for (uint8_t j = 0; j < committed.count; j++) {
                    main.add(committed.events[j]);
                }
                
                // Clear committed buffer (it's now part of main)
                committed.clear();
            }
            
            // Now enable recording to activeOverdub
            loop->isRecording = true;
            loop->recordArmed = false;
        } else {
            // NEW RECORDING: ARM record mode (wait for first MIDI)
            loop->isPlaying = false;
            loop->isRecording = false;
            loop->recordArmed = true;
            loop->currentBeat = 0;
            loop->subPulse = 0;
            loop->lastBeatWithEvent = 0;
            loop->samplesSinceLastClock = 0;
            // Clear all buffers for fresh recording
            for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
                loop->eventBuckets[i].clear();
                loop->committedOverdub[i].clear();
                loop->activeOverdub[i].clear();
            }
            
            // Check for loop preset (pre-populate loop length)
            int loopPreset = (int)loop->v[kParamLoopPreset];
            if (loopPreset > 0) {
                // Calculate loop length in beats: 1bar=4, 2bars=8, 4bars=16, 8bars=32
                int bars = (loopPreset == 1) ? 1 : (1 << (loopPreset - 1)); // 1, 2, 4, 8
                loop->loopLengthBeats = bars * 4;  // Convert bars to beats (assuming 4/4 time)
            } else {
                loop->loopLengthBeats = 0;  // Will be set when recording stops
            }
        }
    } else if (!recordActive && loop->lastRecord) {
        // Record turned OFF - stop recording
        if (loop->isRecording && !loop->isPlaying) {
            // Was doing initial recording (not overdub)
            // If loop wasn't pre-set, use the last beat that had events + 1
            if (loop->loopLengthBeats == 0 || loop->v[kParamLoopPreset] == 0) {
                loop->loopLengthBeats = loop->lastBeatWithEvent + 1;
            }
            // else: keep the pre-set loop length
            
            // If Play is ON, auto-start playback
            if (playActive && loop->loopLengthBeats > 0) {
                loop->isPlaying = true;
                loop->currentBeat = 0;
                loop->subPulse = 0;
            }
        }
        // If overdubbing, loop length stays the same
        loop->isRecording = false;
        loop->recordArmed = false;
    }
    loop->lastRecord = recordActive;
    
    // Handle Play knob changes
    if (playActive && !loop->lastPlay) {
        // Knob turned up - start playback
#if VLOOP_TEST_SEQUENCE
        // In test mode, generate a test sequence instead of requiring recording
        generateTestSequence(loop);
#else
        // Normal mode: Don't allow playback while recording
        if (loop->loopLengthBeats > 0 && !loop->isRecording) {
            loop->isPlaying = true;
            loop->currentBeat = 0;
            loop->subPulse = 0;
            loop->lastSubPulse = 0;
            loop->samplesSinceLastClock = 0;
        }
#endif
    } else if (!playActive && loop->lastPlay) {
        // Knob turned down - stop playback
        loop->isPlaying = false;
    }
    loop->lastPlay = playActive;
    
    // Process all frames looking for clock edge (detect only ONE edge per step)
    const int totalFrames = numFramesBy4 * 4;
    bool clockEdgeDetected = false;
    for (int frame = 0; frame < totalFrames && !clockEdgeDetected; frame++) {
        // Read clock CV from selected bus
        float clockCV = busFrames[clockBusIndex * totalFrames + frame];
        
        // Detect rising edge (crosses 2.5V threshold)
        bool clockEdge = (clockCV > 2.5f && loop->lastClockCV <= 2.5f);
        loop->lastClockCV = clockCV;
        
        if (clockEdge) {
            clockEdgeDetected = true;
#if VLOOP_DEBUG
            loop->totalClockEdges++;
#endif
            
            // Measure clock period for sub-pulse interpolation
            if (loop->samplesSinceLastClock > 0) {
                // Use exponential moving average to smooth clock period measurement
                // This helps with clock jitter
                loop->samplesPerClock = (loop->samplesPerClock * 7 + loop->samplesSinceLastClock) / 8;
            }
            
            // Handle playback - advance beat and manage loop wrap
            if (loop->isPlaying && loop->loopLengthBeats > 0) {
                // Advance by 1 beat per clock pulse (clock = quarter note)
                loop->currentBeat++;
                
                if (loop->currentBeat >= loop->loopLengthBeats) {
                    // Loop wrap!
                    
                    // If overdubbing, commit active overdub to committed buffer
                    if (loop->isRecording) {
                        for (int beat = 0; beat < loop->loopLengthBeats && beat < MAX_LOOP_LENGTH; beat++) {
                            EventBucket& active = loop->activeOverdub[beat];
                            EventBucket& committed = loop->committedOverdub[beat];
                            
                            // Add all active overdub events to committed buffer
                            for (uint8_t i = 0; i < active.count; i++) {
                                committed.add(active.events[i]);
                            }
                            
                            // Clear active overdub buffer for next cycle
                            active.clear();
                        }
                    }
                    
                    // Send all-notes-off to prevent stuck notes
                    uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
                    NT_sendMidi3ByteMessage(
                        kNT_destinationInternal,
                        0xB0 | outputChannel,  // CC on output channel
                        123,                    // All Notes Off
                        0
                    );
                    loop->currentBeat = 0; // Loop wrap
                }
                
                // Reset sub-pulse and time tracking for new beat
                loop->subPulse = 0;
                loop->samplesSinceLastClock = 0;
                loop->lastSubPulse = 0;
                
                // Play any events at position 0 immediately on the beat
                if (loop->currentBeat < loop->loopLengthBeats && loop->currentBeat < MAX_LOOP_LENGTH) {
                    uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
                    
                    // Play main buffer events at position 0
                    EventBucket& mainBucket = loop->eventBuckets[loop->currentBeat];
                    for (uint8_t i = 0; i < mainBucket.count; i++) {
                        if (mainBucket.events[i].subPulseOffset == 0) {
                            uint8_t statusByte = mainBucket.events[i].data[0];
                            uint8_t messageType = statusByte & 0xF0;
                            uint8_t remappedStatus = messageType | outputChannel;
                            
                            NT_sendMidi3ByteMessage(
                                kNT_destinationInternal,
                                remappedStatus,
                                mainBucket.events[i].data[1],
                                mainBucket.events[i].data[2]
                            );
                        }
                    }
                    
                    // Play committed overdub events at position 0
                    EventBucket& committedBucket = loop->committedOverdub[loop->currentBeat];
                    for (uint8_t i = 0; i < committedBucket.count; i++) {
                        if (committedBucket.events[i].subPulseOffset == 0) {
                            uint8_t statusByte = committedBucket.events[i].data[0];
                            uint8_t messageType = statusByte & 0xF0;
                            uint8_t remappedStatus = messageType | outputChannel;
                            
                            NT_sendMidi3ByteMessage(
                                kNT_destinationInternal,
                                remappedStatus,
                                committedBucket.events[i].data[1],
                                committedBucket.events[i].data[2]
                            );
                        }
                    }
                }
            }
            // Handle recording (initial recording only, not overdub)
            else if (loop->isRecording && !loop->isPlaying) {
                // Initial recording - increment by 1 beat per clock pulse
                loop->currentBeat++;
                
                // Auto-stop if we've reached a pre-set loop length
                if (loop->loopLengthBeats > 0 && loop->currentBeat >= loop->loopLengthBeats) {
                    loop->isRecording = false;
                    loop->currentBeat = 0;
                    // Optionally auto-start playback if Play knob is on
                    bool playActive = loop->v[kParamPlay] > 0.5f;
                    if (playActive) {
                        loop->isPlaying = true;
                    }
                }
                else if (loop->currentBeat >= MAX_LOOP_LENGTH) {
                    loop->currentBeat = MAX_LOOP_LENGTH - 1;
                }
                
                // Reset sub-pulse and time tracking
                loop->subPulse = 0;
                loop->samplesSinceLastClock = 0;
            }
            // Record armed - waiting for first MIDI
            else if (loop->recordArmed) {
                // Armed but not recording - stay at beat 0, waiting for first MIDI
                // Do nothing, let midiMessage() start the recording
            }
        }
    }
    
    // Track samples for sub-pulse interpolation (only when playing or recording)
    if (loop->isPlaying || loop->isRecording) {
        loop->samplesSinceLastClock += totalFrames;
        
        // Calculate current sub-pulse position (interpolate between clocks)
        if (loop->samplesPerClock > 0) {
            uint32_t estimatedSubPulse = (loop->samplesSinceLastClock * PPQN) / loop->samplesPerClock;
            if (estimatedSubPulse < PPQN) {
                loop->subPulse = (uint16_t)estimatedSubPulse;
            } else {
                loop->subPulse = PPQN - 1; // Cap at max sub-pulse
            }
        }
        
        // CONTINUOUS PLAYBACK: Check if we've crossed any event sub-pulse thresholds
        // This allows events to play at their precise quantized timing within the beat
        if (loop->isPlaying && loop->loopLengthBeats > 0 && loop->subPulse != loop->lastSubPulse) {
            if (loop->currentBeat < loop->loopLengthBeats && loop->currentBeat < MAX_LOOP_LENGTH) {
                uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
                
                // Check main buffer events
                EventBucket& mainBucket = loop->eventBuckets[loop->currentBeat];
                for (uint8_t i = 0; i < mainBucket.count; i++) {
                    // Play event if we just crossed its sub-pulse threshold
                    // Handle wraparound: if lastSubPulse > subPulse, we crossed beat boundary
                    bool crossed = false;
                    if (loop->lastSubPulse < loop->subPulse) {
                        // Normal case: moving forward in time
                        crossed = (mainBucket.events[i].subPulseOffset > loop->lastSubPulse && 
                                   mainBucket.events[i].subPulseOffset <= loop->subPulse);
                    } else {
                        // Beat wraparound case: play events from lastSubPulse to end, or 0 to subPulse
                        crossed = (mainBucket.events[i].subPulseOffset > loop->lastSubPulse || 
                                   mainBucket.events[i].subPulseOffset <= loop->subPulse);
                    }
                    
                    if (crossed) {
                        
                        uint8_t statusByte = mainBucket.events[i].data[0];
                        uint8_t messageType = statusByte & 0xF0;
                        uint8_t remappedStatus = messageType | outputChannel;
                        
                        NT_sendMidi3ByteMessage(
                            kNT_destinationInternal,
                            remappedStatus,
                            mainBucket.events[i].data[1],
                            mainBucket.events[i].data[2]
                        );
                    }
                }
                
                // Check committed overdub events
                EventBucket& committedBucket = loop->committedOverdub[loop->currentBeat];
                for (uint8_t i = 0; i < committedBucket.count; i++) {
                    // Play event if we just crossed its sub-pulse threshold
                    // Handle wraparound: if lastSubPulse > subPulse, we crossed beat boundary
                    bool crossed = false;
                    if (loop->lastSubPulse < loop->subPulse) {
                        // Normal case: moving forward in time
                        crossed = (committedBucket.events[i].subPulseOffset > loop->lastSubPulse && 
                                   committedBucket.events[i].subPulseOffset <= loop->subPulse);
                    } else {
                        // Beat wraparound case: play events from lastSubPulse to end, or 0 to subPulse
                        crossed = (committedBucket.events[i].subPulseOffset > loop->lastSubPulse || 
                                   committedBucket.events[i].subPulseOffset <= loop->subPulse);
                    }
                    
                    if (crossed) {
                        
                        uint8_t statusByte = committedBucket.events[i].data[0];
                        uint8_t messageType = statusByte & 0xF0;
                        uint8_t remappedStatus = messageType | outputChannel;
                        
                        NT_sendMidi3ByteMessage(
                            kNT_destinationInternal,
                            remappedStatus,
                            committedBucket.events[i].data[1],
                            committedBucket.events[i].data[2]
                        );
                    }
                }
            }
        }
        
        // Update last sub-pulse for next iteration
        loop->lastSubPulse = loop->subPulse;
    }
}

void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    VLoop* loop = (VLoop*)self;
    
    // Extract channel from incoming MIDI
    uint8_t inputChannel = byte0 & 0x0F;
    uint8_t messageType = byte0 & 0xF0;
    
    // ONLY process Channel 1 (0 in 0-indexed) - ignore everything else
    if (inputChannel != 0) {
        return;  // Not channel 1, ignore completely
    }
    
    // Get output channel parameter (1-16, convert to 0-15)
    uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
    
    // ALWAYS pass Channel 1 through to output channel (so you hear what you play)
    uint8_t remappedStatus = messageType | outputChannel;
    NT_sendMidi3ByteMessage(
        kNT_destinationInternal,
        remappedStatus,
        byte1,
        byte2
    );
    
#if VLOOP_DEBUG
    loop->totalMidiReceived++;
    // Count note on/off
    uint8_t msgType = byte0 & 0xF0;
    if (msgType == 0x90 && byte2 > 0) {  // Note on (velocity > 0)
        loop->noteOnReceived++;
    } else if (msgType == 0x80 || (msgType == 0x90 && byte2 == 0)) {  // Note off
        loop->noteOffReceived++;
    }
#endif
    
    // Don't record system messages
    if ((byte0 & 0xF0) == 0xF0) return;
    
    // If armed, start recording on first MIDI message (quantized start)
    if (loop->recordArmed && !loop->isRecording) {
        loop->isRecording = true;
        loop->recordArmed = false;
        loop->currentBeat = 0;
        loop->subPulse = 0;
        loop->recordStartPulse = 0;
        loop->samplesSinceLastClock = 0;
        // First event goes to beat 0
    }
    
    // Only record when in recording mode
    if (!loop->isRecording) {
#if VLOOP_DEBUG
        loop->midiSkippedNotRecording++;
#endif
        return;
    }
    
    // Get quantization setting: 0=off, 1=1/32, 2=1/16, 3=1/8, 4=1/4
    // Convert to sub-pulse ticks (PPQN=32, so 1/4=32, 1/8=16, 1/16=8, 1/32=4)
    int quantize = (int)loop->v[kParamQuantize];
    int quantizeTicks;
    if (quantize == 0) {
        quantizeTicks = 1;  // No quantization
    } else if (quantize == 1) {
        quantizeTicks = PPQN / 8;  // 1/32 note = 4 ticks
    } else if (quantize == 2) {
        quantizeTicks = PPQN / 4;  // 1/16 note = 8 ticks
    } else if (quantize == 3) {
        quantizeTicks = PPQN / 2;  // 1/8 note = 16 ticks
    } else {  // quantize == 4
        quantizeTicks = PPQN;  // 1/4 note = 32 ticks
    }
    
    // Quantize current sub-pulse to nearest quantized tick
    uint16_t quantizedSubPulse = loop->subPulse;
    if (quantize > 0) {
        // Round to nearest quantized tick
        quantizedSubPulse = ((loop->subPulse + (quantizeTicks / 2)) / quantizeTicks) * quantizeTicks;
        if (quantizedSubPulse >= PPQN) {
            quantizedSubPulse = PPQN - 1; // Cap at max sub-pulse
        }
    }
    
    // Current beat for recording
    uint16_t recordBeat = loop->currentBeat;
    
    // Ensure we don't go past max length (or loop length during overdub)
    if (loop->isPlaying && recordBeat >= loop->loopLengthBeats) {
        recordBeat = recordBeat % loop->loopLengthBeats;  // Wrap to loop length during overdub
    } else if (recordBeat >= MAX_LOOP_LENGTH) {
        recordBeat = MAX_LOOP_LENGTH - 1;
    }
    
    // Choose buffer: active overdub if playing (overdub mode), main buffer if not (initial recording)
    EventBucket* targetBucket;
    if (loop->isPlaying) {
        // OVERDUB MODE: Record to active overdub buffer (will be committed at loop wrap)
        targetBucket = &loop->activeOverdub[recordBeat];
    } else {
        // INITIAL RECORDING: Record directly to main buffer
        targetBucket = &loop->eventBuckets[recordBeat];
    }
    
    // Add event to target bucket with quantized sub-pulse offset
    if (recordBeat < MAX_LOOP_LENGTH) {
        uint8_t midiData[3] = {byte0, byte1, byte2};
        bool added = targetBucket->add(midiData, (uint8_t)quantizedSubPulse);
        
        if (added) {
            // Track last beat with recorded event (for initial recording only)
            if (!loop->isPlaying) {
                loop->lastBeatWithEvent = recordBeat;
            }
            
#if VLOOP_DEBUG
            // Count recorded notes
            uint8_t msgType = byte0 & 0xF0;
            if (msgType == 0x90 && byte2 > 0) {  // Note on
                loop->noteOnRecorded++;
            } else if (msgType == 0x80 || (msgType == 0x90 && byte2 == 0)) {  // Note off
                loop->noteOffRecorded++;
            }
#endif
        }
#if VLOOP_DEBUG
        else {
            // Bucket was full - event dropped!
            loop->bucketOverflows++;
        }
#endif
    }
}

void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
    VLoop* loop = (VLoop*)self;
    
    // Write loop metadata
    stream.addMemberName("loopLengthBeats");
    stream.addNumber((int)loop->loopLengthBeats);
    
    stream.addMemberName("currentBeat");
    stream.addNumber((int)loop->currentBeat);
    
    stream.addMemberName("subPulse");
    stream.addNumber((int)loop->subPulse);
    
    stream.addMemberName("isRecording");
    stream.addBoolean(loop->isRecording);
    
    stream.addMemberName("recordArmed");
    stream.addBoolean(loop->recordArmed);
    
    stream.addMemberName("isPlaying");
    stream.addBoolean(loop->isPlaying);
    
#if VLOOP_DEBUG
    // Write debug counters
    stream.addMemberName("debug");
    stream.openObject();
    stream.addMemberName("totalClockEdges");
    stream.addNumber((int)loop->totalClockEdges);
    stream.addMemberName("totalMidiSent");
    stream.addNumber((int)loop->totalMidiSent);
    stream.addMemberName("totalMidiReceived");
    stream.addNumber((int)loop->totalMidiReceived);
    stream.addMemberName("noteOnReceived");
    stream.addNumber((int)loop->noteOnReceived);
    stream.addMemberName("noteOffReceived");
    stream.addNumber((int)loop->noteOffReceived);
    stream.addMemberName("noteOnRecorded");
    stream.addNumber((int)loop->noteOnRecorded);
    stream.addMemberName("noteOffRecorded");
    stream.addNumber((int)loop->noteOffRecorded);
    stream.addMemberName("noteOnSent");
    stream.addNumber((int)loop->noteOnSent);
    stream.addMemberName("noteOffSent");
    stream.addNumber((int)loop->noteOffSent);
    stream.addMemberName("midiSkippedNotRecording");
    stream.addNumber((int)loop->midiSkippedNotRecording);
    stream.addMemberName("midiSkippedQuantization");
    stream.addNumber((int)loop->midiSkippedQuantization);
    stream.addMemberName("bucketOverflows");
    stream.addNumber((int)loop->bucketOverflows);
    stream.addMemberName("stepCallCount");
    stream.addNumber((int)loop->stepCallCount);
    stream.addMemberName("lastPulseWithMidi");
    stream.addNumber((int)loop->lastPulseWithMidi);
    stream.addMemberName("lastMidiSent");
    stream.openObject();
    stream.addMemberName("status");
    stream.addNumber((int)loop->lastMidiStatus);
    stream.addMemberName("data1");
    stream.addNumber((int)loop->lastMidiData1);
    stream.addMemberName("data2");
    stream.addNumber((int)loop->lastMidiData2);
    stream.closeObject();
    
    // Timing debug info for continuous playback
    stream.addMemberName("timing");
    stream.openObject();
    stream.addMemberName("currentBeat");
    stream.addNumber((int)loop->currentBeat);
    stream.addMemberName("subPulse");
    stream.addNumber((int)loop->subPulse);
    stream.addMemberName("lastSubPulse");
    stream.addNumber((int)loop->lastSubPulse);
    stream.addMemberName("samplesPerClock");
    stream.addNumber((int)loop->samplesPerClock);
    stream.addMemberName("samplesSinceLastClock");
    stream.addNumber((int)loop->samplesSinceLastClock);
    stream.closeObject();
    
    stream.closeObject();
#endif
    
    // Write event buckets (only non-empty ones for efficiency)
    stream.addMemberName("buckets");
    stream.openArray();
    for (uint16_t pulse = 0; pulse < MAX_LOOP_LENGTH; pulse++) {
        if (loop->eventBuckets[pulse].count > 0) {
            stream.openObject();
            stream.addMemberName("pulse");
            stream.addNumber((int)pulse);
            stream.addMemberName("events");
            stream.openArray();
            for (uint8_t i = 0; i < loop->eventBuckets[pulse].count; i++) {
                stream.openObject();
                stream.addMemberName("status");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[0]);
                stream.addMemberName("data1");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[1]);
                stream.addMemberName("data2");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].data[2]);
                stream.addMemberName("subPulseOffset");
                stream.addNumber((int)loop->eventBuckets[pulse].events[i].subPulseOffset);
                stream.closeObject();
            }
            stream.closeArray();
            stream.closeObject();
        }
    }
    stream.closeArray();
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
    VLoop* loop = (VLoop*)self;
    
    // Reset state when loading preset
    for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
        loop->eventBuckets[i].clear();
    }
    loop->loopLengthBeats = 0;
    loop->currentBeat = 0;
    loop->subPulse = 0;
    loop->isRecording = false;
    loop->isPlaying = false;
    
    // Load the data
    int num;
    if (parse.matchName("loopLengthBeats")) {
        parse.number(num);
        loop->loopLengthBeats = num;
    }
    // Also support old "loopLength" name for compatibility
    if (parse.matchName("loopLength")) {
        parse.number(num);
        loop->loopLengthBeats = num;
    }
    
    // Load event buckets array
    if (parse.matchName("buckets")) {
        int arraySize = 0;
        if (parse.numberOfArrayElements(arraySize)) {
            for (int i = 0; i < arraySize; i++) {
                int bucketObjSize = 0;
                if (parse.numberOfObjectMembers(bucketObjSize)) {
                    uint16_t pulse = 0;
                    for (int j = 0; j < bucketObjSize; j++) {
                        if (parse.matchName("pulse")) {
                            parse.number(num);
                            pulse = num;
                        } else if (parse.matchName("events")) {
                            int eventsArraySize = 0;
                            if (parse.numberOfArrayElements(eventsArraySize)) {
                                for (int k = 0; k < eventsArraySize && k < MAX_EVENTS_PER_PULSE; k++) {
                                    int eventObjSize = 0;
                                    if (parse.numberOfObjectMembers(eventObjSize)) {
                                        MidiEvent event;
                                        for (int m = 0; m < eventObjSize; m++) {
                                            if (parse.matchName("status")) {
                                                parse.number(num);
                                                event.data[0] = num;
                                            } else if (parse.matchName("data1")) {
                                                parse.number(num);
                                                event.data[1] = num;
                                            } else if (parse.matchName("data2")) {
                                                parse.number(num);
                                                event.data[2] = num;
                                            } else {
                                                parse.skipMember();
                                            }
                                        }
                                        if (pulse < MAX_LOOP_LENGTH) {
                                            loop->eventBuckets[pulse].add(event);
                                        }
                                    }
                                }
                            }
                        } else {
                            parse.skipMember();
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('V', 'L', 'O', 'P'),
    .name = "VLoop",
    .description = "Simple MIDI Looper",
    .numSpecifications = 0,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .step = step,
    .midiMessage = midiMessage,
    .tags = kNT_tagUtility,
    .hasCustomUi = 0,
    .customUi = 0,
    .setupUi = 0,
    .serialise = serialise,
    .deserialise = deserialise
};

extern "C" {
    uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
        switch (selector) {
            case kNT_selector_version:
                return kNT_apiVersionCurrent;
            case kNT_selector_numFactories:
                return 1;
            case kNT_selector_factoryInfo:
                return (uintptr_t)((data == 0) ? &factory : 0);
        }
        return 0;
    }
}
