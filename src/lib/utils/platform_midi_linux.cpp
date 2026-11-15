/**
 * @file platform_midi_linux.cpp
 * @brief Linux ALSA implementation of MidiInterface
 */
#ifdef __linux__
#include "platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <vector>
#include <string>
#include <poll.h>
namespace synth {
    namespace
    {
        struct LinuxMidiEndpoint
        {
            std::string name;     // display name
            std::string rawPath;  // e.g. "hw:3,0,0" (may be empty)
            int card{-1};
            int device{-1};
            int subdevice{-1};
            int seqClient{-1};
            int seqPort{-1};
        };

        std::vector<LinuxMidiEndpoint> g_linuxMidiEndpoints;

        /// Enumerate real MIDI *input* endpoints only (no bare sound cards).
        void refreshLinuxMidiEndpoints()
        {
            g_linuxMidiEndpoints.clear();

            // ── 1) Raw MIDI subdevices ──────────────────────────────────────
            int card = -1;
            while (snd_card_next(&card) >= 0 && card >= 0)
            {
                char cardId[32];
                std::snprintf(cardId, sizeof(cardId), "hw:%d", card);
                snd_ctl_t *ctl = nullptr;
                if (snd_ctl_open(&ctl, cardId, 0) < 0)
                    continue;

                snd_ctl_card_info_t *cardInfo;
                snd_ctl_card_info_alloca(&cardInfo);
                std::string cardName = "card" + std::to_string(card);
                if (snd_ctl_card_info(ctl, cardInfo) >= 0)
                {
                    const char *n = snd_ctl_card_info_get_name(cardInfo);
                    if (n && n[0])
                        cardName = n;
                }

                int dev = -1;
                while (snd_ctl_rawmidi_next_device(ctl, &dev) >= 0 && dev >= 0)
                {
                    snd_rawmidi_info_t *info;
                    snd_rawmidi_info_alloca(&info);
                    snd_rawmidi_info_set_device(info, dev);
                    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
                    if (snd_ctl_rawmidi_info(ctl, info) < 0)
                        continue;

                    const int subs = static_cast<int>(snd_rawmidi_info_get_subdevices_count(info));
                    const char *midiName = snd_rawmidi_info_get_name(info);
                    for (int sub = 0; sub < std::max(1, subs); ++sub)
                    {
                        // Confirm subdevice exists for input
                        snd_rawmidi_info_set_subdevice(info, sub);
                        snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
                        if (snd_ctl_rawmidi_info(ctl, info) < 0)
                            continue;

                        LinuxMidiEndpoint ep;
                        ep.card = card;
                        ep.device = dev;
                        ep.subdevice = sub;
                        ep.rawPath = "hw:" + std::to_string(card) + "," +
                                     std::to_string(dev) + "," + std::to_string(sub);
                        ep.name = cardName;
                        if (midiName && midiName[0])
                        {
                            ep.name += " — ";
                            ep.name += midiName;
                        }
                        ep.name += " [" + ep.rawPath + "]";
                        g_linuxMidiEndpoints.push_back(ep);
                    }
                }
                snd_ctl_close(ctl);
            }

            // ── 2) Annotate with ALSA sequencer client:port when possible ───
            snd_seq_t *seq = nullptr;
            if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) >= 0)
            {
                snd_seq_client_info_t *ci;
                snd_seq_client_info_alloca(&ci);
                snd_seq_client_info_set_client(ci, -1);
                while (snd_seq_query_next_client(seq, ci) >= 0)
                {
                    const int client = snd_seq_client_info_get_client(ci);
                    const int cardOfClient = snd_seq_client_info_get_card(ci);
                    if (cardOfClient < 0)
                    {
                        // a program's own MIDI out (a software sequencer, a modular, a virtual keyboard):
                        // an endpoint of its own, opened through the sequencer (no raw path). Not the
                        // kernel's System and Midi Through clients, nor this program's own reader.
                        const char *cname = snd_seq_client_info_get_name(ci);
                        const std::string clientName = cname ? cname : "";
                        if (snd_seq_client_info_get_type(ci) != SND_SEQ_USER_CLIENT || clientName == "SynthMIDIReader" ||
                            clientName == "ObsSynth4D" || clientName == "ObsDrums") // our own readers
                            continue;
                        snd_seq_port_info_t *upi;
                        snd_seq_port_info_alloca(&upi);
                        snd_seq_port_info_set_client(upi, client);
                        snd_seq_port_info_set_port(upi, -1);
                        while (snd_seq_query_next_port(seq, upi) >= 0)
                        {
                            const unsigned ucaps = snd_seq_port_info_get_capability(upi);
                            if (!(ucaps & SND_SEQ_PORT_CAP_READ) || !(ucaps & SND_SEQ_PORT_CAP_SUBS_READ) ||
                                (ucaps & SND_SEQ_PORT_CAP_NO_EXPORT))
                                continue;
                            LinuxMidiEndpoint ep;
                            ep.seqClient = client;
                            ep.seqPort = snd_seq_port_info_get_port(upi);
                            const char *pname = snd_seq_port_info_get_name(upi);
                            ep.name = clientName + " — " + (pname ? pname : "") + " [seq " +
                                      std::to_string(ep.seqClient) + ":" + std::to_string(ep.seqPort) + "]";
                            g_linuxMidiEndpoints.push_back(ep);
                        }
                        continue;
                    }

                    snd_seq_port_info_t *pi;
                    snd_seq_port_info_alloca(&pi);
                    snd_seq_port_info_set_client(pi, client);
                    snd_seq_port_info_set_port(pi, -1);
                    while (snd_seq_query_next_port(seq, pi) >= 0)
                    {
                        const unsigned caps = snd_seq_port_info_get_capability(pi);
                        if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ))
                            continue;
                        const int port = snd_seq_port_info_get_port(pi);

                        // Match raw endpoints on same card; fill first free seq slot
                        for (auto &ep : g_linuxMidiEndpoints)
                        {
                            if (ep.card == cardOfClient && ep.seqClient < 0)
                            {
                                ep.seqClient = client;
                                ep.seqPort = port;
                                break;
                            }
                        }
                    }
                }
                snd_seq_close(seq);
            }
        }

        const LinuxMidiEndpoint *linuxEndpoint(int deviceId)
        {
            if (g_linuxMidiEndpoints.empty())
                refreshLinuxMidiEndpoints();
            if (deviceId < 0 || deviceId >= static_cast<int>(g_linuxMidiEndpoints.size()))
                return nullptr;
            return &g_linuxMidiEndpoints[static_cast<size_t>(deviceId)];
        }
    } // namespace

    MidiInterface::MidiInterface()
        : midi_in_(nullptr), midi_out_(nullptr), seq_handle_(nullptr), seq_port_(-1),
          seq_dest_client_(-1), seq_dest_port_(-1), use_sequencer_(false),
          should_stop_(false), isOpen_(false), isOutputOpen_(false), useKeyboardFallback_(false),
          keyboard_should_stop_(false), currentScrollController_(1)
    {
        initializeKeyMapping();
        lastScrollTime_ = std::chrono::steady_clock::now();
    }

    MidiInterface::~MidiInterface()
    {
        close();
        closeOutput();
        disableKeyboardFallback();
    }

    int MidiInterface::init()
    {
        return 0; // No special initialization needed for Linux
    }

    int MidiInterface::open(int deviceId)
    {
        if (isOpen_)
            return 0;

        // First try to open a real MIDI device
        bool midiDeviceOpened = false;

        refreshLinuxMidiEndpoints();
        const LinuxMidiEndpoint *ep = linuxEndpoint(deviceId);
        if (!ep)
        {
            std::cerr << "[MIDI] Invalid device id " << deviceId
                      << " (have " << g_linuxMidiEndpoints.size() << " real MIDI ports)\n";
            // deviceId < 0 → explicit keyboard fallback request
            if (deviceId < 0)
            {
                enableKeyboardFallback();
                isOpen_ = true;
                return 0;
            }
            return -1;
        }

        std::cout << "[MIDI] Opening \"" << ep->name << "\" path=" << ep->rawPath << std::endl;

        // 1) Prefer raw MIDI when we have a concrete hw path
        int err = -1;
        if (!ep->rawPath.empty())
        {
            err = snd_rawmidi_open(&midi_in_, nullptr, ep->rawPath.c_str(), SND_RAWMIDI_NONBLOCK);
            if (err < 0 && err == -EBUSY)
            {
                forceResetMidiDevice(deviceId);
                err = snd_rawmidi_open(&midi_in_, nullptr, ep->rawPath.c_str(), SND_RAWMIDI_NONBLOCK);
            }
            if (err >= 0)
            {
                drainMidiDevice();
                use_sequencer_ = false;
                should_stop_.store(false);
                midi_thread_ = std::thread(&MidiInterface::midiThreadFunction, this);
                midiDeviceOpened = true;
                std::cout << "[MIDI] rawmidi OK: " << ep->rawPath << std::endl;
            }
            else
            {
                std::cerr << "[MIDI] rawmidi open " << ep->rawPath << ": " << snd_strerror(err) << std::endl;
            }
        }

        // 2) Fallback: ALSA sequencer subscribe to this endpoint's client:port
        if (!midiDeviceOpened)
        {
            std::cout << "[MIDI] Trying ALSA sequencer for device " << deviceId << "..." << std::endl;
            if (trySequencerOpen(deviceId))
            {
                should_stop_.store(false);
                midi_thread_ = std::thread(&MidiInterface::midiThreadFunction, this);
                midiDeviceOpened = true;
                std::cout << "[MIDI] sequencer OK" << std::endl;
            }
        }

        if (!midiDeviceOpened)
        {
            std::cerr << "[MIDI] Cannot open device " << deviceId << " (" << ep->name << ")\n";
            return -1; // do NOT fake success / keyboard fallback here
        }


        isOpen_ = true;
        return 0;
    }

    int MidiInterface::openVirtual(const char *clientName)
    {
        if (isOpen_)
            return 0;
        if (snd_seq_open(&seq_handle_, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
        {
            std::cerr << "[MIDI] Failed to open ALSA sequencer\n";
            return -1;
        }
        snd_seq_nonblock(seq_handle_, 1);
        snd_seq_set_client_name(seq_handle_, clientName);
        seq_port_ = snd_seq_create_simple_port(
            seq_handle_, "input",
            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        if (seq_port_ < 0)
        {
            snd_seq_close(seq_handle_);
            seq_handle_ = nullptr;
            return -1;
        }
        use_sequencer_ = true;
        should_stop_.store(false);
        midi_thread_ = std::thread(&MidiInterface::midiThreadFunction, this);
        isOpen_ = true;
        std::cout << "[MIDI] listening on " << clientName << ":input (client " << snd_seq_client_id(seq_handle_)
                  << "): connect a sequencer or a controller to it" << std::endl;
        return 0;
    }

    std::vector<MidiInterface::Source> MidiInterface::sources() const
    {
        std::vector<Source> v;
        if (!seq_handle_ || seq_port_ < 0)
            return v;
        const int me = snd_seq_client_id(seq_handle_);
        snd_seq_t *q = nullptr;
        if (snd_seq_open(&q, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
            return v;
        snd_seq_client_info_t *ci;
        snd_seq_port_info_t *pi;
        snd_seq_client_info_alloca(&ci);
        snd_seq_port_info_alloca(&pi);
        snd_seq_client_info_set_client(ci, -1);
        while (snd_seq_query_next_client(q, ci) >= 0)
        {
            const int c = snd_seq_client_info_get_client(ci);
            if (c == me || c == SND_SEQ_CLIENT_SYSTEM)
                continue;
            const std::string cname = snd_seq_client_info_get_name(ci);
            if (cname.rfind("Midi Through", 0) == 0 || cname.rfind("PipeWire", 0) == 0)
                continue;
            std::vector<Source> ports;
            snd_seq_port_info_set_client(pi, c);
            snd_seq_port_info_set_port(pi, -1);
            while (snd_seq_query_next_port(q, pi) >= 0)
            {
                const unsigned caps = snd_seq_port_info_get_capability(pi);
                if ((caps & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) != (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ) ||
                    (caps & SND_SEQ_PORT_CAP_NO_EXPORT))
                    continue;
                Source s;
                s.client = c;
                s.port = snd_seq_port_info_get_port(pi);
                s.name = snd_seq_port_info_get_name(pi);
                snd_seq_addr_t sender{static_cast<unsigned char>(c), static_cast<unsigned char>(s.port)};
                snd_seq_port_subscribe_t *sub;
                snd_seq_port_subscribe_alloca(&sub);
                snd_seq_addr_t dest{static_cast<unsigned char>(me), static_cast<unsigned char>(seq_port_)};
                snd_seq_port_subscribe_set_sender(sub, &sender);
                snd_seq_port_subscribe_set_dest(sub, &dest);
                s.connected = snd_seq_get_port_subscription(q, sub) == 0;
                ports.push_back(s);
            }
            for (Source &s : ports) // one port: the client's name; several: the port's too
                s.name = ports.size() == 1 || s.name.find(cname) == 0 ? (ports.size() == 1 ? cname : s.name) : cname + " · " + s.name;
            v.insert(v.end(), ports.begin(), ports.end());
        }
        snd_seq_close(q);
        return v;
    }

    bool MidiInterface::connectSource(int client, int port, bool on)
    {
        if (!seq_handle_ || seq_port_ < 0)
            return false;
        snd_seq_t *q = nullptr;
        if (snd_seq_open(&q, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
            return false;
        snd_seq_addr_t sender{static_cast<unsigned char>(client), static_cast<unsigned char>(port)};
        snd_seq_addr_t dest{static_cast<unsigned char>(snd_seq_client_id(seq_handle_)), static_cast<unsigned char>(seq_port_)};
        snd_seq_port_subscribe_t *sub;
        snd_seq_port_subscribe_alloca(&sub);
        snd_seq_port_subscribe_set_sender(sub, &sender);
        snd_seq_port_subscribe_set_dest(sub, &dest);
        const bool now = snd_seq_get_port_subscription(q, sub) == 0;
        int r = 0;
        if (on && !now)
            r = snd_seq_subscribe_port(q, sub);
        else if (!on && now)
            r = snd_seq_unsubscribe_port(q, sub);
        snd_seq_close(q);
        return r == 0;
    }

    bool MidiInterface::testMidiConnection()
    {
        if (!isOpen_)
        {
            return false;
        }

        // If using keyboard fallback, consider it always connected
        if (useKeyboardFallback_)
        {
            return true;
        }

        // For Linux, check if we can still poll the device
        if (!midi_in_)
        {
            return false;
        }

        struct pollfd pfd;
        int count = snd_rawmidi_poll_descriptors_count(midi_in_);
        if (count != 1)
        {
            return false;
        }

        int result = snd_rawmidi_poll_descriptors(midi_in_, &pfd, 1);
        return (result >= 0);
    }

    void MidiInterface::close()
    {
        if (!isOpen_)
            return;

        // Disable keyboard fallback if enabled
        disableKeyboardFallback();

        should_stop_.store(true);
        if (midi_thread_.joinable())
        {
            midi_thread_.join();
        }

        if (use_sequencer_)
        {
            // Close sequencer interface
            if (seq_handle_)
            {
                if (seq_port_ >= 0)
                {
                    snd_seq_delete_simple_port(seq_handle_, seq_port_);
                }
                snd_seq_close(seq_handle_);
                seq_handle_ = nullptr;
                seq_port_ = -1;
                use_sequencer_ = false;
            }
        }
        else if (midi_in_)
        {
            // Close raw MIDI interface
            drainMidiDevice();
            snd_rawmidi_close(midi_in_);
            midi_in_ = nullptr;
        }

        isOpen_ = false;
    }

    void MidiInterface::drainMidiDevice()
    {
        if (!midi_in_)
            return;

        uint8_t buffer[1024];
        ssize_t bytes_read;
        int attempts = 0;
        const int max_attempts = 100; // Prevent infinite loop

        // Read and discard any pending data
        do
        {
            bytes_read = snd_rawmidi_read(midi_in_, buffer, sizeof(buffer));
            attempts++;
        } while (bytes_read > 0 && attempts < max_attempts);

        if (attempts >= max_attempts)
        {
            std::cout << "Warning: MIDI device drain reached maximum attempts" << std::endl;
        }
    }

    bool MidiInterface::probeMidiDevice()
    {
        if (!midi_in_)
            return false;

        // Try to get device info to verify connection
        snd_rawmidi_info_t *info;
        snd_rawmidi_info_alloca(&info);

        int err = snd_rawmidi_info(midi_in_, info);
        if (err < 0)
        {
            std::cout << "MIDI device info probe failed: " << snd_strerror(err) << std::endl;
            return false;
        }

        // Check if we can poll the device
        struct pollfd pfd;
        int count = snd_rawmidi_poll_descriptors_count(midi_in_);
        if (count != 1)
        {
            std::cout << "MIDI device poll descriptor count unexpected: " << count << std::endl;
            return false;
        }

        if (snd_rawmidi_poll_descriptors(midi_in_, &pfd, 1) < 0)
        {
            std::cout << "MIDI device poll descriptor setup failed" << std::endl;
            return false;
        }

        std::cout << "MIDI device probe successful, ready for input" << std::endl;
        return true;
    }

    void MidiInterface::forceResetMidiDevice(int deviceId)
    {
        (void)deviceId; // Unused on Linux
        std::cout << "Attempting force reset (non-blocking)..." << std::endl;

        // Don't use blocking operations that might hang
        // Just sleep a bit to let any processes release the device
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "Force reset completed (minimal delay)" << std::endl;
    }

    bool MidiInterface::trySequencerOpen(int deviceId)
    {
        const LinuxMidiEndpoint *ep = linuxEndpoint(deviceId);
        if (!ep)
            return false;

        if (snd_seq_open(&seq_handle_, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
        {
            std::cerr << "[MIDI] Failed to open ALSA sequencer\n";
            return false;
        }
        snd_seq_nonblock(seq_handle_, 1);
        snd_seq_set_client_name(seq_handle_, "SynthMIDIReader");

        seq_port_ = snd_seq_create_simple_port(
            seq_handle_, "input",
            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        if (seq_port_ < 0)
        {
            std::cerr << "[MIDI] Failed to create sequencer port\n";
            snd_seq_close(seq_handle_);
            seq_handle_ = nullptr;
            return false;
        }

        auto subscribe = [&](int client, int port) -> bool
        {
            if (client < 0 || port < 0)
                return false;
            snd_seq_addr_t sender{};
            sender.client = static_cast<unsigned char>(client);
            sender.port = static_cast<unsigned char>(port);
            snd_seq_addr_t dest{};
            dest.client = static_cast<unsigned char>(snd_seq_client_id(seq_handle_));
            dest.port = static_cast<unsigned char>(seq_port_);
            snd_seq_port_subscribe_t *subs = nullptr;
            snd_seq_port_subscribe_alloca(&subs);
            snd_seq_port_subscribe_set_sender(subs, &sender);
            snd_seq_port_subscribe_set_dest(subs, &dest);
            int err = snd_seq_subscribe_port(seq_handle_, subs);
            if (err < 0)
            {
                std::cerr << "[MIDI] seq subscribe " << client << ":" << port
                          << " failed: " << snd_strerror(err) << std::endl;
                return false;
            }
            std::cout << "[MIDI] seq subscribed " << client << ":" << port << std::endl;
            return true;
        };

        bool connected = false;

        // Prefer annotated client:port from enumeration
        if (ep->seqClient >= 0)
            connected = subscribe(ep->seqClient, ep->seqPort) || connected;

        // Subscribe every readable hardware port (optionally filter by card). Not for a program's own port
        // (no card, no raw path: a software sequencer, a modular): that one is the endpoint, and the sweep
        // would also take every other program's output.
        const bool softwareEndpoint = ep->card < 0 && ep->rawPath.empty() && ep->seqClient >= 0;
        if (!(softwareEndpoint && connected))
        {
            snd_seq_client_info_t *ci;
            snd_seq_client_info_alloca(&ci);
            snd_seq_client_info_set_client(ci, -1);
            while (snd_seq_query_next_client(seq_handle_, ci) >= 0)
            {
                const int client = snd_seq_client_info_get_client(ci);
                const int cardOf = snd_seq_client_info_get_card(ci);
                // Prefer same card; if card metadata missing (-1), still try kernel clients
                if (ep->card >= 0 && cardOf >= 0 && cardOf != ep->card)
                    continue;
                if (client < 16) // skip System(0) / reserved low ids except we'll filter by caps
                    continue;

                snd_seq_port_info_t *pi;
                snd_seq_port_info_alloca(&pi);
                snd_seq_port_info_set_client(pi, client);
                snd_seq_port_info_set_port(pi, -1);
                while (snd_seq_query_next_port(seq_handle_, pi) >= 0)
                {
                    const unsigned caps = snd_seq_port_info_get_capability(pi);
                    if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ) ||
                        (caps & SND_SEQ_PORT_CAP_NO_EXPORT))
                        continue;
                    // Skip Midi Through unless nothing else works
                    const char *pname = snd_seq_port_info_get_name(pi);
                    if (pname && std::string(pname).find("Midi Through") != std::string::npos)
                        continue;
                    if (subscribe(client, snd_seq_port_info_get_port(pi)))
                        connected = true;
                }
            }
        }

        if (!connected)
        {
            std::cerr << "[MIDI] sequencer: no ports subscribed for device " << deviceId << std::endl;
            snd_seq_delete_simple_port(seq_handle_, seq_port_);
            snd_seq_close(seq_handle_);
            seq_handle_ = nullptr;
            seq_port_ = -1;
            return false;
        }

        use_sequencer_ = true;
        seq_dest_client_ = ep->seqClient;
        seq_dest_port_ = ep->seqPort;
        return true;
    }

    void MidiInterface::midiThreadFunction()
    {
        if (use_sequencer_)
        {
            // Non-blocking sequencer poll
            while (!should_stop_.load())
            {
                snd_seq_event_t *ev = nullptr;
                int r = snd_seq_event_input(seq_handle_, &ev);
                if (r >= 0 && ev)
                {
                    eventSource_ = ev->source.client;
                    switch (ev->type)
                    {
                    case SND_SEQ_EVENT_NOTEON:
                        if (ev->data.note.velocity > 0)
                            parseMidiMessage(MIDI_NOTE_ON | ev->data.note.channel,
                                             ev->data.note.note, ev->data.note.velocity);
                        else
                            parseMidiMessage(MIDI_NOTE_OFF | ev->data.note.channel,
                                             ev->data.note.note, 0);
                        break;
                    case SND_SEQ_EVENT_NOTEOFF:
                        parseMidiMessage(MIDI_NOTE_OFF | ev->data.note.channel,
                                         ev->data.note.note, ev->data.note.velocity);
                        break;
                    case SND_SEQ_EVENT_CONTROLLER:
                        parseMidiMessage(MIDI_CONTROL | ev->data.control.channel,
                                         static_cast<uint8_t>(ev->data.control.param),
                                         static_cast<uint8_t>(ev->data.control.value));
                        break;
                    case SND_SEQ_EVENT_PGMCHANGE:
                        parseMidiMessage(MIDI_PROGRAM | ev->data.control.channel,
                                         static_cast<uint8_t>(ev->data.control.value), 0);
                        break;
                    case SND_SEQ_EVENT_CHANPRESS:
                        parseMidiMessage(MIDI_CHANNEL_AFTERTOUCH | ev->data.control.channel,
                                         static_cast<uint8_t>(ev->data.control.value), 0);
                        break;
                    case SND_SEQ_EVENT_KEYPRESS:
                        parseMidiMessage(MIDI_POLYPHONIC_AFTERTOUCH | ev->data.note.channel,
                                         ev->data.note.note, ev->data.note.velocity);
                        break;
                    case SND_SEQ_EVENT_PITCHBEND:
                    {
                        // ALSA pitchbend is signed 14-bit (-8192..8191); MIDI is 0..16383
                        int v = ev->data.control.value + 8192;
                        v = std::max(0, std::min(16383, v));
                        parseMidiMessage(MIDI_PITCH_BEND | ev->data.control.channel,
                                         static_cast<uint8_t>(v & 0x7F),
                                         static_cast<uint8_t>((v >> 7) & 0x7F));
                        break;
                    }
                    case SND_SEQ_EVENT_SYSEX:
                        if (ev->data.ext.len > 0 && ev->data.ext.ptr && sysExCallback_)
                        {
                            std::vector<uint8_t> sysexData(
                                static_cast<const uint8_t *>(ev->data.ext.ptr),
                                static_cast<const uint8_t *>(ev->data.ext.ptr) + ev->data.ext.len);
                            sysExCallback_(sysexData);
                        }
                        break;
                    default:
                        break;
                    }
                    snd_seq_free_event(ev);
                }
                else if (r == -EAGAIN)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else if (r < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
        else
        {
            // Raw MIDI with running-status support
            uint8_t msg[3]{};
            int need = 0; // expected data bytes remaining
            int got = 0;  // data bytes collected
            uint8_t runningStatus = 0;
            int consecutive_errors = 0;
            const int max_consecutive_errors = 10;

            auto dataBytesFor = [](uint8_t status) -> int
            {
                switch (status & 0xF0)
                {
                case MIDI_PROGRAM:
                case MIDI_CHANNEL_AFTERTOUCH:
                    return 1;
                case MIDI_NOTE_OFF:
                case MIDI_NOTE_ON:
                case MIDI_POLYPHONIC_AFTERTOUCH:
                case MIDI_CONTROL:
                case MIDI_PITCH_BEND:
                    return 2;
                default:
                    return 0; // real-time / sys common handled loosely
                }
            };

            while (!should_stop_.load())
            {
                struct pollfd pfd{};
                int count = snd_rawmidi_poll_descriptors_count(midi_in_);
                if (count < 1)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                if (snd_rawmidi_poll_descriptors(midi_in_, &pfd, 1) < 0)
                {
                    consecutive_errors++;
                    if (consecutive_errors >= max_consecutive_errors)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }

                int poll_result = poll(&pfd, 1, 5);
                if (poll_result <= 0)
                    continue;

                uint8_t byte = 0;
                ssize_t n = snd_rawmidi_read(midi_in_, &byte, 1);
                if (n <= 0)
                {
                    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        consecutive_errors++;
                        if (consecutive_errors >= max_consecutive_errors)
                            break;
                    }
                    continue;
                }
                consecutive_errors = 0;

                if (byte & 0x80)
                {
                    // Status byte (skip pure real-time 0xF8-0xFF except start of sys)
                    if (byte >= 0xF8)
                        continue; // real-time: ignore
                    runningStatus = byte;
                    need = dataBytesFor(runningStatus);
                    got = 0;
                    if (need == 0)
                    {
                        parseMidiMessage(runningStatus, 0, 0);
                    }
                    continue;
                }

                // Data byte
                if (runningStatus == 0)
                    continue; // orphan data
                if (got < 2)
                    msg[got++] = byte;
                if (got >= need)
                {
                    parseMidiMessage(runningStatus,
                                     need >= 1 ? msg[0] : 0,
                                     need >= 2 ? msg[1] : 0);
                    got = 0; // keep running status for next message
                }
            }
        }

        std::cout << "MIDI thread exiting..." << std::endl;
    }

    std::vector<std::string> MidiInterface::getDeviceList()
    {
        std::vector<std::string> devices;

        // Only real MIDI input endpoints (not every sound card)
        refreshLinuxMidiEndpoints();
        devices.reserve(g_linuxMidiEndpoints.size());
        for (const auto &ep : g_linuxMidiEndpoints)
            devices.push_back(ep.name);

        return devices;
    }

    int MidiInterface::getDeviceCount()
    {
        return static_cast<int>(getDeviceList().size());
    }

    std::string MidiInterface::getDeviceName(int deviceId)
    {
        auto devices = getDeviceList();
        if (deviceId >= 0 && deviceId < static_cast<int>(devices.size()))
        {
            return devices[deviceId];
        }
        return "Unknown Device";
    }

    int MidiInterface::openOutput(int deviceId)
    {
        if (isOutputOpen_)
        {
            return 0; // Already open
        }

        // If we're already using sequencer for input, try to use it for output too
        if (use_sequencer_ && seq_handle_)
        {
            std::cout << "Using existing ALSA sequencer connection for MIDI output" << std::endl;

            // Try to find and connect to the target device using sequencer
            snd_seq_client_info_t *client_info;
            snd_seq_port_info_t *port_info;

            snd_seq_client_info_alloca(&client_info);
            snd_seq_port_info_alloca(&port_info);

            // Look for the target device by scanning clients
            snd_seq_client_info_set_client(client_info, -1);
            int current_device = 0;

            while (snd_seq_query_next_client(seq_handle_, client_info) >= 0)
            {
                int client_id = snd_seq_client_info_get_client(client_info);

                // Skip our own client and system clients
                if (client_id < 16)
                    continue;

                snd_seq_port_info_set_client(port_info, client_id);
                snd_seq_port_info_set_port(port_info, -1);

                while (snd_seq_query_next_port(seq_handle_, port_info) >= 0)
                {
                    unsigned int capability = snd_seq_port_info_get_capability(port_info);

                    // Check if this port can accept output (WRITE capability)
                    if ((capability & SND_SEQ_PORT_CAP_WRITE) &&
                        (capability & SND_SEQ_PORT_CAP_SUBS_WRITE))
                    {
                        if (current_device == deviceId)
                        {
                            seq_dest_client_ = client_id;
                            seq_dest_port_ = snd_seq_port_info_get_port(port_info);

                            std::cout << "Found target device for output: client " << seq_dest_client_
                                      << ", port " << seq_dest_port_ << std::endl;

                            isOutputOpen_ = true;
                            return 0;
                        }
                        current_device++;
                    }
                }
            }

            std::cerr << "Could not find MIDI device " << deviceId << " for sequencer output" << std::endl;
            // Fall through to try raw MIDI
        }

        // Try to open raw MIDI output device
        std::vector<std::string> device_paths;
        device_paths.push_back("hw:" + std::to_string(deviceId) + ",0,0"); // Standard format
        device_paths.push_back("hw:" + std::to_string(deviceId) + ",0");   // Without subdevice
        device_paths.push_back("hw:" + std::to_string(deviceId));          // Just card

        // Try direct device node access
        std::string direct_device = "/dev/snd/midiC" + std::to_string(deviceId) + "D0";
        device_paths.push_back(direct_device);

        int err = -1;
        std::string successful_device;

        for (const auto &try_device : device_paths)
        {
            std::cout << "Trying to open MIDI output device: " << try_device << std::endl;

            err = snd_rawmidi_open(nullptr, &midi_out_, try_device.c_str(), SND_RAWMIDI_NONBLOCK);
            if (err >= 0)
            {
                successful_device = try_device;
                std::cout << "Successfully opened MIDI output: " << successful_device << std::endl;
                break;
            }

            std::cout << "Failed to open " << try_device << " for output: " << snd_strerror(err) << std::endl;
        }

        if (err < 0)
        {
            std::cerr << "Cannot open MIDI output device " << deviceId << " via any method" << std::endl;
            return -1;
        }

        isOutputOpen_ = true;
        std::cout << "MIDI output device opened successfully: " << successful_device << std::endl;
        return 0;
    }

    void MidiInterface::closeOutput()
    {
        if (!isOutputOpen_)
        {
            return;
        }

        if (midi_out_)
        {
            snd_rawmidi_close(midi_out_);
            midi_out_ = nullptr;
        }

        isOutputOpen_ = false;
        std::cout << "MIDI output device closed" << std::endl;
    }

    int MidiInterface::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        note = std::min(note, static_cast<uint8_t>(127));
        velocity = std::min(velocity, static_cast<uint8_t>(127));

        // Use sequencer if available and we have a destination
        if (use_sequencer_ && seq_handle_ && seq_dest_client_ >= 0)
        {
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);

            snd_seq_ev_set_source(&ev, seq_port_);
            snd_seq_ev_set_dest(&ev, seq_dest_client_, seq_dest_port_);
            snd_seq_ev_set_direct(&ev);

            snd_seq_ev_set_noteon(&ev, channel, note, velocity);

            int err = snd_seq_event_output_direct(seq_handle_, &ev);
            if (err < 0)
            {
                std::cerr << "Failed to send MIDI Note On via sequencer: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Flush the output to ensure immediate sending
            snd_seq_drain_output(seq_handle_);
            return 0;
        }
        // Use raw MIDI if available
        else if (midi_out_)
        {
            uint8_t buffer[3] = {
                static_cast<uint8_t>(0x90 | channel),
                note,
                velocity};

            ssize_t written = snd_rawmidi_write(midi_out_, buffer, 3);
            if (written < 3)
            {
                std::cerr << "Failed to send MIDI Note On: " << snd_strerror(written) << std::endl;
                return -1;
            }

            // Drain to ensure immediate sending
            snd_rawmidi_drain(midi_out_);
            return 0;
        }
        else
        {
            std::cerr << "No MIDI output method available" << std::endl;
            return -1;
        }
    }

    int MidiInterface::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        note = std::min(note, static_cast<uint8_t>(127));
        velocity = std::min(velocity, static_cast<uint8_t>(127));

        // Use sequencer if available and we have a destination
        if (use_sequencer_ && seq_handle_ && seq_dest_client_ >= 0)
        {
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);

            snd_seq_ev_set_source(&ev, seq_port_);
            snd_seq_ev_set_dest(&ev, seq_dest_client_, seq_dest_port_);
            snd_seq_ev_set_direct(&ev);

            snd_seq_ev_set_noteoff(&ev, channel, note, velocity);

            int err = snd_seq_event_output_direct(seq_handle_, &ev);
            if (err < 0)
            {
                std::cerr << "Failed to send MIDI Note Off via sequencer: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Flush the output to ensure immediate sending
            snd_seq_drain_output(seq_handle_);
            return 0;
        }
        // Use raw MIDI if available
        else if (midi_out_)
        {
            uint8_t buffer[3] = {
                static_cast<uint8_t>(0x80 | channel),
                note,
                velocity};

            ssize_t written = snd_rawmidi_write(midi_out_, buffer, 3);
            if (written < 3)
            {
                std::cerr << "Failed to send MIDI Note Off: " << snd_strerror(written) << std::endl;
                return -1;
            }

            // Drain to ensure immediate sending
            snd_rawmidi_drain(midi_out_);
            return 0;
        }
        else
        {
            std::cerr << "No MIDI output method available" << std::endl;
            return -1;
        }
    }

    int MidiInterface::sendControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        controller = std::min(controller, static_cast<uint8_t>(127));
        value = std::min(value, static_cast<uint8_t>(127));

        // Use sequencer if available and we have a destination
        if (use_sequencer_ && seq_handle_ && seq_dest_client_ >= 0)
        {
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);

            snd_seq_ev_set_source(&ev, seq_port_);
            snd_seq_ev_set_dest(&ev, seq_dest_client_, seq_dest_port_);
            snd_seq_ev_set_direct(&ev);

            snd_seq_ev_set_controller(&ev, channel, controller, value);

            int err = snd_seq_event_output_direct(seq_handle_, &ev);
            if (err < 0)
            {
                std::cerr << "Failed to send MIDI Control Change via sequencer: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Flush the output to ensure immediate sending
            snd_seq_drain_output(seq_handle_);
            return 0;
        }
        // Use raw MIDI if available
        else if (midi_out_)
        {
            uint8_t buffer[3] = {
                static_cast<uint8_t>(0xB0 | channel),
                controller,
                value};

            ssize_t written = snd_rawmidi_write(midi_out_, buffer, 3);
            if (written < 3)
            {
                std::cerr << "Failed to send MIDI Control Change: " << snd_strerror(written) << std::endl;
                return -1;
            }

            // Drain to ensure immediate sending
            snd_rawmidi_drain(midi_out_);
            return 0;
        }
        else
        {
            std::cerr << "No MIDI output method available" << std::endl;
            return -1;
        }
    }

    int MidiInterface::sendProgramChange(uint8_t channel, uint8_t program)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        program = std::min(program, static_cast<uint8_t>(127));

        // Use sequencer if available and we have a destination
        if (use_sequencer_ && seq_handle_ && seq_dest_client_ >= 0)
        {
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);

            snd_seq_ev_set_source(&ev, seq_port_);
            snd_seq_ev_set_dest(&ev, seq_dest_client_, seq_dest_port_);
            snd_seq_ev_set_direct(&ev);

            snd_seq_ev_set_pgmchange(&ev, channel, program);

            int err = snd_seq_event_output_direct(seq_handle_, &ev);
            if (err < 0)
            {
                std::cerr << "Failed to send MIDI Program Change via sequencer: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Flush the output to ensure immediate sending
            snd_seq_drain_output(seq_handle_);
            return 0;
        }
        // Use raw MIDI if available
        else if (midi_out_)
        {
            uint8_t buffer[2] = {
                static_cast<uint8_t>(0xC0 | channel),
                program};

            ssize_t written = snd_rawmidi_write(midi_out_, buffer, 2);
            if (written < 2)
            {
                std::cerr << "Failed to send MIDI Program Change: " << snd_strerror(written) << std::endl;
                return -1;
            }

            // Drain to ensure immediate sending
            snd_rawmidi_drain(midi_out_);
            return 0;
        }
        else
        {
            std::cerr << "No MIDI output method available" << std::endl;
            return -1;
        }
    }

    int MidiInterface::sendSysEx(const std::vector<uint8_t> &data)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        if (data.empty())
        {
            std::cerr << "Empty SysEx data" << std::endl;
            return -1;
        }

        // Validate SysEx format (should start with 0xF0 and end with 0xF7)
        if (data[0] != 0xF0)
        {
            std::cerr << "Invalid SysEx message: must start with 0xF0" << std::endl;
            return -1;
        }

        if (data.back() != 0xF7)
        {
            std::cerr << "Invalid SysEx message: must end with 0xF7" << std::endl;
            return -1;
        }

        // Use sequencer if available and we have a destination
        if (use_sequencer_ && seq_handle_ && seq_dest_client_ >= 0)
        {
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);

            snd_seq_ev_set_source(&ev, seq_port_);
            snd_seq_ev_set_dest(&ev, seq_dest_client_, seq_dest_port_);
            snd_seq_ev_set_direct(&ev);

            // Set SysEx data
            snd_seq_ev_set_sysex(&ev, data.size(), const_cast<uint8_t *>(data.data()));

            int err = snd_seq_event_output_direct(seq_handle_, &ev);
            if (err < 0)
            {
                std::cerr << "Failed to send SysEx via sequencer: " << snd_strerror(err) << std::endl;
                return -1;
            }

            // Flush the output to ensure immediate sending
            snd_seq_drain_output(seq_handle_);
            return 0;
        }
        // Use raw MIDI if available
        else if (midi_out_)
        {
            ssize_t written = snd_rawmidi_write(midi_out_, data.data(), data.size());
            if (written < static_cast<ssize_t>(data.size()))
            {
                std::cerr << "Failed to send complete SysEx message: " << snd_strerror(written) << std::endl;
                return -1;
            }

            // Drain to ensure immediate sending
            snd_rawmidi_drain(midi_out_);
            return 0;
        }
        else
        {
            std::cerr << "No MIDI output method available for SysEx" << std::endl;
            return -1;
        }
    }

} // namespace synth
#endif
