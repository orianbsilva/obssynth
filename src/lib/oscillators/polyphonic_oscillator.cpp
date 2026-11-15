#include "polyphonic_oscillator.hpp"
#include "pcm_waveform.hpp"
#include "../math/synt_math.hpp"
#include <iostream>
#include <algorithm>
#include <cstdlib>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{
    // Constructor
    PolyphonicOscillator::PolyphonicOscillator(WaveType wave_type, float sample_rate)
        : sample_rate_(sample_rate), wave_type_(wave_type),
          attack_time_(0.150f),   // 150ms default attack
          decay_time_(0.350f),    // 350ms default decay
          sustain_level_(0.500f), // 50% default sustain
          release_time_(0.700f),  // 700ms default release
          dry_wet_mix_(1.0f)      // 100% wet (envelope processed) by default
    {
        // Initialize all voices
        for (auto &voice : voices_)
        {
            voice.oscillator_ = std::make_unique<Oscillator>(440.0f, wave_type, sample_rate);
            voice.envelope_ = std::make_unique<ADSREnvelope>(sample_rate);
            voice.oscillator_->setAmplitude(0.0f); // Start silent
            voice.is_active_ = false;

            // Set default ADSR values for the envelope
            voice.envelope_->setADSR(attack_time_, decay_time_, sustain_level_, release_time_);
        }

        std::cout << "PolyphonicOscillator initialized with " << MAX_VOICES
                  << " voices at " << sample_rate << " Hz\n";
    }

    // Find a free voice for note allocation
    int PolyphonicOscillator::findFreeVoice() const
    {
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (!voices_[i].is_active_)
            {
                return i;
            }
        }
        return -1; // No free voice found
    }

    // Find the oldest active voice (simple voice stealing)
    int PolyphonicOscillator::findOldestVoice() const
    {
        // For simplicity, just return the first active voice
        // In a more sophisticated implementation, you'd track voice age
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (voices_[i].is_active_)
            {
                return i;
            }
        }
        return 0; // Fallback to first voice
    }

    // Find voice playing a specific note
    int PolyphonicOscillator::findVoiceByNote(uint8_t note_number) const
    {
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (voices_[i].is_active_ && voices_[i].note_number_ == note_number)
            {
                return i;
            }
        }
        return -1; // Note not found
    }

    // Start playing a note
    void PolyphonicOscillator::noteOn(uint8_t note_number, float velocity, float pan)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Check if this note is already playing and stop it first
        int existing_voice = findVoiceByNote(note_number);
        if (existing_voice != -1)
        {
            // Immediately deactivate the existing voice and reset its envelope
            Voice &existing = voices_[existing_voice];
            existing.is_active_ = false;
            existing.is_crossfading_ = false;
            existing.envelope_->reset();
            existing.oscillator_->setAmplitude(0.0f);

            std::cout << "Retriggering note " << static_cast<int>(note_number)
                      << " - deactivated voice " << existing_voice << "\n";
        }

        // Find a free voice
        int voice_index = findFreeVoice();

        // If no free voice, steal the oldest one with crossfade
        if (voice_index == -1)
        {
            voice_index = findOldestVoice();
            Voice &stolen_voice = voices_[voice_index];

            // Start crossfade to fade out the stolen voice
            if (stolen_voice.is_active_ && stolen_voice.amplitude_ > 0.0f)
            {
                initiateCrossfade(stolen_voice, 0.0f, 2.0f); // 2ms quick fadeout
                std::cout << "Voice stealing with crossfade: using voice " << voice_index << "\n";
            }
        }

        // Configure the voice
        Voice &voice = voices_[voice_index];
        voice.note_number_ = note_number;
        voice.frequency_ = noteToFrequency(note_number);
        voice.velocity_ = velocity;
        float target_amplitude = velocity * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES)); // Better amplitude scaling for professional output
        voice.pan_position_ = pan;

        // Configure the oscillator
        voice.oscillator_->setFrequency(voice.frequency_);
        voice.oscillator_->setPan(voice.pan_position_);
        voice.oscillator_->resetPhase(); // Reset phase for clean attack

        // Start ADSR envelope
        voice.envelope_->noteOn();

        // Always start voices smoothly to prevent clicks
        if (voice.is_active_ && voice.amplitude_ > 0.0f)
        {
            // Voice was already active - crossfade from current amplitude to new target
            initiateCrossfade(voice, target_amplitude, 5.0f); // 5ms fade-in
        }
        else
        {
            // Fresh voice - start at zero and use crossfade for smooth attack
            voice.amplitude_ = 0.0f;
            voice.oscillator_->setAmplitude(0.0f);
            initiateCrossfade(voice, target_amplitude, 3.0f); // 3ms smooth start
        }

        voice.is_active_ = true;

        std::cout << "Note ON: " << static_cast<int>(note_number)
                  << " (" << voice.frequency_ << " Hz) on voice " << voice_index
                  << " with velocity " << velocity << " pan " << pan << "\n";
    }

    // Stop playing a note
    void PolyphonicOscillator::noteOff(uint8_t note_number)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        int voice_index = findVoiceByNote(note_number);
        if (voice_index != -1)
        {
            Voice &voice = voices_[voice_index];

            // Start envelope release phase - this is the primary release mechanism
            voice.envelope_->noteOff();

            // For very short release times or emergency cases, use a gentle crossfade as backup
            // This prevents stuck notes if the envelope fails to deactivate properly
            if (release_time_ < 0.05f) // If release time is less than 50ms
            {
                // Use crossfade for very short releases to ensure clean cutoff
                initiateCrossfade(voice, 0.0f, release_time_ * 1000.0f); // Convert to ms
            }

            std::cout << "Note OFF: " << static_cast<int>(note_number)
                      << " on voice " << voice_index << " (ADSR release phase)\n";
        }
    }

    // Stop all playing notes
    void PolyphonicOscillator::allNotesOff()
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        for (auto &voice : voices_)
        {
            voice.is_active_ = false;
            voice.oscillator_->setAmplitude(0.0f);
            voice.envelope_->reset(); // Reset ADSR envelope
        }

        std::cout << "All notes OFF\n";
    }

    // Set wave type for all voices
    void PolyphonicOscillator::setWaveType(WaveType wave_type)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        wave_type_ = wave_type;
        for (auto &voice : voices_)
        {
            voice.oscillator_->setWaveType(wave_type);
        }
    }

    // Set custom PCM waveform for all voices
    void PolyphonicOscillator::setCustomWaveform(const std::vector<float> &pcm_data)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Set the custom waveform for all voices
        for (auto &voice : voices_)
        {
            voice.oscillator_->set_custom_waveform(pcm_data);
        }
    }

    void PolyphonicOscillator::setCustomWaveform(std::shared_ptr<PCMWaveform> waveform)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Set the custom waveform for all voices using PCMWaveform object
        // This preserves wav bank metadata including bank position control
        for (auto &voice : voices_)
        {
            voice.oscillator_->set_custom_waveform(waveform);
        }
    }

    // Set wav bank position for all voices
    void PolyphonicOscillator::setBankPosition(float position)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Set bank position for all voices
        for (auto &voice : voices_)
        {
            voice.oscillator_->set_bank_position(position);
        }
    }

    // Get current wav bank position (from first voice)
    float PolyphonicOscillator::getBankPosition() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Return bank position from first voice (all voices should have the same position)
        if (!voices_.empty())
        {
            return voices_[0].oscillator_->get_bank_position();
        }
        return 0.0f;
    }

    // Check if any voice is using a wav bank
    bool PolyphonicOscillator::isWavBank() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Check if first voice is using a wav bank (all voices should be the same)
        if (!voices_.empty())
        {
            return voices_[0].oscillator_->is_wav_bank();
        }
        return false;
    }

    // Set master amplitude scaling for all voices
    void PolyphonicOscillator::setMasterAmplitude(float amplitude)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        for (auto &voice : voices_)
        {
            if (voice.is_active_)
            {
                float scaled_amplitude = voice.velocity_ * amplitude / MAX_VOICES;
                voice.oscillator_->setAmplitude(scaled_amplitude);
            }
        }
    }

    // Set pan position for a specific voice by note number
    void PolyphonicOscillator::setVoicePan(uint8_t note_number, float pan)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        int voice_index = findVoiceByNote(note_number);
        if (voice_index != -1)
        {
            voices_[voice_index].pan_position_ = pan;
            voices_[voice_index].oscillator_->setPan(pan);
        }
    }

    void PolyphonicOscillator::setADSR(float attack, float decay, float sustain, float release)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        attack_time_ = attack;
        decay_time_ = decay;
        sustain_level_ = sustain;
        release_time_ = release;

        // Update all voice envelopes
        for (auto &voice : voices_)
        {
            voice.envelope_->setADSR(attack, decay, sustain, release);
        }
    }

    // Set attack time for all voices
    void PolyphonicOscillator::setAttackTime(float attack_seconds)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        attack_time_ = attack_seconds;

        for (auto &voice : voices_)
        {
            voice.envelope_->setAttackTime(attack_seconds);
        }
    }

    // Set decay time for all voices
    void PolyphonicOscillator::setDecayTime(float decay_seconds)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        decay_time_ = decay_seconds;

        for (auto &voice : voices_)
        {
            voice.envelope_->setDecayTime(decay_seconds);
        }
    }

    // Set sustain level for all voices
    void PolyphonicOscillator::setSustainLevel(float sustain_level)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        sustain_level_ = sustain_level;

        for (auto &voice : voices_)
        {
            voice.envelope_->setSustainLevel(sustain_level);
        }
    }

    // Set release time for all voices
    void PolyphonicOscillator::setReleaseTime(float release_seconds)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        release_time_ = release_seconds;

        for (auto &voice : voices_)
        {
            voice.envelope_->setReleaseTime(release_seconds);
        }
    }

    // Set dry/wet mix for envelope processing
    void PolyphonicOscillator::setDryWetMix(float mix)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        // Clamp mix to [0.0f, 1.0f] range
        dry_wet_mix_ = std::max(0.0f, std::min(1.0f, mix));
    }

    // Generate a single sample by mixing all active voices
    float PolyphonicOscillator::getSample()
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        float mixed_sample = 0.0f;

        for (auto &voice : voices_)
        {
            // Process voice if it's active OR if the envelope is still active (including release phase)
            if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
            {
                // Process crossfade if active
                if (voice.is_crossfading_)
                {
                    processCrossfade(voice);

                    // Check if voice should be deactivated after crossfade
                    if (!voice.is_crossfading_ && voice.amplitude_ <= 0.0f)
                    {
                        voice.is_active_ = false;
                    }
                }

                // Generate sample if voice is still active, crossfading, or envelope is active
                if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                {
                    // Get envelope amplitude
                    float envelope_amplitude = voice.envelope_->getNextSample();

                    // Calculate dry and wet amplitudes with professional output levels
                    float dry_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));                      // Raw oscillator amplitude
                    float wet_amplitude = voice.velocity_ * envelope_amplitude * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES)); // Envelope-processed amplitude

                    // Mix dry and wet signals based on dry_wet_mix_
                    float mixed_amplitude = (1.0f - dry_wet_mix_) * dry_amplitude + dry_wet_mix_ * wet_amplitude;

                    // Apply crossfade scaling if active (multiply by crossfade ratio)
                    if (voice.is_crossfading_)
                    {
                        float target_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));
                        if (target_amplitude > 0.0f)
                        {
                            float crossfade_scale = voice.amplitude_ / target_amplitude;
                            mixed_amplitude *= crossfade_scale;
                        }
                    }

                    voice.oscillator_->setAmplitude(mixed_amplitude);
                    mixed_sample += voice.oscillator_->getSample();

                    // Check if envelope is finished and voice should be deactivated
                    if (!voice.envelope_->isActive() && envelope_amplitude <= 0.0f)
                    {
                        voice.is_active_ = false;
                        voice.oscillator_->setAmplitude(0.0f);
                    }
                }
            }
        }

        return mixed_sample;
    }

    // Generate a single stereo sample by mixing all active voices
    StereoSample PolyphonicOscillator::getStereoSample()
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        StereoSample mixed_sample(0.0f, 0.0f);

        for (auto &voice : voices_)
        {
            // Process voice if it's active OR if the envelope is still active (including release phase)
            if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
            {
                // Process crossfade if active
                if (voice.is_crossfading_)
                {
                    processCrossfade(voice);

                    // Check if voice should be deactivated after crossfade
                    if (!voice.is_crossfading_ && voice.amplitude_ <= 0.0f)
                    {
                        voice.is_active_ = false;
                    }
                }

                // Generate stereo sample if voice is still active, crossfading, or envelope is active
                if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                {
                    // Get envelope amplitude
                    float envelope_amplitude = voice.envelope_->getNextSample();

                    // Calculate dry and wet amplitudes with professional output levels
                    float dry_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));                      // Raw oscillator amplitude
                    float wet_amplitude = voice.velocity_ * envelope_amplitude * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES)); // Envelope-processed amplitude

                    // Mix dry and wet signals based on dry_wet_mix_
                    float mixed_amplitude = (1.0f - dry_wet_mix_) * dry_amplitude + dry_wet_mix_ * wet_amplitude;

                    // Apply crossfade scaling if active (multiply by crossfade ratio)
                    if (voice.is_crossfading_)
                    {
                        float target_amplitude = voice.velocity_ * 0.8f / MAX_VOICES;
                        if (target_amplitude > 0.0f)
                        {
                            float crossfade_scale = voice.amplitude_ / target_amplitude;
                            mixed_amplitude *= crossfade_scale;
                        }
                    }

                    voice.oscillator_->setAmplitude(mixed_amplitude);
                    StereoSample voice_sample = voice.oscillator_->getStereoSample();
                    mixed_sample = mixed_sample + voice_sample;

                    // Check if envelope is finished and voice should be deactivated
                    if (!voice.envelope_->isActive() && envelope_amplitude <= 0.0f)
                    {
                        voice.is_active_ = false;
                        voice.oscillator_->setAmplitude(0.0f);
                    }
                }
            }
        }

        return mixed_sample;
    }

    // Generate a buffer of samples (mono) - Now uses optimized method
    void PolyphonicOscillator::generateBuffer(float *buffer, int size)
    {
        generateBufferOptimized(buffer, size);
    }

    // Generate a buffer of stereo samples (interleaved L,R,L,R...) - Now uses optimized method
    void PolyphonicOscillator::generateStereoBuffer(float *buffer, int frames)
    {
        generateStereoBufferOptimized(buffer, frames);
    }

    // Generate a buffer of samples (mono) - OPTIMIZED: Block-level locking
    void PolyphonicOscillator::generateBufferOptimized(float *buffer, int size)
    {
        if (!buffer || size <= 0)
            return;

        // Initialize output buffer to zero
        std::fill(buffer, buffer + size, 0.0f);

        // Process sample-by-sample to handle crossfades properly
        for (int sample = 0; sample < size; ++sample)
        {
            float mixedSample = 0.0f;

            {
                std::lock_guard<std::mutex> lock(voices_mutex_);

                for (int v = 0; v < MAX_VOICES; ++v)
                {
                    Voice &voice = voices_[v];

                    // Process voice if it's active OR if the envelope is still active (including release phase)
                    if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                    {
                        // Process crossfade if active
                        if (voice.is_crossfading_)
                        {
                            processCrossfade(voice);

                            // Check if voice should be deactivated after crossfade
                            if (!voice.is_crossfading_ && voice.amplitude_ <= 0.0f)
                            {
                                voice.is_active_ = false;
                            }
                        }

                        // Generate sample if voice is still active, crossfading, or envelope is active
                        if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                        {
                            // Get envelope amplitude
                            float envelope_amplitude = voice.envelope_->getNextSample();

                            // Calculate dry and wet amplitudes with professional output levels
                            float dry_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));                      // Raw oscillator amplitude
                            float wet_amplitude = voice.velocity_ * envelope_amplitude * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES)); // Envelope-processed amplitude

                            // Mix dry and wet signals based on dry_wet_mix_
                            float mixed_amplitude = (1.0f - dry_wet_mix_) * dry_amplitude + dry_wet_mix_ * wet_amplitude;

                            // Apply crossfade scaling if active (multiply by crossfade ratio)
                            if (voice.is_crossfading_)
                            {
                                float target_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));
                                if (target_amplitude > 0.0f)
                                {
                                    float crossfade_scale = voice.amplitude_ / target_amplitude;
                                    mixed_amplitude *= crossfade_scale;
                                }
                            }

                            voice.oscillator_->setAmplitude(mixed_amplitude);
                            mixedSample += voice.oscillator_->getSample();

                            // Check if envelope is finished and voice should be deactivated
                            if (!voice.envelope_->isActive() && envelope_amplitude <= 0.0f)
                            {
                                voice.is_active_ = false;
                                voice.oscillator_->setAmplitude(0.0f);
                            }
                        }
                    }
                }
            }

            buffer[sample] = mixedSample;
        }
    }

    // Generate a buffer of stereo samples (interleaved L,R,L,R...) - OPTIMIZED: Block-level locking
    void PolyphonicOscillator::generateStereoBufferOptimized(float *buffer, int frames)
    {
        if (!buffer || frames <= 0)
            return;

        // Initialize output buffer to zero
        std::fill(buffer, buffer + frames * 2, 0.0f);

        // Process frame-by-frame to handle crossfades properly
        for (int frame = 0; frame < frames; ++frame)
        {
            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            {
                std::lock_guard<std::mutex> lock(voices_mutex_);

                for (int v = 0; v < MAX_VOICES; ++v)
                {
                    Voice &voice = voices_[v];

                    // Process voice if it's active OR if the envelope is still active (including release phase)
                    if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                    {
                        // Process crossfade if active
                        if (voice.is_crossfading_)
                        {
                            processCrossfade(voice);

                            // Check if voice should be deactivated after crossfade
                            if (!voice.is_crossfading_ && voice.amplitude_ <= 0.0f)
                            {
                                voice.is_active_ = false;
                            }
                        }

                        // Generate stereo sample if voice is still active, crossfading, or envelope is active
                        if (voice.is_active_ || voice.is_crossfading_ || voice.envelope_->isActive())
                        {
                            // Get envelope amplitude
                            float envelope_amplitude = voice.envelope_->getNextSample();

                            // Calculate dry and wet amplitudes with professional output levels
                            float dry_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));                      // Raw oscillator amplitude
                            float wet_amplitude = voice.velocity_ * envelope_amplitude * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES)); // Envelope-processed amplitude

                            // Mix dry and wet signals based on dry_wet_mix_
                            float mixed_amplitude = (1.0f - dry_wet_mix_) * dry_amplitude + dry_wet_mix_ * wet_amplitude;

                            // Apply crossfade scaling if active (multiply by crossfade ratio)
                            if (voice.is_crossfading_)
                            {
                                float target_amplitude = voice.velocity_ * 3.0f / static_cast<float>(std::sqrt(MAX_VOICES));
                                if (target_amplitude > 0.0f)
                                {
                                    float crossfade_scale = voice.amplitude_ / target_amplitude;
                                    mixed_amplitude *= crossfade_scale;
                                }
                            }

                            voice.oscillator_->setAmplitude(mixed_amplitude);
                            StereoSample stereo_sample = voice.oscillator_->getStereoSample();
                            mixedLeft += stereo_sample.left;
                            mixedRight += stereo_sample.right;

                            // Check if envelope is finished and voice should be deactivated
                            if (!voice.envelope_->isActive() && envelope_amplitude <= 0.0f)
                            {
                                voice.is_active_ = false;
                                voice.oscillator_->setAmplitude(0.0f);
                            }
                        }
                    }
                }
            }

            buffer[frame * 2] = mixedLeft;      // Left channel
            buffer[frame * 2 + 1] = mixedRight; // Right channel
        }
    }

    // Get number of active voices
    int PolyphonicOscillator::getActiveVoiceCount() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        int count = 0;
        for (const auto &voice : voices_)
        {
            if (voice.is_active_)
            {
                count++;
            }
        }
        return count;
    }

    int PolyphonicOscillator::getReleasingVoiceCount() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        int count = 0;
        for (const auto &voice : voices_)
        {
            if (voice.is_active_ && !voice.envelope_->isActive())
            {
                count++;
            }
        }
        return count;
    }

    bool PolyphonicOscillator::isActive() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        for (const auto &voice : voices_)
        {
            if (voice.is_active_)
            {
                return true;
            }
        }
        return false;
    }

    // Check if a specific note is currently active
    bool PolyphonicOscillator::isNoteActive(uint8_t note_number) const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        return findVoiceByNote(note_number) != -1;
    }

    // Convert MIDI note number to frequency (A4 = 440 Hz)
    float PolyphonicOscillator::noteToFrequency(uint8_t note_number)
    {
        // MIDI note 69 = A4 = 440 Hz
        // Each semitone is a factor of 2^(1/12)
        constexpr float A4_FREQUENCY = 440.0f;
        constexpr int A4_NOTE = 69;

        float semitones_from_a4 = static_cast<float>(note_number - A4_NOTE);
        return A4_FREQUENCY * math::pow(2.0f, semitones_from_a4 / 12.0f);
    }

    // Convert single float sample to 16-bit short with proper clamping and dithering
    short PolyphonicOscillator::convertSampleToShort(float sample)
    {
        // Clamp to [-1.0f, 1.0f] range
        if (sample > 1.0f)
            sample = 1.0f;
        else if (sample < -1.0f)
            sample = -1.0f;

        // Add triangular dither noise for better quantization (1 LSB)
        constexpr float DITHER_AMPLITUDE = 1.0f / 65536.0f; // 1 LSB for 16-bit
        static thread_local float last_dither = 0.0f;

        float current_dither = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * DITHER_AMPLITUDE;
        float triangular_dither = current_dither - last_dither;
        last_dither = current_dither;

        sample += triangular_dither;

        // Convert to 16-bit with proper scaling
        return static_cast<short>(sample * 32767.0f);
    }

    // Convert buffer of float samples to 16-bit shorts with SIMD optimization where available
    /*static*/ void PolyphonicOscillator::convertAndQuantize(const float *floatBuffer, short *shortBuffer, int sampleCount)
    {
        if (!floatBuffer || !shortBuffer || sampleCount <= 0)
            return;

// Use SIMD for bulk conversion if available, otherwise fallback to scalar
#ifdef SYNTH_HAS_SSE2
        // SSE2 optimized conversion for x86/x64
        const int simdSamples = sampleCount & ~3; // Process 4 samples at a time

        for (int i = 0; i < simdSamples; i += 4)
        {
            __m128 floats = _mm_loadu_ps(&floatBuffer[i]); // Use unaligned load for safety

            // Clamp to [-1.0, 1.0]
            __m128 ones = _mm_set1_ps(1.0f);
            __m128 neg_ones = _mm_set1_ps(-1.0f);
            floats = _mm_min_ps(_mm_max_ps(floats, neg_ones), ones);

            // Scale by 32767
            __m128 scale = _mm_set1_ps(32767.0f);
            floats = _mm_mul_ps(floats, scale);

            // Convert to integers
            __m128i ints = _mm_cvtps_epi32(floats);

            // Pack to 16-bit and store
            __m128i shorts = _mm_packs_epi32(ints, ints);
            _mm_storel_epi64((__m128i *)&shortBuffer[i], shorts);
        }

        // Handle remaining samples
        for (int i = simdSamples; i < sampleCount; ++i)
        {
            shortBuffer[i] = convertSampleToShort(floatBuffer[i]);
        }
#else
        // Scalar fallback for ARM/other architectures
        for (int i = 0; i < sampleCount; ++i)
        {
            shortBuffer[i] = convertSampleToShort(floatBuffer[i]);
        }
#endif
    }

    void PolyphonicOscillator::reset() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);

        for (const auto &voice : voices_)
        {
            if (voice.is_active_)
            {
                voice.oscillator_->resetPhase();
            }
        }
    }

    // Initiate a crossfade transition for smooth voice stealing
    void PolyphonicOscillator::initiateCrossfade(Voice &voice, float target_amplitude, float crossfade_duration_ms)
    {
        voice.is_crossfading_ = true;
        voice.target_amplitude_ = target_amplitude;

        // Calculate crossfade parameters
        int crossfade_samples = static_cast<int>(crossfade_duration_ms * sample_rate_ / 1000.0f);
        voice.crossfade_samples_remaining_ = crossfade_samples;

        if (crossfade_samples > 0)
        {
            // Calculate increment per sample for smooth transition
            voice.crossfade_increment_ = (target_amplitude - voice.amplitude_) / static_cast<float>(crossfade_samples);
        }
        else
        {
            // Instant transition if duration is too small
            voice.amplitude_ = target_amplitude;
            voice.is_crossfading_ = false;
            voice.oscillator_->setAmplitude(target_amplitude);
        }
    }

    // Process crossfade transition - called during audio generation
    void PolyphonicOscillator::processCrossfade(Voice &voice)
    {
        if (!voice.is_crossfading_)
            return;

        if (voice.crossfade_samples_remaining_ > 0)
        {
            // Update amplitude gradually
            voice.amplitude_ += voice.crossfade_increment_;
            voice.oscillator_->setAmplitude(voice.amplitude_);
            voice.crossfade_samples_remaining_--;
        }
        else
        {
            // Crossfade complete
            voice.amplitude_ = voice.target_amplitude_;
            voice.oscillator_->setAmplitude(voice.target_amplitude_);
            voice.is_crossfading_ = false;
        }
    }

    // === FM Synthesis Control ===

    void PolyphonicOscillator::setFMModulatorRatio(float ratio)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMModulatorRatio(ratio);
            }
        }
    }

    void PolyphonicOscillator::setFMModulationIndex(float index)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMModulationIndex(index);
            }
        }
    }

    void PolyphonicOscillator::setFMAlgorithm(FMAlgorithm algorithm)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMAlgorithm(algorithm);
            }
        }
    }

    void PolyphonicOscillator::setFMOperatorRatio(int operator_num, float ratio)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMOperatorRatio(operator_num, ratio);
            }
        }
    }

    void PolyphonicOscillator::setFMOperatorLevel(int operator_num, float level)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMOperatorLevel(operator_num, level);
            }
        }
    }

    void PolyphonicOscillator::setFMFeedback(float feedback)
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                voice.oscillator_->setFMFeedback(feedback);
            }
        }
    }

    FMAlgorithm PolyphonicOscillator::getFMAlgorithm() const
    {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        // Return the algorithm from the first voice (they should all be the same)
        for (const auto &voice : voices_)
        {
            if (voice.oscillator_)
            {
                return voice.oscillator_->getFMAlgorithm();
            }
        }
        // Default fallback
        return FMAlgorithm::TWO_OP_SIMPLE;
    }

} // namespace synth
