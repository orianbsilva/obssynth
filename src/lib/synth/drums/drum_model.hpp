#pragma once
// The shape every drum model takes, whatever its engine: four knobs 0..1 (TUNE, DECAY and the model's own A and B,
// named by the model) and "bent" (the same model past what its circuit allowed). The model maps them to its own
// units. A hit triggers it; process() gives one sample at the product's rate (2x oversampled in ObsDrums), mono.
// Nothing here allocates or locks: a model is built off the audio thread and only played on it. FLOAT-ONLY.
#include "drum_common.hpp"

#include <cmath>

namespace synth
{
    struct DrumParams
    {
        float tune = 0.5f, decay = 0.5f, a = 0.5f, b = 0.5f;
        bool bent = false;
    };

    class DrumModel
    {
    public:
        virtual ~DrumModel() = default;
        virtual void set(const DrumParams &p) = 0;
        virtual void trigger(const DrumHit &hit) = 0;
        virtual float process() = 0;
        virtual bool active() const = 0;
        virtual const char *nameA() const = 0; // the panel's labels for A and B
        virtual const char *nameB() const = 0;
    };

    // 0..1 to lo..hi on a log scale (frequencies, times)
    inline float drumExp(float n, float lo, float hi) { return lo * std::pow(hi / lo, n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n)); }
    inline float drumLin(float n, float lo, float hi) { return lo + (hi - lo) * (n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n)); }
} // namespace synth
