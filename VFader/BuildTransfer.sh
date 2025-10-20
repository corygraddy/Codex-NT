#!/bin/bash
# Deploy VFader plugin to the disting NT SD card

PLUGIN_BINARY="VFader.o"
SOURCE_DIR="$(pwd)/build"
VOLUME_NAME="Untitled"
VOLUME_PATH="/Volumes/${VOLUME_NAME}"
DEST_DIR="${VOLUME_PATH}/programs/plug-ins"

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

cp -v "$SOURCE_DIR/$PLUGIN_BINARY" "$DEST_DIR/"

diskutil eject "$VOLUME_PATH"
echo "Deployment complete."
