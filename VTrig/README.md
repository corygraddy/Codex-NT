# VTrig - 6-Track Trigger Sequencer

A powerful 6-track trigger/gate sequencer for Expert Sleepers Disting NT with swing, section looping, and MIDI CC output.

## Features

- **6 Independent Trigger Tracks** with 32 steps each
- **Flexible Playback**: Forward, Backward, Pingpong modes
- **Clock Division/Multiplication**: /16 to x16 (31 options per track)
- **Swing**: 0-100% adjustable timing offset for odd steps
- **Section Looping**: Two-section structure with repeat counts
- **Fill Feature**: Jump to Section 2 on last repeat of Section 1
- **MIDI CC Output**: Parallel CC output for each trigger track
- **Visual Editor**: Step grid with real-time playback indicator

## Installation

Copy `VTrig.o` to your Disting NT's SD card at `/programs/plug-ins/` and restart the device.

[Download Latest Release](https://github.com/corygraddy/VTrig/releases/latest)

## Quick Start

1. **Select Track**: Turn left encoder to choose track 1-6
2. **Select Step**: Turn right encoder to choose step 1-32
3. **Toggle Gate**: Press right encoder button to enable/disable step
4. **Adjust Parameters**: Use parameter pages to configure timing, clock division, sections

## Documentation

- [Full User Guide](release/README.md)
- [Release Notes](release/RELEASE_NOTES.md)
- [Build Instructions](release/GITHUB_RELEASE_INSTRUCTIONS.md)

## Development

Built for Expert Sleepers Disting NT using the distingNT API.

**Author**: Cory Graddy  
**License**: See [LICENSE](../LICENSE)
