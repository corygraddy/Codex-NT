#!/bin/bash

# --- Configuration ---

PLUGIN_BINARY="EdgeLike.o"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)/build"
VOLUME_NAME="Untitled"
VOLUME_PATH="/Volumes/${VOLUME_NAME}"
DEST_DIR="${VOLUME_PATH}/programs/plug-ins"

echo "Starting deployment of ${PLUGIN_BINARY}..."

if [ ! -d "$VOLUME_PATH" ]; then
  echo "Error: SD card volume not found at $VOLUME_PATH"; exit 1; fi
if [ ! -d "$DEST_DIR" ]; then
  echo "Error: Destination directory not found at $DEST_DIR"; exit 1; fi
if [ ! -f "$SOURCE_DIR/$PLUGIN_BINARY" ]; then
  echo "Error: Compiled plugin not found at $SOURCE_DIR/$PLUGIN_BINARY"; exit 1; fi

echo "Copying to $DEST_DIR ..."
cp "$SOURCE_DIR/$PLUGIN_BINARY" "$DEST_DIR/" || { echo "Copy failed"; exit 1; }

echo "Ejecting $VOLUME_PATH ..."
diskutil eject "$VOLUME_PATH"
echo "Done."
