#!/bin/bash

# --- Configuration ---
# This script deploys a custom Disting NT plugin from your build directory to the SD card.

# Set the name of your plugin's binary file.
# NOTE: The compiled output is likely a .bin file, not a .o file.
# Check your build folder for the actual final filename.
PLUGIN_BINARY="CompositionMatrix.o"

# Set the full path to your local build directory.
SOURCE_DIR="/Users/corygraddy/Documents/Codex-NT/CompositionMatrix/build"

# Set the path to the plug-ins folder on your Disting NT's SD card.
# "Untitled" is the default name for many SD cards. Change it if yours is different.
DEST_DIR="/Volumes/Untitled/programs/plug-ins"


# --- Script Logic ---

echo "Starting deployment to Disting NT..."

# 1. Check if the Disting NT's SD card is mounted
if [ ! -d "$DEST_DIR" ]; then
    echo "Error: Disting NT SD card not found at $DEST_DIR"
    echo "Please ensure the card is mounted and the volume name is correct."
    exit 1
fi

# 2. Check if the compiled plugin file exists
if [ ! -f "$SOURCE_DIR/$PLUGIN_BINARY" ]; then
    echo "Error: Compiled plugin not found at $SOURCE_DIR/$PLUGIN_BINARY"
    echo "Please build your project in VS Code first."
    exit 1
fi

# 3. Copy the file, overwriting the existing one
echo "Copying $PLUGIN_BINARY to the SD card..."
cp "$SOURCE_DIR/$PLUGIN_BINARY" "$DEST_DIR/"

# Check if the copy was successful
if [ $? -eq 0 ]; then
    echo "Success! Plugin has been updated on the SD card."
else
    echo "Error: File copy failed. Check permissions."
    exit 1
fi

# 4. Safely eject the SD card
echo "Ejecting SD card..."
diskutil eject "$DEST_DIR"

echo "Deployment complete. You can now remove the SD card."
