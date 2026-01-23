# GitHub Release Instructions for V3Seq Build 1

## Step 1: Create Git Tag

```bash
cd /Users/corygraddy/Documents/Codex-NT
git tag -a v3seq-build1 -m "V3Seq Build 1 - Initial Release"
git push origin v3seq-build1
```

## Step 2: Go to GitHub Releases
1. Visit: https://github.com/corygraddy/Codex-NT/releases
2. Click "Draft a new release"

## Step 3: Configure the Release

### Tag
- Select tag: **v3seq-build1**

### Release Title
```
V3Seq Build 1 - 3-Output CV Sequencer for Disting NT
```

### Release Description
Copy and paste this:

```markdown
# V3Seq Build 1 - Initial Release

3-Output CV Sequencer for Expert Sleepers Disting NT

## 🎛️ Key Features

- **3 Independent CV Outputs** with 32 steps each
- **4 Voltage Ranges**: 0-5V, 0-10V, -5V to +5V, -10V to +10V
- **Configurable Sequence Range**: First Step / Last Step parameters (1-32)
- **4 Playback Modes**: Forward, Backward, Pingpong, Random
- **Section Looping**: Two sections with configurable repeat counts
- **Coarse/Fine Editing**: 25 steps for broad adjustments, 500 steps for precision
- **Per-Output Clock Division**: 1, 2, 4, 8, 16, 32
- **3 MIDI CC Outputs**: Parallel MIDI control for each CV output
- **Page-Based UI**: One page per CV output with bar graph visualization

## 🆕 What's New in Build 1

This is the first public release of V3Seq!

✅ Full CV sequencer functionality with 3 outputs  
✅ Configurable voltage ranges for maximum flexibility  
✅ First/Last Step parameters for custom sequence lengths  
✅ Section looping with repeat counts  
✅ Coarse/Fine editing modes with visual indicator  
✅ Page-based UI with reference dots and position indicators  
✅ MIDI CC output for external control  

## 📥 Installation

1. Download `V3Seq-Build1.zip` from the Assets section below
2. Extract the ZIP file
3. Copy `V3Seq.o` to your Disting NT's SD card: `/programs/plug-ins/`
4. Restart your Disting NT or reload plugins
5. V3Seq will appear in the plugin menu

## 🚀 Quick Start

1. **Select Page**: Turn left encoder to choose CV output (1, 2, or 3)
2. **Select Step**: Turn right encoder to choose step to edit
3. **Edit Value**: Turn left pot to adjust step value
4. **Fine Tune**: Press right encoder button to enter fine mode ("F" appears)
5. **Configure Range**: Set Voltage Range parameter for your needs
6. **Set Sequence Length**: Use First Step and Last Step parameters
7. **Section Looping**: Set Split Point, Sec1 Reps, and Sec2 Reps as desired
8. **Assign Outputs**: Set Out parameter (1-8) for each CV output

## 📖 Documentation

See the included `README.md` for complete documentation including:
- Detailed parameter descriptions
- Section looping examples
- Voltage range explanations
- Coarse/Fine mode usage
- Tips and tricks

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
   - `V3Seq-Build1.zip` (from `/Users/corygraddy/Documents/Codex-NT/V3Seq/release/`)

## Step 5: Publish

1. Click "Publish release"
2. The release will be live at: https://github.com/corygraddy/Codex-NT/releases
