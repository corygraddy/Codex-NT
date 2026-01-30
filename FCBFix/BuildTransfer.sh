#!/bin/bash
# Build and transfer FCBFix to Disting NT SD card

set -e

echo "Building FCBFix..."
make clean
make

echo ""
echo "Looking for SD card..."

# Try common mount points
if [ -d "/Volumes/Untitled/programs/plug-ins" ]; then
    DEST="/Volumes/Untitled/programs/plug-ins"
elif [ -d "/Volumes/DISTDEV NT/programs/plug-ins" ]; then
    DEST="/Volumes/DISTDEV NT/programs/plug-ins"
else
    echo "ERROR: Could not find Disting NT SD card"
    echo "Please mount SD card and ensure /programs/plug-ins/ exists"
    exit 1
fi

echo "Found SD card at: $DEST"
echo ""
echo "Copying FCBFix.o to SD card..."
cp build/FCBFix.o "$DEST/"

echo ""
echo "✓ FCBFix successfully built and copied to SD card!"
echo ""
echo "Next steps:"
echo "1. Safely eject the SD card"
echo "2. Insert it into your Disting NT"
echo "3. Power cycle the Disting NT"
echo "4. FCBFix should appear in the plugin list"
