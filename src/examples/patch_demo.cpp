#include <iostream>
#include "../lib/patch/patch_system.hpp"
#include "../lib/utils/wav_writer.hpp"

using namespace synth;

int main()
{
    std::cout << "Audio Patch System Demo" << std::endl;
    std::cout << "======================" << std::endl;

    // Example 1: Simple oscillator patch using DSL
    std::cout << "\n1. Creating simple oscillator patch..." << std::endl;
    auto simple_patch = PatchDSL()
                            .create("simple_osc")
                            .add_oscillator("osc", "sine", 440.0f)
                            .add_gain("volume", 0.3f)
                            .connect("osc", "volume")
                            .build();

    patch_utils::print_patch_info(*simple_patch);

    // Example 4: Manual patch construction
    std::cout << "\n4. Manual patch construction..." << std::endl;
    auto manual_patch = std::make_unique<AudioPatch>("manual", 48000.0f);

    // Add nodes manually
    manual_patch->add_node(std::make_unique<OscillatorNode>("osc1", 220.0f, WaveType::SAW));
    manual_patch->add_node(std::make_unique<FilterNode>("filter", 1000.0f, 0.7f, true));
    manual_patch->add_node(std::make_unique<ReverbNode>("reverb"));
    manual_patch->add_node(std::make_unique<GainNode>("output", 0.5f));

    // Connect manually
    manual_patch->connect("osc1", 0, "filter", 0);
    manual_patch->connect("filter", 0, "reverb", 0);
    manual_patch->connect("reverb", 0, "output", 0);

    // Set parameters
    manual_patch->set_node_parameter("filter", "cutoff", 800.0f);
    manual_patch->set_node_parameter("reverb", "room_size", 0.7f);

    patch_utils::print_patch_info(*manual_patch);

    std::cout << "\n6. DSL chaining example..." << std::endl;
    auto chained_patch = PatchDSL()
                             .create("chained")
                             .add_oscillator("bass", "sine", 55.0f)
                             .add_oscillator("lead", "square", 440.0f)
                             .add_mixer("mix")
                             .add_filter("master_filter", "lowpass", 5000.0f, 0.7f)
                             .add_reverb("master_reverb", "hall", 0.5f)
                             .add_gain("master_out", 0.8f)
                             .connect("bass", "mix", 0)
                             .connect("lead", "mix", 1)
                             .chain({"mix", "master_filter", "master_reverb", "master_out"})
                             // TODO: Implement parameter setting functionality
                             // .set("mix.gain_0", 0.8f)    // Bass level
                             // .set("mix.gain_1", 0.6f)    // Lead level
                             // .set("master_filter.cutoff", 3000.0f)
                             // .set("master_reverb.wet_level", 0.3f)
                             .build();

    patch_utils::print_patch_info(*chained_patch);

    std::cout << "\nPatch system demo completed successfully!" << std::endl;

    return 0;
}
