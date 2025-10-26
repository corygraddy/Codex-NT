# VFader Build 50 - Release Notes

**Date:** October 26, 2025

## Major Improvements

### Full 14-bit MIDI Resolution
- Parameters now use full 0-16383 range (was 0-1000)
- Smooth, continuous parameter control without value skipping
- True 14-bit MIDI output for maximum precision

### Fixed Catch Mode Pickup Logic
- **No more getting stuck in pickup loops** - catch mode now properly exits when you move the control
- **No more value jumps** - large mismatches (>10%) always enter pickup mode smoothly
- **Scaled pickup behavior when far away** - catch mode uses proportional scaling when >5% from target
- **Smooth slewing transition** - automatically slews to target when within 5%
- **Smart movement detection** - continuous same-direction movement properly detected and handled

### Improved Endpoint Reliability
- Increased min/max deadzones to 3% (was 1%) for more reliable endpoint hitting
- Deadzones applied consistently to both incoming values and current state
- Sticky endpoints removed (no longer needed with larger deadzones)

### Pot Control Enhancements
- Pot values quantized to 2500 steps to eliminate jitter and backtracking
- Prevents unwanted catch triggers from pot noise
- Smoother, more predictable pot behavior

### Intelligent Pickup Mode Entry/Exit
- **Big mismatch (>10%)**: Always enters pickup mode to prevent jumps
- **Small mismatch (1-10%)**: Enters pickup unless doing a fast sweep
- **Tiny mismatch (<1%)**: Updates directly for immediate response
- Movement-based exit only when already close (<10%) to prevent premature exits
- 5-frame settle period after exiting pickup to prevent jitter re-entry
- Settle period clears immediately on active movement

### Visual Feedback
- Decimal digit display (0-9) shows fine resolution next to main value (0-100)
- Provides visual confirmation of 14-bit precision

## Technical Details

### Coarse Resolution Comparisons
- Pickup mode logic uses 500-step coarse resolution for comparisons
- Prevents false triggers from minor value fluctuations
- Maintains full 14-bit output resolution while filtering noise

### Movement Detection
- Threshold: 0.001f (~0.5 coarse steps)
- Properly accounts for pot quantization
- Reliable detection of continuous same-direction movement

### Catch Mode Behavior Zones
1. **Far (>5%)**: Scaled pickup - value scales proportionally with pot movement
2. **Medium (2-5%)**: Slewing - smoothly slews toward pot position
3. **Close (<0.4%)**: Direct follow - exits pickup and tracks pot directly

## Installation

1. Copy `VFader_Build50.o` to your Disting NT SD card
2. Rename to `VFader.o` if needed
3. Load the algorithm on your Disting NT

## Compatibility

- Requires Disting NT firmware (current version)
- Compatible with F8R fader controller
- Supports 14-bit MIDI output
- 32 virtual faders across 4 pages

## Known Issues

None currently reported.

## Previous Issues Fixed

- ✅ Parameters showing 0-1000 instead of full 14-bit range
- ✅ MIDI output quantizing to 0-100 before scaling to 14-bit
- ✅ Endpoints not hitting exact min/max values reliably
- ✅ Fast fader movements triggering unwanted pickup mode
- ✅ Catch mode getting stuck in pickup loops
- ✅ Value jumps when pot position differs from fader value
- ✅ Pot jitter causing backtracking and unwanted catches

---

**Build:** 50  
**Compiler:** ARM GCC for Cortex-M7  
**Warnings:** Minor unused variable warnings (non-critical)  
**Status:** Stable Release
