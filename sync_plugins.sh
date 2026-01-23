#!/bin/bash
# Sync plugin subdirectories to their dedicated repositories
# Run this after committing changes to Codex-NT

set -e

echo "🔄 Syncing plugins to dedicated repositories..."
echo ""

# VTrig
echo "📦 Pushing VTrig to github.com/corygraddy/VTrig..."
git subtree push --prefix=VTrig vtrig-repo main

echo ""

# V3Seq
echo "📦 Pushing V3Seq to github.com/corygraddy/V3Seq..."
git subtree push --prefix=V3Seq v3seq-repo main

echo ""
echo "✅ All plugins synced successfully!"
echo ""
echo "VTrig: https://github.com/corygraddy/VTrig"
echo "V3Seq: https://github.com/corygraddy/V3Seq"
