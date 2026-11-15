# Audio Patch System - Advanced Audio Pipeline Architecture

## Overview

The Audio Patch System is a sophisticated framework for creating modular audio processing pipelines in the synth project. It provides a flexible, extensible architecture that allows users to build complex audio circuits using both programmatic APIs and a Domain Specific Language (DSL).

## Core Concepts

### 1. AudioNode - Processing Units

AudioNodes are the fundamental building blocks of audio patches. Each node represents a single audio processing unit with defined inputs and outputs.

**Key Features:**
- **Multi-input/Multi-output**: Nodes can have any number of audio inputs and outputs
- **Parameter System**: Dynamic parameter control with real-time updates
- **Sample Rate Awareness**: Automatic sample rate propagation and adaptation
- **Reset Capability**: State reset for consistent audio processing

**Base AudioNode Interface:**
```cpp
class AudioNode {
public:
    AudioNode(const std::string& name, size_t inputs = 1, size_t outputs = 1);
    virtual void process(const std::vector<const float*>& inputs,
                        std::vector<float*>& outputs,
                        size_t sample_count) = 0;
    virtual void set_parameter(const std::string& param, float value);
    virtual float get_parameter(const std::string& param) const;
};
```

### 2. AudioPatch - Pipeline Container

AudioPatch manages collections of interconnected AudioNodes, handling routing, buffer management, and processing order.

**Key Features:**
- **Node Management**: Add, remove, and configure audio processing nodes
- **Connection Routing**: Direct connections and side-chain routing
- **Processing Order**: Automatic topological sorting for dependency resolution
- **Buffer Management**: Efficient audio buffer allocation and reuse
- **Real-time Processing**: Low-latency audio processing suitable for live performance

### 3. PatchDSL - Fluent API

The Patch Domain Specific Language provides a readable, chainable API for building audio patches programmatically.

**Design Philosophy:**
- **Fluent Interface**: Method chaining for readable patch construction
- **Type Safety**: Compile-time verification of patch structure
- **Extensibility**: Easy addition of new node types and connection patterns
- **Convenience**: Pre-built patch templates for common use cases

## Available Audio Nodes

### Synthesis Nodes

#### OscillatorNode
Multi-waveform audio oscillator supporting sine, square, sawtooth, and triangle waves.

```cpp
auto osc = std::make_unique<OscillatorNode>("osc1", OscillatorNode::WaveType::Sawtooth, 440.0f);
```

**Parameters:**
- `frequency`: Oscillator frequency in Hz (20.0f - 20000.0f)
- `amplitude`: Output amplitude (0.0f - 1.0f)

#### EnvelopeNode
ADSR envelope generator for amplitude and parameter modulation.

```cpp
auto env = std::make_unique<EnvelopeNode>("env1", 0.1f, 0.2f, 0.7f, 0.5f);
```

**Parameters:**
- `attack`: Attack time in seconds
- `decay`: Decay time in seconds
- `sustain`: Sustain level (0.0f - 1.0f)
- `release`: Release time in seconds

### Processing Nodes

#### FilterNode
Digital filter with lowpass, highpass, and bandpass modes.

```cpp
auto filter = std::make_unique<FilterNode>("lpf1", FilterNode::FilterType::Lowpass, 1000.0f);
```

**Parameters:**
- `cutoff`: Cutoff frequency in Hz
- `resonance`: Filter resonance (0.0f - 2.0f)

#### GainNode
Simple amplification/attenuation node.

```cpp
auto gain = std::make_unique<GainNode>("volume", 0.8f);
```

**Parameters:**
- `gain`: Gain factor (0.0f - 2.0f)

### Effect Nodes

#### ReverbNode
Schroeder reverb algorithm with configurable room characteristics.

```cpp
auto reverb = std::make_unique<ReverbNode>("hall", 0.8f, 0.3f, 0.4f);
```

**Parameters:**
- `room_size`: Room size (0.0f - 1.0f)
- `damping`: High-frequency damping (0.0f - 1.0f)
- `wet_level`: Wet/dry mix (0.0f - 1.0f)

#### DelayNode
Digital delay with feedback and wet/dry mixing.

```cpp
auto delay = std::make_unique<DelayNode>("echo", 0.25f, 0.4f, 0.3f);
```

**Parameters:**
- `delay_time`: Delay time in seconds
- `feedback`: Feedback amount (0.0f - 0.95f)
- `wet_level`: Wet/dry mix (0.0f - 1.0f)

### Utility Nodes

#### MixerNode
Multi-channel mixer with per-channel gain and panning.

```cpp
auto mixer = std::make_unique<MixerNode>("mix", 4);
mixer->set_channel_gain(0, 0.8f);
mixer->set_channel_pan(1, -0.5f); // Pan left
```

**Parameters:**
- `gain_N`: Gain for channel N
- `pan_N`: Pan for channel N (-1.0f = left, 1.0f = right)

## DSL Usage Examples

### Basic Synthesizer Voice

```cpp
auto synth_patch = PatchDSL("basic_synth")
    .add_oscillator("osc", "sawtooth", 440.0f)
    .add_envelope("amp_env", 0.01f, 0.3f, 0.7f, 0.8f)
    .add_filter("vcf", "lowpass", 2000.0f)
    .add_reverb("reverb", "hall")
    .add_gain("output", 0.8f)
    .connect("osc", "amp_env")
    .connect("amp_env", "vcf")
    .connect("vcf", "reverb")
    .connect("reverb", "output")
    .set("vcf.cutoff", 1500.0f)
    .set("reverb.wet_level", 0.3f)
    .build();
```

### Effects Processing Chain

```cpp
auto fx_chain = PatchDSL("guitar_fx")
    .add_gain("input", 1.2f)                    // Input boost
    .add_filter("hpf", "highpass", 80.0f)       // High-pass filter
    .add_filter("lpf", "lowpass", 8000.0f)      // Low-pass filter
    .add_delay("delay", 0.125f)                 // 1/8 note delay
    .add_reverb("reverb", "medium")             // Medium room reverb
    .add_gain("output", 0.9f)                   // Output level
    .chain({"input", "hpf", "lpf", "delay", "reverb", "output"})
    .set("delay.feedback", 0.4f)
    .set("delay.wet_level", 0.25f)
    .set("reverb.room_size", 0.6f)
    .build();
```

### Complex Multi-Oscillator Patch

```cpp
auto complex_patch = PatchDSL("poly_synth")
    // Oscillators
    .add_oscillator("osc1", "sawtooth", 440.0f)
    .add_oscillator("osc2", "square", 440.0f * 1.005f)  // Slight detune
    .add_oscillator("sub", "sine", 440.0f * 0.5f)       // Sub-oscillator

    // Mixing
    .add_mixer("osc_mix", 3)
    .connect("osc1", "osc_mix", 0)
    .connect("osc2", "osc_mix", 1)
    .connect("sub", "osc_mix", 2)
    .set("osc_mix.gain_0", 0.5f)
    .set("osc_mix.gain_1", 0.3f)
    .set("osc_mix.gain_2", 0.4f)

    // Envelopes
    .add_envelope("amp_env", 0.01f, 0.3f, 0.7f, 0.8f)
    .add_envelope("filter_env", 0.01f, 0.5f, 0.2f, 1.0f)

    // Filter with envelope modulation
    .add_filter("vcf", "lowpass", 1000.0f)
    .connect("osc_mix", "amp_env")
    .connect("amp_env", "vcf")
    .sidechain("filter_env", "vcf", 0.8f)      // Filter modulation

    // Effects
    .add_delay("delay", 0.1875f)               // Dotted 1/8 note
    .add_reverb("reverb", "hall")
    .add_gain("output", 0.7f)
    .connect("vcf", "delay")
    .connect("delay", "reverb")
    .connect("reverb", "output")

    // Fine-tuning
    .set("delay.feedback", 0.3f)
    .set("delay.wet_level", 0.15f)
    .set("reverb.room_size", 0.8f)
    .set("reverb.wet_level", 0.25f)
    .build();
```

## Side-Chain Connections

Side-chain connections allow one signal to modulate parameters of another node without being mixed into the audio path.

```cpp
auto patch = PatchDSL("sidechained")
    .add_oscillator("carrier", "sine", 440.0f)
    .add_oscillator("modulator", "sine", 2.0f)      // 2 Hz LFO
    .add_filter("vcf", "lowpass", 1000.0f)
    .connect("carrier", "vcf")                      // Audio path
    .sidechain("modulator", "vcf", 0.5f)           // Modulation path
    .build();
```

## Advanced Features

### Dynamic Parameter Control

```cpp
// Runtime parameter modification
patch->set_node_parameter("vcf", "cutoff", 800.0f);
patch->set_node_parameter("reverb", "room_size", 0.9f);

// Parameter querying
float current_cutoff = patch->get_node_parameter("vcf", "cutoff");
```

### Patch Introspection

```cpp
// Get patch information
auto node_names = patch->get_node_names();
auto connections = patch->get_connections();

// Debug output
patch_utils::print_patch_info(*patch);
```

### Real-time Processing

```cpp
const size_t buffer_size = 256;
float input_buffer[buffer_size];
float output_buffer[buffer_size];

// Process audio in real-time chunks
patch->process(input_buffer, output_buffer, buffer_size);
```

## Performance Considerations

### Buffer Management
- **Efficient Allocation**: Buffers are allocated once and reused
- **Minimal Copying**: Zero-copy audio routing where possible
- **SIMD Optimization**: Float-only operations optimized for SIMD

### Processing Order
- **Topological Sorting**: Automatic dependency resolution
- **Cache Efficiency**: Memory layout optimized for cache performance
- **Parallel Processing**: Future support for parallel node processing

### Memory Usage
- **Static Allocation**: Buffers allocated at patch creation
- **Pool Management**: Node object pooling for dynamic patches
- **Reference Counting**: Efficient connection management

## Integration with Existing Systems

### MIDI Integration

```cpp
// MIDI-controlled patch
auto midi_patch = patch_utils::create_synth_voice(440.0f);

// MIDI note on/off handling
void on_midi_note(int note, int velocity) {
    float frequency = midi_to_frequency(note);
    float amplitude = velocity / 127.0f;

    midi_patch->set_node_parameter("osc", "frequency", frequency);
    midi_patch->set_node_parameter("osc", "amplitude", amplitude);

    // Trigger envelope
    auto* env_node = dynamic_cast<EnvelopeNode*>(midi_patch->get_node("amp_env"));
    if (env_node) env_node->note_on();
}
```

### Audio Backend Integration

```cpp
// Integration with audio sink
class PatchAudioSink : public AudioSink {
    std::unique_ptr<AudioPatch> patch_;

public:
    void audio_callback(float* output, size_t frames) override {
        static float silent_input[frames] = {0.0f};
        patch_->process(silent_input, output, frames);
    }
};
```

## Future Extensions

### Planned Features
1. **Visual Patch Editor**: Graphical patch construction interface
2. **Patch Save/Load**: Serialization to JSON/XML formats
3. **Plugin Architecture**: Dynamic loading of third-party nodes
4. **Automation**: Parameter automation and modulation matrices
5. **Multi-threading**: Parallel processing for complex patches
6. **GPU Acceleration**: CUDA/OpenCL support for heavy processing

### Node Extensions
1. **Granular Synthesis**: Granular synthesis node
2. **Physical Modeling**: String/membrane modeling nodes
3. **Spectral Processing**: FFT-based spectral effects
4. **Machine Learning**: AI-powered audio processing nodes

## Best Practices

### Patch Design
1. **Modular Approach**: Design reusable sub-patches
2. **Parameter Ranges**: Use sensible parameter ranges and defaults
3. **Performance Testing**: Profile patches under load
4. **Error Handling**: Graceful degradation for invalid parameters

### DSL Usage
1. **Readable Names**: Use descriptive node names
2. **Logical Grouping**: Group related operations
3. **Parameter Documentation**: Comment complex parameter settings
4. **Template Usage**: Leverage pre-built patch templates

### Integration
1. **Thread Safety**: Ensure thread-safe parameter updates
2. **Latency Awareness**: Minimize processing delay
3. **Resource Management**: Proper cleanup of patch resources
4. **Testing**: Comprehensive unit and integration testing

## Conclusion

The Audio Patch System provides a powerful, flexible foundation for advanced audio processing in the synth project. Its modular design enables both simple and complex audio pipelines while maintaining high performance and real-time capabilities. The DSL makes patch creation intuitive and maintainable, while the underlying architecture supports future extensions and optimizations.
