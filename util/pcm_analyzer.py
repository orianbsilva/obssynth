#!/usr/bin/env python3
"""
PCM Waveform Player - Integration Demo
======================================

This script demonstrates how to use waveforms created with the Waveform Drawer
in Python-based audio applications. It can play back WAV files from the PCM
folder and show basic analysis.

This serves as a bridge between the GUI waveform creator and the C++ synthesizer.
"""

import os
import sys
import numpy as np
from typing import List, Tuple, Optional

try:
    from scipy.io import wavfile
    import scipy.signal
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    import wave

try:
    import matplotlib.pyplot as plt
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

class PCMWaveformAnalyzer:
    """Analyzes waveforms from the PCM folder."""
    
    def __init__(self, pcm_folder: str = None):
        """Initialize with PCM folder path."""
        if pcm_folder is None:
            # Default to ../pcm relative to this script
            script_dir = os.path.dirname(__file__)
            self.pcm_folder = os.path.join(os.path.dirname(script_dir), "pcm")
        else:
            self.pcm_folder = pcm_folder
            
        if not os.path.exists(self.pcm_folder):
            raise FileNotFoundError(f"PCM folder not found: {self.pcm_folder}")
    
    def list_waveforms(self) -> List[str]:
        """List all WAV files in the PCM folder."""
        wav_files = []
        for filename in os.listdir(self.pcm_folder):
            if filename.lower().endswith('.wav'):
                wav_files.append(filename)
        return sorted(wav_files)
    
    def load_waveform(self, filename: str) -> Tuple[np.ndarray, int]:
        """Load a waveform from a WAV file."""
        filepath = os.path.join(self.pcm_folder, filename)
        
        if SCIPY_AVAILABLE:
            sample_rate, data = wavfile.read(filepath)
            
            # Convert to float if necessary
            if data.dtype == np.int16:
                data = data.astype(np.float32) / 32767.0
            elif data.dtype == np.int32:
                data = data.astype(np.float32) / 2147483647.0
                
            return data, sample_rate
        else:
            # Use built-in wave module
            with wave.open(filepath, 'rb') as wav_file:
                frames = wav_file.readframes(wav_file.getnframes())
                sample_rate = wav_file.getframerate()
                
                # Convert bytes to numpy array
                if wav_file.getsampwidth() == 2:  # 16-bit
                    data = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32767.0
                else:
                    raise ValueError(f"Unsupported sample width: {wav_file.getsampwidth()}")
                    
                return data, sample_rate
    
    def analyze_waveform(self, data: np.ndarray, sample_rate: int) -> dict:
        """Analyze a waveform and return statistics."""
        stats = {
            'length': len(data),
            'duration': len(data) / sample_rate,
            'sample_rate': sample_rate,
            'min_value': float(np.min(data)),
            'max_value': float(np.max(data)),
            'peak_to_peak': float(np.max(data) - np.min(data)),
            'rms': float(np.sqrt(np.mean(data ** 2))),
            'dc_offset': float(np.mean(data)),
            'zero_crossings': int(np.sum(np.diff(np.sign(data)) != 0))
        }
        
        # Calculate fundamental frequency (if scipy available)
        if SCIPY_AVAILABLE and len(data) > 1024:
            # Find the period by autocorrelation
            autocorr = np.correlate(data, data, mode='full')
            autocorr = autocorr[autocorr.size // 2:]
            
            # Find the first peak after the center
            if len(autocorr) > 1:
                peaks, _ = scipy.signal.find_peaks(autocorr[1:], height=0.1 * np.max(autocorr))
                if len(peaks) > 0:
                    period_samples = peaks[0] + 1
                    stats['fundamental_freq'] = sample_rate / period_samples
                else:
                    stats['fundamental_freq'] = None
            else:
                stats['fundamental_freq'] = None
        else:
            stats['fundamental_freq'] = None
            
        return stats
    
    def extract_cycle(self, data: np.ndarray, sample_rate: int) -> Optional[np.ndarray]:
        """Extract one cycle from the waveform data."""
        if not SCIPY_AVAILABLE:
            print("Warning: scipy not available, cannot extract cycle accurately")
            # Simple approximation: assume periodic data
            cycle_length = len(data) // (len(data) // 1024)  # Approximate
            return data[:cycle_length] if cycle_length > 0 else data
        
        # Use autocorrelation to find the period
        autocorr = np.correlate(data, data, mode='full')
        autocorr = autocorr[autocorr.size // 2:]
        
        # Find peaks in autocorrelation
        peaks, _ = scipy.signal.find_peaks(autocorr[1:], height=0.1 * np.max(autocorr))
        
        if len(peaks) > 0:
            period_samples = peaks[0] + 1
            return data[:period_samples]
        else:
            # Fallback: return first 1024 samples or all data
            return data[:min(1024, len(data))]
    
    def plot_waveform(self, filename: str, show_analysis: bool = True) -> None:
        """Plot a waveform with optional analysis."""
        if not MATPLOTLIB_AVAILABLE:
            print("Matplotlib not available, cannot plot waveforms")
            return
            
        data, sample_rate = self.load_waveform(filename)
        stats = self.analyze_waveform(data, sample_rate)
        
        # Create time axis
        time = np.arange(len(data)) / sample_rate
        
        # Plot full waveform
        plt.figure(figsize=(12, 8))
        
        # Full waveform
        plt.subplot(2, 2, 1)
        plt.plot(time, data, 'b-', linewidth=1)
        plt.title(f'Full Waveform: {filename}')
        plt.xlabel('Time (seconds)')
        plt.ylabel('Amplitude')
        plt.grid(True, alpha=0.3)
        
        # Single cycle
        cycle = self.extract_cycle(data, sample_rate)
        cycle_time = np.arange(len(cycle)) / sample_rate * 1000  # Convert to ms
        
        plt.subplot(2, 2, 2)
        plt.plot(cycle_time, cycle, 'r-', linewidth=2)
        plt.title('Single Cycle')
        plt.xlabel('Time (ms)')
        plt.ylabel('Amplitude')
        plt.grid(True, alpha=0.3)
        
        # Frequency spectrum (if scipy available)
        if SCIPY_AVAILABLE:
            plt.subplot(2, 2, 3)
            freqs = np.fft.fftfreq(len(cycle), 1/sample_rate)
            fft = np.fft.fft(cycle)
            magnitude = np.abs(fft)
            
            # Plot only positive frequencies
            pos_mask = freqs >= 0
            plt.plot(freqs[pos_mask], magnitude[pos_mask], 'g-')
            plt.title('Frequency Spectrum')
            plt.xlabel('Frequency (Hz)')
            plt.ylabel('Magnitude')
            plt.grid(True, alpha=0.3)
            plt.xlim(0, sample_rate // 8)  # Show up to 1/8 of sample rate
        
        # Statistics text
        plt.subplot(2, 2, 4)
        plt.axis('off')
        
        if show_analysis:
            stats_text = f"""Waveform Statistics:
            
Length: {stats['length']:,} samples
Duration: {stats['duration']:.3f} seconds
Sample Rate: {stats['sample_rate']:,} Hz

Amplitude:
• Min: {stats['min_value']:.4f}
• Max: {stats['max_value']:.4f}
• Peak-to-Peak: {stats['peak_to_peak']:.4f}
• RMS: {stats['rms']:.4f}
• DC Offset: {stats['dc_offset']:.4f}

Analysis:
• Zero Crossings: {stats['zero_crossings']}"""
            
            if stats['fundamental_freq'] is not None:
                stats_text += f"\n• Fundamental: {stats['fundamental_freq']:.1f} Hz"
            
            plt.text(0.05, 0.95, stats_text, transform=plt.gca().transAxes,
                    verticalalignment='top', fontfamily='monospace', fontsize=9)
        
        plt.tight_layout()
        plt.show()

def main():
    """Main entry point."""
    print("PCM Waveform Analyzer")
    print("====================")
    print()
    
    try:
        analyzer = PCMWaveformAnalyzer()
        print(f"PCM folder: {analyzer.pcm_folder}")
        
        # List available waveforms
        waveforms = analyzer.list_waveforms()
        
        if not waveforms:
            print("No WAV files found in PCM folder!")
            print("Run the waveform drawer or test script first:")
            print("  python waveform_drawer.py")
            print("  python test_waveforms.py")
            return 1
        
        print(f"Found {len(waveforms)} waveform(s):")
        for i, filename in enumerate(waveforms):
            filepath = os.path.join(analyzer.pcm_folder, filename)
            size = os.path.getsize(filepath)
            print(f"  [{i+1:2d}] {filename} ({size:,} bytes)")
        
        print()
        
        # Interactive analysis
        while True:
            print("Options:")
            print("  1-N  - Analyze waveform by number")
            print("  a    - Analyze all waveforms")
            print("  p    - Plot waveform (requires matplotlib)")
            print("  q    - Quit")
            
            choice = input("\nChoice: ").strip().lower()
            
            if choice == 'q':
                break
            elif choice == 'a':
                # Analyze all waveforms
                for filename in waveforms:
                    print(f"\nAnalyzing: {filename}")
                    print("-" * 50)
                    
                    try:
                        data, sample_rate = analyzer.load_waveform(filename)
                        stats = analyzer.analyze_waveform(data, sample_rate)
                        
                        print(f"Length: {stats['length']:,} samples")
                        print(f"Duration: {stats['duration']:.3f} seconds")
                        print(f"Sample Rate: {stats['sample_rate']:,} Hz")
                        print(f"Range: {stats['min_value']:.4f} to {stats['max_value']:.4f}")
                        print(f"RMS Level: {stats['rms']:.4f}")
                        if stats['fundamental_freq']:
                            print(f"Fundamental: {stats['fundamental_freq']:.1f} Hz")
                        
                    except Exception as e:
                        print(f"Error analyzing {filename}: {e}")
                        
            elif choice == 'p':
                if not MATPLOTLIB_AVAILABLE:
                    print("Matplotlib not available for plotting!")
                    continue
                    
                # Select waveform to plot
                print("\nSelect waveform to plot:")
                for i, filename in enumerate(waveforms):
                    print(f"  [{i+1}] {filename}")
                
                try:
                    idx = int(input("Number: ")) - 1
                    if 0 <= idx < len(waveforms):
                        analyzer.plot_waveform(waveforms[idx])
                    else:
                        print("Invalid selection!")
                except (ValueError, IndexError):
                    print("Invalid input!")
                    
            elif choice.isdigit():
                # Analyze specific waveform
                try:
                    idx = int(choice) - 1
                    if 0 <= idx < len(waveforms):
                        filename = waveforms[idx]
                        print(f"\nDetailed analysis: {filename}")
                        print("=" * 60)
                        
                        data, sample_rate = analyzer.load_waveform(filename)
                        stats = analyzer.analyze_waveform(data, sample_rate)
                        
                        for key, value in stats.items():
                            if isinstance(value, float):
                                print(f"{key.replace('_', ' ').title()}: {value:.6f}")
                            else:
                                print(f"{key.replace('_', ' ').title()}: {value}")
                                
                        # Show cycle information
                        cycle = analyzer.extract_cycle(data, sample_rate)
                        print(f"\nSingle cycle length: {len(cycle)} samples")
                        print(f"Cycle duration: {len(cycle) / sample_rate * 1000:.2f} ms")
                        
                    else:
                        print("Invalid selection!")
                except (ValueError, IndexError):
                    print("Invalid input!")
            else:
                print("Invalid choice!")
        
        print("\nAnalysis complete.")
        return 0
        
    except Exception as e:
        print(f"Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
