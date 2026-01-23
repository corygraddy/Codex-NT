# GitHub Release Instructions for VTrig Build 1

## Step 1: Create Git Tag

```bash
cd /Users/corygraddy/Documents/Codex-NT
git tag -a vtrig-build1 -m "VTrig Build 1 - Initial Release"
git push origin vtrig-build1
```

## Step 2: Go to GitHub Releases
1. Visit: https://github.com/corygraddy/Codex-NT/releases
2. Click "Draft a new release"

## Step 3: Configure the Release

### Tag
- Select tag: **vtrig-build1**

### Release Title
```
VTrig Build 1 - 6-Track Trigger Sequencer for Disting NT
```

### Release Description
Copy and paste this:

```markdown
# VTrig Build 1 - Initial Release

6-Track Trigger Sequencer for Expert Sleepers Disting NT

## 🎛️ Key Features

- **6 Independent Trigger Tracks** with 32 steps each
- **Configurable Sequence Range**: First Step / Last Step parameters (1-32)
- **4 Playback Modes**: Forward, Backward, Pingpong, Random
- **Global Swing**: 50-75% for shuffle feel
- **Fill Patterns**: Automatic fills on last repeat of Section 1
- **Section Looping**: Two sections with configurable repeat counts
- **Adjustable Gate Length**: 1-100% per track
- **Per-Track Clock Division**: 1, 2, 4, 8, 16, 32
- **6 MIDI CC Outputs**: One per track for external control
- **Individual Run Control**: Enable/disable tracks independently

## 🆕 What's New in Build 1

This is the first public release of VTrig!

✅ 6 independent trigger tracks with flexible programming  
✅ Global swing parameter for shuffle feel  
✅ Fill pattern feature for dynamic variation  
✅ Section looping with repeat counts  
✅ Configurable gate length per track  
✅ First/Last Step parameters for custom sequence lengths  
✅ Page-based UI with step grid visualization  
✅ MIDI CC output for each track  

## 📥 Installation

1. Download `VTrig-Build1.zip` from the Assets section below
2. Extract the ZIP file
3. Copy `VTrig.o` to your Disting NT's SD card: `/programs/plug-ins/`
4. Restart your Disting NT or reload plugins
5. VTrig will appear in the plugin menu

## 🚀 Quick Start

1. **Select Track**: Turn left encoder to choose track (1-6)
2. **Select Step**: Turn right encoder to choose step
3. **Toggle Step**: Press right encoder button to enable/disable step
4. **Program Pattern**: Repeat steps 2-3 to create your rhythm
5. **Set Outputs**: Assign Out parameter (1-8) for each track
6. **Enable Track**: Set Run parameter to On
7. **Adjust Gate Length**: Use Length parameter to control gate duration
8. **Add Swing**: Set Swing parameter (try 62-66% for classic feel)

## 📖 Documentation

See the included `README.md` for complete documentation including:
- Detailed parameter descriptions
- Section looping with fill examples
- Swing explanation
- Polyrhythm tips
- Usage tips and tricks

## 💡 Tips

- **Polyrhythms**: Use different Clock Div settings per track
- **Fill Patterns**: Set Fill Start to create automatic variation
- **Gate Length**: Short gates (10-20%) for hi-hats, longer (50-80%) for kicks
- **Swing**: 66% = triplet swing (2:1 ratio)

## ⚙️ Requirements

- Disting NT with latest firmware
- SD card with `/programs/plug-ins/` folder

## 🙏 Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## 📄 License

See LICENSE file for details.
```

## Step 4: Upload Release Asset

1. In the "Attach binaries" section, drag and drop or click to upload:
   - `VTrig-Build1.zip` (from `/Users/corygraddy/Documents/Codex-NT/VTrig/release/`)

## Step 5: Publish

1. Click "Publish release"
2. The release will be live at: https://github.com/corygraddy/Codex-NT/releases
