# LFO Module Documentation

## Overview

The LFO (Low Frequency Oscillator) module provides low-frequency modulation capabilities for the synthesizer. It generates control signals that can modulate various parameters of the audio oscillators.

## Features

- **5 Waveform Types**: Sine, Triangle, Square, Sawtooth, Random
- **Frequency Range**: 0.01 Hz to 20 Hz (typical LFO range)
- **Amplitude Control**: 0% to 100%
- **Phase Reset**: Ability to reset the LFO phase to zero
- **Real-time Control**: Interactive keyboard controls for all parameters

## Integration with test_midi.cpp

The LFO has been integrated into the `test_midi.cpp` example program with the following keyboard controls:

### Keyboard Controls

| Key | Function |
|-----|----------|
| `l` | Toggle LFO on/off |
| `f` | Increase LFO frequency (×1.5) |
| `F` | Decrease LFO frequency (÷1.5) |
| `w` | Cycle through LFO waveforms |
| `a` | Increase LFO amplitude (+10%) |
| `A` | Decrease LFO amplitude (-10%) |
| `r` | Reset LFO phase |

### Default Settings

- **Initial State**: OFF (amplitude = 0%)
- **Initial Frequency**: 2.0 Hz
- **Initial Waveform**: Sine
- **Target**: Frequency modulation of main oscillator

## Technical Implementation

### LFO Class Structure

```cpp
namespace synth {
    enum class LFOWaveType {
        SINE, TRIANGLE, SQUARE, SAW, RANDOM
    };
    
    class LFO {
        // Member functions for parameter control
        void setFrequency(float frequency);
        void setWaveType(LFOWaveType wave_type);
        void setAmplitude(float amplitude);
        void resetPhase();
        float getSample();
    };
}
```

### Integration Points

1. **Audio Callback**: LFO modulation is applied once per audio buffer
2. **Frequency Modulation**: Uses existing `setFrequencyModulation()` method
3. **Thread Safety**: LFO operates in the audio thread context

## Usage Example

```cpp
// Create LFO
auto lfo = std::make_unique<LFO>(2.0f, LFOWaveType::SINE, 44100.0f);

// Configure LFO
lfo->setAmplitude(0.5f);     // 50% modulation depth
lfo->setFrequency(1.0f);     // 1 Hz frequency

// In audio callback
float modulation = lfo->getSample();
oscillator->setFrequencyModulation(modulation);
```

## Design Considerations

- **Float Precision**: Consistent with project standards using `float` types
- **Naming Convention**: Private members suffixed with underscore (`_`)
- **Frequency Clamping**: LFO frequency is automatically clamped to valid range
- **Phase Wrapping**: Proper phase management to prevent overflow
- **Random Generator**: Thread-safe random number generation for random waveform

## Performance Notes

- LFO generates one sample per audio buffer (not per audio sample)
- Minimal CPU overhead with optimized waveform generators
- Memory-efficient with static wave generator functions
- Compatible with existing oscillator frequency modulation system