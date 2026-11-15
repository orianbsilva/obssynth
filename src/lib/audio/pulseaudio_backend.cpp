#include "audio_backend.hpp"
#ifdef __linux__
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>

namespace synth
{
    /**
     * Low-latency PulseAudio playback via pa_simple.
     *
     * bufferSize_ is FRAMES (samples per channel), matching ASIO/WaveOut/ALSA.
     * tlength is sized for ~3 periods of latency.
     */
    class PulseAudioBackend : public IAudioBackend
    {
    public:
        explicit PulseAudioBackend(int framesPerBuffer = 256)
            : pa_simple_(nullptr), shouldStop_(false), isRunning_(false),
              sampleRate_(48000), channels_(2), framesPerBuffer_(framesPerBuffer)
        {
        }

        ~PulseAudioBackend() override { stop(); }

        int init() override
        {
            pa_sample_spec ss;
            ss.format = PA_SAMPLE_S16LE;
            ss.channels = static_cast<uint8_t>(channels_);
            ss.rate = sampleRate_;

            // Low-latency buffer attributes
            // bytes per period = frames * channels * sizeof(int16)
            const uint32_t bytesPerPeriod =
                static_cast<uint32_t>(framesPerBuffer_) * channels_ * sizeof(int16_t);
            pa_buffer_attr ba;
            ba.maxlength = bytesPerPeriod * 8; // hard cap
            ba.tlength = bytesPerPeriod * 3;   // target latency ~3 periods
            ba.prebuf = bytesPerPeriod;        // start after 1 period
            ba.minreq = bytesPerPeriod / 2;    // request more often
            ba.fragsize = (uint32_t)-1;        // playback N/A

            int error = 0;
            pa_simple_ = pa_simple_new(
                nullptr,
                audioClientName().c_str(),
                PA_STREAM_PLAYBACK,
                audioRequest().device.empty() ? nullptr : audioRequest().device.c_str(), // "" the default sink
                "Audio Output",
                &ss,
                nullptr,
                &ba,
                &error);

            if (!pa_simple_)
            {
                std::cerr << "[Pulse] connection failed: " << pa_strerror(error) << std::endl;
                return -1;
            }

            const size_t samples = static_cast<size_t>(framesPerBuffer_) * channels_;
            floatBuf_.assign(samples, 0.0f);
            pcmBuf_.assign(samples, 0);

            const float periodMs = 1000.0f * static_cast<float>(framesPerBuffer_) / static_cast<float>(sampleRate_);
            std::cout << "[Pulse] frames/buffer=" << framesPerBuffer_
                      << " (~" << periodMs << " ms), rate=" << sampleRate_
                      << " Hz, ch=" << channels_ << std::endl;

            return 0;
        }

        int start() override
        {
            if (isRunning_.load())
                return 0;
            shouldStop_.store(false);
            isRunning_.store(true);
            thread_ = std::thread(&PulseAudioBackend::threadFunc, this);
            return 0;
        }

        void stop() override
        {
            if (!isRunning_.load())
                return;
            shouldStop_.store(true);
            if (thread_.joinable())
                thread_.join();
            if (pa_simple_)
            {
                pa_simple_free(pa_simple_);
                pa_simple_ = nullptr;
            }
            isRunning_.store(false);
        }

        void setCallback(AudioCallback cb) override { callback_ = std::move(cb); }

        float getSampleRate() const override { return static_cast<float>(sampleRate_); }

    private:
        void threadFunc()
        {
            while (!shouldStop_.load())
            {
                if (callback_)
                {
                    std::fill(floatBuf_.begin(), floatBuf_.end(), 0.0f);
                    callback_(floatBuf_.data(), framesPerBuffer_, channels_);
                }

                for (size_t i = 0; i < floatBuf_.size(); ++i)
                {
                    float s = floatBuf_[i];
                    if (s > 1.0f)
                        s = 1.0f;
                    else if (s < -1.0f)
                        s = -1.0f;
                    pcmBuf_[i] = static_cast<short>(s * 32767.0f);
                }

                int error = 0;
                // Blocks until Pulse can accept data — paces the loop
                if (pa_simple_write(pa_simple_, pcmBuf_.data(),
                                    pcmBuf_.size() * sizeof(short), &error) < 0)
                {
                    std::cerr << "[Pulse] write failed: " << pa_strerror(error) << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }

        pa_simple *pa_simple_;
        AudioCallback callback_;
        std::thread thread_;
        std::atomic<bool> shouldStop_;
        std::atomic<bool> isRunning_;

        const uint32_t sampleRate_;
        const uint16_t channels_;
        const int framesPerBuffer_;

        std::vector<float> floatBuf_;
        std::vector<short> pcmBuf_;
    };

    std::unique_ptr<IAudioBackend> createPulseAudioBackend()
    {
        return std::make_unique<PulseAudioBackend>(audioRequest().frames > 0 ? audioRequest().frames : 256);
    }

    std::vector<AudioDevice> listPulseSinks()
    {
        // the asynchronous API, run to its end here: connect, ask for the sinks, gather them
        std::vector<AudioDevice> v;
        pa_mainloop *ml = pa_mainloop_new();
        if (!ml)
            return v;
        pa_context *ctx = pa_context_new(pa_mainloop_get_api(ml), audioClientName().c_str());
        if (!ctx || pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
        {
            if (ctx)
                pa_context_unref(ctx);
            pa_mainloop_free(ml);
            return v;
        }
        struct Ask
        {
            std::vector<AudioDevice> *out;
            bool done = false;
        } ask{&v};
        pa_operation *op = nullptr;
        for (int i = 0; i < 400 && !ask.done; ++i) // at most ~2 s
        {
            pa_mainloop_iterate(ml, 0, nullptr);
            const pa_context_state_t st = pa_context_get_state(ctx);
            if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED)
                break;
            if (st == PA_CONTEXT_READY && !op)
                op = pa_context_get_sink_info_list(ctx, [](pa_context *, const pa_sink_info *si, int eol, void *data)
                                                   {
                    auto *a = static_cast<Ask *>(data);
                    if (eol)
                        a->done = true;
                    else if (si)
                        a->out->push_back({si->name, si->description ? si->description : si->name}); }, &ask);
            if (!ask.done)
                pa_msleep(5);
        }
        if (op)
            pa_operation_unref(op);
        pa_context_disconnect(ctx);
        pa_context_unref(ctx);
        pa_mainloop_free(ml);
        return v;
    }

} // namespace synth

#endif // __linux__
