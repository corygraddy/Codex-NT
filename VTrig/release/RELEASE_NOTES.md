# VTrig Build 1 Release Notes

## Initial Release - January 23, 2026

First public release of VTrig, a 6-track trigger sequencer for Disting NT.

## Features

### Core Functionality
✅ 6 independent trigger tracks with 32 steps each  
✅ Configurable sequence range (First Step / Last Step parameters)  
✅ 3 playback modes: Forward, Backward, Pingpong  
✅ Per-track clock division (1, 2, 4, 8, 16, 32)  
✅ Adjustable gate length per track (1-100%)  
✅ Individual track run/stop control  
✅ Step data persists in presets  

### Advanced Features
✅ Global swing parameter (50-75%)  
✅ Section looping with two-section structure  
✅ Fill pattern feature (triggered on last repeat of Section 1)  
✅ Visual step grid with current position indicator  

### User Interface
✅ Page-based display (one page per trigger track)  
✅ Step grid visualization  
✅ Current position and selected step indicators  
✅ Page indicator at top (1-6 bars)  
✅ Run status display  

## Installation

Copy `VTrig.o` to your Disting NT's SD card at `/programs/plug-ins/` and restart the device.

## Known Limitations

- Section looping disabled in Pingpong mode (full sequence bounce)
- Fill feature only works in Forward mode and when Fill Start is within Section 1
- User must manually adjust Fill Start to be less than Split Point

## Usage Tips

- **Polyrhythms**: Use different clock divisions per track
- **Fill Patterns**: Set Fill Start parameter to add automatic variation
- **Swing**: Try 62-66% for classic swing feel
- **Gate Length**: Shorter gates for hi-hats, longer for kicks

## Credits

Developed by Cory Graddy  
Built with GitHub Copilot: Claude Sonnet 4.5
