#pragma once
/**
 * @file wave_capture.hpp
 * @brief Lock-free SPSC ring buffer for waveform visualization
 *
 * Audio thread writes samples; GUI thread reads the latest snapshot
 * for oscilloscope display. Zero allocations in the audio path.
 */

#include <array>
#include <atomic>
#include <cstring>

namespace synth
{

    /**
     * @brief Lock-free single-producer single-consumer ring buffer for audio capture
     *
     * Audio thread calls write(). GUI thread calls readLatest() to get
     * the most recent N samples for display. No mutex, no allocation.
     */
    class WaveCapture
    {
    public:
        static constexpr int BUFFER_SIZE = 4096;
        static constexpr int DISPLAY_SAMPLES = 512;

        WaveCapture() { buffer_.fill(0.0f); }

        /**
         * @brief Write interleaved stereo samples (audio thread)
         * Extracts left channel only for display.
         */
        void writeStereo(const float *interleaved, int frames)
        {
            for (int i = 0; i < frames; ++i)
            {
                int idx = write_pos_.load(std::memory_order_relaxed);
                buffer_[idx] = interleaved[i * 2]; // left channel
                write_pos_.store((idx + 1) & (BUFFER_SIZE - 1), std::memory_order_release);
            }
        }

        /**
         * @brief Read the latest DISPLAY_SAMPLES with zero-crossing trigger (GUI thread)
         * @param out Output array of at least DISPLAY_SAMPLES floats
         * @return Number of samples written to out
         */
        int readTriggered(float *out) const
        {
            int wp = write_pos_.load(std::memory_order_acquire);

            // Start searching from DISPLAY_SAMPLES*2 back for a rising zero-crossing
            int search_start = (wp - DISPLAY_SAMPLES * 2 + BUFFER_SIZE) & (BUFFER_SIZE - 1);
            int trigger_pos = -1;

            for (int i = 0; i < DISPLAY_SAMPLES; ++i)
            {
                int idx0 = (search_start + i) & (BUFFER_SIZE - 1);
                int idx1 = (search_start + i + 1) & (BUFFER_SIZE - 1);
                float s0 = buffer_[idx0];
                float s1 = buffer_[idx1];
                if (s0 <= 0.0f && s1 > 0.0f)
                {
                    trigger_pos = idx1;
                    break;
                }
            }

            // If no zero-crossing found, just use the most recent samples
            if (trigger_pos < 0)
                trigger_pos = (wp - DISPLAY_SAMPLES + BUFFER_SIZE) & (BUFFER_SIZE - 1);

            for (int i = 0; i < DISPLAY_SAMPLES; ++i)
            {
                int idx = (trigger_pos + i) & (BUFFER_SIZE - 1);
                out[i] = buffer_[idx];
            }
            return DISPLAY_SAMPLES;
        }

    private:
        std::array<float, BUFFER_SIZE> buffer_;
        std::atomic<int> write_pos_{0};
    };

} // namespace synth
