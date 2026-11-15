/**
 * @file pcm_waveform.hpp
 * @brief PCM waveform storage and playback class
 *
 * Provides functionality for loading and playing back custom PCM waveforms
 * from WAV files. Supports interpolation for smooth playback at different
 * frequencies and handles single-cycle waveform loops.
 */

#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <memory>

namespace synth
{
    /**
     * @brief PCM waveform storage and playback class
     *
     * Manages custom waveforms loaded from PCM data, typically from WAV files.
     * Provides interpolated sample access for smooth playback at any frequency.
     * Designed for single-cycle waveforms that loop seamlessly.
     * Also supports wav banks containing multiple waveforms.
     */
    class PCMWaveform
    {
    private:
        std::vector<float> samples_;
        size_t sample_count_;
        bool is_loaded_;
        std::string filename_;
        float sample_rate_;

        // Wav bank support
        bool is_wav_bank_;
        size_t bank_wave_count_;
        size_t samples_per_wave_;
        float current_bank_position_float_;

    public:
        /**
         * @brief Construct a new PCMWaveform object
         */
        PCMWaveform();

        /**
         * @brief Construct a new PCMWaveform object with data
         * @param samples PCM sample data for one complete waveform cycle
         */
        PCMWaveform(const std::vector<float> &samples);

        /**
         * @brief Load waveform from a WAV file
         * @param filename Path to WAV file containing single-cycle waveform
         * @return True if loaded successfully, false otherwise
         */
        bool loadFromFile(const std::string &filename);

        /**
         * @brief Load wav bank from a WAV file
         * @param filename Path to WAV file containing multiple concatenated waveforms
         * @param wave_count Number of waveforms in the bank (default auto-detect)
         * @param samples_per_wave Number of samples per waveform (default 1024)
         * @return True if loaded successfully, false otherwise
         */
        bool loadWavBank(const std::string &filename, size_t wave_count = 0, size_t samples_per_wave = 0);

        /**
         * @brief Set waveform data directly
         * @param samples PCM sample data for one complete waveform cycle
         */
        void setData(const std::vector<float> &samples);

        /**
         * @brief Get interpolated sample at normalized phase position
         * @param normalized_phase Phase position (0.0 to 1.0) within the waveform cycle
         * @return Interpolated sample value
         */
        float getSample(float normalized_phase) const;

        /**
         * @brief Get the current bank position (0 to wave_count - 1)
         * @return Current bank position
         */
        int getScaledBankPosition() const;

        /**
         * @brief Set current bank position for wav banks
         * @param position Bank position (0.0 to 1.0, automatically scaled to wave count)
         */
        void setBankPosition(float position);

        /**
         * @brief Get current bank position
         * @return Current bank position (0.0 to 1.0)
         */
        float getBankPosition() const;

        /**
         * @brief Check if this is a wav bank
         * @return True if loaded as wav bank
         */
        bool isWavBank() const;

        /**
         * @brief Get number of waves in bank
         * @return Wave count (1 for single waveforms)
         */
        size_t getBankWaveCount() const;

    private:
        void dropSilentTail(); // the bank's count ends at its last table with sound

    public:

        /**
         * @brief Check if waveform data is loaded
         * @return True if waveform is loaded and ready for playback
         */
        bool isLoaded() const;

        /**
         * @brief Get the number of samples in the waveform
         * @return Sample count
         */
        size_t getSampleCount() const;

        /**
         * @brief Get the filename of the loaded waveform
         * @return Filename string (empty if not loaded from file)
         */
        const std::string &getFilename() const;

        /**
         * @brief Get the sample rate of the loaded waveform
         * @return Sample rate in Hz
         */
        float getSampleRate() const;

        /**
         * @brief Clear the waveform data
         */
        void clear();

        /**
         * @brief Normalize the waveform to range [-1.0, 1.0]
         */
        void normalize();

        /**
         * @brief Get raw sample data (const reference)
         * @return Const reference to sample vector
         */
        const std::vector<float> &getData() const;

        /**
         * @brief Get samples per wave table
         * @return Number of samples per single-cycle table
         */
        size_t getSamplesPerWave() const { return samples_per_wave_; }

        /**
         * @brief Create a PCMWaveform from a file and return as shared pointer
         * @param filename Path to WAV file
         * @return Shared pointer to PCMWaveform, null if loading failed
         */
        static std::shared_ptr<PCMWaveform> createFromFile(const std::string &filename);

        /**
         * @brief List available PCM files in a directory
         * @param directory Directory path to search for WAV files
         * @return Vector of filenames found
         */
        static std::vector<std::string> listPCMFiles(const std::string &directory);

    private:
        /**
         * @brief Perform linear interpolation between two samples
         * @param sample1 First sample value
         * @param sample2 Second sample value
         * @param fraction Interpolation fraction (0.0 to 1.0)
         * @return Interpolated value
         */
        float interpolate(float sample1, float sample2, float fraction) const;
    };

} // namespace synth
