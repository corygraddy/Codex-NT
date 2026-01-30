# FCBFix v1.0.0 Release Notes

## Overview
MIDI Program Change to CV Gate converter for Disting NT. Perfect for foot controllers like the Behringer FCB1010.

## Features
- **10 programmable slots**: Each maps a program change number (0-127) to a CV output
- **100ms gate duration**: Fixed-length 10V gates
- **Real-time visual feedback**: Gate indicators show active outputs
- **Simple parameter layout**: Program number + output assignment per slot
- **All MIDI channels**: Responds to program changes on any channel

## Installation
1. Copy `FCBFix.o` to your Disting NT SD card at `/programs/plug-ins/`
2. Safely eject the SD card
3. Insert into Disting NT and power cycle
4. FCBFix will appear in the plugin browser

## Configuration
Each slot has 2 parameters:
- **Slot X Program**: MIDI program change number (0-127)
- **Slot X Output**: CV output to trigger (0=disabled, 1-28=bus number)

## Usage Example
Configure for FCB1010 foot controller:
- Slot 1: Program 0 → Output 1 (kick drum trigger)
- Slot 2: Program 1 → Output 2 (snare trigger)
- Slot 3: Program 2 → Output 3 (hi-hat trigger)
- etc.

Press footswitches 1-10 on FCB1010, get CV gates on assigned outputs.

## Technical Details
- **GUID**: FCBF
- **Gate voltage**: 10V
- **Gate duration**: 100ms (4800 samples @ 48kHz)
- **Multiple triggers**: If multiple slots use the same program number, all trigger simultaneously
- **MIDI channel**: Responds to all channels (1-16)

## Known Limitations
- Gate duration is fixed at 100ms (no user adjustment)
- Maximum 10 program change mappings
- No velocity sensitivity

## Version History
- **v1.0.0** (2026-01-30): Initial release
