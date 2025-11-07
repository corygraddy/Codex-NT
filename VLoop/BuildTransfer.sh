#!/bin/bash
# Deploy VLoop plugin to the disting NT SD card

PLUGIN_BINARY="1VLoop.o"
SOURCE_DIR="$(pwd)/build"
VOLUME_NAME="DISTDEV NT"
VOLUME_PATH="/Volumes/${VOLUME_NAME}"
DEST_DIR="${VOLUME_PATH}/programs/plug-ins"
PRESET_FILE="${VOLUME_PATH}/presets/vltest.json"
CLEAN_PRESET="vltest_clean.json"

set -e

echo "Starting deployment of ${PLUGIN_BINARY} ..."

if [ ! -d "$VOLUME_PATH" ]; then
  echo "Error: SD card volume not found at $VOLUME_PATH" >&2
  exit 1
fi
if [ ! -d "$DEST_DIR" ]; then
  echo "Error: Destination directory not found at $DEST_DIR" >&2
  exit 1
fi
if [ ! -f "$SOURCE_DIR/$PLUGIN_BINARY" ]; then
  echo "Error: Compiled plugin not found at $SOURCE_DIR/$PLUGIN_BINARY" >&2
  exit 1
fi

# Copy plugin
cp -v "$SOURCE_DIR/$PLUGIN_BINARY" "$DEST_DIR/"

# Copy clean preset template to SD card (overwrites dirty one with debug data)
if [ -f "$CLEAN_PRESET" ]; then
  echo "Copying clean preset to SD card..."
  cp -v "$CLEAN_PRESET" "$PRESET_FILE"
fi

diskutil eject "$VOLUME_PATH"
echo "Deployment complete."
