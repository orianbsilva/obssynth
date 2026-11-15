#include "audio_sink.hpp"
#include "audio_backend.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#elif defined(__linux__)
#include <alsa/asoundlib.h>
#endif

namespace synth
{
    // TripleBuffer implementation kept in header for legacy; no change here (moved to waveout backend)

    // Constructor
    AudioSink::AudioSink()
#ifdef _WIN32
        : backendType_(AudioBackendType::WaveOut)
#elif defined(__linux__)
#ifdef HAVE_PULSEAUDIO
        : backendType_(AudioBackendType::PulseAudio)
#else
        : backendType_(AudioBackendType::ALSA)
#endif
#endif
    {
    }

    // Destructor
    AudioSink::~AudioSink() {}

    // Move constructor
    AudioSink::AudioSink(AudioSink &&other) noexcept : backend_(std::move(other.backend_)), backendType_(other.backendType_), audioCallback_(std::move(other.audioCallback_)) {}

    // Move assignment operator
    AudioSink &AudioSink::operator=(AudioSink &&other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            backend_ = std::move(other.backend_);
            backendType_ = other.backendType_;
            audioCallback_ = std::move(other.audioCallback_);
        }
        return *this;
    }

    // Initialize the audio sink
    int AudioSink::init()
    {
        switch (backendType_)
        {
#ifdef _WIN32
        case AudioBackendType::WaveOut:
            backend_ = createWaveOutBackend();
            break;
        case AudioBackendType::WASAPI:
            backend_ = createWasapiBackend(false);
            break;
        case AudioBackendType::ASIO:
            backend_ = createAsioBackend();
            break;
        case AudioBackendType::DirectSound:
            backend_ = createDirectSoundBackend();
            break;
#elif defined(__linux__)
        case AudioBackendType::ALSA:
            backend_ = createAlsaBackend();
            break;
#ifdef HAVE_PULSEAUDIO
        case AudioBackendType::PulseAudio:
            backend_ = createPulseAudioBackend();
            break;
#endif
#endif
        }
        if (!backend_)
            return -1;
        backend_->setCallback(audioCallback_);

        int result = backend_->init();

#ifdef __linux__
#ifdef HAVE_PULSEAUDIO
        // Fallback to ALSA if PulseAudio fails
        if (result != 0 && backendType_ == AudioBackendType::PulseAudio)
        {
            std::cout << "Falling back to ALSA..." << std::endl;
            backend_.reset();
            backendType_ = AudioBackendType::ALSA;
            backend_ = createAlsaBackend();
            if (backend_)
            {
                backend_->setCallback(audioCallback_);
                result = backend_->init();
            }
        }
#endif
#endif

        return result;
    }

#ifdef _WIN32
    // Wave output callback procedure
    void CALLBACK AudioSink::waveOutProc(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {}
#endif

    // Audio production thread function - generates audio data
    void AudioSink::audioProductionThreadFunction() {}

    // Audio playback thread function - handles hardware playback
    void AudioSink::audioPlaybackThreadFunction() {}

    // Combined audio thread function that manages both production and playback
    void AudioSink::audioThreadFunction() {}

    // Start the audio output
    int AudioSink::start() { return backend_ ? backend_->start() : -1; }

    // Start with callback for continuous audio generation
    int AudioSink::startWithCallback(std::function<void(float *, int, int)> callback)
    {
        setAudioCallback(callback);
        return start();
    }

    // Set audio callback
    void AudioSink::setAudioCallback(std::function<void(float *, int, int)> callback)
    {
        audioCallback_ = callback;
        if (backend_)
            backend_->setCallback(audioCallback_);
    }

    // Stop audio output
    void AudioSink::stop()
    {
        if (backend_)
            backend_->stop();
    }

    // Clean up resources
    void AudioSink::cleanup()
    {
        stop();
        backend_.reset();
    }

    // List available audio devices (static method)
    int AudioSink::listDevices()
    {
#ifdef _WIN32
        // Windows audio device enumeration
        std::cout << "\nAvailable audio output devices:\n";

        int deviceCount = 0;

        // Enumerate WaveOut devices
        UINT numDevices = waveOutGetNumDevs();
        if (numDevices > 0)
        {
            std::cout << "WaveOut Devices:\n";
            for (UINT i = 0; i < numDevices; i++)
            {
                WAVEOUTCAPS caps;
                if (waveOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                {
                    std::cout << "  [" << deviceCount++ << "] " << caps.szPname << " (WaveOut)\n";
                }
            }
        }

        // Add WASAPI device
        std::cout << "  [" << deviceCount++ << "] Windows Audio Session API (WASAPI)\n";

        // Add ASIO device
        std::cout << "  [" << deviceCount++ << "] Audio Stream Input/Output (ASIO)\n";

        // Add DirectSound device
        std::cout << "  [" << deviceCount++ << "] DirectSound (DSound)\n";

        if (deviceCount == 0)
        {
            std::cout << "  No audio output devices found.\n";
        }

        return deviceCount;

#elif defined(__linux__)
        // Linux ALSA audio device enumeration
        std::cout << "\nAvailable audio output devices:\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

        int card = -1;
        int device_count = 0;

        // Enumerate sound cards
        while (snd_card_next(&card) >= 0 && card >= 0)
        {
            snd_ctl_t *ctl;
            snd_ctl_card_info_t *info;
            snd_ctl_card_info_alloca(&info);

            std::string name = "hw:" + std::to_string(card);
            if (snd_ctl_open(&ctl, name.c_str(), 0) >= 0)
            {
                if (snd_ctl_card_info(ctl, info) >= 0)
                {
                    const char *card_name = snd_ctl_card_info_get_name(info);
                    const char *card_longname = snd_ctl_card_info_get_longname(info);

                    std::cout << "  [" << device_count << "] " << card_name;
                    if (card_longname && strlen(card_longname) > 0 && strcmp(card_name, card_longname) != 0)
                    {
                        std::cout << " (" << card_longname << ")";
                    }
                    std::cout << " [hw:" << card << "]\n";
                    device_count++;
                }
                snd_ctl_close(ctl);
            }
        }

        // Add default device
        std::cout << "  [" << device_count << "] Default ALSA Device [default]\n";
        device_count++;

        // Add PulseAudio device if available
#ifdef HAVE_PULSEAUDIO
        std::cout << "  [" << device_count << "] PulseAudio [pulse]\n";
        device_count++;
#endif

        if (device_count == 0)
        {
            std::cout << "  No audio output devices found.\n";
        }

        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        return device_count;

#else
        std::cout << "\nAudio device enumeration not implemented for this platform.\n";
        return 0;
#endif
    }

    // Set the audio backend type
    void AudioSink::setBackend(AudioBackendType type)
    {
        backendType_ = type;
    }

} // namespace synth
