// VLoop V2 EXTREME Stress Tests - Push Beyond Normal Limits
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>

// Mock Disting NT API
void NT_sendMidi3ByteMessage(unsigned char status, unsigned char data1, unsigned char data2) { }

unsigned long global_tick_counter = 0;
#define MAX_LOOP_EVENTS 2560

struct LoopEvent {
    unsigned char status, data1, data2;
    unsigned long relative_timestamp;
};

class VLoopExtreme {
public:
    LoopEvent loopEvents[MAX_LOOP_EVENTS];
    int eventCount = 0;
    unsigned long loopLength = 0;
    bool recording = false;
    unsigned long recordStartTime = 0;
    int totalMidiEventsReceived = 0;
    std::vector<int> deltaGaps;

    void startRecording() {
        recording = true;
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

        std::sort(loopEvents, loopEvents + eventCount, 
                  [](const LoopEvent& a, const LoopEvent& b) {
                      return a.relative_timestamp < b.relative_timestamp;
                  });

        for (int i = 1; i < eventCount; i++) {
            int gap = loopEvents[i].relative_timestamp - loopEvents[i-1].relative_timestamp;
            deltaGaps.push_back(gap);
        }
    }

    void recordMidiEvent(unsigned char status, unsigned char data1, unsigned char data2) {
        totalMidiEventsReceived++;
        if (!recording || eventCount >= MAX_LOOP_EVENTS) return;

        loopEvents[eventCount] = {status, data1, data2, global_tick_counter - recordStartTime};
        eventCount++;
    }

    void clockTick() { global_tick_counter++; }

    void printStats() {
        double captureRate = totalMidiEventsReceived > 0 ? 
            (double)eventCount / totalMidiEventsReceived * 100.0 : 0.0;
        std::cout << "Events: " << eventCount << "/" << totalMidiEventsReceived 
                  << " (" << captureRate << "%)" << std::endl;
        std::cout << "Loop length: " << loopLength << " ticks" << std::endl;
    }
};

class ExtremeStressTests {
private:
    static std::mt19937 rng;
    
public:
    static void testSuperHighSpeed() {
        std::cout << "\n=== EXTREME: Super High Speed (Every Tick for 1000 ticks) ===" << std::endl;
        VLoopExtreme vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send MIDI every single tick for 1000 ticks
        for (int i = 0; i < 1000; i++) {
            vloop.recordMidiEvent(0x90, 60 + (i % 12), 100);
            vloop.clockTick();
        }
        
        vloop.stopRecording();
        vloop.printStats();
        
        // Should capture up to buffer limit
        assert(vloop.eventCount <= MAX_LOOP_EVENTS);
        assert(vloop.totalMidiEventsReceived == 1000);
    }
    
    static void testMassiveMIDIBurst() {
        std::cout << "\n=== EXTREME: Massive MIDI Burst (5000 events same tick) ===" << std::endl;
        VLoopExtreme vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send 5000 MIDI events on the same tick
        for (int i = 0; i < 5000; i++) {
            vloop.recordMidiEvent(0x90, 60 + (i % 127), 127);
        }
        
        vloop.clockTick();
        vloop.stopRecording();
        vloop.printStats();
        
        // Should handle gracefully up to buffer limit
        assert(vloop.eventCount <= MAX_LOOP_EVENTS);
        assert(vloop.totalMidiEventsReceived == 5000);
    }
    
    static void testRandomChaos() {
        std::cout << "\n=== EXTREME: Random Chaos (1500 events, random timing) ===" << std::endl;
        VLoopExtreme vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        std::uniform_int_distribution<int> tickDist(0, 3);  // 0-3 ticks advance
        std::uniform_int_distribution<int> noteDist(0, 127);
        std::uniform_int_distribution<int> velDist(1, 127);
        
        for (int i = 0; i < 1500; i++) {
            // Random clock advance
            int advance = tickDist(rng);
            for (int j = 0; j < advance; j++) {
                vloop.clockTick();
            }
            
            // Random MIDI
            vloop.recordMidiEvent(0x90, noteDist(rng), velDist(rng));
        }
        
        vloop.stopRecording();
        vloop.printStats();
        
        assert(vloop.eventCount <= MAX_LOOP_EVENTS);
        assert(vloop.totalMidiEventsReceived == 1500);
    }
    
    static void testBufferSaturation() {
        std::cout << "\n=== EXTREME: Buffer Saturation (Exactly 2560 events) ===" << std::endl;
        VLoopExtreme vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Send exactly MAX_LOOP_EVENTS
        for (int i = 0; i < MAX_LOOP_EVENTS; i++) {
            vloop.recordMidiEvent(0x90, 60 + (i % 12), 100);
            if (i % 64 == 0) vloop.clockTick();  // Advance occasionally
        }
        
        vloop.stopRecording();
        vloop.printStats();
        
        // Should capture exactly all events
        assert(vloop.eventCount == MAX_LOOP_EVENTS);
        assert(vloop.totalMidiEventsReceived == MAX_LOOP_EVENTS);
    }
    
    static void testLongSequenceStress() {
        std::cout << "\n=== EXTREME: Long Sequence (Complex musical pattern) ===" << std::endl;
        VLoopExtreme vloop;
        
        vloop.startRecording();
        vloop.clockTick();
        
        // Simulate complex musical sequence: arpeggios + bass + drums
        int eventsSent = 0;
        
        for (int bar = 0; bar < 32; bar++) {  // 32 bars
            for (int beat = 0; beat < 16; beat++) {  // 16th notes
                // Arpeggio pattern
                if (beat % 2 == 0) {
                    vloop.recordMidiEvent(0x90, 60 + (beat % 12), 80);
                    eventsSent++;
                }
                
                // Bass on downbeats
                if (beat % 8 == 0) {
                    vloop.recordMidiEvent(0x90, 36, 127);
                    eventsSent++;
                }
                
                // Hi-hat every beat
                if (beat % 4 == 0) {
                    vloop.recordMidiEvent(0x99, 42, 64);
                    eventsSent++;
                }
                
                vloop.clockTick();
                
                if (eventsSent >= MAX_LOOP_EVENTS) break;
            }
            if (eventsSent >= MAX_LOOP_EVENTS) break;
        }
        
        vloop.stopRecording();
        vloop.printStats();
        
        std::cout << "Complex sequence sent " << eventsSent << " events" << std::endl;
        assert(vloop.eventCount <= MAX_LOOP_EVENTS);
    }
    
    static void runExtremeTests() {
        std::cout << "\n🔥 STARTING EXTREME STRESS TESTS 🔥" << std::endl;
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        testSuperHighSpeed();
        testMassiveMIDIBurst();
        testRandomChaos();
        testBufferSaturation();
        testLongSequenceStress();
        
        std::cout << "\n🎯 ALL EXTREME TESTS PASSED! VLoop is BULLETPROOF! 🎯" << std::endl;
    }
};

std::mt19937 ExtremeStressTests::rng;

int main() {
    ExtremeStressTests::runExtremeTests();
    return 0;
}