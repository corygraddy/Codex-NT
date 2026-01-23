#!/bin/bash
# Build and transfer V3Seq to Disting NT SD card

set -e

echo "Building V3Seq..."
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
cp build/1V3Seq.o "$DEST/"

echo ""
echo "SUCCESS: V3Seq deployed to SD card"
echo "Safely eject the SD card and insert into Disting NT"
