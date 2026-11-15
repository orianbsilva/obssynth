/**
 * @file synth.hpp
 * @brief Main include header for synth module
 *
 * Include this file to get access to all synth components:
 * - BiquadFilter
 * - EffectSlot
 * - LFO (from oscillators/)
 * - ADSREnvelope (from oscillators/)
 * - ModMatrix
 * - OscillatorChain
 * - Voice
 * - VoiceAllocator
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

// Core DSP components
#include "core/biquad_filter.hpp"
#include "core/effect_slot.hpp"

// Modulation system (generic, int-based)
#include "modulation/mod_matrix.hpp"

// Signal chains
#include "chains/osc_chain.hpp"
#include "chains/voice.hpp"
#include "chains/voice_allocator.hpp"

// High-level engine
#include "synth_engine.hpp"

// Re-export existing components for convenience
#include "../oscillators/lfo.hpp"
#include "../oscillators/adsr_envelope.hpp"
#include "../oscillators/oscillator.hpp"
