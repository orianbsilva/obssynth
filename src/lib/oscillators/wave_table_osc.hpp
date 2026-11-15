/**
 * @file wave_table_osc.hpp
 * @brief WaveTable oscillator with multi-table morphing
 *
 * Wraps PCMWaveform::loadWavBank() to provide a wavetable oscillator
 * with smooth cross-fade between adjacent tables driven by a morph
 * position.  All math is float-only.
 */

#pragma once

#include "pcm_waveform.hpp"
#include <cstddef>
#include <memory>
#include <string>

namespace synth
{
    /**
     * @brief Wavetable oscillator — loads a multi-cycle WAV bank and
     *        provides smooth morphing between adjacent tables.
     *
     * Usage:
     *   1. loadBank("bank.wav", 2048)   — load file, 2048 samples/table
     *   2. setMorphPosition(pos)        — 0 = first table, tableCount-1 = last
     *   3. getSample(normalizedPhase)   — returns cross-faded sample [0..1) phase
     */
    class WaveTableOsc
    {
    public:
        WaveTableOsc() = default;
        ~WaveTableOsc() = default;

        /**
         * @brief Load a multi-cycle WAV file as a wavetable bank.
         * @param path          Path to .wav file
         * @param samplesPerWave Samples per single-cycle table (default 2048)
         * @return true on success
         */
        bool loadBank(const std::string &path, size_t samplesPerWave = 0)
        {
            if (!waveform_)
                waveform_ = std::make_shared<PCMWaveform>();
            if (!waveform_->loadWavBank(path, 0, samplesPerWave))
                return false;
            morph_pos_ = 0.0f;
            return true;
        }

        /**
         * @brief Set the morph position between tables.
         * @param pos  [0, tableCount-1]; clamped automatically.
         */
        void setMorphPosition(float pos)
        {
            if (!waveform_ || !waveform_->isWavBank())
                return;
            float maxPos = static_cast<float>(waveform_->getBankWaveCount() - 1);
            if (maxPos <= 0.0f)
            {
                morph_pos_ = 0.0f;
                return;
            }
            pos = pos < 0.0f ? 0.0f : (pos > maxPos ? maxPos : pos);
            morph_pos_ = pos;
            // PCMWaveform's bank position: 0..1 over the whole bank
            waveform_->setBankPosition(morph_pos_ / maxPos);
        }

        /**
         * @brief Get morphed sample at normalized phase.
         * @param normalizedPhase  [0, 1) — wraps automatically
         * @return interpolated sample
         */
        float getSample(float normalizedPhase) const
        {
            if (!waveform_ || !waveform_->isLoaded())
                return 0.0f;
            return waveform_->getSample(normalizedPhase);
        }

        /** Number of tables in the bank (0 if not loaded). */
        int tableCount() const
        {
            if (!waveform_ || !waveform_->isWavBank())
                return waveform_ && waveform_->isLoaded() ? 1 : 0;
            return static_cast<int>(waveform_->getBankWaveCount());
        }

        bool isLoaded() const { return waveform_ && waveform_->isLoaded(); }

        float getMorphPosition() const { return morph_pos_; }

        /** Direct access to raw PCMWaveform for viewer / visualisation (read-only). */
        const PCMWaveform *getWaveform() const { return waveform_.get(); }

        const std::string &getFilename() const
        {
            static const std::string empty;
            return waveform_ ? waveform_->getFilename() : empty;
        }

    private:
        std::shared_ptr<PCMWaveform> waveform_;
        float morph_pos_ = 0.0f;
    };

} // namespace synth
