/**
 * @file wav_reader.cpp
 * @brief Implementation of WAV file reading utilities
 */

#include "wav_reader.hpp"
#include "wav_writer.hpp" // For shared WAV structures
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace synth
{

    int WAVReader::readFile(const std::string &filename, std::vector<float> &samples, WAVInfo &info)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            return ERROR_FILE_NOT_FOUND;
        }

        int result = readAndValidateHeader(file, info);
        if (result != 0)
        {
            return result;
        }

        // Read audio data based on format
        if (info.bits_per_sample == 16 && info.audio_format == 1)
        {
            // 16-bit PCM
            std::vector<int16_t> int_samples;
            result = readAudioData(file, int_samples, info);
            if (result != 0)
            {
                return result;
            }
            convertToFloat(int_samples, samples);
        }
        else if (info.bits_per_sample == 32 && info.audio_format == 3)
        {
            // 32-bit float
            result = readAudioData(file, samples, info);
            if (result != 0)
            {
                return result;
            }
        }
        else
        {
            return ERROR_UNSUPPORTED_FORMAT;
        }

        return 0;
    }

    int WAVReader::readFile(const std::string &filename, std::vector<int16_t> &samples, WAVInfo &info)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            return ERROR_FILE_NOT_FOUND;
        }

        int result = readAndValidateHeader(file, info);
        if (result != 0)
        {
            return result;
        }

        // Only support 16-bit PCM for int16_t output
        if (info.bits_per_sample != 16 || info.audio_format != 1)
        {
            return ERROR_UNSUPPORTED_FORMAT;
        }

        return readAudioData(file, samples, info);
    }

    int WAVReader::readHeader(const std::string &filename, WAVInfo &info)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            return ERROR_FILE_NOT_FOUND;
        }

        return readAndValidateHeader(file, info);
    }

    bool WAVReader::isValidWAVFile(const std::string &filename)
    {
        WAVInfo info;
        return readHeader(filename, info) == 0;
    }

    void WAVReader::convertToFloat(const std::vector<int16_t> &int_samples, std::vector<float> &float_samples)
    {
        float_samples.resize(int_samples.size());
        const float scale = 1.0f / 32768.0f; // Convert 16-bit range to [-1.0, 1.0]

        for (size_t i = 0; i < int_samples.size(); ++i)
        {
            float_samples[i] = static_cast<float>(int_samples[i]) * scale;
        }
    }

    void WAVReader::normalize(std::vector<float> &samples)
    {
        if (samples.empty())
            return;

        // Find maximum absolute value
        float max_val = 0.0f;
        for (float sample : samples)
        {
            max_val = std::max(max_val, std::abs(sample));
        }

        // Avoid division by zero
        if (max_val > 0.0f)
        {
            float scale = 1.0f / max_val;
            for (float &sample : samples)
            {
                sample *= scale;
            }
        }
    }

    void WAVReader::extractChannel(const std::vector<float> &samples, int channel_index,
                                   int num_channels, std::vector<float> &output)
    {
        if (channel_index >= num_channels || channel_index < 0)
        {
            output.clear();
            return;
        }

        size_t num_frames = samples.size() / num_channels;
        output.resize(num_frames);

        for (size_t frame = 0; frame < num_frames; ++frame)
        {
            output[frame] = samples[frame * num_channels + channel_index];
        }
    }

    void WAVReader::mixToMono(const std::vector<float> &samples, int num_channels, std::vector<float> &output)
    {
        if (num_channels <= 0)
        {
            output.clear();
            return;
        }

        size_t num_frames = samples.size() / num_channels;
        output.resize(num_frames);

        for (size_t frame = 0; frame < num_frames; ++frame)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < num_channels; ++ch)
            {
                sum += samples[frame * num_channels + ch];
            }
            output[frame] = sum / static_cast<float>(num_channels);
        }
    }

    std::string WAVReader::getErrorMessage(int error_code)
    {
        switch (error_code)
        {
        case 0:
            return "Success";
        case ERROR_FILE_NOT_FOUND:
            return "File not found or could not be opened";
        case ERROR_INVALID_HEADER:
            return "Invalid WAV file header";
        case ERROR_UNSUPPORTED_FORMAT:
            return "Unsupported audio format";
        case ERROR_READ_FAILED:
            return "Failed to read audio data";
        case ERROR_INVALID_DATA:
            return "Invalid or corrupted audio data";
        default:
            return "Unknown error";
        }
    }

    int WAVReader::readAndValidateHeader(std::ifstream &file, WAVInfo &info)
    {
        WAVHeader header;
        WAVFormatChunk format;
        WAVDataChunk data = {}; // Initialize to avoid warnings

        // Read main header
        file.read(reinterpret_cast<char *>(&header), sizeof(WAVHeader));
        if (file.gcount() != sizeof(WAVHeader))
        {
            return ERROR_READ_FAILED;
        }

        // Validate RIFF header
        if (std::strncmp(header.chunk_id, "RIFF", 4) != 0 ||
            std::strncmp(header.format, "WAVE", 4) != 0)
        {
            return ERROR_INVALID_HEADER;
        }

        // Read format chunk
        file.read(reinterpret_cast<char *>(&format), sizeof(WAVFormatChunk));
        if (file.gcount() != sizeof(WAVFormatChunk))
        {
            return ERROR_READ_FAILED;
        }

        // Validate format chunk
        if (std::strncmp(format.subchunk1_id, "fmt ", 4) != 0)
        {
            return ERROR_INVALID_HEADER;
        }

        // Skip any extra format bytes
        if (format.subchunk1_size > 16)
        {
            file.seekg(format.subchunk1_size - 16, std::ios::cur);
        }

        // Find data chunk (skip any other chunks)
        char chunk_id[4];
        uint32_t chunk_size;
        bool found_data = false;

        while (file.read(chunk_id, 4) && file.read(reinterpret_cast<char *>(&chunk_size), 4))
        {
            if (std::strncmp(chunk_id, "data", 4) == 0)
            {
                // Found data chunk
                std::memcpy(data.subchunk2_id, chunk_id, 4);
                data.subchunk2_size = chunk_size;
                found_data = true;
                break;
            }
            else
            {
                // Skip this chunk
                file.seekg(chunk_size, std::ios::cur);
            }
        }

        if (!found_data)
        {
            return ERROR_INVALID_HEADER;
        }

        // Fill WAVInfo structure
        info.sample_rate = format.sample_rate;
        info.num_channels = format.num_channels;
        info.bits_per_sample = format.bits_per_sample;
        info.audio_format = format.audio_format;

        // Calculate number of samples
        uint32_t bytes_per_sample = format.bits_per_sample / 8;
        info.num_samples = data.subchunk2_size / (bytes_per_sample * format.num_channels);

        // Calculate duration
        info.duration = static_cast<float>(info.num_samples) / static_cast<float>(format.sample_rate);

        return 0;
    }

    template <typename T>
    int WAVReader::readAudioData(std::ifstream &file, std::vector<T> &samples, const WAVInfo &info)
    {
        size_t total_samples = info.num_samples * info.num_channels;
        samples.resize(total_samples);

        size_t bytes_to_read = total_samples * sizeof(T);
        file.read(reinterpret_cast<char *>(samples.data()), bytes_to_read);

        if (file.gcount() != static_cast<std::streamsize>(bytes_to_read))
        {
            return ERROR_READ_FAILED;
        }

        return 0;
    }

    // Explicit template instantiations
    template int WAVReader::readAudioData<float>(std::ifstream &file, std::vector<float> &samples, const WAVInfo &info);
    template int WAVReader::readAudioData<int16_t>(std::ifstream &file, std::vector<int16_t> &samples, const WAVInfo &info);

} // namespace synth
