#include <gtest/gtest.h>
#include <iostream>
#include <chrono>

// Simplified test for VLoop state machine logic
// Testing the core logic without the full Disting NT framework

enum RecordingState {
    IDLE = 0,
    WAITING_FOR_RESET,
    ACTIVELY_RECORDING
};

struct TestVLoop {
    RecordingState currentState = IDLE;
    int transitionCount = 0;
    int totalClockTicks = 0;
    int totalResetTicks = 0;
    int recordingStartTick = 0;
    int recordingEndTick = 0;
    int loopLengthTicks = 0;
    
    // Previous values for edge detection
    bool lastClockValue = false;
    bool lastResetValue = false;
    bool lastRecordValue = false;
    
    void logTransition(RecordingState from, RecordingState to) {
        currentState = to;
        transitionCount++;
        std::cout << "State transition: " << from << " -> " << to << std::endl;
    }
    
    void processInputs(bool clock, bool reset, bool record) {
        // Edge detection
        bool clockRising = clock && !lastClockValue;
        bool resetRising = reset && !lastResetValue;
        bool recordRising = record && !lastRecordValue;
        bool recordFalling = !record && lastRecordValue;
        
        // Process clock
        if (clockRising) {
            totalClockTicks++;
        }
        
        // Process reset
        if (resetRising) {
            totalResetTicks++;
            
            if (currentState == WAITING_FOR_RESET) {
                recordingStartTick = totalClockTicks;
                logTransition(WAITING_FOR_RESET, ACTIVELY_RECORDING);
            }
            else if (currentState == ACTIVELY_RECORDING) {
                recordingEndTick = totalClockTicks;
                loopLengthTicks = totalClockTicks - recordingStartTick;
                logTransition(ACTIVELY_RECORDING, IDLE);
            }
        }
        
        // Process record button
        if (recordRising) {
            if (currentState == IDLE) {
                logTransition(IDLE, WAITING_FOR_RESET);
            }
        }
        else if (recordFalling) {
            // FIXED: Only cancel from WAITING_FOR_RESET
            if (currentState == WAITING_FOR_RESET) {
                logTransition(WAITING_FOR_RESET, IDLE);
            }
            // In ACTIVELY_RECORDING, button release doesn't stop recording
        }
        
        // Update previous values
        lastClockValue = clock;
        lastResetValue = reset;
        lastRecordValue = record;
    }
};

class VLoopLogicTest : public ::testing::Test {
protected:
    TestVLoop vloop;
    
    void sendClock() {
        vloop.processInputs(false, false, vloop.lastRecordValue);  // Clock low
        vloop.processInputs(true, false, vloop.lastRecordValue);   // Clock high
    }
    
    void sendReset() {
        vloop.processInputs(vloop.lastClockValue, false, vloop.lastRecordValue);  // Reset low
        vloop.processInputs(vloop.lastClockValue, true, vloop.lastRecordValue);   // Reset high
    }
    
    void pressRecord() {
        vloop.processInputs(vloop.lastClockValue, vloop.lastResetValue, true);
    }
    
    void releaseRecord() {
        vloop.processInputs(vloop.lastClockValue, vloop.lastResetValue, false);
    }
};

TEST_F(VLoopLogicTest, InitialState) {
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_EQ(vloop.transitionCount, 0);
}

TEST_F(VLoopLogicTest, RecordButtonStartsWaiting) {
    pressRecord();
    EXPECT_EQ(vloop.currentState, WAITING_FOR_RESET);
    EXPECT_EQ(vloop.transitionCount, 1);
}

TEST_F(VLoopLogicTest, RecordReleaseFromWaitingReturnsToIdle) {
    pressRecord();
    releaseRecord();
    
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_EQ(vloop.transitionCount, 2);
}

TEST_F(VLoopLogicTest, ResetStartsRecording) {
    pressRecord();          // IDLE -> WAITING_FOR_RESET
    sendReset();           // WAITING_FOR_RESET -> ACTIVELY_RECORDING
    
    EXPECT_EQ(vloop.currentState, ACTIVELY_RECORDING);
    EXPECT_EQ(vloop.transitionCount, 2);
    EXPECT_EQ(vloop.recordingStartTick, vloop.totalClockTicks);
}

TEST_F(VLoopLogicTest, RecordReleaseDoesNotStopActiveRecording) {
    pressRecord();          // Enter waiting
    sendReset();           // Start recording
    releaseRecord();       // Should NOT stop recording
    
    EXPECT_EQ(vloop.currentState, ACTIVELY_RECORDING);
    EXPECT_EQ(vloop.transitionCount, 2);  // Only 2 transitions, not 3
}

TEST_F(VLoopLogicTest, ResetEndsRecording) {
    pressRecord();
    sendClock();
    sendClock();
    sendReset();           // Start recording at tick 2
    sendClock();           // Tick 3
    sendClock();           // Tick 4
    sendReset();           // End recording at tick 4
    
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_EQ(vloop.recordingStartTick, 2);
    EXPECT_EQ(vloop.recordingEndTick, 4);
    EXPECT_EQ(vloop.loopLengthTicks, 2);  // 4 - 2 = 2
}

TEST_F(VLoopLogicTest, CompleteWorkflow) {
    // Test a complete recording workflow
    
    // 1. Press record
    pressRecord();
    EXPECT_EQ(vloop.currentState, WAITING_FOR_RESET);
    
    // 2. Some clock pulses while waiting
    sendClock();
    sendClock();
    EXPECT_EQ(vloop.currentState, WAITING_FOR_RESET);  // Still waiting
    EXPECT_EQ(vloop.totalClockTicks, 2);
    
    // 3. Reset starts recording
    sendReset();
    EXPECT_EQ(vloop.currentState, ACTIVELY_RECORDING);
    EXPECT_EQ(vloop.recordingStartTick, 2);
    
    // 4. Recording continues through button release
    sendClock();
    releaseRecord();  // This should NOT stop recording
    sendClock();
    EXPECT_EQ(vloop.currentState, ACTIVELY_RECORDING);
    
    // 5. Next reset ends recording
    sendReset();
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_GT(vloop.loopLengthTicks, 0);
    
    // Final verification
    EXPECT_EQ(vloop.transitionCount, 3);  // IDLE->WAITING->RECORDING->IDLE
    EXPECT_GT(vloop.recordingEndTick, vloop.recordingStartTick);
}

TEST_F(VLoopLogicTest, ResetWithoutRecordDoesNothing) {
    sendReset();
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_EQ(vloop.transitionCount, 0);
}

TEST_F(VLoopLogicTest, MultipleResetsInActiveRecording) {
    pressRecord();
    sendReset();  // Start recording
    
    int startTick = vloop.recordingStartTick;
    
    sendClock();
    sendReset();  // First reset ends recording
    
    EXPECT_EQ(vloop.currentState, IDLE);
    EXPECT_EQ(vloop.recordingStartTick, startTick);
    EXPECT_GT(vloop.recordingEndTick, startTick);
}

// Performance test
TEST_F(VLoopLogicTest, StateTransitionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate rapid input changes
    for (int i = 0; i < 10000; i++) {
        pressRecord();
        sendClock();
        sendReset();
        sendClock();
        sendReset();
        releaseRecord();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "10000 cycles took: " << duration.count() << " microseconds" << std::endl;
    EXPECT_LT(duration.count(), 10000);  // Should complete in under 10ms
}