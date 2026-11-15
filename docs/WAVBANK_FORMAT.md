# WAV Bank Format

## Overview

A **WAV Bank** is a single-file multi-table wavetable format used by the
`PCMWaveform` / `WaveTableOsc` subsystem in ObsSynth4D.  
Each bank holds N identical-length single-cycle waveforms concatenated
sequentially in one IEEE float-32 mono WAV file.  
The `WaveTableOsc` interpolates smoothly between adjacent tables using a
**morph position** (0 → N-1).

---

## File Layout

```
┌─────────────────────────────────────────────┐
│  Standard WAV header (RIFF/WAVE/fmt /data)  │
│  Audio format : IEEE float-32 (type 0x0003) │
│  Channels     : 1 (mono)                    │
│  Sample rate  : 44100 Hz (recommended)      │
├─────────────────────────────────────────────┤
│  Table 0  [0 .. SPW-1]                      │  ← samples_per_wave samples
│  Table 1  [SPW .. 2·SPW-1]                  │
│  …                                          │
│  Table N-1  [(N-1)·SPW .. N·SPW-1]          │
└─────────────────────────────────────────────┘

Total samples = N × SPW
Total bytes   = N × SPW × 4  (+ WAV header ~44–58 bytes)
```

Every table **must** be the same length (`SPW` samples).  
Tables are not padded or aligned beyond sample boundaries.

---

## Filename Convention

```
bank_<name>_<N>waves_<SPW>samples_32float.wav
```

| Field         | Description                                  | Example          |
|---------------|----------------------------------------------|------------------|
| `bank_`       | Fixed prefix — triggers bank auto-detection  | `bank_`          |
| `<name>`      | Descriptive slug (letters, digits, `_`)      | `sine_to_saw`    |
| `<N>waves_`   | Number of tables in the bank                 | `16waves_`       |
| `<SPW>samples_` | Samples per single-cycle table             | `1024samples_`   |
| `32float`     | Bit-depth/format tag                         | `32float`        |

**Examples:**
```
bank_basic_8waves_1024samples_32float.wav      →   8 tables, 1024 spw
bank_harmonic_16waves_1024samples_32float.wav  →  16 tables, 1024 spw
bank_morph_32waves_1024samples_32float.wav     →  32 tables, 1024 spw
bank_sine_to_saw_16waves_2048samples_32float.wav → 16 tables, 2048 spw
```

The loader (`PCMWaveform::parseBankFilename`) extracts **N** and **SPW**
directly from the filename — no sidecar metadata file is needed.

---

## Auto-Detection Logic

`PCMWaveform::loadWavBank()` and `loadFromFile()` both call the internal
`parseBankFilename()` helper at load time:

```
Input: "bank_sine_to_saw_16waves_1024samples_32float.wav"
                                 ↑                ↑
                           → N = 16         → SPW = 1024
```

The parser scans backwards from the keyword boundary:
- `"waves_"` → extract decimal digits immediately before it → N
- `"samples_"` → extract decimal digits immediately before it → SPW

If parsing fails (non-standard filename), the loader falls back to:
- `SPW = 1024` (default)
- `N   = total_samples / SPW`

`loadWavBank()` accepts optional override parameters `(wave_count, samples_per_wave)`;
passing `0` for either uses the auto-detected value.

---

## Supported SPW Values

| SPW    | Latency at 44100 Hz | Recommended use            |
|--------|---------------------|----------------------------|
| 256    | 5.8 ms              | Low-memory, short banks    |
| 512    | 11.6 ms             | General purpose            |
| **1024** | **23.2 ms**       | **Default / recommended**  |
| **2048** | **46.4 ms**       | **High-quality morphing**  |
| 4096   | 92.9 ms             | Very high quality          |

All values must be a power of two (not enforced by the loader, but required
for correct phase interpolation).

---

## C++ API

```cpp
// Load a bank — parameters auto-detected from filename
bool PCMWaveform::loadWavBank(path, wave_count = 0, samples_per_wave = 0);

// Via WaveTableOsc (preferred for OSC panel use)
bool WaveTableOsc::loadBank(path, samplesPerWave = 0);

// Via OscChain
bool OscChain::loadWtBank(path, samplesPerWave = 0);

// Via ObsSynth4D (applies to all voices)
void ObsSynth4D::loadWtBank(osc, path, samplesPerWave = 0);

// Set morph position (0 = table 0, N-1 = last table)
void WaveTableOsc::setMorphPosition(float pos);  // clamped automatically

// Inspect loaded bank
int    WaveTableOsc::tableCount();
bool   WaveTableOsc::isLoaded();
float  WaveTableOsc::getMorphPosition();

// Raw data access (for UI / visualisation)
const PCMWaveform *WaveTableOsc::getWaveform();   // read-only
size_t PCMWaveform::getBankWaveCount();
size_t PCMWaveform::getSamplesPerWave();
const std::vector<float> &PCMWaveform::getData(); // interleaved tables
```

---

## Generating Bank Files

Use `util/gen_wavbank.py`:

```bash
# List available presets
python util/gen_wavbank.py --list

# Generate sine→saw morph bank (16 tables, 2048 spw)
python util/gen_wavbank.py --preset sine_to_saw --tables 16 --spw 2048

# Generate FM index sweep (32 tables) and show overlay plot
python util/gen_wavbank.py --preset fm_sweep --tables 32 --plot

# All options
python util/gen_wavbank.py --help
```

Output is saved to `pcm/` by default.

### Available Presets

| Preset           | Description                                         |
|------------------|-----------------------------------------------------|
| `sine_to_saw`    | Sine → band-limited sawtooth crossfade              |
| `sine_to_square` | Sine → band-limited square crossfade                |
| `tri_to_saw`     | Triangle → band-limited sawtooth                   |
| `additive_sweep` | Increasing harmonic content (1 → 16 harmonics)     |
| `fm_sweep`       | FM modulation index sweep (0 → 10)                 |
| `all_waves`      | Cycles through all 6 basic waveforms               |

---

## Morphing Algorithm

Interpolation is performed inside `PCMWaveform::getSample()`:

```
scaled_pos = (bank_position / 2.0) × (N − 1)
table_A    = floor(scaled_pos)
table_B    = table_A + 1
frac       = scaled_pos − table_A

for each sample:
    s_A = linear_interp(table_A[i], table_A[i+1], phase_frac)
    s_B = linear_interp(table_B[i], table_B[i+1], phase_frac)
    out = linear_interp(s_A, s_B, frac)
```

The `/2.0` factor in `scaled_pos` is a legacy quirk of the
`setBankPosition()` interface (position 0..1 maps to indices 0..N-1 via the
formula). `WaveTableOsc::setMorphPosition(pos)` compensates automatically:

```cpp
waveform_->setBankPosition(2.0f * morph_pos_ / maxPos);
```

---

## UI Visualisation

When an OSC slot is in **WT mode**, the oscillator panel replaces the
standard waveform scope with the **bank overlay viewer**:

- All N tables drawn in green with opacity fading by distance from the current morph position
- The cross-faded "live" wave drawn on top in bright white-green
- Circular table markers along the bottom edge (active = lit)
- Table count label in the top-left corner

The viewer reads data directly from `PCMWaveform::getData()` and
`getSamplesPerWave()` — zero allocations per frame.
