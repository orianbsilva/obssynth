/**
 * @file audio_patch.hpp
 * @brief Audio patch system for modular synthesis
 *
 * Provides node-based audio processing framework for creating complex synthesis
 * patches. Supports real-time audio processing with configurable routing,
 * parameter automation, and dynamic patch construction.
 *
 * Features:
 * - Modular audio node architecture
 * - Dynamic patch connection and routing
 * - Real-time parameter control
 * - Buffer-based audio processing
 * - Thread-safe patch modifications
 *
 * @code
 * auto oscillator = std::make_shared<OscillatorNode>("osc1");
 * auto filter = std::make_shared<FilterNode>("filter1");
 * auto patch = std::make_shared<AudioPatch>("main_patch");
 * patch->add_node(oscillator);
 * patch->add_node(filter);
 * patch->connect(oscillator, 0, filter, 0);
 * @endcode
 */

#pragma once

#include <cstddef>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <functional>
#include "../math/synt_math.hpp"

namespace synth
{

    /**
     * @brief Base class for all audio processing nodes in the patch system
     *
     * AudioNode represents a single unit in an audio processing pipeline.
     * Each node can have multiple inputs and outputs, and processes audio
     * in real-time with configurable buffer sizes.
     *
     * Key features:
     * - Multiple input/output support
     * - Parameter automation system
     * - Sample rate awareness
     * - Reset functionality for state clearing
     * - Virtual interface for easy subclassing
     *
     * @code
     * class MyEffectNode : public AudioNode {
     * public:
     *     MyEffectNode() : AudioNode("my_effect", 1, 1) {}
     *
     *     void process(const std::vector<const float*>& inputs,
     *                  std::vector<float*>& outputs,
     *                  size_t sample_count) override {
     *         // Process audio here
     *     }
     * };
     * @endcode
     *
     * @warning All subclasses must implement the process() method
     * @see AudioPatch for patch management and node connections
     */
    class AudioNode
    {
    public:
        /**
         * @brief Construct a new AudioNode
         * @param name Unique name for this node
         * @param inputs Number of input channels (default 1)
         * @param outputs Number of output channels (default 1)
         */
        AudioNode(const std::string &name, size_t inputs = 1, size_t outputs = 1);

        /**
         * @brief Virtual destructor for proper cleanup
         */
        virtual ~AudioNode() = default;

        // Core processing interface

        /**
         * @brief Process audio samples through this node
         * @param inputs Array of input buffer pointers
         * @param outputs Array of output buffer pointers
         * @param sample_count Number of samples to process
         *
         * @warning Input and output buffers must have at least sample_count elements
         * @warning This method must be implemented by all subclasses
         */
        virtual void process(const std::vector<const float *> &inputs,
                             std::vector<float *> &outputs,
                             size_t sample_count) = 0;

        // Configuration

        /**
         * @brief Set the sample rate for this node
         * @param sample_rate Sample rate in Hz (e.g., 44100.0f)
         */
        virtual void set_sample_rate(float sample_rate);

        /**
         * @brief Reset internal state (clear delays, filters, etc.)
         */
        virtual void reset();

        // Connection management

        /**
         * @brief Get node name
         * @return Node name string
         */
        const std::string &name() const { return name_; }

        /**
         * @brief Get number of input channels
         * @return Input channel count
         */
        size_t input_count() const { return input_count_; }

        /**
         * @brief Get number of output channels
         * @return Output channel count
         */
        size_t output_count() const { return output_count_; }

        // Parameter system

        /**
         * @brief Set parameter value
         * @param param Parameter name
         * @param value Parameter value
         */
        virtual void set_parameter(const std::string &param, float value);

        /**
         * @brief Get parameter value
         * @param param Parameter name
         * @return Parameter value, or 0.0f if parameter not found
         */
        virtual float get_parameter(const std::string &param) const;

        /**
         * @brief Get list of all parameter names
         * @return Vector of parameter name strings
         */
        virtual std::vector<std::string> get_parameter_names() const;

    protected:
        std::string name_;                        ///< Node name for identification
        size_t input_count_;                      ///< Number of input channels
        size_t output_count_;                     ///< Number of output channels
        float sample_rate_;                       ///< Current sample rate in Hz
        std::map<std::string, float> parameters_; ///< Node parameters
    };

    /**
     * @brief Connection between audio nodes
     *
     * Represents a signal path from one node's output to another node's input.
     * Supports both direct connections and side-chain connections with
     * configurable gain control.
     *
     * @code
     * AudioConnection conn("oscillator", 0, "filter", 0, 1.0f, false);
     * patch.add_connection(conn);
     * @endcode
     */
    struct AudioConnection
    {
        std::string from_node; ///< Source node name
        size_t from_output;    ///< Source output channel index
        std::string to_node;   ///< Destination node name
        size_t to_input;       ///< Destination input channel index
        float gain;            ///< Connection gain multiplier
        bool is_sidechain;     ///< True for side-chain connections

        /**
         * @brief Construct a new AudioConnection
         * @param from_node Source node name
         * @param from_output Source output channel (0-based)
         * @param to_node Destination node name
         * @param to_input Destination input channel (0-based)
         * @param gain Connection gain (default 1.0f)
         * @param is_sidechain Side-chain flag (default false)
         */
        AudioConnection(const std::string &from_node, size_t from_output,
                        const std::string &to_node, size_t to_input,
                        float gain = 1.0f, bool is_sidechain = false)
            : from_node(from_node), from_output(from_output), to_node(to_node), to_input(to_input), gain(gain), is_sidechain(is_sidechain) {}
    };

    /**
     * @brief Audio patch that manages a network of interconnected audio nodes
     *
     * The AudioPatch class represents a complete audio processing pipeline.
     * It manages node lifecycle, handles routing between nodes, and provides
     * a high-level interface for patch configuration via DSL.
     *
     * Key features:
     * - Dynamic node addition/removal
     * - Automatic connection management
     * - Real-time audio processing
     * - Parameter automation
     * - Patch save/load functionality
     * - Thread-safe operation
     *
     * @code
     * auto patch = std::make_unique<AudioPatch>("my_patch", 44100.0f);
     * auto osc = std::make_unique<OscillatorNode>("osc1");
     * auto filter = std::make_unique<FilterNode>("filter1");
     *
     * patch->add_node(std::move(osc));
     * patch->add_node(std::move(filter));
     * patch->connect("osc1", 0, "filter1", 0);
     *
     * // Process audio
     * std::vector<float> output(512);
     * patch->process_to_output(output.data(), 512);
     * @endcode
     *
     * @warning Nodes must be added before creating connections
     * @see AudioNode for creating custom processing nodes
     */
    class AudioPatch
    {
    public:
        /**
         * @brief Construct a new AudioPatch
         * @param name Patch name for identification
         * @param sample_rate Sample rate in Hz (default 48000.0f)
         */
        AudioPatch(const std::string &name, float sample_rate = 48000.0f);

        /**
         * @brief Destructor - automatically cleans up nodes and connections
         */
        ~AudioPatch() = default;

        // Node management

        /**
         * @brief Add audio node to the patch
         * @param node Unique pointer to audio node (ownership transferred)
         */
        void add_node(std::unique_ptr<AudioNode> node);

        /**
         * @brief Remove node from patch by name
         * @param name Node name to remove
         */
        void remove_node(const std::string &name);

        /**
         * @brief Get pointer to node by name
         * @param name Node name to find
         * @return Pointer to node, or nullptr if not found
         */
        AudioNode *get_node(const std::string &name);

        // Connection management

        /**
         * @brief Connect output of one node to input of another
         * @param from_node Source node name
         * @param from_output Source output channel (0-based)
         * @param to_node Destination node name
         * @param to_input Destination input channel (0-based)
         * @param gain Connection gain multiplier (default 1.0f)
         */
        void connect(const std::string &from_node, size_t from_output,
                     const std::string &to_node, size_t to_input,
                     float gain = 1.0f);

        /**
         * @brief Create side-chain connection between nodes
         * @param from_node Source node name
         * @param from_output Source output channel (0-based)
         * @param to_node Destination node name
         * @param to_input Destination input channel (0-based)
         * @param gain Connection gain multiplier (default 1.0f)
         */
        void connect_sidechain(const std::string &from_node, size_t from_output,
                               const std::string &to_node, size_t to_input,
                               float gain = 1.0f);

        /**
         * @brief Disconnect all connections between two nodes
         * @param from_node Source node name
         * @param to_node Destination node name
         */
        void disconnect(const std::string &from_node, const std::string &to_node);

        // Processing

        /**
         * @brief Process mono audio through the patch
         * @param input Input audio buffer
         * @param output Output audio buffer
         * @param sample_count Number of samples to process
         */
        void process(const float *input, float *output, size_t sample_count);

        /**
         * @brief Process stereo audio through the patch
         * @param left_in Left channel input buffer
         * @param right_in Right channel input buffer
         * @param left_out Left channel output buffer
         * @param right_out Right channel output buffer
         * @param sample_count Number of samples to process
         */
        void process_stereo(const float *left_in, const float *right_in,
                            float *left_out, float *right_out, size_t sample_count);

        // Configuration

        /**
         * @brief Set sample rate for all nodes in patch
         * @param sample_rate New sample rate in Hz
         */
        void set_sample_rate(float sample_rate);

        /**
         * @brief Reset all nodes in the patch
         */
        void reset();

        // Patch introspection

        /**
         * @brief Get patch name
         * @return Patch name string
         */
        const std::string &name() const { return name_; }

        /**
         * @brief Get list of all node names in patch
         * @return Vector of node name strings
         */
        std::vector<std::string> get_node_names() const;

        /**
         * @brief Get list of all connections in patch
         * @return Vector of AudioConnection objects
         */
        std::vector<AudioConnection> get_connections() const;

        // Parameter access

        /**
         * @brief Set parameter value for specific node
         * @param node_name Target node name
         * @param param Parameter name
         * @param value Parameter value
         */
        void set_node_parameter(const std::string &node_name,
                                const std::string &param, float value);

        /**
         * @brief Get parameter value from specific node
         * @param node_name Target node name
         * @param param Parameter name
         * @return Parameter value, or 0.0f if not found
         */
        float get_node_parameter(const std::string &node_name,
                                 const std::string &param) const;

    private:
        std::string name_;
        float sample_rate_;
        std::map<std::string, std::unique_ptr<AudioNode>> nodes_;
        std::vector<AudioConnection> connections_;

        // Internal processing buffers
        std::map<std::string, std::vector<std::vector<float>>> node_buffers_;
        std::vector<float> temp_buffer_;

        void allocate_buffers(size_t sample_count);
        void clear_buffers();
        std::vector<std::string> get_processing_order() const;
    };

} // namespace synth
