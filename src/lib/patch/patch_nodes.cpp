#include "patch_nodes.hpp"
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace synth
{

    // OscillatorNode implementation
    OscillatorNode::OscillatorNode(const std::string &name, float frequency, WaveType wave_type)
        : AudioNode(name, 0, 1), oscillator_(frequency, wave_type, 48000.0f)
    {

        set_parameter("frequency", frequency);
        set_parameter("wave_type", static_cast<float>(wave_type));
    }

    void OscillatorNode::process(const std::vector<const float *> &inputs,
                                 std::vector<float *> &outputs,
                                 size_t sample_count)
    {
        (void)inputs; // Oscillator has no inputs
        if (outputs.empty())
            return;

        for (size_t i = 0; i < sample_count; ++i)
        {
            outputs[0][i] = oscillator_.getSample();
        }
    }

    void OscillatorNode::set_sample_rate(float sample_rate)
    {
        AudioNode::set_sample_rate(sample_rate);
        oscillator_ = Oscillator(oscillator_.getFrequency(), oscillator_.getWaveType(), sample_rate);
    }

    void OscillatorNode::set_frequency(float frequency)
    {
        oscillator_.setFrequency(frequency);
        set_parameter("frequency", frequency);
    }

    void OscillatorNode::set_wave_type(WaveType wave_type)
    {
        oscillator_.setWaveType(wave_type);
        set_parameter("wave_type", static_cast<float>(wave_type));
    }

    // Simple GainNode implementation
    GainNode::GainNode(const std::string &name, float gain)
        : AudioNode(name, 1, 1), gain_effect_(gain)
    {
        set_parameter("gain", gain);
    }

    void GainNode::process(const std::vector<const float *> &inputs,
                           std::vector<float *> &outputs,
                           size_t sample_count)
    {
        if (inputs.empty() || outputs.empty() || !inputs[0] || !outputs[0])
            return;

        gain_effect_.process(inputs[0], outputs[0], sample_count);
    }

    void GainNode::set_gain(float gain)
    {
        gain_effect_.set_gain(gain);
        set_parameter("gain", gain);
    }

    void FilterNode::process(const std::vector<const float *> &inputs,
                             std::vector<float *> &outputs,
                             size_t sample_count)
    {
        if (inputs.empty() || outputs.empty() || !inputs[0] || !outputs[0])
            return;

        filter_.process(inputs[0], outputs[0], sample_count);
    }

    void MixerNode::process(const std::vector<const float *> &inputs,
                            std::vector<float *> &outputs,
                            size_t sample_count)
    {
        if (outputs.empty())
            return;

        mixer_.process(inputs, outputs[0], sample_count);
    }

} // namespace synth
