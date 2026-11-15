#pragma once
// A sample player: a recording (GrainSource, shared and read-only) played from a start position at the note's pitch
// (the recording's own speed at its root), once or looped from the start to the end. Linear interpolation. The
// position is an integer and a fraction (float only: a float alone loses the fraction in a long recording). No
// allocation, no lock. FLOAT-ONLY.
#include "grain_engine.hpp" // GrainSource

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace synth
{
    class SamplePlayer
    {
    public:
        void setSampleRate(float sr) { sr_ = sr; }
        void setSource(std::shared_ptr<const GrainSource> s) { src_ = std::move(s); }
        bool hasSource() const { return src_ && src_->samples.size() > 4; }
        void setStart(float p) { start_ = std::clamp(p, 0.0f, 1.0f); } // 0..1 through the recording
        void setLoop(bool on) { loop_ = on; }
        void setRoot(float midiNote) { rootHz_ = 440.0f * std::exp2((midiNote - 69.0f) / 12.0f); } // the pitch it plays at its own speed
        void useSourceRoot() { rootHz_ = 0.0f; }

        // a note begins: from the start position
        void noteOn()
        {
            if (!hasSource())
                return;
            idx_ = static_cast<uint32_t>(start_ * static_cast<float>(src_->samples.size() - 1));
            frac_ = 0.0f;
            playing_ = true;
        }

        // one output sample; hz: the note's frequency now (pitch modulation and drift included)
        float process(float hz)
        {
            if (!playing_ || !hasSource())
                return 0.0f;
            const std::vector<float> &s = src_->samples;
            const uint32_t n = static_cast<uint32_t>(s.size());
            const float root = rootHz_ > 0.0f ? rootHz_ : src_->rootHz;
            const float a = s[idx_], b = s[idx_ + 1 < n ? idx_ + 1 : (loop_ ? 0 : idx_)];
            const float out = a + (b - a) * frac_;
            frac_ += hz / root * src_->rate / sr_; // source samples per output sample
            const float whole = std::floor(frac_);
            frac_ -= whole;
            idx_ += static_cast<uint32_t>(whole);
            if (idx_ >= n)
            {
                if (loop_)
                    idx_ %= n;
                else
                    playing_ = false;
            }
            return out;
        }

    private:
        std::shared_ptr<const GrainSource> src_;
        float sr_ = 48000.0f, start_ = 0.0f, rootHz_ = 0.0f, frac_ = 0.0f;
        uint32_t idx_ = 0;
        bool loop_ = false, playing_ = false;
    };
} // namespace synth
