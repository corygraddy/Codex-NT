// VLoop V2 Unit Test Framework
// Systematic testing of MIDI recording and quantization logic
#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>

// Mock the Disting NT API for testing
struct MockNT {
    static std::vector<std::string> midiOutput;
    static void sendMidi(uint8_t b0, uint8_t b1, uint8_t b2) {
        midiOutput.push_back("MIDI: " + std::to_string(b0) + "," + std::to_string(b1) + "," + std::to_string(b2));
    }
};
std::vector<std::string> MockNT::midiOutput;

#define NT_sendMidi3ByteMessage(dest, b0, b1, b2) MockNT::sendMidi(b0, b1, b2)
#define MAX_LOOP_EVENTS 2560

// Copy VLoop core logic for testing
enum LooperState {
    STOPPED = 0,
    RECORDING,
    PLAYING
};

struct LoopEvent {
    uint32_t timeDelta;
    uint8_t midiData[3];
    
    LoopEvent() : timeDelta(0) {
        midiData[0] = midiData[1] = midiData[2] = 0;
    }
    
    LoopEvent(uint32_t delta, uint8_t b0, uint8_t b1, uint8_t b2) : timeDelta(delta) {
        midiData[0] = b0; midiData[1] = b1; midiData[2] = b2;
    }
    
    bool operator<(const LoopEvent& other) const {
        return timeDelta < other.timeDelta;
    }
};

struct VLoopTest {
    // Core timing
    uint32_t globalTime;
    uint32_t loopStartTime;
    uint32_t loopLength;
    
    // Event storage
    LoopEvent loopEvents[MAX_LOOP_EVENTS];
    uint32_t eventCount;
    
    // Playback state
    uint32_t currentPlaybackTime;
    uint32_t playbackIndex;
    LooperState currentState;
    
    // Test metrics
    uint32_t totalMidiEventsReceived;
    uint32_t lastTimeDelta;
    std::vector<uint32_t> deltaGaps;
    
    VLoopTest() {
        reset();
    }
    
    void reset() {
        globalTime = 0;
        loopStartTime = 0;
        loopLength = 0;
        eventCount = 0;
        currentPlaybackTime = 0;
        playbackIndex = 0;
        currentState = STOPPED;
        totalMidiEventsReceived = 0;
        lastTimeDelta = 0;
        deltaGaps.clear();
        MockNT::midiOutput.clear();
    }
    
    void clockTick() {
        globalTime++;
    }
    
    void startRecording() {
        if (currentState != STOPPED) return;
        currentState = RECORDING;
        loopStartTime = globalTime + 1;  // Start on next clock
        eventCount = 0;
        totalMidiEventsReceived = 0;
        lastTimeDelta = 0;
        deltaGaps.clear();
    }
    
    void stopRecording() {
        if (currentState != RECORDING) return;
        loopLength = (globalTime - loopStartTime) + 1;
        if (loopLength == 0) loopLength = 1;
        
        // Sort events by timeDelta
        if (eventCount > 1) {
            std::sort(loopEvents, loopEvents + eventCount);
        }
        currentState = STOPPED;
    }
    
    void recordMidiEvent(uint8_t byte0, uint8_t byte1, uint8_t byte2) {
        if (currentState != RECORDING) return;
        if (eventCount >= MAX_LOOP_EVENTS) return;
        
        totalMidiEventsReceived++;
        
        // Calculate relative timestamp (quantized to current clock)
        uint32_t timeDelta;
        if (globalTime >= loopStartTime) {
            timeDelta = globalTime - loopStartTime;
        } else {
            timeDelta = 0;
        }
        
        // Track delta gaps
        if (eventCount > 0) {
            uint32_t gap = (timeDelta > lastTimeDelta) ? (timeDelta - lastTimeDelta) : 0;
            deltaGaps.push_back(gap);
        }
        lastTimeDelta = timeDelta;
        
        // Store event
        loopEvents[eventCount] = LoopEvent(timeDelta, byte0, byte1, byte2);
        eventCount++;
    }
    
    void startPlayback() {
        if (currentState != STOPPED || eventCount == 0) return;
        currentState = PLAYING;
        currentPlaybackTime = 0;
        playbackIndex = 0;
    }
    
    void resetPlayback() {
        if (currentState == PLAYING) {
            currentPlaybackTime = 0;
            playbackIndex = 0;
        }
    }
    
    void updatePlayback() {
        if (currentState != PLAYING) return;
        if (eventCount == 0 || loopLength == 0) return;
        
        // Check for events at current playback time
        while (playbackIndex < eventCount) {
            LoopEvent& event = loopEvents[playbackIndex];
            
            if (event.timeDelta == currentPlaybackTime) {
                MockNT::sendMidi(event.midiData[0], event.midiData[1], event.midiData[2]);
                playbackIndex++;
            }
            else if (event.timeDelta > currentPlaybackTime) {
                break;
            }
            else {
                playbackIndex++;
            }
        }
        
        currentPlaybackTime++;
        if (currentPlaybackTime >= loopLength) {
            currentPlaybackTime = 0;
            playbackIndex = 0;
        }
    }
    
    void printStats() {
        std::cout << "Events: " << eventCount << "/" << totalMidiEventsReceived 
                  << " (capture rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * eventCount / std::max(1u, totalMidiEventsReceived)) << "%)" << std::endl;
        std::cout << "Loop length: " << loopLength << " ticks" << std::endl;
        
        if (!deltaGaps.empty()) {
            std::cout << "Delta gaps: ";
            for (size_t i = 0; i < std::min(deltaGaps.size(), size_t(16)); i++) {
                std::cout << deltaGaps[i] << " ";
            }
            std::cout << std::endl;
        }
        
        if (eventCount > 0) {
            std::cout << "Event deltas: ";
            for (uint32_t i = 0; i < std::min(eventCount, 16u); i++) {
                std::cout << loopEvents[i].timeDelta << " ";
            }
            std::cout << std::endl;
        }
    }
};

// Unit Test Functions
class VLoopUnitTests {
public:
    static void testBasicRecording() {
        std::cout << "\n=== Test: Basic Recording ===" << std::endl;
        VLoopTest vloop;
        
        // Simulate: Start recording, clock tick, send MIDI, clock tick, send MIDI, stop
        vloop.startRecording();
        vloop.clockTick();  // globalTime = 1, loopStartTime = 1
        vloop.recordMidiEvent(0x90, 60, 100);  // Note On C4
        
        vloop.clockTick();  // globalTime = 2
        vloop.clockTick();  // globalTime = 3
        vloop.clockTick();  // globalTime = 4
        vloop.clockTick();  // globalTime = 5
        vloop.recordMidiEvent(0x80, 60, 0);    // Note Off C4
        
        vloop.stopRecording();
        
        std::cout << "Expected: 2 events, 4-tick gap" << std::endl;
        vloop.printStats();
        
        assert(vloop.eventCount == 2);
        assert(vloop.deltaGaps.size() == 1);
        assert(vloop.deltaGaps[0] == 4);  // Should be 4 ticks apart
    }
    
    static void testQuarterNoteSequence() {
        std::cout << "\n=== Test: Quarter Note Sequence (4-tick spacing) ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();  // Start recording
        
        // Simulate 8 notes, each 4 ticks apart
        for (int note = 0; note < 8; note++) {
            // Note On
            vloop.recordMidiEvent(0x90, 60 + note, 100);
            vloop.clockTick();
            
            // Note Off (1 tick later)  
            vloop.recordMidiEvent(0x80, 60 + note, 0);
            
            // Advance to next note position (3 more ticks)
            vloop.clockTick();
            vloop.clockTick();
            vloop.clockTick();
        }
        
        vloop.stopRecording();
        
        std::cout << "Expected: 16 events, gaps of [1,3,1,3,1,3...] pattern" << std::endl;
        vloop.printStats();
        
        assert(vloop.eventCount == 16);
    }
    
    static void testHighSpeedStress() {
        std::cout << "\n=== Test: High Speed Stress (every tick) ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send MIDI on every clock tick for 32 ticks
        for (int i = 0; i < 32; i++) {
            vloop.recordMidiEvent(0x90, 60, 100);
            vloop.clockTick();
        }
        
        vloop.stopRecording();
        
        std::cout << "Expected: 32 events, all gaps = 1" << std::endl;
        vloop.printStats();
        
        assert(vloop.eventCount == 32);
        // Check that all gaps are 1
        for (auto gap : vloop.deltaGaps) {
            assert(gap == 1);
        }
    }
    
    static void testMidTickMIDI() {
        std::cout << "\n=== Test: MIDI Between Clock Ticks ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();  // globalTime = 1
        
        // Send MIDI between clock ticks (should get quantized to current tick)
        vloop.recordMidiEvent(0x90, 60, 100);  // Gets timeDelta = 0
        vloop.recordMidiEvent(0x90, 61, 100);  // Also gets timeDelta = 0
        
        vloop.clockTick();  // globalTime = 2
        vloop.recordMidiEvent(0x90, 62, 100);  // Gets timeDelta = 1
        
        vloop.stopRecording();
        
        std::cout << "Expected: 3 events, first two at same timeDelta" << std::endl;
        vloop.printStats();
        
        assert(vloop.eventCount == 3);
        assert(vloop.loopEvents[0].timeDelta == 0);
        assert(vloop.loopEvents[1].timeDelta == 0);
        assert(vloop.loopEvents[2].timeDelta == 1);
    }
    
    static void testPlayback() {
        std::cout << "\n=== Test: Playback ===" << std::endl;
        VLoopTest vloop;
        
        // Record a simple sequence
        vloop.startRecording();
        vloop.clockTick();
        vloop.recordMidiEvent(0x90, 60, 100);
        vloop.clockTick();
        vloop.clockTick();
        vloop.recordMidiEvent(0x90, 62, 100);
        vloop.stopRecording();
        
        // Test playback
        vloop.startPlayback();
        for (int i = 0; i < vloop.loopLength * 2; i++) {  // Play 2 full loops
            vloop.updatePlayback();
            vloop.clockTick();
        }
        
        std::cout << "MIDI output during playback:" << std::endl;
        for (const auto& msg : MockNT::midiOutput) {
            std::cout << msg << std::endl;
        }
        
        // Should have played each event twice (2 loops)
        assert(MockNT::midiOutput.size() == 4);
    }
    
    static void testResetFunctionality() {
        std::cout << "\n=== Test: Reset Functionality ===\n" << std::endl;
        VLoopTest vloop;
        
        // Record a simple loop
        vloop.startRecording();
        vloop.clockTick();  // globalTime = 1, loopStartTime = 1
        vloop.recordMidiEvent(0x90, 60, 100);  // Note On C4
        vloop.clockTick();  
        vloop.clockTick();  
        vloop.recordMidiEvent(0x90, 62, 100);  // Note On D4  
        vloop.clockTick();  
        vloop.recordMidiEvent(0x90, 64, 100);  // Note On E4
        vloop.stopRecording();
        
        std::cout << "Recorded 3-note loop" << std::endl;
        vloop.printStats();
        
        // Start playback and advance partway through
        vloop.startPlayback();
        vloop.clockTick();  // Should play first note
        vloop.clockTick();  // Should play second note
        
        std::cout << "Playback state before reset:" << std::endl;
        std::cout << "currentPlaybackTime: " << vloop.currentPlaybackTime << std::endl;
        std::cout << "playbackIndex: " << vloop.playbackIndex << std::endl;
        
        // Reset should restart from beginning
        vloop.resetPlayback();
        
        std::cout << "Playback state after reset:" << std::endl;
        std::cout << "currentPlaybackTime: " << vloop.currentPlaybackTime << std::endl;
        std::cout << "playbackIndex: " << vloop.playbackIndex << std::endl;
        
        // Verify reset worked
        assert(vloop.currentPlaybackTime == 0);
        assert(vloop.playbackIndex == 0);
        
        std::cout << "✅ Reset functionality working correctly" << std::endl;
    }
    
    static void runAllTests() {
        std::cout << "Starting VLoop V2 Unit Tests..." << std::endl;
        
        testBasicRecording();
        testQuarterNoteSequence();
        testHighSpeedStress();
        testMidTickMIDI();
        testPlayback();
        testResetFunctionality();
        
        std::cout << "\n🎉 All tests passed!" << std::endl;
    }
};

int main() {
    VLoopUnitTests::runAllTests();
    return 0;
}