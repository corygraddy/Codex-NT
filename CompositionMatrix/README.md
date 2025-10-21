# CompositionMatrix (Disting NT Plug-in)

A custom C++ plug-in for the Expert Sleepers Disting NT. Current MVP: single voice (pitch/gate) driven by a clocked shift register with freeze snapshot, custom UI, and JSON persistence.

## Prerequisites
- macOS
- ARM GCC toolchain: `arm-none-eabi-gcc`/`arm-none-eabi-g++` in PATH
- This repo checked out alongside `distingNT_API` (expected at `../distingNT_API`)

## Build
- From `CompositionMatrix/`:
  - `make` builds `build/CompositionMatrix.o`
- Notes:
  - Makefile expects headers in `../distingNT_API/include`.
  - Output is a relocatable `.o` (required by the NT host).

## Deploy
- Copy `build/CompositionMatrix.o` to the SD card at `/programs/plug-ins/`.
- Or use `BuildTransfer.sh` (ensure SOURCE_DIR and DEST_DIR are correct; use your SD volume name).

## Parameters (MVP)
- Global Key/Scale/Polyphony (placeholders for future phases)
- Clock In (CV bus, 0 = none)
- Freeze In (CV bus, 0 = none)
- Pitch Out (CV bus, 0 = none)
- Gate Out (CV bus, 0 = none)

## UI
- Custom UI renders basic info.
- Press the center pot button to snapshot the current shift register and bump the hidden `State Version` (for persistence testing).

## Notes
- Gate width is time-based (~10ms) using the host sample rate.
- Pitch quantization uses exact semitone math for a C major pentatonic mapping across octaves.
- Logging stub available (disabled by default). Define `CMX_ENABLE_LOGGING` to write `engine_log.txt` to the plug-ins folder (avoid during real-time processing).

## Roadmap
- Polyphony, harmonic engine, dynamics, and song mode (see `CompositionMatrix Brief`).