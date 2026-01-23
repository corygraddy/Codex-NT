# VTrig - 6-Track Trigger Sequencer for Disting NT

A powerful 6-track trigger/gate sequencer with swing, fill patterns, section looping, and MIDI CC output.

## Features

### Core Sequencing
- **6 Independent Trigger Tracks**: Each with 32 steps
- **32 Steps per Track**: Configurable start and end points (First Step / Last Step)
- **Flexible Pattern Programming**: Enable/disable individual steps
- **Per-Track Clock Division**: 1, 2, 4, 8, 16, 32
- **Multiple Playback Modes**: Forward, Backward, Pingpong, Random

### Advanced Features
- **Swing**: Global swing parameter (50-75%) affects all tracks
- **Fill Patterns**: Automatic fill behavior on last repeat of Section 1
- **Section Looping**: Two-section structure with configurable repeat counts
- **Gate Length**: Adjustable gate length per track (1-100%)
- **MIDI CC Output**: Each track can send MIDI CC (0-127)

### Section Looping
- **Split Point**: Divides each sequence into Section 1 and Section 2
- **Repeat Counts**: Configure how many times each section repeats
- **Fill Feature**: On last repeat of Section 1, steps from fillStart (in Section 2) are added
- **Behavior**: Section 1 × Reps → Section 2 × Reps → Loop

### Page-Based UI
- **6 Pages**: One page per trigger track
- **Visual Display**:
  - Step grid with active steps shown
  - Current position indicator
  - Selected step underline
  - Page indicator (1-6 bars at top)
  - Gate 1-6 labels
  - Run status indicator

## Controls

### Encoders
- **Left Encoder**: Navigate between trigger tracks (1-6)
- **Right Encoder**: 
  - Turn: Select step (1-32, constrained to First Step - Last Step range)
  - Press: Toggle selected step on/off
- **Left Button**: Toggle Run status for selected track

### Pots
- **Left Pot**: Edit step pattern (turn to select, adjust to toggle)
- **Middle Pot**: Select page/track
- **Right Pot**: Select step

### CV Inputs
- **Clock In**: External clock input (parameter selects input 1-28, or 0 for internal)
- **Reset In**: External reset input (parameter selects input 1-28, or 0 for none)

## Parameters

### Global Parameters
- **Clock In**: Clock source (0 = internal, 1-28 = CV input)
- **Reset In**: Reset trigger source (0 = none, 1-28 = CV input)
- **Swing**: Swing amount 50-75% (50 = no swing, 75 = maximum swing)
- **First Step**: Starting step of sequence (1-32)
- **Last Step**: Ending step of sequence (1-32)
- **Split Point**: Divides sequence into two sections (1-32)
- **Sec1 Reps**: Number of times to repeat Section 1 (1-32)
- **Sec2 Reps**: Number of times to repeat Section 2 (1-32)
- **Fill Start**: Step in Section 2 where fill pattern begins (Split Point + 1 to 32)

### Per-Track Parameters (Gate 1-6)
- **Out**: CV output assignment (0 = off, 1-8 = CV bus)
- **Run**: Enable/disable track (On/Off)
- **Length**: Gate length 1-100% (percentage of clock period)
- **Direction**: Playback direction (Forward, Backward, Pingpong, Random)
- **Clock Div**: Clock division (1, 2, 4, 8, 16, 32)
- **MIDI**: MIDI CC number (0-127, 0 = disabled)

## Installation

1. Download `VTrig-Build1.zip`
2. Extract the ZIP file
3. Copy `VTrig.o` to your Disting NT's SD card: `/programs/plug-ins/`
4. Restart your Disting NT or reload plugins
5. VTrig will appear in the plugin menu

## Quick Start

1. **Select Track**: Turn left encoder to choose track (1-6)
2. **Select Step**: Turn right encoder to choose step
3. **Toggle Step**: Press right encoder button to enable/disable step
4. **Program Pattern**: Repeat steps 2-3 to create your rhythm
5. **Set Outputs**: Assign Out parameter (1-8) for each track
6. **Enable Track**: Set Run parameter to On
7. **Adjust Gate Length**: Use Length parameter to control gate duration
8. **Clock Input**: Set Clock In parameter (0 for internal, 1-28 for external)

## Section Looping with Fill Example

With settings:
- First Step: 1
- Last Step: 16
- Split Point: 8
- Sec1 Reps: 4
- Sec2 Reps: 1
- Fill Start: 13

Playback behavior:
1. **Repeats 1-3 of Sec1**: Steps 1-8 (normal pattern)
2. **Repeat 4 of Sec1** (last repeat): Steps 1-8 + steps 13-16 (adds fill!)
3. **Sec2**: Steps 9-16 (normal pattern)
4. Loop back to step 1

The fill adds extra steps from Section 2 on the last repeat of Section 1, creating dynamic variation in your patterns.

## Swing Explained

Swing delays every other clock tick, creating a "shuffle" feel:
- **50%**: No swing (straight time)
- **60%**: Subtle swing
- **66%**: Triplet swing (2:1 ratio)
- **75%**: Maximum swing

Swing affects all tracks globally and interacts with each track's clock division setting.

## Tips

- **Polyrhythms**: Use different Clock Div settings per track (e.g., track 1 = 4, track 2 = 3)
- **Gate Length**: Short gates (10-20%) for hi-hats, longer gates (50-80%) for bass drums
- **Fill Patterns**: Set Fill Start to create automatic fills (great for live performance)
- **Section Looping**: Perfect for verse/chorus structures or building/breaking patterns
- **Random Mode**: Disables section looping, plays random steps from First-Last range
- **MIDI CC**: Send MIDI CC to control external gear or soft synths
- **Swing**: Try 62-66% for classic swing feel

## Requirements

- Disting NT with latest firmware
- SD card with `/programs/plug-ins/` folder

## Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## License

See LICENSE file in root directory for details.
