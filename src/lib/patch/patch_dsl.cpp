#include "patch_dsl.hpp"
#include <cstddef>

namespace synth
{

    PatchDSL &PatchDSL::create(const std::string &name)
    {
        current_patch_ = std::make_unique<AudioPatch>(name);
        return *this;
    }

    PatchDSL &PatchDSL::add_oscillator(const std::string &name, const std::string &wave_type, float frequency)
    {

        WaveType type = WaveType::SINE;
        if (wave_type == "sine")
        {
            type = WaveType::SINE;
        }
        else if (wave_type == "square")
        {
            type = WaveType::SQUARE;
        }
        else if (wave_type == "sawtooth")
        {
            type = WaveType::SAW;
        }

        auto node = std::make_unique<OscillatorNode>(name, frequency, type);
        current_patch_->add_node(std::move(node));
        return *this;
    }

    PatchDSL &PatchDSL::add_gain(const std::string &name, float gain)
    {
        auto node = std::make_unique<GainNode>(name, gain);
        current_patch_->add_node(std::move(node));
        return *this;
    }

    PatchDSL &PatchDSL::add_filter(const std::string &name, const std::string &filter_type, float cutoff, float resonance)
    {
        if (filter_type == "lowpass")
        {
            auto node = std::make_unique<FilterNode>(name, cutoff, resonance, true);
            current_patch_->add_node(std::move(node));
        }
        else if (filter_type == "highpass")
        {
            auto node = std::make_unique<FilterNode>(name, cutoff, resonance, false);
            current_patch_->add_node(std::move(node));
        }
        return *this;
    }

    PatchDSL &PatchDSL::add_tube_reverb(const std::string &name, float tube_length, float reflection, float mix)
    {
        auto node = std::make_unique<TubeReverbNode>(name, tube_length, reflection, mix);
        current_patch_->add_node(std::move(node));
        return *this;
    }

    PatchDSL &PatchDSL::add_reverb(const std::string &name, const std::string &reverb_type, float room_size)
    {
        (void)reverb_type; // TODO: Use reverb_type to select different reverb algorithms
        // Convert reverb_type to appropriate decay value and use room_size as wet_level
        float decay = 0.5f;          // Default decay
        float wet_level = room_size; // Use room_size as wet_level
        auto node = std::make_unique<ReverbNode>(name, decay, wet_level);
        current_patch_->add_node(std::move(node));
        return *this;
    }

    PatchDSL &PatchDSL::add_mixer(const std::string &name)
    {
        auto node = std::make_unique<MixerNode>(name, 2, 1.0f); // 2 inputs by default
        current_patch_->add_node(std::move(node));
        return *this;
    }

    PatchDSL &PatchDSL::connect(const std::string &source, const std::string &dest, int input_index)
    {
        current_patch_->connect(source, 0, dest, input_index, 1.0f);
        return *this;
    }

    PatchDSL &PatchDSL::chain(const std::vector<std::string> &nodes)
    {
        // Connect nodes in sequence: nodes[0] -> nodes[1] -> nodes[2] -> ... -> nodes[n-1]
        for (size_t i = 0; i < nodes.size() - 1; ++i)
        {
            current_patch_->connect(nodes[i], 0, nodes[i + 1], 0, 1.0f);
        }
        return *this;
    }

    PatchDSL &PatchDSL::set_output(const std::string &node_name)
    {
        output_node_ = node_name;
        return *this;
    }

    std::unique_ptr<AudioPatch> PatchDSL::build()
    {
        // For now, just return the patch. Output handling would need additional design.
        return std::move(current_patch_);
    }

    // Convenience factory functions
    std::unique_ptr<AudioPatch> create_simple_synth(float frequency = 440.0f)
    {
        PatchDSL builder;
        return builder
            .create("simple_synth")
            .add_oscillator("osc", "sine", frequency)
            .add_gain("amp", 0.5f)
            .connect("osc", "amp")
            .set_output("amp")
            .build();
    }

} // namespace synth
