#!/bin/bash
# Fetch vltest.json preset from the disting NT SD card

VOLUME_NAME="DISTDEV NT"
VOLUME_PATH="/Volumes/${VOLUME_NAME}"
PRESET_FILE="${VOLUME_PATH}/presets/vltest.json"
LOCAL_PRESET="vltest.json"

set -e

echo "Fetching preset from SD card..."

if [ ! -d "$VOLUME_PATH" ]; then
  echo "Error: SD card volume not found at $VOLUME_PATH" >&2
  exit 1
fi

if [ ! -f "$PRESET_FILE" ]; then
  echo "Error: Preset file not found at $PRESET_FILE" >&2
  exit 1
fi

cp -v "$PRESET_FILE" "$LOCAL_PRESET"
echo "Preset fetched successfully."
