# VSeq - Hybrid CV & Trigger Sequencer for Disting NT

**Version 2.0** - 3 CV Sequencers + 6-Track Trigger Sequencer

## Overview

VSeq combines three 32-step CV sequencers (3 outputs each) with a six-track trigger sequencer in a single Disting NT algorithm. Each sequencer has independent clock division, direction control, section looping, and the trigger tracks feature swing and fill patterns for dynamic rhythm generation.

**Total Outputs:**
- 9 CV outputs (3 sequencers × 3 outputs)
- 6 trigger outputs (independent gate tracks)
- Clock and Reset inputs

## Features

### CV Sequencers (3 channels)
- **32 steps** per sequencer with **3 CV outputs** each (9 total CV outputs)
- **Playback modes:** Forward, Backward, Pingpong
- **Clock division/multiplication:** /16, /8, /4, /2, x1, x2, x4, x8, x16
- **Variable step count:** 1-32 steps
- **Section looping:** Split sequences with independent repeat counts for each section
- **Visual editor:** Two rows of 16 steps with 3 vertical bars per step showing CV values
- **Voltage range:** 0-10V per output

### Trigger Sequencer (6 tracks)
- **32 steps** per track with **6 independent gate outputs**
- **Playback modes:** Forward, Backward, Pingpong
- **Clock division/multiplication:** /16, /8, /4, /2, x1, x2, x4, x8, x16
- **Gate length:** 1-99 milliseconds
- **Swing:** 0-99% (shuffle timing for even steps)
- **Section looping:** Split patterns with independent repeat counts
- **Fill mode:** Jump to fill section on button press
- **Run/Stop:** Enable/disable individual tracks
- **Visual editor:** 6 horizontal rows showing active steps per track

## UI Controls

### Hardware
- **Left Encoder:** Select sequencer (CV 1-3, or Trigger)
- **Right Encoder:** Select step (0-31)
- **Left Pot:** Adjust Output 1 / Select track (in trigger mode)
- **Center Pot:** Adjust Output 2 / Edit step value
- **Right Pot:** Adjust Output 3
- **Button 4:** Trigger fill mode (trigger sequencer only)
- **Right Encoder Button:** Toggle between edit modes

### Display
- **CV Mode:** Three vertical bars per step show output voltages (0V bottom, 10V top)
- **Trigger Mode:** Six horizontal rows show active steps per track
- **Indicators:** 
  - Current step marked with dot
  - Selected step underlined
  - Page indicator bars at top

## Parameters

### Global
- **Clock In** (CV Input 1-28): External clock input
- **Reset In** (CV Input 1-28): Reset all sequencers to step 0

### CV Sequencer 1 (Seq 1)
- **Seq 1 Out 1/2/3** (CV Output): Three independent CV outputs
- **Seq 1 Clock Div** (/16 to x16): Clock division/multiplication
- **Seq 1 Direction** (Forward/Backward/Pingpong): Playback direction
- **Seq 1 Steps** (1-32): Number of active steps
- **Seq 1 Split Point** (1-31): Where section 1 ends, section 2 begins
- **Seq 1 Sec1 Reps** (1-99): Repeat count for section 1
- **Seq 1 Sec2 Reps** (1-99): Repeat count for section 2

### CV Sequencer 2 (Seq 2)
*Same parameter structure as Seq 1*

### CV Sequencer 3 (Seq 3)
*Same parameter structure as Seq 1*

### Trigger Track 1-6 (Gate 1-6)
Each track has:
- **Gate Out** (CV Output): Trigger/gate output
- **Run** (On/Off): Enable/disable track
- **Length** (1-99ms): Gate pulse duration
- **Direction** (Forward/Backward/Pingpong): Playback direction
- **ClockDiv** (/16 to x16): Clock division/multiplication
- **Swing** (0-99%): Timing offset for even steps (50% = straight)
- **Split** (1-31): Section boundary
- **Sec1 Reps** (1-99): Section 1 repeat count
- **Sec2 Reps** (1-99): Section 2 repeat count
- **Fill Start** (0-31): First step of fill pattern

## Section Looping

Each sequencer splits into two sections:
- **Section 1:** Steps 0 to Split Point
- **Section 2:** Steps Split Point to Step Count

The sequencer plays Section 1 for its repeat count, then Section 2 for its repeat count, then loops back. This creates complex rhythmic variations and song-structure patterns.

**Note:** In Pingpong mode, section looping is disabled and the full sequence plays.

## Swing & Fill (Trigger Tracks Only)

### Swing
- Delays even-numbered steps by the swing percentage
- 0% = straight timing, 50% = no swing, 99% = maximum shuffle
- Creates groove and humanization

### Fill Mode
- Press **Button 4** to jump all tracks to their Fill Start position
- Allows dynamic pattern variations during performance
- Automatically returns to normal sequence after fill section completes

## Installation

1. Copy `1VSeq.o` to `/programs/plug-ins/` on your Disting NT SD card
2. Restart Disting NT or reload plugins
3. Select "VSeq" from the algorithm menu

## Use Cases

- **Melodic sequencing:** Use CV outputs for pitch, velocity, modulation
- **Polyrhythmic patterns:** Different clock divisions per sequencer
- **Drum programming:** 6-track trigger sequencer with swing and fills
- **Generative music:** Section looping creates evolving patterns
- **Live performance:** Dynamic fills and independent track control

## Technical Details

- **Algorithm GUID:** VSEQ
- **Memory:** Optimized for Disting NT SRAM
- **Step Resolution:** 32 steps per sequencer/track
- **CV Range:** 0-10V (int16_t internally)
- **Gate Timing:** 1-99ms pulse width
- **Preset Format:** JSON with full state serialization

## Version History

### Version 2.0 (November 2025)
- Changed from 4 CV sequencers to 3 CV + 6-track trigger sequencer
- Added swing timing for trigger tracks
- Added fill mode for dynamic pattern variations
- Added per-track run/stop control
- Added gate length control (1-99ms)

### Version 1.0 (October 2025)
- Initial release with 4 CV sequencers
- 32 steps, 3 outputs per sequencer
- Clock division and direction control
- Section looping with independent repeats

## Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## License

See LICENSE file for details.
