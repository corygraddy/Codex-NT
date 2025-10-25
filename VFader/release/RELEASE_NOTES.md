# VFader Build 48 - Release Notes

## Installation

1. Copy `VFader.o` to your Disting NT's SD card in the `Algorithms` folder
2. Restart your Disting NT or reload algorithms
3. Select VFader from the algorithm menu

## What's New in Build 48

- **Fixed drift control settings** that prevented catch mode from working properly
- Removed "High" drift setting (2% threshold)
- Renamed "Med" to "High" (1% threshold)  
- Available drift control options: Off, Low (0.5%), High (1%)
- Resolves issue where catch mode couldn't be reached when fader was at zero

## Key Features

- **32 Virtual Faders** organized into 4 pages of 8 faders each
- **Note Mode**: Send specific MIDI notes with customizable scales
- **Number Mode**: Send scaled values (0-100 range, customizable)
- **7-bit and 14-bit MIDI CC output** (CC 1-32 / CC 0-31 with LSB)
- **Pickup Modes**: Scaled or Catch for smooth page transitions
- **Macro Faders**: Control multiple faders with one (Absolute or Relative modes)
- **Custom Naming**: 6-character name + 5-character category per fader
- **Pot Control**: Enable/disable pot input to prevent accidental changes
- **Drift Control**: Minimize unintended value changes from noise

## Quick Start

1. **Navigate Pages**: Left encoder (Pages 1-4)
2. **Select Fader**: Right encoder (Faders 1-8)
3. **Control Faders**: 
   - Left Pot → Left fader
   - Center Pot → Selected fader
   - Right Pot → Right fader
4. **Edit Names**: Press right encoder button
5. **Save Preset**: Save your fader names and configurations

See README.md for complete documentation.

## Requirements

- Disting NT with latest firmware
- SD card with Algorithms folder

## Support

For issues or questions, visit: https://github.com/corygraddy/Codex-NT

## License

See LICENSE file for details.
