/**
 * @file triple_buffer.hpp
 * @brief Lock-free triple buffering for real-time audio streaming
 *
 * Implements a lock-free triple buffer system for efficient audio data exchange
 * between audio generation thread and platform audio callback. Provides smooth
 * audio streaming without blocking or buffer underruns.
 *
 * Features:
 * - Lock-free atomic operations for thread safety
 * - Triple buffering prevents blocking between producer/consumer
 * - Automatic float-to-int16 conversion for audio hardware
 * - Platform-specific optimizations (Windows WAVEHDR support)
 * - Configurable buffer sizes for different latency requirements
 *
 * @warning This is a low-level audio component - use AudioSink for high-level access
 * @see AudioSink for the complete audio output implementation
 */

#pragma once
#include <memory>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "platform_audio.hpp"

namespace synth
{
    /// Number of buffers in the triple buffer system
    constexpr int NUM_BUFFERS = 3;

    /**
     * @brief Lock-free triple buffer for real-time audio streaming
     *
     * Provides thread-safe audio buffer management using atomic operations.
     * One buffer for writing (producer), one for reading (consumer), and
     * one for pending data exchange. Eliminates the need for mutexes or
     * other blocking synchronization primitives.
     *
     * Buffer states:
     * - Write buffer: Currently being filled by audio generator
     * - Pending buffer: Ready for consumption by audio callback
     * - Read buffer: Currently being consumed by audio hardware
     *
     * @code
     * TripleBuffer triple_buffer(1024);
     *
     * // Producer thread
     * float* write_buf = triple_buffer.getWriteBuffer();
     * // Fill write_buf with audio data...
     * triple_buffer.setBufferLength(samples_written);
     * triple_buffer.commitWrite();
     *
     * // Consumer thread
     * int buffer_index;
     * int length = triple_buffer.getBufferLength(buffer_index);
     * short* read_buf = triple_buffer.getReadBuffer(buffer_index);
     * // Send read_buf to audio hardware...
     * @endcode
     *
     * @warning All buffer operations must be performed on correct threads
     * @warning Buffer size cannot be changed after construction
     */
    struct TripleBuffer
    {
        std::unique_ptr<float[]> buffers[NUM_BUFFERS];          ///< Float audio buffers
        std::atomic<int> buffer_length[NUM_BUFFERS];            ///< Actual data length per buffer
        std::unique_ptr<short[]> convertedBuffers[NUM_BUFFERS]; ///< Converted int16 buffers
#ifdef _WIN32
        WAVEHDR headers[NUM_BUFFERS]; ///< Windows WaveOut headers
#endif
        std::atomic<int> writeIndex{0};   ///< Current write buffer index
        std::atomic<int> pendingIndex{1}; ///< Current pending buffer index
        int bufferSize_;                  ///< Maximum buffer size in samples

        /**
         * @brief Construct a new TripleBuffer
         * @param size Maximum buffer size in samples
         */
        TripleBuffer(int size) : bufferSize_(size)
        {
            for (int i = 0; i < NUM_BUFFERS; i++)
            {
                buffers[i] = std::make_unique<float[]>(size);
                convertedBuffers[i] = std::make_unique<short[]>(size);
                buffer_length[i].store(size); // Initialize with default buffer size
#ifdef _WIN32
                std::memset(&headers[i], 0, sizeof(WAVEHDR));
#endif
                std::memset(buffers[i].get(), 0, size * sizeof(float));
                std::memset(convertedBuffers[i].get(), 0, size * sizeof(short));
            }
        }

        /**
         * @brief Get pointer to current write buffer
         * @return Pointer to float buffer for writing audio data
         */
        float *getWriteBuffer() { return buffers[writeIndex.load()].get(); }

        /**
         * @brief Set actual length of data in current write buffer
         * @param length Number of samples written (must be <= buffer size)
         */
        void setBufferLength(int length) { buffer_length[writeIndex.load()].store(length); }

        /**
         * @brief Get buffer length and index for reading
         * @param bufferIndex Reference to store buffer index
         * @return Number of samples available for reading
         */
        int getBufferLength(int &bufferIndex)
        {
            bufferIndex = pendingIndex.load();
            return buffer_length[bufferIndex].load();
        }

        /**
         * @brief Commit current write buffer and swap for next
         *
         * Converts float data to int16, then atomically swaps write and
         * pending buffers. This makes the written data available for
         * consumption while providing a fresh buffer for writing.
         */
        void commitWrite()
        {
            int currentWrite = writeIndex.load();
            convert(currentWrite);
            int currentPending = pendingIndex.load();
            writeIndex.store(currentPending);
            pendingIndex.store(currentWrite);
        }

        /**
         * @brief Get pointer to converted int16 read buffer
         * @param bufferIndex Reference to store buffer index
         * @return Pointer to int16 buffer for audio hardware consumption
         */
        short *getReadBuffer(int &bufferIndex)
        {
            bufferIndex = pendingIndex.load();
            return convertedBuffers[bufferIndex].get();
        }

    private:
        /**
         * @brief Convert float buffer to int16 with clipping
         * @param idx Buffer index to convert
         *
         * Converts floating-point audio samples (-1.0 to +1.0) to
         * 16-bit signed integers with proper clipping and scaling.
         */
        void convert(int idx)
        {
            float *f = buffers[idx].get();
            short *s = convertedBuffers[idx].get();
            for (int i = 0; i < bufferSize_; ++i)
            {
                float sample = f[i];
                if (sample > 1.0f)
                    sample = 1.0f;
                else if (sample < -1.0f)
                    sample = -1.0f;
                s[i] = static_cast<short>(sample * 32767.0f);
            }
        }
    };
} // namespace synth
