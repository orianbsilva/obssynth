#include "audio_backend.hpp"
#ifdef __linux__
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace synth
{
    /**
     * Low-latency ALSA playback backend.
     *
     * Single realtime-paced thread:
     *   1) call user callback for one period (frames)
     *   2) convert float → s16
     *   3) blocking snd_pcm_writei (self-throttles to hardware rate)
     *
     * Previous dual-thread design free-ran the DSP write path (100 µs sleep)
     * with bufferSize*8 = 48000 frames (~1 s latency) and starved the UI.
     */
    class AlsaBackend : public IAudioBackend
    {
    public:
        // framesPerPeriod: samples per channel per callback (~5.3 ms @ 48 kHz with 256)
        // periodsInBuffer: hardware ring depth (3–4 periods is typical)
        explicit AlsaBackend(unsigned int framesPerPeriod = 256, unsigned int periods = 4)
            : pcm_(nullptr), shouldStop_(false), isRunning_(false),
              sampleRate_(48000), channels_(2),
              framesPerPeriod_(framesPerPeriod), periods_(periods)
        {
        }

        ~AlsaBackend() override { stop(); }

        int init() override
        {
            // Prefer "default" first so PulseAudio/PipeWire routes to the
            // desktop default sink (e.g. Scarlett). hw:0 is often HDMI.
            const char *device_names[] = {
                "default",
                "pipewire",
                "pulse",
                "plughw:1,0", // common USB interface slot
                "plughw:2,0",
                "hw:1,0",
                "hw:2,0",
                "plughw:0,0",
                "hw:0,0",
                "sysdefault",
                nullptr};

            int err = -1;
            if (!audioRequest().device.empty()) // the one the person chose, before the usual ones
            {
                err = snd_pcm_open(&pcm_, audioRequest().device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
                if (err >= 0)
                    std::cout << "[ALSA] Opened device: " << audioRequest().device << std::endl;
                else
                    std::cerr << "[ALSA] Cannot open '" << audioRequest().device << "': " << snd_strerror(err) << std::endl;
            }
            for (int i = 0; err < 0 && device_names[i] != nullptr; ++i)
            {
                err = snd_pcm_open(&pcm_, device_names[i], SND_PCM_STREAM_PLAYBACK, 0);
                if (err >= 0)
                {
                    std::cout << "[ALSA] Opened device: " << device_names[i] << std::endl;
                    break;
                }
                std::cerr << "[ALSA] Cannot open '" << device_names[i]
                          << "': " << snd_strerror(err) << std::endl;
            }
            if (err < 0 || !pcm_)
            {
                std::cerr << "[ALSA] Failed to open any device" << std::endl;
                return -1;
            }

            snd_pcm_hw_params_t *hw;
            snd_pcm_hw_params_alloca(&hw);
            snd_pcm_hw_params_any(pcm_, hw);

            err = snd_pcm_hw_params_set_access(pcm_, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_access: " << snd_strerror(err) << std::endl;
                return -1;
            }

            err = snd_pcm_hw_params_set_format(pcm_, hw, SND_PCM_FORMAT_S16_LE);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_format: " << snd_strerror(err) << std::endl;
                return -1;
            }

            unsigned int rate = sampleRate_;
            int dir = 0;
            err = snd_pcm_hw_params_set_rate_near(pcm_, hw, &rate, &dir);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_rate: " << snd_strerror(err) << std::endl;
                return -1;
            }
            if (rate != sampleRate_)
            {
                std::cout << "[ALSA] Sample rate adjusted " << sampleRate_ << " → " << rate << " Hz" << std::endl;
                sampleRate_ = rate;
            }

            err = snd_pcm_hw_params_set_channels(pcm_, hw, channels_);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_channels: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Target period / buffer (frames = samples per channel)
            snd_pcm_uframes_t period = framesPerPeriod_;
            err = snd_pcm_hw_params_set_period_size_near(pcm_, hw, &period, &dir);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_period_size: " << snd_strerror(err) << std::endl;
                return -1;
            }
            framesPerPeriod_ = static_cast<unsigned int>(period);

            snd_pcm_uframes_t buffer = period * periods_;
            err = snd_pcm_hw_params_set_buffer_size_near(pcm_, hw, &buffer);
            if (err < 0)
            {
                std::cerr << "[ALSA] set_buffer_size: " << snd_strerror(err) << std::endl;
                return -1;
            }

            err = snd_pcm_hw_params(pcm_, hw);
            if (err < 0)
            {
                std::cerr << "[ALSA] hw_params: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Read back actual config
            snd_pcm_hw_params_get_period_size(hw, &period, &dir);
            snd_pcm_hw_params_get_buffer_size(hw, &buffer);
            framesPerPeriod_ = static_cast<unsigned int>(period);
            bufferFrames_ = static_cast<unsigned int>(buffer);

            const float periodMs = 1000.0f * static_cast<float>(framesPerPeriod_) / static_cast<float>(sampleRate_);
            const float bufferMs = 1000.0f * static_cast<float>(bufferFrames_) / static_cast<float>(sampleRate_);
            std::cout << "[ALSA] period=" << framesPerPeriod_ << " frames (" << periodMs
                      << " ms), buffer=" << bufferFrames_ << " frames (" << bufferMs
                      << " ms), rate=" << sampleRate_ << " Hz, ch=" << channels_ << std::endl;

            // Software params
            snd_pcm_sw_params_t *sw;
            snd_pcm_sw_params_alloca(&sw);
            snd_pcm_sw_params_current(pcm_, sw);
            snd_pcm_sw_params_set_start_threshold(pcm_, sw, period);
            snd_pcm_sw_params_set_avail_min(pcm_, sw, period);
            err = snd_pcm_sw_params(pcm_, sw);
            if (err < 0)
            {
                std::cerr << "[ALSA] sw_params: " << snd_strerror(err) << std::endl;
                return -1;
            }

            err = snd_pcm_prepare(pcm_);
            if (err < 0)
            {
                std::cerr << "[ALSA] prepare: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Working buffers: interleaved stereo
            const size_t samples = static_cast<size_t>(framesPerPeriod_) * channels_;
            floatBuf_.assign(samples, 0.0f);
            pcmBuf_.assign(samples, 0);

            return 0;
        }

        int start() override
        {
            if (isRunning_.load())
                return 0;
            shouldStop_.store(false);
            isRunning_.store(true);
            thread_ = std::thread(&AlsaBackend::threadFunc, this);
            return 0;
        }

        void stop() override
        {
            if (!isRunning_.load())
                return;
            shouldStop_.store(true);
            if (thread_.joinable())
                thread_.join();
            if (pcm_)
            {
                snd_pcm_drop(pcm_);
                snd_pcm_close(pcm_);
                pcm_ = nullptr;
            }
            isRunning_.store(false);
        }

        void setCallback(AudioCallback cb) override { callback_ = std::move(cb); }

        float getSampleRate() const override { return static_cast<float>(sampleRate_); }

    private:
        // the audio thread before the UI's and the compositor's: SCHED_FIFO when the user may (RLIMIT_RTPRIO: the
        // audio group's limits.d, rtkit's own), else as nice as allowed (RLIMIT_NICE); said once, not an error
        static void raisePriority()
        {
            rlimit rt{};
            const int want = 70;
            if (getrlimit(RLIMIT_RTPRIO, &rt) == 0 && rt.rlim_cur > 0)
            {
                sched_param sp{};
                sp.sched_priority = static_cast<int>(std::min<rlim_t>(rt.rlim_cur, static_cast<rlim_t>(want)));
                if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0)
                {
                    std::cout << "[ALSA] audio thread: realtime (SCHED_FIFO " << sp.sched_priority << ")" << std::endl;
                    return;
                }
            }
            // not realtime: the best nice this thread may have (Linux: setpriority on a thread id is per thread)
            rlimit ni{};
            const int floor = getrlimit(RLIMIT_NICE, &ni) == 0 && ni.rlim_cur > 0 ? 20 - static_cast<int>(ni.rlim_cur) : 0;
            const int nice = std::max(floor, -11);
            if (nice < 0 && setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), nice) == 0)
                std::cout << "[ALSA] audio thread: nice " << nice << " (no realtime: RLIMIT_RTPRIO is 0)" << std::endl;
            else
                std::cout << "[ALSA] audio thread: normal priority (no RLIMIT_RTPRIO nor RLIMIT_NICE: the audio group's limits give them)" << std::endl;
        }

        void threadFunc()
        {
            pthread_setname_np(pthread_self(), "alsa_audio");
            raisePriority();

            while (!shouldStop_.load())
            {
                // 1) Generate one period (frames = samples per channel)
                if (callback_)
                {
                    std::fill(floatBuf_.begin(), floatBuf_.end(), 0.0f);
                    // Convention: bufferSize arg = frames when channels==2 (matches ASIO/WaveOut)
                    callback_(floatBuf_.data(), static_cast<int>(framesPerPeriod_), static_cast<int>(channels_));
                }

                // 2) float → s16
                const size_t n = floatBuf_.size();
                for (size_t i = 0; i < n; ++i)
                {
                    float s = floatBuf_[i];
                    if (s > 1.0f)
                        s = 1.0f;
                    else if (s < -1.0f)
                        s = -1.0f;
                    pcmBuf_[i] = static_cast<short>(s * 32767.0f);
                }

                // 3) Blocking write — paces the loop to wall-clock audio rate
                snd_pcm_uframes_t toWrite = framesPerPeriod_;
                short *ptr = pcmBuf_.data();
                while (toWrite > 0 && !shouldStop_.load())
                {
                    snd_pcm_sframes_t wrote = snd_pcm_writei(pcm_, ptr, toWrite);
                    if (wrote == -EPIPE)
                    {
                        std::cerr << "[ALSA] underrun, recovering" << std::endl;
                        snd_pcm_prepare(pcm_);
                        continue;
                    }
                    if (wrote == -ESTRPIPE)
                    {
                        while ((wrote = snd_pcm_resume(pcm_)) == -EAGAIN)
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        if (wrote < 0)
                            snd_pcm_prepare(pcm_);
                        continue;
                    }
                    if (wrote < 0)
                    {
                        wrote = snd_pcm_recover(pcm_, static_cast<int>(wrote), 1);
                        if (wrote < 0)
                        {
                            std::cerr << "[ALSA] write error: " << snd_strerror(static_cast<int>(wrote)) << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                            break;
                        }
                        continue;
                    }
                    ptr += wrote * channels_;
                    toWrite -= static_cast<snd_pcm_uframes_t>(wrote);
                }
            }
        }

        snd_pcm_t *pcm_;
        AudioCallback callback_;
        std::thread thread_;
        std::atomic<bool> shouldStop_;
        std::atomic<bool> isRunning_;

        unsigned int sampleRate_;
        unsigned int channels_;
        unsigned int framesPerPeriod_;
        unsigned int periods_;
        unsigned int bufferFrames_{0};

        std::vector<float> floatBuf_;
        std::vector<short> pcmBuf_;
    };

    std::unique_ptr<IAudioBackend> createAlsaBackend()
    {
        // 256 frames @ 48 kHz ≈ 5.3 ms period, ~21 ms buffer (4 periods); or the period the person chose
        const int f = audioRequest().frames;
        return std::make_unique<AlsaBackend>(f > 0 ? static_cast<unsigned int>(f) : 256u, 4);
    }

    std::vector<AudioDevice> listAlsaDevices()
    {
        std::vector<AudioDevice> v;
        void **hints = nullptr;
        if (snd_device_name_hint(-1, "pcm", &hints) < 0)
            return v;
        for (void **h = hints; *h; ++h)
        {
            char *name = snd_device_name_get_hint(*h, "NAME");
            char *desc = snd_device_name_get_hint(*h, "DESC");
            char *io = snd_device_name_get_hint(*h, "IOID");
            if (name && (!io || std::string(io) == "Output"))
            {
                const std::string n = name;
                // the ones worth choosing: the servers, the defaults, each card's plughw (hw: refuses most formats)
                if (n == "default" || n == "pipewire" || n == "pulse" || n.rfind("plughw:", 0) == 0 || n.rfind("sysdefault:", 0) == 0)
                {
                    std::string d = desc ? desc : n;
                    for (auto &c : d)
                        if (c == '\n')
                            c = ' ';
                    v.push_back({n, d});
                }
            }
            free(name);
            free(desc);
            free(io);
        }
        snd_device_name_free_hint(hints);
        return v;
    }

} // namespace synth

#endif // __linux__
