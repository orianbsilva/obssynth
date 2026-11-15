#pragma once
// The pad engines beside CIRCUIT, from the synth's parts:
//   FmDrum      two operators (the Oscillator's FM), the index falling faster than the sound: metal, wood, bells
//   VaDrum      a saw/square (morphed) through a low-pass whose envelope falls: synthetic toms, zaps, basses
//   SampleDrum  a recording, one shot: pitch, where it starts, how long it lasts, a low-pass
//   GrainDrum   the synth's granular engine on a recording, under a decay
// Each takes the four knobs as DrumModel says. The recordings are GrainSource (read off the audio thread, shared,
// handed over as a pointer). FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/grain_engine.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/oscillator.hpp"

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <memory>

namespace synth
{
    class FmDrum : public DrumModel
    {
    public:
        explicit FmDrum(float sampleRate) : osc_(200.0f, WaveType::FM_SYNTHESIS, sampleRate), amp_(sampleRate), index_(sampleRate), pitch_(sampleRate)
        {
            osc_.setAmplitude(1.0f);
            osc_.setFMAlgorithm(FMAlgorithm::TWO_OP_SIMPLE);
            set(DrumParams{});
        }
        // TUNE 40..1200 Hz, DECAY 0.03..2 s, A RATIO 0.5..8 (the modulator's), B INDEX 0..12 at the strike
        void set(const DrumParams &n) override
        {
            p_ = n;
            hz_ = drumExp(n.tune, 40.0f, 1200.0f);
            decay_ = drumExp(n.decay, 0.03f, 2.0f);
            osc_.setFMModulatorRatio(drumExp(n.a, 0.5f, 8.0f) * (n.bent ? 1.414f : 1.0f));
            peakIndex_ = n.b * (n.bent ? 24.0f : 12.0f);
        }
        const char *nameA() const override { return "RATIO"; }
        const char *nameB() const override { return "INDEX"; }
        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.5f + 0.5f * s;
            strike_ = 0.6f + 0.4f * s;
            if (amp_.getCurrentAmplitude() < 1e-3f)
            {
                osc_.resetPhase();
                osc_.resetFMModulatorPhase();
            }
            amp_.setADSR(0.0003f, decay_, 0.0f, decay_);
            index_.setADSR(0.0f, decay_ * 0.35f, 0.0f, decay_ * 0.35f);
            pitch_.setADSR(0.0f, 0.02f, 0.0f, 0.02f);
            amp_.noteOn();
            index_.noteOn();
            pitch_.noteOn();
        }
        bool active() const override { return amp_.isActive(); }
        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            osc_.setFrequency(hz_ * std::exp2(0.5f * pitch_.getNextSample()));
            osc_.setFMModulationIndex(peakIndex_ * strike_ * index_.getNextSample());
            return 0.6f * osc_.getSample() * amp_.getNextSample() * level_;
        }

    private:
        DrumParams p_;
        Oscillator osc_;
        ADSREnvelope amp_, index_, pitch_;
        float hz_ = 200.0f, decay_ = 0.3f, peakIndex_ = 6.0f, level_ = 1.0f, strike_ = 1.0f;
    };

    class VaDrum : public DrumModel
    {
    public:
        explicit VaDrum(float sampleRate)
            : saw_(100.0f, WaveType::SAW, sampleRate), sq_(100.0f, WaveType::SQUARE, sampleRate), amp_(sampleRate), fenv_(sampleRate),
              pitch_(sampleRate), lp_(sampleRate, BiquadType::LOWPASS, 2000.0f, 0.4f)
        {
            saw_.setAmplitude(1.0f);
            sq_.setAmplitude(1.0f);
            set(DrumParams{});
        }
        // TUNE 30..1000 Hz, DECAY 0.03..2.5 s, A WAVE (saw to square), B FILTER (how far the envelope opens it)
        void set(const DrumParams &n) override
        {
            p_ = n;
            hz_ = drumExp(n.tune, 30.0f, 1000.0f);
            decay_ = drumExp(n.decay, 0.03f, 2.5f);
            lp_.setResonance(n.bent ? 0.85f : 0.4f);
        }
        const char *nameA() const override { return "WAVE"; }
        const char *nameB() const override { return "FILTER"; }
        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.5f + 0.5f * s;
            open_ = p_.b * (4.0f + 3.0f * s); // octaves above the note
            if (amp_.getCurrentAmplitude() < 1e-3f)
            {
                saw_.resetPhase();
                sq_.resetPhase();
            }
            amp_.setADSR(0.001f, decay_, 0.0f, decay_);
            fenv_.setADSR(0.0005f, decay_ * 0.5f, 0.0f, decay_ * 0.5f);
            pitch_.setADSR(0.0f, p_.bent ? 0.2f : 0.03f, 0.0f, 0.03f);
            amp_.noteOn();
            fenv_.noteOn();
            pitch_.noteOn();
        }
        bool active() const override { return amp_.isActive(); }
        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            const float hz = hz_ * std::exp2((p_.bent ? 2.0f : 0.5f) * pitch_.getNextSample());
            saw_.setFrequency(hz);
            sq_.setFrequency(hz);
            lp_.setCutoff(std::min(18000.0f, hz * 1.5f * std::exp2(open_ * fenv_.getNextSample())));
            const float x = saw_.getSample() * (1.0f - p_.a) + sq_.getSample() * p_.a;
            return 0.55f * lp_.process(x) * amp_.getNextSample() * level_;
        }

    private:
        DrumParams p_;
        Oscillator saw_, sq_;
        ADSREnvelope amp_, fenv_, pitch_;
        SvfFilter lp_;
        float hz_ = 100.0f, decay_ = 0.3f, level_ = 1.0f, open_ = 4.0f;
    };

    class SampleDrum : public DrumModel
    {
    public:
        explicit SampleDrum(float sampleRate) : sr_(sampleRate), amp_(sampleRate), lp_(sampleRate, BiquadType::LOWPASS, 20000.0f, 0.1f) { set(DrumParams{}); }
        void setSource(std::shared_ptr<const GrainSource> s) { src_ = std::move(s); } // (a pointer copy: no allocation)
        const GrainSource *source() const { return src_.get(); }
        // TUNE: ±2 octaves around the recording's own speed, DECAY: 0.03 s .. the whole recording, A START (into the
        // first 80%), B FILTER (a low-pass, 300 Hz .. open)
        void set(const DrumParams &n) override
        {
            p_ = n;
            rate_ = std::exp2((n.tune - 0.5f) * 4.0f) * (n.bent ? -1.0f : 1.0f); // bent: backwards
            lp_.setCutoff(drumExp(n.b, 300.0f, 20000.0f));
        }
        const char *nameA() const override { return "START"; }
        const char *nameB() const override { return "FILTER"; }
        void trigger(const DrumHit &hit) override
        {
            if (!src_ || src_->samples.size() < 2)
                return;
            const float s = hit.strength();
            level_ = 0.45f + 0.55f * s;
            const double n = static_cast<double>(src_->samples.size());
            pos_ = rate_ >= 0.0f ? static_cast<double>(p_.a) * 0.8 * n : n - 1.0 - static_cast<double>(p_.a) * 0.8 * n;
            const float len = static_cast<float>(n) / src_->rate;
            const float d = p_.decay >= 0.99f ? len * 4.0f : drumExp(p_.decay, 0.03f, std::max(0.05f, len));
            amp_.setADSR(0.0005f, d, 0.0f, d);
            amp_.noteOn();
            playing_ = true;
        }
        bool active() const override { return playing_ && amp_.isActive(); }
        float process() override
        {
            if (!active() || !src_)
                return 0.0f;
            const auto &v = src_->samples;
            const double n = static_cast<double>(v.size());
            if (pos_ < 0.0 || pos_ >= n - 1.0)
            {
                playing_ = false;
                return 0.0f;
            }
            const size_t i = static_cast<size_t>(pos_);
            const float f = static_cast<float>(pos_ - static_cast<double>(i));
            const float x = v[i] + (v[i + 1] - v[i]) * f;
            pos_ += static_cast<double>(rate_ * src_->rate / sr_);
            return 0.7f * lp_.process(x) * amp_.getNextSample() * level_;
        }

    private:
        float sr_;
        DrumParams p_;
        std::shared_ptr<const GrainSource> src_;
        ADSREnvelope amp_;
        SvfFilter lp_;
        double pos_ = 0.0;
        float rate_ = 1.0f, level_ = 1.0f;
        bool playing_ = false;
    };

    class GrainDrum : public DrumModel
    {
    public:
        explicit GrainDrum(float sampleRate) : amp_(sampleRate)
        {
            grains_.setSampleRate(sampleRate);
            grains_.setDensity(40.0f);
            set(DrumParams{});
        }
        void setSource(std::shared_ptr<const GrainSource> s) { grains_.setSource(std::move(s)); }
        void setPosition(float p) { grains_.setPosition(p); }
        // TUNE: ±2 octaves around the recording's root, DECAY 0.05..4 s, A SIZE (10..300 ms), B SPRAY
        void set(const DrumParams &n) override
        {
            p_ = n;
            shift_ = std::exp2((n.tune - 0.5f) * 4.0f);
            decay_ = drumExp(n.decay, 0.05f, 4.0f);
            grains_.setSize(drumExp(n.a, 0.01f, 0.3f));
            grains_.setSpray(n.b);
            grains_.setDensity(n.bent ? 120.0f : 40.0f);
        }
        const char *nameA() const override { return "SIZE"; }
        const char *nameB() const override { return "SPRAY"; }
        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.45f + 0.55f * s;
            grains_.noteOn(seed_ += 0x9E3779B9u, amp_.getCurrentAmplitude() < 1e-3f);
            amp_.setADSR(0.003f, decay_, 0.0f, decay_);
            amp_.noteOn();
        }
        bool active() const override { return amp_.isActive(); }
        float process() override
        {
            if (!amp_.isActive() || !grains_.hasSource())
                return 0.0f;
            const float root = grains_.source()->rootHz;
            return 0.6f * grains_.process(root * shift_) * amp_.getNextSample() * level_;
        }

    private:
        DrumParams p_;
        GrainEngine grains_;
        ADSREnvelope amp_;
        float shift_ = 1.0f, decay_ = 0.5f, level_ = 1.0f;
        uint32_t seed_ = 0x1234567u;
    };
} // namespace synth
