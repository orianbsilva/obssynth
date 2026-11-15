// ObsSynth4D as a CLAP plugin (ObsSynth4D.clap): the product's engine behind the CLAP ABI, its values the panel's own
// (app/obssynth4d: params.hpp, driver.hpp, presets.cpp), so a preset sounds here as it sounds in the app.
//   notes: one note port, CLAP or MIDI; the wheel (CC1), expression (CC11), CC A-D (16-19), bend and pressure as MIDI.
//   params: the four oscillators' and the global values, 0..1 as the panel's knobs turn (their text in their units),
//     automatable; PROGRAM (not automatable) loads a factory preset (the 34 of the bank).
//   state: JSON: the values, the matrix's routes, the mod envelopes, the tube, the vector path; {"preset": "<name>"}
//     (and "osc"/"glb" over it) loads a factory preset, for sessions written by hand.
//   editor: the product's panel (obssynth4d --remote), a program of its own (remote_editor.hpp), on Windows inside
//     the host's window: what it turns comes back as parameter changes the host records, its routes, envelopes and
//     vector into the state; its keyboard plays. It is told the whole state on opening, at a program, at a session.
// The wavetable banks and the grains' recordings are read on the main thread (activate; a recording the editor or a
// state chooses, per oscillator: "grains" in the state, a samples-folder name or a path), from beside the plugin,
// $OBS_PCM_DIR, ~/.local/share/obssynth4d, or the source tree it was built from.
// Threads: the audio thread is the engine's only writer; a state or a value from the main thread goes through a queue
// the next process() or flush() applies. The values the host reads are atomics.
#include <clap/clap.h>

#include "../../app/obssynth4d/controllers.hpp"
#include "../../app/obssynth4d/driver.hpp"
#include "../../app/obssynth4d/presets.hpp"
#include "../../lib/synth/core/grain_engine.hpp"
#include "../../vendor/json.hpp"
#include "remote_editor.hpp"
#include "../../app/common/editor_log.hpp"
#include "../../app/common/sample_find.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "plugin_path.hpp"

using namespace obs;

namespace
{
    constexpr int NOSC = 4 * OSC_PARAM_COUNT;
    constexpr int NPARAMS = NOSC + GLB_PARAM_COUNT;
    constexpr clap_id PROGRAM_ID = NPARAMS;
    constexpr int SLOTS = 32;
    constexpr int QUEUE = 8192;
    constexpr int SCOPE = 512;   // the panel's scope: the last samples (left)
    constexpr int SOURCES = 64;  // the mod sources' values, for the matrix's meters

    // ── the panel's state: values 0..1, routes and what has no knob ─────────────────────────────
    struct Route
    {
        int src = 0, dst = -1;
        float amount = 0; // engine units, -2..2
        bool on = false;
    };
    struct State
    {
        std::array<float, NPARAMS> v{}; // 0..1
        std::array<Route, SLOTS> routes{};
        float modEnv[3][4] = {{0.01f, 0.2f, 0.5f, 0.3f}, {0.01f, 0.2f, 0.5f, 0.3f}, {0.01f, 0.2f, 0.5f, 0.3f}};
        int tubeModel = 0;
        float tubeChaos = 0;
        bool vecOn = false, vecLoop = false;
        std::vector<std::array<float, 3>> vecPath;
        std::string name = "Init";
    };

    const Range &rangeOf(int id) { return id < NOSC ? OSC_RANGES[id % OSC_PARAM_COUNT] : GLB_RANGES[id - NOSC]; }
    std::string nameOf(int id)
    {
        if (id < NOSC)
            return "OSC" + std::to_string(id / OSC_PARAM_COUNT + 1) + "_" + kOscIds[id % OSC_PARAM_COUNT];
        return kGlbIds[id - NOSC];
    }

    // a preset (engine units) to the panel's state, as the app loads one (main.cpp loadPresetNow)
    State fromPreset(const PresetData &d)
    {
        State s;
        s.name = d.name;
        for (int i = 0; i < NPARAMS; ++i)
        {
            const Range &rg = rangeOf(i);
            const float raw = i < NOSC ? d.osc[i / OSC_PARAM_COUNT][i % OSC_PARAM_COUNT] : d.glb[i - NOSC];
            const float x = std::isnan(raw) ? rg.def : raw;
            s.v[static_cast<size_t>(i)] = std::clamp(toNorm(rg, std::clamp(x, std::min(rg.lo, rg.hi), std::max(rg.lo, rg.hi))), 0.0f, 1.0f);
        }
        for (size_t i = 0; i < d.routes.size() && i < s.routes.size(); ++i)
            s.routes[i] = {d.routes[i].src, d.routes[i].dst, d.routes[i].amount, true};
        std::memcpy(s.modEnv, d.modEnv, sizeof s.modEnv);
        s.tubeModel = d.tubeModel, s.tubeChaos = d.tubeChaos;
        s.vecOn = d.vecOn, s.vecLoop = d.vecLoop;
        s.vecPath = d.vecPath;
        if (s.vecPath.size() > 16)
            s.vecPath.resize(16);
        return s;
    }

    // a state into the engine (the audio thread, or before it runs)
    void applyState(Driver &dv, const State &s)
    {
        for (int i = 0; i < NPARAMS; ++i)
        {
            const float x = toValue(rangeOf(i), s.v[static_cast<size_t>(i)]);
            if (i < NOSC)
                dv.osc(i / OSC_PARAM_COUNT, i % OSC_PARAM_COUNT, x);
            else
                dv.global(i - NOSC, x);
        }
        for (int k = 0; k < SLOTS; ++k)
        {
            const Route &r = s.routes[static_cast<size_t>(k)];
            dv.route(k, r.src, std::max(0, r.dst), r.amount, r.on && r.dst >= 0);
        }
        for (int m = 0; m < 3; ++m)
            for (int q = 0; q < 4; ++q)
                dv.extra(m * 4 + q, s.modEnv[m][q]);
        dv.extra(12, static_cast<float>(s.tubeModel));
        dv.extra(13, s.tubeChaos);
        for (size_t i = 0; i < s.vecPath.size(); ++i)
            for (int k = 0; k < 3; ++k)
                dv.extra(17 + static_cast<int>(i) * 3 + k, s.vecPath[i][static_cast<size_t>(k)]);
        dv.extra(16, static_cast<float>(s.vecPath.size()));
        dv.extra(15, s.vecLoop ? 1.0f : 0.0f);
        dv.extra(14, s.vecOn ? 1.0f : 0.0f);
    }

    nlohmann::json toJson(const State &s, int program)
    {
        nlohmann::json j, v = nlohmann::json::object(), routes = nlohmann::json::array(), env = nlohmann::json::array(), path = nlohmann::json::array();
        for (int i = 0; i < NPARAMS; ++i)
            v[nameOf(i)] = s.v[static_cast<size_t>(i)];
        for (const Route &r : s.routes)
            if (r.dst >= 0)
                routes.push_back({r.src, r.dst, r.amount, r.on});
        for (const auto &e : s.modEnv)
            env.push_back({e[0], e[1], e[2], e[3]});
        for (const auto &p : s.vecPath)
            path.push_back({p[0], p[1], p[2]});
        j["product"] = "ObsSynth4D", j["name"] = s.name, j["program"] = program, j["values"] = v, j["routes"] = routes, j["modEnv"] = env;
        j["tube"] = {s.tubeModel, s.tubeChaos};
        j["vector"] = {{"on", s.vecOn}, {"loop", s.vecLoop}, {"path", path}};
        return j;
    }

    // ── where things are ────────────────────────────────────────────────────────────────────────
    std::string findBank(const char *file)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> dirs = synth::clap::pcmDirs();
#ifdef SYNTH_PCM_DIR
        dirs.push_back(SYNTH_PCM_DIR);
#endif
        for (const auto &d : dirs)
        {
            std::error_code ec;
            if (fs::is_regular_file(d / file, ec))
                return (d / file).string();
        }
        return {};
    }

    // the panel: $OBS_4D_PANEL, beside the plugin, in a VST3's Resources, or where it was built
    std::string panelProgram()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries;
        if (const char *e = std::getenv("OBS_4D_PANEL"))
            tries.push_back(e);
#ifdef _WIN32
        const char *exe = "obssynth4d.exe";
#else
        const char *exe = "obssynth4d";
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
        // Value, Program, Whole: the host's or a state's; the rest the panel's (Value too: its knobs)
        enum Kind : uint8_t { Value, Program, Whole, RouteSet, Extra, Control, NoteOn, NoteOff } kind;
        int id;      // a value's; a route's slot; an extra's; a controller; a note
        float value; // 0..1; a route's amount (engine units); a velocity
        int src = 0, dst = -1;
        bool on = false;
    };

    struct Plugin
    {
        clap_plugin_t plugin{};
        const clap_host_t *host = nullptr;
        const clap_host_params_t *hostParams = nullptr;
        std::vector<State> presets; // the factory's, as panel states
        std::unique_ptr<synth::ObsSynth4D> engine;
        std::unique_ptr<Driver> driver;
        State state;                        // the audio thread's (and the main thread's before activate)
        State pending;                      // a whole state from the main thread (Change::Whole)
        std::atomic<bool> pendingReady{false};
        std::array<std::atomic<float>, NPARAMS> value{}; // what the host reads, 0..1
        std::atomic<float> program{0};
        std::atomic<bool> rescan{false};
        std::array<Change, QUEUE> queue{};
        std::atomic<size_t> head{0}, tail{0};
        std::vector<float> buffer;
        std::vector<std::shared_ptr<const synth::GrainSource>> grains; // kept here: never freed on the audio thread
        // each granular oscillator's recording (the main thread's): its name ("": the default) and the recording; one
        // handed to the audio thread through its slot (taken when its flag is back to 0)
        std::array<std::string, 4> grainFile{};
        std::array<std::shared_ptr<const synth::GrainSource>, 4> grainOf{};
        std::array<std::shared_ptr<const synth::GrainSource>, 4> grainSlot{};
        std::array<std::atomic<int>, 4> grainReady{};
        std::array<std::string, 4> grainWanted{}; // the panel's choice, not taken yet (under wholeLock)
        // the editor: the panel's changes (the io thread to the audio thread), what the panel was last told, the
        // whole state for it (the main thread to the io thread), the engine's state for its meters (the audio thread)
        synth::clap::RemoteEditor editor;
        std::array<Change, QUEUE> fromPanel{};
        std::atomic<size_t> panelHead{0}, panelTail{0};
        std::array<float, NPARAMS> told{};
        std::mutex wholeLock;
        std::string wholeLine; // "S <json>\n", not sent yet
        std::atomic<int> wholeProgram{-1}; // a program the audio thread took: its state for the panel (the main thread)
        std::atomic<bool> editorClosed{false};
        const clap_host_gui_t *hostGui = nullptr;
        bool embedded = false, wantShown = false;
        uint64_t parent = 0;
        double guiScale = 1.0;
        uint32_t guiW = 0, guiH = 0; // the size the host gave (set_size; 0: not yet, the default)
        std::array<float, SCOPE> scope{}; // the audio thread writes, the io thread reads (a frame may tear)
        std::atomic<int> scopeAt{0};
        std::array<std::atomic<float>, SOURCES> sources{};
        std::atomic<float> vecX{0.5f}, vecY{0.5f};

        void fromEditor(const Change &c) // the io thread
        {
            const size_t h = panelHead.load(std::memory_order_relaxed);
            if (h - panelTail.load(std::memory_order_acquire) == QUEUE)
                return;
            fromPanel[h % QUEUE] = c;
            panelHead.store(h + 1, std::memory_order_release);
        }
        // the panel's changes on the audio thread: into the state and the engine, its values to the host as events
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
        nlohmann::json json(const State &st, int prog) const // the state as saved and as the panel is told (the main thread)
        {
            nlohmann::json j = toJson(st, prog);
            j["grains"] = grainFile;
            return j;
        }
        void setWhole(const State &st, int prog) // the main thread
        {
            const std::string line = "S " + json(st, prog).dump() + "\n";
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
                value[static_cast<size_t>(i)].store(state.v[static_cast<size_t>(i)], std::memory_order_relaxed);
        }

        void apply(const Change &c) // the audio thread (or before it runs)
        {
            switch (c.kind)
            {
            case Change::Value:
                if (c.id < 0 || c.id >= NPARAMS)
                    return;
                state.v[static_cast<size_t>(c.id)] = c.value;
                value[static_cast<size_t>(c.id)].store(c.value, std::memory_order_relaxed);
                if (driver)
                {
                    const float x = toValue(rangeOf(c.id), c.value);
                    if (c.id < NOSC)
                        driver->osc(c.id / OSC_PARAM_COUNT, c.id % OSC_PARAM_COUNT, x);
                    else
                        driver->global(c.id - NOSC, x);
                }
                break;
            case Change::Program:
            {
                const int k = std::clamp(static_cast<int>(std::lround(c.value)), 0, static_cast<int>(presets.size()) - 1);
                state = presets[static_cast<size_t>(k)]; // copies a vector (the path): a preset change, not a sample's work
                program.store(static_cast<float>(k));
                if (driver)
                    applyState(*driver, state);
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
                    state = pending;
                    if (driver)
                        applyState(*driver, state);
                    publish();
                }
                break;
            case Change::RouteSet:
                if (c.id < 0 || c.id >= SLOTS)
                    return;
                state.routes[static_cast<size_t>(c.id)] = {c.src, c.dst, c.value, c.on};
                if (driver)
                    driver->route(c.id, c.src, std::max(0, c.dst), c.value, c.on && c.dst >= 0);
                break;
            case Change::Extra: // as Driver::extra numbers them (applyState)
                if (c.id < 12)
                    state.modEnv[c.id / 4][c.id % 4] = c.value;
                else if (c.id == 12)
                    state.tubeModel = static_cast<int>(std::lround(c.value));
                else if (c.id == 13)
                    state.tubeChaos = c.value;
                else if (c.id == 14)
                    state.vecOn = c.value >= 0.5f;
                else if (c.id == 15)
                    state.vecLoop = c.value >= 0.5f;
                else if (c.id == 16) // the points' count (they came before it): within the 16 reserved, no allocation
                    state.vecPath.resize(static_cast<size_t>(std::clamp(static_cast<int>(std::lround(c.value)), 0, 16)));
                else if (const size_t i = static_cast<size_t>(c.id - 17) / 3; i < 16)
                {
                    if (i >= state.vecPath.size())
                        state.vecPath.resize(i + 1);
                    state.vecPath[i][static_cast<size_t>((c.id - 17) % 3)] = c.value;
                }
                if (driver)
                    driver->extra(c.id, c.value);
                break;
            case Change::Control: // the panel's mod sources, as their MIDI controllers would
                if (!engine)
                    return;
                if (c.id == 1)
                    engine->setModWheel(c.value);
                else if (c.id == 11)
                    engine->setExpression(c.value);
                else if (c.id >= 16 && c.id <= 19)
                    engine->setCC(c.id - 16, c.value);
                else if (c.id == 120 || c.id == 123)
                    engine->allNotesOff();
                break;
            case Change::NoteOn:
                if (engine)
                    engine->noteOn(static_cast<uint8_t>(c.id), c.value);
                break;
            case Change::NoteOff:
                if (engine)
                    engine->noteOff(static_cast<uint8_t>(c.id));
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
                if (p->param_id == PROGRAM_ID)
                    apply({Change::Program, 0, static_cast<float>(p->value)});
                else
                    apply({Change::Value, static_cast<int>(p->param_id), static_cast<float>(p->value)});
                return;
            }
            if (!engine)
                return;
            switch (h->type)
            {
            case CLAP_EVENT_NOTE_ON:
            {
                const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
                if (n->key >= 0)
                    engine->noteOn(static_cast<uint8_t>(n->key), static_cast<float>(n->velocity));
                break;
            }
            case CLAP_EVENT_NOTE_OFF:
            case CLAP_EVENT_NOTE_CHOKE:
            {
                const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
                if (n->key >= 0)
                    engine->noteOff(static_cast<uint8_t>(n->key));
                else
                    engine->allNotesOff();
                break;
            }
            case CLAP_EVENT_MIDI:
            {
                const auto *m = reinterpret_cast<const clap_event_midi_t *>(h);
                const uint8_t st = m->data[0] & 0xF0, d1 = m->data[1], d2 = m->data[2];
                const float n = static_cast<float>(d2) / 127.0f;
                if (st == 0x90 && d2 > 0)
                    engine->noteOn(d1, n);
                else if (st == 0x80 || (st == 0x90 && d2 == 0))
                    engine->noteOff(d1);
                else if (st == 0xB0 && d1 == 1)
                    engine->setModWheel(n);
                else if (st == 0xB0 && d1 == 11)
                    engine->setExpression(n);
                else if (st == 0xB0 && d1 >= 16 && d1 <= 19)
                    engine->setCC(d1 - 16, n);
                else if (st == 0xB0 && (d1 == 120 || d1 == 123))
                    engine->allNotesOff();
                else if (st == 0xE0)
                    engine->setPitchBend((static_cast<float>((d2 << 7) | d1) - 8192.0f) / 8192.0f * 2.0f);
                else if (st == 0xD0)
                    engine->setChannelAftertouch(static_cast<float>(d1) / 127.0f);
                break;
            }
            default: break;
            }
        }
    };

    Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

    std::string shown(int id, float n)
    {
        const Range &rg = rangeOf(id);
        const float v = toValue(rg, n);
        const std::string name = nameOf(id);
        char b[48];
        const double d = static_cast<double>(v);
        auto has = [&](const char *s) { return name.find(s) != std::string::npos; };
        if (rg.steps > 0)
            std::snprintf(b, sizeof b, "%d", static_cast<int>(std::lround(v)));
        else if (has("CUTOFF") || has("HPF") || has("RATE"))
            std::snprintf(b, sizeof b, d >= 1000 ? "%.2f kHz" : "%.1f Hz", d >= 1000 ? d / 1000 : d);
        else if (has("_ATK") || has("_DEC") || has("_REL") || has("DLY_L") || has("DLY_R"))
            std::snprintf(b, sizeof b, d >= 1 ? "%.2f s" : "%.0f ms", d >= 1 ? d : d * 1000);
        else if (has("TUNE"))
            std::snprintf(b, sizeof b, "%+.2f st", d);
        else
            std::snprintf(b, sizeof b, "%.2f", d);
        return b;
    }

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
        std::snprintf(info->name, sizeof info->name, "%s", "Notes");
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
            std::snprintf(info->module, sizeof info->module, "%s", "Presets");
            info->max_value = static_cast<double>(self(p)->presets.size() - 1);
            return true;
        }
        const int id = static_cast<int>(index);
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | (rangeOf(id).steps > 0 ? CLAP_PARAM_IS_STEPPED : 0);
        std::snprintf(info->name, sizeof info->name, "%s", nameOf(id).c_str());
        std::snprintf(info->module, sizeof info->module, "%s", id < NOSC ? ("Oscillator " + std::to_string(id / OSC_PARAM_COUNT + 1)).c_str() : "Global");
        info->min_value = 0, info->max_value = 1;
        info->default_value = static_cast<double>(toNorm(rangeOf(id), rangeOf(id).def));
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
            const auto &pr = self(p)->presets;
            std::snprintf(out, size, "%s", pr[static_cast<size_t>(std::clamp(static_cast<long>(std::lround(v)), 0L, static_cast<long>(pr.size()) - 1))].name.c_str());
            return true;
        }
        if (id >= NPARAMS)
            return false;
        std::snprintf(out, size, "%s", shown(static_cast<int>(id), static_cast<float>(v)).c_str());
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
        State copy = s->state; // the values from the atomics (the host's latest)
        for (int i = 0; i < NPARAMS; ++i)
            copy.v[static_cast<size_t>(i)] = s->value[static_cast<size_t>(i)].load();
        const std::string text = s->json(copy, static_cast<int>(s->program.load())).dump();
        for (size_t done = 0; done < text.size();)
        {
            const int64_t w = stream->write(stream, text.data() + done, text.size() - done);
            if (w <= 0)
                return false;
            done += static_cast<size_t>(w);
        }
        return true;
    }
    // an oscillator's recording by name (the main thread): read here, kept, handed to the audio thread when it runs.
    // false: the last one is not taken yet (again later)
    bool setGrain(Plugin *s, int o, const std::string &name)
    {
        const size_t oi = static_cast<size_t>(o);
        if (o < 0 || o > 3 || name == s->grainFile[oi])
            return true;
        if (s->engine && s->grainReady[oi].load(std::memory_order_acquire))
            return false;
        auto src = name.empty() ? (s->grains.empty() ? nullptr : s->grains.front()) : synth::GrainSource::loadWav(samplePath(name));
        synth::editorlog::say("ObsSynth4D", "plugin", "grain %d: %s%s", o, name.empty() ? "the default" : name.c_str(), src ? "" : " (CANNOT READ IT)");
        if (!src)
            return true; // unreadable: as it was
        s->grains.push_back(src);
        s->grainFile[oi] = name;
        s->grainOf[oi] = src;
        if (s->engine)
        {
            s->grainSlot[oi] = std::move(src);
            s->grainReady[oi].store(1, std::memory_order_release);
        }
        return true;
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
            State st = s->presets.front();
            int program = 0;
            if (j.contains("preset"))
                for (size_t k = 0; k < s->presets.size(); ++k)
                    if (s->presets[k].name == j["preset"].get<std::string>())
                        st = s->presets[k], program = static_cast<int>(k);
            if (j.contains("program") && !j.contains("preset"))
                program = j["program"].get<int>();
            if (j.contains("values"))
                for (int i = 0; i < NPARAMS; ++i)
                    if (j["values"].contains(nameOf(i)))
                        st.v[static_cast<size_t>(i)] = j["values"][nameOf(i)].get<float>();
            if (j.contains("routes"))
            {
                st.routes = {};
                size_t k = 0;
                for (const auto &r : j["routes"])
                    if (k < st.routes.size())
                        st.routes[k++] = {r[0].get<int>(), r[1].get<int>(), r[2].get<float>(), r[3].get<bool>()};
            }
            if (j.contains("modEnv"))
                for (size_t m = 0; m < 3 && m < j["modEnv"].size(); ++m)
                    for (size_t q = 0; q < 4; ++q)
                        st.modEnv[m][q] = j["modEnv"][m][q].get<float>();
            if (j.contains("tube"))
                st.tubeModel = j["tube"][0].get<int>(), st.tubeChaos = j["tube"][1].get<float>();
            if (j.contains("vector"))
            {
                st.vecOn = j["vector"].value("on", false), st.vecLoop = j["vector"].value("loop", false);
                st.vecPath.clear();
                for (const auto &q : j["vector"].value("path", nlohmann::json::array()))
                    if (st.vecPath.size() < 16)
                        st.vecPath.push_back({q[0].get<float>(), q[1].get<float>(), q[2].get<float>()});
            }
            st.name = j.value("name", st.name);
            if (j.contains("grains"))
                for (int o = 0; o < 4 && o < static_cast<int>(j["grains"].size()); ++o)
                    setGrain(s, o, j["grains"][static_cast<size_t>(o)].get<std::string>());
            for (int i = 0; i < NPARAMS; ++i)
                s->value[static_cast<size_t>(i)].store(st.v[static_cast<size_t>(i)]);
            s->program.store(static_cast<float>(program));
            st.vecPath.reserve(16);
            if (s->editor.running()) // the panel: the whole of it
                s->setWhole(st, program);
            if (s->engine) // playing: the audio thread takes it whole
            {
                s->pending = st;
                s->pendingReady = true;
                s->push({Change::Whole, 0, 0});
            }
            else
                s->state = st;
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

    // ── the editor (clap.gui): the panel as its own program, as ObsSynthCT1's (ct1_clap.cpp). On Windows inside the
    // host's window (set_parent: an HWND the panel's window becomes a child of), or floating; elsewhere floating.
#define EDLOG(...) synth::editorlog::say("ObsSynth4D", "plugin", __VA_ARGS__)
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
            EDLOG("create: NO PANEL found (obssynth4d beside the plugin, in ../Resources, or $OBS_4D_PANEL)");
            return false;
        }
        s->embedded = !floating;
        s->parent = 0;
        s->wantShown = false;
        s->editor.onLine = [s](const std::string &line)
        {
            int id = 0, a = 0, b = 0, on = 0;
            float v = 0;
            if (std::sscanf(line.c_str(), "p %d %f", &id, &v) == 2 && id >= 0 && id < NPARAMS)
            {
                s->value[static_cast<size_t>(id)].store(v);
                s->told[static_cast<size_t>(id)] = v; // the panel has it: not sent back
                s->fromEditor({Change::Value, id, v});
                if (s->hostParams && s->hostParams->request_flush)
                    s->hostParams->request_flush(s->host);
            }
            else if (std::sscanf(line.c_str(), "r %d %d %d %f %d", &id, &a, &b, &v, &on) == 5)
                s->fromEditor({Change::RouteSet, id, v, a, b, on != 0});
            else if (std::sscanf(line.c_str(), "x %d %f", &id, &v) == 2 && id >= 0)
                s->fromEditor({Change::Extra, id, v});
            else if (std::sscanf(line.c_str(), "g %d", &id) == 1 && id >= 0 && id < 4) // "g osc name": a recording, read on the main thread
            {
                const size_t at = line.find(' ', 2);
                {
                    std::lock_guard<std::mutex> g(s->wholeLock);
                    s->grainWanted[static_cast<size_t>(id)] = at == std::string::npos ? std::string() : line.substr(at + 1);
                }
                if (s->host->request_callback)
                    s->host->request_callback(s->host);
            }
            else if (std::sscanf(line.c_str(), "c %d %f", &id, &v) == 2)
                s->fromEditor({Change::Control, id, v});
            else if (std::sscanf(line.c_str(), "n %d %f", &id, &v) == 2 && id >= 0 && id < 128)
                s->fromEditor({Change::NoteOn, id, v});
            else if (std::sscanf(line.c_str(), "o %d", &id) == 1 && id >= 0 && id < 128)
                s->fromEditor({Change::NoteOff, id, 0});
        };
        s->editor.tick = [s]
        {
            std::string out;
            {
                std::lock_guard<std::mutex> g(s->wholeLock);
                out.swap(s->wholeLine);
            }
            char b[64];
            for (int i = 0; i < NPARAMS; ++i) // what the host changed (automation, a program, a state)
            {
                const float v = s->value[static_cast<size_t>(i)].load();
                if (v != s->told[static_cast<size_t>(i)])
                {
                    std::snprintf(b, sizeof b, "v %d %.9g\n", i, static_cast<double>(v));
                    out += b;
                    s->told[static_cast<size_t>(i)] = v;
                }
            }
            std::snprintf(b, sizeof b, "s %.4f %.4f", static_cast<double>(s->vecX.load()), static_cast<double>(s->vecY.load()));
            out += b;
            for (const auto &src : s->sources)
            {
                std::snprintf(b, sizeof b, " %.4f", static_cast<double>(src.load(std::memory_order_relaxed)));
                out += b;
            }
            out += "\nw";
            const int at = s->scopeAt.load(std::memory_order_acquire);
            for (int i = 0; i < SCOPE; ++i)
            {
                std::snprintf(b, sizeof b, " %.4f", static_cast<double>(s->scope[static_cast<size_t>((at + i) % SCOPE)]));
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
        // inside a host: 1086 x 720 (the panel is drawn for 1600 x 900 less its PLAY column, 1358 x 900, and scales), in
        // pixels; its own window 1358 x 900
        const Plugin *s = self(p);
        if (s->embedded && s->guiW && s->guiH) // the host's (a window it remembers wider, say): the panel fills it
        {
            *w = s->guiW;
            *h = s->guiH;
            return true;
        }
        *w = s->embedded ? static_cast<uint32_t>(1086 * s->guiScale) : 1358;
        *h = s->embedded ? static_cast<uint32_t>(720 * s->guiScale) : 900;
        return true;
    }
    // resizable, its proportions kept: a host's window of another size (Live's remembered one) holds the panel whole,
    // no strip of the host's left around it
    bool guiCanResize(const clap_plugin_t *p) { return self(p)->embedded; }
    bool guiResizeHints(const clap_plugin_t *, clap_gui_resize_hints_t *hints)
    {
        hints->can_resize_horizontally = true;
        hints->can_resize_vertically = true;
        hints->preserve_aspect_ratio = true;
        hints->aspect_ratio_width = 1086;
        hints->aspect_ratio_height = 720;
        return true;
    }
    bool guiAdjustSize(const clap_plugin_t *, uint32_t *w, uint32_t *h)
    {
        const double k = std::max(0.5, std::min(*w / 1086.0, *h / 720.0));
        *w = static_cast<uint32_t>(1086 * k);
        *h = static_cast<uint32_t>(720 * k);
        return true;
    }
    bool guiSetSize(const clap_plugin_t *p, uint32_t w, uint32_t h)
    {
        Plugin *s = self(p); // the panel follows its host window by itself
        s->guiW = w;
        s->guiH = h;
        EDLOG("set_size %u x %u", w, h);
        return true;
    }
    bool openPanel(Plugin *s)
    {
        s->wantShown = false;
        s->told.fill(-12345.0f); // a new panel: every value, once
        State now = s->state;    // as stateSave reads it: the values from the atomics
        for (int i = 0; i < NPARAMS; ++i)
            now.v[static_cast<size_t>(i)] = s->value[static_cast<size_t>(i)].load();
        s->setWhole(now, static_cast<int>(s->program.load()));
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
    const char *kBanks[4] = {"bank_morph_32waves_1024samples_32float.wav", "bank_morph_32waves_1024samples_32float.wav",
                             "bank_harmonic_16waves_1024samples_32float.wav", "bank_all_waves_32waves_1024samples_32float.wav"};

    // an engine at a rate, its banks and recording loaded, the state on it (the main thread)
    void build(Plugin *s, float rate)
    {
        s->engine = std::make_unique<synth::ObsSynth4D>(rate);
        s->driver = std::make_unique<Driver>(*s->engine);
        for (int o = 0; o < 4; ++o)
        {
            const std::string bank = findBank(kBanks[o]);
            if (!bank.empty())
                s->engine->loadWtBank(o, bank);
            s->driver->tables[static_cast<size_t>(o)] = s->engine->getEngine().getAllocator().getVoice(0).getChain(o).getWtTableCount();
        }
        if (s->grains.empty())
        {
            std::string path = samplePath("alsa-rear-center.wav"); // the ALSA voice where it is, else the organ shipped in pcm/
            if (!std::filesystem::exists(path))
                path = "/usr/share/sounds/alsa/Rear_Center.wav";
            if (!std::filesystem::exists(path))
                path = samplePath("vcsl-pipe-organ-quiet-C4.wav");
            if (auto g = synth::GrainSource::loadWav(path))
                s->grains.push_back(std::move(g));
        }
        for (int o = 0; o < 4; ++o)
            if (const auto &g = s->grainOf[static_cast<size_t>(o)] ? s->grainOf[static_cast<size_t>(o)] : s->grains.empty() ? nullptr : s->grains.front())
                s->engine->setGrainSource(o, g);
        applyState(*s->driver, s->state);
    }

    bool init(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        s->hostParams = static_cast<const clap_host_params_t *>(s->host->get_extension(s->host, CLAP_EXT_PARAMS));
        for (const auto &d : synth::clap::pcmDirs()) // the grains' recordings: shipped beside the plugin
            synth::addSampleDir(d.lexically_normal().string());
#ifdef SYNTH_PCM_DIR
        synth::addSampleDir(SYNTH_PCM_DIR);
#endif
        // the factory's presets as panel states (their tables: the banks' counts, from a first engine)
        build(s, 48000.0f);
        int tables[4];
        for (int o = 0; o < 4; ++o)
            tables[o] = s->driver->tables[static_cast<size_t>(o)];
        for (const PresetEntry &e : listPresets())
        {
            PresetData d{};
            if (e.bank >= 0 && loadPreset(e, tables, d))
                s->presets.push_back(fromPreset(d));
        }
        if (s->presets.empty())
            s->presets.push_back(State{});
        s->state = s->presets.front();
        for (size_t k = 0; k < s->presets.size(); ++k)
            if (s->presets[k].name == "Lush Poly Pad") // a pad, to start: what it plays first
                s->state = s->presets[k], s->program.store(static_cast<float>(k));
        s->state.vecPath.reserve(16); // the panel's points land here on the audio thread: never an allocation there
        s->publish();
        s->engine.reset();
        s->driver.reset();
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
        build(s, static_cast<float>(rate));
        s->buffer.assign(static_cast<size_t>(std::max<uint32_t>(maxFrames, 64)) * 2, 0.0f);
        return true;
    }
    void deactivate(const clap_plugin_t *p)
    {
        self(p)->driver.reset();
        self(p)->engine.reset();
    }
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
        for (size_t o = 0; o < 4; ++o) // a recording the main thread handed over (a pointer: kept there, never freed here)
            if (s->grainReady[o].load(std::memory_order_acquire))
            {
                s->engine->setGrainSource(static_cast<int>(o), s->grainSlot[o]);
                s->grainReady[o].store(0, std::memory_order_release);
            }
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
                s->engine->generateStereoBuffer(s->buffer.data(), static_cast<int>(n));
                for (uint32_t i = 0; i < n; ++i)
                    L[at + i] = s->buffer[i * 2], R[at + i] = s->buffer[i * 2 + 1];
                at += n;
            }
        }
        for (; ev < nev; ++ev)
            s->event(pr->in_events->get(pr->in_events, ev));
        if (s->editor.running()) // for the panel's scope, matrix meters and vector
        {
            int at = s->scopeAt.load(std::memory_order_relaxed);
            for (uint32_t i = 0; i < frames; ++i)
            {
                s->scope[static_cast<size_t>(at)] = L[i];
                at = (at + 1) % SCOPE;
            }
            s->scopeAt.store(at, std::memory_order_release);
            const auto &mm = s->engine->getEngine().getModMatrix();
            for (int i = 0; i < SOURCES; ++i)
                s->sources[static_cast<size_t>(i)].store(mm.getSourceValue(i), std::memory_order_relaxed);
            s->vecX.store(s->engine->getEngine().vectorX(), std::memory_order_relaxed);
            s->vecY.store(s->engine->getEngine().vectorY(), std::memory_order_relaxed);
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
        if (const int k = s->wholeProgram.exchange(-1); k >= 0 && k < static_cast<int>(s->presets.size())) // a program: the panel's too
            s->setWhole(s->presets[static_cast<size_t>(k)], k);
        std::array<std::string, 4> wanted;
        {
            std::lock_guard<std::mutex> g(s->wholeLock);
            wanted.swap(s->grainWanted);
        }
        bool later = false;
        for (int o = 0; o < 4; ++o)
            if (!wanted[static_cast<size_t>(o)].empty() && !setGrain(s, o, wanted[static_cast<size_t>(o)]))
            {
                std::lock_guard<std::mutex> g(s->wholeLock); // the audio thread has not taken the last one: again soon
                if (s->grainWanted[static_cast<size_t>(o)].empty())
                    s->grainWanted[static_cast<size_t>(o)] = wanted[static_cast<size_t>(o)];
                later = true;
            }
        if (later && s->host->request_callback)
            s->host->request_callback(s->host);
        if (s->editorClosed.exchange(false) && s->hostGui) // the panel's window was closed by the person
            s->hostGui->closed(s->host, false);
    }

    const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
    const clap_plugin_descriptor_t descriptor{CLAP_VERSION_INIT, "org.carbonian.obssynth4d", "ObsSynth4D", "Carbonian",
                                              "https://github.com/orianbsilva/obssynth", "", "", "0.1.0",
                                              "Four oscillators (standard, wavetable, FM, granular), vector synthesis and a mod matrix", features};

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
