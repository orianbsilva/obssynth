/**
 * @file audio_sink.hpp
 * @brief Cross-platform audio output sink with threaded playback
 *
 * Provides audio output functionality supporting multiple backends:
 * - Windows: WinMM WaveOut and WASAPI
 * - Linux: ALSA and PulseAudio
 *
 * Features lock-free triple buffering for real-time audio streaming
 * with minimal latency and dropout protection.
 */

#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include "platform_audio.hpp"
#include "triple_buffer.hpp"

namespace synth
{

    // Reused constants come from triple_buffer.hpp / audio_backend.hpp

    // Triple buffer structure for lock-free audio streaming
    struct TripleBuffer; // now defined in triple_buffer.hpp

    // Audio sink class with threaded continuous playback
    class IAudioBackend; // from audio_backend.hpp

    /**
     * @brief Audio sink class with threaded continuous playback
     *
     * Provides cross-platform audio output functionality with support for
     * multiple backends (ALSA, PulseAudio, WinMM, WASAPI). Manages audio
     * streaming with lock-free triple buffering for real-time performance.
     */
    class AudioSink
    {
    public:
        /**
         * @brief Construct a new AudioSink object
         */
        AudioSink();

        /**
         * @brief Destroy the AudioSink object
         */
        ~AudioSink();

        /**
         * @brief Copy constructor (deleted for safety)
         */
        AudioSink(const AudioSink &) = delete;

        /**
         * @brief Copy assignment operator (deleted for safety)
         */
        AudioSink &operator=(const AudioSink &) = delete;

        /**
         * @brief Move constructor
         * @param other AudioSink object to move from
         */
        AudioSink(AudioSink &&other) noexcept;

        /**
         * @brief Move assignment operator
         * @param other AudioSink object to move from
         * @return Reference to this object
         */
        AudioSink &operator=(AudioSink &&other) noexcept;

        /**
         * @brief Initialize the audio sink
         * @return 0 on success, negative error code on failure
         */
        int init();

        /**
         * @brief Start audio playback
         * @return 0 on success, negative error code on failure
         */
        int start();

        /**
         * @brief Start audio playback with a callback function
         * @param callback Function to call for audio processing (buffer, size, channels)
         * @return 0 on success, negative error code on failure
         */
        int startWithCallback(std::function<void(float *, int, int)> callback);

        /**
         * @brief Stop audio playback
         */
        void stop();

        /**
         * @brief Clean up audio resources
         */
        void cleanup();

        /**
         * @brief Set the audio processing callback function
         * @param callback Function to call for audio processing, (buffer, size, channels)
         */
        void setAudioCallback(std::function<void(float *, int, int)> callback);

        /**
         * @brief List available audio output devices
         * @return Number of available devices
         */
        static int listDevices();

        /**
         * @brief Set the audio backend type
         * @param type Audio backend type to use
         */
        void setBackend(AudioBackendType type);

    private:
#ifdef _WIN32
        static void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
#endif
        void audioProductionThreadFunction();
        void audioPlaybackThreadFunction();
        void audioThreadFunction();
        std::unique_ptr<IAudioBackend> backend_;
        AudioBackendType backendType_;
        std::function<void(float *, int, int)> audioCallback_;
    };

    // C-style function wrappers for backward compatibility
    extern "C"
    {
        int audio_sink_init(AudioSink *sink);
        int audio_sink_start(AudioSink *sink);
        int audio_sink_play_buffer(AudioSink *sink, float *buffer, int size);
        void audio_sink_stop(AudioSink *sink);
        void audio_sink_cleanup(AudioSink *sink);
        int audio_sink_list_devices(void);
    }

} // namespace synth
