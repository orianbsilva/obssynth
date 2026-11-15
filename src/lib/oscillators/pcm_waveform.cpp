/**
 * @file pcm_waveform.cpp
 * @brief Implementation of PCM waveform storage and playback
 */

#include "pcm_waveform.hpp"
#include <cstddef>
#include <algorithm>
#include <cmath>
#include "../utils/wav_reader.hpp"
#include "../math/synt_math.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace synth
{

    // ── Filename parser: bank_<name>_<N>waves_<spw>samples_32float.wav ──────
    static bool parseBankFilename(const std::string &filename,
                                  size_t &out_waves, size_t &out_spw)
    {
        size_t slash = filename.find_last_of("/\\");
        const std::string base = (slash == std::string::npos)
                                     ? filename
                                     : filename.substr(slash + 1);

        auto extractNumBefore = [&](const std::string &marker, size_t &out) -> bool
        {
            size_t pos = base.find(marker);
            if (pos == std::string::npos || pos == 0)
                return false;
            size_t end = pos;
            size_t start = end;
            while (start > 0 && std::isdigit((unsigned char)base[start - 1]))
                --start;
            if (start == end)
                return false;
            try
            {
                out = std::stoul(base.substr(start, end - start));
            }
            catch (...)
            {
                return false;
            }
            return out > 0;
        };

        return extractNumBefore("waves_", out_waves) &&
               extractNumBefore("samples_", out_spw);
    }

    PCMWaveform::PCMWaveform()
        : sample_count_(0), is_loaded_(false), sample_rate_(44100.0f),
          is_wav_bank_(false), bank_wave_count_(1), samples_per_wave_(1024), current_bank_position_float_(0.0f)
    {
    }

    PCMWaveform::PCMWaveform(const std::vector<float> &samples)
        : samples_(samples), sample_count_(samples.size()),
          is_loaded_(true), sample_rate_(44100.0f),
          is_wav_bank_(false), bank_wave_count_(1), samples_per_wave_(samples.size()), current_bank_position_float_(0.0f)
    {
        std::cout << "DEBUG: PCMWaveform constructor with " << samples.size() << " samples, is_loaded: " << is_loaded_ << std::endl;
        if (!samples.empty())
        {
            std::cout << "DEBUG: First sample: " << samples[0] << ", Last sample: " << samples[samples.size() - 1] << std::endl;
        }
    }

    bool PCMWaveform::loadFromFile(const std::string &filename)
    {
        WAVInfo info;
        std::vector<float> file_samples;

        int result = WAVReader::readFile(filename, file_samples, info);
        if (result != 0)
        {
            std::cerr << "Failed to load PCM waveform from " << filename
                      << ": " << WAVReader::getErrorMessage(result) << std::endl;
            return false;
        }

        // If stereo, mix down to mono
        if (info.num_channels > 1)
        {
            std::vector<float> mono_samples;
            WAVReader::mixToMono(file_samples, info.num_channels, mono_samples);
            samples_ = std::move(mono_samples);
        }
        else
        {
            samples_ = std::move(file_samples);
        }

        sample_count_ = samples_.size();
        is_loaded_ = true;
        filename_ = filename;
        sample_rate_ = static_cast<float>(info.sample_rate);

        // Auto-detect bank from filename pattern bank_<name>_<N>waves_<spw>samples_32float.wav
        size_t fn_waves = 0, fn_spw = 0;
        is_wav_bank_ = parseBankFilename(filename, fn_waves, fn_spw);
        if (is_wav_bank_ && fn_waves > 1)
        {
            bank_wave_count_ = fn_waves;
            samples_per_wave_ = fn_spw;
            if (bank_wave_count_ * samples_per_wave_ > sample_count_)
                bank_wave_count_ = sample_count_ / samples_per_wave_;
            if (bank_wave_count_ == 0) // a wave longer than the file: the file as one wave (0 underflowed count-1)
            {
                bank_wave_count_ = 1;
                samples_per_wave_ = sample_count_;
            }
            dropSilentTail();
            std::cout << "Loaded as wav bank with " << bank_wave_count_ << " waves, "
                      << samples_per_wave_ << " samples per wave" << std::endl;
        }
        else
        {
            is_wav_bank_ = false;
            bank_wave_count_ = 1;
            samples_per_wave_ = sample_count_;
        }

        // Normalize the waveform
        normalize();

        // Determine if this is likely a single cycle or a longer sample
        bool is_single_cycle = samples_per_wave_ < sample_rate_ * 0.1f; // Less than 100ms suggests single cycle

        std::cout << "Loaded PCM waveform: " << filename
                  << " (" << sample_count_ << " samples at "
                  << sample_rate_ << " Hz)"
                  << (is_single_cycle ? " [single cycle]" : " [long sample]")
                  << (is_wav_bank_ ? " [wav bank]" : "") << std::endl;

        return true;
    }

    bool PCMWaveform::loadWavBank(const std::string &filename, size_t wave_count, size_t samples_per_wave)
    {
        WAVInfo info;
        std::vector<float> file_samples;

        int result = WAVReader::readFile(filename, file_samples, info);
        if (result != 0)
        {
            std::cerr << "Failed to load wav bank from " << filename
                      << ": " << WAVReader::getErrorMessage(result) << std::endl;
            return false;
        }

        // If stereo, mix down to mono
        if (info.num_channels > 1)
        {
            std::vector<float> mono_samples;
            WAVReader::mixToMono(file_samples, info.num_channels, mono_samples);
            samples_ = std::move(mono_samples);
        }
        else
        {
            samples_ = std::move(file_samples);
        }

        sample_count_ = samples_.size();
        is_loaded_ = true;
        filename_ = filename;
        sample_rate_ = static_cast<float>(info.sample_rate);
        is_wav_bank_ = true;
        current_bank_position_float_ = 0.0f;

        // Auto-detect samples_per_wave and wave_count from filename pattern
        // bank_<name>_<N>waves_<spw>samples_32float.wav
        size_t fn_waves = 0, fn_spw = 0;
        bool parsed = parseBankFilename(filename, fn_waves, fn_spw);

        if (samples_per_wave == 0)
            samples_per_wave_ = parsed ? fn_spw : 1024;
        else
            samples_per_wave_ = samples_per_wave;

        if (wave_count == 0)
            bank_wave_count_ = parsed ? fn_waves : sample_count_ / samples_per_wave_;
        else
            bank_wave_count_ = wave_count;

        // Ensure we don't exceed available samples
        if (bank_wave_count_ * samples_per_wave_ > sample_count_)
        {
            bank_wave_count_ = sample_count_ / samples_per_wave_;
        }
        if (bank_wave_count_ == 0 || samples_per_wave_ == 0) // as above: the file as one wave
        {
            bank_wave_count_ = 1;
            samples_per_wave_ = std::max<size_t>(1, sample_count_);
        }
        dropSilentTail();

        // Normalize the waveform
        normalize();

        std::cout << "Loaded wav bank: " << filename
                  << " (" << bank_wave_count_ << " waves, "
                  << samples_per_wave_ << " samples per wave, "
                  << sample_count_ << " total samples)" << std::endl;

        return true;
    }

    void PCMWaveform::setData(const std::vector<float> &samples)
    {
        samples_ = samples;
        sample_count_ = samples.size();
        is_loaded_ = !samples.empty();
        filename_.clear();
        is_wav_bank_ = false;
        bank_wave_count_ = 1;
        samples_per_wave_ = sample_count_;
        current_bank_position_float_ = 0.0f;
    }

    float PCMWaveform::getSample(float normalized_phase) const
    {
        if (!is_loaded_ || sample_count_ == 0)
        {
            return 0.0f;
        }

        // Wrap phase to [0.0, 1.0) range for seamless looping
        normalized_phase = normalized_phase - math::floor(normalized_phase);
        if (normalized_phase < 0.0f || normalized_phase >= 1.0f) // a tiny negative phase wraps to exactly 1.0f
            normalized_phase = 0.0f;

        if (is_wav_bank_)
        {
            // For wav banks, interpolate between adjacent waves based on bank position
            // Scale position to cover the full bank range [0, bank_wave_count_-1]
            // 0..1 over the whole bank (it was halved here, and setBankPosition clamps to 1: the upper half of
            // every bank could not be reached)
            float scaled_position = current_bank_position_float_ * static_cast<float>(bank_wave_count_ - 1);

            // Get the two wave indices for interpolation
            size_t wave_index1 = static_cast<size_t>(scaled_position);
            size_t wave_index2 = wave_index1 + 1;

            float wave_fraction = scaled_position - static_cast<float>(wave_index1);

            // Handle boundary cases properly
            if (wave_index1 >= bank_wave_count_)
            {
                // Should not happen with proper input, but safety check
                wave_index1 = bank_wave_count_ - 1;
                wave_index2 = bank_wave_count_ - 1;
                wave_fraction = 0.0f;
            }
            else if (wave_index2 >= bank_wave_count_)
            {
                // When at the very end (position = 1.0), we're at the last wave
                wave_index2 = bank_wave_count_ - 1;
                // For interpolation, if we're exactly at the last wave, no interpolation needed
                if (wave_index1 == wave_index2)
                {
                    wave_fraction = 0.0f;
                }
            }

            // Calculate sample position within each wave
            float sample_pos = normalized_phase * static_cast<float>(samples_per_wave_);
            size_t base_index1 = wave_index1 * samples_per_wave_;
            size_t base_index2 = wave_index2 * samples_per_wave_;

            // Get integer and fractional parts for interpolation within each wave
            size_t sample_offset = static_cast<size_t>(sample_pos);
            float sample_fraction = sample_pos - static_cast<float>(sample_offset);

            // Get samples from first wave with wrapping
            size_t idx1_1 = base_index1 + sample_offset;
            size_t idx1_2 = base_index1 + ((sample_offset + 1) % samples_per_wave_);

            // Get samples from second wave with wrapping
            size_t idx2_1 = base_index2 + sample_offset;
            size_t idx2_2 = base_index2 + ((sample_offset + 1) % samples_per_wave_);

            // Ensure indices are within bounds for the specific waves
            // Clamp to the end of each individual wave, not the entire sample array
            size_t wave1_end = (wave_index1 + 1) * samples_per_wave_;
            size_t wave2_end = (wave_index2 + 1) * samples_per_wave_;

            if (idx1_1 >= wave1_end)
                idx1_1 = wave1_end - 1;
            if (idx1_2 >= wave1_end)
                idx1_2 = wave1_end - 1;
            if (idx2_1 >= wave2_end)
                idx2_1 = wave2_end - 1;
            if (idx2_2 >= wave2_end)
                idx2_2 = wave2_end - 1;

            // Final safety check against total sample count
            if (idx1_1 >= sample_count_)
                idx1_1 = sample_count_ - 1;
            if (idx1_2 >= sample_count_)
                idx1_2 = sample_count_ - 1;
            if (idx2_1 >= sample_count_)
                idx2_1 = sample_count_ - 1;
            if (idx2_2 >= sample_count_)
                idx2_2 = sample_count_ - 1;

            // Interpolate within each wave
            float sample1 = interpolate(samples_[idx1_1], samples_[idx1_2], sample_fraction);
            float sample2 = interpolate(samples_[idx2_1], samples_[idx2_2], sample_fraction);

            // Interpolate between waves
            return interpolate(sample1, sample2, wave_fraction);
        }
        else
        {
            // Single waveform behavior (original code)
            float sample_pos = normalized_phase * static_cast<float>(sample_count_);

            // Get integer and fractional parts for interpolation
            size_t index1 = std::min(static_cast<size_t>(sample_pos), sample_count_ - 1); // (rounding can give the count)
            float fraction = sample_pos - static_cast<float>(index1);

            // Handle wrap-around for seamless looping
            size_t index2 = (index1 + 1) % sample_count_;

            return interpolate(samples_[index1], samples_[index2], fraction);
        }
    }

    void PCMWaveform::setBankPosition(float position)
    {
        if (!is_wav_bank_ || bank_wave_count_ <= 1)
            return;

        // Clamp position to [0.0, 1.0]
        if (position < 0.0f)
            position = 0.0f;
        else if (position > 1.0f)
            position = 1.0f;

        // Store the exact position for interpolation between waves
        current_bank_position_float_ = position;
    }

    int PCMWaveform::getScaledBankPosition() const
    {
        if (!is_wav_bank_ || bank_wave_count_ <= 1)
            return 0;

        // Scale position to cover the full bank range [0, bank_wave_count_-1]
        float scaled_position = (current_bank_position_float_ / 2.f) * static_cast<float>(bank_wave_count_ - 1);
        return static_cast<int>(scaled_position + 0.5f); // Round to nearest integer
    }

    float PCMWaveform::getBankPosition() const
    {
        if (!is_wav_bank_ || bank_wave_count_ <= 1)
            return 0.0f;

        return current_bank_position_float_;
    }

    bool PCMWaveform::isWavBank() const
    {
        return is_wav_bank_;
    }

    size_t PCMWaveform::getBankWaveCount() const
    {
        return bank_wave_count_;
    }

    bool PCMWaveform::isLoaded() const
    {
        return is_loaded_;
    }

    size_t PCMWaveform::getSampleCount() const
    {
        return sample_count_;
    }

    const std::string &PCMWaveform::getFilename() const
    {
        return filename_;
    }

    float PCMWaveform::getSampleRate() const
    {
        return sample_rate_;
    }

    void PCMWaveform::clear()
    {
        samples_.clear();
        sample_count_ = 0;
        is_loaded_ = false;
        filename_.clear();
        is_wav_bank_ = false;
        bank_wave_count_ = 1;
        samples_per_wave_ = 1024;
        current_bank_position_float_ = 0.0f;
    }

    void PCMWaveform::normalize()
    {
        if (samples_.empty())
            return;

        // Find maximum absolute value
        float max_val = 0.0f;
        for (float sample : samples_)
        {
            max_val = std::max(max_val, std::abs(sample));
        }

        // Avoid division by zero
        if (max_val > 0.0f)
        {
            float scale = 1.0f / max_val;
            for (float &sample : samples_)
            {
                sample *= scale;
            }
        }
    }

    const std::vector<float> &PCMWaveform::getData() const
    {
        return samples_;
    }

    std::shared_ptr<PCMWaveform> PCMWaveform::createFromFile(const std::string &filename)
    {
        auto waveform = std::make_shared<PCMWaveform>();
        if (waveform->loadFromFile(filename))
        {
            return waveform;
        }
        return nullptr;
    }

    std::vector<std::string> PCMWaveform::listPCMFiles(const std::string &directory)
    {
        std::vector<std::string> wav_files;

        try
        {
            if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
            {
                for (const auto &entry : std::filesystem::directory_iterator(directory))
                {
                    if (entry.is_regular_file())
                    {
                        std::string filename = entry.path().filename().string();
                        std::string extension = entry.path().extension().string();

                        // Convert extension to lowercase for comparison
                        std::transform(extension.begin(), extension.end(), extension.begin(),
                                       [](unsigned char c)
                                       { return static_cast<char>(std::tolower(c)); });

                        if (extension == ".wav")
                        {
                            wav_files.push_back(filename);
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error &ex)
        {
            std::cerr << "Error listing PCM files in " << directory << ": " << ex.what() << std::endl;
        }

        // Sort alphabetically for consistent ordering
        std::sort(wav_files.begin(), wav_files.end());

        return wav_files;
    }

    float PCMWaveform::interpolate(float sample1, float sample2, float fraction) const
    {
        return sample1 + fraction * (sample2 - sample1);
    }


    // Tables of silence at a bank's end are not tables: some banks were written with their second half empty
    // (pcm/ had bank_morph_64waves with 32 waves, bank_harmonic_32waves with 16, bank_basic_16waves with 8, now
    // trimmed and named for what they hold), and the morph position ran into silence past the middle. A bank
    // from elsewhere may still end so: the count stops at the last table with sound.
    void PCMWaveform::dropSilentTail()
    {
        const size_t declared = bank_wave_count_;
        while (bank_wave_count_ > 1)
        {
            const size_t from = (bank_wave_count_ - 1) * samples_per_wave_;
            float peak = 0.0f;
            for (size_t i = from; i < from + samples_per_wave_ && i < samples_.size(); ++i)
                peak = std::max(peak, std::fabs(samples_[i]));
            if (peak > 1e-5f)
                break;
            --bank_wave_count_;
        }
        if (bank_wave_count_ != declared)
            std::cout << "Bank " << filename_ << ": " << declared << " tables declared, the last "
                      << declared - bank_wave_count_ << " silent: " << bank_wave_count_ << " used" << std::endl;
    }
} // namespace synth
