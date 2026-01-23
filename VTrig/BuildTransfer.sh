#!/bin/bash
# Build and transfer VTrig to Disting NT SD card

set -e

echo "Building VTrig..."
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

echo "Copying to $DEST ..."
cp build/1VTrig.o "$DEST/"

echo ""
echo "✓ VTrig transferred successfully"
echo "Eject SD card and load plugin on Disting NT"
