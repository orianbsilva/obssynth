#!/usr/bin/env python3
"""
gen_wavbank.py — Wavetable Bank Generator
==========================================

Generates a multi-table single-cycle WAV bank compatible with the
ObsSynth4D WaveTableOsc / PCMWaveform loader.

The output file contains N waveforms concatenated end-to-end.
Each waveform is exactly `samples_per_wave` samples long.
The bank is detected automatically by the PCMWaveform loader when
`loadWavBank()` is used.

Usage examples
--------------
# Default: sine→saw morph, 16 tables, 2048 spw, saved to pcm/
python util/gen_wavbank.py

# Specific preset
python util/gen_wavbank.py --preset sine_to_square --tables 32 --spw 2048

# Custom sequence of wave types (interpolated across the bank)
python util/gen_wavbank.py --waves sine saw square triangle --tables 64

# List available presets
python util/gen_wavbank.py --list

Requirements
------------
  pip install numpy scipy matplotlib
"""

import argparse
import os
import sys
import math
import struct

try:
    import numpy as np
except ImportError:
    print("ERROR: numpy required.  pip install numpy")
    sys.exit(1)

try:
    from scipy.io import wavfile as scipy_wavfile
    SCIPY_OK = True
except ImportError:
    SCIPY_OK = False

try:
    import matplotlib.pyplot as plt
    import matplotlib.cm as cm
    MPL_OK = True
except ImportError:
    MPL_OK = False

# ─── Band-limiting via FFT (brick-wall anti-alias) ───────────────────────────
# For wavetable generation, FFT band-limiting is the correct approach:
# generate the ideal waveform → FFT → zero harmonics above the cutoff → IFFT.
# This is superior to additive Fourier series (which approximates) and to
# PolyBLEP (which is a per-sample real-time correction, not suited for offline
# table generation).  The result is a perfectly band-limited single-cycle wave.

def fft_bandlimit(wave: np.ndarray, max_harmonic: int = None) -> np.ndarray:
    """
    Band-limit a single-cycle waveform by zeroing harmonics above max_harmonic.
    max_harmonic=None → keep all harmonics up to SPW/2 (Nyquist).
    max_harmonic=k    → keep only harmonics 1..k (removes aliasing above k).
    """
    spw = len(wave)
    if max_harmonic is None:
        max_harmonic = spw // 2
    spectrum = np.fft.rfft(wave)
    # Zero DC (harmonic 0) and harmonics above max_harmonic
    spectrum[0] = 0.0
    cutoff = min(max_harmonic + 1, len(spectrum))
    spectrum[cutoff:] = 0.0
    out = np.fft.irfft(spectrum, n=spw)
    # Normalise to ±1
    peak = np.max(np.abs(out))
    if peak > 1e-9:
        out /= peak
    return out.astype(np.float64)


# ─── Wave generators ─────────────────────────────────────────────────────────
# All *_raw functions generate the naive (aliased) waveform.
# The *_bl / fft variants apply FFT band-limiting.

def wave_sine(phase: np.ndarray) -> np.ndarray:
    return np.sin(2.0 * math.pi * phase)

def wave_saw_raw(phase: np.ndarray) -> np.ndarray:
    return 2.0 * phase - 1.0

def wave_saw_bl(phase: np.ndarray, max_harmonic: int = None) -> np.ndarray:
    """Band-limited sawtooth via FFT."""
    return fft_bandlimit(wave_saw_raw(phase), max_harmonic)

def wave_square_raw(phase: np.ndarray) -> np.ndarray:
    return np.where(phase < 0.5, 1.0, -1.0).astype(float)

def wave_square_bl(phase: np.ndarray, max_harmonic: int = None) -> np.ndarray:
    """Band-limited square via FFT."""
    return fft_bandlimit(wave_square_raw(phase), max_harmonic)

def wave_triangle(phase: np.ndarray) -> np.ndarray:
    # Triangle is already naturally band-limited (very low harmonic energy)
    return (2.0 * np.abs(2.0 * phase - 1.0) - 1.0).astype(np.float64)

def wave_pulse25_raw(phase: np.ndarray) -> np.ndarray:
    return np.where(phase < 0.25, 1.0, -1.0).astype(float)

def wave_pulse25_bl(phase: np.ndarray, max_harmonic: int = None) -> np.ndarray:
    """Band-limited 25% pulse via FFT."""
    return fft_bandlimit(wave_pulse25_raw(phase), max_harmonic)

def wave_ramp_raw(phase: np.ndarray) -> np.ndarray:
    return (1.0 - 2.0 * phase).astype(np.float64)

def wave_ramp_bl(phase: np.ndarray, max_harmonic: int = None) -> np.ndarray:
    """Band-limited ramp via FFT."""
    return fft_bandlimit(wave_ramp_raw(phase), max_harmonic)

def wave_fm(phase: np.ndarray, mod_ratio: float = 2.0, mod_index: float = 3.0) -> np.ndarray:
    return np.sin(2.0 * math.pi * phase + mod_index * np.sin(2.0 * math.pi * mod_ratio * phase))

def wave_additive(phase: np.ndarray, harmonics: list) -> np.ndarray:
    """harmonics: list of (amplitude, harmonic_number) tuples."""
    out = np.zeros_like(phase)
    for amp, h in harmonics:
        out += amp * np.sin(2.0 * math.pi * h * phase)
    total = sum(abs(a) for a, _ in harmonics) or 1.0
    return out / total

# Convenience map (band-limited versions used by default)
WAVE_FNS = {
    "sine":     wave_sine,
    "saw":      wave_saw_bl,
    "square":   wave_square_bl,
    "triangle": wave_triangle,
    "pulse25":  wave_pulse25_bl,
    "ramp":     wave_ramp_bl,
}

# ─── Preset bank definitions ────────────────────────────────────────────────

def make_preset(name: str, spw: int, tables: int, max_harmonic: int = None) -> np.ndarray:
    """Return float32 array of shape (tables, spw) for the named preset."""
    phase = np.linspace(0.0, 1.0, spw, endpoint=False)

    # Helper: generate and FFT band-limit a raw wave in one call
    def bl(wave_fn, *a, **kw):
        raw = wave_fn(phase, *a, **kw)
        return fft_bandlimit(raw, max_harmonic)

    if name == "sine_to_saw":
        bank = []
        for t in range(tables):
            alpha = t / max(tables - 1, 1)
            w = (1.0 - alpha) * wave_sine(phase) + alpha * bl(wave_saw_raw)
            bank.append(w)

    elif name == "sine_to_square":
        bank = []
        for t in range(tables):
            alpha = t / max(tables - 1, 1)
            w = (1.0 - alpha) * wave_sine(phase) + alpha * bl(wave_square_raw)
            bank.append(w)

    elif name == "tri_to_saw":
        bank = []
        for t in range(tables):
            alpha = t / max(tables - 1, 1)
            w = (1.0 - alpha) * wave_triangle(phase) + alpha * bl(wave_saw_raw)
            bank.append(w)

    elif name == "additive_sweep":
        # Sine → rich harmonic series (naturally limited by harmonic count)
        bank = []
        for t in range(tables):
            alpha = t / max(tables - 1, 1)
            n_harm = max(1, int(1 + alpha * (spw // 4)))
            if max_harmonic is not None:
                n_harm = min(n_harm, max_harmonic)
            harms = [(1.0 / k, k) for k in range(1, n_harm + 1)]
            bank.append(wave_additive(phase, harms))

    elif name == "fm_sweep":
        # FM index sweep: 0 → 10  (then FFT band-limit each table)
        bank = []
        for t in range(tables):
            idx = t / max(tables - 1, 1) * 10.0
            raw = wave_fm(phase, mod_ratio=2.0, mod_index=idx)
            bank.append(fft_bandlimit(raw, max_harmonic))

    elif name == "all_waves":
        # Cycles through: sine → saw → square → triangle → ramp → pulse25 (all FFT band-limited)
        segs = ["sine", "saw", "square", "triangle", "ramp", "pulse25"]
        bank = []
        for t in range(tables):
            pos  = t / max(tables - 1, 1) * (len(segs) - 1)
            i0   = int(pos)
            i1   = min(i0 + 1, len(segs) - 1)
            frac = pos - i0
            w0   = WAVE_FNS[segs[i0]](phase)
            w1   = WAVE_FNS[segs[i1]](phase)
            bank.append((1.0 - frac) * w0 + frac * w1)

    elif name == "wavepack":
        # Same as all_waves but with more tables spread across the same waves
        return make_preset("all_waves", spw, tables, max_harmonic)

    else:
        raise ValueError(f"Unknown preset: {name}")

    return np.array(bank, dtype=np.float32)


# ─── Normalise ────────────────────────────────────────────────────────────────

def normalize_bank(bank: np.ndarray) -> np.ndarray:
    """Peak-normalise to ±1 across the whole bank."""
    peak = np.max(np.abs(bank))
    if peak > 1e-6:
        bank = bank / peak
    return bank.astype(np.float32)


# ─── WAV writer (no scipy dependency) ────────────────────────────────────────

def write_wav_f32(path: str, samples: np.ndarray, sample_rate: int = 44100):
    """Write IEEE float-32 WAV."""
    n = len(samples)
    byte_rate  = sample_rate * 4
    block_align = 4
    data_size  = n * 4
    with open(path, "wb") as f:
        # RIFF header
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + data_size))
        f.write(b"WAVE")
        # fmt  chunk — PCM float (type 3)
        f.write(b"fmt ")
        f.write(struct.pack("<I", 18))        # chunk size
        f.write(struct.pack("<H", 3))         # IEEE float
        f.write(struct.pack("<H", 1))         # mono
        f.write(struct.pack("<I", sample_rate))
        f.write(struct.pack("<I", byte_rate))
        f.write(struct.pack("<H", block_align))
        f.write(struct.pack("<H", 32))        # bits per sample
        f.write(struct.pack("<H", 0))         # cbSize = 0
        # data chunk
        f.write(b"data")
        f.write(struct.pack("<I", data_size))
        f.write(samples.astype("<f4").tobytes())


# ─── Plot ────────────────────────────────────────────────────────────────────

def plot_bank(bank: np.ndarray, title: str, morph_pos: float = None):
    if not MPL_OK:
        print("matplotlib not available — skipping plot")
        return

    n_tables, spw = bank.shape
    phase = np.linspace(0.0, 1.0, spw, endpoint=False)

    fig, ax = plt.subplots(figsize=(12, 5))
    fig.patch.set_facecolor("#0a0e16")
    ax.set_facecolor("#0a0e16")

    cmap = plt.get_cmap("plasma")
    for t in range(n_tables):
        if morph_pos is not None:
            dist  = abs(morph_pos - t)
            alpha = max(0.05, 1.0 - dist / max(n_tables - 1, 1))
        else:
            alpha = 0.15 + 0.85 * (t / max(n_tables - 1, 1))

        col   = list(cmap(t / max(n_tables - 1, 1)))
        col[3] = alpha
        lw    = 2.2 if (morph_pos is not None and abs(morph_pos - t) < 0.5) else 0.9
        ax.plot(phase, bank[t], color=col, linewidth=lw)

    # Draw the current morphed wave on top
    if morph_pos is not None:
        t0   = int(morph_pos)
        t1   = min(t0 + 1, n_tables - 1)
        frac = morph_pos - t0
        morphed = bank[t0] * (1.0 - frac) + bank[t1] * frac
        ax.plot(phase, morphed, color="#7fffb0", linewidth=2.8, label=f"morph={morph_pos:.2f}")
        ax.legend(facecolor="#1a1e2e", labelcolor="white")

    ax.set_xlim(0, 1)
    ax.set_ylim(-1.15, 1.15)
    ax.set_xlabel("Phase", color="#8899bb")
    ax.set_ylabel("Amplitude", color="#8899bb")
    ax.set_title(title, color="#aabbdd", fontsize=13)
    ax.tick_params(colors="#556677")
    for spine in ax.spines.values():
        spine.set_edgecolor("#2a3050")

    plt.tight_layout()
    plt.show()


def plot_spectrum(bank: np.ndarray, title: str, morph_pos: float = None):
    """Plot harmonic spectrum (magnitude) for each table in the bank."""
    if not MPL_OK:
        print("matplotlib not available — skipping spectrum plot")
        return

    n_tables, spw = bank.shape
    freqs = np.arange(spw // 2 + 1)

    fig, ax = plt.subplots(figsize=(12, 5))
    fig.patch.set_facecolor("#0a0e16")
    ax.set_facecolor("#0a0e16")

    cmap = plt.get_cmap("plasma")

    for t in range(n_tables):
        spectrum = np.abs(np.fft.rfft(bank[t])) / (spw / 2)
        spectrum[0] = 0.0  # suppress DC
        is_cur = (morph_pos is not None and abs(morph_pos - t) < 0.5)
        alpha = 0.7 if is_cur else (0.15 + 0.5 * (t / max(n_tables - 1, 1)))
        col   = list(cmap(t / max(n_tables - 1, 1)))
        col[3] = alpha
        lw    = 2.0 if is_cur else 0.7
        ax.plot(freqs[1:spw // 4], spectrum[1:spw // 4], color=col, linewidth=lw)

    ax.set_xlim(1, spw // 4)
    ax.set_xlabel("Harmonic", color="#8899bb")
    ax.set_ylabel("Magnitude", color="#8899bb")
    ax.set_title(f"{title} — spectrum", color="#aabbdd", fontsize=13)
    ax.tick_params(colors="#556677")
    for spine in ax.spines.values():
        spine.set_edgecolor("#2a3050")

    plt.tight_layout()
    plt.show()


# ─── CLI ────────────────────────────────────────────────────────────────────

PRESETS = ["sine_to_saw", "sine_to_square", "tri_to_saw",
           "additive_sweep", "fm_sweep", "all_waves", "wavepack"]

def main():
    parser = argparse.ArgumentParser(
        description="Generate multi-table wavetable bank WAV files for ObsSynth4D")
    parser.add_argument("--preset", default="sine_to_saw",
                        help=f"Bank preset name (default: sine_to_saw). "
                             f"Available: {', '.join(PRESETS)}")
    parser.add_argument("--tables", type=int, default=16,
                        help="Number of tables in the bank (default: 16)")
    parser.add_argument("--spw", type=int, default=2048,
                        help="Samples per wave table (default: 2048)")
    parser.add_argument("--rate", type=int, default=44100,
                        help="Sample rate in Hz (default: 44100)")
    parser.add_argument("--out", default=None,
                        help="Output directory (default: pcm/ in workspace root)")
    parser.add_argument("--name", default=None,
                        help="Output filename stem (default: <preset>_<tables>t_<spw>spw)")
    parser.add_argument("--plot", action="store_true",
                        help="Show waveform overlay plot after generation")
    parser.add_argument("--morph", type=float, default=None,
                        help="Morph position to highlight in plot (0 .. tables-1)")
    parser.add_argument("--list", action="store_true",
                        help="List available presets and exit")
    parser.add_argument("--no-normalize", action="store_true",
                        help="Skip peak normalisation")
    parser.add_argument("--max-harmonic", type=int, default=None,
                        help="FFT band-limit: keep harmonics 1..N only (default: SPW/2 = Nyquist)")
    parser.add_argument("--plot-spectrum", action="store_true",
                        help="Show harmonic spectrum plot after generation")
    args = parser.parse_args()

    if args.list:
        print("Available presets:")
        for p in PRESETS:
            print(f"  {p}")
        return

    # Output directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir   = os.path.dirname(script_dir)
    out_dir    = args.out or os.path.join(root_dir, "pcm")
    os.makedirs(out_dir, exist_ok=True)

    # Filename — matches existing pattern: bank_<name>_<N>waves_<spw>samples_32float.wav
    stem     = args.name or f"bank_{args.preset}_{args.tables}waves_{args.spw}samples_32float"
    out_path = os.path.join(out_dir, stem + ".wav")

    print(f"Generating '{args.preset}' bank: {args.tables} tables × {args.spw} spw ...")

    try:
        bank = make_preset(args.preset, args.spw, args.tables, args.max_harmonic)
    except ValueError as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    if not args.no_normalize:
        bank = normalize_bank(bank)

    # Flatten: tables concatenated → (tables * spw,) float32
    flat = bank.reshape(-1).astype(np.float32)

    if SCIPY_OK:
        scipy_wavfile.write(out_path, args.rate, flat)
    else:
        write_wav_f32(out_path, flat, args.rate)

    size_kb = os.path.getsize(out_path) / 1024.0
    print(f"  Saved: {out_path}  ({size_kb:.1f} KB)")
    print(f"  Total samples : {len(flat)}")
    print(f"  Tables        : {args.tables}")
    print(f"  SPW           : {args.spw}")

    if args.plot:
        morph = args.morph if args.morph is not None else (args.tables - 1) * 0.5
        plot_bank(bank, f"{args.preset} — {args.tables} tables", morph_pos=morph)

    if args.plot_spectrum:
        morph = args.morph if args.morph is not None else (args.tables - 1) * 0.5
        plot_spectrum(bank, f"{args.preset} — {args.tables} tables", morph_pos=morph)


if __name__ == "__main__":
    main()
