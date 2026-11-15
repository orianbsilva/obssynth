#include "audio_patch.hpp"
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <set>

namespace synth
{

    // AudioNode implementation
    AudioNode::AudioNode(const std::string &name, size_t inputs, size_t outputs)
        : name_(name), input_count_(inputs), output_count_(outputs), sample_rate_(48000.0f) {}

    void AudioNode::set_sample_rate(float sample_rate)
    {
        sample_rate_ = sample_rate;
    }

    void AudioNode::reset()
    {
        // Default implementation - subclasses can override
    }

    void AudioNode::set_parameter(const std::string &param, float value)
    {
        parameters_[param] = value;
    }

    float AudioNode::get_parameter(const std::string &param) const
    {
        auto it = parameters_.find(param);
        return (it != parameters_.end()) ? it->second : 0.0f;
    }

    std::vector<std::string> AudioNode::get_parameter_names() const
    {
        std::vector<std::string> names;
        for (const auto &pair : parameters_)
        {
            names.push_back(pair.first);
        }
        return names;
    }

    // AudioPatch implementation
    AudioPatch::AudioPatch(const std::string &name, float sample_rate)
        : name_(name), sample_rate_(sample_rate) {}

    void AudioPatch::add_node(std::unique_ptr<AudioNode> node)
    {
        if (!node)
        {
            throw std::invalid_argument("Cannot add null node to patch");
        }

        const std::string &node_name = node->name();
        if (nodes_.find(node_name) != nodes_.end())
        {
            throw std::invalid_argument("Node with name '" + node_name + "' already exists");
        }

        node->set_sample_rate(sample_rate_);
        nodes_[node_name] = std::move(node);

        std::cout << "Added node '" << node_name << "' to patch '" << name_ << "'" << std::endl;
    }

    void AudioPatch::remove_node(const std::string &name)
    {
        auto it = nodes_.find(name);
        if (it == nodes_.end())
        {
            throw std::invalid_argument("Node '" + name + "' not found in patch");
        }

        // Remove all connections involving this node
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                           [&name](const AudioConnection &conn)
                           {
                               return conn.from_node == name || conn.to_node == name;
                           }),
            connections_.end());

        nodes_.erase(it);
        std::cout << "Removed node '" << name << "' from patch '" << name_ << "'" << std::endl;
    }

    AudioNode *AudioPatch::get_node(const std::string &name)
    {
        auto it = nodes_.find(name);
        return (it != nodes_.end()) ? it->second.get() : nullptr;
    }

    void AudioPatch::connect(const std::string &from_node, size_t from_output,
                             const std::string &to_node, size_t to_input, float gain)
    {
        // Validate nodes exist
        if (nodes_.find(from_node) == nodes_.end())
        {
            throw std::invalid_argument("Source node '" + from_node + "' not found");
        }
        if (nodes_.find(to_node) == nodes_.end())
        {
            throw std::invalid_argument("Destination node '" + to_node + "' not found");
        }

        // Validate output/input indices
        if (from_output >= nodes_[from_node]->output_count())
        {
            throw std::invalid_argument("Invalid output index for node '" + from_node + "'");
        }
        if (to_input >= nodes_[to_node]->input_count())
        {
            throw std::invalid_argument("Invalid input index for node '" + to_node + "'");
        }

        connections_.emplace_back(from_node, from_output, to_node, to_input, gain, false);
        std::cout << "Connected " << from_node << "[" << from_output << "] -> "
                  << to_node << "[" << to_input << "] (gain: " << gain << ")" << std::endl;
    }

    void AudioPatch::connect_sidechain(const std::string &from_node, size_t from_output,
                                       const std::string &to_node, size_t to_input, float gain)
    {
        // Same validation as regular connect
        if (nodes_.find(from_node) == nodes_.end())
        {
            throw std::invalid_argument("Source node '" + from_node + "' not found");
        }
        if (nodes_.find(to_node) == nodes_.end())
        {
            throw std::invalid_argument("Destination node '" + to_node + "' not found");
        }

        if (from_output >= nodes_[from_node]->output_count())
        {
            throw std::invalid_argument("Invalid output index for node '" + from_node + "'");
        }
        if (to_input >= nodes_[to_node]->input_count())
        {
            throw std::invalid_argument("Invalid input index for node '" + to_node + "'");
        }

        connections_.emplace_back(from_node, from_output, to_node, to_input, gain, true);
        std::cout << "Sidechain connected " << from_node << "[" << from_output << "] ~> "
                  << to_node << "[" << to_input << "] (gain: " << gain << ")" << std::endl;
    }

    void AudioPatch::disconnect(const std::string &from_node, const std::string &to_node)
    {
        auto original_size = connections_.size();
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                           [&from_node, &to_node](const AudioConnection &conn)
                           {
                               return conn.from_node == from_node && conn.to_node == to_node;
                           }),
            connections_.end());

        if (connections_.size() < original_size)
        {
            std::cout << "Disconnected " << from_node << " -> " << to_node << std::endl;
        }
    }

    void AudioPatch::process(const float *input, float *output, size_t sample_count)
    {
        if (nodes_.empty())
        {
            // No processing - copy input to output
            std::copy(input, input + sample_count, output);
            return;
        }

        allocate_buffers(sample_count);
        clear_buffers();

        // Get processing order (topological sort)
        auto processing_order = get_processing_order();

        // Process nodes in order
        for (const auto &node_name : processing_order)
        {
            auto *node = nodes_[node_name].get();
            auto &input_buffers = node_buffers_[node_name];

            // Prepare input pointers
            std::vector<const float *> input_ptrs;
            for (size_t i = 0; i < node->input_count(); ++i)
            {
                input_ptrs.push_back(input_buffers[i].data());
            }

            // Prepare output pointers
            std::vector<float *> output_ptrs;
            for (size_t i = 0; i < node->output_count(); ++i)
            {
                output_ptrs.push_back(node_buffers_[node_name][node->input_count() + i].data());
            }

            // Process the node
            node->process(input_ptrs, output_ptrs, sample_count);
        }

        // Copy final output (assuming last node in processing order produces output)
        if (!processing_order.empty())
        {
            const auto &last_node = processing_order.back();
            auto &last_buffers = node_buffers_[last_node];
            if (!last_buffers.empty())
            {
                std::copy(last_buffers.back().begin(),
                          last_buffers.back().begin() + sample_count, output);
            }
        }
    }

    void AudioPatch::process_stereo(const float *left_in, const float *right_in,
                                    float *left_out, float *right_out, size_t sample_count)
    {
        // For now, process left and right channels separately
        // TODO: Implement proper stereo patch processing
        process(left_in, left_out, sample_count);
        process(right_in, right_out, sample_count);
    }

    void AudioPatch::set_sample_rate(float sample_rate)
    {
        sample_rate_ = sample_rate;
        for (auto &pair : nodes_)
        {
            pair.second->set_sample_rate(sample_rate);
        }
    }

    void AudioPatch::reset()
    {
        for (auto &pair : nodes_)
        {
            pair.second->reset();
        }
        clear_buffers();
    }

    std::vector<std::string> AudioPatch::get_node_names() const
    {
        std::vector<std::string> names;
        for (const auto &pair : nodes_)
        {
            names.push_back(pair.first);
        }
        return names;
    }

    std::vector<AudioConnection> AudioPatch::get_connections() const
    {
        return connections_;
    }

    void AudioPatch::set_node_parameter(const std::string &node_name,
                                        const std::string &param, float value)
    {
        auto *node = get_node(node_name);
        if (!node)
        {
            throw std::invalid_argument("Node '" + node_name + "' not found");
        }
        node->set_parameter(param, value);
    }

    float AudioPatch::get_node_parameter(const std::string &node_name,
                                         const std::string &param) const
    {
        auto it = nodes_.find(node_name);
        if (it == nodes_.end())
        {
            throw std::invalid_argument("Node '" + node_name + "' not found");
        }
        return it->second->get_parameter(param);
    }

    void AudioPatch::allocate_buffers(size_t sample_count)
    {
        if (temp_buffer_.size() < sample_count)
        {
            temp_buffer_.resize(sample_count);
        }

        for (const auto &pair : nodes_)
        {
            const auto &node_name = pair.first;
            const auto *node = pair.second.get();

            auto &buffers = node_buffers_[node_name];
            size_t total_buffers = node->input_count() + node->output_count();

            if (buffers.size() != total_buffers)
            {
                buffers.resize(total_buffers);
            }

            for (auto &buffer : buffers)
            {
                if (buffer.size() < sample_count)
                {
                    buffer.resize(sample_count);
                }
            }
        }
    }

    void AudioPatch::clear_buffers()
    {
        for (auto &node_pair : node_buffers_)
        {
            for (auto &buffer : node_pair.second)
            {
                std::fill(buffer.begin(), buffer.end(), 0.0f);
            }
        }
    }

    std::vector<std::string> AudioPatch::get_processing_order() const
    {
        // Simple topological sort for now
        // TODO: Implement proper dependency resolution
        std::vector<std::string> order;
        std::set<std::string> visited;

        for (const auto &pair : nodes_)
        {
            if (visited.find(pair.first) == visited.end())
            {
                order.push_back(pair.first);
                visited.insert(pair.first);
            }
        }

        return order;
    }

} // namespace synth
