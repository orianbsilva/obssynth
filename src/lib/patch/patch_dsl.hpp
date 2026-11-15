/**
 * @file patch_dsl.hpp
 * @brief Domain Specific Language for audio patch creation
 *
 * Provides a fluent API for creating complex audio synthesis patches
 * using simple method chaining. Simplifies patch construction compared
 * to manual node management and connection setup.
 *
 * Features:
 * - Fluent method chaining interface
 * - Built-in node types (oscillators, filters, effects)
 * - Automatic connection management
 * - Predefined patch templates
 * - Error-safe patch construction
 *
 * @code
 * auto patch = PatchDSL()
 *     .create("my_synth")
 *     .add_oscillator("osc1", "sine", 440.0f)
 *     .add_filter("filter1", "lowpass", 2000.0f, 0.8f)
 *     .add_reverb("reverb1", "hall", 0.6f)
 *     .chain({"osc1", "filter1", "reverb1"})
 *     .set_output("reverb1")
 *     .build();
 * @endcode
 */

#pragma once

#include "audio_patch.hpp"
#include "patch_nodes.hpp"

namespace synth
{

    /**
     * @brief Domain Specific Language for simplified patch creation
     *
     * Provides a fluent API for creating audio synthesis patches without
     * manual node management. Supports method chaining for readable patch
     * construction and automatic error handling.
     *
     * Key benefits:
     * - Simplified syntax compared to manual AudioPatch manipulation
     * - Automatic node type selection and configuration
     * - Built-in connection validation
     * - Predefined parameter defaults for quick prototyping
     * - Method chaining for readable code
     *
     * @code
     * // Create a simple synthesizer patch
     * auto synth = PatchDSL()
     *     .create("basic_synth")
     *     .add_oscillator("osc", "sawtooth", 220.0f)
     *     .add_filter("lpf", "lowpass", 1500.0f)
     *     .add_gain("volume", 0.8f)
     *     .chain({"osc", "lpf", "volume"})
     *     .set_output("volume")
     *     .build();
     *
     * // Create complex patch with multiple oscillators
     * auto complex = PatchDSL()
     *     .create("dual_osc")
     *     .add_oscillator("osc1", "sine", 440.0f)
     *     .add_oscillator("osc2", "square", 441.0f)  // Slight detune
     *     .add_mixer("mix")
     *     .add_reverb("space", "hall", 0.4f)
     *     .connect("osc1", "mix", 0)
     *     .connect("osc2", "mix", 1)
     *     .connect("mix", "space")
     *     .set_output("space")
     *     .build();
     * @endcode
     *
     * @warning build() transfers ownership - DSL object becomes invalid after use
     * @see AudioPatch for low-level patch management
     * @see patch_nodes.hpp for available node types
     */
    class PatchDSL
    {
    private:
        std::unique_ptr<AudioPatch> current_patch_; ///< Current patch being built
        std::string output_node_;                   ///< Name of designated output node

    public:
        /**
         * @brief Default constructor
         */
        PatchDSL() = default;

        // Start building a new patch

        /**
         * @brief Start building a new patch with given name
         * @param name Patch name for identification
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &create(const std::string &name);

        // Add nodes (only working ones)

        /**
         * @brief Add oscillator node to patch
         * @param name Unique node name
         * @param wave_type Waveform type ("sine", "square", "sawtooth", "triangle")
         * @param frequency Initial frequency in Hz (default 440.0f)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_oscillator(const std::string &name, const std::string &wave_type = "sine", float frequency = 440.0f);

        /**
         * @brief Add gain/amplifier node to patch
         * @param name Unique node name
         * @param gain Initial gain value (1.0f = unity, default 1.0f)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_gain(const std::string &name, float gain = 1.0f);

        /**
         * @brief Add filter node to patch
         * @param name Unique node name
         * @param filter_type Filter type ("lowpass", "highpass", "bandpass")
         * @param cutoff Cutoff frequency in Hz (default 1000.0f)
         * @param resonance Filter resonance (0.0f to 1.0f, default 0.7f)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_filter(const std::string &name, const std::string &filter_type = "lowpass", float cutoff = 1000.0f, float resonance = 0.7f);

        /**
         * @brief Add tube reverb effect node to patch (physical model waveguide)
         * @param name        Unique node name
         * @param tube_length P1: tube length in metres [0.3 – 6.0] (default 1.2f)
         * @param reflection  P2: reflection coefficient [0.5 – 0.99] (default 0.92f)
         * @param mix         P3: dry/wet blend [0.0 – 1.0] (default 0.5f)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_tube_reverb(const std::string &name,
                                  float tube_length = 1.2f,
                                  float reflection = 0.92f,
                                  float mix = 0.5f);

        /**
         * @brief Add reverb effect node to patch
         * @param name Unique node name
         * @param reverb_type Reverb type ("hall", "room", "plate", "spring")
         * @param room_size Room size parameter (0.0f to 1.0f, default 0.5f)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_reverb(const std::string &name, const std::string &reverb_type = "hall", float room_size = 0.5f);

        /**
         * @brief Add mixer node to patch
         * @param name Unique node name
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &add_mixer(const std::string &name);

        // Connect nodes

        /**
         * @brief Connect two nodes in the patch
         * @param source Source node name
         * @param dest Destination node name
         * @param input_index Input channel index on destination (default 0)
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &connect(const std::string &source, const std::string &dest, int input_index = 0);

        /**
         * @brief Chain multiple nodes in sequence
         * @param nodes Vector of node names in connection order
         * @return Reference to this DSL for method chaining
         *
         * Connects nodes[0] -> nodes[1] -> nodes[2] -> ... in sequence.
         * Equivalent to multiple connect() calls but more readable.
         */
        PatchDSL &chain(const std::vector<std::string> &nodes);

        // Set output

        /**
         * @brief Set which node provides the patch output
         * @param node_name Name of node to use as output
         * @return Reference to this DSL for method chaining
         */
        PatchDSL &set_output(const std::string &node_name);

        // Build the final patch

        /**
         * @brief Build and return the completed patch
         * @return Unique pointer to constructed AudioPatch
         *
         * @warning DSL object becomes invalid after calling build()
         * @warning Returns nullptr if patch construction failed
         */
        std::unique_ptr<AudioPatch> build();
    };

    /**
     * @brief Create a simple predefined synthesizer patch
     * @return Complete synthesizer patch ready for use
     *
     * Creates a basic synthesizer with oscillator, filter, and reverb
     * suitable for immediate use without DSL configuration.
     */
    std::unique_ptr<AudioPatch> create_simple_synth();

} // namespace synth
