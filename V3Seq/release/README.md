# V3Seq - 3-Output CV Sequencer for Disting NT

A versatile 3-output CV sequencer with configurable voltage ranges, section looping, and flexible playback modes.

## Features

### Core Sequencing
- **3 Independent CV Outputs**: Each with 32 steps and configurable voltage range
- **32 Steps per Sequence**: Configurable start and end points (First Step / Last Step)
- **4 Voltage Ranges**: 0-5V, 0-10V, -5V to +5V, -10V to +10V
- **3 MIDI CC Outputs**: Parallel MIDI control for each CV output

### Playback Modes
- **Forward**: Play from first to last step
- **Backward**: Play from last to first step
- **Pingpong**: Bounce between first and last steps
- **Random**: Random step selection

### Section Looping
- **Split Point**: Divides sequence into Section 1 and Section 2
- **Repeat Counts**: Configure how many times each section repeats before moving to the next
- **Section 1**: Steps from First Step to Split Point
- **Section 2**: Steps from Split Point + 1 to Last Step
- **Behavior**: Plays Section 1 × Repeat Count, then Section 2 × Repeat Count, then loops

### Value Editing
- **Coarse Mode** (default): 25 steps across full voltage range for quick adjustments
- **Fine Mode**: 500 steps across full voltage range for precise control
- Toggle between modes by pressing the right encoder button
- Fine mode indicator: "F" appears in the display

### Page-Based UI
- **3 Pages**: One page per CV output
- **Visual Feedback**: 
  - Bar graph showing step values
  - Reference dots at 0%, 25%, 50%, 75%, 100%
  - Current position indicator
  - Selected step underline
  - Page indicator at top (1, 2, or 3 bars)

## Controls

### Encoders
- **Left Encoder**: Select between pages (CV1, CV2, CV3)
- **Right Encoder**: 
  - Turn: Select step (1-32, constrained to First Step - Last Step range)
  - Press: Toggle between Coarse and Fine edit modes

### Pots
- **Left Pot**: Edit value of selected step
- **Middle Pot**: Select page (with catch behavior)
- **Right Pot**: Select step

### CV Inputs
- **Clock In**: External clock input (parameter selects input 1-28, or 0 for internal)
- **Reset In**: External reset input (parameter selects input 1-28, or 0 for none)

## Parameters

### Global Parameters
- **Clock In**: Clock source (0 = internal, 1-28 = CV input)
- **Reset In**: Reset trigger source (0 = none, 1-28 = CV input)
- **Voltage Range**: Output voltage range (0-5V, 0-10V, -5-+5V, -10-+10V)
- **First Step**: Starting step of sequence (1-32)
- **Last Step**: Ending step of sequence (1-32)
- **Split Point**: Divides sequence into two sections (1-32)
- **Sec1 Reps**: Number of times to repeat Section 1 (1-32)
- **Sec2 Reps**: Number of times to repeat Section 2 (1-32)

### Per-Output Parameters (CV1, CV2, CV3)
- **Out**: CV output assignment (0 = off, 1-8 = CV bus)
- **MIDI**: MIDI CC number (0-127)
- **Clock Div**: Clock division (1, 2, 4, 8, 16, 32)
- **Direction**: Playback direction (Forward, Backward, Pingpong, Random)

## Installation

1. Download `V3Seq-Build1.zip`
2. Extract the ZIP file
3. Copy `V3Seq.o` to your Disting NT's SD card: `/programs/plug-ins/`
4. Restart your Disting NT or reload plugins
5. V3Seq will appear in the plugin menu

## Quick Start

1. **Select Page**: Turn left encoder to choose CV output (1, 2, or 3)
2. **Select Step**: Turn right encoder to choose step to edit
3. **Edit Value**: Turn left pot to adjust step value
4. **Fine Tune**: Press right encoder to enter fine mode ("F" appears)
5. **Configure Range**: Set Voltage Range parameter for your needs
6. **Set Sequence Length**: Use First Step and Last Step parameters
7. **Section Looping**: Set Split Point, Sec1 Reps, and Sec2 Reps as desired
8. **Assign Outputs**: Set Out parameter (1-8) for each CV output
9. **Clock Input**: Set Clock In parameter (0 for internal, 1-28 for external)

## Section Looping Example

With settings:
- First Step: 1
- Last Step: 16
- Split Point: 8
- Sec1 Reps: 3
- Sec2 Reps: 1

Playback order:
1. Steps 1-8 (repeat 3 times)
2. Steps 9-16 (repeat 1 time)
3. Loop back to step 1

Total pattern length: (8 steps × 3) + (8 steps × 1) = 32 steps before loop

## Tips

- **Quick Setup**: Start with 0-10V range for maximum flexibility
- **Voltage Scaling**: Use -5-+5V for bipolar control (LFOs, pitch)
- **Section Looping**: Great for verse/chorus structures or modulation patterns
- **Fine Mode**: Essential for precise tuning of pitch sequences
- **MIDI CC**: Use parallel MIDI control to modulate soft synth parameters
- **Random Mode**: Disable section looping (acts on full First-Last range)

## Requirements

- Disting NT with latest firmware
- SD card with `/programs/plug-ins/` folder

## Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## License

See LICENSE file in root directory for details.
