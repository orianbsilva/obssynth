# SYNTH_VIEW - OpenGL Waveform Viewer

Real-time waveform visualization with interactive control and MIDI integration.

## Features

### Real-time Visualization
- **Live OpenGL Display**: Hardware-accelerated waveform rendering
- **Multiple Waveforms**: Sine, sawtooth, square wave visualization
- **Interactive Control**: Mouse and keyboard parameter adjustment
- **Smooth Animation**: 60 FPS real-time updates

### MIDI Integration
- **Real-time MIDI Processing**: Live note visualization
- **Visual Feedback**: Frequency and amplitude changes in real-time
- **MIDI Device Support**: Professional MIDI controller integration

### Cross-platform Graphics
- **Linux**: none (synth_view is Windows only); the Linux panel is obssynth4d (SDL3, Wayland)
- **Windows**: Win32/OpenGL implementation
- **Fallback Support**: Virtual keyboard when MIDI unavailable

## Controls

### Keyboard Controls
- **1, 2, 3**: Switch waveform type (Sine, Square, Sawtooth)
- **↑/↓**: Adjust frequency
- **+/-**: Adjust amplitude
- **Space**: Pause/resume visualization
- **R**: Reset to default parameters
- **F**: Toggle fullscreen mode
- **ESC**: Exit application

### Mouse Controls
- **Left Click + Drag**: Adjust frequency (horizontal) and amplitude (vertical)
- **Right Click**: Context menu for advanced options
- **Scroll Wheel**: Fine frequency adjustment

### MIDI Controls
- **Note On/Off**: Real-time frequency control
- **Velocity**: Amplitude control
- **Pitch Bend**: Fine frequency modulation
- **Mod Wheel**: Waveform morphing (CC 1)

## Architecture

### Core Components
- **`main_screen.cpp`**: Main application entry point and coordination
- **`gl_renderer.cpp`**: OpenGL rendering engine for waveform display
- **Platform Window Implementations**:
  - **`win32_gl_window.cpp`**: Win32/OpenGL window management for Windows
- **Platform Headers**:
  - **`platform_gl.hpp`**: OpenGL context abstraction
  - **`platform_window.hpp`**: Window management abstraction

### Design Patterns
- **Platform Abstraction**: Unified interface for different operating systems
- **Renderer Separation**: Graphics logic separated from platform code
- **Event-driven Architecture**: Responsive to user input and MIDI events

## Build Requirements

### Linux
- **OpenGL**: WGL (Windows)
- **ALSA**: Audio device enumeration (`libasound2-dev`)

### Windows
- **OpenGL**: OpenGL32.lib (included with Windows)
- **Win32 API**: Window management and OpenGL context
- **WinMM**: MIDI device support

## Performance

### Optimization Features
- **Hardware Acceleration**: GPU-based waveform rendering
- **Efficient Updates**: Only redraw when parameters change
- **Low CPU Usage**: Optimized for real-time performance
- **Memory Management**: Efficient buffer management for continuous operation

### System Requirements
- **Graphics**: OpenGL 2.1 or higher
- **Memory**: 64MB RAM minimum
- **CPU**: Any modern processor (audio processing offloaded to synth_core)

## Technical Implementation

### Rendering Pipeline
1. **Waveform Generation**: Mathematical wave calculation in CPU
2. **Buffer Management**: Efficient vertex buffer updates
3. **GPU Rendering**: Hardware-accelerated line drawing
4. **Display Refresh**: V-sync for smooth animation

### Platform Integration
- **Window Management**: Native window creation and event handling
- **OpenGL Context**: Platform-specific OpenGL context creation
- **Input Handling**: Keyboard, mouse, and MIDI input processing
- **Timer Management**: High-resolution timing for smooth animation
