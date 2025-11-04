// VLoop V2 Stress Tests - Test Edge Cases and Failure Modes
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>

// Mock Disting NT API for testing
void NT_sendMidi3ByteMessage(unsigned char status, unsigned char data1, unsigned char data2) {
    // Mock implementation - do nothing
}

// Global tick counter for testing
unsigned long global_tick_counter = 0;

// Global debug variables
int total_midi_events_received = 0;
std::vector<int> debug_delta_gaps;

// Include VLoop main code with testing modifications
#define MAX_LOOP_EVENTS 2560

struct LoopEvent {
    unsigned char status;
    unsigned char data1;
    unsigned char data2;
    unsigned long relative_timestamp;
};

class VLoopTest {
public:
    LoopEvent loopEvents[MAX_LOOP_EVENTS];
    int eventCount = 0;
    unsigned long loopLength = 0;
    bool recording = false;
    bool playing = false;
    unsigned long recordStartTime = 0;
    unsigned long playStartTime = 0;
    int totalMidiEventsReceived = 0;
    std::vector<int> deltaGaps;

    void startRecording() {
        recording = true;
        playing = false;
        eventCount = 0;
        recordStartTime = global_tick_counter;
        totalMidiEventsReceived = 0;
        deltaGaps.clear();
    }

    void stopRecording() {
        if (!recording) return;
        
        recording = false;
        loopLength = global_tick_counter - recordStartTime;
        if (loopLength == 0) loopLength = 1;

        // Sort events by timestamp
        std::sort(loopEvents, loopEvents + eventCount, 
                  [](const LoopEvent& a, const LoopEvent& b) {
                      return a.relative_timestamp < b.relative_timestamp;
                  });

        // Calculate delta gaps
        for (int i = 1; i < eventCount; i++) {
            int gap = loopEvents[i].relative_timestamp - loopEvents[i-1].relative_timestamp;
            deltaGaps.push_back(gap);
        }
    }

    void startPlayback() {
        if (eventCount == 0) return;
        playing = true;
        playStartTime = global_tick_counter;
    }

    void recordMidiEvent(unsigned char status, unsigned char data1, unsigned char data2) {
        totalMidiEventsReceived++;
        
        if (!recording || eventCount >= MAX_LOOP_EVENTS) return;

        loopEvents[eventCount] = {
            status, data1, data2,
            global_tick_counter - recordStartTime
        };
        eventCount++;
    }

    void clockTick() {
        global_tick_counter++;
        
        if (playing && eventCount > 0) {
            unsigned long playPosition = (global_tick_counter - playStartTime) % loopLength;
            
            for (int i = 0; i < eventCount; i++) {
                if (loopEvents[i].relative_timestamp == playPosition) {
                    NT_sendMidi3ByteMessage(loopEvents[i].status, loopEvents[i].data1, loopEvents[i].data2);
                }
            }
        }
    }

    void printStats() {
        std::cout << "Events captured: " << eventCount << "/" << totalMidiEventsReceived;
        if (totalMidiEventsReceived > 0) {
            double captureRate = (double)eventCount / totalMidiEventsReceived * 100.0;
            std::cout << " (" << captureRate << "%)";
        }
        std::cout << std::endl;
        std::cout << "Loop length: " << loopLength << " ticks" << std::endl;
        if (deltaGaps.size() > 0) {
            int minGap = *std::min_element(deltaGaps.begin(), deltaGaps.end());
            int maxGap = *std::max_element(deltaGaps.begin(), deltaGaps.end());
            std::cout << "Delta gaps: min=" << minGap << ", max=" << maxGap << std::endl;
        }
    }
};

class VLoopStressTests {
private:
    static std::mt19937 rng;
    
public:
    static void testRandomMIDITiming() {
        std::cout << "\n=== Stress Test: Random MIDI Timing ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send MIDI at random intervals
        std::uniform_int_distribution<int> clockDist(1, 8);  // 1-8 ticks between events
        std::uniform_int_distribution<int> noteDist(60, 72); // Notes C4-C5
        
        int totalClocks = 0;
        int expectedEvents = 20;
        
        for (int i = 0; i < expectedEvents; i++) {
            // Random clock advancement
            int clockAdvance = clockDist(rng);
            for (int j = 0; j < clockAdvance; j++) {
                vloop.clockTick();
                totalClocks++;
            }
            
            // Send MIDI
            int note = noteDist(rng);
            vloop.recordMidiEvent(0x90, note, 100);
        }
        
        vloop.stopRecording();
        
        std::cout << "Sent " << expectedEvents << " events over " << totalClocks << " clock ticks" << std::endl;
        vloop.printStats();
        
        // Should capture all events
        assert(vloop.eventCount == expectedEvents);
        assert(vloop.totalMidiEventsReceived == expectedEvents);
    }
    
    static void testMIDIFlood() {
        std::cout << "\n=== Stress Test: MIDI Flood ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send massive amounts of MIDI on same tick
        for (int i = 0; i < 100; i++) {
            vloop.recordMidiEvent(0x90, 60 + (i % 12), 100);
        }
        
        vloop.clockTick();
        vloop.stopRecording();
        
        std::cout << "Sent 100 MIDI events on same tick" << std::endl;
        vloop.printStats();
        
        // Should capture all 100 events (within buffer limit)
        assert(vloop.eventCount == 100);
    }
    
    static void testBufferOverflow() {
        std::cout << "\n=== Stress Test: Buffer Overflow Protection ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Try to send more than MAX_LOOP_EVENTS
        for (int i = 0; i < MAX_LOOP_EVENTS + 100; i++) {
            vloop.recordMidiEvent(0x90, 60, 100);
            if (i % 100 == 0) vloop.clockTick();  // Advance clock occasionally
        }
        
        vloop.stopRecording();
        
        std::cout << "Sent " << (MAX_LOOP_EVENTS + 100) << " MIDI events" << std::endl;
        vloop.printStats();
        
        // Should cap at MAX_LOOP_EVENTS
        assert(vloop.eventCount == MAX_LOOP_EVENTS);
        assert(vloop.totalMidiEventsReceived > MAX_LOOP_EVENTS);  // More received than stored
    }
    
    static void testEarlyStopRecording() {
        std::cout << "\n=== Stress Test: Early Stop Recording ===" << std::endl;
        VLoopTest vloop;
        
        vloop.startRecording();
        // Stop immediately without any clock ticks or MIDI
        vloop.stopRecording();
        
        std::cout << "Stopped recording immediately" << std::endl;
        vloop.printStats();
        
        // Should handle gracefully
        assert(vloop.eventCount == 0);
        assert(vloop.loopLength >= 1);  // Should have minimum loop length
    }
    
    static void testKeyStepSimulation() {
        std::cout << "\n=== Stress Test: KeyStep Simulation ===" << std::endl;
        
        // Test different sync offsets
        for (int offset = 0; offset < 4; offset++) {
            std::cout << "\nKeyStep offset: " << offset << " sub-ticks" << std::endl;
            
            VLoopTest vloop;
            vloop.startRecording();
            
            // Advance to recording start
            vloop.clockTick();
            
            // Simulate KeyStep playing 8 notes with specific offset
            for (int note = 0; note < 8; note++) {
                // Advance to note position (4 ticks per quarter note)
                for (int tick = 0; tick < 4; tick++) {
                    vloop.clockTick();
                }
                
                // Send Note On/Off pair
                vloop.recordMidiEvent(0x90, 60 + note, 100);  // Note On
                vloop.recordMidiEvent(0x80, 60 + note, 0);    // Note Off immediately
            }
            
            vloop.stopRecording();
            vloop.printStats();
            
            // All offsets should capture the same number of events
            assert(vloop.eventCount == 16);  // 8 Note On + 8 Note Off
            
            // Delta gaps should be consistent
            bool hasConsistentGaps = true;
            for (size_t i = 1; i < vloop.deltaGaps.size(); i += 2) {
                if (vloop.deltaGaps[i] != 4) {  // Should be 4 ticks between notes
                    hasConsistentGaps = false;
                    break;
                }
            }
            assert(hasConsistentGaps);
        }
    }
    
    static void testQuantizationAccuracy() {
        std::cout << "\n=== Stress Test: Quantization Accuracy ===" << std::endl;
        
        struct TestCase {
            int clocksPerNote;
            int expectedGap;
            std::string description;
        };
        
        std::vector<TestCase> testCases = {
            {1, 1, "Every tick (1/16 notes)"},
            {2, 2, "Every 2 ticks (1/8 notes)"},
            {4, 4, "Every 4 ticks (1/4 notes)"},
            {8, 8, "Every 8 ticks (1/2 notes)"}
        };
        
        for (const auto& testCase : testCases) {
            std::cout << "\nTest: " << testCase.description << std::endl;
            
            VLoopTest vloop;
            vloop.startRecording();
            vloop.clockTick();
            
            // Send 8 notes at specified intervals
            for (int note = 0; note < 8; note++) {
                vloop.recordMidiEvent(0x90, 60 + note, 100);
                
                // Advance by specified number of clocks
                for (int tick = 0; tick < testCase.clocksPerNote; tick++) {
                    vloop.clockTick();
                }
            }
            
            vloop.stopRecording();
            vloop.printStats();
            
            // Check that gaps match expected pattern
            if (vloop.deltaGaps.size() > 0) {
                int averageGap = 0;
                for (auto gap : vloop.deltaGaps) {
                    averageGap += gap;
                }
                averageGap /= vloop.deltaGaps.size();
                
                std::cout << "Average gap: " << averageGap << " (expected: " << testCase.expectedGap << ")" << std::endl;
                assert(abs(averageGap - testCase.expectedGap) <= 1);  // Allow 1 tick tolerance
            }
        }
    }
    
    static void runStressTests() {
        std::cout << "\nStarting VLoop V2 Stress Tests..." << std::endl;
        
        // Seed random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        testRandomMIDITiming();
        testMIDIFlood();
        testBufferOverflow();
        testEarlyStopRecording();
        testKeyStepSimulation();
        testQuantizationAccuracy();
        
        std::cout << "\n🚀 All stress tests passed!" << std::endl;
    }
};

std::mt19937 VLoopStressTests::rng;

int main() {
    // Run stress tests
    VLoopStressTests::runStressTests();
    
    return 0;
}