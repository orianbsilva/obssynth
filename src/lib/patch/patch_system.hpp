#pragma once

/**
 * @file patch_system.hpp
 * @brief Main header for the audio patch system
 *
 * This header provides a unified interface to the patch system,
 * including all necessary components for creating and managing
 * audio processing pipelines.
 */

// Core patch system
#include "audio_patch.hpp"
#include "patch_dsl.hpp"
#include "patch_nodes.hpp"

namespace synth {

/**
 * @brief Convenience typedef for patch pointer
 */
using PatchPtr = std::unique_ptr<AudioPatch>;

/**
 * @brief Utility functions for patch management
 */
namespace patch_utils {

/**
 * @brief Print patch information for debugging
 */
  void print_patch_info(const AudioPatch& patch) {
    std::cout << "Patch: " << patch.name() << std::endl;
    std::cout << "Nodes:" << std::endl;

    auto node_names = patch.get_node_names();
    for (const auto& name : node_names) {
        std::cout << "  - " << name << std::endl;
    }

    std::cout << "Connections:" << std::endl;
    auto connections = patch.get_connections();
    for (const auto& conn : connections) {
        std::cout << "  " << conn.from_node << "[" << conn.from_output << "]";
        if (conn.is_sidechain) {
            std::cout << " ~> ";
        } else {
            std::cout << " -> ";
        }
        std::cout << conn.to_node << "[" << conn.to_input << "]";
        if (conn.gain != 1.0f) {
            std::cout << " (gain: " << conn.gain << ")";
        }
        std::cout << std::endl;
    }
}

} // namespace patch_utils

} // namespace synth
