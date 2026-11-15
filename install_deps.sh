#!/bin/bash
# install_deps.sh - Install all dependencies for building the Synth project on Linux
# Usage: ./install_deps.sh

set -e

# Update package list
echo "Updating package list..."
sudo apt-get update

echo "Installing build dependencies for Synth (ALSA, PulseAudio, CMake, build tools; SDL3 for the panels)..."
sudo apt-get install -y \
    libasound2-dev \
    libpulse-dev \
    libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
    pkg-config \
    cmake \
    build-essential

echo "\nAll dependencies installed!"
echo "You can now build the project with ./build.sh or the CMake instructions."
