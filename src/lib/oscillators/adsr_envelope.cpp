#include "adsr_envelope.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace synth
{
    // Constructor
    ADSREnvelope::ADSREnvelope(float sample_rate)
        : attack_time_(0.150f),   // 150ms attack for a noticeable swell
          decay_time_(0.350f),    // 350ms decay for a smoother transition
          sustain_level_(0.500f), // 50% sustain for more dynamic contrast
          release_time_(0.700f),  // 700ms release for a gentle tail
          state_(EnvelopeState::IDLE),
          current_amplitude_(0.0f),
          sample_rate_(sample_rate),
          attack_increment_(0.0f),
          decay_increment_(0.0f),
          release_increment_(0.0f),
          target_amplitude_(0.0f),
          samples_in_current_phase_(0),
          total_samples_in_phase_(0)
    {
        calculateIncrements();
        // std::cout << "ADSR Envelope initialized at " << sample_rate << " Hz\n";
    }

    // Calculate increment values for each phase
    // The curves: an attack that rises towards 1.3 and stops at 1 (the quick, rounded rise of an analog
    // envelope), a decay and a release that fall exponentially, the time being the time to fall 60 dB. They
    // used to be straight lines: every sound had the same gesture, and a linear release sounds cut.
    // attack_increment_: the attack's one-pole coefficient; decay_increment_, release_increment_: the
    // multipliers per sample.
    void ADSREnvelope::calculateIncrements()
    {
        auto pole = [this](float tau) { return std::exp(-1.0f / std::max(tau * sample_rate_, 1e-6f)); };
        attack_increment_ = attack_time_ > 0.0f ? 1.0f - pole(attack_time_ / 1.4663f) : 1.0f; // 1.3(1 - e^-t/tau) = 1 at t = attack
        decay_increment_ = decay_time_ > 0.0f ? pole(decay_time_ / 6.9078f) : 0.0f;          // e^-6.9 = -60 dB
        release_increment_ = release_time_ > 0.0f ? pole(release_time_ / 6.9078f) : 0.0f;
    }

    void ADSREnvelope::setAttackTime(float attack_seconds)
    {
        attack_time_ = std::max(0.0f, attack_seconds);
        calculateIncrements();
    }

    // Set decay time in seconds
    void ADSREnvelope::setDecayTime(float decay_seconds)
    {
        decay_time_ = std::max(0.0f, decay_seconds);
        calculateIncrements();
    }

    // Set sustain level (0.0 to 1.0)
    void ADSREnvelope::setSustainLevel(float sustain_level)
    {
        sustain_level_ = std::clamp(sustain_level, 0.0f, 1.0f);
        calculateIncrements();
    }

    // Set release time in seconds
    void ADSREnvelope::setSampleRate(float sample_rate)
    {
        if (sample_rate > 0.0f)
            sample_rate_ = sample_rate;
        calculateIncrements();
    }

    void ADSREnvelope::setReleaseTime(float release_seconds)
    {
        release_time_ = std::max(0.0f, release_seconds);
        calculateIncrements(); // (it said "calculated when release starts", and was not: a release set alone was ignored)
    }

    // Set all ADSR parameters at once
    void ADSREnvelope::setADSR(float attack, float decay, float sustain, float release)
    {
        attack_time_ = std::max(0.0f, attack);
        decay_time_ = std::max(0.0f, decay);
        sustain_level_ = std::clamp(sustain, 0.0f, 1.0f);
        release_time_ = std::max(0.0f, release);
        calculateIncrements();
    }

    // Start the envelope (note on)
    void ADSREnvelope::noteOn()
    {
        // from where the envelope is (a retrigger does not jump to 0: no click)
        state_ = EnvelopeState::ATTACK;
        samples_in_current_phase_ = 0;
        target_amplitude_ = 1.0f;
        if (attack_time_ <= 0.0f)
        {
            current_amplitude_ = 1.0f;
            state_ = decay_time_ > 0.0f ? EnvelopeState::DECAY : EnvelopeState::SUSTAIN;
            if (state_ == EnvelopeState::SUSTAIN)
                current_amplitude_ = sustain_level_;
        }
    }

    // Start release phase (note off)
    void ADSREnvelope::noteOff()
    {
        if (state_ == EnvelopeState::IDLE)
            return;
        state_ = EnvelopeState::RELEASE;
        samples_in_current_phase_ = 0;
        target_amplitude_ = 0.0f;
        if (release_time_ <= 0.0f)
        {
            current_amplitude_ = 0.0f;
            state_ = EnvelopeState::IDLE;
        }
    }

    // Reset envelope to idle state
    void ADSREnvelope::reset()
    {
        state_ = EnvelopeState::IDLE;
        current_amplitude_ = 0.0f;
        samples_in_current_phase_ = 0;
        total_samples_in_phase_ = 0;
        target_amplitude_ = 0.0f;
    }

    // Update to next phase when current phase is complete
    void ADSREnvelope::updatePhase()
    {

        switch (state_)
        {
        case EnvelopeState::ATTACK:
            // Attack finished, move to decay
            state_ = EnvelopeState::DECAY;
            samples_in_current_phase_ = 0;
            current_amplitude_ = 1.0f; // Ensure we're at peak

            if (decay_time_ > 0.0f)
            {
                total_samples_in_phase_ = static_cast<int>(decay_time_ * sample_rate_);
                target_amplitude_ = sustain_level_;
            }
            else
            {
                // Instant decay - go straight to sustain
                current_amplitude_ = sustain_level_;
                state_ = EnvelopeState::SUSTAIN;
            }
            break;

        case EnvelopeState::DECAY:
            // Decay finished, move to sustain
            state_ = EnvelopeState::SUSTAIN;
            current_amplitude_ = sustain_level_; // Ensure we're at sustain level
            samples_in_current_phase_ = 0;
            total_samples_in_phase_ = 0; // Sustain has no time limit
            target_amplitude_ = sustain_level_;
            break;

        case EnvelopeState::RELEASE:
            // Release finished, envelope is done
            state_ = EnvelopeState::IDLE;
            current_amplitude_ = 0.0f;
            samples_in_current_phase_ = 0;
            total_samples_in_phase_ = 0;
            target_amplitude_ = 0.0f;
            break;

        case EnvelopeState::SUSTAIN:
        case EnvelopeState::IDLE:
            // No automatic transitions from these states
            break;
        }

        // std::cout << "ADSR: Transitioned to state " << static_cast<int>(state_)
        // << " with amplitude " << current_amplitude_ << "\n";
    }

    // Get next envelope sample and advance
    float ADSREnvelope::getNextSample()
    {
        switch (state_)
        {
        case EnvelopeState::IDLE:
            current_amplitude_ = 0.0f;
            break;
        case EnvelopeState::ATTACK:
            current_amplitude_ += (1.3f - current_amplitude_) * attack_increment_;
            if (current_amplitude_ >= 1.0f)
            {
                current_amplitude_ = 1.0f;
                state_ = decay_time_ > 0.0f ? EnvelopeState::DECAY : EnvelopeState::SUSTAIN;
                if (state_ == EnvelopeState::SUSTAIN)
                    current_amplitude_ = sustain_level_;
            }
            break;
        case EnvelopeState::DECAY:
            current_amplitude_ = sustain_level_ + (current_amplitude_ - sustain_level_) * decay_increment_;
            if (current_amplitude_ - sustain_level_ < 1e-4f)
            {
                current_amplitude_ = sustain_level_;
                state_ = EnvelopeState::SUSTAIN;
            }
            break;
        case EnvelopeState::SUSTAIN:
            current_amplitude_ = sustain_level_; // follows a sustain turned while held
            break;
        case EnvelopeState::RELEASE:
            current_amplitude_ *= release_increment_;
            if (current_amplitude_ < 1e-4f) // -80 dB: done
            {
                current_amplitude_ = 0.0f;
                state_ = EnvelopeState::IDLE;
            }
            break;
        }
        return current_amplitude_;
    }

    // Get current amplitude without advancing
    float ADSREnvelope::getCurrentAmplitude() const
    {
        return current_amplitude_;
    }

    float ADSREnvelope::peekSampleAt(int sampleOffset, int releaseSampleOffset) const
    {
        if (sampleOffset < 0)
            return 0.0f;

        // Precompute phase lengths in samples
        const int attackSamples = attack_time_ > 0.0f ? static_cast<int>(attack_time_ * sample_rate_) : 0;
        const int decaySamples = decay_time_ > 0.0f ? static_cast<int>(decay_time_ * sample_rate_) : 0;
        const int releaseSamples = release_time_ > 0.0f ? static_cast<int>(release_time_ * sample_rate_) : 0;

        auto baseAmplitudeBeforeRelease = [&](int s) -> float
        {
            if (attackSamples > 0 && s < attackSamples)
            {
                return static_cast<float>(s) / static_cast<float>(attackSamples);
            }
            s -= attackSamples;
            if (decaySamples > 0 && s < decaySamples)
            {
                float t = static_cast<float>(s) / static_cast<float>(decaySamples);
                return 1.0f - (1.0f - sustain_level_) * t;
            }
            // Sustain phase (or instantaneous transitions)
            return sustain_level_;
        };

        if (releaseSampleOffset >= 0 && sampleOffset >= releaseSampleOffset)
        {
            // Amplitude at the moment release begins
            float startReleaseAmp = baseAmplitudeBeforeRelease(releaseSampleOffset);
            int relPos = sampleOffset - releaseSampleOffset;
            if (releaseSamples <= 0)
                return 0.0f; // instant release
            if (relPos >= releaseSamples)
                return 0.0f;
            float t = static_cast<float>(relPos) / static_cast<float>(releaseSamples);
            return startReleaseAmp * (1.0f - t);
        }
        else
        {
            return baseAmplitudeBeforeRelease(sampleOffset);
        }
    }

    // Check if envelope is active (not idle)
    bool ADSREnvelope::isActive() const
    {
        return state_ != EnvelopeState::IDLE;
    }

    // Check if in release phase
    bool ADSREnvelope::isInRelease() const
    {
        return state_ == EnvelopeState::RELEASE;
    }

    // Get current envelope state
    EnvelopeState ADSREnvelope::getState() const
    {
        return state_;
    }

} // namespace synth
