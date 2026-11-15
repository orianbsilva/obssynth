// ObsSynthCT1 as a CLAP plugin (ObsSynthCT1.clap), for a DAW (SoundCraft, Bitwig, Reaper...): the product's engine
// (products/obs_ct1/obs_synth_ct1.hpp) behind the CLAP ABI.
//   notes: one note port, CLAP or MIDI; the wheel (CC1) and the bend as MIDI.
//   params: every parameter of the product, by its name (OP2_POS, FM_2_1, INDEX...), automatable, sample-accurate;
//     the ratios as their list's index; PROGRAM (not automatable) loads a factory preset.
//   state: JSON, every parameter by name and the WT operators' banks by name (the session keeps the sound); a state
//     of {"preset": "<name>"} (and "params" over it) loads a factory preset, for sessions written by hand.
//   editor: the product's panel (obssynthct1 --remote 3), a program of its own in a floating window of its own
//     (remote_editor.hpp): what it turns comes back as parameter changes the host records; its keyboard plays.
// The wavetable banks (pcm/bank_*.wav) are read once, off the audio thread, from beside the plugin (pcm/ or
// ../pcm), $OBS_PCM_DIR, ~/.local/share/obssynthct1/pcm, or the source tree it was built from.
// Threads: the audio thread is the engine's only writer; a change from the main thread (a state, a value typed in)
// goes through a queue the next process() or flush() applies. The values the host reads are atomics.
#include <clap/clap.h>

#include "../../products/obs_ct1/obs_synth_ct1.hpp"
#include "../../vendor/json.hpp"
#include "remote_editor.hpp"
#include "../../app/common/editor_log.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "plugin_path.hpp"

using namespace synth::ct1;

namespace
{
    constexpr clap_id PROGRAM_ID = NUM_PARAMS; // the factory presets, after the product's parameters
    constexpr int VALUE_QUEUE = 4096;
    constexpr clap_id NOTE_ON = 100000, NOTE_OFF = 100200; // the editor's keyboard, through the same queue

    // ── where the banks are ─────────────────────────────────────────────────────────────────────
    std::string pluginDir() { return synth::clap::pluginDir().string(); }

    // the panel: $OBS_CT1_PANEL, beside the plugin, or where it was built
    std::string panelProgram()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries;
        if (const char *e = std::getenv("OBS_CT1_PANEL"))
            tries.push_back(e);
#ifdef _WIN32
        const char *exe = "obssynthct1.exe";
#else
        const char *exe = "obssynthct1";
#endif
        tries.push_back(fs::path(pluginDir()) / exe);
        tries.push_back(fs::path(pluginDir()) / ".." / "Resources" / exe); // a VST3 folder
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

    std::vector<std::shared_ptr<const Table>> findBanks()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries = synth::clap::pcmDirs();
        if (const char *h = std::getenv("HOME"))
            tries.push_back(fs::path(h) / ".local/share/obssynthct1/pcm");
#ifdef SYNTH_PCM_DIR
        tries.push_back(SYNTH_PCM_DIR);
#endif
        for (const auto &p : tries)
        {
            std::error_code ec;
            if (fs::is_directory(p, ec))
            {
                auto b = loadBanks(p.string());
                if (!b.empty())
                    return b;
            }
        }
        return {};
    }

    // ── the values: CLAP sees physical ones, the ratios as an index ─────────────────────────────
    bool isRatio(clap_id id) { return id < NUM_PARAMS && paramInfo()[id].kind == ParamKind::Ratio; }
    double toClap(clap_id id, float v)
    {
        if (!isRatio(id))
            return static_cast<double>(v);
        const auto &r = ratios();
        size_t best = 0;
        for (size_t i = 0; i < r.size(); ++i)
            if (std::fabs(r[i] - v) < std::fabs(r[best] - v))
                best = i;
        return static_cast<double>(best);
    }
    float fromClap(clap_id id, double v)
    {
        if (!isRatio(id))
            return static_cast<float>(v);
        const auto &r = ratios();
        return r[static_cast<size_t>(std::clamp(static_cast<long>(std::lround(v)), 0L, static_cast<long>(r.size()) - 1))];
    }

    std::string shown(clap_id id, float v)
    {
        char b[48];
        const std::string &n = paramInfo()[id].name;
        auto ends = [&](const char *s) { return n.size() >= std::strlen(s) && n.compare(n.size() - std::strlen(s), std::strlen(s), s) == 0; };
        const double d = static_cast<double>(v);
        if (ends("_RATIO"))
            std::snprintf(b, sizeof b, "x%g", d);
        else if (ends("_FINE"))
            std::snprintf(b, sizeof b, "%+.0f cents", d);
        else if (ends("_HZ") || n == "CUTOFF" || ends("SCAN_RATE"))
            std::snprintf(b, sizeof b, d >= 1000 ? "%.2f kHz" : "%.1f Hz", d >= 1000 ? d / 1000 : d);
        else if (ends("_A") || ends("_D") || ends("_R") || n == "DLY_TIME")
            std::snprintf(b, sizeof b, d >= 1 ? "%.2f s" : "%.0f ms", d >= 1 ? d : d * 1000);
        else if (ends("_TYPE"))
            return v >= 0.5f ? "WT" : "STD";
        else if (ends("_SHAPE"))
        {
            const char *s[] = {"Sine", "Saw", "Square", "Triangle"};
            return s[std::clamp(static_cast<int>(v), 0, 3)];
        }
        else if (ends("_FIXED"))
            return v >= 0.5f ? "Fixed" : "Ratio";
        else if (n.rfind("FM_", 0) == 0 || n == "INDEX")
            std::snprintf(b, sizeof b, "%.2f", d);
        else if (ends("_PAN"))
            std::snprintf(b, sizeof b, "%+.0f", d * 100);
        else
            std::snprintf(b, sizeof b, "%.0f %%", d * 100);
        return b;
    }

    // ── the plugin ──────────────────────────────────────────────────────────────────────────────
    struct ValueChange
    {
        clap_id id;
        float value; // physical; PROGRAM: the preset's index
    };

    struct Plugin
    {
        clap_plugin_t plugin{};
        const clap_host_t *host = nullptr;
        const clap_host_params_t *hostParams = nullptr;
        std::vector<std::shared_ptr<const Table>> banks;
        std::vector<Preset> presets;
        std::unique_ptr<Engine> engine;
        std::array<std::atomic<float>, NUM_PARAMS> value{}; // what the host reads (physical)
        std::atomic<float> program{0};
        std::atomic<bool> rescan{false};
        // the main thread's changes, for the audio thread (one producer, one consumer)
        std::array<ValueChange, VALUE_QUEUE> queue{};
        std::atomic<size_t> head{0}, tail{0};
        std::vector<float> buffer; // interleaved, the engine's
        // the editor: the panel's changes (the io thread to the audio thread), what the panel was last told, the
        // engine's state for it (the audio thread to the io thread)
        synth::clap::RemoteEditor editor;
        std::array<ValueChange, VALUE_QUEUE> fromPanel{};
        std::atomic<size_t> panelHead{0}, panelTail{0};
        std::array<float, NUM_PARAMS> told{};
        float toldProgram = -1;
        std::atomic<int> voices{0};
        std::atomic<float> peakL{0}, peakR{0};
        std::array<std::atomic<float>, OPS> pos{};
        std::atomic<bool> editorClosed{false};
        const clap_host_gui_t *hostGui = nullptr;
        bool embedded = false;   // the editor inside the host's window (Windows), else its own
        bool wantShown = false;  // show() came before set_parent(): the panel opens with the parent
        uint64_t parent = 0;     // that window (an HWND)
        double guiScale = 1.0;

        void fromEditor(ValueChange c) // the io thread
        {
            const size_t h = panelHead.load(std::memory_order_relaxed);
            if (h - panelTail.load(std::memory_order_acquire) == VALUE_QUEUE)
                return;
            fromPanel[h % VALUE_QUEUE] = c;
            panelHead.store(h + 1, std::memory_order_release);
        }
        // the panel's changes on the audio thread: into the engine, and to the host as parameter events
        void drainPanel(const clap_output_events_t *out)
        {
            size_t t = panelTail.load(std::memory_order_relaxed);
            const size_t h = panelHead.load(std::memory_order_acquire);
            for (; t != h; ++t)
            {
                const ValueChange c = fromPanel[t % VALUE_QUEUE];
                if (c.id >= NOTE_ON && c.id < NOTE_ON + 128)
                {
                    if (engine)
                        engine->noteOn(static_cast<int>(c.id - NOTE_ON), c.value);
                    continue;
                }
                if (c.id >= NOTE_OFF && c.id < NOTE_OFF + 128)
                {
                    if (engine)
                        engine->noteOff(static_cast<int>(c.id - NOTE_OFF));
                    continue;
                }
                apply(c);
                if (out && c.id < NUM_PARAMS)
                {
                    clap_event_param_value_t e{};
                    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
                    e.param_id = c.id, e.cookie = nullptr, e.note_id = -1, e.port_index = -1, e.channel = -1, e.key = -1;
                    e.value = toClap(c.id, c.value);
                    out->try_push(out, &e.header);
                }
            }
            panelTail.store(t, std::memory_order_release);
        }

        void push(ValueChange c)
        {
            const size_t h = head.load(std::memory_order_relaxed);
            if (h - tail.load(std::memory_order_acquire) == VALUE_QUEUE)
                return;
            queue[h % VALUE_QUEUE] = c;
            head.store(h + 1, std::memory_order_release);
        }
        void drainQueue()
        {
            size_t t = tail.load(std::memory_order_relaxed);
            const size_t h = head.load(std::memory_order_acquire);
            for (; t != h; ++t)
                apply(queue[t % VALUE_QUEUE]);
            tail.store(t, std::memory_order_release);
        }

        // a value, on the audio thread
        void apply(const ValueChange &c)
        {
            if (c.id == PROGRAM_ID)
            {
                const int k = std::clamp(static_cast<int>(std::lround(c.value)), 0, static_cast<int>(presets.size()) - 1);
                const Params p = resolve(presets[static_cast<size_t>(k)], banks);
                for (int i = 0; i < NUM_PARAMS; ++i)
                {
                    value[static_cast<size_t>(i)].store(p[static_cast<size_t>(i)], std::memory_order_relaxed);
                    if (engine)
                        engine->setParam(i, p[static_cast<size_t>(i)]);
                }
                program.store(static_cast<float>(k), std::memory_order_relaxed);
                rescan.store(true, std::memory_order_release);
                if (host && host->request_callback)
                    host->request_callback(host);
                return;
            }
            if (c.id >= NUM_PARAMS)
                return;
            value[c.id].store(c.value, std::memory_order_relaxed);
            if (engine)
                engine->setParam(static_cast<int>(c.id), c.value);
        }

        void event(const clap_event_header_t *h)
        {
            if (h->space_id != CLAP_CORE_EVENT_SPACE_ID || !engine)
                return;
            switch (h->type)
            {
            case CLAP_EVENT_NOTE_ON:
            {
                const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
                if (n->key >= 0)
                    engine->noteOn(n->key, static_cast<float>(n->velocity));
                break;
            }
            case CLAP_EVENT_NOTE_OFF:
            case CLAP_EVENT_NOTE_CHOKE:
            {
                const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
                if (n->key >= 0)
                    engine->noteOff(n->key);
                else if (h->type == CLAP_EVENT_NOTE_CHOKE)
                    engine->allNotesOff();
                break;
            }
            case CLAP_EVENT_PARAM_VALUE:
            {
                const auto *p = reinterpret_cast<const clap_event_param_value_t *>(h);
                apply({p->param_id, p->param_id == PROGRAM_ID ? static_cast<float>(p->value) : fromClap(p->param_id, p->value)});
                break;
            }
            case CLAP_EVENT_MIDI:
            {
                const auto *m = reinterpret_cast<const clap_event_midi_t *>(h);
                const uint8_t st = m->data[0] & 0xF0, d1 = m->data[1], d2 = m->data[2];
                if (st == 0x90 && d2 > 0)
                    engine->noteOn(d1, static_cast<float>(d2) / 127.0f);
                else if (st == 0x80 || (st == 0x90 && d2 == 0))
                    engine->noteOff(d1);
                else if (st == 0xB0 && d1 == 1)
                    engine->setModWheel(static_cast<float>(d2) / 127.0f);
                else if (st == 0xB0 && (d1 == 120 || d1 == 123))
                    engine->allNotesOff();
                else if (st == 0xE0)
                    engine->setPitchBend((static_cast<float>((d2 << 7) | d1) - 8192.0f) / 8192.0f * 2.0f);
                break;
            }
            default: break;
            }
        }

        nlohmann::json state() const
        {
            nlohmann::json j, params = nlohmann::json::object();
            for (int i = 0; i < NUM_PARAMS; ++i)
                params[paramInfo()[static_cast<size_t>(i)].name] = value[static_cast<size_t>(i)].load(std::memory_order_relaxed);
            j["params"] = params;
            nlohmann::json bk = nlohmann::json::array();
            for (int o = 0; o < OPS; ++o)
            {
                const int b = static_cast<int>(value[static_cast<size_t>(opParam(o, OP_BANK))].load());
                bk.push_back(value[static_cast<size_t>(opParam(o, OP_TYPE))].load() >= 0.5f && b >= 0 && b < static_cast<int>(banks.size()) ? banks[static_cast<size_t>(b)]->name() : std::string());
            }
            j["banks"] = bk;
            j["program"] = program.load();
            j["product"] = "ObsSynthCT1";
            return j;
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
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->channel_count = 2;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
        return true;
    }
    const clap_plugin_audio_ports_t audioPorts{audioPortsCount, audioPortsGet};

    uint32_t notePortsCount(const clap_plugin_t *, bool isInput) { return isInput ? 1 : 0; }
    bool notePortsGet(const clap_plugin_t *, uint32_t index, bool isInput, clap_note_port_info_t *info)
    {
        if (!isInput || index != 0)
            return false;
        info->id = 0;
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        std::snprintf(info->name, sizeof info->name, "%s", "Notes");
        return true;
    }
    const clap_plugin_note_ports_t notePorts{notePortsCount, notePortsGet};

    uint32_t paramsCount(const clap_plugin_t *) { return NUM_PARAMS + 1; }
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
            std::snprintf(info->module, sizeof info->module, "%s", "Presets");
            info->min_value = 0;
            info->max_value = static_cast<double>(self(p)->presets.size() - 1);
            return true;
        }
        const ParamInfo &pi = paramInfo()[index];
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        if (pi.kind == ParamKind::Choice || pi.kind == ParamKind::Toggle || pi.kind == ParamKind::Ratio)
            info->flags |= CLAP_PARAM_IS_STEPPED;
        std::snprintf(info->name, sizeof info->name, "%s", pi.name.c_str());
        const std::string &n = pi.name;
        std::snprintf(info->module, sizeof info->module, "%s",
                      n.rfind("OP", 0) == 0 ? ("Operator " + n.substr(2, 1)).c_str() : n.rfind("FM_", 0) == 0 || n.rfind("OUT_", 0) == 0 ? "FM" : "Master");
        info->min_value = pi.kind == ParamKind::Ratio ? 0.0 : static_cast<double>(pi.lo);
        info->max_value = pi.kind == ParamKind::Ratio ? static_cast<double>(ratios().size() - 1) : static_cast<double>(pi.hi);
        if (n.size() > 5 && n.compare(n.size() - 5, 5, "_BANK") == 0) // as many as there are
            info->max_value = static_cast<double>(std::max<size_t>(1, self(p)->banks.size()) - 1);
        info->default_value = toClap(index, pi.def);
        return true;
    }
    bool paramsValue(const clap_plugin_t *p, clap_id id, double *out)
    {
        if (id == PROGRAM_ID)
            return *out = static_cast<double>(self(p)->program.load()), true;
        if (id >= NUM_PARAMS)
            return false;
        *out = toClap(id, self(p)->value[id].load(std::memory_order_relaxed));
        return true;
    }
    bool paramsToText(const clap_plugin_t *p, clap_id id, double v, char *out, uint32_t size)
    {
        if (id == PROGRAM_ID)
        {
            const auto &pr = self(p)->presets;
            std::snprintf(out, size, "%s", pr[static_cast<size_t>(std::clamp(static_cast<long>(std::lround(v)), 0L, static_cast<long>(pr.size()) - 1))].name.c_str());
            return true;
        }
        if (id >= NUM_PARAMS)
            return false;
        if (paramInfo()[id].name.find("_BANK") != std::string::npos)
        {
            const auto &b = self(p)->banks;
            const long k = std::lround(v);
            std::snprintf(out, size, "%s", k >= 0 && k < static_cast<long>(b.size()) ? b[static_cast<size_t>(k)]->name().c_str() : "-");
            return true;
        }
        std::snprintf(out, size, "%s", shown(id, fromClap(id, v)).c_str());
        return true;
    }
    bool paramsFromText(const clap_plugin_t *, clap_id id, const char *text, double *out)
    {
        if (id > PROGRAM_ID)
            return false;
        char *end = nullptr;
        const double v = std::strtod(text, &end);
        if (end == text)
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
        if (!s->engine) // not active: the values still change
            for (uint32_t i = 0, n = in->size(in); i < n; ++i)
            {
                const clap_event_header_t *h = in->get(in, i);
                if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
                {
                    const auto *pv = reinterpret_cast<const clap_event_param_value_t *>(h);
                    s->apply({pv->param_id, pv->param_id == PROGRAM_ID ? static_cast<float>(pv->value) : fromClap(pv->param_id, pv->value)});
                }
            }
    }
    const clap_plugin_params_t params{paramsCount, paramsInfo, paramsValue, paramsToText, paramsFromText, paramsFlush};

    bool stateSave(const clap_plugin_t *p, const clap_ostream_t *stream)
    {
        const std::string text = self(p)->state().dump();
        size_t done = 0;
        while (done < text.size())
        {
            const int64_t w = stream->write(stream, text.data() + done, text.size() - done);
            if (w <= 0)
                return false;
            done += static_cast<size_t>(w);
        }
        return true;
    }
    bool stateLoad(const clap_plugin_t *p, const clap_istream_t *stream)
    {
        std::string text;
        char buf[4096];
        for (;;)
        {
            const int64_t r = stream->read(stream, buf, sizeof buf);
            if (r < 0)
                return false;
            if (r == 0)
                break;
            text.append(buf, static_cast<size_t>(r));
        }
        Plugin *s = self(p);
        try
        {
            const auto j = nlohmann::json::parse(text);
            Preset pr{"State", defaultParams(), {}};
            // {"preset": "Ring Drone"}: a factory preset by name (what "params" may then change)
            if (j.contains("preset"))
                for (size_t k = 0; k < s->presets.size(); ++k)
                    if (s->presets[k].name == j["preset"].get<std::string>())
                    {
                        pr = s->presets[k];
                        s->program.store(static_cast<float>(k));
                    }
            if (j.contains("params"))
                for (auto it = j["params"].begin(); it != j["params"].end(); ++it)
                    if (const int id = paramId(it.key()); id >= 0)
                        pr.params[static_cast<size_t>(id)] = it.value().get<float>();
            if (j.contains("banks"))
                for (size_t o = 0; o < pr.bank.size() && o < j["banks"].size(); ++o)
                    pr.bank[o] = j["banks"][o].get<std::string>();
            const Params v = resolve(pr, s->banks);
            for (int i = 0; i < NUM_PARAMS; ++i)
            {
                s->value[static_cast<size_t>(i)].store(v[static_cast<size_t>(i)]);
                s->push({static_cast<clap_id>(i), v[static_cast<size_t>(i)]});
            }
            if (j.contains("program"))
                s->program.store(j["program"].get<float>());
            if (s->hostParams)
                s->hostParams->rescan(s->host, CLAP_PARAM_RESCAN_VALUES);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    const clap_plugin_state_t stateExt{stateSave, stateLoad};

    // ── the editor (clap.gui): the panel as its own program. On Windows inside the host's window (set_parent: an
    // HWND the panel's window becomes a child of), or floating; elsewhere floating (Wayland: no window to go into).
#define EDLOG(...) synth::editorlog::say("ObsSynthCT1", "plugin", __VA_ARGS__)
    bool guiApiSupported(const clap_plugin_t *, const char *api, bool floating)
    {
#if defined(_WIN32)
        EDLOG("is_api_supported %s floating=%d", api ? api : "?", floating ? 1 : 0);
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
        *api = CLAP_WINDOW_API_WAYLAND; // the panel opens its own window: a Wayland one where there is Wayland
        *floating = true;
#endif
        return true;
    }
    bool guiCreate(const clap_plugin_t *p, const char *api, bool floating)
    {
        Plugin *s = self(p);
        EDLOG("create %s floating=%d; the plugin is in %s", api ? api : "?", floating ? 1 : 0, pluginDir().c_str());
#if !defined(_WIN32)
        if (!floating)
            return false;
#endif
        if (panelProgram().empty())
        {
            EDLOG("create: NO PANEL found (obssynthct1 beside the plugin, in ../Resources, or $OBS_CT1_PANEL)");
            return false;
        }
        EDLOG("create: the panel %s", panelProgram().c_str());
        s->embedded = !floating;
        s->parent = 0;
        s->wantShown = false;
        s->editor.onLine = [s](const std::string &line)
        {
            int id = 0, note = 0;
            float v = 0;
            if (std::sscanf(line.c_str(), "p %d %f", &id, &v) == 2 && id >= 0 && id < NUM_PARAMS)
            {
                s->value[static_cast<size_t>(id)].store(v);
                s->told[static_cast<size_t>(id)] = v; // the panel has it: not sent back
                s->fromEditor({static_cast<clap_id>(id), v});
                if (s->hostParams && s->hostParams->request_flush)
                    s->hostParams->request_flush(s->host);
            }
            else if (std::sscanf(line.c_str(), "n %d %f", &note, &v) == 2 && note >= 0 && note < 128)
                s->fromEditor({NOTE_ON + static_cast<clap_id>(note), v});
            else if (std::sscanf(line.c_str(), "o %d", &note) == 1 && note >= 0 && note < 128)
                s->fromEditor({NOTE_OFF + static_cast<clap_id>(note), 0});
        };
        s->editor.tick = [s]
        {
            std::string out;
            char b[160];
            for (int i = 0; i < NUM_PARAMS; ++i) // what the host changed (automation, a program, a state)
            {
                const float v = s->value[static_cast<size_t>(i)].load();
                if (v != s->told[static_cast<size_t>(i)])
                {
                    std::snprintf(b, sizeof b, "v %d %.9g\n", i, static_cast<double>(v));
                    out += b;
                    s->told[static_cast<size_t>(i)] = v;
                }
            }
            if (s->program.load() != s->toldProgram)
            {
                s->toldProgram = s->program.load();
                std::snprintf(b, sizeof b, "P %g\n", static_cast<double>(s->toldProgram));
                out += b;
            }
            std::snprintf(b, sizeof b, "s %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", s->voices.load(), static_cast<double>(s->peakL.load()),
                          static_cast<double>(s->peakR.load()), static_cast<double>(s->pos[0].load()), static_cast<double>(s->pos[1].load()),
                          static_cast<double>(s->pos[2].load()), static_cast<double>(s->pos[3].load()), static_cast<double>(s->pos[4].load()));
            return out + b;
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
        EDLOG("set_scale %g", scale);
        self(p)->guiScale = scale > 0 ? scale : 1.0;
        return true;
    }
    bool guiGetSize(const clap_plugin_t *p, uint32_t *w, uint32_t *h)
    {
        const Plugin *s = self(p); // inside a host: 1280 x 720 (the panel is drawn for 1600 x 900 and scales), in pixels
        *w = s->embedded ? static_cast<uint32_t>(1280 * s->guiScale) : 1600;
        *h = s->embedded ? static_cast<uint32_t>(720 * s->guiScale) : 900;
        EDLOG("get_size %u x %u", *w, *h);
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
        s->toldProgram = -1;
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
        EDLOG("set_parent HWND %llu%s", static_cast<unsigned long long>(s->parent), s->wantShown ? " (show was waiting: opening now)" : "");
        if (s->editor.running() && was != s->parent) // a new window without a hide before (clap-wrapper's removed()): the panel moves
        {
            EDLOG("set_parent: a new window while the panel runs in %llu: reopened in the new one", static_cast<unsigned long long>(was));
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
        s->hostGui = static_cast<const clap_host_gui_t *>(s->host->get_extension(s->host, CLAP_EXT_GUI));
        s->banks = findBanks();
        s->presets = factoryPresets();
        const Params v = resolve(s->presets[1], s->banks); // a table modulating a sine: what it is about
        for (int i = 0; i < NUM_PARAMS; ++i)
            s->value[static_cast<size_t>(i)].store(v[static_cast<size_t>(i)]);
        s->program.store(1);
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
        s->engine = std::make_unique<Engine>(static_cast<float>(rate), s->banks);
        for (int i = 0; i < NUM_PARAMS; ++i)
            s->engine->setParam(i, s->value[static_cast<size_t>(i)].load());
        s->buffer.assign(static_cast<size_t>(std::max<uint32_t>(maxFrames, 64)) * 2, 0.0f);
        return true;
    }
    void deactivate(const clap_plugin_t *p) { self(p)->engine.reset(); }
    bool startProcessing(const clap_plugin_t *) { return true; }
    void stopProcessing(const clap_plugin_t *) {}
    void reset(const clap_plugin_t *p)
    {
        if (self(p)->engine)
            self(p)->engine->allNotesOff();
    }

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
        // render between the events: each one at its sample
        while (at < frames)
        {
            while (ev < nev)
            {
                const clap_event_header_t *h = pr->in_events->get(pr->in_events, ev);
                if (h->time > at)
                    break;
                s->event(h);
                ++ev;
            }
            uint32_t to = frames;
            if (ev < nev)
                to = std::min(frames, pr->in_events->get(pr->in_events, ev)->time);
            while (at < to)
            {
                const uint32_t n = std::min<uint32_t>(to - at, static_cast<uint32_t>(s->buffer.size() / 2));
                s->engine->render(s->buffer.data(), static_cast<int>(n));
                for (uint32_t i = 0; i < n; ++i)
                {
                    L[at + i] = s->buffer[i * 2];
                    R[at + i] = s->buffer[i * 2 + 1];
                }
                at += n;
            }
        }
        for (; ev < nev; ++ev)
            s->event(pr->in_events->get(pr->in_events, ev));
        if (s->editor.running()) // for the panel's meters and pictures
        {
            float pl = 0, prr = 0;
            for (uint32_t i = 0; i < frames; ++i)
                pl = std::max(pl, std::fabs(L[i])), prr = std::max(prr, std::fabs(R[i]));
            s->peakL.store(pl, std::memory_order_relaxed), s->peakR.store(prr, std::memory_order_relaxed);
            s->voices.store(s->engine->sounding(), std::memory_order_relaxed);
            for (int o = 0; o < OPS; ++o)
                s->pos[static_cast<size_t>(o)].store(s->engine->livePos(o), std::memory_order_relaxed);
        }
        return s->engine->sounding() > 0 ? CLAP_PROCESS_CONTINUE : CLAP_PROCESS_CONTINUE_IF_NOT_QUIET;
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
        if (s->rescan.exchange(false) && s->hostParams) // a program changed every value
            s->hostParams->rescan(s->host, CLAP_PARAM_RESCAN_VALUES);
        if (s->editorClosed.exchange(false) && s->hostGui) // the panel's window was closed by the person
            s->hostGui->closed(s->host, false);
    }

    const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
    const clap_plugin_descriptor_t descriptor{CLAP_VERSION_INIT,
                                              "org.carbonian.obssynthct1",
                                              "ObsSynthCT1",
                                              "Carbonian",
                                              "https://github.com/orianbsilva/obssynth",
                                              "",
                                              "",
                                              "0.1.0",
                                              "Cross Table One: five STD or wavetable operators cross-modulating through a 5 x 5 FM matrix",
                                              features};

    const clap_plugin_t *create(const clap_host_t *host)
    {
        auto *s = new Plugin();
        s->host = host;
        s->plugin = {&descriptor, s, init, destroy, activate, deactivate, startProcessing, stopProcessing, reset, process, getExtension, onMainThread};
        return &s->plugin;
    }

    // ── the factory and the entry ───────────────────────────────────────────────────────────────
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
