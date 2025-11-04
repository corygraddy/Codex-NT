#include <gtest/gtest.h>
#include <vector>
#include <iostream>

// Mock structures for testing VLoop timing
struct MockMidiEvent {
    uint32_t timestamp;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    
    MockMidiEvent(uint32_t ts, uint8_t s, uint8_t d1, uint8_t d2) 
        : timestamp(ts), status(s), data1(d1), data2(d2) {}
};

struct MockVLoop {
    std::vector<MockMidiEvent> recordedEvents;
    uint32_t currentClockTick = 0;
    uint32_t currentSubTick = 0;
    uint32_t loopStartTick = 0;
    uint32_t loopLengthTicks = 0;
    uint32_t playbackTick = 0;
    uint32_t playbackSubTick = 0;
    uint32_t playbackEventIndex = 0;
    uint32_t precision = 4;
    bool isRecording = false;
    bool isPlaying = false;
    
    std::vector<MockMidiEvent> playbackOutput;
    std::vector<uint32_t> playbackTimestamps;
    
    void advanceTime() {
        currentSubTick++;
        if (currentSubTick >= precision) {
            currentSubTick = 0;
        }
        
        if (isPlaying) {
            playbackSubTick++;
            if (playbackSubTick >= precision) {
                playbackSubTick = 0;
            }
        }
    }
    
    void clockTick() {
        currentClockTick++;
        currentSubTick = 0;
        
        if (isPlaying) {
            playbackTick++;
            playbackSubTick = 0;
        }
    }
    
    void startRecording() {
        isRecording = true;
        loopStartTick = currentClockTick;
        recordedEvents.clear();
    }
    
    void recordMidiEvent(uint8_t status, uint8_t data1, uint8_t data2) {
        if (isRecording) {
            uint32_t timestamp = (currentClockTick - loopStartTick) * precision + currentSubTick;
            recordedEvents.push_back(MockMidiEvent(timestamp, status, data1, data2));
        }
    }
    
    void stopRecording() {
        isRecording = false;
        loopLengthTicks = currentClockTick - loopStartTick;
    }
    
    void startPlayback() {
        if (!recordedEvents.empty()) {
            isPlaying = true;
            // Start from first event's timestamp
            uint32_t firstEventTimestamp = recordedEvents[0].timestamp;
            playbackTick = firstEventTimestamp / precision;
            playbackSubTick = firstEventTimestamp % precision;
            playbackEventIndex = 0;
            playbackOutput.clear();
            playbackTimestamps.clear();
        }
    }
    
    void processPlayback() {
        if (isPlaying && !recordedEvents.empty()) {
            uint32_t currentPlaybackTimestamp = playbackTick * precision + playbackSubTick;
            
            while (playbackEventIndex < recordedEvents.size()) {
                MockMidiEvent& event = recordedEvents[playbackEventIndex];
                if (event.timestamp <= currentPlaybackTimestamp) {
                    // Record when this event was played back
                    playbackOutput.push_back(event);
                    playbackTimestamps.push_back(currentPlaybackTimestamp);
                    playbackEventIndex++;
                } else {
                    break;
                }
            }
            
            // Loop restart logic
            if (playbackTick >= loopLengthTicks) {
                uint32_t firstEventTimestamp = recordedEvents[0].timestamp;
                playbackTick = firstEventTimestamp / precision;
                playbackSubTick = firstEventTimestamp % precision;
                playbackEventIndex = 0;
            }
        }
    }
    
    void stopPlayback() {
        isPlaying = false;
    }
    
    void printTimingAnalysis() {
        std::cout << "\n=== TIMING ANALYSIS ===" << std::endl;
        std::cout << "Precision: " << precision << std::endl;
        std::cout << "Loop Length: " << loopLengthTicks << " ticks" << std::endl;
        
        std::cout << "\nRecorded Events:" << std::endl;
        for (size_t i = 0; i < recordedEvents.size(); i++) {
            auto& event = recordedEvents[i];
            uint32_t clockTick = event.timestamp / precision;
            uint32_t subTick = event.timestamp % precision;
            std::cout << "  Event " << i << ": timestamp=" << event.timestamp 
                      << " (tick:" << clockTick << ", sub:" << subTick << ")" << std::endl;
        }
        
        std::cout << "\nPlayback Events:" << std::endl;
        for (size_t i = 0; i < playbackOutput.size(); i++) {
            auto& event = playbackOutput[i];
            uint32_t playbackTime = playbackTimestamps[i];
            uint32_t clockTick = playbackTime / precision;
            uint32_t subTick = playbackTime % precision;
            std::cout << "  Event " << i << ": played at timestamp=" << playbackTime
                      << " (tick:" << clockTick << ", sub:" << subTick << ")"
                      << " original=" << event.timestamp << std::endl;
        }
        
        // Calculate timing differences
        std::cout << "\nTiming Differences:" << std::endl;
        for (size_t i = 0; i < std::min(recordedEvents.size(), playbackOutput.size()); i++) {
            int32_t diff = (int32_t)playbackTimestamps[i] - (int32_t)recordedEvents[i].timestamp;
            std::cout << "  Event " << i << ": difference=" << diff << " sub-ticks" << std::endl;
        }
    }
};

class VLoopTimingTest : public ::testing::Test {
protected:
    MockVLoop vloop;
    
    void SetUp() override {
        vloop = MockVLoop();
    }
};

TEST_F(VLoopTimingTest, BasicRecordingTimestamps) {
    // Test: Record events at different times and verify timestamps
    
    // Start recording at tick 0
    vloop.startRecording();
    EXPECT_EQ(vloop.loopStartTick, 0);
    
    // Record event immediately (should be timestamp 0)
    vloop.recordMidiEvent(0x90, 60, 100);
    EXPECT_EQ(vloop.recordedEvents[0].timestamp, 0);
    
    // Advance 2 sub-ticks and record another event
    vloop.advanceTime();
    vloop.advanceTime();
    vloop.recordMidiEvent(0x90, 62, 100);
    EXPECT_EQ(vloop.recordedEvents[1].timestamp, 2);
    
    // Advance to next clock tick and record
    vloop.clockTick();
    vloop.recordMidiEvent(0x90, 64, 100);
    uint32_t expectedTimestamp = 1 * vloop.precision; // 4
    EXPECT_EQ(vloop.recordedEvents[2].timestamp, expectedTimestamp);
    
    vloop.stopRecording();
    EXPECT_EQ(vloop.loopLengthTicks, 1);
    
    vloop.printTimingAnalysis();
}

TEST_F(VLoopTimingTest, DelayedStartRecording) {
    // Test: Start recording but don't play immediately
    
    vloop.startRecording();
    
    // Advance time without recording (simulating delay before playing)
    vloop.advanceTime(); // sub-tick 1
    vloop.advanceTime(); // sub-tick 2
    vloop.clockTick();   // tick 1, sub-tick 0
    vloop.advanceTime(); // sub-tick 1
    
    // Now record first event
    vloop.recordMidiEvent(0x90, 60, 100);
    uint32_t expectedFirstTimestamp = 1 * vloop.precision + 1; // 5
    EXPECT_EQ(vloop.recordedEvents[0].timestamp, expectedFirstTimestamp);
    
    // Record another event shortly after
    vloop.advanceTime();
    vloop.recordMidiEvent(0x90, 62, 100);
    EXPECT_EQ(vloop.recordedEvents[1].timestamp, expectedFirstTimestamp + 1);
    
    vloop.clockTick(); // End recording at tick 2
    vloop.stopRecording();
    
    vloop.printTimingAnalysis();
}

TEST_F(VLoopTimingTest, PlaybackTimingAccuracy) {
    // Test: Record events and verify playback timing matches exactly
    
    // Record a pattern
    vloop.startRecording();
    vloop.recordMidiEvent(0x90, 60, 100); // timestamp 0
    
    vloop.advanceTime();
    vloop.advanceTime(); 
    vloop.recordMidiEvent(0x90, 62, 100); // timestamp 2
    
    vloop.clockTick();
    vloop.recordMidiEvent(0x90, 64, 100); // timestamp 4
    
    vloop.clockTick();
    vloop.stopRecording(); // Loop length = 2 ticks
    
    // Start playback
    vloop.startPlayback();
    
    // Simulate playback process
    for (int frame = 0; frame < 20; frame++) {
        vloop.processPlayback();
        vloop.advanceTime();
        if (frame % 4 == 3) { // Every 4 frames = 1 clock tick
            vloop.clockTick();
        }
    }
    
    vloop.printTimingAnalysis();
    
    // Verify all events were played back
    EXPECT_EQ(vloop.playbackOutput.size(), vloop.recordedEvents.size());
    
    // Verify timing accuracy (should be exact or very close)
    for (size_t i = 0; i < vloop.playbackOutput.size(); i++) {
        uint32_t recordedTime = vloop.recordedEvents[i].timestamp;
        uint32_t playbackTime = vloop.playbackTimestamps[i];
        int32_t difference = (int32_t)playbackTime - (int32_t)recordedTime;
        
        // Allow small timing tolerance
        EXPECT_LE(abs(difference), 1) << "Event " << i << " timing difference too large: " << difference;
    }
}

TEST_F(VLoopTimingTest, GapAtPlaybackStart) {
    // Test: Specifically check for gap at playback start
    
    vloop.startRecording();
    
    // Simulate delay before first event (the gap issue)
    for (int i = 0; i < 10; i++) {
        vloop.advanceTime();
    }
    vloop.clockTick(); // Now at tick 1
    
    // Record first event with some delay
    vloop.advanceTime();
    vloop.recordMidiEvent(0x90, 60, 100); // timestamp = 1*4 + 1 = 5
    
    vloop.clockTick(); // tick 2
    vloop.stopRecording();
    
    std::cout << "\nFirst event timestamp: " << vloop.recordedEvents[0].timestamp << std::endl;
    
    // Start playback
    vloop.startPlayback();
    
    std::cout << "Playback starts at tick: " << vloop.playbackTick 
              << ", sub-tick: " << vloop.playbackSubTick << std::endl;
    
    // The first processPlayback call should immediately play the first event
    vloop.processPlayback();
    
    EXPECT_GT(vloop.playbackOutput.size(), 0) << "No events played on first processPlayback call";
    
    if (vloop.playbackOutput.size() > 0) {
        std::cout << "First event played at timestamp: " << vloop.playbackTimestamps[0] << std::endl;
        
        // The gap would show up as a difference between expected and actual playback time
        uint32_t expectedTime = vloop.recordedEvents[0].timestamp;
        uint32_t actualTime = vloop.playbackTimestamps[0];
        int32_t gap = (int32_t)actualTime - (int32_t)expectedTime;
        
        std::cout << "Gap detected: " << gap << " sub-ticks" << std::endl;
        EXPECT_EQ(gap, 0) << "Gap detected at playback start";
    }
    
    vloop.printTimingAnalysis();
}

TEST_F(VLoopTimingTest, LoopRestartTiming) {
    // Test: Verify loop restart maintains proper timing
    
    vloop.startRecording();
    vloop.recordMidiEvent(0x90, 60, 100); // timestamp 0
    vloop.clockTick();
    vloop.recordMidiEvent(0x90, 62, 100); // timestamp 4
    vloop.clockTick();
    vloop.stopRecording(); // 2-tick loop
    
    vloop.startPlayback();
    
    // Run through multiple loop cycles
    int totalFrames = 50;
    for (int frame = 0; frame < totalFrames; frame++) {
        vloop.processPlayback();
        vloop.advanceTime();
        if (frame % 4 == 3) {
            vloop.clockTick();
        }
    }
    
    vloop.printTimingAnalysis();
    
    // Should have multiple cycles of the same events
    EXPECT_GT(vloop.playbackOutput.size(), vloop.recordedEvents.size()) 
        << "Expected multiple loop cycles";
}

// Performance/stress test
TEST_F(VLoopTimingTest, HighPrecisionTiming) {
    vloop.precision = 16; // Very high precision
    
    vloop.startRecording();
    
    // Record events with very fine timing
    for (int i = 0; i < 32; i++) {
        vloop.recordMidiEvent(0x90, 60 + (i % 12), 100);
        vloop.advanceTime();
    }
    vloop.clockTick();
    vloop.stopRecording();
    
    vloop.startPlayback();
    
    // Process playback
    for (int frame = 0; frame < 100; frame++) {
        vloop.processPlayback();
        vloop.advanceTime();
        if (frame % 16 == 15) {
            vloop.clockTick();
        }
    }
    
    vloop.printTimingAnalysis();
    
    // Verify high precision timing accuracy
    EXPECT_EQ(vloop.playbackOutput.size(), vloop.recordedEvents.size());
}