/**
 * @file wav_reader.hpp
 * @brief WAV file reading utilities for audio input
 *
 * Provides functionality for reading audio data from WAV files in various formats.
 * Supports both 16-bit integer and 32-bit float audio data with automatic
 * format detection and conversion. Includes validation and error handling
 * for robust file processing.
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace synth
{

    /**
     * @brief WAV file information structure
     *
     * Contains metadata extracted from a WAV file including sample rate,
     * channel configuration, bit depth, and audio format information.
     */
    struct WAVInfo
    {
        uint32_t sample_rate;
        uint16_t num_channels;
        uint16_t bits_per_sample;
        uint16_t audio_format;
        uint32_t num_samples;
        float duration;

        /**
         * @brief Construct a new WAVInfo object with default values
         */
        WAVInfo() : sample_rate(0), num_channels(0), bits_per_sample(0),
                    audio_format(0), num_samples(0), duration(0.0f) {}
    };

    /**
     * @brief WAV file reader utility class
     *
     * Provides static methods for reading audio data from WAV files.
     * Supports multiple audio formats including 16-bit integer and 32-bit float.
     * Offers automatic format detection and conversion to float arrays.
     */
    class WAVReader
    {
    public:
        /**
         * @brief Read WAV file and extract audio data as floating-point samples
         * @param filename Input WAV filename
         * @param samples Output vector to store audio samples
         * @param info Output WAV file information
         * @return 0 on success, negative error code on failure
         */
        static int readFile(const std::string &filename, std::vector<float> &samples, WAVInfo &info);

        /**
         * @brief Read WAV file and extract audio data as 16-bit integer samples
         * @param filename Input WAV filename
         * @param samples Output vector to store audio samples
         * @param info Output WAV file information
         * @return 0 on success, negative error code on failure
         */
        static int readFile(const std::string &filename, std::vector<int16_t> &samples, WAVInfo &info);

        /**
         * @brief Read only the header information from a WAV file
         * @param filename Input WAV filename
         * @param info Output WAV file information
         * @return 0 on success, negative error code on failure
         */
        static int readHeader(const std::string &filename, WAVInfo &info);

        /**
         * @brief Check if a file is a valid WAV file
         * @param filename Filename to check
         * @return True if file is a valid WAV file, false otherwise
         */
        static bool isValidWAVFile(const std::string &filename);

        /**
         * @brief Convert 16-bit integer samples to floating-point samples
         * @param int_samples Input 16-bit integer samples
         * @param float_samples Output floating-point samples
         */
        static void convertToFloat(const std::vector<int16_t> &int_samples, std::vector<float> &float_samples);

        /**
         * @brief Normalize floating-point samples to range [-1.0, 1.0]
         * @param samples Audio samples to normalize
         */
        static void normalize(std::vector<float> &samples);

        /**
         * @brief Extract a single channel from multi-channel audio data
         * @param samples Input interleaved multi-channel samples
         * @param channel_index Channel to extract (0-based)
         * @param num_channels Total number of channels
         * @param output Output single-channel samples
         */
        static void extractChannel(const std::vector<float> &samples, int channel_index,
                                   int num_channels, std::vector<float> &output);

        /**
         * @brief Mix multi-channel audio down to mono
         * @param samples Input interleaved multi-channel samples
         * @param num_channels Total number of channels
         * @param output Output mono samples
         */
        static void mixToMono(const std::vector<float> &samples, int num_channels, std::vector<float> &output);

        /**
         * @brief Get error message for a given error code
         * @param error_code Error code returned by read functions
         * @return Human-readable error message
         */
        static std::string getErrorMessage(int error_code);

    private:
        // Error codes
        static const int ERROR_FILE_NOT_FOUND = -1;
        static const int ERROR_INVALID_HEADER = -2;
        static const int ERROR_UNSUPPORTED_FORMAT = -3;
        static const int ERROR_READ_FAILED = -4;
        static const int ERROR_INVALID_DATA = -5;

        /**
         * @brief Read and validate WAV file header
         * @param file Input file stream
         * @param info Output WAV file information
         * @return 0 on success, negative error code on failure
         */
        static int readAndValidateHeader(std::ifstream &file, WAVInfo &info);

        /**
         * @brief Read audio data from file stream
         * @param file Input file stream
         * @param samples Output sample vector
         * @param info WAV file information
         * @return 0 on success, negative error code on failure
         */
        template <typename T>
        static int readAudioData(std::ifstream &file, std::vector<T> &samples, const WAVInfo &info);
    };

} // namespace synth
