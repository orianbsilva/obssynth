#include "audio_backend.hpp"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <comdef.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <ksmedia.h>

#pragma comment(lib, "ole32.lib")

namespace synth
{
    using Microsoft::WRL::ComPtr;

    class WasapiBackend : public IAudioBackend
    {
    public:
        explicit WasapiBackend(bool exclusive) : exclusive_(exclusive) {}
        ~WasapiBackend() override { stop(); }
        int init() override
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
            {
                std::cerr << "CoInitializeEx failed: 0x" << std::hex << hr << std::dec << '\n';
                return -1;
            }
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator_));
            if (FAILED(hr))
            {
                std::cerr << "MMDeviceEnumerator creation failed\n";
                return -1;
            }
            hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
            if (FAILED(hr))
            {
                std::cerr << "GetDefaultAudioEndpoint failed\n";
                return -1;
            }
            hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client_);
            if (FAILED(hr))
            {
                std::cerr << "IAudioClient activate failed\n";
                return -1;
            }
            WAVEFORMATEX *mix = nullptr;
            hr = client_->GetMixFormat(&mix);
            if (FAILED(hr))
            {
                std::cerr << "GetMixFormat failed\n";
                return -1;
            }

            // First try to use the system's preferred format
            REFERENCE_TIME dur = 10000000; // 1s default
            hr = client_->Initialize(exclusive_ ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED, 0, dur, 0, mix, nullptr);

            if (FAILED(hr))
            {
                std::cerr << "Failed with system format, trying custom format. Error: 0x" << std::hex << hr << std::dec << std::endl;

                // If system format fails, try our custom format but keep stereo
                mix->nSamplesPerSec = 48000; // 48kHz sample rate
                mix->nChannels = 2;          // Force stereo instead of mono
                mix->wFormatTag = WAVE_FORMAT_PCM;
                mix->wBitsPerSample = 16; // 16-bit samples
                mix->nBlockAlign = (mix->nChannels * mix->wBitsPerSample) / 8;
                mix->nAvgBytesPerSec = mix->nSamplesPerSec * mix->nBlockAlign;

                hr = client_->Initialize(exclusive_ ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED, 0, dur, 0, mix, nullptr);

                if (FAILED(hr))
                {
                    std::cerr << "Custom format also failed. Error: 0x" << std::hex << hr << std::dec << std::endl;
                    // Try one more time with float format
                    WAVEFORMATEXTENSIBLE wfx = {};
                    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                    wfx.Format.nChannels = 2;
                    wfx.Format.nSamplesPerSec = 48000; // 48kHz sample rate
                    wfx.Format.wBitsPerSample = 32;
                    wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
                    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
                    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
                    wfx.Samples.wValidBitsPerSample = 32;
                    wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
                    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

                    hr = client_->Initialize(exclusive_ ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED, 0, dur, 0, (WAVEFORMATEX *)&wfx, nullptr);
                }
            }

            CoTaskMemFree(mix);
            if (FAILED(hr))
            {
                std::cerr << "IAudioClient Initialize failed with all formats. Final error: 0x" << std::hex << hr << std::dec << std::endl;
                return -1;
            }
            hr = client_->GetService(IID_PPV_ARGS(&renderClient_));
            if (FAILED(hr))
            {
                std::cerr << "GetService IAudioRenderClient failed\n";
                return -1;
            }
            tripleBuffer_ = std::make_unique<float[]>(6000 * 2); // 6000 frames * 2 channels
            return 0;
        }
        int start() override
        {
            if (running_.load())
                return 0;
            HRESULT hr = client_->Start();
            if (FAILED(hr))
            {
                std::cerr << "IAudioClient Start failed\n";
                return -1;
            }
            running_.store(true);
            thread_ = std::thread(&WasapiBackend::loop, this);
            return 0;
        }
        void stop() override
        {
            if (!running_.load())
                return;
            running_.store(false);
            if (thread_.joinable())
                thread_.join();
            if (client_)
                client_->Stop();
        }
        void setCallback(AudioCallback cb) override { callback_ = cb; }

    private:
        void loop()
        {
            while (running_.load())
            {
                if (callback_)
                {
                    UINT32 padding = 0;
                    UINT32 frames = 0;
                    if (SUCCEEDED(client_->GetCurrentPadding(&padding)))
                    {
                        UINT32 bufferFrames = 0;
                        client_->GetBufferSize(&bufferFrames);
                        frames = bufferFrames - padding;
                        if (frames > 0)
                        {
                            BYTE *data = nullptr;
                            if (SUCCEEDED(renderClient_->GetBuffer(frames, &data)))
                            {
                                // Call the callback to get stereo audio
                                int monoSamples = (int)frames;
                                callback_(tripleBuffer_.get(), monoSamples, 2); // 2 channels (stereo)

                                // Convert stereo float to stereo int16 with proper interleaving
                                short *out = reinterpret_cast<short *>(data);
                                for (UINT32 i = 0; i < frames; ++i)
                                {
                                    // Audio data is already interleaved: L,R,L,R...
                                    float leftSample = tripleBuffer_[i * 2];
                                    float rightSample = tripleBuffer_[i * 2 + 1];

                                    // Clamp the samples
                                    if (leftSample > 1.0f)
                                        leftSample = 1.0f;
                                    else if (leftSample < -1.0f)
                                        leftSample = -1.0f;

                                    if (rightSample > 1.0f)
                                        rightSample = 1.0f;
                                    else if (rightSample < -1.0f)
                                        rightSample = -1.0f;

                                    short leftShort = static_cast<short>(leftSample * 32767.0f);
                                    short rightShort = static_cast<short>(rightSample * 32767.0f);

                                    // Write interleaved stereo data
                                    out[i * 2] = leftShort;      // Left channel
                                    out[i * 2 + 1] = rightShort; // Right channel
                                }
                                renderClient_->ReleaseBuffer(frames, 0);
                            }
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
        bool exclusive_;
        ComPtr<IMMDeviceEnumerator> enumerator_;
        ComPtr<IMMDevice> device_;
        ComPtr<IAudioClient> client_;
        ComPtr<IAudioRenderClient> renderClient_;
        std::unique_ptr<float[]> tripleBuffer_;
        AudioCallback callback_;
        std::thread thread_;
        std::atomic<bool> running_{false};
    };

    std::unique_ptr<IAudioBackend> createWasapiBackend(bool exclusiveMode) { return std::make_unique<WasapiBackend>(exclusiveMode); }

} // namespace synth
