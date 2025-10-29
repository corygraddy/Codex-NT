#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

// Simple test framework
int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

#define EXPECT_EQ(actual, expected) do { \
    totalTests++; \
    if ((actual) != (expected)) { \
        std::cout << "  FAIL: " << #actual << " == " << #expected << " (line " << __LINE__ << ")\n"; \
        std::cout << "    Expected: " << (expected) << ", Got: " << (actual) << "\n"; \
        failedTests++; \
    } else { \
        passedTests++; \
    } \
} while(0)

#define EXPECT_TRUE(condition) do { \
    totalTests++; \
    if (!(condition)) { \
        std::cout << "  FAIL: " << #condition << " should be true (line " << __LINE__ << ")\n"; \
        failedTests++; \
    } else { \
        passedTests++; \
    } \
} while(0)

#define EXPECT_FALSE(condition) do { \
    totalTests++; \
    if ((condition)) { \
        std::cout << "  FAIL: " << #condition << " should be false (line " << __LINE__ << ")\n"; \
        failedTests++; \
    } else { \
        passedTests++; \
    } \
} while(0)

#define TEST_F(fixture, name) void test_##fixture##_##name()

class VSeqSequencerTest {
protected:
    void SetUp() {}
};

// Mock distingNT API structures and globals
struct _NT_algorithm {
    float v[128];  // Parameter values
};

uint8_t NT_screen[128 * 64];  // Mock screen buffer

// Include VSeq struct definition (we'll extract it to a header later)
// For now, copy the struct with only what we need for testing

struct VSeqTest : public _NT_algorithm {
    // Sequencer data: 4 sequencers × 32 steps × 3 outputs
    int16_t stepValues[4][32][3];
    uint8_t stepMode[4][32];
    bool gateSteps[6][32];
    
    // Sequencer state
    int currentStep[4];
    bool pingpongForward[4];
    int ratchetCounter[4];
    int repeatCounter[4];
    int section1Counter[4];
    int section2Counter[4];
    bool inSection2[4];
    
    // Gate sequencer state (6 tracks)
    int gateCurrentStep[6];
    bool gatePingpongForward[6];
    int gateSwingCounter[6];
    int gateSection1Counter[6];
    int gateSection2Counter[6];
    bool gateInSection2[6];
    bool gateInFill[6];
    int gateTriggerCounter[6];
    bool gateTriggered[6];
    
    float lastClockIn;
    float lastResetIn;
    int selectedStep;
    int selectedSeq;
    int selectedTrack;
    int lastSelectedStep;
    uint16_t lastButton4State;
    uint16_t lastEncoderRButton;
    float lastPotLValue;
    bool potCaught[3];
    int debugOutputBus[12];
    
    VSeqTest() {
        // Initialize everything to default state
        for (int seq = 0; seq < 4; seq++) {
            for (int step = 0; step < 32; step++) {
                for (int out = 0; out < 3; out++) {
                    stepValues[seq][step][out] = 0;
                }
                stepMode[seq][step] = 0;
            }
            currentStep[seq] = 0;
            pingpongForward[seq] = true;
            ratchetCounter[seq] = 0;
            repeatCounter[seq] = 0;
            section1Counter[seq] = 0;
            section2Counter[seq] = 0;
            inSection2[seq] = false;
        }
        
        for (int track = 0; track < 6; track++) {
            for (int step = 0; step < 32; step++) {
                gateSteps[track][step] = false;
            }
            gateCurrentStep[track] = 0;
            gatePingpongForward[track] = true;
            gateSwingCounter[track] = 0;
            gateSection1Counter[track] = 0;
            gateSection2Counter[track] = 0;
            gateInSection2[track] = false;
            gateInFill[track] = false;
            gateTriggerCounter[track] = 0;
            gateTriggered[track] = false;
        }
        
        lastClockIn = 0.0f;
        lastResetIn = 0.0f;
        selectedStep = 0;
        selectedSeq = 0;
        selectedTrack = 0;
        lastSelectedStep = 0;
        lastButton4State = 0;
        lastEncoderRButton = 0;
        lastPotLValue = 0.5f;
        potCaught[0] = false;
        potCaught[1] = false;
        potCaught[2] = false;
        
        for (int i = 0; i < 12; i++) {
            debugOutputBus[i] = 0;
        }
    }
    
    // CV sequencer advancement
    void advanceSequencer(int seq, int direction, int stepCount, int splitPoint, int sec1Reps, int sec2Reps) {
        // If no sections (splitPoint >= stepCount), use simple wrapping logic
        if (splitPoint >= stepCount) {
            if (direction == 0) {
                // Forward
                currentStep[seq]++;
                if (currentStep[seq] >= stepCount) {
                    currentStep[seq] = 0;
                }
            } else if (direction == 1) {
                // Backward
                currentStep[seq]--;
                if (currentStep[seq] < 0) {
                    currentStep[seq] = stepCount - 1;
                }
            } else {
                // Pingpong
                if (pingpongForward[seq]) {
                    currentStep[seq]++;
                    if (currentStep[seq] >= stepCount) {
                        currentStep[seq] = stepCount - 1;  // Stay at max
                        pingpongForward[seq] = false;  // Reverse direction
                    }
                } else {
                    currentStep[seq]--;
                    if (currentStep[seq] < 0) {
                        currentStep[seq] = 0;  // Stay at min
                        pingpongForward[seq] = true;  // Reverse direction
                    }
                }
            }
            return;
        }
        
        // Section-based logic
        if (direction == 0) {
            // Forward
            currentStep[seq]++;
            
            if (!inSection2[seq]) {
                if (currentStep[seq] >= splitPoint) {
                    section1Counter[seq]++;
                    if (section1Counter[seq] >= sec1Reps) {
                        inSection2[seq] = true;
                        section1Counter[seq] = 0;
                    } else {
                        currentStep[seq] = 0;
                    }
                }
            } else {
                if (currentStep[seq] >= stepCount) {
                    section2Counter[seq]++;
                    if (section2Counter[seq] >= sec2Reps) {
                        inSection2[seq] = false;
                        section2Counter[seq] = 0;
                        currentStep[seq] = 0;
                    } else {
                        currentStep[seq] = splitPoint;
                    }
                }
            }
        } else if (direction == 1) {
            // Backward
            currentStep[seq]--;
            
            if (inSection2[seq]) {
                if (currentStep[seq] < splitPoint) {
                    section2Counter[seq]++;
                    if (section2Counter[seq] >= sec2Reps) {
                        inSection2[seq] = false;
                        section2Counter[seq] = 0;
                    } else {
                        currentStep[seq] = stepCount - 1;
                    }
                }
            } else {
                if (currentStep[seq] < 0) {
                    section1Counter[seq]++;
                    if (section1Counter[seq] >= sec1Reps) {
                        inSection2[seq] = true;
                        section1Counter[seq] = 0;
                        currentStep[seq] = stepCount - 1;
                    } else {
                        currentStep[seq] = splitPoint - 1;
                    }
                }
            }
        } else {
            // Pingpong
            if (pingpongForward[seq]) {
                currentStep[seq]++;
                if (currentStep[seq] >= stepCount) {
                    currentStep[seq] = stepCount - 1;
                    pingpongForward[seq] = false;
                }
            } else {
                currentStep[seq]--;
                if (currentStep[seq] <= 0) {
                    currentStep[seq] = 0;
                    pingpongForward[seq] = true;
                }
            }
        }
    }
    
    // Gate sequencer advancement
    void advanceGateSequencer(int track, int direction, int trackLength, int splitPoint, 
                              int sec1Reps, int sec2Reps, int fillStart) {
        // If no sections (splitPoint >= trackLength), use simple wrapping logic
        if (splitPoint >= trackLength) {
            if (direction == 0) {
                // Forward
                gateCurrentStep[track]++;
                if (gateCurrentStep[track] >= trackLength) {
                    gateCurrentStep[track] = 0;
                }
            } else if (direction == 1) {
                // Backward
                gateCurrentStep[track]--;
                if (gateCurrentStep[track] < 0) {
                    gateCurrentStep[track] = trackLength - 1;
                }
            } else if (direction == 2) {
                // Pingpong
                if (gatePingpongForward[track]) {
                    gateCurrentStep[track]++;
                    if (gateCurrentStep[track] >= trackLength) {
                        gateCurrentStep[track] = trackLength - 2;
                        if (gateCurrentStep[track] < 0) gateCurrentStep[track] = 0;
                        gatePingpongForward[track] = false;
                    }
                } else {
                    gateCurrentStep[track]--;
                    if (gateCurrentStep[track] < 0) {
                        gateCurrentStep[track] = 1;
                        if (gateCurrentStep[track] >= trackLength) gateCurrentStep[track] = trackLength - 1;
                        gatePingpongForward[track] = true;
                    }
                }
            }
            return;
        }
        
        // Section-based logic
        int section1End = (splitPoint > 0 && splitPoint < trackLength) ? splitPoint : trackLength;
        
        if (direction == 0) {  // Forward
            gateCurrentStep[track]++;
            
            // Check for fill trigger on last repetition of section 1
            if (!gateInSection2[track] && 
                splitPoint > 0 && 
                splitPoint < trackLength &&  // Only if sections are actually being used
                fillStart > 0 &&  // Only if fill is enabled (fillStart > 0)
                fillStart < splitPoint &&
                sec1Reps > 1 &&  // Only if repeating section 1
                gateSection1Counter[track] == sec1Reps - 1 &&
                gateCurrentStep[track] >= fillStart) {
                gateSection1Counter[track] = 0;
                gateInSection2[track] = true;
                gateCurrentStep[track] = splitPoint;
            }
            else if (!gateInSection2[track] && gateCurrentStep[track] >= section1End) {
                gateSection1Counter[track]++;
                if (gateSection1Counter[track] >= sec1Reps) {
                    gateSection1Counter[track] = 0;
                    gateInSection2[track] = true;
                    gateCurrentStep[track] = (splitPoint > 0) ? splitPoint : 0;
                } else {
                    gateCurrentStep[track] = 0;
                }
            } else if (gateInSection2[track] && gateCurrentStep[track] >= trackLength) {
                gateSection2Counter[track]++;
                if (gateSection2Counter[track] >= sec2Reps) {
                    gateSection2Counter[track] = 0;
                    gateInSection2[track] = false;
                }
                gateCurrentStep[track] = (splitPoint > 0) ? splitPoint : 0;
                if (!gateInSection2[track]) {
                    gateCurrentStep[track] = 0;
                }
            }
        } else if (direction == 1) {  // Backward
            gateCurrentStep[track]--;
            
            if (gateInSection2[track] && gateCurrentStep[track] < splitPoint) {
                gateSection2Counter[track]++;
                if (gateSection2Counter[track] >= sec2Reps) {
                    gateSection2Counter[track] = 0;
                    gateInSection2[track] = false;
                    gateCurrentStep[track] = section1End - 1;
                } else {
                    gateCurrentStep[track] = trackLength - 1;
                }
            } else if (!gateInSection2[track] && gateCurrentStep[track] < 0) {
                gateSection1Counter[track]++;
                if (gateSection1Counter[track] >= sec1Reps) {
                    gateSection1Counter[track] = 0;
                    gateInSection2[track] = true;
                    gateCurrentStep[track] = trackLength - 1;
                } else {
                    gateCurrentStep[track] = section1End - 1;
                }
            }
        } else if (direction == 2) {  // Pingpong
            if (gatePingpongForward[track]) {
                gateCurrentStep[track]++;
                if (gateCurrentStep[track] >= trackLength) {
                    gateCurrentStep[track] = trackLength - 2;
                    if (gateCurrentStep[track] < 0) gateCurrentStep[track] = 0;
                    gatePingpongForward[track] = false;
                }
            } else {
                gateCurrentStep[track]--;
                if (gateCurrentStep[track] < 0) {
                    gateCurrentStep[track] = 1;
                    if (gateCurrentStep[track] >= trackLength) gateCurrentStep[track] = trackLength - 1;
                    gatePingpongForward[track] = true;
                }
            }
        }
    }
};

// Test fixture
VSeqTest vseq;

// ============================================================================
// CV Sequencer Tests
// ============================================================================

TEST_F(VSeqSequencerTest, CVForwardBasic) {
    // Test basic forward sequencing with 8 steps
    // Start at 0, advance 8 times should go through 1,2,3,4,5,6,7,0
    for (int i = 1; i <= 8; i++) {
        vseq.advanceSequencer(0, 0, 8, 8, 1, 1);  // direction=0 (forward), 8 steps, no sections
        int expected = (i < 8) ? i : 0;  // After 8th advance, should wrap to 0
        EXPECT_EQ(vseq.currentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, CVBackwardBasic) {
    // Start at step 7 for backward
    vseq.currentStep[0] = 7;
    
    // Advance backward 8 times: 7→6,5,4,3,2,1,0,7
    for (int i = 6; i >= -1; i--) {
        vseq.advanceSequencer(0, 1, 8, 8, 1, 1);  // direction=1 (backward)
        int expected = (i >= 0) ? i : 7;  // After going below 0, should wrap to 7
        EXPECT_EQ(vseq.currentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, CVPingpongBasic) {
    // Test pingpong motion - starts at 0, direction forward
    std::vector<int> expectedSteps = {1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2};
    
    for (int expected : expectedSteps) {
        vseq.advanceSequencer(0, 2, 8, 8, 1, 1);  // direction=2 (pingpong)
        EXPECT_EQ(vseq.currentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, CVSectionLooping) {
    // Test section 1 repeating 3 times before moving to section 2
    // splitPoint=4, sec1Reps=3, sec2Reps=1
    
    // First loop of section 1 (0→1,2,3,0 - wraps after reaching splitPoint 4)
    for (int i = 0; i < 4; i++) {
        vseq.advanceSequencer(0, 0, 8, 4, 3, 1);
        int expected = (i < 3) ? (i + 1) : 0;  // 1,2,3,0
        EXPECT_EQ(vseq.currentStep[0], expected);
        EXPECT_FALSE(vseq.inSection2[0]);
    }
    
    // Second loop of section 1
    for (int i = 0; i < 4; i++) {
        vseq.advanceSequencer(0, 0, 8, 4, 3, 1);
        int expected = (i < 3) ? (i + 1) : 0;
        EXPECT_EQ(vseq.currentStep[0], expected);
        EXPECT_FALSE(vseq.inSection2[0]);
    }
    
    // Third loop of section 1
    for (int i = 0; i < 4; i++) {
        vseq.advanceSequencer(0, 0, 8, 4, 3, 1);
        if (i < 3) {
            int expected = i + 1;  // 1,2,3
            EXPECT_EQ(vseq.currentStep[0], expected);
            EXPECT_FALSE(vseq.inSection2[0]);
        } else {
            // After 3rd rep completes, moves to section 2
            EXPECT_EQ(vseq.currentStep[0], 4);  // Jumps to splitPoint
            EXPECT_TRUE(vseq.inSection2[0]);
        }
    }
    
    // Now in section 2 (steps 4-7, continues to 8 then wraps)
    for (int i = 0; i < 4; i++) {
        vseq.advanceSequencer(0, 0, 8, 4, 3, 1);
        if (i < 3) {
            EXPECT_EQ(vseq.currentStep[0], 5 + i);  // 5,6,7
            EXPECT_TRUE(vseq.inSection2[0]);
        } else {
            // After section 2 completes, wraps back to section 1
            EXPECT_EQ(vseq.currentStep[0], 0);
            EXPECT_FALSE(vseq.inSection2[0]);
        }
    }
}

// ============================================================================
// Gate Sequencer Tests
// ============================================================================

TEST_F(VSeqSequencerTest, GateForwardBasic) {
    // Test basic forward sequencing with 16 steps
    for (int i = 1; i <= 16; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 16, 1, 1, 0);
        int expected = (i < 16) ? i : 0;  // After 16th advance, wraps to 0
        EXPECT_EQ(vseq.gateCurrentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, GateBackwardBasic) {
    // Start at step 15 for backward
    vseq.gateCurrentStep[0] = 15;
    
    // Advance backward 16 times
    for (int i = 14; i >= -1; i--) {
        vseq.advanceGateSequencer(0, 1, 16, 16, 1, 1, 0);
        int expected = (i >= 0) ? i : 15;  // After going below 0, wraps to 15
        EXPECT_EQ(vseq.gateCurrentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, GatePingpongBasic) {
    // Test pingpong motion with 8 steps - starts at 0, forward
    std::vector<int> expectedSteps = {1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2};
    
    for (int expected : expectedSteps) {
        vseq.advanceGateSequencer(0, 2, 8, 8, 1, 1, 0);
        EXPECT_EQ(vseq.gateCurrentStep[0], expected);
    }
}

TEST_F(VSeqSequencerTest, GateSectionLooping) {
    // Test section 1 repeating 2 times before section 2
    // splitPoint=8, trackLength=16, sec1Reps=2, sec2Reps=1
    
    // First loop of section 1 (0→1,2,3,4,5,6,7,0)
    for (int i = 0; i < 8; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 0);
        int expected = (i < 7) ? (i + 1) : 0;  // 1,2,3,4,5,6,7,0
        EXPECT_EQ(vseq.gateCurrentStep[0], expected);
        EXPECT_FALSE(vseq.gateInSection2[0]);
    }
    
    // Second loop of section 1
    for (int i = 0; i < 8; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 0);
        if (i < 7) {
            EXPECT_EQ(vseq.gateCurrentStep[0], i + 1);  // 1,2,3,4,5,6,7
            EXPECT_FALSE(vseq.gateInSection2[0]);
        } else {
            // After 2nd rep, moves to section 2
            EXPECT_EQ(vseq.gateCurrentStep[0], 8);  // Jumps to splitPoint
            EXPECT_TRUE(vseq.gateInSection2[0]);
        }
    }
    
    // Now in section 2 (steps 8-15)
    for (int i = 0; i < 8; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 0);
        if (i < 7) {
            EXPECT_EQ(vseq.gateCurrentStep[0], 9 + i);  // 9,10,11,12,13,14,15
            EXPECT_TRUE(vseq.gateInSection2[0]);
        } else {
            // After section 2, wraps back to section 1
            EXPECT_EQ(vseq.gateCurrentStep[0], 0);
            EXPECT_FALSE(vseq.gateInSection2[0]);
        }
    }
}

TEST_F(VSeqSequencerTest, GateFillFeature) {
    // Test fill triggering jump to section 2
    // splitPoint=8, fillStart=6, sec1Reps=2
    
    // First loop of section 1 - normal playback (0→1,2,3,4,5,6,7,0)
    for (int i = 0; i < 8; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 6);
        EXPECT_FALSE(vseq.gateInSection2[0]);
    }
    
    // Second loop (last rep) - advance until step 5 (before fill trigger)
    for (int i = 0; i < 5; i++) {
        vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 6);
        EXPECT_FALSE(vseq.gateInSection2[0]);
    }
    
    // Next advance reaches step 6, should trigger fill and jump to section 2
    vseq.advanceGateSequencer(0, 0, 16, 8, 2, 1, 6);
    EXPECT_TRUE(vseq.gateInSection2[0]);
    EXPECT_EQ(vseq.gateCurrentStep[0], 8);  // Should jump to splitPoint
}

TEST_F(VSeqSequencerTest, GateBackwardSectionLooping) {
    // Test backward with sections
    vseq.gateCurrentStep[0] = 15;
    vseq.gateInSection2[0] = true;
    
    // Play section 2 backward (15→14,13,12,11,10,9,8,7)
    // After reaching splitPoint (8), it should move to section 1
    for (int i = 14; i >= 7; i--) {
        vseq.advanceGateSequencer(0, 1, 16, 8, 1, 1, 0);
        if (i >= 8) {
            EXPECT_EQ(vseq.gateCurrentStep[0], i);
            EXPECT_TRUE(vseq.gateInSection2[0]);
        } else {
            // At step 7 (section1End - 1), moved to section 1
            EXPECT_EQ(vseq.gateCurrentStep[0], 7);
            EXPECT_FALSE(vseq.gateInSection2[0]);
        }
    }
}

// Main function for running tests
int main(int argc, char **argv) {
    std::cout << "Running VSeq Unit Tests\n";
    std::cout << "=======================\n\n";
    
    // CV Sequencer Tests
    std::cout << "CV Sequencer Tests:\n";
    std::cout << "------------------\n";
    
    std::cout << "Test: CVForwardBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_CVForwardBasic();
    
    std::cout << "Test: CVBackwardBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_CVBackwardBasic();
    
    std::cout << "Test: CVPingpongBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_CVPingpongBasic();
    
    std::cout << "Test: CVSectionLooping\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_CVSectionLooping();
    
    std::cout << "\nGate Sequencer Tests:\n";
    std::cout << "--------------------\n";
    
    std::cout << "Test: GateForwardBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GateForwardBasic();
    
    std::cout << "Test: GateBackwardBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GateBackwardBasic();
    
    std::cout << "Test: GatePingpongBasic\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GatePingpongBasic();
    
    std::cout << "Test: GateSectionLooping\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GateSectionLooping();
    
    std::cout << "Test: GateFillFeature\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GateFillFeature();
    
    std::cout << "Test: GateBackwardSectionLooping\n";
    vseq = VSeqTest();
    test_VSeqSequencerTest_GateBackwardSectionLooping();
    
    // Summary
    std::cout << "\n=======================\n";
    std::cout << "Test Results:\n";
    std::cout << "  Total:  " << totalTests << "\n";
    std::cout << "  Passed: " << passedTests << "\n";
    std::cout << "  Failed: " << failedTests << "\n";
    
    if (failedTests == 0) {
        std::cout << "\n✓ All tests passed!\n";
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed.\n";
        return 1;
    }
}
