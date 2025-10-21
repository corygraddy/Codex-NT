# EdgeLike (Disting NT plugin)

An Edge-inspired percussive synth voice for Expert Sleepers disting EX/NT C++ API.

- 2 oscillators (tri/pulse shape mix), hard sync, light linear FM
- White/pink noise blend
- 4-pole cascade filter (LP/HP) with drive and resonance
- Three decay envelopes: pitch, VCF, VCA
- Trigger input, audio output

This approximates the Behringer Edge voice architecture; the internal sequencer and patch bay are not implemented here.

## Build

Requires the distingNT_API headers and ARM toolchain. The Makefile assumes `../distingNT_API`.

- Output: `build/EdgeLike.o`

## Deploy

- Insert SD card, ensure it mounts as `/Volumes/Untitled` (or edit `BuildTransfer.sh`)
- Run the script to copy and eject

## Parameters (pages)

- OSC: VCO tunes, shape mixes, pulse width, FM depth, Sync
- NOISE: color and level
- FILTER: mode, cutoff, resonance, drive, Env→VCF amount
- ENVS: Pitch Decay/Amt, VCF Decay, VCA Decay
- IO: Trig In, Audio Out

## Notes

- Filter is a fast 4-pole cascade stand-in; upgrade to a ZDF ladder for closer Edge character.
- FM kept conservative to avoid aliasing; try triangle as FM source for smoother sweeps.