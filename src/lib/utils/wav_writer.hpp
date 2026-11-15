/**
 * @file wav_writer.hpp
 * @brief WAV file writing utilities for audio output
 *
 * Provides functionality for writing audio data to WAV files in various formats.
 * Supports both 16-bit integer and 32-bit float audio data with configurable
 * sample rates, channel counts, and bit depths. Includes C-style and C++
 * convenience interfaces for compatibility.
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace synth
{

// WAV file structures
#pragma pack(push, 1)
    struct WAVHeader
    {
        char chunk_id[4]; // "RIFF"
        uint32_t chunk_size;
        char format[4]; // "WAVE"
    };

    struct WAVFormatChunk
    {
        char subchunk1_id[4]; // "fmt "
        uint32_t subchunk1_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    };

    struct WAVDataChunk
    {
        char subchunk2_id[4]; // "data"
        uint32_t subchunk2_size;
    };
#pragma pack(pop)

    /**
     * @brief WAV file configuration structure
     *
     * Encapsulates the parameters needed for WAV file creation including
     * sample rate, channel configuration, bit depth, and audio format.
     */
    struct WAVConfig
    {
        uint32_t sample_rate;
        uint16_t num_channels;
        uint16_t bits_per_sample;
        uint16_t audio_format;

        /**
         * @brief Construct a new WAVConfig object with specified parameters
         * @param sr Sample rate in Hz (default: 48000)
         * @param channels Number of audio channels (default: 1 for mono)
         * @param bits Bits per sample (default: 16)
         * @param format Audio format (default: PCM)
         */
        WAVConfig(uint32_t sr = 48000,
                  uint16_t channels = 1,
                  uint16_t bits = 16,
                  uint16_t format = 1)
            : sample_rate(sr), num_channels(channels), bits_per_sample(bits), audio_format(format) {}
    };

    /**
     * @brief WAV file writer utility class
     *
     * Provides static methods for writing audio data to WAV files.
     * Supports multiple audio formats including 16-bit integer and 32-bit float.
     * Offers both C++ vector-based and C-style pointer-based interfaces.
     */
    class WAVWriter
    {
    public:
        /**
         * @brief Write audio data to WAV file with full configuration
         * @param filename Output filename
         * @param buffer Audio data buffer (16-bit signed integers)
         * @param num_samples Number of samples to write
         * @param config WAV file configuration
         * @return 0 on success, negative error code on failure
         */
        static int writeFile(const std::string &filename, const std::vector<int16_t> &buffer,
                             int num_samples, const WAVConfig &config);

        /**
         * @brief Write audio data to WAV file with simple configuration
         * @param filename Output filename
         * @param buffer Audio data buffer (16-bit signed integers)
         * @param num_samples Number of samples to write
         * @param sample_rate Sample rate in Hz
         * @return 0 on success, negative error code on failure
         */
        static int writeFileSimple(const std::string &filename, const std::vector<int16_t> &buffer,
                                   int num_samples, int sample_rate);

        /**
         * @brief Write floating-point audio data to WAV file with full configuration
         * @param filename Output filename
         * @param buffer Audio data buffer (32-bit floats)
         * @param num_samples Number of samples to write
         * @param config WAV file configuration
         * @return 0 on success, negative error code on failure
         */
        static int writeFile(const std::string &filename, const std::vector<float> &buffer,
                             int num_samples, const WAVConfig &config);

        /**
         * @brief Write floating-point audio data to WAV file with simple configuration
         * @param filename Output filename
         * @param buffer Audio data buffer (32-bit floats)
         * @param num_samples Number of samples to write
         * @param sample_rate Sample rate in Hz
         * @return 0 on success, negative error code on failure
         */
        static int writeFileSimple(const std::string &filename, const std::vector<float> &buffer,
                                   int num_samples, int sample_rate);

        /**
         * @brief Create a WAV configuration with specified parameters
         * @param sample_rate Sample rate in Hz
         * @param channels Number of audio channels
         * @param bits_per_sample Bits per sample
         * @return Configured WAVConfig object
         */
        static WAVConfig createConfig(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample);

        /**
         * @brief Get default WAV configuration
         * @return Default WAVConfig (48kHz, stereo, 16-bit PCM)
         */
        static WAVConfig getDefaultConfig();

        /**
         * @brief Calculate expected WAV file size
         * @param num_samples Number of audio samples
         * @param config WAV configuration
         * @return File size in bytes
         */
        static int calculateFileSize(int num_samples, const WAVConfig &config);

        /**
         * @brief Calculate audio duration
         * @param num_samples Number of audio samples
         * @param sample_rate Sample rate in Hz
         * @return Duration in seconds
         */
        static float calculateDuration(int num_samples, int sample_rate);

        /**
         * @brief Write 16-bit audio data using C-style arrays
         * @param filename Output filename
         * @param buffer Audio data buffer pointer
         * @param num_samples Number of samples to write
         * @param config WAV file configuration
         * @return 0 on success, negative error code on failure
         */
        static int writeFile(const char *filename, int16_t *buffer, int num_samples, const WAVConfig &config);

        /**
         * @brief Write 16-bit audio data using C-style arrays with simple configuration
         * @param filename Output filename
         * @param buffer Audio data buffer pointer
         * @param num_samples Number of samples to write
         * @param sample_rate Sample rate in Hz
         * @return 0 on success, negative error code on failure
         */
        static int writeFileSimple(const char *filename, int16_t *buffer, int num_samples, int sample_rate);

        /**
         * @brief Write floating-point audio data using C-style arrays
         * @param filename Output filename
         * @param buffer Audio data buffer pointer
         * @param num_samples Number of samples to write
         * @param config WAV file configuration
         * @return 0 on success, negative error code on failure
         */
        static int writeFile(const char *filename, float *buffer, int num_samples, const WAVConfig &config);

        /**
         * @brief Write floating-point audio data using C-style arrays with simple configuration
         * @param filename Output filename
         * @param buffer Audio data buffer pointer
         * @param num_samples Number of samples to write
         * @param sample_rate Sample rate in Hz
         * @return 0 on success, negative error code on failure
         */
        static int writeFileSimple(const char *filename, float *buffer, int num_samples, int sample_rate);

        // Convenience methods for common configurations

        /**
         * @brief Create stereo WAV configuration
         * @param sample_rate Sample rate in Hz (default: 48000)
         * @param bits_per_sample Bits per sample (default: 16)
         * @return Configured WAVConfig for stereo output
         */
        static WAVConfig createStereoConfig(uint32_t sample_rate = 48000, uint16_t bits_per_sample = 16);

        /**
         * @brief Create mono WAV configuration
         * @param sample_rate Sample rate in Hz (default: 48000)
         * @param bits_per_sample Bits per sample (default: 16)
         * @return Configured WAVConfig for mono output
         */
        static WAVConfig createMonoConfig(uint32_t sample_rate = 48000, uint16_t bits_per_sample = 16);
    };

} // namespace synth
