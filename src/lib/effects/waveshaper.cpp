#include "waveshaper.hpp"
#include <cmath>
#include <algorithm>

namespace synth
{

    TubeSaturation::TubeSaturation(float sample_rate)
        : sample_rate_(sample_rate),
          model_(TubeModel::AX7),
          drive_(1.8f),
          asymmetry_(0.3f),
          chaos_(0.0f),
          mix_(0.5f),
          slow_mod_(0.0f),
          curve_fn_(&TubeSaturation::curveAX7)
    {
    }

    void TubeSaturation::setModel(TubeModel model)
    {
        model_ = model;
        switch (model)
        {
        case TubeModel::FLAT:
            curve_fn_ = &TubeSaturation::curveFLAT;
            break;
        case TubeModel::AX7:
            curve_fn_ = &TubeSaturation::curveAX7;
            break;
        case TubeModel::AT7:
            curve_fn_ = &TubeSaturation::curveAT7;
            break;
        case TubeModel::AU7:
            curve_fn_ = &TubeSaturation::curveAU7;
            break;
        case TubeModel::EL34:
            curve_fn_ = &TubeSaturation::curveEL34;
            break;
        case TubeModel::P6L6:
            curve_fn_ = &TubeSaturation::curveP6L6;
            break;
        }
    }

    TubeModel TubeSaturation::getModel() const { return model_; }

    void TubeSaturation::setDrive(float drive)
    {
        drive_ = std::clamp(drive, 0.5f, 8.0f);
    }

    void TubeSaturation::setAsymmetry(float asymmetry)
    {
        asymmetry_ = std::clamp(asymmetry, 0.0f, 1.0f);
    }

    void TubeSaturation::setChaos(float chaos)
    {
        chaos_ = std::clamp(chaos, 0.0f, 1.0f);
    }

    void TubeSaturation::setMix(float mix)
    {
        mix_ = std::clamp(mix, 0.0f, 1.0f);
    }

    float TubeSaturation::transferCurve(float x)
    {
        x = std::clamp(x, -1.0f, 1.0f);
        float y = (this->*curve_fn_)(x);

        // Apply asymmetry: adds even harmonics (y² term = octave up content)
        y += asymmetry_ * y * y * 0.4f;

        // Physical tube randomness: slow drift emulating thermal fluctuation,
        // power supply ripple, and microphonics
        if (chaos_ > 0.0f)
        {
            slow_mod_ += 0.0008f + chaos_ * 0.0003f;
            // Keep phase bounded to avoid float precision loss over time
            constexpr float two_pi = 6.2831853f;
            if (slow_mod_ > two_pi * 100.0f)
                slow_mod_ -= two_pi * 100.0f;

            float physical_variation = std::sin(slow_mod_) * 0.018f * chaos_ + std::sin(slow_mod_ * 3.7f) * 0.007f * chaos_;

            // More effect on saturated portions of the signal
            y += physical_variation * (y * y);

            // Slow bias drift (tube heating variation)
            y += chaos_ * 0.012f * std::sin(slow_mod_ * 0.4f);
        }

        return std::clamp(y, -1.0f, 1.0f);
    }

    // ---- Per-model curve implementations ----

    float TubeSaturation::curveFLAT(float x)
    {
        // Linear passthrough — no saturation, no harmonic distortion
        return x;
    }

    float TubeSaturation::curveAX7(float x)
    {
        // 12AX7: High-gain preamp triode (mu ~100)
        // Strong asymmetry, rich 2nd harmonic, warm saturation
        float pos = std::tanh(x * 1.5f) * 0.8f + std::tanh(x * 3.0f) * 0.1f;
        float neg = std::tanh(x * 1.2f) * 0.9f;
        return (x >= 0.0f) ? pos : neg;
    }

    float TubeSaturation::curveAT7(float x)
    {
        // 12AT7: Medium-gain preamp triode (mu ~60)
        // sin() waveshaper (Jon Watte, musicdsp.org)
        constexpr float a = 0.8f;
        constexpr float z = 3.14159265f * a;
        float s = 1.0f / std::sin(z);
        float b = 1.0f / a;
        float y;
        if (std::fabs(x) > b)
            y = (x > 0.0f) ? 1.0f : -1.0f;
        else
            y = std::sin(z * x) * s;
        if (x < 0.0f)
            y *= 0.92f;
        return y;
    }

    float TubeSaturation::curveAU7(float x)
    {
        // 12AU7: Low-gain preamp triode (mu ~17)
        // Polynomial: (3/2)x - (1/2)x³ (classic musicdsp)
        float y = 1.5f * x - 0.5f * x * x * x;
        if (x < 0.0f)
            y *= 0.97f;
        return y;
    }

    float TubeSaturation::curveEL34(float x)
    {
        // EL34: Pentode power tube (British character)
        // Gloubi-boulga (Laurent de Soras)
        float scaled = x * 0.686306f;
        float a_coeff = 1.0f + std::exp(std::sqrt(std::fabs(scaled)) * -0.75f);
        float ex = std::exp(scaled);
        float emxa = std::exp(-scaled * a_coeff);
        float emx = std::exp(-scaled);
        float y = (ex - emxa) / (ex + emx);
        if (x < 0.0f)
            y *= 0.88f;
        return y;
    }

    float TubeSaturation::curveP6L6(float x)
    {
        // 6L6: Beam tetrode power tube (American character)
        // Tarrabia & de Jong: (1+k)*x/(1+k*|x|)
        constexpr float k = 1.6f;
        float y = (1.0f + k) * x / (1.0f + k * std::fabs(x));
        if (x < 0.0f)
            y *= 0.95f;
        return y;
    }

    float TubeSaturation::process(float input)
    {
        float driven = input * drive_;
        float wet = transferCurve(driven);

        // Compensate output gain (driven signal is louder)
        float compensation = 1.0f / std::max(drive_ * 0.6f, 1.0f);
        wet *= compensation;

        return input * (1.0f - mix_) + wet * mix_;
    }

    void TubeSaturation::processStereo(float *buffer, int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            buffer[i * 2] = process(buffer[i * 2]);
            buffer[i * 2 + 1] = process(buffer[i * 2 + 1]);
        }
    }

    void TubeSaturation::reset()
    {
        slow_mod_ = 0.0f;
    }

    void TubeSaturation::setSampleRate(float sample_rate)
    {
        sample_rate_ = sample_rate;
    }

    float TubeSaturation::getDrive() const { return drive_; }
    float TubeSaturation::getAsymmetry() const { return asymmetry_; }
    float TubeSaturation::getChaos() const { return chaos_; }
    float TubeSaturation::getMix() const { return mix_; }

    float TubeSaturation::plotTransferCurve(float x) const
    {
        x = std::clamp(x, -1.0f, 1.0f);
        float y = (const_cast<TubeSaturation *>(this)->*curve_fn_)(x);

        // Apply asymmetry
        y += asymmetry_ * y * y * 0.4f;

        // Show chaos contribution at current slow_mod_ snapshot (read-only)
        if (chaos_ > 0.0f)
        {
            float physical_variation = std::sin(slow_mod_) * 0.018f * chaos_ + std::sin(slow_mod_ * 3.7f) * 0.007f * chaos_;
            y += physical_variation * (y * y);
            y += chaos_ * 0.012f * std::sin(slow_mod_ * 0.4f);
        }

        return std::clamp(y, -1.0f, 1.0f);
    }

} // namespace synth
