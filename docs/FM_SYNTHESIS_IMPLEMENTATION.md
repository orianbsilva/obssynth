# FM Synthesis Implementation

## Overview

This document describes the implementation of FM (Frequency Modulation) synthesis in the synth oscillator system. FM synthesis is a powerful digital audio synthesis technique that creates complex harmonic content by using one oscillator (modulator) to modulate the frequency of another oscillator (carrier).

## Implementation Details

### Core Algorithm

The FM synthesis implementation uses a classic 2-operator FM algorithm:

```
Carrier = sin(carrier_phase + modulation_index * sin(modulator_phase))
```

Where:
- `carrier_phase`: Phase of the main carrier oscillator
- `modulator_phase`: Phase of the modulating oscillator  
- `modulation_index`: Controls the depth of modulation (typically 0-20)

### Key Components

#### 1. WaveType Enum Extension
Added `FM_SYNTHESIS` to the `WaveType` enum in `oscillator.hpp`:
```cpp
enum class WaveType
{
    SINE,
    SAW,
    SQUARE,
    TRIANGLE,
    PULSE,
    NOISE,
    RAMP_UP,
    RAMP_DOWN,
    PCM_CUSTOM,
    FM_SYNTHESIS  // New FM synthesis wave type
};
```

#### 2. FM Parameters
Added new private member variables to track FM state:
```cpp
// FM Synthesis parameters
float fm_modulator_frequency_ratio_;  // Ratio of modulator to carrier frequency
float fm_modulation_index_;           // Modulation depth
float fm_modulator_phase_;            // Current modulator phase
float fm_modulator_phase_increment_;  // Modulator phase increment per sample
```

#### 3. FM Control Methods
Public interface for controlling FM parameters:
```cpp
void setFMModulatorRatio(float ratio);     // Set frequency ratio (e.g., 2.0 = octave above)
void setFMModulationIndex(float index);    // Set modulation depth (0-20 typical range)
float getFMModulatorRatio() const;         // Get current frequency ratio
float getFMModulationIndex() const;        // Get current modulation index
void resetFMModulatorPhase();              // Reset modulator phase to 0
```

#### 4. Wave Generator Function
The core FM synthesis algorithm implemented in `fm_synthesis_wave()`:
```cpp
float fm_synthesis_wave(float carrier_phase) {
    // Calculate modulator output
    float modulator_output = math::sin(fm_modulator_phase_);
    
    // Apply modulation to carrier phase
    float modulated_phase = carrier_phase + (fm_modulation_index_ * modulator_output);
    
    // Generate carrier wave with modulated phase
    float carrier_output = math::sin(modulated_phase);
    
    // Update modulator phase for next sample
    fm_modulator_phase_ += fm_modulator_phase_increment_;
    
    // Keep phase in range [0, 2π]
    if (fm_modulator_phase_ >= math::TWO_PI) {
        fm_modulator_phase_ -= math::TWO_PI;
    }
    
    return carrier_output;
}
```

## Usage Examples

### Basic FM Synthesis
```cpp
// Create FM oscillator at 440Hz
Oscillator osc(440.0f, WaveType::FM_SYNTHESIS, 48000.0f);

// Configure FM parameters
osc.setFMModulatorRatio(2.0f);    // Modulator 2x carrier frequency
osc.setFMModulationIndex(5.0f);   // Moderate modulation depth

// Generate samples
float sample = osc.getSample();
```

### Parameter Ranges and Effects

#### Modulator Frequency Ratio
- **1.0**: Modulator same frequency as carrier (creates vibrato-like effect)
- **2.0, 3.0, 4.0**: Harmonic ratios create rich, musical timbres
- **1.414, 2.718**: Non-harmonic ratios create complex, metallic sounds
- **0.5, 0.25**: Sub-harmonic ratios create darker, thicker tones

#### Modulation Index
- **0.0**: No modulation (pure sine wave)
- **1.0-3.0**: Subtle harmonic enrichment
- **5.0-10.0**: Rich harmonic content
- **15.0+**: Very complex, aggressive timbres

## Testing and Validation

### Test Programs

#### 1. `test_all_waveshapes.exe`
Includes FM synthesis in the comprehensive waveform test suite.

#### 2. `test_fm_synthesis.exe`
Dedicated FM synthesis test that generates multiple examples:
- `carrier_only_fm.wav`: No modulation (pure sine)
- `ratio1_mod2_fm.wav`: Gentle FM effect
- `ratio1_mod8_fm.wav`: Aggressive FM
- `ratio2_mod5_fm.wav`: Harmonic modulator
- `ratio1414_mod10_fm.wav`: Non-harmonic metallic sounds
- `ratio35_mod15_fm.wav`: Very complex spectrum
- `ratio05_mod3_fm.wav`: Sub-harmonic dark tones

### Build and Test
```bash
# Build the project
.\build_cmake.bat

# Run FM synthesis test
.\build\bin\Release\test_fm_synthesis.exe

# Run all waveforms test (includes FM)
.\build\bin\Release\test_all_waveshapes.exe
```

## Integration Notes

### Automatic Parameter Updates
The implementation automatically updates modulator phase increments when:
- Base frequency changes (`setFrequency()`)
- Frequency modulation changes (`setFrequencyModulation()`)
- FM modulator ratio changes (`setFMModulatorRatio()`)

### Performance Considerations
- Uses single-precision floating point throughout (`float`)
- Efficient phase accumulation and wrapping
- Minimal computational overhead (2 sin() calls per sample)
- Compatible with existing oscillator architecture

### Thread Safety
FM synthesis follows the same thread safety model as other oscillator types:
- Safe for single-threaded use
- Parameter changes should be synchronized in multi-threaded environments

## Mathematical Background

FM synthesis generates harmonic content through phase modulation. The resulting spectrum contains:

1. **Carrier frequency** (original tone)
2. **Upper sidebands** at `carrier + n*modulator` frequencies
3. **Lower sidebands** at `carrier - n*modulator` frequencies

The amplitude of each sideband is determined by Bessel functions of the modulation index, creating complex but predictable harmonic relationships.

## Future Enhancements

Potential improvements for the FM synthesis implementation:

1. **Multi-operator FM**: Support for 4-operator and 6-operator algorithms
2. **Envelope control**: Independent envelopes for carrier and modulator
3. **Feedback**: Self-modulation capabilities
4. **Waveform selection**: Non-sine modulator waveforms
5. **Real-time parameter interpolation**: Smooth parameter changes during playback

## References

- Chowning, John. "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation." Journal of the Audio Engineering Society, 1973.
- Yamaha DX7 Technical Manual (classic FM synthesis implementation)
- Roads, Curtis. "The Computer Music Tutorial." MIT Press, 1996.