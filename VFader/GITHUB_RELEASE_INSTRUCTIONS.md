# GitHub Release Instructions for VFader Build 48

Your release is ready! Follow these steps to create the GitHub release:

## Step 1: Go to GitHub Releases
1. Visit: https://github.com/corygraddy/Codex-NT/releases
2. Click "Draft a new release"

## Step 2: Configure the Release

### Tag
- Select existing tag: **v1.0-build48**

### Release Title
```
VFader Build 48 - 32 Virtual Faders for Disting NT
```

### Release Description
Copy and paste this:

```markdown
# VFader Build 48

32 Virtual MIDI Faders for Expert Sleepers Disting NT

## 🎛️ Key Features

- **32 Virtual Faders** organized into 4 pages of 8 faders each
- **Note Mode**: Send specific MIDI notes with customizable scales
- **Number Mode**: Send scaled values (0-100 range, customizable)
- **7-bit and 14-bit MIDI CC output** (CC 1-32 or CC 0-31 with LSB)
- **Macro Faders**: Control multiple child faders simultaneously (Absolute or Relative modes)
- **Pickup Modes**: Scaled or Catch for smooth page transitions
- **Custom Naming**: 6-character name + 5-character category per fader
- **Pot Control**: Enable/disable pot input to prevent accidental changes
- **Drift Control**: Minimize unintended value changes from electrical noise

## 🆕 What's New in Build 48

- **Fixed drift control settings** that prevented catch mode from working properly
- Removed "High" drift setting (2% threshold) 
- Renamed "Med" to "High" (1% threshold)
- Available drift control options: **Off**, **Low (0.5%)**, **High (1%)**
- Resolves issue where catch mode couldn't be reached when fader was at zero with high drift setting

## 📥 Installation

1. Download `VFader-Build48.zip` from the Assets section below
2. Extract the ZIP file
3. Copy `VFader.o` to your Disting NT's SD card in the `Algorithms` folder
4. Restart your Disting NT or reload algorithms
5. Select VFader from the algorithm menu

## 🚀 Quick Start

1. **Navigate Pages**: Left encoder (Pages 1-4)
2. **Select Fader**: Right encoder (Faders 1-8)
3. **Control Faders**: 
   - Left Pot → Fader to the left of selection
   - Center Pot → Selected fader
   - Right Pot → Fader to the right of selection
4. **Edit Names**: Press right encoder button
5. **Configure Settings**: From name edit mode, turn right pot to access function settings
6. **Save Preset**: Save your fader names and configurations

## 📖 Documentation

See the included `README.md` for complete documentation including:
- Detailed parameter descriptions
- Macro fader setup and usage
- Note mode and scale masking
- MIDI routing and 14-bit mode
- Tips and tricks
- Troubleshooting guide

## ⚙️ Requirements

- Disting NT with latest firmware
- SD card with Algorithms folder

## 📜 Version History

Previous builds:
- **Build 47**: Fixed macro fader initialization bug
- **Build 46**: Removed CV output functionality (abandoned feature)
- **Build 45**: Increased pot deadband, fixed Relative mode macro behavior
- **Build 44**: Added Pot Control parameter
- **Build 43**: Added macro fader functionality

## 🙏 Credits

Developed by Cory Graddy for Disting NT  
Built using the Expert Sleepers Disting NT API  
Made with the help of GitHub Copilot: Claude Sonnet 4.5

## 📄 License

See LICENSE file for details.
```

## Step 3: Attach Files

Upload these files from `VFader/release/` as release assets:
- ✅ **VFader-Build48.zip** (contains VFader.o, README.md, and RELEASE_NOTES.md)

Or individually:
- ✅ **VFader.o** (the binary algorithm file)
- ✅ **README.md** (complete documentation)
- ✅ **RELEASE_NOTES.md** (release notes)

## Step 4: Publish

1. Check "Set as the latest release" ✅
2. Click "Publish release"

## Alternative: Use GitHub CLI

If you have GitHub CLI installed, you can run:

```bash
cd /Users/corygraddy/Documents/Codex-NT
gh release create v1.0-build48 \
  VFader/release/VFader-Build48.zip \
  VFader/release/VFader.o \
  VFader/release/README.md \
  VFader/release/RELEASE_NOTES.md \
  --title "VFader Build 48 - 32 Virtual Faders for Disting NT" \
  --notes-file VFader/release/RELEASE_NOTES.md
```

---

Your tag **v1.0-build48** is already pushed to GitHub and ready to use!
