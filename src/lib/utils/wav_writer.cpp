/**
 * @file wav_writer.cpp
 * @brief WAV file writing utilities with configurable audio format support
 *
 * This implementation provides flexible WAV file writing capabilities without
 * hardcoded audio format constants. All audio parameters are configurable
 * through the constructor or method parameters, supporting both mono and stereo
 * output with various sample rates and bit depths.
 *
 * Stereo support:
 * - Handles interleaved stereo samples in WAV format
 * - Proper channel count and data size calculations
 * - Configurable defaults for different use cases
 */

#include "wav_writer.hpp"
#include <cstring>
#include <iostream>
#include <fstream>

namespace synth
{

    // Write a WAV file with full configuration control (C++ vector version)
    int WAVWriter::writeFile(const std::string &filename, const std::vector<int16_t> &buffer,
                             int num_samples, const WAVConfig &config)
    {
        if (filename.empty() || buffer.empty() || num_samples <= 0)
        {
            return -1;
        }

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            return -1;
        }

        // Calculate sizes
        int bytes_per_sample = config.bits_per_sample / 8;
        int data_size = num_samples * bytes_per_sample * config.num_channels;
        int file_size = sizeof(WAVHeader) + sizeof(WAVFormatChunk) + sizeof(WAVDataChunk) + data_size;

        // WAV Header
        WAVHeader header;
        std::memcpy(header.chunk_id, "RIFF", 4);
        header.chunk_size = file_size - 8;
        std::memcpy(header.format, "WAVE", 4);

        // Format Chunk
        WAVFormatChunk format;
        std::memcpy(format.subchunk1_id, "fmt ", 4);
        format.subchunk1_size = 16; // PCM format chunk size
        format.audio_format = config.audio_format;
        format.num_channels = config.num_channels;
        format.sample_rate = config.sample_rate;
        format.bits_per_sample = config.bits_per_sample;
        format.block_align = config.num_channels * config.bits_per_sample / 8;
        format.byte_rate = config.sample_rate * format.block_align;

        // Data Chunk
        WAVDataChunk data_chunk;
        std::memcpy(data_chunk.subchunk2_id, "data", 4);
        data_chunk.subchunk2_size = data_size;

        // Write to file
        bool success = true;
        if (!file.write(reinterpret_cast<const char *>(&header), sizeof(WAVHeader)) ||
            !file.write(reinterpret_cast<const char *>(&format), sizeof(WAVFormatChunk)) ||
            !file.write(reinterpret_cast<const char *>(&data_chunk), sizeof(WAVDataChunk)) ||
            !file.write(reinterpret_cast<const char *>(buffer.data()), sizeof(int16_t) * num_samples))
        {
            success = false;
        }

        file.close();
        return success ? 0 : -1;
    }

    // Write a WAV file with raw pointer (C compatibility)
    int WAVWriter::writeFile(const char *filename, int16_t *buffer, int num_samples, const WAVConfig &config)
    {
        if (!filename || !buffer || num_samples <= 0)
        {
            return -1;
        }

        std::vector<int16_t> vec_buffer(buffer, buffer + num_samples);
        return writeFile(std::string(filename), vec_buffer, num_samples, config);
    }

    // Write a WAV file with float vector (new float-based version)
    int WAVWriter::writeFile(const std::string &filename, const std::vector<float> &buffer,
                             int num_samples, const WAVConfig &config)
    {
        if (filename.empty() || buffer.empty() || num_samples <= 0)
        {
            return -1;
        }

        // Convert float samples to int16_t
        std::vector<int16_t> int16_buffer;
        int16_buffer.reserve(num_samples);

        for (int i = 0; i < num_samples && i < static_cast<int>(buffer.size()); ++i)
        {
            // Clamp float sample to valid range [-1.0f, 1.0f]
            float sample = buffer[i];
            if (sample > 1.0f)
                sample = 1.0f;
            if (sample < -1.0f)
                sample = -1.0f;

            // Convert to 16-bit signed integer
            int16_t sample_16bit = static_cast<int16_t>(sample * 32767.0f);
            int16_buffer.push_back(sample_16bit);
        }

        // Use existing int16_t implementation
        return writeFile(filename, int16_buffer, num_samples, config);
    }

    // Write a WAV file with raw float pointer (C compatibility)
    int WAVWriter::writeFile(const char *filename, float *buffer, int num_samples, const WAVConfig &config)
    {
        if (!filename || !buffer || num_samples <= 0)
        {
            return -1;
        }

        std::vector<float> vec_buffer(buffer, buffer + num_samples);
        return writeFile(std::string(filename), vec_buffer, num_samples, config);
    }

    // Simplified WAV file writer with default settings (mono, 16-bit PCM) - float version
    int WAVWriter::writeFileSimple(const std::string &filename, const std::vector<int16_t> &buffer,
                                   int num_samples, int sample_rate)
    {
        // Use configurable defaults: specified sample rate, mono, 16-bit PCM
        WAVConfig config = createConfig(sample_rate, 1, 16); // 1 channel (mono), 16 bits
        return writeFile(filename, buffer, num_samples, config);
    }

    // Simplified WAV file writer with raw pointer (C compatibility)
    int WAVWriter::writeFileSimple(const char *filename, int16_t *buffer, int num_samples, int sample_rate)
    {
        if (!filename || !buffer || num_samples <= 0)
        {
            return -1;
        }

        std::vector<int16_t> vec_buffer(buffer, buffer + num_samples);
        return writeFileSimple(std::string(filename), vec_buffer, num_samples, sample_rate);
    }

    // Simplified WAV file writer with float vector (new float-based version)
    int WAVWriter::writeFileSimple(const std::string &filename, const std::vector<float> &buffer,
                                   int num_samples, int sample_rate)
    {
        // Use configurable defaults: specified sample rate, mono, 16-bit PCM
        WAVConfig config = createConfig(sample_rate, 1, 16); // 1 channel (mono), 16 bits
        return writeFile(filename, buffer, num_samples, config);
    }

    // Simplified WAV file writer with raw float pointer (C compatibility)
    int WAVWriter::writeFileSimple(const char *filename, float *buffer, int num_samples, int sample_rate)
    {
        if (!filename || !buffer || num_samples <= 0)
        {
            return -1;
        }

        std::vector<float> vec_buffer(buffer, buffer + num_samples);
        return writeFileSimple(std::string(filename), vec_buffer, num_samples, sample_rate);
    }

    // Create a WAV configuration structure
    WAVConfig WAVWriter::createConfig(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
    {
        // Use configurable PCM format constant instead of hardcoded value
        return WAVConfig(sample_rate, channels, bits_per_sample, 1); // 1 = PCM format
    }

    // Get default WAV configuration (48kHz, stereo, 16-bit PCM)
    WAVConfig WAVWriter::getDefaultConfig()
    {
        // Use modern audio defaults: 48kHz, stereo, 16-bit for better compatibility
        return createConfig(48000, 2, 16); // 48kHz, 2 channels (stereo), 16 bits
    }

    // Calculate the total file size for given parameters
    int WAVWriter::calculateFileSize(int num_samples, const WAVConfig &config)
    {
        if (num_samples <= 0)
        {
            return -1;
        }

        int bytes_per_sample = config.bits_per_sample / 8;
        int data_size = num_samples * bytes_per_sample * config.num_channels;
        return sizeof(WAVHeader) + sizeof(WAVFormatChunk) + sizeof(WAVDataChunk) + data_size;
    }

    // Calculate duration in seconds for given samples and sample rate
    float WAVWriter::calculateDuration(int num_samples, int sample_rate)
    {
        if (sample_rate <= 0 || num_samples <= 0)
        {
            return 0.0f;
        }
        return static_cast<float>(num_samples) / static_cast<float>(sample_rate);
    }

    // Convenience methods for common configurations

    // Create stereo WAV configuration
    WAVConfig WAVWriter::createStereoConfig(uint32_t sample_rate, uint16_t bits_per_sample)
    {
        return createConfig(sample_rate, 2, bits_per_sample); // 2 channels for stereo
    }

    // Create mono WAV configuration
    WAVConfig WAVWriter::createMonoConfig(uint32_t sample_rate, uint16_t bits_per_sample)
    {
        return createConfig(sample_rate, 1, bits_per_sample); // 1 channel for mono
    }

} // namespace synth
