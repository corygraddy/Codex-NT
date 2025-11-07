#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <new>

// Debug mode - set to true to enable debug counters and logging
#define VLOOP_DEBUG true

// Test mode - set to true to enable test sequence generation
#define VLOOP_TEST_SEQUENCE false

struct MidiEvent {
    uint8_t data[3];
    
    MidiEvent() {
        data[0] = data[1] = data[2] = 0;
    }
};

#define MAX_EVENTS_PER_PULSE 16
#define MAX_LOOP_LENGTH 512

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
};

struct VLoop : public _NT_algorithm {
    EventBucket eventBuckets[MAX_LOOP_LENGTH];
    uint16_t currentPulse;
    uint16_t loopLength;
    bool isRecording;
    bool isPlaying;
    bool recordArmed;         // Record mode armed, waiting for first MIDI
    uint16_t recordStartPulse; // Pulse where recording actually started
    float lastClockCV;
    bool lastRecord;
    bool lastPlay;
    bool lastClear;
    uint16_t lastPulseWithEvent;  // Track last pulse that had MIDI recorded
    
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
    uint16_t lastPulseWithMidi;
    uint8_t lastMidiStatus;
    uint8_t lastMidiData1;
    uint8_t lastMidiData2;
#endif
    
    VLoop() {
        currentPulse = 0;
        loopLength = 0;
        isRecording = false;
        isPlaying = false;
        recordArmed = false;
        recordStartPulse = 0;
        lastClockCV = 0.0f;
        lastRecord = false;
        lastPlay = false;
        lastClear = false;
        lastPulseWithEvent = 0;
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
    kParamQuantize,
    kParamAutoStop,
    kParamMidiOutChannel,
    kNumParams
};

static const _NT_parameter parameters[] = {
    { .name = "Clock In", .min = 1, .max = 28, .def = 1, .unit = kNT_unitCvInput },
    { .name = "Record", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Play", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Clear", .min = 0, .max = 1, .def = 0, .scaling = kNT_scalingNone },
    { .name = "Quantize", .min = 0, .max = 2, .def = 0, .scaling = kNT_scalingNone },  // 0=1/32, 1=1/16, 2=1/8
    { .name = "Auto Stop", .min = 0, .max = 16, .def = 0, .scaling = kNT_scalingNone }, // 0=off, 1-16=beats
    { .name = "MIDI Out Ch", .min = 1, .max = 16, .def = 2, .scaling = kNT_scalingNone } // Output channel (default ch 2)
};

static const uint8_t page1[] = { kParamClockInput, kParamRecord, kParamPlay, kParamClear, kParamQuantize, kParamAutoStop, kParamMidiOutChannel };
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
#if VLOOP_DEBUG
    int quantize = (int)loop->v[kParamQuantize];  // 0=1/32, 1=1/16, 2=1/8
#endif
    int autoStop = (int)loop->v[kParamAutoStop];   // 0=off, 1-16=beats
    
    // Assuming clock input is 1/32 notes (8 pulses per beat)
    int pulsesPerBeat = 8;  // 1/32 notes, 8 per quarter note
    
    // Handle Clear knob
    if (clearActive && !loop->lastClear) {
        // Clear all event buckets
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->eventBuckets[i].clear();
        }
        loop->loopLength = 0;
        loop->currentPulse = 0;
        loop->isRecording = false;
        loop->isPlaying = false;
        loop->lastPulseWithEvent = 0;
        
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
    
    // Handle Record knob changes
    if (recordActive && !loop->lastRecord) {
        // Knob turned up - ARM record mode (wait for first MIDI)
        loop->isPlaying = false;
        loop->isRecording = false;
        loop->recordArmed = true;
        loop->currentPulse = 0;
        loop->lastPulseWithEvent = 0;
        // Clear all buckets
        for (int i = 0; i < MAX_LOOP_LENGTH; i++) {
            loop->eventBuckets[i].clear();
        }
        loop->loopLength = 0;
    } else if (!recordActive && loop->lastRecord) {
        // Knob turned down - stop recording, save loop length
        if (loop->isRecording) {
            // Quantize end to beat boundary AFTER last recorded event
            // This ensures we don't cut off the last note
            loop->loopLength = ((loop->lastPulseWithEvent + pulsesPerBeat) / pulsesPerBeat) * pulsesPerBeat;
        }
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
        if (loop->loopLength > 0 && !loop->isRecording) {
            loop->isPlaying = true;
            loop->currentPulse = 0;
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
            
            if (loop->isRecording) {
                // Already recording - increment pulse counter
                loop->currentPulse++;
                if (loop->currentPulse >= MAX_LOOP_LENGTH) {
                    loop->currentPulse = MAX_LOOP_LENGTH - 1;
                }
                
                // Check for auto-stop
                if (autoStop > 0) {
                    uint16_t pulsesRecorded = loop->currentPulse - loop->recordStartPulse;
                    uint16_t targetPulses = autoStop * pulsesPerBeat;  // beats * pulses per beat
                    
                    if (pulsesRecorded >= targetPulses) {
                        // Quantize end to beat boundary AFTER last recorded event
                        loop->loopLength = ((loop->lastPulseWithEvent + pulsesPerBeat) / pulsesPerBeat) * pulsesPerBeat;
                        loop->isRecording = false;
                        loop->recordArmed = false;
                        
                        // Auto-start playback
                        loop->isPlaying = true;
                        loop->currentPulse = 0;
                    }
                }
            } else if (loop->recordArmed) {
                // Armed but not recording - stay at pulse 0, waiting for first MIDI
                // Do nothing, let midiMessage() start the recording
            } else if (loop->isPlaying && loop->loopLength > 0) {
                // Send ALL events at current pulse
                if (loop->currentPulse < loop->loopLength && loop->currentPulse < MAX_LOOP_LENGTH) {
                    EventBucket& bucket = loop->eventBuckets[loop->currentPulse];
                    
                    // Send all events in this bucket
                    for (uint8_t i = 0; i < bucket.count; i++) {
#if VLOOP_DEBUG
                        loop->lastPulseWithMidi = loop->currentPulse;
                        loop->lastMidiStatus = bucket.events[i].data[0];
                        loop->lastMidiData1 = bucket.events[i].data[1];
                        loop->lastMidiData2 = bucket.events[i].data[2];
                        loop->totalMidiSent++;
                        
                        // Count sent notes
                        uint8_t msgType = bucket.events[i].data[0] & 0xF0;
                        if (msgType == 0x90 && bucket.events[i].data[2] > 0) {  // Note on
                            loop->noteOnSent++;
                        } else if (msgType == 0x80 || (msgType == 0x90 && bucket.events[i].data[2] == 0)) {  // Note off
                            loop->noteOffSent++;
                        }
#endif
                        
                        // Get output channel parameter (1-16, convert to 0-15)
                        uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
                        
                        // Remap to output channel (preserve original message type, replace channel)
                        uint8_t statusByte = bucket.events[i].data[0];
                        uint8_t messageType = statusByte & 0xF0;  // Extract message type
                        uint8_t remappedStatus = messageType | outputChannel;  // Apply output channel
                        
                        // Send only to Internal (for routing to other slots)
                        NT_sendMidi3ByteMessage(
                            kNT_destinationInternal,
                            remappedStatus,  // Use remapped channel
                            bucket.events[i].data[1],
                            bucket.events[i].data[2]
                        );
                    }
                }
                
                // Advance to next pulse
                loop->currentPulse++;
                if (loop->currentPulse >= loop->loopLength) {
                    // Loop wrap - send all-notes-off to prevent stuck notes
                    // Send on output channel only
                    uint8_t outputChannel = ((int)loop->v[kParamMidiOutChannel] - 1) & 0x0F;
                    NT_sendMidi3ByteMessage(
                        kNT_destinationInternal,
                        0xB0 | outputChannel,  // CC on output channel
                        123,                    // All Notes Off
                        0
                    );
                    loop->currentPulse = 0; // Loop wrap
                }
            }
        }
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
        loop->currentPulse = 0;
        loop->recordStartPulse = 0;
        // First event goes to pulse 0
    }
    
    // Only record when in recording mode
    if (!loop->isRecording) {
#if VLOOP_DEBUG
        loop->midiSkippedNotRecording++;
#endif
        return;
    }
    
    // Apply quantization - quantize to NEAREST quantized pulse
    int quantize = (int)loop->v[kParamQuantize];
    int quantizeDivisor = 1 << quantize;  // 1, 2, or 4
    
    // Round to nearest quantized pulse instead of skipping
    uint16_t quantizedPulse = ((loop->currentPulse + (quantizeDivisor / 2)) / quantizeDivisor) * quantizeDivisor;
    
    // Ensure we don't go past max length
    if (quantizedPulse >= MAX_LOOP_LENGTH) {
        quantizedPulse = MAX_LOOP_LENGTH - 1;
    }
    
    // Add event to quantized pulse's bucket
    if (quantizedPulse < MAX_LOOP_LENGTH) {
        MidiEvent event;
        event.data[0] = byte0;
        event.data[1] = byte1;
        event.data[2] = byte2;
        bool added = loop->eventBuckets[quantizedPulse].add(event);
        
        if (added) {
            // Track last pulse with recorded event
            loop->lastPulseWithEvent = quantizedPulse;
            
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
    stream.addMemberName("loopLength");
    stream.addNumber((int)loop->loopLength);
    
    stream.addMemberName("currentPulse");
    stream.addNumber((int)loop->currentPulse);
    
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
    loop->loopLength = 0;
    loop->currentPulse = 0;
    loop->isRecording = false;
    loop->isPlaying = false;
    
    // Load the data
    int num;
    if (parse.matchName("loopLength")) {
        parse.number(num);
        loop->loopLength = num;
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
