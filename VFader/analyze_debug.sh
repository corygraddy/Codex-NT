#!/bin/bash

# Extract debug info from VFader preset JSON
# Usage: ./analyze_debug.sh

PRESET_FILE="/Volumes/Untitled/presets/VFADERTEST.json"

if [ ! -f "$PRESET_FILE" ]; then
    echo "ERROR: Preset file not found at $PRESET_FILE"
    echo "Please save a preset named 'VFADERTEST' on the disting NT"
    exit 1
fi

echo "=== VFader Debug Analysis ==="
echo ""
echo "Extracting debug data from: $PRESET_FILE"
echo ""

# Use Python to parse and display the debug section
python3 << 'EOF'
import json
import sys

try:
    with open('/Volumes/Untitled/presets/VFADERTEST.json', 'r') as f:
        preset = json.load(f)
    
    # Find the VFader algorithm in the preset
    vfader_data = None
    if 'slots' in preset:
        for slot in preset['slots']:
            if slot.get('guid') == 'VFDR':
                vfader_data = slot
                break
    
    if not vfader_data:
        print("ERROR: VFader algorithm not found in preset")
        sys.exit(1)
    
    # Extract debug section
    if 'debug' not in vfader_data:
        print("ERROR: No debug data found in preset")
        print("This might be an old preset. Please re-save after running tests.")
        sys.exit(1)
    
    debug = vfader_data['debug']
    
    print("DEBUG SNAPSHOT:")
    print(f"  Step Count:              {debug.get('stepCount', 'N/A')}")
    print(f"  Fader 0 Value:           {debug.get('fader0Value', 'N/A')}")
    print(f"  Last MIDI Value (F0):    {debug.get('lastMidiValue0', 'N/A')}")
    print(f"  Has Control (F0):        {debug.get('hasControl0', 'N/A')}")
    print(f"  Param Changed Count:     {debug.get('paramChangedCount', 'N/A')}")
    print(f"  MIDI Sent Count:         {debug.get('midiSentCount', 'N/A')}")
    print(f"  Last Param Changed Val:  {debug.get('lastParamChangedValue', 'N/A')}")
    print(f"  Last Param Changed Step: {debug.get('lastParamChangedStep', 'N/A')}")
    print()
    print("PICKUP MODE DEBUG:")
    print(f"  Pickup Enter Count:      {debug.get('pickupEnterCount', 'N/A')}")
    print(f"  Pickup Exit Count:       {debug.get('pickupExitCount', 'N/A')}")
    print(f"  Last Physical Pos:       {debug.get('lastPhysicalPos', 'N/A')}")
    print(f"  Last Pickup Pivot:       {debug.get('lastPickupPivot', 'N/A')}")
    print(f"  Last Pickup Start Val:   {debug.get('lastPickupStartValue', 'N/A')}")
    print(f"  Last Mismatch:           {debug.get('lastMismatch', 'N/A')}")
    print(f"  Last Caught Up (Up):     {debug.get('lastCaughtUpUp', 'N/A')}")
    print(f"  Last Caught Up (Down):   {debug.get('lastCaughtUpDown', 'N/A')}")
    print()
    
    # Show all fader values
    if 'faders' in vfader_data:
        faders = vfader_data['faders']
        print("FADER VALUES (0-31):")
        for i in range(0, 32, 8):
            vals = [f"{faders[j]:.3f}" for j in range(i, min(i+8, len(faders)))]
            print(f"  F{i:2d}-{i+7:2d}: {' '.join(vals)}")
        print()
    
    # Show soft takeover state
    if 'hasControl' in vfader_data:
        control = vfader_data['hasControl']
        print("SOFT TAKEOVER STATE (has control):")
        for i in range(0, 32, 8):
            states = [str(control[j])[0] for j in range(i, min(i+8, len(control)))]
            print(f"  F{i:2d}-{i+7:2d}: {' '.join(states)}")
        print()
    
    # Analyze the results
    print("ANALYSIS:")
    
    if debug.get('paramChangedCount', 0) == 0:
        print("  ⚠️  parameterChanged() was NEVER called for fader 0")
        print("      → F8R I2C mapping may not be working")
        print("      → Or you didn't move F8R fader 1")
    else:
        print(f"  ✓ parameterChanged() called {debug['paramChangedCount']} times")
        print(f"    Last value: {debug.get('lastParamChangedValue', 'N/A')}")
        print(f"    Last step: {debug.get('lastParamChangedStep', 'N/A')}")
    
    if debug.get('midiSentCount', 0) == 0:
        print("  ⚠️  MIDI was NEVER sent for fader 0")
        if debug.get('paramChangedCount', 0) > 0:
            print("      → parameterChanged() works, but step() isn't sending")
            print("      → Soft takeover logic may be blocking")
            print(f"      → Has control: {debug.get('hasControl0', 'N/A')}")
            print(f"      → Fader value: {debug.get('fader0Value', 'N/A')}")
            print(f"      → Target value: {debug.get('lastMidiValue0', 'N/A')}")
        else:
            print("      → Neither parameterChanged nor MIDI sending worked")
    else:
        print(f"  ✓ MIDI sent {debug['midiSentCount']} times")
    
    print()
    
except FileNotFoundError:
    print("ERROR: Could not read preset file")
    sys.exit(1)
except json.JSONDecodeError as e:
    print(f"ERROR: Invalid JSON in preset file: {e}")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
EOF
