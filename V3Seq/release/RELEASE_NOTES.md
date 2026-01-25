# V3Seq Build 1 Release Notes

## Initial Release - January 23, 2026

First public release of V3Seq, a 3-output CV sequencer for Disting NT.

## Features

### Core Functionality
✅ 3 independent CV outputs with 32 steps each  
✅ 4 configurable voltage ranges (0-5V, 0-10V, -5-+5V, -10-+10V)  
✅ Configurable sequence range (First Step / Last Step parameters)  
✅ 3 playback modes: Forward, Backward, Pingpong  
✅ Per-output clock division (1, 2, 4, 8, 16, 32)  
✅ Step values persist in presets  

### Section Looping
✅ Two-section structure with split point  
✅ Configurable repeat counts for each section  
✅ Automatic looping behavior  

### Value Editing
✅ Coarse mode: 25 steps across voltage range  
✅ Fine mode: 500 steps for precise control  
✅ Toggle between modes with right encoder button  
✅ Mode display shows "coarse" or "fine"  

### User Interface
✅ Page-based display (one page per CV output)  
✅ Bar graph visualization with reference dots  
✅ Current position and selected step indicators  
✅ Page indicator at top (1-3 bars)  

## Installation

Copy `V3Seq.o` to your Disting NT's SD card at `/programs/plug-ins/` and restart the device.

## Known Limitations

- Section looping disabled in Pingpong mode (full sequence bounce)

## Credits

Developed by Cory Graddy  
Built with GitHub Copilot: Claude Sonnet 4.5
