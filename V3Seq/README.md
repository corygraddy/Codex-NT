# V3Seq - 3-Output CV Sequencer

A polyphonic CV sequencer for Expert Sleepers Disting NT with 3 independent outputs, section looping, and MIDI CC output.

## Features

- **3 Independent CV Outputs** with 32 steps each
- **4 Voltage Ranges**: 0-5V, 0-10V, -5-+5V, -10-+10V
- **Flexible Playback**: Forward, Backward, Pingpong modes
- **Clock Division**: 1, 2, 4, 8, 16, 32 per output
- **Section Looping**: Two-section structure with repeat counts
- **Fine/Coarse Editing**: 25 coarse steps or 500 fine steps
- **MIDI CC Output**: Parallel CC output for each CV channel
- **Visual Bar Graph**: Real-time display with reference dots every 4 steps

## Installation

Copy `V3Seq.o` to your Disting NT's SD card at `/programs/plug-ins/` and restart the device.

[Download Latest Release](https://github.com/corygraddy/V3Seq/releases/latest)

## Quick Start

1. **Select Page**: Turn left encoder or use middle pot to choose CV output 1-3
2. **Select Step**: Turn right encoder to choose step 1-32
3. **Edit Value**: Turn middle pot to adjust step voltage
4. **Toggle Fine Mode**: Press right encoder button for fine adjustment (500 steps)
5. **Adjust Parameters**: Use parameter pages to configure range, clock division, sections

## Documentation

- [Release Notes](release/RELEASE_NOTES.md)
- [Build Instructions](release/GITHUB_RELEASE_INSTRUCTIONS.md)

## Development

Built for Expert Sleepers Disting NT using the distingNT API.

**Author**: Cory Graddy  
**License**: See [LICENSE](../LICENSE)
