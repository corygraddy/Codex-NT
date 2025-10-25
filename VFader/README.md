# VFader

**Build 43** - 32 Virtual Faders with MIDI CC and CV Output for Disting NT

## Overview

VFader is a Disting NT algorithm that provides 32 virtual faders organized into 4 pages of 8 faders each. Each fader can be controlled via the three pots (L, C, R) and outputs both MIDI CC messages (7-bit or 14-bit) and direct CV outputs.

## Key Features

- **32 Virtual Faders**: 4 pages × 8 faders per page
- **8 CV Outputs**: Direct voltage control (0-10V) from any fader
- **Dual Control Modes**: 
  - **Note Mode**: Send specific MIDI notes with customizable chromatic scales
  - **Number Mode**: Send scaled values (0-100 range, customizable)
- **MIDI Output**: 
  - 7-bit CC (CC 1-32, values 0-127)
  - 14-bit CC (CC 0-31 MSB paired with CC 32-63 LSB, values 0-16383)
- **CV Output**:
  - 8 configurable CV outputs
  - Route to any of 28 disting NT output buses
  - Map each CV output to any of the 32 faders
  - Add or Replace output modes
  - 0-10V range based on fader position
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
  - Example: With range 20-80, pot at 50% sets value to 50
- **Catch**: Fader value doesn't change until pot "catches up" to it
  - Prevents value jumps when switching pages
  - Pot must cross the current fader value before changes take effect
- Default: Catch

### Debug Log
- **Off**: Normal operation
- **On**: Captures detailed state information in preset JSON for troubleshooting
- Default: Off

### CV Out 1-8
Each CV output has three parameters (scroll down on the VFADER parameter page):
- **CV Out N**: Select output bus (1-28)
- **CV Out N mode**: Add (mix with existing signal) or Replace (overwrite)
- **CV Out N Fader**: Map to a fader (None, or Fader 1-32)

CV outputs generate 0-10V based on the mapped fader's position (0.0-1.0 → 0-10V).

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

### Controlling External Modules via CV
- Map VFader outputs directly to module CV inputs (V/Oct, filter cutoff, etc.)
- No need for separate MIDI→CV conversion
- Control up to 8 parameters simultaneously
- Perfect for complex patches with multiple modules

### Controlling a DAW
- Map VFader's MIDI CCs to DAW mixer faders, plugin parameters, or automation
- Use 14-bit mode for smooth, high-resolution control
- Create presets for different mixing sessions

### Modular Control
- Control other Disting NT algorithms via internal MIDI routing
- Use Note mode to sequence melodic content
- Use Number mode for CV-like parameter control

### External MIDI Gear
- Control hardware synths, effects, or lighting
- Use Note mode to play specific notes from a scale
- Save presets for different performance setups

### F8R Integration
- Map F8R's faders to VFader's FADER 1-8 parameters
- Control 8 faders simultaneously per page
- Switch pages for 32 total faders under F8R control
- CV outputs enable direct modular control alongside MIDI

## Tips & Tricks

1. **Efficient Editing**: Use the 3-pot layout (Left/Center/Right) to adjust adjacent faders quickly without moving selection.

2. **Scale Constraints**: In Note mode, disable notes in the chromatic scale to constrain to specific musical scales.

3. **Value Ranges**: Set custom ranges (e.g., 20-80) to limit fader travel for fine control over a smaller parameter range.

4. **Pickup Mode**: Use Catch mode when switching between pages to avoid sudden value jumps.

5. **Categories**: Use the 5-char category field to organize faders (e.g., "MIX", "FILT", "ENV", "FX").

6. **14-bit Smoothness**: Enable 14-bit mode for ultra-smooth parameter changes, especially important for audio-rate parameters.

## Technical Specifications

- **Algorithm GUID**: VFDR
- **Build Version**: 43
- **Memory Usage**: Optimized for Disting NT SRAM
- **MIDI Channel**: 1 (hardcoded)
- **MIDI Destinations**: USB + Internal
- **CV Outputs**: 8 configurable outputs, 0-10V range
- **Output Buses**: Routes to any of 28 disting NT buses
- **Preset Format**: JSON with full state serialization

## Troubleshooting

### Fader not responding to pot
- Check that the correct page is selected
- Verify Pickup Mode settings (may need to "catch" the value in Catch mode)
- Ensure the pot is mapped to the correct FADER parameter

### CV output not working
- Check that the CV output is mapped to a fader (not "None")
- Verify the output bus routing matches your patch
- Ensure the output mode (Add/Replace) is appropriate
- Check that the mapped fader has a non-zero value

### MIDI not received by destination
- Verify MIDI routing in Disting NT settings
- Check that the receiving device is listening on channel 1
- Confirm USB MIDI connection (if using USB)

### Values jumping when switching pages
- Enable **Catch** pickup mode to prevent jumps
- Alternatively, use **Scaled** mode and adjust pots after page switch

### Debug Mode
Enable Debug Log parameter and save a preset to capture detailed state information in the JSON file.

## Version History

### Build 43 (October 2025)
- Added 8 CV outputs with fader mapping
- CV outputs generate 0-10V based on fader position
- Configurable output bus routing (1-28)
- Add/Replace output modes
- Build number display in UI (bottom right corner as "B43")
- Improved parameter page layout (single scrollable page)
- Updated description to include CV outputs

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
