#!/bin/bash
# Quick build script for the Synth project
# Uses Clang by default, but supports compiler override

set -e  # Exit on any error

echo "Synth Project Build Script"
echo "========================"

# Check if build directory exists and offer to clean
if [ -d "build" ]; then
    echo "Existing build directory found."
    read -p "Clean build? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Cleaning build directory..."
        rm -rf build
    fi
fi

# Create build directory
mkdir -p build

# Detect compiler choice
if [ -n "$CXX" ] || [ -n "$CC" ]; then
    echo "Using explicitly set compiler: $CXX"
    COMPILER_INFO="(explicitly set)"
else
    echo "Using default compiler (Clang)"
    COMPILER_INFO="(default)"
fi

echo "Configuring project..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo ""
echo "Building project..."
cmake --build build --config Release

echo ""
echo "✅ Build completed successfully!"
echo ""
echo "Available executables in build/bin/:"
ls -la build/bin/ 2>/dev/null | grep -E "^-.*x" | awk '{print "  " $9}' || echo "  (Build directory structure may vary)"

echo ""
echo "Quick test commands:"
echo "  ./build/bin/test_wave_print     # Test wave generation"
echo "  ./build/bin/simple_wav_example  # Generate WAV files"
echo "  ./build/bin/synth_cli           # Main MIDI synthesizer"

echo ""
echo "To use a different compiler next time:"
echo "  CC=gcc CXX=g++ ./build.sh      # Use GCC"
echo "  CC=clang CXX=clang++ ./build.sh # Use Clang explicitly"
