// ObsDrums as a CLAP plugin (ObsDrums.clap): the drum machine's engine (products/obs_drum) behind the CLAP ABI.
//   notes: one note port, CLAP or MIDI; each pad answers to its note (the kit's: 36 kick, 38 snare, 42 the closed
//     hat...); the wheel (CC1) and CC16-17 for the per-hit matrix.
//   params: every pad's engine, circuit, TUNE, DECAY, A, B, BENT, LEVEL, PAN, FX, mute, solo and grain position, and
//     the master's, automatable; PROGRAM (not automatable) loads a factory kit.
//   state: the kit as the app saves it (obsdrums/kits_io.hpp: JSON), or {"kit": "<name>"} for a factory kit.
//   editor: the product's panel (obsdrums --remote), a program of its own (remote_editor.hpp), on Windows inside the
//     host's window: its knobs come back as parameter changes the host records; what has no parameter (a pad's note
//     or choke, the routes, a recording) comes as the whole kit; its pads play. It is told the whole kit on opening,
//     at a program, at a session.
// The recordings (SAMPLE and GRAIN pads) are read on the main thread: the factory kits' at init, a state's when it is
// loaded; the audio thread is only handed pointers. Threads as in ObsSynthCT1.clap.
#include <clap/clap.h>

#include "../../app/obsdrums/kits_io.hpp"
#include "../../app/obsdrums/sound.hpp" // the panel's numbering of a pad's and the master's values (PadParam, MasterParam)
#include "../../app/common/editor_log.hpp"
#include "remote_editor.hpp"
#include "plugin_path.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace synth;
using namespace synth::drums;

namespace
{
    enum PadP { PP_ENGINE, PP_CIRCUIT, PP_TUNE, PP_DECAY, PP_A, PP_B, PP_BENT, PP_LEVEL, PP_PAN, PP_FX, PP_MUTE, PP_SOLO, PP_GRAINPOS, PP_COUNT };
    enum MasterP { MP_DELAY_TIME, MP_DELAY_FB, MP_DELAY_MIX, MP_REV_SIZE, MP_REV_DAMP, MP_REV_MIX, MP_DRIVE, MP_GLUE, MP_TUBE_MIX, MP_VOLUME, MP_COUNT };
    constexpr int NPAD = NUM_DRUM_PADS * PP_COUNT;
    constexpr int NPARAMS = NPAD + MP_COUNT;
    constexpr clap_id PROGRAM_ID = NPARAMS;
    constexpr int QUEUE = 8192;
    const char *kPadNames[PP_COUNT] = {"ENGINE", "CIRCUIT", "TUNE", "DECAY", "A", "B", "BENT", "LEVEL", "PAN", "FX", "MUTE", "SOLO", "GRAIN_POS"};
    const char *kMasterNames[MP_COUNT] = {"DELAY_TIME", "DELAY_FB", "DELAY_MIX", "REV_SIZE", "REV_DAMP", "REV_MIX", "DRIVE", "GLUE", "TUBE_MIX", "VOLUME"};

    struct Info
    {
        float lo, hi;
        bool stepped;
    };
    Info infoOf(int id)
    {
        if (id >= NPAD)
            return id - NPAD == MP_DELAY_TIME ? Info{0.02f, 1.0f, false} : id - NPAD == MP_DELAY_FB ? Info{0, 0.95f, false} : Info{0, 1, false};
        switch (id % PP_COUNT)
        {
        case PP_ENGINE: return {0, static_cast<float>(NUM_DRUM_ENGINES - 1), true};
        case PP_CIRCUIT: return {0, static_cast<float>(NUM_DRUM_KINDS - 1), true};
        case PP_BENT:
        case PP_MUTE:
        case PP_SOLO: return {0, 1, true};
        case PP_PAN: return {-1, 1, false};
        default: return {0, 1, false};
        }
    }
    std::string nameOf(int id)
    {
        if (id >= NPAD)
            return std::string("MASTER_") + kMasterNames[id - NPAD];
        std::string pad = drumPadName(id / PP_COUNT);
        for (auto &c : pad)
            c = c == ' ' ? '_' : c;
        return pad + "_" + kPadNames[id % PP_COUNT];
    }

    // a value of the kit, and into it
    float get(const DrumKit &k, int id)
    {
        if (id >= NPAD)
        {
            const DrumMaster &m = k.master;
            const float v[MP_COUNT] = {m.delayTime, m.delayFeedback, m.delayMix, m.reverbSize, m.reverbDamp, m.reverbMix, m.drive, m.glue, m.tubeMix, m.volume};
            return v[id - NPAD];
        }
        const PadSettings &s = k.pads[static_cast<size_t>(id / PP_COUNT)];
        switch (id % PP_COUNT)
        {
        case PP_ENGINE: return static_cast<float>(s.engine);
        case PP_CIRCUIT: return static_cast<float>(s.circuit);
        case PP_TUNE: return s.p.tune;
        case PP_DECAY: return s.p.decay;
        case PP_A: return s.p.a;
        case PP_B: return s.p.b;
        case PP_BENT: return s.p.bent ? 1.0f : 0.0f;
        case PP_LEVEL: return s.level;
        case PP_PAN: return s.pan;
        case PP_FX: return s.fx;
        case PP_MUTE: return s.mute ? 1.0f : 0.0f;
        case PP_SOLO: return s.solo ? 1.0f : 0.0f;
        default: return s.grainPos;
        }
    }
    void set(DrumKit &k, int id, float v)
    {
        if (id >= NPAD)
        {
            DrumMaster &m = k.master;
            float *f[MP_COUNT] = {&m.delayTime, &m.delayFeedback, &m.delayMix, &m.reverbSize, &m.reverbDamp, &m.reverbMix, &m.drive, &m.glue, &m.tubeMix, &m.volume};
            *f[id - NPAD] = v;
            return;
        }
        PadSettings &s = k.pads[static_cast<size_t>(id / PP_COUNT)];
        const int n = static_cast<int>(std::lround(v));
        switch (id % PP_COUNT)
        {
        case PP_ENGINE: s.engine = static_cast<DrumEngine>(std::clamp(n, 0, NUM_DRUM_ENGINES - 1)); break;
        case PP_CIRCUIT: s.circuit = static_cast<DrumKind>(std::clamp(n, 0, NUM_DRUM_KINDS - 1)); break;
        case PP_TUNE: s.p.tune = v; break;
        case PP_DECAY: s.p.decay = v; break;
        case PP_A: s.p.a = v; break;
        case PP_B: s.p.b = v; break;
        case PP_BENT: s.p.bent = v >= 0.5f; break;
        case PP_LEVEL: s.level = v; break;
        case PP_PAN: s.pan = v; break;
        case PP_FX: s.fx = v; break;
        case PP_MUTE: s.mute = v >= 0.5f; break;
        case PP_SOLO: s.solo = v >= 0.5f; break;
        default: s.grainPos = v; break;
        }
    }

    using Sources = std::array<std::shared_ptr<const GrainSource>, NUM_DRUM_PADS>;

    // the panel's numbering (sound.hpp: PadParam, MasterParam) and the plugin's: the same but for the note and the
    // choke (no parameter here: the kit carries them) and the grain position, last here
    int idOfPanel(int pad, int param)
    {
        if (pad < 0)
            return param >= 0 && param < MP_COUNT ? NPAD + param : -1;
        if (pad >= NUM_DRUM_PADS || param == P_NOTE || param == P_CHOKE || param < 0 || param > P_GRAINPOS)
            return -1;
        return pad * PP_COUNT + (param == P_GRAINPOS ? PP_GRAINPOS : param);
    }

    // the panel: $OBS_DRUMS_PANEL, beside the plugin, in a VST3's Resources, or where it was built
    std::string panelProgram()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries;
        if (const char *e = std::getenv("OBS_DRUMS_PANEL"))
            tries.push_back(e);
#ifdef _WIN32
        const char *exe = "obsdrums.exe";
#else
        const char *exe = "obsdrums";
#endif
        const fs::path dir = synth::clap::pluginDir();
        tries.push_back(dir / exe);
        tries.push_back(dir / ".." / "Resources" / exe);
#ifdef SYNTH_PANEL_DIR
        tries.push_back(fs::path(SYNTH_PANEL_DIR) / exe);
        tries.push_back(fs::path(SYNTH_PANEL_DIR) / "Release" / exe); // Visual Studio's
#endif
        for (const auto &p : tries)
        {
            std::error_code ec;
            if (fs::is_regular_file(p, ec))
                return p.string();
        }
        return {};
    }

    struct Change
    {
        enum Kind : uint8_t { Value, Program, Whole, Hit } kind;
        int id;      // a value's; Hit: the pad
        float value; // Hit: the velocity
    };

    struct Plugin
    {
        clap_plugin_t plugin{};
        const clap_host_t *host = nullptr;
        const clap_host_params_t *hostParams = nullptr;
        std::vector<DrumKit> kits;
        std::vector<Sources> kitSources; // the factory kits' recordings, read at init
        std::unique_ptr<ObsDrums> engine;
        DrumKit kit;     // the audio thread's (and the main thread's before activate)
        Sources sources; // the recordings the kit plays
        DrumKit pending;
        Sources pendingSources;
        std::atomic<bool> pendingReady{false};
        std::array<std::atomic<float>, NPARAMS> value{};
        std::atomic<float> program{0};
        std::atomic<bool> rescan{false};
        std::array<Change, QUEUE> queue{};
        std::atomic<size_t> head{0}, tail{0};
        std::vector<float> buffer;
        std::vector<Sources> kept; // every recording handed over stays alive here (freed on the main thread only)
        // the editor: the panel's changes (the io thread to the audio thread), what the panel was last told, the
        // whole kit both ways (the main thread to the io thread; the panel's to the main thread), the meters
        synth::clap::RemoteEditor editor;
        std::array<Change, QUEUE> fromPanel{};
        std::atomic<size_t> panelHead{0}, panelTail{0};
        std::array<float, NPARAMS> told{};
        std::mutex wholeLock;
        std::string wholeLine;  // "K <json>\n" for the panel, not sent yet
        std::string panelKit;   // the panel's kit, not taken yet (the main thread takes it)
        std::atomic<int> wholeProgram{-1}; // a program the audio thread took: its kit for the panel (the main thread)
        std::atomic<bool> editorClosed{false};
        const clap_host_gui_t *hostGui = nullptr;
        bool embedded = false, wantShown = false;
        uint64_t parent = 0;
        double guiScale = 1.0;
        std::array<std::atomic<float>, NUM_DRUM_PADS> padPeak{}, padFlash{};
        std::array<std::atomic<float>, 2> outPeak{};

        void fromEditor(const Change &c) // the io thread
        {
            const size_t h = panelHead.load(std::memory_order_relaxed);
            if (h - panelTail.load(std::memory_order_acquire) == QUEUE)
                return;
            fromPanel[h % QUEUE] = c;
            panelHead.store(h + 1, std::memory_order_release);
        }
        // the panel's changes on the audio thread: into the kit and the engine, its values to the host as events
        void drainPanel(const clap_output_events_t *out)
        {
            size_t t = panelTail.load(std::memory_order_relaxed);
            const size_t h = panelHead.load(std::memory_order_acquire);
            for (; t != h; ++t)
            {
                const Change c = fromPanel[t % QUEUE];
                apply(c);
                if (out && c.kind == Change::Value && c.id >= 0 && c.id < NPARAMS)
                {
                    clap_event_param_value_t e{};
                    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
                    e.param_id = static_cast<clap_id>(c.id), e.cookie = nullptr, e.note_id = -1, e.port_index = -1, e.channel = -1, e.key = -1;
                    e.value = static_cast<double>(c.value);
                    out->try_push(out, &e.header);
                }
            }
            panelTail.store(t, std::memory_order_release);
        }
        void setWhole(const DrumKit &k) // the main thread
        {
            const std::string line = "K " + kitToJson(k).dump() + "\n";
            std::lock_guard<std::mutex> g(wholeLock);
            wholeLine = line;
        }

        void push(Change c)
        {
            const size_t h = head.load(std::memory_order_relaxed);
            if (h - tail.load(std::memory_order_acquire) == QUEUE)
                return;
            queue[h % QUEUE] = c;
            head.store(h + 1, std::memory_order_release);
        }
        void drainQueue()
        {
            size_t t = tail.load(std::memory_order_relaxed);
            const size_t h = head.load(std::memory_order_acquire);
            for (; t != h; ++t)
                apply(queue[t % QUEUE]);
            tail.store(t, std::memory_order_release);
        }
        void publish()
        {
            for (int i = 0; i < NPARAMS; ++i)
                value[static_cast<size_t>(i)].store(get(kit, i), std::memory_order_relaxed);
        }
        void load() // the kit and its recordings into the engine
        {
            if (!engine)
                return;
            applyDrumKit(*engine, kit);
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
                engine->setSource(i, sources[static_cast<size_t>(i)]);
        }
        void apply(const Change &c)
        {
            switch (c.kind)
            {
            case Change::Value:
                if (c.id < 0 || c.id >= NPARAMS)
                    return;
                set(kit, c.id, c.value);
                value[static_cast<size_t>(c.id)].store(c.value, std::memory_order_relaxed);
                if (engine && c.id >= NPAD)
                    engine->setMaster(kit.master);
                else if (engine)
                    engine->setPad(c.id / PP_COUNT, kit.pads[static_cast<size_t>(c.id / PP_COUNT)]);
                break;
            case Change::Program:
            {
                const int k = std::clamp(static_cast<int>(std::lround(c.value)), 0, static_cast<int>(kits.size()) - 1);
                kit = kits[static_cast<size_t>(k)]; // copies strings: a kit change, not a sample's work
                sources = kitSources[static_cast<size_t>(k)];
                program.store(static_cast<float>(k));
                load();
                publish();
                rescan = true;
                if (editor.running())
                    wholeProgram.store(k);
                if (host->request_callback)
                    host->request_callback(host);
                break;
            }
            case Change::Whole:
                if (pendingReady.exchange(false))
                {
                    kit = pending;
                    sources = pendingSources;
                    load();
                    publish();
                }
                break;
            case Change::Hit:
                if (engine && c.id >= 0 && c.id < NUM_DRUM_PADS)
                    engine->hit(c.id, c.value);
                break;
            }
        }
        void event(const clap_event_header_t *h)
        {
            if (h->space_id != CLAP_CORE_EVENT_SPACE_ID)
                return;
            if (h->type == CLAP_EVENT_PARAM_VALUE)
            {
                const auto *p = reinterpret_cast<const clap_event_param_value_t *>(h);
                apply(p->param_id == PROGRAM_ID ? Change{Change::Program, 0, static_cast<float>(p->value)}
                                                : Change{Change::Value, static_cast<int>(p->param_id), static_cast<float>(p->value)});
                return;
            }
            if (!engine)
                return;
            if (h->type == CLAP_EVENT_NOTE_ON)
            {
                const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
                if (n->key >= 0)
                    engine->noteOn(n->key, static_cast<float>(n->velocity));
            }
            else if (h->type == CLAP_EVENT_MIDI)
            {
                const auto *m = reinterpret_cast<const clap_event_midi_t *>(h);
                const uint8_t st = m->data[0] & 0xF0, d1 = m->data[1], d2 = m->data[2];
                if (st == 0x90 && d2 > 0)
                    engine->noteOn(d1, static_cast<float>(d2) / 127.0f);
                else if (st == 0xB0 && d1 == 1)
                    engine->setModWheel(static_cast<float>(d2) / 127.0f);
                else if (st == 0xB0 && (d1 == 16 || d1 == 17))
                    engine->setCC(d1 - 16, static_cast<float>(d2) / 127.0f);
            }
        }
    };

    Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

    // ── extensions ──────────────────────────────────────────────────────────────────────────────
    uint32_t audioPortsCount(const clap_plugin_t *, bool isInput) { return isInput ? 0 : 1; }
    bool audioPortsGet(const clap_plugin_t *, uint32_t index, bool isInput, clap_audio_port_info_t *info)
    {
        if (isInput || index != 0)
            return false;
        info->id = 0;
        std::snprintf(info->name, sizeof info->name, "%s", "Out");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN, info->channel_count = 2, info->port_type = CLAP_PORT_STEREO, info->in_place_pair = CLAP_INVALID_ID;
        return true;
    }
    const clap_plugin_audio_ports_t audioPorts{audioPortsCount, audioPortsGet};
    uint32_t notePortsCount(const clap_plugin_t *, bool isInput) { return isInput ? 1 : 0; }
    bool notePortsGet(const clap_plugin_t *, uint32_t index, bool isInput, clap_note_port_info_t *info)
    {
        if (!isInput || index != 0)
            return false;
        info->id = 0, info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI, info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        std::snprintf(info->name, sizeof info->name, "%s", "Pads");
        return true;
    }
    const clap_plugin_note_ports_t notePorts{notePortsCount, notePortsGet};

    uint32_t paramsCount(const clap_plugin_t *) { return NPARAMS + 1; }
    bool paramsInfo(const clap_plugin_t *p, uint32_t index, clap_param_info_t *info)
    {
        if (index > PROGRAM_ID)
            return false;
        std::memset(info, 0, sizeof *info);
        info->id = index;
        if (index == PROGRAM_ID)
        {
            info->flags = CLAP_PARAM_IS_STEPPED;
            std::snprintf(info->name, sizeof info->name, "%s", "PROGRAM");
            std::snprintf(info->module, sizeof info->module, "%s", "Kits");
            info->max_value = static_cast<double>(self(p)->kits.size() - 1);
            return true;
        }
        const int id = static_cast<int>(index);
        const Info in = infoOf(id);
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | (in.stepped ? CLAP_PARAM_IS_STEPPED : 0);
        std::snprintf(info->name, sizeof info->name, "%s", nameOf(id).c_str());
        std::snprintf(info->module, sizeof info->module, "%s", id >= NPAD ? "Master" : drumPadName(id / PP_COUNT));
        info->min_value = static_cast<double>(in.lo), info->max_value = static_cast<double>(in.hi);
        info->default_value = static_cast<double>(get(self(p)->kits.front(), id));
        return true;
    }
    bool paramsValue(const clap_plugin_t *p, clap_id id, double *out)
    {
        if (id == PROGRAM_ID)
            return *out = static_cast<double>(self(p)->program.load()), true;
        if (id >= NPARAMS)
            return false;
        *out = static_cast<double>(self(p)->value[id].load(std::memory_order_relaxed));
        return true;
    }
    bool paramsToText(const clap_plugin_t *p, clap_id id, double v, char *out, uint32_t size)
    {
        if (id == PROGRAM_ID)
        {
            const auto &k = self(p)->kits;
            std::snprintf(out, size, "%s", k[static_cast<size_t>(std::clamp(static_cast<long>(std::lround(v)), 0L, static_cast<long>(k.size()) - 1))].name.c_str());
            return true;
        }
        if (id >= NPARAMS)
            return false;
        const int n = static_cast<int>(std::lround(v));
        if (id < NPAD && id % PP_COUNT == PP_ENGINE)
            std::snprintf(out, size, "%s", drumEngineName(static_cast<DrumEngine>(std::clamp(n, 0, NUM_DRUM_ENGINES - 1))));
        else if (id < NPAD && id % PP_COUNT == PP_CIRCUIT)
            std::snprintf(out, size, "%s", drumKindName(static_cast<DrumKind>(std::clamp(n, 0, NUM_DRUM_KINDS - 1))));
        else if (infoOf(static_cast<int>(id)).stepped)
            std::snprintf(out, size, "%s", n ? "On" : "Off");
        else
            std::snprintf(out, size, "%.2f", v);
        return true;
    }
    bool paramsFromText(const clap_plugin_t *, clap_id id, const char *text, double *out)
    {
        char *end = nullptr;
        const double v = std::strtod(text, &end);
        if (id > PROGRAM_ID || end == text)
            return false;
        *out = v;
        return true;
    }
    void paramsFlush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *out)
    {
        Plugin *s = self(p);
        s->drainQueue();
        s->drainPanel(out);
        for (uint32_t i = 0, n = in->size(in); i < n; ++i)
            s->event(in->get(in, i));
    }
    const clap_plugin_params_t params{paramsCount, paramsInfo, paramsValue, paramsToText, paramsFromText, paramsFlush};

    bool stateSave(const clap_plugin_t *p, const clap_ostream_t *stream)
    {
        Plugin *s = self(p);
        DrumKit k = s->kit;
        for (int i = 0; i < NPARAMS; ++i)
            set(k, i, s->value[static_cast<size_t>(i)].load());
        nlohmann::json j = kitToJson(k);
        j["product"] = "ObsDrums", j["program"] = static_cast<int>(s->program.load());
        const std::string text = j.dump();
        for (size_t done = 0; done < text.size();)
        {
            const int64_t w = stream->write(stream, text.data() + done, text.size() - done);
            if (w <= 0)
                return false;
            done += static_cast<size_t>(w);
        }
        return true;
    }
    // a whole kit, its recordings read (the main thread): into the engine at its next block, the values for the host
    void takeKit(Plugin *s, const DrumKit &k, const Sources &src, int program)
    {
        s->kept.push_back(src);
        for (int i = 0; i < NPARAMS; ++i)
            s->value[static_cast<size_t>(i)].store(get(k, i));
        s->program.store(static_cast<float>(program));
        if (s->engine)
        {
            s->pending = k, s->pendingSources = src;
            s->pendingReady = true;
            s->push({Change::Whole, 0, 0});
        }
        else
            s->kit = k, s->sources = src;
        if (s->hostParams)
            s->hostParams->rescan(s->host, CLAP_PARAM_RESCAN_VALUES);
    }

    bool stateLoad(const clap_plugin_t *p, const clap_istream_t *stream)
    {
        std::string text;
        char buf[4096];
        for (int64_t r; (r = stream->read(stream, buf, sizeof buf)) > 0;)
            text.append(buf, static_cast<size_t>(r));
        Plugin *s = self(p);
        try
        {
            const auto j = nlohmann::json::parse(text);
            DrumKit k;
            int program = 0;
            Sources src{};
            if (j.contains("kit")) // a factory kit by name
            {
                bool found = false;
                for (size_t i = 0; i < s->kits.size(); ++i)
                    if (s->kits[i].name == j["kit"].get<std::string>())
                        k = s->kits[i], src = s->kitSources[i], program = static_cast<int>(i), found = true;
                if (!found)
                    return false;
            }
            else
            {
                if (!kitFromJson(j, k))
                    return false;
                src = loadDrumKitSamples(k, samplePathOf); // here, the main thread
                program = j.value("program", 0);
            }
            takeKit(s, k, src, program);
            if (s->editor.running()) // the panel: the whole of it
                s->setWhole(k);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    const clap_plugin_state_t stateExt{stateSave, stateLoad};

    // ── the editor (clap.gui): the panel as its own program, as ObsSynthCT1's (ct1_clap.cpp). On Windows inside the
    // host's window (set_parent: an HWND the panel's window becomes a child of), or floating; elsewhere floating.
#define EDLOG(...) synth::editorlog::say("ObsDrums", "plugin", __VA_ARGS__)
    bool guiApiSupported(const clap_plugin_t *, const char *api, bool floating)
    {
#if defined(_WIN32)
        return !std::strcmp(api, CLAP_WINDOW_API_WIN32) || floating;
#else
        (void)api;
        return floating;
#endif
    }
    bool guiPreferredApi(const clap_plugin_t *, const char **api, bool *floating)
    {
#if defined(__APPLE__)
        *api = CLAP_WINDOW_API_COCOA;
        *floating = true;
#elif defined(_WIN32)
        *api = CLAP_WINDOW_API_WIN32;
        *floating = false;
#else
        *api = CLAP_WINDOW_API_WAYLAND;
        *floating = true;
#endif
        return true;
    }
    bool guiCreate(const clap_plugin_t *p, const char *api, bool floating)
    {
        Plugin *s = self(p);
        EDLOG("create %s floating=%d; the plugin is in %s", api ? api : "?", floating ? 1 : 0, synth::clap::pluginDir().string().c_str());
#if !defined(_WIN32)
        if (!floating)
            return false;
#endif
        if (panelProgram().empty())
        {
            EDLOG("create: NO PANEL found (obsdrums beside the plugin, in ../Resources, or $OBS_DRUMS_PANEL)");
            return false;
        }
        s->embedded = !floating;
        s->parent = 0;
        s->wantShown = false;
        s->editor.onLine = [s](const std::string &line)
        {
            int pad = 0, param = 0;
            float v = 0;
            if (line.size() > 2 && line[0] == 'K' && line[1] == ' ') // the panel's whole kit: read on the main thread
            {
                {
                    std::lock_guard<std::mutex> g(s->wholeLock);
                    s->panelKit = line.substr(2);
                }
                if (s->host->request_callback)
                    s->host->request_callback(s->host);
            }
            else if (std::sscanf(line.c_str(), "p %d %d %f", &pad, &param, &v) == 3)
            {
                const int id = idOfPanel(pad, param);
                if (id < 0)
                    return;
                s->value[static_cast<size_t>(id)].store(v);
                s->told[static_cast<size_t>(id)] = v; // the panel has it: not sent back
                s->fromEditor({Change::Value, id, v});
                if (s->hostParams && s->hostParams->request_flush)
                    s->hostParams->request_flush(s->host);
            }
            else if (std::sscanf(line.c_str(), "h %d %f", &pad, &v) == 2 && pad >= 0 && pad < NUM_DRUM_PADS)
                s->fromEditor({Change::Hit, pad, v});
        };
        s->editor.tick = [s]
        {
            std::string out;
            {
                std::lock_guard<std::mutex> g(s->wholeLock);
                out.swap(s->wholeLine);
            }
            char b[64];
            for (int i = 0; i < NPARAMS; ++i) // what the host changed (automation, a program, a state), as the panel numbers it
            {
                const float v = s->value[static_cast<size_t>(i)].load();
                if (v == s->told[static_cast<size_t>(i)])
                    continue;
                s->told[static_cast<size_t>(i)] = v;
                const int pad = i < NPAD ? i / PP_COUNT : -1, pp = i < NPAD ? i % PP_COUNT : i - NPAD;
                std::snprintf(b, sizeof b, "v %d %d %.9g\n", pad, pad >= 0 && pp == PP_GRAINPOS ? static_cast<int>(P_GRAINPOS) : pp, static_cast<double>(v));
                out += b;
            }
            std::snprintf(b, sizeof b, "s %.4f %.4f", static_cast<double>(s->outPeak[0].load()), static_cast<double>(s->outPeak[1].load()));
            out += b;
            for (const auto *arr : {&s->padPeak, &s->padFlash})
                for (const auto &x : *arr)
                {
                    std::snprintf(b, sizeof b, " %.4f", static_cast<double>(x.load(std::memory_order_relaxed)));
                    out += b;
                }
            return out + "\n";
        };
        s->editor.onExit = [s] // its window closed: the host is told on the main thread
        {
            s->editorClosed = true;
            if (s->host->request_callback)
                s->host->request_callback(s->host);
        };
        return true;
    }
    void guiDestroy(const clap_plugin_t *p)
    {
        EDLOG("destroy");
        self(p)->editor.close();
    }
    bool guiSetScale(const clap_plugin_t *p, double scale)
    {
        self(p)->guiScale = scale > 0 ? scale : 1.0;
        return true;
    }
    bool guiGetSize(const clap_plugin_t *p, uint32_t *w, uint32_t *h)
    {
        const Plugin *s = self(p); // inside a host: 1280 x 720 (the panel is drawn for 1600 x 900 and scales), in pixels
        *w = s->embedded ? static_cast<uint32_t>(1280 * s->guiScale) : 1600;
        *h = s->embedded ? static_cast<uint32_t>(720 * s->guiScale) : 900;
        return true;
    }
    bool guiCanResize(const clap_plugin_t *) { return false; }
    bool guiResizeHints(const clap_plugin_t *, clap_gui_resize_hints_t *) { return false; }
    bool guiAdjustSize(const clap_plugin_t *, uint32_t *, uint32_t *) { return false; }
    bool guiSetSize(const clap_plugin_t *, uint32_t, uint32_t) { return false; }
    bool openPanel(Plugin *s)
    {
        s->wantShown = false;
        s->told.fill(-12345.0f); // a new panel: every value, once
        DrumKit now = s->kit;    // as stateSave reads it: the values from the atomics
        for (int i = 0; i < NPARAMS; ++i)
            set(now, i, s->value[static_cast<size_t>(i)].load());
        s->setWhole(now);
        std::vector<std::string> args{"--remote", synth::clap::RemoteEditor::LINK};
        if (s->embedded && s->parent)
        {
            args.push_back("--parent");
            args.push_back(std::to_string(s->parent));
        }
        const bool ok = s->editor.open(panelProgram(), args);
        EDLOG("panel %s: %s", ok ? "started" : "DID NOT START", ok ? panelProgram().c_str() : s->editor.lastError().c_str());
        return ok;
    }
    bool guiSetParent(const clap_plugin_t *p, const clap_window_t *window)
    {
#if defined(_WIN32)
        Plugin *s = self(p);
        const uint64_t was = s->parent;
        s->parent = reinterpret_cast<uint64_t>(window->win32);
        EDLOG("set_parent HWND %llu", static_cast<unsigned long long>(s->parent));
        if (s->editor.running() && was != s->parent) // a new window without a hide before (clap-wrapper's removed()): the panel moves
        {
            s->editor.close();
            return s->parent ? openPanel(s) : false;
        }
        if (s->parent && s->wantShown)
            return openPanel(s);
        return s->parent != 0;
#else
        (void)p, (void)window;
        return false;
#endif
    }
    bool guiSetTransient(const clap_plugin_t *, const clap_window_t *) { return true; }
    void guiSuggestTitle(const clap_plugin_t *, const char *) {}
    bool guiShow(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        EDLOG("show (running=%d embedded=%d parent=%llu)", s->editor.running() ? 1 : 0, s->embedded ? 1 : 0, static_cast<unsigned long long>(s->parent));
        if (s->editor.running())
            return true;
        if (s->embedded && !s->parent) // no window to go into yet: set_parent opens it
        {
            s->wantShown = true;
            return true;
        }
        return openPanel(s);
    }
    bool guiHide(const clap_plugin_t *p)
    {
        EDLOG("hide");
        self(p)->wantShown = false;
        self(p)->editor.close();
        return true;
    }
    const clap_plugin_gui_t gui{guiApiSupported, guiPreferredApi, guiCreate, guiDestroy, guiSetScale, guiGetSize, guiCanResize, guiResizeHints,
                                guiAdjustSize, guiSetSize, guiSetParent, guiSetTransient, guiSuggestTitle, guiShow, guiHide};

    // ── the plugin's calls ──────────────────────────────────────────────────────────────────────
    bool init(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        s->hostParams = static_cast<const clap_host_params_t *>(s->host->get_extension(s->host, CLAP_EXT_PARAMS));
        s->kits = factoryDrumKits();
        for (const auto &d : synth::clap::pcmDirs()) // the recordings the kits name: shipped beside the plugin
            synth::addSampleDir(d.lexically_normal().string());
        for (const DrumKit &k : s->kits)
            s->kitSources.push_back(loadDrumKitSamples(k, samplePathOf));
        s->kit = s->kits.front();
        s->sources = s->kitSources.front();
        s->publish();
        s->hostGui = static_cast<const clap_host_gui_t *>(s->host->get_extension(s->host, CLAP_EXT_GUI));
        return true;
    }
    void destroy(const clap_plugin_t *p)
    {
        self(p)->editor.close();
        delete self(p);
    }
    bool activate(const clap_plugin_t *p, double rate, uint32_t, uint32_t maxFrames)
    {
        Plugin *s = self(p);
        s->engine = std::make_unique<ObsDrums>(static_cast<float>(rate));
        s->load();
        s->buffer.assign(static_cast<size_t>(std::max<uint32_t>(maxFrames, 64)) * 2, 0.0f);
        return true;
    }
    void deactivate(const clap_plugin_t *p) { self(p)->engine.reset(); }
    bool startProcessing(const clap_plugin_t *) { return true; }
    void stopProcessing(const clap_plugin_t *) {}
    void reset(const clap_plugin_t *) {}

    clap_process_status process(const clap_plugin_t *p, const clap_process_t *pr)
    {
        Plugin *s = self(p);
        if (!s->engine || pr->audio_outputs_count < 1 || pr->audio_outputs[0].channel_count < 2)
            return CLAP_PROCESS_ERROR;
        s->drainQueue();
        s->drainPanel(pr->out_events);
        float *L = pr->audio_outputs[0].data32[0], *R = pr->audio_outputs[0].data32[1];
        const uint32_t frames = pr->frames_count, nev = pr->in_events->size(pr->in_events);
        uint32_t ev = 0, at = 0;
        while (at < frames)
        {
            for (; ev < nev; ++ev)
            {
                const clap_event_header_t *h = pr->in_events->get(pr->in_events, ev);
                if (h->time > at)
                    break;
                s->event(h);
            }
            const uint32_t to = ev < nev ? std::min(frames, pr->in_events->get(pr->in_events, ev)->time) : frames;
            while (at < to)
            {
                const uint32_t n = std::min<uint32_t>(to - at, static_cast<uint32_t>(s->buffer.size() / 2));
                s->engine->render(s->buffer.data(), static_cast<int>(n));
                for (uint32_t i = 0; i < n; ++i)
                    L[at + i] = s->buffer[i * 2], R[at + i] = s->buffer[i * 2 + 1];
                at += n;
            }
        }
        for (; ev < nev; ++ev)
            s->event(pr->in_events->get(pr->in_events, ev));
        if (s->editor.running()) // for the panel's meters and pads
        {
            float pl = 0, prr = 0;
            for (uint32_t i = 0; i < frames; ++i)
                pl = std::max(pl, std::fabs(L[i])), prr = std::max(prr, std::fabs(R[i]));
            s->outPeak[0].store(pl, std::memory_order_relaxed), s->outPeak[1].store(prr, std::memory_order_relaxed);
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
            {
                s->padPeak[static_cast<size_t>(i)].store(s->engine->padPeak(i), std::memory_order_relaxed);
                s->padFlash[static_cast<size_t>(i)].store(s->engine->padFlash(i), std::memory_order_relaxed);
            }
        }
        return CLAP_PROCESS_CONTINUE;
    }

    const void *getExtension(const clap_plugin_t *, const char *id)
    {
        if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
            return &audioPorts;
        if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
            return &notePorts;
        if (!std::strcmp(id, CLAP_EXT_PARAMS))
            return &params;
        if (!std::strcmp(id, CLAP_EXT_STATE))
            return &stateExt;
        if (!std::strcmp(id, CLAP_EXT_GUI))
            return &gui;
        return nullptr;
    }
    void onMainThread(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        if (s->rescan.exchange(false) && s->hostParams)
            s->hostParams->rescan(s->host, CLAP_PARAM_RESCAN_VALUES);
        if (const int k = s->wholeProgram.exchange(-1); k >= 0 && k < static_cast<int>(s->kits.size())) // a program: the panel's too
            s->setWhole(s->kits[static_cast<size_t>(k)]);
        std::string fromPanel;
        {
            std::lock_guard<std::mutex> g(s->wholeLock);
            fromPanel.swap(s->panelKit);
        }
        if (!fromPanel.empty()) // the panel's kit: its recordings read here, then as a state is taken
            try
            {
                DrumKit k;
                if (kitFromJson(nlohmann::json::parse(fromPanel), k))
                    takeKit(s, k, loadDrumKitSamples(k, samplePathOf), static_cast<int>(s->program.load()));
            }
            catch (...)
            {
                EDLOG("the panel's kit could not be read");
            }
        if (s->editorClosed.exchange(false) && s->hostGui) // the panel's window was closed by the person
            s->hostGui->closed(s->host, false);
    }

    const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_DRUM_MACHINE, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
    const clap_plugin_descriptor_t descriptor{CLAP_VERSION_INIT, "org.carbonian.obsdrums", "ObsDrums", "Carbonian",
                                              "https://github.com/orianbsilva/obssynth", "", "", "0.1.0",
                                              "Sixteen pads: twelve instruments and four free, each CIRCUIT, FM, VA, SAMPLE or GRAIN", features};

    const clap_plugin_t *create(const clap_host_t *host)
    {
        auto *s = new Plugin();
        s->host = host;
        s->plugin = {&descriptor, s, init, destroy, activate, deactivate, startProcessing, stopProcessing, reset, process, getExtension, onMainThread};
        return &s->plugin;
    }
    uint32_t factoryCount(const clap_plugin_factory_t *) { return 1; }
    const clap_plugin_descriptor_t *factoryDescriptor(const clap_plugin_factory_t *, uint32_t i) { return i == 0 ? &descriptor : nullptr; }
    const clap_plugin_t *factoryCreate(const clap_plugin_factory_t *, const clap_host_t *host, const char *id)
    {
        return clap_version_is_compatible(host->clap_version) && !std::strcmp(id, descriptor.id) ? create(host) : nullptr;
    }
    const clap_plugin_factory_t factory{factoryCount, factoryDescriptor, factoryCreate};
    bool entryInit(const char *) { return true; }
    void entryDeinit() {}
    const void *entryFactory(const char *id) { return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &factory : nullptr; }
} // namespace

// the entry (entry.cpp exports it)
extern "C" bool obs_entry_init(const char *path) { return entryInit(path); }
extern "C" void obs_entry_deinit(void) { entryDeinit(); }
extern "C" const void *obs_entry_factory(const char *id) { return entryFactory(id); }
