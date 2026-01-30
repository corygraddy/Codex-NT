# FCBFix

**MIDI Program Change to CV Gate Converter for Disting NT**

## Overview

FCBFix converts MIDI Program Change messages to CV gate triggers. Perfect for controlling hardware modules from MIDI foot controllers like the Behringer FCB1010 or any MIDI controller that sends program change messages.

## Features

- **10 Programmable Slots**: Each slot can be assigned to a specific program change number
- **Flexible Output Routing**: Each slot can trigger any of the 28 CV outputs
- **Fixed Gate Duration**: 100ms gate pulses (10V amplitude)
- **Visual Feedback**: Real-time gate indicators on display
- **Simple Configuration**: 2 parameters per slot (Program Number + Output)

## Parameters

Each of the 10 slots has:
- **Program**: MIDI Program Change number to listen for (0-127)
- **Output**: CV output to trigger (1-28, or 0 for disabled)

## Usage

1. Configure each slot with a program change number and output assignment
2. Connect MIDI controller to Disting NT
3. Send program change messages from your controller
4. FCBFix fires CV gates on matching program numbers

## Display

- **10 Slot Grid**: Shows all 10 slots with their configuration
- **Program Numbers**: "P0" through "P127" labels
- **Output Assignments**: "O1" through "O28" labels
- **Gate Indicators**: Circles light up when gates are active
- **Selection**: Underline shows currently selected slot

## Use Cases

- Trigger drums/percussion from MIDI foot controller
- Switch between modular patches using program changes
- Control envelope generators or sequencer clocks
- Create complex switching matrices with MIDI control

## Building

```bash
cd FCBFix
make
./BuildTransfer.sh
```

## Technical Details

- **GUID**: FCBF
- **Gate Duration**: 100ms (4800 samples at 48kHz)
- **Gate Voltage**: 10V
- **MIDI Channels**: Responds to all MIDI channels
- **Multiple Matches**: If multiple slots are assigned to the same program number, all will trigger simultaneously

## Version History

- **v1.0.0** (2026-01-30): Initial release
