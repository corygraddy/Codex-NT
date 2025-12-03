# VTrig - 6-Track Trigger Sequencer

**VTrig** is a 6-track trigger/gate sequencer plugin for Expert Sleepers Disting NT.

## Features

- **6 Independent Trigger Tracks**: Each with 32 steps and dedicated output
- **Clock & Reset Inputs**: External sync with dedicated reset
- **Per-Track Parameters**:
  - Run/Stop control
  - Sequence length (1-32 steps)
  - Direction (Forward/Backward/Pingpong)
  - Clock divider with swing
  - Section looping with repeats
  - Fill patterns for last repeat
- **MIDI CC Output**: Configurable MIDI channel and CC per track
- **Velocity Control**:
  - Normal velocity (0-127, default 100)
  - Accent velocity (0-127, default 127)
  - Per-step accent markers (Off/Normal/Accent)
- **Custom UI**: Visual step sequencer with diamond markers for accents

## Parameters

### Inputs
- Clock In (1-28)
- Reset In (1-28)

### Gate Outputs
- Trigger MIDI Channel (Off, 1-16)
- Master Velocity (0-127)
- Master Accent (0-127)
- Track 1-6 Output Bus (0-28)
- Track 1-6 MIDI CC (0-127)

### Per-Track Parameters (Trig Track 1-6)
- Run (Stop/Run)
- Length (1-32)
- Direction (Forward/Backward/Pingpong)
- Clock Divider (/1, /2, /4, /8, /16, /32)
- Swing (0-99%)
- Split Point (0-31)
- Section 1 Repeats (1-99)
- Section 2 Repeats (1-99)
- Fill Start Step (1-32)

## Building

```bash
make
```

Builds `build/1VTrig.o` ready for deployment to Disting NT.

## Usage

1. Set Clock In and Reset In parameters to match your external sources
2. Configure each track's output bus assignment
3. Set MIDI channel if using MIDI CC output
4. Adjust per-track length, direction, and timing parameters
5. Use the custom UI to program trigger patterns with accent control

## Version History

- **v1.0** (2025-11-09): Initial release
  - 6-track trigger sequencer
  - MIDI CC output with velocity control
  - Accent velocity per step
  - Section looping and fill patterns
