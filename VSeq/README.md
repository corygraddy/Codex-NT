# VSeq - 4-Channel CV Sequencer for Disting NT

**Build 1** - 4-Channel 32-Step Sequencer with Section Looping

## Overview

VSeq is a powerful 4-channel voltage sequencer for Disting NT. Each sequencer has 32 steps with 3 independent CV outputs, flexible playback modes, clock division/multiplication, and advanced section looping capabilities.

## Features

- **4 Independent Sequencers**: Each with 32 steps and 3 CV outputs
- **Flexible Playback**: Forward, Backward, and Pingpong modes
- **Clock Division/Multiplication**: /16, /8, /4, /2, x1, x2, x4, x8, x16
- **Variable Step Count**: 1-32 steps per sequencer
- **Section Looping**: Split sequences into two sections with independent repeat counts
- **Visual Step Editor**: Two rows of 16 steps with 3 vertical bars per step
- **Real-time Indicators**: Current step dots and selection underlines
- **Page Indicators**: Visual bars show active sequencer (1-4)

## Specifications

- **Inputs**: Clock, Reset
- **Outputs**: 12 CV outputs (4 sequencers × 3 outputs each)
- **Voltage Range**: 0-10V (configurable per step)
- **Resolution**: 32 steps per sequencer
- **Step Modes**: Normal (ratchet/repeat framework in place for future)

## Controls

### Hardware Controls
- **Left Encoder**: Select sequencer page (1-4)
- **Right Encoder**: Select step (0-31)
- **Left Pot**: Adjust Output 1 value for selected step
- **Center Pot**: Adjust Output 2 value for selected step
- **Right Pot**: Adjust Output 3 value for selected step
- **Button 4**: Reserved for step mode cycling (future feature)

### Parameters (Per Sequencer)
- **Clock Division**: /16 to x16 range
- **Direction**: Forward, Backward, Pingpong
- **Step Count**: Number of active steps (1-32)
- **Split Point**: Where section 1 ends and section 2 begins (1-31)
- **Section 1 Repeats**: How many times to repeat section 1 (1-99)
- **Section 2 Repeats**: How many times to repeat section 2 (1-99)

## Section Looping Behavior

Each sequencer can be split into two sections:
- **Section 1**: Steps 0 to Split Point
- **Section 2**: Steps Split Point to Step Count

The sequencer will play Section 1 for the specified number of repeats, then play Section 2 for its repeats, then loop back to Section 1. This allows for complex rhythmic patterns and variations.

**Note:** Section looping only applies to Forward and Backward modes. Pingpong mode plays through the entire sequence.

When you change the Step Count parameter, the split parameters automatically reset:
- Split Point = Step Count / 2
- Section Repeats = 1

## UI Elements

- **Step Bars**: Three vertical bars per step showing CV output values (0V = bottom, 10V = top)
- **Current Step Indicator**: Dot above the middle bar of the currently playing step
- **Selection Underline**: Line under the selected step being edited
- **Separator Lines**: Dark vertical lines between groups of 4 steps
- **Page Indicators**: Four horizontal bars at the top, bright bar shows active sequencer

## Installation

1. Copy `1VSeq.o` to your Disting NT's SD card in the `/plugins/` directory
2. Restart your Disting NT or reload plugins
3. Select "VSeq" from the algorithm menu

## Preset

An example preset "Assured research.json" is included showing:
- Clock module in slot 1 providing sync
- VSeq in slot 2 with all 4 sequencers configured
- Different voltage patterns per sequencer (2V/4V/6V patterns)
- All sequencers set to 32 steps with split at step 16

## Use Cases

### Generative Patterns
- Set different section repeat counts to create evolving patterns
- Use split points to create verse/chorus structures
- Combine with clock division for polyrhythmic textures

### Melodic Sequencing
- Use the 3 outputs for pitch, gate, and velocity/modulation
- Set custom voltage values per step for specific melodic patterns
- Use Pingpong mode for palindromic melodies

### Modulation Sources
- Generate complex modulation patterns with section looping
- Use different sequencers for different modulation destinations
- Clock multiplication for fast, evolving modulation

### Rhythmic Control
- Use gate patterns with section repeats for variations
- Combine Forward/Backward modes with splits for interesting rhythms
- Clock division for slower evolving patterns

## Version History

### Build 1 (October 26, 2025)
- Initial release
- 4-channel 32-step sequencer with 3 CV outputs each
- Clock division/multiplication and direction control
- Section looping with independent repeat counts
- Visual step editor with page indicators
- Left encoder page switching between sequencers
- Automatic parameter reset when step count changes

## Technical Details

- **Algorithm GUID**: VSEQ
- **Total Parameters**: 39 (2 inputs + 12 outputs + 25 configuration)
- **Memory Usage**: Optimized for Disting NT SRAM
- **Preset Format**: JSON with full state serialization
- **Step Value Range**: int16_t (-32768 to 32767) mapped to 0-10V

## Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## License

See LICENSE file for details.

## Key Features

- **32 Virtual Faders**: 4 pages × 8 faders per page
- **Dual Control Modes**: 
  - **Note Mode**: Send specific MIDI notes with customizable scales
  - **Number Mode**: Send scaled values (0-100 range, customizable)
- **MIDI Output**: 
  - 7-bit CC (CC 1-32, values 0-127)
  - 14-bit CC (CC 0-31 MSB paired with CC 32-63 LSB, values 0-16383)
- **Pickup Modes**: 
  - **Scaled**: Physical pot position scales the fader's range
  - **Catch**: Fader doesn't change until pot catches the current value
- **Custom Naming**: Each fader has a 6-character name + 5-character category

### Basic Operation

#### Page Navigation
- **Left Encoder**: Rotate to select pages 1-4
- **Display**: Top-right corner shows current page number

#### Fader Selection
- **Right Encoder**: Rotate to select faders 1-8 within the current page
- **Display**: Selected fader is highlighted

#### Controlling Faders
- **Left Pot**: Controls the fader to the LEFT of selection (or first fader if leftmost selected)
- **Center Pot**: Controls the SELECTED fader
- **Right Pot**: Controls the fader to the RIGHT of selection (or last fader if rightmost selected)

## Fader Configuration

### Naming Faders

1. Select the fader you want to name
2. Press **Right Encoder Button** to enter name edit mode
3. **Left Encoder**: Navigate between characters (6 name + 5 category chars)
4. **Right Encoder**: Change the current character
5. Press **Left Encoder Button** to exit and remember to save the preset!

**Name Structure:**
- Characters 1-6: Fader name (displayed on main screen)
- Characters 7-11: Category (displayed in bottom-right corner when fader is selected)

### Fader Function Settings

From name edit mode, turn **Right Pot** to access function settings:

#### Display Mode
- **Number**: Displays values 0-100 (scaled to MIDI range)
- **Note**: Displays specific MIDI note numbers

#### Accidental (Note Mode only)
- **Sharp**: Display notes with sharps (C, C#, D, etc.)
- **Flat**: Display notes with flats (C, Db, D, etc.)

#### Top Value / Bottom Value
Defines the range for the fader:
- **Note Mode**: Set the top and bottom MIDI notes (0-127, displayed as note names)
- **Number Mode**: Set the top and bottom values (0-100)

The fader will snap to values within this range. Top must be greater than bottom.

#### Note Mask (Note Mode only)
Select which notes in the chromatic scale are active:
- Active notes shown in color 15
- Inactive notes shown in color 7
- Only active notes will be sent when moving the fader
- Useful for constraining to specific scales (pentatonic, major, minor, etc.)

### Macro Fader Settings

From name edit mode, turn **Right Pot** to page 3 to access macro fader settings. A macro fader controls multiple child faders to the right of it.

#### Control Count (0-31)
- **Off (0)**: Fader operates independently
- **1-31**: Number of faders to the right that this fader will control
- Maximum count is limited by faders remaining (can't control beyond fader 32)
- Cannot overlap with another macro fader

#### Control Mode
- **Absolute**: Children move in parallel with the macro fader
  - When macro moves up/down, all children move by the same amount
  - Maintains relative spacing between children
  - Best for groups that should move together (e.g., mix bus faders)

- **Relative**: Children scale proportionally based on macro position
  - Macro at 50% (center): children stay at their current positions
  - Macro below 50%: children scale toward 0.0 proportionally
  - Macro above 50%: children scale toward 1.0 proportionally
  - Manual child adjustments are preserved - reference values update continuously
  - Best for dynamic control (e.g., overall volume while preserving mix balance)

**Visual Indicators:**
- Macro faders show "M" on the right side
- Child faders show "C" on the right side

**Note:** When you manually adjust a child fader, its reference value updates. In Relative mode, this new position becomes the scaling reference for future macro movements.

## Parameters

### FADER 1-8
The 8 physical control parameters that map to the current page's virtual faders. These are what external controllers (like F8R) can map to.

### PAGE
Selects which page (1-4) is currently active. Hidden from parameter page but accessible via left encoder in the UI.

### MIDI Mode
- **7-bit CC**: Sends CC 1-32 with values 0-127
- **14-bit CC**: Sends CC 0-31 (MSB) paired with CC 32-63 (LSB), values 0-16383
- Default: 14-bit CC

### Pickup Mode
Controls how physical pot movement affects the virtual fader:
- **Scaled**: Pot position (0-100%) scales the fader's configured range
- **Catch**: Fader value doesn't change until pot "catches up" to it
  - Prevents value jumps when switching pages
  - Pot must cross the current fader value before changes take effect
- Default: Catch

### Pot Control
Enable or disable fader level changes via the three pots:
- **On**: Pots control fader levels (default)
  - Left pot controls the fader to the left of selected
  - Center pot controls the selected fader
  - Right pot controls the fader to the right of selected
  - Dotted lines appear under adjacent faders to show pot mapping
- **Off**: Pots do not change fader levels
  - Only the solid line appears under the selected fader
  - All three pots are disabled in this mode
  - Useful when you want to prevent accidental pot changes
- Default: On

## MIDI Output Details

### 7-bit Mode
- Faders 1-32 → MIDI CC 1-32
- Values: 0-127
- Standard single-byte MIDI CC messages

### 14-bit Mode
- Faders 1-32 → MIDI CC 0-31 (MSB) paired with CC 32-63 (LSB)
- Values: 0-16383 (high resolution)
- Follows standard MIDI 14-bit CC implementation
- Alternates sending MSB and LSB each step for efficiency

### MIDI Routing
- **USB MIDI**: Sent to USB host
- **Internal**: Available to other Disting NT algorithms via MIDI routing

**Note:** MIDI channel is currently hardcoded to channel 1.

## Use Cases

### Controlling a DAW
- Map VFader's MIDI CCs to DAW mixer faders, plugin parameters, or automation
- Use 14-bit mode for smooth, high-resolution control
- Create presets for different mixing sessions

### Modular Control via MIDI
- Control other Disting NT algorithms via internal MIDI routing
- Use Note mode to sequence melodic content
- Use Number mode for parameter control
- Route MIDI to external MIDI→CV converters for modular control

### External MIDI Gear
- Control hardware synths, effects, or lighting
- Use Note mode to play specific notes from a scale
- Save presets for different performance setups

### F8R Integration
- Map F8R's faders to VFader's FADER 1-8 parameters
- Control 8 faders simultaneously per page
- Switch pages for 32 total faders under F8R control
- Use MIDI output to control external gear or DAW

## Tips & Tricks

1. **Efficient Editing**: Use the 3-pot layout (Left/Center/Right) to adjust adjacent faders quickly without moving selection.

2. **Scale Constraints**: In Note mode, disable notes in the chromatic scale to constrain to specific musical scales.

3. **Value Ranges**: Set custom ranges (e.g., 20-80) to limit fader travel for fine control over a smaller parameter range.

4. **Pickup Mode**: Use Catch mode when switching between pages to avoid sudden value jumps.

5. **Categories**: Use the 5-char category field to organize faders (e.g., "MIX", "FILT", "ENV", "FX").

6. **14-bit Smoothness**: Enable 14-bit mode for ultra-smooth parameter changes, especially important for audio-rate parameters.

## Technical Specifications

- **Algorithm GUID**: VFDR
- **Build Version**: 47
- **Memory Usage**: Optimized for Disting NT SRAM
- **MIDI Channel**: 1 (hardcoded)
- **MIDI Destinations**: USB + Internal
- **Preset Format**: JSON with full state serialization

## Troubleshooting

### Fader not responding to pot
- Check that the correct page is selected
- Verify Pickup Mode settings (may need to "catch" the value in Catch mode)
- Ensure the pot is mapped to the correct FADER parameter
- Check that Pot Control is set to "On" (not "Off")

### MIDI not received by destination
- Verify MIDI routing in Disting NT settings
- Check that the receiving device is listening on channel 1
- Confirm USB MIDI connection (if using USB)

### Values jumping when switching pages
- Enable **Catch** pickup mode to prevent jumps
- Alternatively, use **Scaled** mode and adjust pots after page switch

## Version History

### Build 48 (October 2025)
- **Reduced drift control settings** to fix catch mode issues
- Removed "High" setting (was 2% threshold)
- "Med" renamed to "High" (1% threshold)
- Options now: Off, Low (0.5%), High (1%)
- Fixes issue where catch mode couldn't be reached when fader at zero with High drift setting

### Build 47 (October 2025)
- **Fixed macro fader initialization bug**
- Children no longer reset/snap on first macro fader movement
- `lastGangValues` now initialized to current macro position (not -1.0)
- Children maintain their positions when macro is first created
- Proper transformation from the moment macro is activated

### Build 46 (October 2025)
- **Removed CV output functionality** (abandoned feature)
- Removed all CV output parameters from parameter page
- Removed CV output generation code from step function
- Cleaner parameter UI with only essential controls

### Build 45 (October 2025)
- **Increased pot deadband** from 1.5% to 3% for better touch sensitivity
- Pots now require more movement before triggering value changes
- Reduces accidental changes from light touches
- **Fixed Relative mode macro fader behavior**
- Child faders no longer snap back to old positions after manual adjustment
- Reference values now update continuously in Relative mode
- Manual child fader changes are preserved when macro fader moves
- Proportional scaling maintains proper relationships between children

### Build 44 (October 2025)
- Added **Pot Control** parameter to enable/disable pot input
- When Pot Control is Off, all three pots are disabled
- GUI updated: dotted lines only appear when Pot Control is On
- Only the solid line under selected fader shows when pots are disabled
- Prevents accidental fader changes from pot movement
- Removed debug logging feature

### Build 43 (October 2025)
- Added macro fader functionality (gang fader control)
- Absolute and Relative control modes for macro faders
- Visual indicators (M and C) for macro and child faders
- Build number display in UI (bottom right corner)
- Improved parameter page layout (single scrollable page)

### v1.0 (October 2025)
- Initial release
- 32 virtual faders with 4-page layout
- Note and Number modes
- 7-bit and 14-bit MIDI output
- Scaled and Catch pickup modes
- Custom naming with categories
- Value range configuration (0-100 for Number, full MIDI range for Note)
- Chromatic scale masking for Note mode
- Visual tick marks at 25%, 50%, 75%
- Debug logging for troubleshooting

## Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## License

See LICENSE file for details.
