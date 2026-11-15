#!/usr/bin/env python3
"""
Waveform Drawer - Interactive Wave Shape Creator
==============================================

A Python application for drawing custom waveforms with the mouse and saving them as WAV files.
Supports both 16-bit PCM and 32-bit float formats with configurable sample rates.

Features:
- Interactive drawing canvas
- Real-time waveform preview
- Multiple sample rates (8000, 11025, 22050, 44100, 48000, 96000 Hz)
- 16-bit PCM or 32-bit float output formats
- Configurable wave cycle resolution (256, 512, 1024, 2048 samples)
- Clear and reset functionality
- Preview playback (if audio device available)

Requirements:
- Python 3.6+
- tkinter (usually included with Python)
- numpy
- scipy (for WAV file writing)

Usage:
    python waveform_drawer.py

Controls:
- Left click and drag to draw waveform
- Right click to clear canvas
- Use controls panel to adjust settings
- Click "Save WAV" to export to PCM folder
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import numpy as np
import wave
import struct
import os
import sys
import threading
import time
import math
from typing import Tuple, List, Optional

try:
    from scipy.io import wavfile
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    print("Warning: scipy not available. Using basic WAV writer.")

try:
    import pygame
    import pygame.midi
    PYGAME_AVAILABLE = True
except ImportError:
    PYGAME_AVAILABLE = False
    print("Warning: pygame not available. MIDI and audio features disabled.")

try:
    import pyaudio
    PYAUDIO_AVAILABLE = True
except ImportError:
    PYAUDIO_AVAILABLE = False
    print("Warning: pyaudio not available. Using pygame for audio playback.")

class WaveformDrawer:
    """Main application class for drawing and saving waveforms."""
    
    def __init__(self, root: tk.Tk):
        """Initialize the waveform drawer application."""
        self.root = root
        self.root.title("Waveform Drawer - Synth Utility")
        self.root.geometry("1000x800")
        
        # Canvas settings
        self.canvas_width = 800
        self.canvas_height = 400
        self.canvas_margin = 50
        
        # Waveform data
        self.wave_points: List[Tuple[float, float]] = []
        self.wave_samples: np.ndarray = np.zeros(1024)
        
        # Audio settings
        self.sample_rates = [8000, 11025, 22050, 44100, 48000, 96000]
        self.sample_rate = tk.IntVar(value=48000)
        self.bit_depths = ["16-bit PCM", "32-bit Float"]
        self.bit_depth = tk.StringVar(value="32-bit Float")
        self.wave_resolutions = [256, 512, 1024, 2048]
        self.wave_resolution = tk.IntVar(value=1024)
        
        # Drawing state
        self.is_drawing = False
        self.last_x = None
        self.last_y = None
        
        # MIDI and audio state
        self.midi_initialized = False
        self.midi_input = None
        self.midi_devices = []
        self.selected_midi_device = tk.StringVar(value="None")
        self.midi_enabled = tk.BooleanVar(value=False)
        self.audio_enabled = tk.BooleanVar(value=True)
        self.current_notes = {}  # Track currently playing notes
        self.audio_threads = {}  # Track audio playback threads
        
        # Setup UI first
        self.setup_ui()
        self.clear_canvas()
        
        # Initialize audio/MIDI systems after UI is ready
        self.initialize_audio_midi()
        
        # Bind events
        self.canvas.bind("<Button-1>", self.start_drawing)
        self.canvas.bind("<B1-Motion>", self.continue_drawing)
        self.canvas.bind("<ButtonRelease-1>", self.stop_drawing)
        self.canvas.bind("<Button-3>", self.clear_canvas)  # Right click to clear
        
        # Start MIDI monitoring
        if self.midi_initialized:
            self.start_midi_monitoring()
        
        # Handle application close
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
    
    def initialize_audio_midi(self) -> None:
        """Initialize pygame MIDI and audio systems."""
        if not PYGAME_AVAILABLE:
            self.update_status("MIDI/Audio disabled - pygame not available")
            return
            
        try:
            # Initialize pygame
            pygame.init()
            
            # Quit any existing mixer to reset configuration
            pygame.mixer.quit()
            
            # Try to initialize mixer for mono audio, but accept whatever works
            sample_rate = self.sample_rate.get()
            pygame.mixer.pre_init(
                frequency=sample_rate, 
                size=-16,        # 16-bit signed
                channels=1,      # Try mono first
                buffer=1024      # Larger buffer for stability
            )
            pygame.mixer.init()
            
            # Check what we actually got and adapt if needed
            mixer_freq, mixer_format, mixer_channels = pygame.mixer.get_init()
            if mixer_channels != 1:
                # System forced stereo, that's ok - we'll adapt our audio data
                print(f"Note: System forced {mixer_channels} channel audio")
            
            print(f"Mixer initialized: {mixer_freq}Hz, {mixer_channels} channels, format {mixer_format}")
            
            # Initialize MIDI
            pygame.midi.init()
            
            # Get MIDI devices
            self.scan_midi_devices()
            
            self.midi_initialized = True
            self.update_status("MIDI and audio systems initialized")
            
        except Exception as e:
            print(f"Failed to initialize MIDI/audio: {e}")
            self.update_status(f"MIDI/Audio initialization failed: {e}")
    
    def scan_midi_devices(self) -> None:
        """Scan for available MIDI input devices."""
        self.midi_devices = [("None", -1)]
        
        if not PYGAME_AVAILABLE:
            return
            
        try:
            for i in range(pygame.midi.get_count()):
                info = pygame.midi.get_device_info(i)
                if info[2]:  # Is input device
                    name = info[1].decode('utf-8')
                    self.midi_devices.append((name, i))
                    
        except Exception as e:
            print(f"Error scanning MIDI devices: {e}")
    
    def select_midi_device(self, device_name: str) -> None:
        """Select and open a MIDI input device."""
        try:
            # Close current device if any
            if self.midi_input:
                self.midi_input.close()
                self.midi_input = None
            
            # Find device ID
            device_id = -1
            for name, dev_id in self.midi_devices:
                if name == device_name:
                    device_id = dev_id
                    break
            
            if device_id >= 0:
                self.midi_input = pygame.midi.Input(device_id)
                self.midi_enabled.set(True)
                self.update_status(f"MIDI device connected: {device_name}")
            else:
                self.midi_enabled.set(False)
                self.update_status("No MIDI device selected")
                
        except Exception as e:
            self.update_status(f"MIDI device error: {e}")
            self.midi_enabled.set(False)
    
    def start_midi_monitoring(self) -> None:
        """Start MIDI input monitoring in a separate thread."""
        if not self.midi_initialized:
            return
            
        def midi_thread():
            while True:
                try:
                    if self.midi_input and self.midi_enabled.get():
                        if self.midi_input.poll():
                            events = self.midi_input.read(10)
                            for event in events:
                                self.process_midi_event(event)
                    
                    time.sleep(0.001)  # Small delay to prevent high CPU usage
                    
                except Exception as e:
                    print(f"MIDI monitoring error: {e}")
                    time.sleep(0.1)
        
        midi_monitor = threading.Thread(target=midi_thread, daemon=True)
        midi_monitor.start()
    
    def process_midi_event(self, event) -> None:
        """Process incoming MIDI events."""
        try:
            status = event[0][0] & 0xF0
            note = event[0][1]
            velocity = event[0][2]
            
            if status == 0x90 and velocity > 0:  # Note on
                self.note_on(note, velocity)
            elif status == 0x80 or (status == 0x90 and velocity == 0):  # Note off
                self.note_off(note)
                
        except Exception as e:
            print(f"MIDI event processing error: {e}")
    
    def note_on(self, note: int, velocity: int) -> None:
        """Handle MIDI note on event."""
        if not self.audio_enabled.get() or len(self.wave_samples) == 0:
            return
        
        # Calculate frequency from MIDI note
        frequency = 440.0 * (2.0 ** ((note - 69) / 12.0))
        
        # Stop existing note if playing
        if note in self.current_notes:
            self.note_off(note)
        
        # Start playing the note
        self.current_notes[note] = {
            'frequency': frequency,
            'velocity': velocity,
            'playing': True
        }
        
        # Start audio playback in a separate thread
        audio_thread = threading.Thread(
            target=self.play_note_audio, 
            args=(note, frequency, velocity / 127.0),
            daemon=True
        )
        audio_thread.start()
        self.audio_threads[note] = audio_thread
        
        # Update UI
        self.root.after(0, lambda: self.update_status(f"Note ON: {note} ({frequency:.1f} Hz)"))
        self.root.after(0, self.update_notes_display)
    
    def note_off(self, note: int) -> None:
        """Handle MIDI note off event."""
        if note in self.current_notes:
            self.current_notes[note]['playing'] = False
            del self.current_notes[note]
            
            # Clean up audio thread
            if note in self.audio_threads:
                del self.audio_threads[note]
        
        # Update UI
        self.root.after(0, lambda: self.update_status(f"Note OFF: {note}"))
        self.root.after(0, self.update_notes_display)
    
    def play_note_audio(self, note: int, frequency: float, amplitude: float) -> None:
        """Play the waveform at the specified frequency."""
        if len(self.wave_samples) == 0:
            return
            
        try:
            # Generate audio data at the target frequency
            duration = 2.0  # Play for 2 seconds or until note off
            sample_rate = self.sample_rate.get()
            total_samples = int(duration * sample_rate)
            
            # Calculate how many times to repeat the wave cycle
            samples_per_cycle = len(self.wave_samples)
            cycles_per_second = frequency
            cycle_duration = 1.0 / cycles_per_second
            samples_per_audio_cycle = int(sample_rate * cycle_duration)
            
            # Generate the audio
            audio_data = np.zeros(total_samples, dtype=np.float32)
            
            for i in range(total_samples):
                if note not in self.current_notes or not self.current_notes[note]['playing']:
                    break
                    
                # Calculate position in the waveform cycle
                cycle_pos = (i % samples_per_audio_cycle) / samples_per_audio_cycle
                sample_index = int(cycle_pos * samples_per_cycle)
                
                # Apply amplitude and copy sample
                audio_data[i] = self.wave_samples[sample_index] * amplitude * 0.3  # Reduce volume
            
            # Convert to 16-bit for pygame
            audio_16bit = (audio_data * 32767).astype(np.int16)
            
            # Play using pygame mixer
            if PYGAME_AVAILABLE:
                try:
                    # Check mixer configuration
                    mixer_freq, mixer_format, mixer_channels = pygame.mixer.get_init()
                    
                    if mixer_channels == 1:
                        # Mono mixer - use 1D array
                        audio_final = np.ascontiguousarray(audio_16bit)
                    elif mixer_channels == 2:
                        # Stereo mixer - create 2D array with identical left/right channels
                        audio_stereo = np.zeros((len(audio_16bit), 2), dtype=np.int16)
                        audio_stereo[:, 0] = audio_16bit  # Left channel
                        audio_stereo[:, 1] = audio_16bit  # Right channel
                        audio_final = np.ascontiguousarray(audio_stereo)
                    else:
                        # Multi-channel - duplicate to all channels
                        audio_multi = np.zeros((len(audio_16bit), mixer_channels), dtype=np.int16)
                        for ch in range(mixer_channels):
                            audio_multi[:, ch] = audio_16bit
                        audio_final = np.ascontiguousarray(audio_multi)
                    
                    sound = pygame.sndarray.make_sound(audio_final)
                    sound.play()
                    
                    # Wait for note off or duration end
                    start_time = time.time()
                    while (time.time() - start_time < duration and 
                           note in self.current_notes and 
                           self.current_notes[note]['playing']):
                        time.sleep(0.01)
                    
                    sound.stop()
                    
                except Exception as pygame_error:
                    print(f"Audio playback error: {pygame_error}")
                    # Don't try fallback - just report the error
                
        except Exception as e:
            print(f"Audio playback error: {e}")
    
    def on_midi_device_changed(self, event=None) -> None:
        """Handle MIDI device selection change."""
        device_name = self.selected_midi_device.get()
        self.select_midi_device(device_name)
    
    def refresh_midi_devices(self) -> None:
        """Refresh the list of available MIDI devices."""
        if not PYGAME_AVAILABLE:
            return
            
        try:
            # Re-scan devices
            self.scan_midi_devices()
            
            # Update combo box
            device_names = [name for name, _ in self.midi_devices]
            self.midi_device_combo.config(values=device_names)
            
            # Reset selection if current device no longer exists
            current_device = self.selected_midi_device.get()
            if current_device not in device_names:
                self.selected_midi_device.set("None")
                self.select_midi_device("None")
            
            self.update_status(f"Found {len(self.midi_devices)-1} MIDI input devices")
            
        except Exception as e:
            self.update_status(f"Error refreshing MIDI devices: {e}")
    
    def test_playback(self) -> None:
        """Test audio playback with a middle C note."""
        if not self.audio_enabled.get():
            messagebox.showwarning("Audio Disabled", "Please enable audio playback first.")
            return
            
        if len(self.wave_samples) == 0 or np.all(self.wave_samples == 0):
            messagebox.showwarning("No Waveform", "Please draw a waveform first.")
            return
        
        # Play middle C (MIDI note 60)
        self.note_on(60, 100)
        
        # Schedule note off after 1 second
        def stop_test():
            self.note_off(60)
        
        self.root.after(1000, stop_test)
        self.update_status("Testing playback with middle C...")
    
    def update_notes_display(self) -> None:
        """Update the display of currently active notes."""
        if not self.current_notes:
            self.notes_label.config(text="None")
        else:
            notes_text = ", ".join([f"{note}" for note in sorted(self.current_notes.keys())])
            self.notes_label.config(text=notes_text)
    
    def on_closing(self) -> None:
        """Handle application closing - cleanup MIDI and audio."""
        try:
            # Stop all playing notes
            for note in list(self.current_notes.keys()):
                self.note_off(note)
            
            # Close MIDI input
            if self.midi_input:
                self.midi_input.close()
            
            # Quit pygame
            if PYGAME_AVAILABLE:
                pygame.midi.quit()
                pygame.mixer.quit()
                pygame.quit()
                
        except Exception as e:
            print(f"Cleanup error: {e}")
        
        # Close the application
        self.root.destroy()
        
    def setup_ui(self) -> None:
        """Setup the user interface."""
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="Waveform Drawer", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=2, pady=(0, 10))
        
        # Canvas frame
        canvas_frame = ttk.LabelFrame(main_frame, text="Draw Waveform", padding="5")
        canvas_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Drawing canvas
        self.canvas = tk.Canvas(canvas_frame, 
                               width=self.canvas_width, 
                               height=self.canvas_height,
                               bg="black", 
                               cursor="crosshair")
        self.canvas.pack()
        
        # Instructions
        instructions = ttk.Label(canvas_frame, 
                                text="Left click and drag to draw • Right click to clear • Center line = 0V")
        instructions.pack(pady=(5, 0))
        
        # Controls frame
        controls_frame = ttk.LabelFrame(main_frame, text="Audio Settings", padding="5")
        controls_frame.grid(row=2, column=0, sticky=(tk.W, tk.E), padx=(0, 5))
        
        # Sample rate
        ttk.Label(controls_frame, text="Sample Rate:").grid(row=0, column=0, sticky=tk.W, pady=2)
        sample_rate_combo = ttk.Combobox(controls_frame, textvariable=self.sample_rate,
                                        values=self.sample_rates, state="readonly", width=10)
        sample_rate_combo.grid(row=0, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        sample_rate_combo.bind("<<ComboboxSelected>>", self.on_sample_rate_changed)
        
        # Bit depth
        ttk.Label(controls_frame, text="Format:").grid(row=1, column=0, sticky=tk.W, pady=2)
        bit_depth_combo = ttk.Combobox(controls_frame, textvariable=self.bit_depth,
                                      values=self.bit_depths, state="readonly", width=15)
        bit_depth_combo.grid(row=1, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        
        # Wave resolution
        ttk.Label(controls_frame, text="Resolution:").grid(row=2, column=0, sticky=tk.W, pady=2)
        resolution_combo = ttk.Combobox(controls_frame, textvariable=self.wave_resolution,
                                       values=self.wave_resolutions, state="readonly", width=10)
        resolution_combo.grid(row=2, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        resolution_combo.bind("<<ComboboxSelected>>", self.on_resolution_changed)
        
        # MIDI controls frame
        midi_frame = ttk.LabelFrame(main_frame, text="MIDI & Audio", padding="5")
        midi_frame.grid(row=2, column=1, sticky=(tk.W, tk.E, tk.N), padx=(5, 0))
        
        # MIDI device selection
        ttk.Label(midi_frame, text="MIDI Device:").grid(row=0, column=0, sticky=tk.W, pady=2)
        device_names = [name for name, _ in self.midi_devices]
        self.midi_device_combo = ttk.Combobox(midi_frame, textvariable=self.selected_midi_device,
                                             values=device_names, state="readonly", width=20)
        self.midi_device_combo.grid(row=0, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        self.midi_device_combo.bind("<<ComboboxSelected>>", self.on_midi_device_changed)
        
        # MIDI enable/disable
        midi_enable_check = ttk.Checkbutton(midi_frame, text="MIDI Input Enabled", 
                                           variable=self.midi_enabled)
        midi_enable_check.grid(row=1, column=0, columnspan=2, sticky=tk.W, pady=2)
        
        # Audio enable/disable
        audio_enable_check = ttk.Checkbutton(midi_frame, text="Audio Playback Enabled", 
                                            variable=self.audio_enabled)
        audio_enable_check.grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=2)
        
        # Current notes display
        ttk.Label(midi_frame, text="Active Notes:").grid(row=3, column=0, sticky=tk.W, pady=2)
        self.notes_label = ttk.Label(midi_frame, text="None", foreground="blue")
        self.notes_label.grid(row=3, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        
        # Refresh MIDI devices button
        ttk.Button(midi_frame, text="Refresh MIDI", 
                  command=self.refresh_midi_devices).grid(row=4, column=0, pady=2, sticky=(tk.W, tk.E))
        
        # Reinitialize audio button
        ttk.Button(midi_frame, text="Reset Audio", 
                  command=self.reinitialize_mixer).grid(row=4, column=1, pady=2, padx=(5, 0), sticky=(tk.W, tk.E))
        
        # Action buttons frame
        actions_frame = ttk.LabelFrame(main_frame, text="Actions", padding="5")
        actions_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(10, 0))
        
        # Buttons
        ttk.Button(actions_frame, text="Clear Canvas", 
                  command=self.clear_canvas).grid(row=0, column=0, pady=2, sticky=(tk.W, tk.E))
        ttk.Button(actions_frame, text="Load Preset", 
                  command=self.load_preset).grid(row=0, column=1, pady=2, padx=(5, 0), sticky=(tk.W, tk.E))
        ttk.Button(actions_frame, text="Preview Wave", 
                  command=self.preview_waveform).grid(row=1, column=0, pady=2, sticky=(tk.W, tk.E))
        ttk.Button(actions_frame, text="Test Playback", 
                  command=self.test_playback).grid(row=1, column=1, pady=2, padx=(5, 0), sticky=(tk.W, tk.E))
        ttk.Button(actions_frame, text="Save WAV File", 
                  command=self.save_wav).grid(row=2, column=0, columnspan=2, pady=2, sticky=(tk.W, tk.E))
        
        # Configure action frame columns
        actions_frame.columnconfigure(0, weight=1)
        actions_frame.columnconfigure(1, weight=1)
        
        # Status frame
        status_frame = ttk.Frame(main_frame)
        status_frame.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(10, 0))
        
        self.status_label = ttk.Label(status_frame, text="Ready to draw waveform")
        self.status_label.pack(side=tk.LEFT)
        
        # Configure grid weights
        main_frame.columnconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=0)
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        
    def canvas_to_wave_coords(self, x: float, y: float) -> Tuple[float, float]:
        """Convert canvas coordinates to normalized wave coordinates."""
        # X: 0 to 1 (one complete cycle)
        wave_x = (x - self.canvas_margin) / (self.canvas_width - 2 * self.canvas_margin)
        wave_x = max(0.0, min(1.0, wave_x))
        
        # Y: -1 to 1 (amplitude)
        center_y = self.canvas_height / 2
        wave_y = -(y - center_y) / (self.canvas_height / 2 - self.canvas_margin)
        wave_y = max(-1.0, min(1.0, wave_y))
        
        return wave_x, wave_y
        
    def wave_to_canvas_coords(self, wave_x: float, wave_y: float) -> Tuple[float, float]:
        """Convert normalized wave coordinates to canvas coordinates."""
        x = wave_x * (self.canvas_width - 2 * self.canvas_margin) + self.canvas_margin
        center_y = self.canvas_height / 2
        y = center_y - wave_y * (self.canvas_height / 2 - self.canvas_margin)
        return x, y
        
    def start_drawing(self, event) -> None:
        """Start drawing when mouse is pressed."""
        self.is_drawing = True
        wave_x, wave_y = self.canvas_to_wave_coords(event.x, event.y)
        self.wave_points = [(wave_x, wave_y)]
        self.last_x, self.last_y = event.x, event.y
        self.update_status(f"Drawing at ({wave_x:.3f}, {wave_y:.3f})")
        
    def continue_drawing(self, event) -> None:
        """Continue drawing when mouse is dragged."""
        if not self.is_drawing:
            return
            
        # Draw line from last position to current
        self.canvas.create_line(self.last_x, self.last_y, event.x, event.y, 
                               fill="lime", width=2, tags="waveform")
        
        # Add point to wave data
        wave_x, wave_y = self.canvas_to_wave_coords(event.x, event.y)
        self.wave_points.append((wave_x, wave_y))
        
        self.last_x, self.last_y = event.x, event.y
        self.update_status(f"Drawing at ({wave_x:.3f}, {wave_y:.3f})")
        
    def stop_drawing(self, event) -> None:
        """Stop drawing when mouse is released."""
        self.is_drawing = False
        self.generate_wave_samples()
        self.update_status(f"Waveform drawn with {len(self.wave_points)} points")
        
    def generate_wave_samples(self) -> None:
        """Generate sample array from drawn points."""
        if not self.wave_points:
            return
            
        resolution = self.wave_resolution.get()
        self.wave_samples = np.zeros(resolution)
        
        # Sort points by x coordinate
        sorted_points = sorted(self.wave_points, key=lambda p: p[0])
        
        # Interpolate to fill sample array
        if len(sorted_points) >= 2:
            x_coords = [p[0] for p in sorted_points]
            y_coords = [p[1] for p in sorted_points]
            
            # Create sample positions (0 to 1)
            sample_positions = np.linspace(0, 1, resolution)
            
            # Interpolate
            self.wave_samples = np.interp(sample_positions, x_coords, y_coords)
        elif len(sorted_points) == 1:
            # Single point - create constant level
            self.wave_samples[:] = sorted_points[0][1]
            
    def clear_canvas(self, event=None) -> None:
        """Clear the drawing canvas."""
        self.canvas.delete("waveform")
        self.draw_grid()
        self.wave_points = []
        self.wave_samples = np.zeros(self.wave_resolution.get())
        self.update_status("Canvas cleared")
        
    def draw_grid(self) -> None:
        """Draw grid lines and labels on canvas."""
        # Clear existing grid
        self.canvas.delete("grid")
        
        # Center line (0V)
        center_y = self.canvas_height / 2
        self.canvas.create_line(self.canvas_margin, center_y, 
                               self.canvas_width - self.canvas_margin, center_y,
                               fill="gray50", width=1, tags="grid")
        
        # Amplitude lines (+1V, -1V)
        top_y = self.canvas_margin
        bottom_y = self.canvas_height - self.canvas_margin
        
        self.canvas.create_line(self.canvas_margin, top_y,
                               self.canvas_width - self.canvas_margin, top_y,
                               fill="gray30", width=1, tags="grid")
        self.canvas.create_line(self.canvas_margin, bottom_y,
                               self.canvas_width - self.canvas_margin, bottom_y,
                               fill="gray30", width=1, tags="grid")
        
        # Vertical time divisions
        for i in range(9):  # 8 divisions + start/end
            x = self.canvas_margin + i * (self.canvas_width - 2 * self.canvas_margin) / 8
            self.canvas.create_line(x, top_y, x, bottom_y,
                                   fill="gray20", width=1, tags="grid")
        
        # Labels
        self.canvas.create_text(20, top_y, text="+1V", fill="white", tags="grid")
        self.canvas.create_text(20, center_y, text="0V", fill="white", tags="grid")
        self.canvas.create_text(20, bottom_y, text="-1V", fill="white", tags="grid")
        
        self.canvas.create_text(self.canvas_margin, self.canvas_height - 10, 
                               text="0°", fill="white", tags="grid")
        self.canvas.create_text(self.canvas_width - self.canvas_margin, self.canvas_height - 10,
                               text="360°", fill="white", tags="grid")
        
    def on_sample_rate_changed(self, event=None) -> None:
        """Handle sample rate change."""
        self.reinitialize_mixer()
        
    def on_resolution_changed(self, event=None) -> None:
        """Handle resolution change."""
        if self.wave_points:
            self.generate_wave_samples()
            self.update_status(f"Resolution changed to {self.wave_resolution.get()} samples")
    
    def reinitialize_mixer(self) -> None:
        """Reinitialize the pygame mixer with current settings."""
        if not PYGAME_AVAILABLE:
            return
            
        try:
            # Stop all sounds and quit mixer
            pygame.mixer.stop()
            pygame.mixer.quit()
            
            # Reinitialize with current sample rate
            sample_rate = self.sample_rate.get()
            pygame.mixer.pre_init(
                frequency=sample_rate,
                size=-16,
                channels=1,  # Force mono
                buffer=1024
            )
            pygame.mixer.init()
            
            # Verify configuration
            mixer_freq, mixer_format, mixer_channels = pygame.mixer.get_init()
            self.update_status(f"Mixer reinitialized: {mixer_freq}Hz, {mixer_channels} channels")
            
        except Exception as e:
            print(f"Mixer reinitialize error: {e}")
            self.update_status(f"Mixer error: {e}")
        
    def load_preset(self) -> None:
        """Load a preset waveform."""
        presets = {
            "Sine Wave": self.generate_sine,
            "Square Wave": self.generate_square,
            "Sawtooth Wave": self.generate_sawtooth,
            "Triangle Wave": self.generate_triangle,
            "Pulse Wave": self.generate_pulse,
            "Noise": self.generate_noise
        }
        
        # Create preset selection dialog
        preset_window = tk.Toplevel(self.root)
        preset_window.title("Load Preset")
        preset_window.geometry("250x200")
        preset_window.transient(self.root)
        preset_window.grab_set()
        
        ttk.Label(preset_window, text="Select a preset waveform:").pack(pady=10)
        
        for name, func in presets.items():
            ttk.Button(preset_window, text=name, 
                      command=lambda f=func, w=preset_window: self.apply_preset(f, w)).pack(pady=2)
        
    def apply_preset(self, preset_func, window) -> None:
        """Apply a preset waveform."""
        window.destroy()
        self.clear_canvas()
        preset_func()
        self.draw_waveform_from_samples()
        self.update_status("Preset loaded")
        
    def generate_sine(self) -> None:
        """Generate sine wave."""
        resolution = self.wave_resolution.get()
        t = np.linspace(0, 2 * np.pi, resolution, endpoint=False)
        self.wave_samples = np.sin(t)
        
    def generate_square(self) -> None:
        """Generate square wave."""
        resolution = self.wave_resolution.get()
        t = np.linspace(0, 1, resolution, endpoint=False)
        self.wave_samples = np.where(t < 0.5, 1.0, -1.0)
        
    def generate_sawtooth(self) -> None:
        """Generate sawtooth wave."""
        resolution = self.wave_resolution.get()
        t = np.linspace(0, 1, resolution, endpoint=False)
        self.wave_samples = 2 * t - 1
        
    def generate_triangle(self) -> None:
        """Generate triangle wave."""
        resolution = self.wave_resolution.get()
        t = np.linspace(0, 1, resolution, endpoint=False)
        self.wave_samples = np.where(t < 0.5, 4 * t - 1, 3 - 4 * t)
        
    def generate_pulse(self) -> None:
        """Generate pulse wave (25% duty cycle)."""
        resolution = self.wave_resolution.get()
        t = np.linspace(0, 1, resolution, endpoint=False)
        self.wave_samples = np.where(t < 0.25, 1.0, -1.0)
        
    def generate_noise(self) -> None:
        """Generate noise."""
        resolution = self.wave_resolution.get()
        self.wave_samples = np.random.uniform(-1, 1, resolution)
        
    def draw_waveform_from_samples(self) -> None:
        """Draw waveform on canvas from sample data."""
        if len(self.wave_samples) == 0:
            return
            
        # Convert samples to canvas coordinates
        points = []
        for i, sample in enumerate(self.wave_samples):
            wave_x = i / len(self.wave_samples)
            canvas_x, canvas_y = self.wave_to_canvas_coords(wave_x, sample)
            points.extend([canvas_x, canvas_y])
            
        # Draw the waveform
        if len(points) >= 4:
            self.canvas.create_line(*points, fill="lime", width=2, tags="waveform")
            
    def preview_waveform(self) -> None:
        """Preview the drawn waveform (show statistics)."""
        if len(self.wave_samples) == 0 or np.all(self.wave_samples == 0):
            messagebox.showwarning("Preview", "No waveform data to preview!")
            return
            
        # Calculate statistics
        min_val = np.min(self.wave_samples)
        max_val = np.max(self.wave_samples)
        rms = np.sqrt(np.mean(self.wave_samples ** 2))
        dc_offset = np.mean(self.wave_samples)
        
        # Show preview window
        preview_window = tk.Toplevel(self.root)
        preview_window.title("Waveform Preview")
        preview_window.geometry("400x300")
        preview_window.transient(self.root)
        
        text = tk.Text(preview_window, wrap=tk.WORD, padx=10, pady=10)
        text.pack(fill=tk.BOTH, expand=True)
        
        preview_text = f"""Waveform Statistics:
        
Resolution: {len(self.wave_samples)} samples
Sample Rate: {self.sample_rate.get()} Hz
Format: {self.bit_depth.get()}

Amplitude Analysis:
• Minimum: {min_val:.4f}
• Maximum: {max_val:.4f}
• Peak-to-Peak: {max_val - min_val:.4f}
• RMS Level: {rms:.4f}
• DC Offset: {dc_offset:.4f}

Cycle Duration: {len(self.wave_samples) / self.sample_rate.get() * 1000:.2f} ms
Fundamental Freq: {self.sample_rate.get() / len(self.wave_samples):.2f} Hz

Note: This represents one complete waveform cycle.
When saved, only this single cycle will be written
to the WAV file (no repetition).
        """
        
        text.insert(tk.END, preview_text)
        text.config(state=tk.DISABLED)
        
    def save_wav(self) -> None:
        """Save the waveform as a WAV file."""
        if len(self.wave_samples) == 0 or np.all(self.wave_samples == 0):
            messagebox.showerror("Save Error", "No waveform data to save!")
            return
            
        # Get save filename
        pcm_folder = os.path.join(os.path.dirname(os.path.dirname(__file__)), "pcm")
        os.makedirs(pcm_folder, exist_ok=True)
        
        default_name = f"custom_wave_{self.wave_resolution.get()}samples_{self.sample_rate.get()}Hz"
        if self.bit_depth.get() == "16-bit PCM":
            default_name += "_16bit.wav"
        else:
            default_name += "_32float.wav"
            
        filename = filedialog.asksaveasfilename(
            title="Save Waveform",
            defaultextension=".wav",
            initialdir=pcm_folder,
            initialfile=default_name,
            filetypes=[("WAV files", "*.wav"), ("All files", "*.*")]
        )
        
        if not filename:
            return
            
        try:
            self.write_wav_file(filename)
            self.update_status(f"Waveform saved to {os.path.basename(filename)}")
            messagebox.showinfo("Save Successful", 
                              f"Waveform saved successfully!\n\nFile: {os.path.basename(filename)}\n"
                              f"Location: {os.path.dirname(filename)}\n"
                              f"Size: {len(self.wave_samples)} samples\n"
                              f"Format: {self.bit_depth.get()}\n"
                              f"Sample Rate: {self.sample_rate.get()} Hz")
        except Exception as e:
            messagebox.showerror("Save Error", f"Failed to save waveform:\n{str(e)}")
            
    def write_wav_file(self, filename: str) -> None:
        """Write waveform data to WAV file."""
        sample_rate = self.sample_rate.get()
        is_float = self.bit_depth.get() == "32-bit Float"
        
        # Use the single cycle as-is (no repetition)
        cycle_samples = self.wave_samples
        
        if SCIPY_AVAILABLE and is_float:
            # Use scipy for 32-bit float
            wavfile.write(filename, sample_rate, cycle_samples.astype(np.float32))
        else:
            # Use built-in wave module for 16-bit PCM
            if is_float:
                print("Warning: scipy not available, saving as 16-bit PCM instead of 32-bit float")
            
            # Convert to 16-bit PCM
            samples_16bit = (cycle_samples * 32767).astype(np.int16)
            
            with wave.open(filename, 'wb') as wav_file:
                wav_file.setnchannels(1)  # Mono
                wav_file.setsampwidth(2)  # 16-bit
                wav_file.setframerate(sample_rate)
                
                # Write samples
                for sample in samples_16bit:
                    wav_file.writeframes(struct.pack('<h', sample))
                    
    def update_status(self, message: str) -> None:
        """Update the status label."""
        self.status_label.config(text=message)
        self.root.update_idletasks()

def main():
    """Main entry point."""
    print("Waveform Drawer - Synth Utility")
    print("================================")
    print("Starting GUI application...")
    
    # Check dependencies
    if not SCIPY_AVAILABLE:
        print("Warning: scipy not available. 32-bit float output will fallback to 16-bit PCM.")
    
    # Create and run application
    root = tk.Tk()
    app = WaveformDrawer(root)
    
    try:
        root.mainloop()
    except KeyboardInterrupt:
        print("\nApplication interrupted by user")
    except Exception as e:
        print(f"Application error: {e}")
        return 1
        
    print("Application closed.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
