#!/bin/bash
# SYNTH_CLI Quick Launch Script
# Professional MIDI Synthesizer

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SYNTH_CLI="$BUILD_DIR/synth_cli"

echo "SYNTH_CLI - Professional MIDI Synthesizer"
echo "=========================================="

# Check if already built
if [ ! -f "$SYNTH_CLI" ]; then
    echo "Building synth_cli..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    echo "Build complete!"
    echo
fi

# Launch synth_cli
echo "Launching synth_cli..."
echo "Press Ctrl+C to exit the synthesizer"
echo
exec "$SYNTH_CLI"
