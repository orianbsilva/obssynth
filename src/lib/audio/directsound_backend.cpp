/**
 * @file directsound_backend.cpp
 * @brief Windows DirectSound audio backend implementation with stereo support
 *
 * This implementation provides low-latency audio output using DirectSound API.
 * Features circular buffer management with proper synchronization and stereo
 * interleaved audio format support.
 *
 * DirectSound advantages:
 * - Lower latency than WaveOut
 * - Better hardware acceleration support
 * - More efficient buffer management
 * - Hardware mixing capabilities
 */

#include "audio_backend.hpp"
#include "audio_sink.hpp"
#include "../oscillators/polyphonic_oscillator.hpp"
#include <windows.h>
#include <dsound.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <cstring>
#include <chrono>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ole32.lib")

namespace synth
{

    class DirectSoundBackend : public IAudioBackend
    {
    public:
        DirectSoundBackend() : directSound_(nullptr),
                               primaryBuffer_(nullptr),
                               secondaryBuffer_(nullptr),
                               shouldStop_(false),
                               isRunning_(false),
                               sampleRate_(48000),
                               channels_(2),
                               bitsPerSample_(16),
                               bufferSize_(audioRequest().frames > 0 ? audioRequest().frames : 512), // the person's, else 512
                               writePosition_(0)
        {
            // Initialize COM for DirectSound
            CoInitialize(nullptr);
        }

        ~DirectSoundBackend() override
        {
            stop();
            CoUninitialize();
        }

        int init() override
        {
            // Create DirectSound object
            HRESULT hr = DirectSoundCreate(nullptr, &directSound_, nullptr);
            if (FAILED(hr))
            {
                std::cerr << "DirectSoundCreate failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            // Set cooperative level to priority
            hr = directSound_->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
            if (FAILED(hr))
            {
                std::cerr << "SetCooperativeLevel failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            // Create primary buffer
            DSBUFFERDESC primaryDesc = {};
            primaryDesc.dwSize = sizeof(DSBUFFERDESC);
            primaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
            primaryDesc.dwBufferBytes = 0;
            primaryDesc.lpwfxFormat = nullptr;

            hr = directSound_->CreateSoundBuffer(&primaryDesc, &primaryBuffer_, nullptr);
            if (FAILED(hr))
            {
                std::cerr << "CreateSoundBuffer (primary) failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            // Set primary buffer format
            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = static_cast<WORD>(channels_);
            waveFormat.nSamplesPerSec = sampleRate_;
            waveFormat.wBitsPerSample = static_cast<WORD>(bitsPerSample_);
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize = 0;

            hr = primaryBuffer_->SetFormat(&waveFormat);
            if (FAILED(hr))
            {
                std::cerr << "SetFormat failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            // Create secondary buffer (streaming buffer)
            // Use a larger buffer to prevent underruns - 4 times the frame size
            DWORD secondaryBufferSize = bufferSize_ * channels_ * sizeof(short) * 4;

            DSBUFFERDESC secondaryDesc = {};
            secondaryDesc.dwSize = sizeof(DSBUFFERDESC);
            secondaryDesc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
            secondaryDesc.dwBufferBytes = secondaryBufferSize;
            secondaryDesc.lpwfxFormat = &waveFormat;

            hr = directSound_->CreateSoundBuffer(&secondaryDesc, &secondaryBuffer_, nullptr);
            if (FAILED(hr))
            {
                std::cerr << "CreateSoundBuffer (secondary) failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            // Store buffer parameters
            bufferBytes_ = secondaryBufferSize;
            frameSize_ = channels_ * sizeof(short);
            chunkSize_ = bufferSize_ * frameSize_;

            // Initialize write position
            writePosition_ = 0;

            return 0;
        }

        int start() override
        {
            if (isRunning_.load())
                return 0;

            if (!secondaryBuffer_)
            {
                std::cerr << "DirectSound not initialized" << std::endl;
                return -1;
            }

            // Clear the buffer before starting
            LPVOID audioPtr1, audioPtr2;
            DWORD audioBytes1, audioBytes2;

            HRESULT hr = secondaryBuffer_->Lock(0, bufferBytes_, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
            if (SUCCEEDED(hr))
            {
                std::memset(audioPtr1, 0, audioBytes1);
                if (audioPtr2 != nullptr)
                    std::memset(audioPtr2, 0, audioBytes2);
                secondaryBuffer_->Unlock(audioPtr1, audioBytes1, audioPtr2, audioBytes2);
            }

            // Start playback
            hr = secondaryBuffer_->Play(0, 0, DSBPLAY_LOOPING);
            if (FAILED(hr))
            {
                std::cerr << "DirectSound Play failed: 0x" << std::hex << hr << std::endl;
                return -1;
            }

            shouldStop_.store(false);
            isRunning_.store(true);
            thread_ = std::thread(&DirectSoundBackend::threadFunc, this);

            return 0;
        }

        void stop() override
        {
            if (!isRunning_.load())
                return;

            shouldStop_.store(true);
            if (thread_.joinable())
                thread_.join();

            if (secondaryBuffer_)
            {
                secondaryBuffer_->Stop();
                secondaryBuffer_->Release();
                secondaryBuffer_ = nullptr;
            }

            if (primaryBuffer_)
            {
                primaryBuffer_->Release();
                primaryBuffer_ = nullptr;
            }

            if (directSound_)
            {
                directSound_->Release();
                directSound_ = nullptr;
            }

            isRunning_.store(false);
        }

        void setCallback(AudioCallback cb) override
        {
            callback_ = cb;
        }

    private:
        void threadFunc()
        {
            // High-precision timer for consistent 10.67ms callback intervals
            const auto exactBufferPeriod = std::chrono::nanoseconds(10666666); // 512/48000 * 1e9
            auto nextCallbackTime = std::chrono::high_resolution_clock::now();

            while (!shouldStop_.load())
            {
                if (callback_ && secondaryBuffer_)
                {
                    // Wait for precise timing
                    std::this_thread::sleep_until(nextCallbackTime);
                    nextCallbackTime += exactBufferPeriod;

                    // Get current play and write positions
                    DWORD playPosition, writePosition;
                    HRESULT hr = secondaryBuffer_->GetCurrentPosition(&playPosition, &writePosition);
                    if (FAILED(hr))
                    {
                        continue; // Skip this cycle but maintain timing
                    }

                    // Calculate how much data we can safely write
                    DWORD bytesToWrite = calculateBytesToWrite(playPosition, writePosition);

                    if (bytesToWrite > 0)
                    {
                        // Allocate float buffer for callback
                        int totalSamples = bufferSize_ * channels_;
                        std::unique_ptr<float[]> floatBuffer = std::make_unique<float[]>(totalSamples);

                        // Get audio data from callback - this now happens at precise intervals
                        callback_(floatBuffer.get(), bufferSize_, channels_);

                        // Convert and write directly to DirectSound buffer
                        writeFloatToBuffer(floatBuffer.get(), totalSamples);
                    }
                    // Note: No sleep here - timing is controlled by sleep_until above
                }
                else
                {
                    // Only sleep when no callback is set to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    nextCallbackTime = std::chrono::high_resolution_clock::now();
                }
            }
        }

        DWORD calculateBytesToWrite(DWORD playPosition, DWORD currentWritePosition)
        {
            // Calculate how far ahead we should stay from play position (safety margin)
            DWORD safetyMargin = chunkSize_ * 2; // Stay 2 chunks ahead
            DWORD safeWritePosition = (playPosition + safetyMargin) % bufferBytes_;

            // Calculate the distance from current write position to safe position
            DWORD bytesAvailable;
            if (safeWritePosition >= writePosition_)
            {
                bytesAvailable = safeWritePosition - writePosition_;
            }
            else
            {
                // Wrap around case
                bytesAvailable = (bufferBytes_ - writePosition_) + safeWritePosition;
            }

            // Only write if we have at least one complete chunk available
            // This prevents partial writes that can cause the periodic pattern
            if (bytesAvailable >= chunkSize_)
            {
                return chunkSize_; // Always write exactly one chunk
            }
            else
            {
                return 0; // Not enough space, wait for next iteration
            }
        }

        void writeFloatToBuffer(float *floatData, int sampleCount)
        {
            LPVOID audioPtr1, audioPtr2;
            DWORD audioBytes1, audioBytes2;

            DWORD bytesToWrite = sampleCount * sizeof(short);

            HRESULT hr = secondaryBuffer_->Lock(writePosition_, bytesToWrite,
                                                &audioPtr1, &audioBytes1,
                                                &audioPtr2, &audioBytes2, 0);
            if (SUCCEEDED(hr))
            {
                // Use optimized conversion with proper quantization and dithering
                short *out1 = reinterpret_cast<short *>(audioPtr1);
                int samples1 = audioBytes1 / sizeof(short);

                // Convert first buffer segment
                PolyphonicOscillator::convertAndQuantize(floatData, out1, samples1);

                // Handle buffer wrap-around if needed
                if (audioPtr2 && audioBytes2 > 0)
                {
                    short *out2 = reinterpret_cast<short *>(audioPtr2);
                    int samples2 = audioBytes2 / sizeof(short);

                    // Convert second buffer segment
                    PolyphonicOscillator::convertAndQuantize(floatData + samples1, out2, samples2);
                }

                secondaryBuffer_->Unlock(audioPtr1, audioBytes1, audioPtr2, audioBytes2);

                // Update write position
                writePosition_ = (writePosition_ + bytesToWrite) % bufferBytes_;
            }
            else
            {
                std::cerr << "DirectSound buffer lock failed: 0x" << std::hex << hr << std::endl;
            }
        }

        void writeToBuffer(short *audioData, DWORD bytesToWrite)
        {
            LPVOID audioPtr1, audioPtr2;
            DWORD audioBytes1, audioBytes2;

            HRESULT hr = secondaryBuffer_->Lock(writePosition_, bytesToWrite,
                                                &audioPtr1, &audioBytes1,
                                                &audioPtr2, &audioBytes2, 0);
            if (SUCCEEDED(hr))
            {
                // Copy audio data to locked buffer
                std::memcpy(audioPtr1, audioData, audioBytes1);
                if (audioPtr2 != nullptr)
                {
                    std::memcpy(audioPtr2, reinterpret_cast<char *>(audioData) + audioBytes1, audioBytes2);
                }

                secondaryBuffer_->Unlock(audioPtr1, audioBytes1, audioPtr2, audioBytes2);

                // Update write position
                writePosition_ = (writePosition_ + bytesToWrite) % bufferBytes_;
            }
            else
            {
                std::cerr << "DirectSound buffer lock failed: 0x" << std::hex << hr << std::endl;
            }
        }

        // DirectSound objects
        LPDIRECTSOUND directSound_;
        LPDIRECTSOUNDBUFFER primaryBuffer_;
        LPDIRECTSOUNDBUFFER secondaryBuffer_;

        // Audio configuration
        const int sampleRate_;
        const int channels_;
        const int bitsPerSample_;
        const int bufferSize_;

        // Buffer management
        DWORD bufferBytes_;
        DWORD frameSize_;
        DWORD chunkSize_;
        DWORD writePosition_;

        // Threading and synchronization
        AudioCallback callback_;
        std::thread thread_;
        std::atomic<bool> shouldStop_;
        std::atomic<bool> isRunning_;
    };

    std::unique_ptr<IAudioBackend> createDirectSoundBackend()
    {
        return std::make_unique<DirectSoundBackend>();
    }

} // namespace synth
