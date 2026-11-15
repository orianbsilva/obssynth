/**
 * @file waveout_backend.cpp
 * @brief Windows WaveOut audio backend implementation with stereo support
 *
 * This implementation provides true stereo audio output using Windows WaveOut API.
 * Audio configuration constants are stored in the constructor rather than using
 * global constants, allowing for more flexible audio format configuration.
 *
 * Stereo support:
 * - Handles interleaved stereo samples (L,R,L,R,...)
 * - Buffer size calculations account for channel count
 * - Proper WAVEFORMATEX configuration for stereo output
 */

#include "audio_backend.hpp"
#include "audio_sink.hpp"
#include "triple_buffer.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <iostream>

#pragma comment(lib, "winmm.lib")

namespace synth
{

    class WaveOutBackend : public IAudioBackend
    {
    public:
        WaveOutBackend() : hWaveOut_(nullptr), shouldStop_(false), isRunning_(false),
                           sampleRate_(48000), channels_(2), bitsPerSample_(16), bufferSize_(6000)
        {
            waveFormat_.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat_.nChannels = channels_;
            waveFormat_.nSamplesPerSec = sampleRate_;
            waveFormat_.wBitsPerSample = bitsPerSample_;
            waveFormat_.nBlockAlign = (waveFormat_.nChannels * waveFormat_.wBitsPerSample) / 8;
            waveFormat_.nAvgBytesPerSec = waveFormat_.nSamplesPerSec * waveFormat_.nBlockAlign;
            waveFormat_.cbSize = 0;
        }
        ~WaveOutBackend() override { stop(); }

        int init() override
        {
            // Buffer size for stereo: bufferSize_ frames * channels_ samples per frame
            int totalSamples = bufferSize_ * channels_;
            tripleBuffer_ = std::make_unique<TripleBuffer>(totalSamples);
            return tripleBuffer_ ? 0 : -1;
        }
        int start() override
        {
            if (isRunning_.load())
                return 0;
            MMRESULT result = waveOutOpen(&hWaveOut_, WAVE_MAPPER, &waveFormat_, 0, 0, CALLBACK_NULL);
            if (result != MMSYSERR_NOERROR)
            {
                std::cerr << "waveOutOpen failed: " << result << '\n';
                return -1;
            }
            shouldStop_.store(false);
            isRunning_.store(true);
            thread_ = std::thread(&WaveOutBackend::threadFunc, this);
            return 0;
        }
        void stop() override
        {
            if (!isRunning_.load())
                return;
            shouldStop_.store(true);
            if (thread_.joinable())
                thread_.join();
            if (hWaveOut_)
            {
                waveOutReset(hWaveOut_);
                waveOutClose(hWaveOut_);
                hWaveOut_ = nullptr;
            }
            isRunning_.store(false);
        }
        void setCallback(AudioCallback cb) override { callback_ = cb; }

    private:
        void threadFunc()
        {
            while (!shouldStop_.load())
            {
                if (callback_)
                {
                    float *w = tripleBuffer_->getWriteBuffer();
                    callback_(w, bufferSize_, channels_);
                    int totalSamples = bufferSize_ * channels_;
                    tripleBuffer_->setBufferLength(totalSamples);
                    tripleBuffer_->commitWrite();
                    int idx;
                    short *buf = tripleBuffer_->getReadBuffer(idx);
                    WAVEHDR &hdr = tripleBuffer_->headers[idx];
                    hdr.lpData = reinterpret_cast<LPSTR>(buf);
                    hdr.dwBufferLength = totalSamples * sizeof(short);
                    hdr.dwFlags = 0;
                    hdr.dwLoops = 0;
                    if (waveOutPrepareHeader(hWaveOut_, &hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
                    {
                        if (waveOutWrite(hWaveOut_, &hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                        {
                            waveOutUnprepareHeader(hWaveOut_, &hdr, sizeof(WAVEHDR));
                        }
                    }
                    else
                    {
                        // Use high-precision yield instead of sleep to avoid timing jitter
                        std::this_thread::yield();
                    }
                }
                else
                {
                    // Only sleep when no callback to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }

        // Audio configuration (moved from constants)
        const int sampleRate_;
        const int channels_;
        const int bitsPerSample_;
        const int bufferSize_;

        HWAVEOUT hWaveOut_;
        WAVEFORMATEX waveFormat_{};
        std::unique_ptr<TripleBuffer> tripleBuffer_;
        AudioCallback callback_;
        std::thread thread_;
        std::atomic<bool> shouldStop_;
        std::atomic<bool> isRunning_;
    };

    std::unique_ptr<IAudioBackend> createWaveOutBackend() { return std::make_unique<WaveOutBackend>(); }

} // namespace synth
