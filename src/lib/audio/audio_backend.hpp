#pragma once
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdint>

namespace synth
{
    // the name the sound server shows for this program's output (PulseAudio's application.name): the product's,
    // set before AudioSink::init(), as the MIDI port is named
    inline std::string &audioClientName()
    {
        static std::string name = "Synth";
        return name;
    }

    // what the person chose (a panel's SETUP), read by the backends at init(): the output (a PulseAudio sink's
    // name, an ALSA PCM, an ASIO driver; "" the system's default) and the buffer in frames (0: the backend's own).
    // Set before AudioSink::init().
    struct AudioRequest
    {
        std::string device;
        int frames = 0;
    };
    inline AudioRequest &audioRequest()
    {
        static AudioRequest r;
        return r;
    }

    // an output a backend can open: what AudioRequest::device takes, and what to show
    struct AudioDevice
    {
        std::string id, name;
    };

    class IAudioBackend
    {
    public:
        using AudioCallback = std::function<void(float *, int, int)>;
        virtual ~IAudioBackend() = default;
        virtual int init() = 0;
        virtual int start() = 0;
        virtual void stop() = 0;
        virtual void setCallback(AudioCallback cb) = 0;
        virtual float getSampleRate() const { return 48000.0f; }
    };

    // Platform-specific backend factory functions
#ifdef _WIN32
    std::unique_ptr<IAudioBackend> createWaveOutBackend();
    std::unique_ptr<IAudioBackend> createWasapiBackend(bool exclusiveMode = false);
    std::unique_ptr<IAudioBackend> createAsioBackend();
    std::unique_ptr<IAudioBackend> createDirectSoundBackend();

    // ASIO driver selection utilities
    std::vector<std::string> listAsioDrivers();
    bool selectAsioDriver(int index);
    bool selectAsioDriverByName(const std::string &nameContains);
#elif defined(__linux__)
    std::unique_ptr<IAudioBackend> createAlsaBackend();
    std::vector<AudioDevice> listAlsaDevices(); // the PCMs ALSA names (default, pipewire, the cards' plughw)
#ifdef HAVE_PULSEAUDIO
    std::unique_ptr<IAudioBackend> createPulseAudioBackend();
    std::vector<AudioDevice> listPulseSinks(); // the sound server's outputs (PipeWire's too)
#endif
#endif

} // namespace synth
