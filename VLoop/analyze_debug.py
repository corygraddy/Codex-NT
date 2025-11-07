#!/usr/bin/env python3
"""Analyze VLoop debug data from crash preset."""

import json
import sys

def analyze_crash(filename='vltest.json'):
    with open(filename) as f:
        data = json.load(f)
    
    # Find VLoop slot
    vloop = None
    for slot in data['slots']:
        if slot.get('guid') == 'VLOP':
            vloop = slot
            break
    
    if not vloop:
        print("ERROR: VLoop slot not found")
        return
    
    print("=" * 70)
    print("VLoop CRASH ANALYSIS")
    print("=" * 70)
    
    # Basic state
    print("\n=== PLAYBACK STATE ===")
    print(f"Loop Length:    {vloop['loopLength']} pulses")
    print(f"Current Pulse:  {vloop['currentPulse']} <-- CRASHED HERE")
    print(f"Is Recording:   {vloop['isRecording']}")
    print(f"Is Playing:     {vloop['isPlaying']}")
    
    # Debug counters
    debug = vloop.get('debug', {})
    print("\n=== DEBUG COUNTERS ===")
    print(f"Total step() calls:     {debug.get('stepCallCount', 0):,}")
    print(f"Total clock edges:      {debug.get('totalClockEdges', 0):,}")
    print(f"Total MIDI received:    {debug.get('totalMidiReceived', 0):,}")
    print(f"Total MIDI sent:        {debug.get('totalMidiSent', 0):,}")
    
    last_midi = debug.get('lastMidiSent', {})
    print(f"\nLast MIDI sent at pulse {debug.get('lastPulseWithMidi', 0)}:")
    status = last_midi.get('status', 0)
    data1 = last_midi.get('data1', 0)
    data2 = last_midi.get('data2', 0)
    
    msg_type = status & 0xF0
    channel = (status & 0x0F) + 1
    if msg_type == 0x90:
        print(f"  Note On ch{channel}: note={data1} vel={data2}")
    elif msg_type == 0x80:
        print(f"  Note Off ch{channel}: note={data1} vel={data2}")
    elif msg_type == 0xB0:
        print(f"  CC ch{channel}: cc={data1} val={data2}")
    else:
        print(f"  Status={hex(status)} data1={data1} data2={data2}")
    
    # Bucket analysis
    buckets = vloop.get('buckets', [])
    print(f"\n=== BUCKET ANALYSIS ===")
    print(f"Non-empty buckets: {len(buckets)}/{vloop['loopLength']}")
    
    pulse_numbers = [b['pulse'] for b in buckets]
    
    # Count events per bucket
    total_events = sum(len(b['events']) for b in buckets)
    max_events = max(len(b['events']) for b in buckets) if buckets else 0
    print(f"Total recorded events: {total_events}")
    print(f"Max events per bucket: {max_events}")
    
    # Check crash point
    crash_pulse = vloop['currentPulse']
    print(f"\n=== CRASH POINT (Pulse {crash_pulse}) ===")
    
    crash_bucket = None
    for b in buckets:
        if b['pulse'] == crash_pulse:
            crash_bucket = b
            break
    
    if crash_bucket:
        events = crash_bucket['events']
        print(f"Bucket at crash has {len(events)} events:")
        for i, ev in enumerate(events):
            status = ev['status']
            d1 = ev['data1']
            d2 = ev['data2']
            msg_type = status & 0xF0
            ch = (status & 0x0F) + 1
            
            if msg_type == 0x90:
                msg = f"Note On ch{ch}: note={d1} vel={d2}"
            elif msg_type == 0x80:
                msg = f"Note Off ch{ch}: note={d1} vel={d2}"
            elif msg_type == 0xB0:
                msg = f"CC ch{ch}: cc={d1} val={d2}"
            else:
                msg = f"Status={hex(status)} data1={d1} data2={d2}"
            
            marker = " <-- WOULD SEND THIS" if i == 0 else ""
            print(f"  Event {i}: {msg}{marker}")
    else:
        print(f"Bucket at crash pulse is EMPTY (no events)")
    
    # Show surrounding context
    print(f"\n=== SURROUNDING BUCKETS ===")
    for b in buckets:
        if crash_pulse - 3 <= b['pulse'] <= crash_pulse + 3:
            pulse = b['pulse']
            events = b['events']
            marker = " <-- CRASH" if pulse == crash_pulse else ""
            print(f"Pulse {pulse}: {len(events)} events{marker}")
    
    # Statistics
    print(f"\n=== PERFORMANCE STATS ===")
    if debug.get('totalClockEdges', 0) > 0:
        avg_frames = debug.get('stepCallCount', 0) / debug.get('totalClockEdges', 1)
        print(f"Avg step() calls per clock: {avg_frames:.1f}")
        
        if vloop['loopLength'] > 0:
            loops_played = debug.get('totalClockEdges', 0) / vloop['loopLength']
            print(f"Loops played before crash: {loops_played:.2f}")

if __name__ == '__main__':
    filename = sys.argv[1] if len(sys.argv) > 1 else 'vltest.json'
    analyze_crash(filename)
