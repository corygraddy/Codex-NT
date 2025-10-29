# VSeq Unit Tests

This directory contains unit tests for the VSeq sequencer algorithm.

## Overview

The test suite validates the core sequencer logic for both CV and gate sequencers:
- Forward, backward, and pingpong playback modes
- Section looping with configurable repetitions
- Fill feature for gate sequencer
- Proper wrapping behavior at sequence boundaries

## Building and Running Tests

```bash
# Build tests
make

# Run tests
make test

# Clean build artifacts
make clean
```

## Test Coverage

### CV Sequencer Tests
- **CVForwardBasic**: Tests simple forward sequencing with wrapping
- **CVBackwardBasic**: Tests backward sequencing with wrapping
- **CVPingpongBasic**: Tests pingpong mode (bounces at endpoints)
- **CVSectionLooping**: Tests section 1 repeating before moving to section 2

### Gate Sequencer Tests  
- **GateForwardBasic**: Tests basic forward sequencing (16 steps)
- **GateBackwardBasic**: Tests backward sequencing
- **GatePingpongBasic**: Tests pingpong mode for gate tracks
- **GateSectionLooping**: Tests section looping with 2 sections
- **GateFillFeature**: Tests fill triggering jump to section 2 on last section 1 repeat
- **GateBackwardSectionLooping**: Tests backward playback across sections

## Implementation Details

The test file (`test_vseq.cpp`) includes:
- Mock distingNT API structures (minimal implementation for testing)
- Complete copy of sequencer advancement logic from main.cpp
- Lightweight test framework (no external dependencies)
- Simple assertion macros (EXPECT_EQ, EXPECT_TRUE, EXPECT_FALSE)

## Bug Fixes Discovered by Tests

1. **Fill trigger bug**: Fill was triggering even when sec1Reps=1 (no repeats). Fixed by adding condition `sec1Reps > 1` and checking `fillStart > 0`.

2. **No-sections wrapping bug**: When splitPoint >= trackLength (no sections), the sequencer was using section logic instead of simple wrapping. Fixed by adding early return with simple wrapping logic.

## Adding New Tests

To add a new test:

1. Create a test function using the `TEST_F` macro:
   ```cpp
   TEST_F(VSeqSequencerTest, MyNewTest) {
       // Test code here
       EXPECT_EQ(actual, expected);
   }
   ```

2. Add the test to the main() function:
   ```cpp
   std::cout << "Test: MyNewTest\n";
   vseq = VSeqTest();
   test_VSeqSequencerTest_MyNewTest();
   ```

3. Run `make test` to verify

## Notes

- Tests run on the host machine (macOS/Linux), not on the ARM target
- The sequencer logic is duplicated in the test file to avoid complex build dependencies
- Keep test logic in sync with main.cpp when making changes to sequencer advancement
