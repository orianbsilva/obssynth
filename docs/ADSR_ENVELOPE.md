# ADSR Envelope System Documentation

## Overview

The ADSR (Attack, Decay, Sustain, Release) envelope system provides professional-grade amplitude envelope control for oscillators. This system allows for expressive and dynamic sound shaping through four distinct phases of amplitude modulation.

## Components

### 1. **ADSREnvelope Class** (`adsr_envelope.hpp/.cpp`)
Core envelope generator with precise timing control.

### 2. **EnvelopedOscillator Class** (`enveloped_oscillator.hpp/.cpp`)
Combines an oscillator with an ADSR envelope for complete sound control.

### 3. **PolyphonicADSROscillator Class** (`polyphonic_adsr_oscillator.hpp/.cpp`)
8-voice polyphonic synthesizer with individual ADSR envelopes per voice.

## ADSR Envelope Phases

```
Amplitude
    ▲
1.0 |    ╱╲
    |   ╱  ╲
0.7 |  ╱    ╲______ Sustain Level
    | ╱            ╲
0.0 |╱              ╲____
    +─────────────────────→ Time
     A   D    S       R
```

### **Attack (A)**
- **Duration**: Time to rise from 0 to peak amplitude (1.0)
- **Range**: 0.001s to 1.0s
- **Effect**: Controls how quickly notes "attack" when triggered
- **Musical Examples**:
  - Fast (0.001s): Piano, percussion, plucked strings
  - Slow (0.5s): Strings with bow, organ swells, pads

### **Decay (D)**
- **Duration**: Time to fall from peak to sustain level
- **Range**: 0.001s to 1.0s  
- **Effect**: Controls the initial brightness/punch after attack
- **Musical Examples**:
  - Fast (0.05s): Punchy bass, bright attacks
  - Slow (0.3s): Smooth transitions, brass instruments

### **Sustain (S)**
- **Level**: Amplitude level maintained while note is held
- **Range**: 0.0 to 1.0 (0% to 100%)
- **Effect**: Determines the "body" volume of sustained notes
- **Musical Examples**:
  - High (0.8-1.0): Organ, brass, sustained strings
  - Low (0.1-0.3): Percussive sounds, plucked instruments
  - Zero (0.0): Pure percussion/transient sounds

### **Release (R)**
- **Duration**: Time to fall from current level to 0 after note off
- **Range**: 0.001s to 2.0s
- **Effect**: Controls how notes fade when released
- **Musical Examples**:
  - Fast (0.1s): Staccato, percussion, organ
  - Slow (1.0s+): Ambient pads, reverb tails, strings

## Basic Usage

### Simple Enveloped Oscillator

```cpp
#include "src/oscillators/enveloped_oscillator.hpp"
using namespace synth;

// Create enveloped oscillator
auto osc = std::make_unique<EnvelopedOscillator>(440.0f, WaveType::SINE, 44100.0f);

// Set ADSR parameters (attack, decay, sustain, release)
osc->setADSR(0.01f, 0.1f, 0.7f, 0.3f);

// Play a note
osc->noteOn();
// ... generate audio samples
osc->noteOff();
```

### Audio Callback Integration

```cpp
// Audio generation callback
void audioCallback(float *buffer, int bufferSize, int *out_buffer_size)
{
    if (enveloped_oscillator)
    {
        enveloped_oscillator->generateBuffer(buffer, bufferSize);
        *out_buffer_size = bufferSize;
    }
}
```

### Polyphonic ADSR Synthesizer

```cpp
#include "src/oscillators/polyphonic_adsr_oscillator.hpp"

// Create 8-voice polyphonic synthesizer
auto poly_synth = std::make_unique<PolyphonicADSROscillator>(WaveType::SINE, 44100.0f);

// Set global ADSR for all voices
poly_synth->setADSR(0.05f, 0.2f, 0.6f, 0.4f);

// Play notes
poly_synth->noteOn(60, 0.8f);  // C4
poly_synth->noteOn(64, 0.8f);  // E4  
poly_synth->noteOn(67, 0.8f);  // G4 (C major chord)

// Release notes
poly_synth->noteOff(60);
poly_synth->allNotesOff();     // Release all
```

## API Reference

### ADSREnvelope Class

#### Constructor
```cpp
ADSREnvelope(float sample_rate)
```

#### Parameter Control
```cpp
void setADSR(float attack, float decay, float sustain, float release);
void setAttackTime(float attack_seconds);
void setDecayTime(float decay_seconds);
void setSustainLevel(float sustain_level);    // 0.0 to 1.0
void setReleaseTime(float release_seconds);
```

#### Envelope Control
```cpp
void noteOn();              // Start attack phase
void noteOff();             // Start release phase
void reset();               // Reset to idle state
```

#### Audio Processing
```cpp
float getNextSample();      // Get amplitude and advance envelope
float getCurrentAmplitude() const;  // Get current amplitude
```

#### State Queries
```cpp
bool isActive() const;      // True if not idle
bool isInRelease() const;   // True if in release phase
EnvelopeState getState() const;  // Get current phase
```

### EnvelopedOscillator Class

#### Constructor
```cpp
EnvelopedOscillator(float frequency, WaveType wave_type, float sample_rate)
```

#### Combined Control
```cpp
void noteOn();              // Start oscillator and envelope
void noteOff();             // Start envelope release
void enableEnvelope(bool enable);  // Toggle envelope on/off
```

#### ADSR Control
```cpp
void setADSR(float attack, float decay, float sustain, float release);
// ... all individual parameter methods
```

#### Audio Generation
```cpp
float getSample();          // Get enveloped sample
void generateBuffer(float* buffer, int size);
```

### PolyphonicADSROscillator Class

#### Voice Management
```cpp
void noteOn(uint8_t note_number, float velocity = 1.0f);
void noteOff(uint8_t note_number);
void allNotesOff();
```

#### Global ADSR Control
```cpp
void setADSR(float attack, float decay, float sustain, float release);
void setAttackTime(float attack_seconds);
// ... other parameter methods
```

#### Voice Status
```cpp
int getActiveVoiceCount() const;
int getReleasingVoiceCount() const;
bool isNoteActive(uint8_t note_number) const;
```

## Preset Examples

### Musical Instrument Presets

#### Piano
```cpp
envelope->setADSR(0.001f, 0.3f, 0.0f, 0.2f);
// Fast attack, medium decay, no sustain, short release
```

#### Organ
```cpp
envelope->setADSR(0.0f, 0.0f, 1.0f, 0.05f);
// Instant attack, no decay, full sustain, quick release
```

#### Strings (Violin/Cello)
```cpp
envelope->setADSR(0.1f, 0.2f, 0.8f, 0.5f);
// Slow attack, quick decay, high sustain, medium release
```

#### Brass (Trumpet/Horn)
```cpp
envelope->setADSR(0.05f, 0.1f, 0.9f, 0.3f);
// Medium attack, quick decay, very high sustain, medium release
```

#### Ambient Pad
```cpp
envelope->setADSR(0.5f, 0.3f, 0.6f, 1.5f);
// Very slow attack, slow decay, medium sustain, long release
```

#### Percussion (Drum/Strike)
```cpp
envelope->setADSR(0.001f, 0.2f, 0.0f, 0.1f);
// Instant attack, quick decay, no sustain, short release
```

#### Plucked (Guitar/Harp)
```cpp
envelope->setADSR(0.002f, 0.4f, 0.3f, 0.8f);
// Very fast attack, medium decay, low sustain, long release
```

## Advanced Features

### Voice-Specific ADSR Control
```cpp
// Set different ADSR for individual voices (advanced)
poly_synth->setVoiceADSR(0, 0.01f, 0.1f, 0.8f, 0.3f);  // Voice 0
poly_synth->setVoiceADSR(1, 0.05f, 0.2f, 0.6f, 0.5f);  // Voice 1
```

### Envelope State Monitoring
```cpp
// Monitor envelope phases for visualization or control
EnvelopeState state = envelope->getState();
switch (state) {
    case EnvelopeState::ATTACK:
        // Handle attack phase
        break;
    case EnvelopeState::DECAY:
        // Handle decay phase
        break;
    // ... etc
}
```

### Real-time Parameter Modulation
```cpp
// Dynamically adjust ADSR during playback
float current_release = poly_synth->getReleaseTime();
poly_synth->setReleaseTime(current_release * 1.5f);  // Extend release
```

## Performance Considerations

### Optimization Tips
- **Voice Management**: The system automatically manages voice allocation and cleanup
- **Sample-Accurate Timing**: All envelope calculations are sample-accurate
- **Thread Safety**: Voice operations are protected with mutexes for real-time audio
- **Memory Efficiency**: Uses linear interpolation for smooth envelope curves
- **CPU Usage**: Optimized for real-time performance with minimal overhead

### Voice Stealing Strategy
1. **Free Voice**: First tries to find an unused voice
2. **Release Phase**: Prefers voices already in release phase
3. **Oldest Voice**: Falls back to stealing the oldest active voice

## Troubleshooting

### Common Issues

#### **Clicks/Pops in Audio**
- **Cause**: Abrupt amplitude changes
- **Solution**: Ensure attack/release times are not zero for smooth transitions

#### **No Sound Output**
- **Cause**: Envelope may be disabled or parameters too extreme
- **Solution**: Check `enableEnvelope(true)` and reasonable ADSR values

#### **Notes Don't Release**
- **Cause**: Missing `noteOff()` calls or infinite sustain
- **Solution**: Ensure proper note off handling and finite release times

#### **Voice Stealing Issues**
- **Cause**: All 8 voices occupied
- **Solution**: Monitor with `getActiveVoiceCount()` and use `allNotesOff()` if needed

### Debug Information
```cpp
// Check envelope status
std::cout << "Active voices: " << poly_synth->getActiveVoiceCount() << "/8\n";
std::cout << "Releasing voices: " << poly_synth->getReleasingVoiceCount() << "\n";
std::cout << "Current ADSR: A=" << poly_synth->getAttackTime() 
          << "s, D=" << poly_synth->getDecayTime() << "s\n";
```

## Integration Examples

### MIDI Integration
```cpp
void midiNoteCallback(const MidiNote &note)
{
    if (note.is_note_on && note.velocity > 0)
    {
        float velocity = static_cast<float>(note.velocity) / 127.0f;
        poly_adsr_synth->noteOn(note.note, velocity);
    }
    else
    {
        poly_adsr_synth->noteOff(note.note);
    }
}
```

### Real-time Parameter Control
```cpp
// Interactive ADSR control (example from test_adsr_demo.cpp)
switch (key) {
    case 'z': // Decrease attack
        current_attack = std::max(0.001f, current_attack - 0.01f);
        poly_synth->setAttackTime(current_attack);
        break;
    case 'x': // Increase attack
        current_attack = std::min(1.0f, current_attack + 0.01f);
        poly_synth->setAttackTime(current_attack);
        break;
    // ... similar for decay, sustain, release
}
```

The ADSR envelope system provides professional-quality sound shaping capabilities that transform simple oscillators into expressive, musical instruments. The combination of precise timing control, polyphonic voice management, and real-time parameter adjustment makes it suitable for everything from basic synthesis to complex musical applications.
