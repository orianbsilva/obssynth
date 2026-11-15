#pragma once
// The products' audio out and MIDI in, as v1 (synth_view) had them on each platform, for every panel, and what the
// person chose in SETUP (setup_page.hpp), kept per product in <data>/<product>/settings.json.
//   Audio. Windows: ASIO through VB-Audio's VASIO-32 (the lowest latency), else DirectSound, else WaveOut.
//          Linux: PulseAudio (the desktop's sink), else ALSA. The output is named for the product.
//          A backend, an output and a buffer chosen go first; the usual order follows if they fail.
//   MIDI.  Linux: a port of the product's own (ObsDrums:input) that gridseq and controllers join; SETUP joins
//          or parts the others. Windows (WinMM has no virtual ports): every MIDI input device opened,
//          controllers first (Launch*, APC, *key*), WinMM's "MIDIINn (name)" duplicates skipped, but those SETUP
//          turned off. A channel (or all) filters what comes in.
// A source, for the handlers: Linux, the ALSA sequencer client; Windows, the device's index (the same numbers
// ccov::seqClients() gives). The handlers run on the MIDI threads, one at a time (a lock: on Windows each device
// has its own), so a single-producer queue behind them stays single-producer.
#include "../../lib/audio/audio_backend.hpp"
#include "../../lib/audio/audio_sink.hpp"
#include "../../lib/utils/platform_midi_reader.hpp"
#include "../../vendor/json.hpp"
#include "data_home.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace synth::io
{
    // ── what the person chose ─────────────────────────────────────────────────────────────────────
    struct Settings
    {
        std::string backend;                // "" the usual order; else a name of audioBackends()
        std::string device;                 // "" the backend's default
        int frames = 0;                     // 0 the backend's own
        int channel = -1;                   // -1 all; 0..15
        std::map<std::string, bool> inputs; // MIDI sources by name, as SETUP left them (absent: as found)
        std::string ccMap;                  // "" the controller present's
        std::string palette;                // "" the system's colours; else one of Carbonian's palettes by name
    };

    inline std::string settingsPath(const std::string &product) { return dataHome() + "/" + product + "/settings.json"; }

    inline Settings loadSettings(const std::string &product)
    {
        Settings s;
        std::ifstream in(settingsPath(product));
        if (!in)
            return s;
        try
        {
            nlohmann::json j;
            in >> j;
            const auto &a = j.value("audio", nlohmann::json::object());
            s.backend = a.value("backend", "");
            s.device = a.value("device", "");
            s.frames = a.value("frames", 0);
            const auto &m = j.value("midi", nlohmann::json::object());
            s.channel = m.value("channel", -1);
            s.ccMap = m.value("ccMap", "");
            if (m.contains("inputs"))
                for (auto it = m["inputs"].begin(); it != m["inputs"].end(); ++it)
                    s.inputs[it.key()] = it.value().get<bool>();
            s.palette = j.value("look", nlohmann::json::object()).value("palette", "");
        }
        catch (...)
        {
        }
        return s;
    }

    inline bool saveSettings(const std::string &product, const Settings &s)
    {
        nlohmann::json j;
        j["audio"] = {{"backend", s.backend}, {"device", s.device}, {"frames", s.frames}};
        nlohmann::json inputs = nlohmann::json::object();
        for (const auto &[k, v] : s.inputs)
            inputs[k] = v;
        j["midi"] = {{"channel", s.channel}, {"ccMap", s.ccMap}, {"inputs", inputs}};
        j["look"] = {{"palette", s.palette}};
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(settingsPath(product)).parent_path(), ec);
        std::ofstream out(settingsPath(product));
        if (!out)
            return false;
        out << j.dump(2);
        return static_cast<bool>(out);
    }

    // ── audio ─────────────────────────────────────────────────────────────────────────────────────
    struct Backend
    {
        const char *name;
        AudioBackendType type;
    };

    // the backends this build has, in the order tried when nothing is chosen
    inline std::vector<Backend> audioBackends()
    {
#ifdef _WIN32
        return {{"ASIO", AudioBackendType::ASIO}, {"DIRECTSOUND", AudioBackendType::DirectSound}, {"WAVEOUT", AudioBackendType::WaveOut}};
#else
        std::vector<Backend> v;
#ifdef HAVE_PULSEAUDIO
        v.push_back({"PULSE", AudioBackendType::PulseAudio});
#endif
        v.push_back({"ALSA", AudioBackendType::ALSA});
        return v;
#endif
    }

    // the outputs a backend can open ("" first: its default)
    inline std::vector<AudioDevice> audioDevices(const std::string &backend)
    {
        std::vector<AudioDevice> v{{"", "DEFAULT"}};
#ifdef _WIN32
        if (backend == "ASIO")
        {
            v.clear(); // ASIO has no default: a driver; VASIO-32 first when present
            for (const std::string &d : listAsioDrivers())
                v.push_back({d, d});
            std::stable_partition(v.begin(), v.end(), [](const AudioDevice &d) { return d.id.find("VASIO-32") != std::string::npos; });
        }
#else
#ifdef HAVE_PULSEAUDIO
        if (backend == "PULSE")
            for (const AudioDevice &d : listPulseSinks())
                v.push_back(d);
#endif
        if (backend == "ALSA")
            for (const AudioDevice &d : listAlsaDevices())
                if (d.id != "default")
                    v.push_back(d);
#endif
        return v;
    }

    inline std::vector<int> audioBuffers() { return {64, 128, 256, 512, 1024}; }

    // the output, opened and reopened (SETUP) while the program runs: the callback goes with it
    class AudioOut
    {
    public:
        ~AudioOut() { close(); }

        bool open(const char *product, const Settings &s, std::function<void(float *, int, int)> callback)
        {
            close();
            audioClientName() = product;
            callback_ = std::move(callback);
            std::vector<Backend> order = audioBackends();
            auto chosen = std::find_if(order.begin(), order.end(), [&](const Backend &b) { return s.backend == b.name; });
            if (chosen != order.end())
                std::rotate(order.begin(), chosen, chosen + 1);
            for (const Backend &b : order)
            {
                const bool mine = s.backend == b.name;
                audioRequest() = {mine ? s.device : std::string(), s.frames};
#ifdef _WIN32
                if (b.type == AudioBackendType::ASIO)
                    selectAsioDriverByName(mine && !s.device.empty() ? s.device : std::string("VASIO-32"));
#endif
                sink_ = std::make_unique<AudioSink>();
                sink_->setBackend(b.type);
                if (sink_->init() != 0)
                {
                    std::cerr << "[" << product << "] " << b.name << " failed" << std::endl;
                    sink_.reset();
                    continue;
                }
                sink_->setAudioCallback(callback_);
                if (sink_->start() != 0)
                {
                    sink_.reset();
                    continue;
                }
                backend_ = b.name;
                device_ = audioRequest().device;
                return true;
            }
            backend_.clear();
            return false;
        }

        void close()
        {
            if (sink_)
                sink_->stop();
            sink_.reset();
            backend_.clear();
        }

        bool running() const { return sink_ != nullptr; }
        const std::string &backend() const { return backend_; } // what opened ("" nothing)
        const std::string &device() const { return device_; }

    private:
        std::unique_ptr<AudioSink> sink_;
        std::function<void(float *, int, int)> callback_;
        std::string backend_, device_;
    };

    // ── MIDI ──────────────────────────────────────────────────────────────────────────────────────
    struct MidiHandlers
    {
        std::function<void(const MidiNote &, int source)> note;
        std::function<void(uint8_t channel, uint8_t cc, uint8_t value, int source)> control;
        std::function<void(uint8_t channel, int value)> bend;
        std::function<void(uint8_t channel, uint8_t pressure)> pressure;
        std::function<void(uint8_t channel, uint8_t note, uint8_t pressure)> polyPressure;
    };

    class MidiIn
    {
    public:
        ~MidiIn() { close(); }

        // the product's MIDI in; false when nothing opened. Windows: the devices SETUP turned off stay closed.
        bool open(const char *product, MidiHandlers h, const Settings &s = {})
        {
            h_ = std::move(h);
            product_ = product;
            channel_.store(s.channel);
#ifdef _WIN32
            const int n = MidiInterface::getDeviceCount();
            std::vector<int> order;
            for (int i = 0; i < n; ++i)
                order.push_back(i);
            auto score = [](std::string u)
            {
                for (char &c : u)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                return u.find("LAUNCH") != std::string::npos ? 0 : u.find("APC") != std::string::npos ? 1 : u.find("KEY") != std::string::npos ? 2 : 10;
            };
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return score(MidiInterface::getDeviceName(a)) < score(MidiInterface::getDeviceName(b)); });
            std::vector<std::string> opened;
            for (int i : order)
            {
                const std::string name = MidiInterface::getDeviceName(i), base = baseName(name);
                if (name.rfind("MIDIIN", 0) == 0 && std::find(opened.begin(), opened.end(), base) != opened.end())
                    continue;
                const auto off = s.inputs.find(name);
                if (off != s.inputs.end() && !off->second)
                    continue;
                if (openDevice(i))
                    opened.push_back(base);
            }
#else
            auto dev = std::make_unique<MidiInterface>();
            wire(*dev, [](const MidiInterface &m) { return m.eventSource(); });
            if (dev->openVirtual(product) == 0)
                devs_.push_back({-1, std::move(dev)});
            for (const MidiInterface::Source &src : sources()) // what SETUP left joined or parted
            {
                const auto want = s.inputs.find(src.name);
                if (want != s.inputs.end() && want->second != src.connected)
                    setSource(src, want->second);
            }
#endif
            return !devs_.empty();
        }

        void close()
        {
            for (auto &d : devs_)
                d.second->close();
            devs_.clear();
        }

        void setChannel(int ch) { channel_.store(ch); } // -1 all

        // what can play the product: Linux, the sequencer's sources and whether each is joined to the port;
        // Windows, the devices and whether each is open (client: the device's index)
        std::vector<MidiInterface::Source> sources() const
        {
#ifdef _WIN32
            std::vector<MidiInterface::Source> v;
            for (int i = 0; i < MidiInterface::getDeviceCount(); ++i)
            {
                const std::string name = MidiInterface::getDeviceName(i);
                if (name.rfind("MIDIIN", 0) == 0)
                    continue;
                MidiInterface::Source s;
                s.client = i;
                s.port = 0;
                s.name = name;
                s.connected = std::any_of(devs_.begin(), devs_.end(), [i](const auto &d) { return d.first == i; });
                v.push_back(s);
            }
            return v;
#else
            return devs_.empty() ? std::vector<MidiInterface::Source>{} : devs_.front().second->sources();
#endif
        }

        bool setSource(const MidiInterface::Source &src, bool on)
        {
#ifdef _WIN32
            auto it = std::find_if(devs_.begin(), devs_.end(), [&](const auto &d) { return d.first == src.client; });
            if (!on && it != devs_.end())
            {
                it->second->close();
                devs_.erase(it);
                return true;
            }
            return on && it == devs_.end() ? openDevice(src.client) : true;
#else
            return !devs_.empty() && devs_.front().second->connectSource(src.client, src.port, on);
#endif
        }

    private:
        MidiHandlers h_;
        std::string product_;
        std::vector<std::pair<int, std::unique_ptr<MidiInterface>>> devs_; // (device index, -1 the virtual port)
        std::mutex lock_;
        std::atomic<int> channel_{-1};

        bool heard(uint8_t ch) const
        {
            const int c = channel_.load(std::memory_order_relaxed);
            return c < 0 || c == (ch & 15);
        }

#ifdef _WIN32
        bool openDevice(int i)
        {
            auto dev = std::make_unique<MidiInterface>();
            if (dev->init() != 0)
                return false;
            wire(*dev, [i](const MidiInterface &) { return i; });
            if (dev->open(i) != 0)
                return false;
            std::cerr << "[" << product_ << "] MIDI in: " << MidiInterface::getDeviceName(i) << std::endl;
            devs_.push_back({i, std::move(dev)});
            return true;
        }
#endif

        template <typename Source>
        void wire(MidiInterface &m, Source source)
        {
            MidiInterface *mp = &m;
            m.setNoteCallback([this, mp, source](const MidiNote &n)
                              { if (!heard(n.channel)) return; std::lock_guard<std::mutex> g(lock_); if (h_.note) h_.note(n, source(*mp)); });
            m.setControlCallback([this, mp, source](uint8_t ch, uint8_t cc, uint8_t v)
                                 { if (!heard(ch)) return; std::lock_guard<std::mutex> g(lock_); if (h_.control) h_.control(ch, cc, v, source(*mp)); });
            m.setPitchBendCallback([this](uint8_t ch, int v)
                                   { if (!heard(ch)) return; std::lock_guard<std::mutex> g(lock_); if (h_.bend) h_.bend(ch, v); });
            m.setChannelAftertouchCallback([this](uint8_t ch, uint8_t p)
                                           { if (!heard(ch)) return; std::lock_guard<std::mutex> g(lock_); if (h_.pressure) h_.pressure(ch, p); });
            m.setPolyphonicAftertouchCallback([this](uint8_t ch, uint8_t note, uint8_t p)
                                              { if (!heard(ch)) return; std::lock_guard<std::mutex> g(lock_); if (h_.polyPressure) h_.polyPressure(ch, note, p); });
        }

        // WinMM shows a device twice: "Foo" and "MIDIIN2 (Foo)"
        static std::string baseName(const std::string &n)
        {
            if (n.rfind("MIDIIN", 0) != 0)
                return n;
            const auto open = n.find(" ("), close = n.rfind(')');
            return open != std::string::npos && close != std::string::npos && close > open + 2 ? n.substr(open + 2, close - open - 2) : n;
        }
    };
} // namespace synth::io
