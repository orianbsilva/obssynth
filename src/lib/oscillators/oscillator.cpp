#include "oscillator.hpp"
#include "pcm_waveform.hpp"
#include "../math/synt_math.hpp"
#include <cmath>
#include <iostream>

namespace synth
{

    // Wave generator functions (now static methods)
    float Oscillator::sine_wave(float normalized_phase)
    {
        return math::sin(normalized_phase * math::TWO_PI);
    }

    // PolyBLEP residual: removes discontinuity at phase position t with increment dt
    // Apply as: sample -= polyblep(t, dt) for upward steps, += for downward steps
    float Oscillator::polyblep(float t, float dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        else if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float Oscillator::saw_wave(float normalized_phase)
    {
        float t = normalized_phase - math::floor(normalized_phase);
        float dt = phase_increment_ / math::TWO_PI;
        // Naive sawtooth: 2t - 1, then subtract PolyBLEP at discontinuity (t=0/1)
        float sample = 2.0f * t - 1.0f;
        sample -= polyblep(t, dt);
        return sample;
    }

    float Oscillator::square_wave(float normalized_phase)
    {
        float t = normalized_phase - math::floor(normalized_phase);
        float dt = phase_increment_ / math::TWO_PI;
        // Naive square, then apply PolyBLEP at both transitions (0 and 0.5)
        float sample = t < 0.5f ? 1.0f : -1.0f;
        sample += polyblep(t, dt);                         // rising edge at t=0
        sample -= polyblep(std::fmod(t + 0.5f, 1.0f), dt); // falling edge at t=0.5
        return sample;
    }

    float Oscillator::triangle_wave(float normalized_phase)
    {
        // Normalize phase to 0-1 range
        float phase = normalized_phase - math::floor(normalized_phase);
        if (phase < 0.5f)
        {
            // Rising edge: -1 to 1
            return 4.0f * phase - 1.0f;
        }
        else
        {
            // Falling edge: 1 to -1
            return 3.0f - 4.0f * phase;
        }
    }

    float Oscillator::pulse_wave(float normalized_phase)
    {
        // Pulse wave with 25% duty cycle — band-limited with PolyBLEP at both edges
        float t = normalized_phase - math::floor(normalized_phase);
        float dt = phase_increment_ / math::TWO_PI;
        float sample = t < 0.25f ? 1.0f : -1.0f;
        sample += polyblep(t, dt);                          // rising edge at t=0
        sample -= polyblep(std::fmod(t + 0.75f, 1.0f), dt); // falling edge at t=0.25
        return sample;
    }

    float Oscillator::noise_wave(float normalized_phase)
    {
        // Simple white noise using phase as seed
        // This creates repeating noise pattern, which is better for musical applications
        static unsigned int seed = 1;
        seed = (unsigned int)(normalized_phase * 0xFFFFFF) ^ (seed << 1) ^ (seed >> 1);
        seed = seed * 1103515245 + 12345;
        return ((float)(seed & 0x7FFFFF) / 0x3FFFFF) - 1.0f;
    }

    float Oscillator::ramp_up_wave(float normalized_phase)
    {
        // Linear ramp from -1 to 1 (same as sawtooth but different name for clarity)
        float phase = normalized_phase - math::floor(normalized_phase);
        return 2.0f * phase - 1.0f;
    }

    float Oscillator::ramp_down_wave(float normalized_phase)
    {
        // Linear ramp from 1 to -1 (inverse sawtooth)
        float phase = normalized_phase - math::floor(normalized_phase);
        return 1.0f - 2.0f * phase;
    }

    float Oscillator::pcm_custom_wave(float normalized_phase) const
    {
        if (!pcm_waveform_ || !pcm_waveform_->isLoaded())
            return 0.0f; // silence (it printed a line here every sample, on the audio thread)
        return pcm_waveform_->getSample(normalized_phase);
    }

    float Oscillator::fm_synthesis_wave(float normalized_phase)
    {
        float carrier_phase = normalized_phase * math::TWO_PI;
        float output = 0.0f;

        switch (fm_algorithm_)
        {
        case FMAlgorithm::TWO_OP_SIMPLE:
        {
            // Classic 2-operator FM: Carrier = sin(C + I * sin(M))
            float modulator_output = math::sin(fm_modulator_phase_);
            float modulation = fm_modulation_index_ * modulator_output;
            output = math::sin(carrier_phase + modulation);
            break;
        }
        case FMAlgorithm::TWO_OP_FEEDBACK:
        {
            // 2-operator with feedback: includes carrier self-modulation
            // NOTE: uses per-instance member (not static!) so voices are independent in polyphony
            float modulator_output = math::sin(fm_modulator_phase_);
            float feedback_modulation = fm_feedback_amount_ * fm_previous_output_;
            float modulation = fm_modulation_index_ * modulator_output + feedback_modulation;
            output = math::sin(carrier_phase + modulation);
            fm_previous_output_ = output;
            break;
        }
        case FMAlgorithm::FOUR_OP_SERIAL:
        {
            // 4-operator serial: Op4->Op3->Op2->Op1->Output
            float op4_output = math::sin(fm_operator4_phase_) * fm_operator4_level_;
            float op3_output = math::sin(fm_operator3_phase_ + op4_output) * fm_operator3_level_;
            float op2_output = math::sin(fm_operator2_phase_ + op3_output) * fm_operator2_level_;
            float modulation = fm_modulation_index_ * math::sin(fm_modulator_phase_ + op2_output);
            output = math::sin(carrier_phase + modulation);
            break;
        }
        case FMAlgorithm::FOUR_OP_PARALLEL:
        {
            // 4-operator parallel: (Op3->Op1) + (Op4->Op2)->Output
            float op3_output = math::sin(fm_operator3_phase_) * fm_operator3_level_;
            float op4_output = math::sin(fm_operator4_phase_) * fm_operator4_level_;
            float mod1 = math::sin(fm_modulator_phase_ + op3_output);
            float mod2 = math::sin(fm_operator2_phase_ + op4_output) * fm_operator2_level_;
            float combined_modulation = fm_modulation_index_ * (mod1 + mod2) * 0.5f;
            output = math::sin(carrier_phase + combined_modulation);
            break;
        }
        case FMAlgorithm::FOUR_OP_MIXED:
        {
            // 4-operator mixed: (Op4->Op3->Op1) + (Op2)->Output
            float op4_output = math::sin(fm_operator4_phase_) * fm_operator4_level_;
            float op3_output = math::sin(fm_operator3_phase_ + op4_output) * fm_operator3_level_;
            float mod1 = math::sin(fm_modulator_phase_ + op3_output);
            float mod2 = math::sin(fm_operator2_phase_) * fm_operator2_level_;
            float combined_modulation = fm_modulation_index_ * (mod1 + mod2) * 0.5f;
            output = math::sin(carrier_phase + combined_modulation);
            break;
        }
        }

        // Advance all operator phases
        fm_modulator_phase_ += fm_modulator_phase_increment_;
        fm_operator2_phase_ += fm_operator2_phase_increment_;
        fm_operator3_phase_ += fm_operator3_phase_increment_;
        fm_operator4_phase_ += fm_operator4_phase_increment_;

        // Wrap phases to prevent overflow
        // wrapped whole: an increment can pass 2 pi (ratio 16 on a high note), and one subtraction let the phase grow
        for (float *p : {&fm_modulator_phase_, &fm_operator2_phase_, &fm_operator3_phase_, &fm_operator4_phase_})
            if (*p >= math::TWO_PI)
                *p = math::fmod(*p, math::TWO_PI);

        return output;
    }

    // Constructor (replaces oscillator_init)
    Oscillator::Oscillator(float frequency, WaveType wave_type, float sample_rate)
        : frequency_(frequency), phase_(0.0f), sample_rate_(sample_rate), wave_type_(wave_type), amplitude_(0.5f) // Default amplitude (50%)
          ,
          phase_increment_(0.0f), samples_per_buffer_(0), base_frequency_(frequency), frequency_mod_amount_(0.0f), pan_position_(0.0f),                                               // Center pan
          fm_algorithm_(FMAlgorithm::TWO_OP_SIMPLE), fm_modulator_frequency_ratio_(1.0f), fm_modulation_index_(1.0f), fm_modulator_phase_(0.0f), fm_modulator_phase_increment_(0.0f), // FM synthesis defaults
          fm_operator2_ratio_(2.0f), fm_operator3_ratio_(3.0f), fm_operator4_ratio_(4.0f),                                                                                            // 4-operator ratios
          fm_operator2_level_(0.5f), fm_operator3_level_(0.5f), fm_operator4_level_(0.5f),                                                                                            // 4-operator levels
          fm_operator2_phase_(0.0f), fm_operator3_phase_(0.0f), fm_operator4_phase_(0.0f),                                                                                            // 4-operator phases
          fm_operator2_phase_increment_(0.0f), fm_operator3_phase_increment_(0.0f), fm_operator4_phase_increment_(0.0f),                                                              // 4-operator phase increments
          fm_feedback_amount_(0.0f),                                                                                                                                                  // Feedback amount
          fm_previous_output_(0.0f)                                                                                                                                                   // Per-instance feedback state
    {
        // Set the wave generator function based on wave type
        switch (wave_type)
        {
        case WaveType::SINE:
            wave_generator_ = sine_wave;
            break;
        case WaveType::SAW:
            wave_generator_ = [this](float phase)
            { return this->saw_wave(phase); };
            break;
        case WaveType::SQUARE:
            wave_generator_ = [this](float phase)
            { return this->square_wave(phase); };
            break;
        case WaveType::TRIANGLE:
            wave_generator_ = triangle_wave;
            break;
        case WaveType::PULSE:
            wave_generator_ = [this](float phase)
            { return this->pulse_wave(phase); };
            break;
        case WaveType::NOISE:
            wave_generator_ = noise_wave;
            break;
        case WaveType::RAMP_UP:
            wave_generator_ = ramp_up_wave;
            break;
        case WaveType::RAMP_DOWN:
            wave_generator_ = ramp_down_wave;
            break;
        case WaveType::PCM_CUSTOM:
            wave_generator_ = [this](float phase)
            { return this->pcm_custom_wave(phase); };
            break;
        case WaveType::FM_SYNTHESIS:
            wave_generator_ = [this](float phase)
            { return this->fm_synthesis_wave(phase); };
            break;
        default:
            wave_generator_ = sine_wave; // Default to sine
            break;
        }

        // Update phase increment
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
    }

    // Set sample rate and recalculate phase increment
    void Oscillator::setSampleRate(float sample_rate)
    {
        sample_rate_ = sample_rate;
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
        fm_modulator_phase_increment_ = phase_increment_ * fm_modulator_frequency_ratio_;
        fm_operator2_phase_increment_ = phase_increment_ * fm_operator2_ratio_;
        fm_operator3_phase_increment_ = phase_increment_ * fm_operator3_ratio_;
        fm_operator4_phase_increment_ = phase_increment_ * fm_operator4_ratio_;
    }

    // Set oscillator frequency
    void Oscillator::setFrequency(float frequency)
    {
        base_frequency_ = frequency;
        // Apply current modulation to the new base frequency
        frequency_ = base_frequency_ * (1.0f + frequency_mod_amount_ * 0.1f); // 10% max modulation
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
        // Update FM modulator phase increment
        fm_modulator_phase_increment_ = phase_increment_ * fm_modulator_frequency_ratio_;
        // Update additional FM operator phase increments
        fm_operator2_phase_increment_ = phase_increment_ * fm_operator2_ratio_;
        fm_operator3_phase_increment_ = phase_increment_ * fm_operator3_ratio_;
        fm_operator4_phase_increment_ = phase_increment_ * fm_operator4_ratio_;
    }

    // Set frequency modulation amount
    void Oscillator::setFrequencyModulation(float mod_amount)
    {
        // Clamp modulation amount to valid range
        if (mod_amount < -1.0f)
            mod_amount = -1.0f;
        if (mod_amount > 1.0f)
            mod_amount = 1.0f;

        frequency_mod_amount_ = mod_amount;

        // Apply modulation to current base frequency
        frequency_ = base_frequency_ * (1.0f + frequency_mod_amount_ * 0.1f); // 10% max modulation (as setFrequency: it was 1% here)
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
        // Update FM modulator phase increment
        fm_modulator_phase_increment_ = phase_increment_ * fm_modulator_frequency_ratio_;
        // Update additional FM operator phase increments
        fm_operator2_phase_increment_ = phase_increment_ * fm_operator2_ratio_;
        fm_operator3_phase_increment_ = phase_increment_ * fm_operator3_ratio_;
        fm_operator4_phase_increment_ = phase_increment_ * fm_operator4_ratio_;
    }

    // Set wave type
    void Oscillator::setWaveType(WaveType wave_type)
    {
        wave_type_ = wave_type;

        // Update the wave generator function pointer
        switch (wave_type)
        {
        case WaveType::SINE:
            wave_generator_ = sine_wave;
            break;
        case WaveType::SAW:
            wave_generator_ = [this](float phase)
            { return this->saw_wave(phase); };
            break;
        case WaveType::SQUARE:
            wave_generator_ = [this](float phase)
            { return this->square_wave(phase); };
            break;
        case WaveType::TRIANGLE:
            wave_generator_ = triangle_wave;
            break;
        case WaveType::PULSE:
            wave_generator_ = [this](float phase)
            { return this->pulse_wave(phase); };
            break;
        case WaveType::NOISE:
            wave_generator_ = noise_wave;
            break;
        case WaveType::RAMP_UP:
            wave_generator_ = ramp_up_wave;
            break;
        case WaveType::RAMP_DOWN:
            wave_generator_ = ramp_down_wave;
            break;
        case WaveType::PCM_CUSTOM:
            wave_generator_ = [this](float phase)
            { return this->pcm_custom_wave(phase); };
            break;
        case WaveType::FM_SYNTHESIS:
            wave_generator_ = [this](float phase)
            { return this->fm_synthesis_wave(phase); };
            break;
        default:
            wave_generator_ = sine_wave; // Default to sine
            break;
        }
    }

    // Set amplitude (0.0 to 1.0)
    void Oscillator::setAmplitude(float amplitude)
    {
        if (amplitude < 0.0f)
            amplitude = 0.0f;
        if (amplitude > 1.0f)
            amplitude = 1.0f;
        amplitude_ = amplitude;
    }

    // Set pan position (-1.0 to 1.0)
    void Oscillator::setPan(float pan)
    {
        if (pan < -1.0f)
            pan = -1.0f;
        if (pan > 1.0f)
            pan = 1.0f;
        pan_position_ = pan;
    }

    // Reset phase to zero (useful for minimizing clicks on note start)
    void Oscillator::resetPhase()
    {
        phase_ = 0.0f;
    }

    void Oscillator::advancePhase()
    {
        phase_ += phase_increment_;
        if (phase_ >= math::TWO_PI)
            phase_ = math::fmod(phase_, math::TWO_PI);
    }

    // Generate single sample
    float Oscillator::getSample()
    {
        float normalized_phase = phase_ / math::TWO_PI;

        float sample = wave(normalized_phase);

        // Apply amplitude
        sample *= amplitude_;

        // Advance phase
        phase_ += phase_increment_;
        phase_ = math::fmod(phase_, math::TWO_PI);

        return sample;
    }

    // Generate single stereo sample
    StereoSample Oscillator::getStereoSample()
    {
        float normalized_phase = phase_ / math::TWO_PI;

        float mono_sample;

        // Use the pre-set wave generator function (consistent with getSample)
        mono_sample = wave(normalized_phase);

        // Apply amplitude
        mono_sample *= amplitude_;

        // Advance phase
        phase_ += phase_increment_;
        phase_ = math::fmod(phase_, math::TWO_PI);

        // Apply panning using constant power panning law
        float left_gain = math::sqrt((1.0f - pan_position_) * 0.5f);
        float right_gain = math::sqrt((1.0f + pan_position_) * 0.5f);

        return StereoSample(mono_sample * left_gain, mono_sample * right_gain);
    }

    // Generate buffer of samples (mono)
    void Oscillator::generateBuffer(float *buffer, int size)
    {
        if (!buffer || size <= 0)
            return;

        // Always generate exactly 'size' samples to ensure deterministic block sizes
        samples_per_buffer_ = size;
        for (int i = 0; i < size; ++i)
        {
            buffer[i] = getSample();
        }
    }

    // Generate buffer of stereo samples (interleaved L,R,L,R...)
    void Oscillator::generateStereoBuffer(float *buffer, int frames)
    {
        if (!buffer || frames <= 0)
            return;

        samples_per_buffer_ = frames;
        for (int i = 0; i < frames; ++i)
        {
            StereoSample stereo_sample = getStereoSample();
            buffer[i * 2] = stereo_sample.left;      // Left channel
            buffer[i * 2 + 1] = stereo_sample.right; // Right channel
        }
    }

    float Oscillator::peekSampleAtOffset(int offset) const
    {
        if (offset < 0)
            offset = 0;
        // Compute phase as if advanced 'offset' steps
        float phase = phase_ + phase_increment_ * static_cast<float>(offset);
        // Wrap
        phase = math::fmod(phase, math::TWO_PI);
        if (phase < 0.f)
            phase += math::TWO_PI;
        float normalized_phase = phase / math::TWO_PI;
        float sample = 0.f;
        switch (wave_type_)
        {
        case WaveType::SINE:
            sample = math::sin(normalized_phase * math::TWO_PI);
            break;
        case WaveType::SAW:
            sample = 2.0f * (normalized_phase - math::floor(normalized_phase + 0.5f));
            break;
        case WaveType::SQUARE:
            sample = (normalized_phase - math::floor(normalized_phase)) < 0.5f ? -1.0f : 1.0f;
            break;
        case WaveType::TRIANGLE:
        {
            float local_phase = normalized_phase - math::floor(normalized_phase);
            if (local_phase < 0.5f)
                sample = 4.0f * local_phase - 1.0f;
            else
                sample = 3.0f - 4.0f * local_phase;
        }
        break;
        case WaveType::PULSE:
            sample = (normalized_phase - math::floor(normalized_phase)) < 0.25f ? 1.0f : -1.0f;
            break;
        case WaveType::NOISE:
        {
            static unsigned int seed = 1;
            seed = (unsigned int)(normalized_phase * 0xFFFFFF) ^ (seed << 1) ^ (seed >> 1);
            seed = seed * 1103515245 + 12345;
            sample = ((float)(seed & 0x7FFFFF) / 0x3FFFFF) - 1.0f;
        }
        break;
        case WaveType::RAMP_UP:
        {
            float local_phase = normalized_phase - math::floor(normalized_phase);
            sample = 2.0f * local_phase - 1.0f;
        }
        break;
        case WaveType::RAMP_DOWN:
        {
            float local_phase = normalized_phase - math::floor(normalized_phase);
            sample = 1.0f - 2.0f * local_phase;
        }
        break;
        case WaveType::PCM_CUSTOM:
            sample = pcm_custom_wave(normalized_phase);
            break;
        case WaveType::FM_SYNTHESIS:
            // FM synthesis cannot be peeked accurately as it modifies state
            // Return a sine wave approximation for preview purposes
            sample = math::sin(normalized_phase * math::TWO_PI);
            break;
        }
        return sample * amplitude_;
    }

    // Set stereo panning position
    void Oscillator::set_pan(float pan)
    {
        // Clamp pan to valid range [-1.0, 1.0]
        if (pan < -1.0f)
            pan = -1.0f;
        if (pan > 1.0f)
            pan = 1.0f;
        pan_position_ = pan;
    }

    // Set custom waveform from PCM data
    void Oscillator::set_custom_waveform(const std::vector<float> &pcm_data)
    {
        std::cout << "DEBUG: set_custom_waveform called with " << pcm_data.size() << " samples" << std::endl;

        if (pcm_data.empty())
        {
            std::cout << "DEBUG: PCM data is empty, resetting waveform" << std::endl;
            pcm_waveform_.reset();
            return;
        }

        // Create new PCMWaveform from the data
        pcm_waveform_ = std::make_shared<PCMWaveform>(pcm_data);
        std::cout << "DEBUG: Created PCMWaveform, loaded: " << (pcm_waveform_->isLoaded() ? "YES" : "NO") << std::endl;

        // Set wave type to PCM_CUSTOM and update the function pointer
        wave_type_ = WaveType::PCM_CUSTOM;
        wave_generator_ = [this](float phase)
        { return this->pcm_custom_wave(phase); };

        std::cout << "DEBUG: Set wave type to PCM_CUSTOM (" << static_cast<int>(wave_type_) << ")" << std::endl;
    }

    // Set custom waveform from PCMWaveform object
    void Oscillator::set_custom_waveform(std::shared_ptr<PCMWaveform> waveform)
    {
        if (!waveform || !waveform->isLoaded())
        {
            pcm_waveform_.reset();
            return;
        }

        // Set the shared PCMWaveform
        pcm_waveform_ = waveform;

        // Set wave type to PCM_CUSTOM and update the function pointer
        wave_type_ = WaveType::PCM_CUSTOM;
        wave_generator_ = [this](float phase)
        { return this->pcm_custom_wave(phase); };
    }

    // Load custom waveform from file
    bool Oscillator::load_custom_waveform(const std::string &filename)
    {
        auto waveform = PCMWaveform::createFromFile(filename);
        if (waveform)
        {
            set_custom_waveform(waveform);
            return true;
        }
        return false;
    }

    // Check if custom waveform is loaded
    bool Oscillator::has_custom_waveform() const
    {
        return pcm_waveform_ && pcm_waveform_->isLoaded();
    }

    // Set wav bank position for custom waveforms
    void Oscillator::set_bank_position(float position)
    {
        if (pcm_waveform_ && pcm_waveform_->isLoaded())
        {
            pcm_waveform_->setBankPosition(position);
        }
    }

    // Get current wav bank position
    float Oscillator::get_bank_position() const
    {
        if (pcm_waveform_ && pcm_waveform_->isLoaded())
        {
            return pcm_waveform_->getBankPosition();
        }
        return 0.0f;
    }

    // Check if current custom waveform is a wav bank
    bool Oscillator::is_wav_bank() const
    {
        return pcm_waveform_ && pcm_waveform_->isLoaded() && pcm_waveform_->isWavBank();
    }

    // FM Synthesis methods
    void Oscillator::setFMModulatorRatio(float ratio)
    {
        fm_modulator_frequency_ratio_ = ratio;
        // Update modulator phase increment when ratio changes
        fm_modulator_phase_increment_ = phase_increment_ * ratio;
    }

    void Oscillator::setFMModulationIndex(float index)
    {
        fm_modulation_index_ = index;
    }

    float Oscillator::getFMModulatorRatio() const
    {
        return fm_modulator_frequency_ratio_;
    }

    float Oscillator::getFMModulationIndex() const
    {
        return fm_modulation_index_;
    }

    void Oscillator::resetFMModulatorPhase()
    {
        fm_modulator_phase_ = 0.0f;
        fm_operator2_phase_ = fm_operator3_phase_ = fm_operator4_phase_ = 0.0f; // every operator: the same attack each note
        fm_previous_output_ = 0.0f; // clear feedback state on noteOn
    }

    void Oscillator::setFMAlgorithm(FMAlgorithm algorithm)
    {
        fm_algorithm_ = algorithm;
    }

    FMAlgorithm Oscillator::getFMAlgorithm() const
    {
        return fm_algorithm_;
    }

    void Oscillator::setFMOperatorRatio(int operator_num, float ratio)
    {
        switch (operator_num)
        {
        case 2:
            fm_operator2_ratio_ = ratio;
            fm_operator2_phase_increment_ = phase_increment_ * ratio;
            break;
        case 3:
            fm_operator3_ratio_ = ratio;
            fm_operator3_phase_increment_ = phase_increment_ * ratio;
            break;
        case 4:
            fm_operator4_ratio_ = ratio;
            fm_operator4_phase_increment_ = phase_increment_ * ratio;
            break;
        }
    }

    void Oscillator::setFMOperatorLevel(int operator_num, float level)
    {
        // Clamp level to valid range
        if (level < 0.0f)
            level = 0.0f;
        if (level > 1.0f)
            level = 1.0f;

        switch (operator_num)
        {
        case 2:
            fm_operator2_level_ = level;
            break;
        case 3:
            fm_operator3_level_ = level;
            break;
        case 4:
            fm_operator4_level_ = level;
            break;
        }
    }

    void Oscillator::setFMFeedback(float feedback)
    {
        // Clamp feedback to valid range
        if (feedback < 0.0f)
            feedback = 0.0f;
        if (feedback > 1.0f)
            feedback = 1.0f;
        fm_feedback_amount_ = feedback;
    }

    float Oscillator::getFMOperatorRatio(int operator_num) const
    {
        switch (operator_num)
        {
        case 2:
            return fm_operator2_ratio_;
        case 3:
            return fm_operator3_ratio_;
        case 4:
            return fm_operator4_ratio_;
        default:
            return 1.0f;
        }
    }

    float Oscillator::getFMOperatorLevel(int operator_num) const
    {
        switch (operator_num)
        {
        case 2:
            return fm_operator2_level_;
        case 3:
            return fm_operator3_level_;
        case 4:
            return fm_operator4_level_;
        default:
            return 0.0f;
        }
    }

    float Oscillator::getFMFeedback() const
    {
        return fm_feedback_amount_;
    }

} // namespace synth
