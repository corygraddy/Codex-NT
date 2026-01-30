# FCBFix

**MIDI Program Change to CV Gate + MIDI Note Converter for Disting NT**

## Overview

FCBFix converts MIDI Program Change messages to both CV gate triggers and MIDI Note On/Off messages. Perfect for controlling hardware modules from MIDI foot controllers like the Behringer FCB1010, while simultaneously sending MIDI notes to downstream synths or samplers.

## Features

- **10 Programmable Slots**: Each slot can be assigned to a specific program change number
- **Flexible Output Routing**: Each slot can trigger any of the 28 CV outputs
- **MIDI Note Transmission**: Sends Note On/Off messages for each triggered slot
- **Configurable MIDI Routing**: Choose destination (USB, Breakout, SelectBus, Internal)
- **Fixed Gate Duration**: 100ms gate pulses (10V amplitude) with matching Note On/Off timing
- **Visual Feedback**: Real-time gate indicators on display
- **Simple Configuration**: 3 parameters per slot (Program Number + Output + MIDI Note)

## Parameters

### Global MIDI Settings (Page "MIDI")
- **MIDI Channel**: Channel for MIDI note transmission (1-16)
- **MIDI Destination**: Where to send MIDI (0=Off, 1=Breakout, 2=SelectBus, 3=USB, 4=Internal)

### Each of the 10 Slots (Pages "Slot 1" - "Slot 10")
- **Program**: MIDI Program Change number to listen for (0-127)
- **Output**: CV output to trigger (1-28, or 0 for disabled)
- **MIDI Note**: MIDI note number to send (0-127)

## Usage

1. Set global MIDI channel and destination on the MIDI page
2. Configure each slot with:
   - Program change number to trigger on
   - CV output assignment
   - MIDI note to send
3. Connect MIDI controller to Disting NT
4. Send program change messages from your controller
5. FCBFix fires CV gates AND sends MIDI Note On/Off messages

## Display

- **10 Slot Grid**: Shows all 10 slots with their configuration
- **Program Numbers**: "P0" through "P127" labels
- **Output Assignments**: "O1" through "O28" labels
- **Gate Indicators**: Circles light up when gates are active
- **Selection**: Underline shows currently selected slot

## Use Cases

- Trigger drums/percussion from MIDI foot controller while sending MIDI to sampler
- Switch between modular patches and change MIDI notes simultaneously
- Control envelope generators via CV while playing notes on synths
- Create complex switching matrices with both CV and MIDI control
- Use foot controller to trigger Eurorack modules and external MIDI gear in sync

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
- **MIDI Velocity**: Fixed at 100
- **MIDI Note Duration**: Matches gate duration (100ms)
- **MIDI Channels**: Listens to all MIDI channels, sends on configured channel
- **Multiple Matches**: If multiple slots are assigned to the same program number, all will trigger simultaneously

## Version History

- **v1.0.0** (2026-01-30): Initial release
