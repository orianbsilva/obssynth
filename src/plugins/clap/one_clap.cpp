// ObsSynth One as a CLAP plugin (ObsSynthOne.clap): one oscillator with the four engines of the product line (standard,
// wavetable, FM, granular), one filter, an amplitude and a filter envelope, the tube and the delay: the product
// ObsSynthOne (products/obs_one), its own wiring over the engine ObsSynth4D uses. Its parameters keep 4D's names and
// ranges (app/obssynth4d/params.hpp) (OSC_CUTOFF, OSC_AMP_ATK, GLB_DLY_FB...), so a host
// that knows one of the synths knows the other; a language that wants Strudel's words converts them on its side.
//   notes: one note port, CLAP or MIDI; the wheel (CC1), expression (CC11), bend and pressure as MIDI.
//   params: 0..1 as the knobs turn (their text in their units), automatable; PROGRAM (not automatable) loads one of
//     the factory's few presets.
//   state: JSON, the values by name.
// The editor is the panel (obssynthone --remote LINK, app/obssynthone: docs/mockups/obssynthone_mockup.py), a program of its own
// (remote_editor.hpp): a window of its own (floating), what it turns comes back as parameter changes the host records. The wavetable and the grains' recording are found as
// ObsSynth4D finds them (beside the plugin, $OBS_PCM_DIR, ~/.local/share/obssynth4d, the source tree).
// Threads: the audio thread is the engine's only writer; a value from the main thread goes through a queue the next
// process() or flush() applies. The values the host reads are atomics.
#include <clap/clap.h>

#include "../../app/obssynthone/one_driver.hpp"
#include "../../app/obssynth4d/presets.hpp"
#include "../../lib/synth/core/grain_engine.hpp"
#include "../../vendor/json.hpp"
#include "../../app/common/sample_find.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "plugin_path.hpp"
#include "remote_editor.hpp"

using namespace obs;

namespace
{
    constexpr int QUEUE = 4096;

    using namespace onep;

    // the recordings the sample engine can play: the project's pcm/ and the user's samples folder, but not the wavetables
    // (bank_*, cycle_*, *_wave_*); read once at init, on the main thread
    std::vector<std::string> &samplePaths()
    {
        static std::vector<std::string> v;
        return v;
    }
    void listSamples()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> dirs = synth::clap::pcmDirs();
#ifdef SYNTH_PCM_DIR
        dirs.push_back(SYNTH_PCM_DIR);
#endif
        dirs.push_back(samplesDir());
        std::vector<std::string> found;
        for (const auto &d : dirs)
        {
            std::error_code ec;
            for (const auto &f : fs::directory_iterator(d, ec))
            {
                const std::string n = f.path().filename().string();
                if (f.path().extension() == ".wav" && n.rfind("bank_", 0) != 0 && n.rfind("cycle_", 0) != 0 && n.find("_wave_") == std::string::npos &&
                    std::none_of(found.begin(), found.end(), [&](const std::string &x) { return fs::path(x).filename() == n; }))
                    found.push_back(f.path().string());
            }
        }
        std::sort(found.begin(), found.end(), [](const std::string &a, const std::string &b) { return fs::path(a).filename() < fs::path(b).filename(); });
        if (found.size() > 64)
            found.resize(64);
        samplePaths() = found;
    }
    std::string sampleName(int i)
    {
        const auto &v = samplePaths();
        return i >= 0 && i < static_cast<int>(v.size()) ? std::filesystem::path(v[static_cast<size_t>(i)]).filename().string() : std::string("-");
    }

    // a recording's file name without its extension, lowercase, for a program to name it (read once: names()[i] is stable)
    std::vector<std::string> &sampleStems()
    {
        static std::vector<std::string> v;
        return v;
    }

    // ── a program's words: SysEx messages, text in the stream of the notes (docs/ONE_SYSEX.md) ──
    // F0 7D "NAME VALUE" F7: NAME is a parameter's name (or PROGRAM); VALUE a number in the parameter's own units, or a word
    // (an engine, a wave, a filter type, a recording's name, a preset's name). 7D is MIDI's manufacturer id for non-commercial use.
    bool same(const char *a, const char *b)
    {
        for (; *a && *b; ++a, ++b)
            if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
                return false;
        return *a == *b;
    }
    int firstOf(const char *w, std::initializer_list<const char *> names) // the index of the name that is w, or -1
    {
        int i = 0;
        for (const char *n : names)
        {
            if (same(w, n))
                return i;
            ++i;
        }
        return -1;
    }

    Values fromPreset(const Preset &p)
    {
        return onep::fromPreset(p, [](const char *file) {
            for (int i = 0; i < static_cast<int>(samplePaths().size()); ++i)
                if (sampleName(i) == file)
                    return i;
            return -1;
        });
    }

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

    struct Change
    {
        enum Kind : uint8_t { Value, Program } kind;
        int id;
        float value;
    };

    struct Plugin
    {
        clap_plugin_t plugin{};
        const clap_host_t *host = nullptr;
        const clap_host_params_t *hostParams = nullptr;
        std::unique_ptr<synth::ObsSynthOne> engine;
        std::unique_ptr<Driver> driver;
        std::shared_ptr<const synth::GrainSource> grain; // kept here: never freed on the audio thread
        std::vector<std::shared_ptr<const synth::GrainSource>> samples; // the sample engine's recordings (read in init, never freed on the audio thread)
        bool listSent = false;
        Values state = defaults();                       // the audio thread's (the main thread's before activate)
        std::array<std::atomic<float>, NPARAMS> value{};
        std::atomic<float> program{0};
        std::atomic<bool> rescan{false};
        std::array<Change, QUEUE> queue{};
        std::atomic<size_t> head{0}, tail{0};
        std::vector<float> buffer;
        // the editor: the panel's changes (the io thread to the audio thread), what the panel was last told
        synth::clap::RemoteEditor editor;
        std::array<Change, QUEUE> fromPanel{};
        std::atomic<size_t> panelHead{0}, panelTail{0};
        std::array<float, NPARAMS> told{};
        int toldProgram = -1;
        std::atomic<bool> editorClosed{false};
        const clap_host_gui_t *hostGui = nullptr;

        void fromEditor(const Change &c) // the io thread
        {
            const size_t h = panelHead.load(std::memory_order_relaxed);
            if (h - panelTail.load(std::memory_order_acquire) == QUEUE)
                return;
            fromPanel[h % QUEUE] = c;
            panelHead.store(h + 1, std::memory_order_release);
        }
        void drainPanel(const clap_output_events_t *out) // the audio thread: the panel's changes into the engine, to the host as events
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

        void publish()
        {
            for (int i = 0; i < NPARAMS; ++i)
                value[static_cast<size_t>(i)].store(state[static_cast<size_t>(i)], std::memory_order_relaxed);
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
        void apply(const Change &c) // the audio thread (or before it runs)
        {
            if (c.kind == Change::Value)
            {
                if (c.id < 0 || c.id >= NPARAMS)
                    return;
                state[static_cast<size_t>(c.id)] = c.value;
                value[static_cast<size_t>(c.id)].store(c.value, std::memory_order_relaxed);
                if (driver)
                    drive(*driver, c.id, c.value);
                return;
            }
            const int k = std::clamp(static_cast<int>(std::lround(c.value)), 0, static_cast<int>(presets().size()) - 1);
            state = fromPreset(presets()[static_cast<size_t>(k)]);
            program.store(static_cast<float>(k));
            if (driver)
                applyAll(*driver, state);
            publish();
            rescan = true;
            if (host->request_callback)
                host->request_callback(host);
        }

        // a program's word (SysEx): "NAME VALUE"; the audio thread, no allocation
        void applyWord(const uint8_t *data, uint32_t size)
        {
            if (size > 0 && data[0] == 0xF0)
                ++data, --size;
            if (size > 0 && data[size - 1] == 0xF7)
                --size;
            if (size < 3 || data[0] != 0x7D || size > 120)
                return; // not ours
            char text[128];
            std::memcpy(text, data + 1, size - 1);
            text[size - 1] = 0;
            char *value = std::strchr(text, ' ');
            if (!value)
                return;
            *value++ = 0;
            while (*value == ' ')
                ++value;
            if (std::strlen(value) > 4 && same(value + std::strlen(value) - 4, ".wav"))
                value[std::strlen(value) - 4] = 0;
            if (same(text, "PROGRAM"))
            {
                int k = -1;
                for (size_t i = 0; i < presets().size() && k < 0; ++i)
                    if (same(value, presets()[i].name))
                        k = static_cast<int>(i);
                char *end = nullptr;
                const long n = std::strtol(value, &end, 10);
                if (k < 0 && end != value && *end == 0)
                    k = static_cast<int>(n) - 1; // from 1, as a person counts
                if (k >= 0)
                    apply({Change::Program, 0, static_cast<float>(k)});
                return;
            }
            const int id = idOfC(text);
            if (id < 0)
                return;
            float engineValue = 0;
            bool found = true;
            switch (DEFS[id].idx)
            {
            case X_PART_TYPE:
                engineValue = static_cast<float>(firstOf(value, {"std", "wt", "fm", "grain", "smp"}));
                if (engineValue < 0) // the other names for them
                    engineValue = static_cast<float>(firstOf(value, {"standard", "wavetable", "", "gr", "sample"}));
                if (engineValue < 0)
                    engineValue = static_cast<float>(firstOf(value, {"", "", "", "granular", ""}));
                found = engineValue >= 0;
                break;
            case OSC_WAVE:
                engineValue = static_cast<float>(firstOf(value, {"sine", "sawtooth", "square", "triangle", "pulse", "noise"}));
                if (engineValue < 0)
                    engineValue = static_cast<float>(firstOf(value, {"", "saw", "", "tri", "", "white"}));
                found = engineValue >= 0;
                break;
            case OSC_FILT_TYPE:
                engineValue = static_cast<float>(firstOf(value, {"lp", "hp", "bp", "notch"}));
                if (engineValue < 0)
                    engineValue = static_cast<float>(firstOf(value, {"lowpass", "highpass", "bandpass", ""}));
                found = engineValue >= 0;
                break;
            case X_SMP_SAMPLE:
            {
                found = false;
                std::string w(value);
                for (char &c : w)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (size_t i = 0; i < sampleStems().size(); ++i)
                    if (sampleStems()[i] == w)
                        engineValue = static_cast<float>(i), found = true;
                break;
            }
            default:
            {
                char *end = nullptr;
                engineValue = std::strtof(value, &end);
                found = end != value;
                break;
            }
            }
            if (!found)
                return;
            const Range &rg = rangeOf(id);
            apply({Change::Value, id, std::clamp(toNorm(rg, std::clamp(engineValue, std::min(rg.lo, rg.hi), std::max(rg.lo, rg.hi))), 0.0f, 1.0f)});
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
                else if (st == 0xB0 && (d1 == 120 || d1 == 123))
                    engine->allNotesOff();
                else if (st == 0xE0)
                    engine->setPitchBend((static_cast<float>((d2 << 7) | d1) - 8192.0f) / 8192.0f * 2.0f);
                else if (st == 0xD0)
                    engine->setChannelAftertouch(static_cast<float>(d1) / 127.0f);
                break;
            }
            case CLAP_EVENT_MIDI_SYSEX:
            {
                const auto *m = reinterpret_cast<const clap_event_midi_sysex_t *>(h);
                applyWord(m->buffer, m->size);
                break;
            }
            default: break;
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
        std::snprintf(info->name, sizeof info->name, "%s", "Notes");
        return true;
    }
    const clap_plugin_note_ports_t notePorts{notePortsCount, notePortsGet};

    uint32_t paramsCount(const clap_plugin_t *) { return NPARAMS + 1; }
    bool paramsInfo(const clap_plugin_t *, uint32_t index, clap_param_info_t *info)
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
            info->max_value = static_cast<double>(presets().size() - 1);
            return true;
        }
        const int id = static_cast<int>(index);
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | (rangeOf(id).steps > 0 ? CLAP_PARAM_IS_STEPPED : 0);
        std::snprintf(info->name, sizeof info->name, "%s", DEFS[id].name);
        std::snprintf(info->module, sizeof info->module, "%s", DEFS[id].glb ? "Global" : "Oscillator");
        info->min_value = 0, info->max_value = 1;
        info->default_value = static_cast<double>(toNorm(rangeOf(id), rangeOf(id).def));
        return true;
    }
    bool paramsValue(const clap_plugin_t *p, clap_id id, double *out)
    {
        if (id == PROGRAM_ID)
            return *out = static_cast<double>(self(p)->program.load()), true;
        if (id >= static_cast<clap_id>(NPARAMS))
            return false;
        *out = static_cast<double>(self(p)->value[id].load(std::memory_order_relaxed));
        return true;
    }
    bool paramsToText(const clap_plugin_t *, clap_id id, double v, char *out, uint32_t size)
    {
        if (id == PROGRAM_ID)
        {
            const auto &pr = presets();
            std::snprintf(out, size, "%s", pr[static_cast<size_t>(std::clamp(static_cast<long>(std::lround(v)), 0L, static_cast<long>(pr.size()) - 1))].name);
            return true;
        }
        if (id >= static_cast<clap_id>(NPARAMS))
            return false;
        if (DEFS[id].idx == X_MOD_SRC || DEFS[id].idx == X_MOD_DST || DEFS[id].idx == X_LFO_WAVE)
        {
            const int k = static_cast<int>(std::lround(toValue(rangeOf(static_cast<int>(id)), static_cast<float>(v))));
            const char *const *names = DEFS[id].idx == X_MOD_SRC ? kModSources : DEFS[id].idx == X_MOD_DST ? kModDests : kLfoWaves;
            std::snprintf(out, size, "%s", names[std::clamp(k, 0, DEFS[id].idx == X_MOD_SRC ? 8 : DEFS[id].idx == X_MOD_DST ? 7 : 4)]);
            return true;
        }
        if (DEFS[id].idx == X_SMP_SAMPLE)
        {
            std::snprintf(out, size, "%s", sampleName(static_cast<int>(std::lround(v * 63))).c_str());
            return true;
        }
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
        nlohmann::json j, v = nlohmann::json::object();
        for (int i = 0; i < NPARAMS; ++i)
            v[DEFS[i].name] = s->value[static_cast<size_t>(i)].load();
        j["product"] = "ObsSynthOne", j["program"] = s->program.load(), j["values"] = v;
        const std::string text = j.dump();
        const char *d = text.data();
        int64_t left = static_cast<int64_t>(text.size());
        while (left > 0)
        {
            const int64_t n = stream->write(stream, d, static_cast<uint64_t>(left));
            if (n <= 0)
                return false;
            d += n, left -= n;
        }
        return true;
    }
    bool stateLoad(const clap_plugin_t *p, const clap_istream_t *stream)
    {
        Plugin *s = self(p);
        std::string text;
        char buf[4096];
        for (int64_t n; (n = stream->read(stream, buf, sizeof buf)) > 0;)
            text.append(buf, static_cast<size_t>(n));
        const auto j = nlohmann::json::parse(text, nullptr, false);
        if (!j.is_object() || !j.contains("values"))
            return false;
        for (const auto &[k, x] : j["values"].items())
            if (const int i = idOf(k); i >= 0 && x.is_number())
                s->push({Change::Value, i, std::clamp(x.get<float>(), 0.0f, 1.0f)});
        s->rescan = true;
        return true;
    }
    const clap_plugin_state_t stateExt{stateSave, stateLoad};

    // ── the editor: the panel as a program of its own ───────────────────────────────────────────
    std::string panelProgram()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries;
        if (const char *e = std::getenv("OBS_ONE_PANEL"))
            tries.push_back(e);
        const fs::path dir = synth::clap::pluginDir();
        tries.push_back(dir / "obssynthone");
        tries.push_back(dir / ".." / "Resources" / "obssynthone");
#ifdef SYNTH_PANEL_DIR
        tries.push_back(fs::path(SYNTH_PANEL_DIR) / "obssynthone");
#endif
        for (const auto &p : tries)
        {
            std::error_code ec;
            if (fs::is_regular_file(p, ec))
                return p.string();
        }
        return {};
    }
    bool guiApiSupported(const clap_plugin_t *, const char *, bool floating) { return floating; }
    bool guiPreferredApi(const clap_plugin_t *, const char **api, bool *floating)
    {
        *api = CLAP_WINDOW_API_WAYLAND;
        *floating = true;
        return true;
    }
    bool guiCreate(const clap_plugin_t *p, const char *, bool floating)
    {
        Plugin *s = self(p);
        if (!floating || panelProgram().empty())
            return false;
        s->editor.onLine = [s](const std::string &line)
        {
            int id = 0;
            float v = 0;
            if (std::sscanf(line.c_str(), "p %d %f", &id, &v) != 2)
                return;
            if (id >= 0 && id < NPARAMS)
            {
                s->value[static_cast<size_t>(id)].store(v);
                s->told[static_cast<size_t>(id)] = v; // the panel has it: not sent back
                s->fromEditor({Change::Value, id, v});
            }
            else if (id == static_cast<int>(PROGRAM_ID))
                s->fromEditor({Change::Program, 0, v});
            else
                return;
            if (s->hostParams && s->hostParams->request_flush)
                s->hostParams->request_flush(s->host);
        };
        s->editor.tick = [s]
        {
            std::string out;
            char b[96];
            const int prog = static_cast<int>(std::lround(s->program.load()));
            if (prog != s->toldProgram) // a program, from the panel, the host or a state: its name, and how many there are
            {
                std::snprintf(b, sizeof b, "C %zu\nP %d %s\n", presets().size(), prog, presets()[static_cast<size_t>(std::clamp(prog, 0, static_cast<int>(presets().size()) - 1))].name);
                out += b;
                s->toldProgram = prog;
            }
            if (!s->listSent) // the recordings the sample engine has: their paths, once
            {
                for (size_t i = 0; i < samplePaths().size(); ++i)
                    out += "L " + std::to_string(i) + " " + samplePaths()[i] + "\n";
                s->listSent = true;
            }
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
            return out;
        };
        s->editor.onExit = [s] // its window closed: the host is told on the main thread
        {
            s->editorClosed = true;
            if (s->host->request_callback)
                s->host->request_callback(s->host);
        };
        return true;
    }
    void guiDestroy(const clap_plugin_t *p) { self(p)->editor.close(); }
    bool guiSetScale(const clap_plugin_t *, double) { return true; }
    bool guiGetSize(const clap_plugin_t *, uint32_t *w, uint32_t *h)
    {
        *w = 1280, *h = 780;
        return true;
    }
    bool guiCanResize(const clap_plugin_t *) { return false; }
    bool guiResizeHints(const clap_plugin_t *, clap_gui_resize_hints_t *) { return false; }
    bool guiAdjustSize(const clap_plugin_t *, uint32_t *, uint32_t *) { return false; }
    bool guiSetSize(const clap_plugin_t *, uint32_t, uint32_t) { return true; }
    bool guiSetParent(const clap_plugin_t *, const clap_window_t *) { return false; }
    bool guiSetTransient(const clap_plugin_t *, const clap_window_t *) { return true; }
    void guiSuggestTitle(const clap_plugin_t *, const char *) {}
    bool guiShow(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        if (s->editor.running())
            return true;
        s->told.fill(-12345.0f); // a new panel: every value, once
        s->toldProgram = -1;
        s->listSent = false;
        return s->editor.open(panelProgram(), {"--remote", synth::clap::RemoteEditor::LINK});
    }
    bool guiHide(const clap_plugin_t *p)
    {
        self(p)->editor.close();
        return true;
    }
    const clap_plugin_gui_t gui{guiApiSupported, guiPreferredApi, guiCreate, guiDestroy, guiSetScale, guiGetSize, guiCanResize, guiResizeHints,
                                guiAdjustSize, guiSetSize, guiSetParent, guiSetTransient, guiSuggestTitle, guiShow, guiHide};

    // ── the plugin's calls ──────────────────────────────────────────────────────────────────────
    void build(Plugin *s, float rate)
    {
        const synth::EngineProfile prof = synth::engineProfile(); // lite on a 32-bit ARM (OBS_PROFILE=lite|full)
        s->engine = std::make_unique<synth::ObsSynthOne>(rate, prof.oversample);
        s->engine->applyProfile(prof);
        s->driver = std::make_unique<Driver>(Driver{*s->engine});
        s->driver->samples = &s->samples;
        const std::string bank = findBank("bank_morph_32waves_1024samples_32float.wav");
        if (!bank.empty())
            s->engine->loadWtBank(bank);
        s->driver->tables = s->engine->wtTableCount();
        if (!s->grain)
        {
            std::string path = samplePath("alsa-rear-center.wav");
            if (!std::filesystem::exists(path))
                path = "/usr/share/sounds/alsa/Rear_Center.wav";
            if (!std::filesystem::exists(path))
                path = samplePath("vcsl-pipe-organ-quiet-C4.wav");
            s->grain = synth::GrainSource::loadWav(path);
        }
        if (s->grain)
            s->engine->setGrainSource(s->grain);
        applyAll(*s->driver, s->state);
    }

    bool init(const clap_plugin_t *p)
    {
        Plugin *s = self(p);
        s->hostParams = static_cast<const clap_host_params_t *>(s->host->get_extension(s->host, CLAP_EXT_PARAMS));
        for (const auto &d : synth::clap::pcmDirs())
            synth::addSampleDir(d.lexically_normal().string());
#ifdef SYNTH_PCM_DIR
        synth::addSampleDir(SYNTH_PCM_DIR);
#endif
        listSamples();
        for (const std::string &path : samplePaths())
        {
            s->samples.push_back(synth::GrainSource::loadWav(path));
            std::string stem = std::filesystem::path(path).stem().string();
            for (char &c : stem)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            sampleStems().push_back(stem);
        }
        s->state = fromPreset(presets().front());
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
        if (s->editorClosed.exchange(false) && s->hostGui) // the panel's window was closed by the person
            s->hostGui->closed(s->host, false);
    }

    const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
    const clap_plugin_descriptor_t descriptor{CLAP_VERSION_INIT, "org.carbonian.obssynthone", "ObsSynthOne", "Carbonian",
                                              "https://github.com/orianbsilva/obssynth", "", "", "0.1.0",
                                              "One oscillator with four engines (standard, wavetable, FM, granular), a filter, envelopes, delay and tube", features};

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
