# VFader v1.0 - Remaining Features to Add

## 1. Control Indicators (Visual Feedback) 🎯
**Description:** Show which faders are controlled by which pots
- Solid line under CENTER pot's fader
- Dotted/dashed line under LEFT pot's fader  
- Dotted/dashed line under RIGHT pot's fader

**Implementation Notes:**
- Variables leftLocal/rightLocal were removed but calculation was simple
- Draw below faders or in a strip area
- Use different line styles or colors to distinguish

**Effort:** 30 minutes
**Priority:** HIGH - Important UX feedback

---

## 2. Enhanced Name Edit Mode with Fader Configuration 🎛️
**Description:** Expand the name edit screen to show configuration options and allow setting fader display format and range

**Right Side Panel Contents:**
- Show MIDI CC# for this fader (e.g., "CC 1", "CC 17", etc.)
- Display format selector
- Range selector

**Display Format Options:**
1. **Notes** - Musical note format (e.g., C3, D#4, A5)
2. **Integer** - Whole numbers (e.g., 0, 50, 127)
3. **Decimal** - Precise decimal format (e.g., 0.000, 1.234, 5.000)

**Range Options (examples):**
- For Notes: C3-C5, A0-C8, A3-A4, etc.
- For Integer: 0-100, -100 to 100, 1-1000, 0-127, etc.
- For Decimal: 0-5v (0.000-5.000), -5v to +5v (-5.000 to 5.000), 0-1 (0.000-1.000)

**Storage Requirements:**
- Per-fader display format (enum: 0=notes, 1=integer, 2=decimal)
- Per-fader range minimum value (float)
- Per-fader range maximum value (float)
- Add to serialization (save/load with presets)

**Conversion Logic:**
- Internal fader value always 0.0-1.0
- On display: scale to selected range and format
- For notes: map 0.0-1.0 to MIDI note numbers, convert to note names

**UI Workflow:**
- Enter name edit mode (existing)
- Character-based editing for all fields (reuse existing character navigation)
- Left encoder: move between rows (name, format, range min, range max)
- Right encoder: edit characters in current row
- Right button: exit when done

**Updated Effort:** 1.5-2 hours (simplified with character-based UI)
**Bug Risk:** LOW-MEDIUM
**Priority:** HIGH - Major usability enhancement for different use cases

---

## 3. Enhanced Debug Data for CV Troubleshooting 🔍
**Description:** Add comprehensive CV-specific debug data to preset JSON for easier troubleshooting

**Debug Data to Include:**
- CV destination for each of 32 faders (which bus or MIDI only)
- Last CV value sent to each bus (In 1-12, Out 1-8, Aux 1-8)
- Step counter and timing information
- Parameter change history (last N parameter changes with timestamps)
- CV output validation (confirm busFrames writes)
- MIDI send counter per fader
- Pickup mode state for all faders

**Benefits:**
- Compare JSON before/after when CV breaks
- Validate F8R mapping is reaching VFader correctly
- Track which faders have CV configured
- Verify CV values are being output to busFrames
- Debug parameter value ranges and conversions

**Implementation Notes:**
- Extend existing debugSnapshot struct
- Add to serialise() function
- Low overhead - only written on preset save

**Effort:** 45 minutes
**Bug Risk:** LOW (read-only debug data)
**Priority:** HIGH - Critical for CV feature development

---

## 4. Fader Reordering (Swap Adjacent Faders) 🔄
**Description:** Allow swapping a fader with the adjacent fader to reorder the layout

**Functionality:**
- In config/edit mode, add option to swap fader with neighbor
- Swap left or swap right
- Swaps all data: value, name, display format, range, CV destination, etc.

**Implementation:**
- Add swap option in name edit/config screen
- Button or encoder action to trigger swap
- Swap all arrays at two indices:
  - internalFaders[i] ↔ internalFaders[i±1]
  - faderNames[i] ↔ faderNames[i±1]
  - faderDisplayFormat[i] ↔ faderDisplayFormat[i±1]
  - faderRangeMin[i] ↔ faderRangeMin[i±1]
  - faderRangeMax[i] ↔ faderRangeMax[i±1]
  - Any other per-fader settings
- Handle edge cases (can't swap fader 0 left, can't swap fader 31 right)
- Update selection to follow the fader

**UI Workflow:**
- In config mode for a fader
- Option to "Swap L" or "Swap R"
- Immediate swap with visual feedback
- Exit config mode after swap

**Effort:** 30-45 minutes
**Bug Risk:** LOW-MEDIUM (array operations, edge cases)
**Priority:** MEDIUM - Nice workflow enhancement

---

## 5. Flexible Fader Count and Signal Type Configuration 🎚️
**Description:** Support 1-64 faders with per-fader signal type and addressing

**Global Settings (in parameter UI):**
- **Fader Count:** 1-64 faders (default: 32)
- Number of pages adjusts automatically (8 faders per page)
- 32 faders = 4 pages, 64 faders = 8 pages, 16 faders = 2 pages, etc.

**Per-Fader Configuration (in fader config screen):**
- **Signal Type:** 7-bit MIDI / 14-bit MIDI / CV
- **Number/Address:** Depends on signal type:
  - **14-bit MIDI:** CC 0-31 (default mapping: fader 1→CC0, fader 2→CC1, etc.)
  - **7-bit MIDI:** CC 0-127 (or 65-96 as suggested range)
  - **CV:** Output 1-8 (Out 1-8 buses)

**Storage Requirements:**
- Expand all arrays from [32] to [64]
  - internalFaders[64]
  - faderNames[64][8]
  - faderDisplayFormat[64]
  - faderRangeMin[64], faderRangeMax[64]
  - faderSignalType[64] (0=7bit, 1=14bit, 2=CV)
  - faderAddress[64] (CC# or CV output#)
  - lastMidiValues[64], pickup arrays[64], etc.
- Global faderCount parameter (1-64)

**Implementation Notes:**
- Default: 32 faders, all 14-bit MIDI, CC 0-31
- step() function: switch based on faderSignalType[i]
  - 7-bit: send single CC message
  - 14-bit: send MSB/LSB pair (standard CC pairing)
  - CV: write to busFrames[faderAddress[i]]
- draw(): calculate numPages = (faderCount + 7) / 8
- Bounds checking on page navigation
- Allow duplicate CC assignments (user's choice)

**Benefits:**
- Full flexibility for different workflows
- Mix MIDI and CV in same preset
- Use up to 64 7-bit CCs if needed
- Scale down to fewer faders for simpler setups

**Effort:** 2-3 hours
**Bug Risk:** MEDIUM
  - Array expansion straightforward
  - Per-fader signal routing requires careful switch logic
  - CV output needs testing (previous issues)
  - Page calculation and navigation bounds checking
**Priority:** HIGH - Major feature expansion, enables many use cases

---

## 6. Master Fader Control (Control Multiple Faders) 🎛️➡️🎛️🎛️

**Description:** Allow a fader to act as a master/group control for a range of other faders, with selectable mode:

**Signal Type Addition:**
- Add **"Fader"** as 4th signal type option (alongside 7-bit, 14-bit, CV)
- When set to "Fader" type, specify a range of target faders
- Add **Mode**: "relative" or "absolute"

**Configuration:**
- **Signal Type:** Fader (instead of MIDI/CV)
- **Target Range:** Start fader # and End fader # (e.g., "5-12" controls faders 5 through 12)
- **Mode:**
  - **Relative:** Master fader acts as a percent multiplier for child faders. If master is at 50%, child at 60%, output is 30%.
  - **Absolute:** Child faders follow master directly, but only move as far as their own max/min allows. If master moves past a child's max, child stops until master returns.

**Behavior:**
- When master fader value changes, update all target faders according to selected mode
- Target faders still output their configured signal (MIDI/CV)
- Master fader itself doesn't send MIDI/CV (only controls other faders)
- Allows hierarchical control and grouping

**Use Cases:**
- Master volume for multiple channels
- Gang multiple faders together
- Create submix groups
- One physical control → multiple CV/MIDI outputs
- Advanced group automation

**Implementation:**
- In parameterChanged() or step(): when master fader changes, update target range
- For relative mode: output = master × child
- For absolute mode: output = min(child.max, max(child.min, master))
- Validate target range (start ≤ end, within bounds, not self-referencing)
- Handle circular references (fader A controls B, B controls A - prevent infinite loop)
- Visual indicator on display that fader is a "master" type and which mode is active

**Storage:**
- faderSignalType[i] = 3 (for "Fader" type)
- faderTargetStart[64] - start of target range
- faderTargetEnd[64] - end of target range
- faderMasterMode[64] = 0 (absolute), 1 (relative)

**Effort:** 2 hours
**Bug Risk:** MEDIUM
  - Circular reference detection important
  - Could cause cascading updates if master→slave→master chain exists
  - Need to prevent infinite loops
  - Relative/absolute math needs careful testing
**Priority:** MEDIUM-HIGH - Very powerful feature, but can wait for v1.1+

---

## 7. [Add more features below as we discuss them]

