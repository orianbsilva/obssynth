#!/usr/bin/env python3
"""
Waveform Drawer Test Script
===========================

This script demonstrates the core functionality of the waveform drawer
by creating sample waveforms programmatically and saving them to the PCM folder.
"""

import numpy as np
import os
import sys

# Add the current directory to the path so we can import waveform_drawer
sys.path.insert(0, os.path.dirname(__file__))

try:
    from scipy.io import wavfile
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False

import wave
import struct

def write_wav_file(filename: str, samples: np.ndarray, sample_rate: int, is_float: bool = True) -> None:
    """Write waveform data to WAV file as a single cycle."""
    
    # Use the samples directly - no repetition, just one cycle
    waveform_samples = samples.copy()
    
    if SCIPY_AVAILABLE and is_float:
        # Use scipy for 32-bit float
        wavfile.write(filename, sample_rate, waveform_samples.astype(np.float32))
    else:
        # Use built-in wave module for 16-bit PCM
        if is_float:
            print("Warning: scipy not available, saving as 16-bit PCM instead of 32-bit float")
        
        # Convert to 16-bit PCM
        samples_16bit = (waveform_samples * 32767).astype(np.int16)
        
        with wave.open(filename, 'wb') as wav_file:
            wav_file.setnchannels(1)  # Mono
            wav_file.setsampwidth(2)  # 16-bit
            wav_file.setframerate(sample_rate)
            
            # Write samples
            for sample in samples_16bit:
                wav_file.writeframes(struct.pack('<h', sample))

def create_test_waveforms():
    """Create test waveforms and save them to the PCM folder."""
    
    # Setup
    pcm_folder = os.path.join(os.path.dirname(os.path.dirname(__file__)), "pcm")
    os.makedirs(pcm_folder, exist_ok=True)
    
    sample_rate = 48000
    resolution = 1024
    
    print("Waveform Drawer Test Script")
    print("===========================")
    print(f"Output folder: {pcm_folder}")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Resolution: {resolution} samples per cycle")
    print(f"Scipy available: {SCIPY_AVAILABLE}")
    print(f"Note: Generating single-cycle waveforms (not 1-second files)")
    print()
    
    # Test waveforms
    waveforms = {
        "sine": lambda t: np.sin(2 * np.pi * t),
        "square": lambda t: np.where(t < 0.5, 1.0, -1.0),
        "sawtooth": lambda t: 2 * t - 1,
        "triangle": lambda t: np.where(t < 0.5, 4 * t - 1, 3 - 4 * t),
        "pulse": lambda t: np.where(t < 0.25, 1.0, -1.0),
        "custom_curve": lambda t: np.sin(2 * np.pi * t) * np.exp(-2 * t),  # Exponential decay sine
        "harmonic_rich": lambda t: (np.sin(2 * np.pi * t) + 
                                   0.5 * np.sin(4 * np.pi * t) + 
                                   0.25 * np.sin(6 * np.pi * t)),  # Multiple harmonics
    }
    
    # Generate and save each waveform
    for name, func in waveforms.items():
        print(f"Generating {name} waveform...")
        
        # Create time array (0 to 1 for one complete cycle)
        t = np.linspace(0, 1, resolution, endpoint=False)
        samples = func(t)
        
        # Normalize to [-1, 1] range
        max_val = np.max(np.abs(samples))
        if max_val > 0:
            samples = samples / max_val
        
        # Save as both 16-bit and 32-bit float
        for bit_depth, is_float in [("16bit", False), ("32float", True)]:
            filename = os.path.join(pcm_folder, f"cycle_{name}_{resolution}samples_{bit_depth}.wav")
            write_wav_file(filename, samples, sample_rate, is_float)
            
            # Calculate file size
            file_size = os.path.getsize(filename)
            print(f"  Saved {bit_depth}: {os.path.basename(filename)} ({file_size:,} bytes)")
    
    print()
    print("Test waveforms created successfully!")
    print(f"Total files: {len(waveforms) * 2}")
    print()
    print("You can now:")
    print("1. Listen to the generated WAV files")
    print("2. Run the GUI application: python waveform_drawer.py")
    print("3. Load these files in audio software to see the waveforms")

def create_wav_bank(bank_name: str, waveforms_list: list, bank_size: int = 64):
    """Create a wav bank file containing multiple waveforms."""
    
    pcm_folder = os.path.join(os.path.dirname(os.path.dirname(__file__)), "pcm")
    os.makedirs(pcm_folder, exist_ok=True)
    
    sample_rate = 48000
    resolution = 1024
    
    print(f"Creating wav bank: {bank_name}")
    print(f"Bank size: {bank_size} waveforms")
    print(f"Resolution: {resolution} samples per waveform")
    print()
    
    # Prepare the bank data - concatenate all waveforms
    all_samples = []
    
    # Create time array for one cycle
    t = np.linspace(0, 1, resolution, endpoint=False)
    
    # Add each waveform to the bank
    for i in range(bank_size):
        if i < len(waveforms_list):
            # Use provided waveform
            name, func = waveforms_list[i]
            samples = func(t)
            print(f"  [{i:2d}] {name}")
        else:
            # Fill remaining slots with silence
            samples = np.zeros(resolution)
            print(f"  [{i:2d}] (silent)")
        
        # Normalize to [-1, 1] range
        max_val = np.max(np.abs(samples))
        if max_val > 0:
            samples = samples / max_val
        
        all_samples.extend(samples)
    
    # Convert to numpy array
    bank_samples = np.array(all_samples, dtype=np.float32)
    
    # Save as 32-bit float WAV bank
    bank_filename = os.path.join(pcm_folder, f"bank_{bank_name}_{bank_size}waves_{resolution}samples_32float.wav")
    
    if SCIPY_AVAILABLE:
        wavfile.write(bank_filename, sample_rate, bank_samples)
        print(f"\nWav bank created: {os.path.basename(bank_filename)}")
        file_size = os.path.getsize(bank_filename)
        print(f"File size: {file_size:,} bytes")
        print(f"Total samples: {len(bank_samples):,}")
        print(f"Duration: {len(bank_samples) / sample_rate:.3f} seconds")
    else:
        print("Error: scipy required for wav bank creation")
        return False
    
    return True

def create_test_wav_banks():
    """Create several test wav banks with different characteristics."""
    
    # Basic waveforms bank
    basic_waveforms = [
        ("sine", lambda t: np.sin(2 * np.pi * t)),
        ("square", lambda t: np.where(t < 0.5, 1.0, -1.0)),
        ("sawtooth", lambda t: 2 * t - 1),
        ("triangle", lambda t: np.where(t < 0.5, 4 * t - 1, 3 - 4 * t)),
        ("pulse_25", lambda t: np.where(t < 0.25, 1.0, -1.0)),
        ("pulse_10", lambda t: np.where(t < 0.1, 1.0, -1.0)),
        ("ramp_up", lambda t: t * 2 - 1),
        ("ramp_down", lambda t: 1 - t * 2),
    ]
    
    # Harmonic series bank (sine waves with different harmonic content)
    harmonic_waveforms = []
    for i in range(16):
        harmonic_num = i + 1
        name = f"harmonic_{harmonic_num}"
        # Create sine wave with harmonic content
        func = lambda t, h=harmonic_num: np.sin(2 * np.pi * t * h) / h
        harmonic_waveforms.append((name, func))
    
    # Wavetable bank (morphing between different waveforms)
    morph_waveforms = []
    for i in range(32):
        morph_factor = i / 31.0  # 0 to 1
        name = f"morph_{i:02d}"
        # Morph between sine and sawtooth
        func = lambda t, m=morph_factor: (1-m) * np.sin(2 * np.pi * t) + m * (2 * t - 1)
        morph_waveforms.append((name, func))
    
    # Create the banks
    create_wav_bank("basic", basic_waveforms, 16)
    create_wav_bank("harmonic", harmonic_waveforms, 32)
    create_wav_bank("morph", morph_waveforms, 64)

def main():
    """Main entry point."""
    try:
        print("Choose operation:")
        print("1. Create individual waveforms")
        print("2. Create wav banks")
        print("3. Create both")
        
        choice = input("Enter choice (1-3, default=1): ").strip()
        
        if choice == "2":
            create_test_wav_banks()
        elif choice == "3":
            create_test_waveforms()
            print("\n" + "="*50 + "\n")
            create_test_wav_banks()
        else:
            create_test_waveforms()
        
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
