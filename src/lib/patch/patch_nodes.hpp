/**
 * @file patch_nodes.hpp
 * @brief Concrete audio node implementations for patch system
 *
 * Provides ready-to-use audio processing nodes that implement the AudioNode
 * interface. These nodes wrap existing audio classes (Oscillator, Filter, etc.)
 * to work seamlessly with the patch system.
 *
 * Available node types:
 * - OscillatorNode: Waveform generation (sine, square, sawtooth, triangle)
 * - GainNode: Volume control and amplification
 * - FilterNode: Audio filtering (lowpass, highpass, bandpass)
 * - ReverbNode: Reverb and spatial effects
 * - MixerNode: Multi-input audio mixing
 *
 * All nodes support real-time parameter changes and automatic sample rate
 * configuration when added to patches.
 *
 * @see AudioPatch for patch management
 * @see PatchDSL for simplified node creation
 */

#pragma once

#include "audio_patch.hpp"
#include <cstddef>
#include "../oscillators/oscillator.hpp"
#include "../effects/filter.hpp"
#include "../effects/mixer.hpp"
#include "../effects/gain.hpp"
#include "../effects/tube_reverb.hpp"

namespace synth
{

  /**
   * @brief Audio oscillator node for waveform generation
   *
   * Wraps the Oscillator class to provide waveform generation within the
   * patch system. Supports all standard waveforms with real-time frequency
   * and waveform type changes.
   *
   * Features:
   * - Multiple waveform types (sine, square, sawtooth, triangle)
   * - Real-time frequency control
   * - Phase-coherent waveform switching
   * - Single mono output
   *
   * @code
   * auto osc = std::make_unique<OscillatorNode>("osc1", 440.0f, WaveType::SINE);
   * osc->set_frequency(880.0f);  // Change frequency
   * osc->set_wave_type(WaveType::SQUARE);  // Change waveform
   * @endcode
   */
  class OscillatorNode : public AudioNode
  {
  private:
    Oscillator oscillator_; ///< Internal oscillator for waveform generation

  public:
    /**
     * @brief Construct a new OscillatorNode
     * @param name Unique node name
     * @param frequency Initial frequency in Hz (default 440.0f)
     * @param wave_type Initial waveform type (default SINE)
     */
    OscillatorNode(const std::string &name, float frequency = 440.0f, WaveType wave_type = WaveType::SINE);

    /**
     * @brief Process audio samples through oscillator
     * @param inputs Input buffers (ignored for oscillator)
     * @param outputs Output buffers for generated waveform
     * @param sample_count Number of samples to generate
     */
    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override;

    /**
     * @brief Set sample rate for oscillator
     * @param sample_rate Sample rate in Hz
     */
    void set_sample_rate(float sample_rate) override;

    // Oscillator-specific controls

    /**
     * @brief Set oscillator frequency
     * @param frequency Frequency in Hz
     */
    void set_frequency(float frequency);

    /**
     * @brief Set waveform type
     * @param wave_type New waveform type
     */
    void set_wave_type(WaveType wave_type);
  };

  /**
   * @brief Audio gain/amplifier node for volume control
   *
   * Provides volume control and amplification for audio signals.
   * Supports real-time gain changes with smooth transitions to
   * prevent audio artifacts.
   *
   * Features:
   * - Real-time gain adjustment
   * - Unity gain (1.0f) = no change
   * - Gain > 1.0f = amplification
   * - Gain < 1.0f = attenuation
   * - Single input/output channel
   *
   * @code
   * auto gain = std::make_unique<GainNode>("volume", 0.8f);
   * gain->set_gain(1.2f);  // Amplify by 20%
   * gain->set_gain(0.5f);  // Reduce to half volume
   * @endcode
   */
  class GainNode : public AudioNode
  {
  private:
    Gain gain_effect_; ///< Internal gain processor

  public:
    /**
     * @brief Construct a new GainNode
     * @param name Unique node name
     * @param gain Initial gain value (default 1.0f = unity)
     */
    GainNode(const std::string &name, float gain = 1.0f);

    /**
     * @brief Process audio samples through gain control
     * @param inputs Input audio buffers
     * @param outputs Output audio buffers
     * @param sample_count Number of samples to process
     */
    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override;

    /**
     * @brief Set gain amount
     * @param gain Gain multiplier (1.0f = unity, >1.0f = amplify, <1.0f = attenuate)
     */
    void set_gain(float gain);

    /**
     * @brief Get current gain value
     * @return Current gain multiplier
     */
    float get_gain() const { return gain_effect_.get_gain(); }
  };

  /**
   * @brief Audio reverb node for spatial effects
   *
   * Provides reverb and spatial audio effects using a feedback delay network.
   * Creates the illusion of audio playing in different acoustic spaces from
   * small rooms to large halls.
   *
   * Features:
   * - Multiple reverb algorithms (hall, room, plate)
   * - Adjustable room size and decay time
   * - Wet/dry mix control
   * - Single input/output processing
   *
   * @code
   * auto reverb = std::make_unique<ReverbNode>("space");
   * reverb->set_room_size(0.8f);  // Large room
   * reverb->set_decay_time(3.0f);  // 3 second decay
   * @endcode
   */
  class ReverbNode : public AudioNode
  {
  private:
    std::vector<float> delay_buffer_; ///< Delay buffer for reverb effect
    size_t buffer_size_;              ///< Size of delay buffer
    size_t write_pos_;                ///< Current write position in buffer
    float decay_;                     ///< Decay factor (0.0f to 1.0f)
    float wet_level_;                 ///< Wet signal level
    float dry_level_;                 ///< Dry signal level
    float sample_rate_;               ///< Current sample rate

  public:
    /**
     * @brief Construct a new ReverbNode
     * @param name Unique node name
     * @param decay Reverb decay time factor (0.0f to 1.0f, default 0.5f)
     * @param wet_level Wet signal level (0.0f to 1.0f, default 0.3f)
     */
    ReverbNode(const std::string &name, float decay = 0.5f, float wet_level = 0.3f)
        : AudioNode(name, 1, 1), // 1 input, 1 output
          write_pos_(0),
          decay_(decay),
          wet_level_(wet_level),
          dry_level_(1.0f - wet_level),
          sample_rate_(48000.0f)
    {
      // Default to ~50ms delay buffer at 44.1kHz
      buffer_size_ = 2200;
      delay_buffer_.resize(buffer_size_, 0.0f);
    }

    /**
     * @brief Process audio samples through reverb effect
     * @param inputs Input audio buffers
     * @param outputs Output audio buffers
     * @param sample_count Number of samples to process
     */
    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override
    {
      (void)sample_count; // TODO: Implement reverb processing

      if (inputs.empty() || outputs.empty())
        return;
    }

    /**
     * @brief Set sample rate and update buffer size
     * @param sample_rate Sample rate in Hz
     */
    void set_sample_rate(float sample_rate) override
    {
      sample_rate_ = sample_rate;
      // Update buffer size to maintain similar reverb time at different sample rates
      buffer_size_ = static_cast<size_t>(sample_rate_ * 0.05f); // ~50ms
      delay_buffer_.resize(buffer_size_, 0.0f);
      write_pos_ = 0;
    }

    /**
     * @brief Set reverb decay time
     * @param decay Decay factor (0.0f = no reverb, 1.0f = infinite)
     */
    void set_decay(float decay)
    {
      decay_ = decay;
    }

    /**
     * @brief Set wet signal level
     * @param wet_level Wet level (0.0f = dry only, 1.0f = wet only)
     */
    void set_wet_level(float wet_level)
    {
      wet_level_ = wet_level;
      dry_level_ = 1.0f - wet_level_;
    }

    // Simple filter node with low-pass and high-pass modes
  };

  /**
   * @brief Physical model tube reverb node (1D digital waveguide)
   *
   * Wraps TubeReverb to expose acoustic tube resonance as a patch node.
   * P1 = tube_length, P2 = reflection, P3 = mix.
   *
   * @code
   * auto tr = std::make_unique<TubeReverbNode>("tube", 2.0f, 0.92f, 0.5f);
   * tr->set_tube_length(3.0f);  // P1
   * tr->set_reflection(0.95f);  // P2
   * tr->set_mix(0.4f);          // P3
   * @endcode
   */
  class TubeReverbNode : public AudioNode
  {
  private:
    TubeReverb tube_reverb_; ///< Physical model tube reverb

  public:
    /**
     * @brief Construct a new TubeReverbNode
     * @param name       Unique node name
     * @param tube_length P1: tube length in metres [0.3 – 6.0] (default 1.2f)
     * @param reflection  P2: reflection coefficient [0.5 – 0.99] (default 0.92f)
     * @param mix         P3: dry/wet blend [0.0 – 1.0] (default 0.5f)
     */
    TubeReverbNode(const std::string &name,
                   float tube_length = 1.2f,
                   float reflection = 0.92f,
                   float mix = 0.5f)
        : AudioNode(name, 1, 1), tube_reverb_(48000.0f)
    {
      tube_reverb_.setTubeLength(tube_length);
      tube_reverb_.setReflection(reflection);
      tube_reverb_.setMix(mix);
      set_parameter("tube_length", tube_length);
      set_parameter("reflection", reflection);
      set_parameter("mix", mix);
    }

    void set_sample_rate(float sample_rate) override
    {
      AudioNode::set_sample_rate(sample_rate);
      // Rebuild with new sample rate (TubeReverb re-creates on construction)
      float len = tube_reverb_.getTubeLength();
      float refl = tube_reverb_.getReflection();
      float mix = tube_reverb_.getMix();
      tube_reverb_ = TubeReverb(sample_rate);
      tube_reverb_.setTubeLength(len);
      tube_reverb_.setReflection(refl);
      tube_reverb_.setMix(mix);
    }

    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override
    {
      if (inputs.empty() || outputs.empty() || !inputs[0] || !outputs[0])
        return;
      tube_reverb_.processBuffer(inputs[0], outputs[0], static_cast<int>(sample_count));
    }

    /** @brief Set tube length (P1) */
    void set_tube_length(float length)
    {
      tube_reverb_.setTubeLength(length);
      set_parameter("tube_length", length);
    }

    /** @brief Set reflection coefficient (P2) */
    void set_reflection(float reflection)
    {
      tube_reverb_.setReflection(reflection);
      set_parameter("reflection", reflection);
    }

    /** @brief Set dry/wet mix (P3) */
    void set_mix(float mix)
    {
      tube_reverb_.setMix(mix);
      set_parameter("mix", mix);
    }

    /** @brief Returns f0 = 343 / (2 * tube_length) in Hz */
    float fundamental_hz() const { return tube_reverb_.fundamentalHz(); }
  };

  class FilterNode : public AudioNode
  {
  private:
    Filter filter_;

  public:
    FilterNode(const std::string &name, float cutoff_frequency = 1000.0f,
               float resonance = 0.7f, bool low_pass = true)
        : AudioNode(name, 1, 1) // 1 input, 1 output
          ,
          filter_(cutoff_frequency, resonance, low_pass)
    {
      set_parameter("cutoff", cutoff_frequency);
      set_parameter("resonance", resonance);
    }

    void set_sample_rate(float sample_rate) override
    {
      AudioNode::set_sample_rate(sample_rate);
      filter_.set_sample_rate(sample_rate);
    }

    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override;

    void set_cutoff(float cutoff)
    {
      filter_.set_cutoff_frequency(cutoff);
      set_parameter("cutoff", cutoff);
    }

    void set_resonance(float resonance)
    {
      filter_.set_resonance(resonance);
      set_parameter("resonance", resonance);
    }

    float get_cutoff() const { return filter_.get_cutoff_frequency(); }
    float get_resonance() const { return filter_.get_resonance(); }
  };

  // MixerNode combines multiple input signals into a single output
  class MixerNode : public AudioNode
  {
  private:
    Mixer mixer_;

  public:
    MixerNode(const std::string &name, size_t input_count = 2, float default_gain = 1.0f)
        : AudioNode(name, input_count, 1) // Multiple inputs, 1 output
          ,
          mixer_(input_count, default_gain)
    {
    }

    void set_gain(size_t channel, float gain)
    {
      mixer_.set_gain(channel, gain);
    }

    float get_gain(size_t channel) const
    {
      return mixer_.get_gain(channel);
    }

    void process(const std::vector<const float *> &inputs,
                 std::vector<float *> &outputs,
                 size_t sample_count) override;
  };

} // namespace synth
