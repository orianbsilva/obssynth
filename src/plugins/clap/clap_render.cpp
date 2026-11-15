// clap_render: a host of the least kind, to try a CLAP instrument without a DAW. Loads the .clap, creates its plugin,
// plays a phrase (notes, the wheel, a parameter automated sample-accurately), switches a program, saves and loads
// its state, and writes what it played as 48 kHz stereo WAVs.
// usage: clap_render <plugin.clap> <out folder>
#include <clap/clap.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace
{
    constexpr double RATE = 48000;
    constexpr uint32_t BLOCK = 256;

    // the host: no extension of its own, the callbacks only noted
    bool callbackRequested = false;
    const void *hostExtension(const clap_host_t *, const char *) { return nullptr; }
    void hostNoop(const clap_host_t *) {}
    void hostCallback(const clap_host_t *) { callbackRequested = true; }
    const clap_host_t host{CLAP_VERSION_INIT, nullptr, "clap_render", "Carbonian", "", "0.1", hostExtension, hostNoop, hostNoop, hostCallback};

    // an event list the plugin reads (a block's worth)
    struct Events
    {
        std::vector<std::vector<uint8_t>> ev;
        clap_input_events_t list{this, size, get};
        static uint32_t size(const clap_input_events_t *l) { return static_cast<uint32_t>(static_cast<const Events *>(l->ctx)->ev.size()); }
        static const clap_event_header_t *get(const clap_input_events_t *l, uint32_t i)
        {
            return reinterpret_cast<const clap_event_header_t *>(static_cast<const Events *>(l->ctx)->ev[i].data());
        }
        template <typename T>
        void add(const T &e)
        {
            std::vector<uint8_t> b(sizeof e);
            std::memcpy(b.data(), &e, sizeof e);
            ev.push_back(std::move(b));
        }
    };
    bool tryPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
    const clap_output_events_t outEvents{nullptr, tryPush};

    struct Note
    {
        double at, len;
        int key;
        double vel;
    };
    struct Auto
    {
        double from, to;
        clap_id param;
        double v0, v1; // a ramp
    };

    clap_id paramByName(const clap_plugin_t *p, const clap_plugin_params_t *pp, const char *name)
    {
        for (uint32_t i = 0; i < pp->count(p); ++i)
        {
            clap_param_info_t info{};
            if (pp->get_info(p, i, &info) && !std::strcmp(info.name, name))
                return info.id;
        }
        return CLAP_INVALID_ID;
    }

    void writeWav(const std::string &path, const std::vector<float> &l, const std::vector<float> &r)
    {
        FILE *f = std::fopen(path.c_str(), "wb");
        if (!f)
            return;
        const uint32_t n = static_cast<uint32_t>(l.size()), data = n * 4, rate = 48000;
        auto u32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };
        std::fwrite("RIFF", 1, 4, f), u32(36 + data), std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16), u16(1), u16(2), u32(rate), u32(rate * 4), u16(4), u16(16), std::fwrite("data", 1, 4, f), u32(data);
        for (uint32_t i = 0; i < n; ++i)
            for (float v : {l[i], r[i]})
            {
                const int16_t s = static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
                std::fwrite(&s, 2, 1, f);
            }
        std::fclose(f);
    }

    // plays notes and ramps through the plugin; returns the peak
    float play(const clap_plugin_t *p, double seconds, const std::vector<Note> &notes, const std::vector<Auto> &autos, std::vector<float> &L, std::vector<float> &R)
    {
        const uint64_t total = static_cast<uint64_t>(seconds * RATE);
        std::vector<float> l(BLOCK), r(BLOCK);
        float *chans[2] = {l.data(), r.data()};
        float peak = 0;
        for (uint64_t at = 0; at < total; at += BLOCK)
        {
            Events in;
            for (const Note &n : notes)
                for (int edge = 0; edge < 2; ++edge)
                {
                    const uint64_t t = static_cast<uint64_t>((edge ? n.at + n.len : n.at) * RATE);
                    if (t < at || t >= at + BLOCK)
                        continue;
                    clap_event_note_t e{};
                    e.header = {sizeof e, static_cast<uint32_t>(t - at), CLAP_CORE_EVENT_SPACE_ID, static_cast<uint16_t>(edge ? CLAP_EVENT_NOTE_OFF : CLAP_EVENT_NOTE_ON), 0};
                    e.note_id = -1, e.port_index = 0, e.channel = 0, e.key = static_cast<int16_t>(n.key), e.velocity = edge ? 0 : n.vel;
                    in.add(e);
                }
            for (const Auto &a : autos) // one value a block, at its start (a DAW would send more)
            {
                const double t = static_cast<double>(at) / RATE;
                if (t < a.from || t > a.to)
                    continue;
                clap_event_param_value_t e{};
                e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
                e.param_id = a.param, e.cookie = nullptr, e.note_id = -1, e.port_index = -1, e.channel = -1, e.key = -1;
                e.value = a.v0 + (a.v1 - a.v0) * (t - a.from) / (a.to - a.from);
                in.add(e);
            }
            std::stable_sort(in.ev.begin(), in.ev.end(), [](const auto &x, const auto &y)
                             { return reinterpret_cast<const clap_event_header_t *>(x.data())->time < reinterpret_cast<const clap_event_header_t *>(y.data())->time; });
            clap_audio_buffer_t out{};
            out.data32 = chans, out.channel_count = 2;
            clap_process_t pr{};
            pr.steady_time = static_cast<int64_t>(at), pr.frames_count = BLOCK, pr.audio_outputs = &out, pr.audio_outputs_count = 1;
            pr.in_events = &in.list, pr.out_events = &outEvents;
            p->process(p, &pr);
            for (uint32_t i = 0; i < BLOCK && L.size() < total; ++i)
            {
                L.push_back(l[i]), R.push_back(r[i]);
                peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
            }
        }
        return peak;
    }

    struct Buffer
    {
        std::string data;
        size_t at = 0;
    };
    int64_t writeStream(const clap_ostream_t *s, const void *b, uint64_t n)
    {
        static_cast<Buffer *>(s->ctx)->data.append(static_cast<const char *>(b), n);
        return static_cast<int64_t>(n);
    }
    int64_t readStream(const clap_istream_t *s, void *b, uint64_t n)
    {
        auto *buf = static_cast<Buffer *>(s->ctx);
        const uint64_t k = std::min<uint64_t>(n, buf->data.size() - buf->at);
        std::memcpy(b, buf->data.data() + buf->at, k);
        buf->at += k;
        return static_cast<int64_t>(k);
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: clap_render <plugin.clap> <out folder>\n");
        return 2;
    }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib)
    {
        std::fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    const auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(lib, "clap_entry"));
    if (!entry || !entry->init(argv[1]))
        return 1;
    const auto *factory = static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    std::printf("%s %s by %s: %s\n", desc->name, desc->version, desc->vendor, desc->description);
    const clap_plugin_t *p = factory->create_plugin(factory, &host, desc->id);
    if (!p || !p->init(p))
        return 1;
    const auto *pp = static_cast<const clap_plugin_params_t *>(p->get_extension(p, CLAP_EXT_PARAMS));
    const auto *st = static_cast<const clap_plugin_state_t *>(p->get_extension(p, CLAP_EXT_STATE));
    std::printf("%u parameters\n", pp->count(p));
    // what to automate and which program to try: $CLAP_RENDER_PARAM, $CLAP_RENDER_PROGRAM, $CLAP_RENDER_SHOW (a value to print)
    const char *autoName = getenv("CLAP_RENDER_PARAM") ? getenv("CLAP_RENDER_PARAM") : "OP2_POS";
    const char *programName = getenv("CLAP_RENDER_PROGRAM") ? getenv("CLAP_RENDER_PROGRAM") : "Ring Drone";
    const char *showName = getenv("CLAP_RENDER_SHOW") ? getenv("CLAP_RENDER_SHOW") : "INDEX";
    const clap_id program = paramByName(p, pp, "PROGRAM"), pos2 = paramByName(p, pp, autoName), index = paramByName(p, pp, showName);
    char text[64];
    double v = 0;
    pp->get_value(p, program, &v), pp->value_to_text(p, program, v, text, sizeof text);
    std::printf("program %s, %s %u, %s %u\n", text, autoName, pos2, showName, index);
    p->activate(p, RATE, 1, BLOCK);
    p->start_processing(p);
    const std::string dir = argv[2];

    // 1: the default program, a phrase, OP2's position swept by automation (what a DAW lane would do)
    std::vector<Note> phrase;
    const int keys[] = {48, 51, 55, 58, 60, 58, 55, 51};
    for (int i = 0; i < 8; ++i)
        phrase.push_back({i * 0.5, 0.42, keys[i], 0.85});
    std::vector<float> L, R;
    float peak = play(p, 5.0, phrase, pos2 == CLAP_INVALID_ID ? std::vector<Auto>{} : std::vector<Auto>{{0.0, 4.0, pos2, 0.0, 1.0}}, L, R);
    writeWav(dir + "/clap_1_automation.wav", L, R);
    std::printf("1 automation: peak %.1f dBFS\n", 20 * std::log10(static_cast<double>(std::max(1e-9f, peak))));

    // 2: PROGRAM to "Ring Drone" (as a DAW's preset list would), a chord
    uint32_t ring = 0;
    for (uint32_t k = 0; k < 20; ++k)
        if (pp->value_to_text(p, program, k, text, sizeof text) && !std::strcmp(text, programName))
            ring = k;
    L.clear(), R.clear();
    peak = play(p, 7.0, {{0.1, 5.0, 33, 0.8}, {0.1, 5.0, 40, 0.8}}, {{0.0, 0.01, program, static_cast<double>(ring), static_cast<double>(ring)}}, L, R);
    writeWav(dir + "/clap_2_program.wav", L, R);
    pp->get_value(p, index, &v), pp->value_to_text(p, index, v, text, sizeof text);
    std::printf("2 program %s: peak %.1f dBFS, INDEX now %s, rescan asked: %s\n", programName, 20 * std::log10(static_cast<double>(std::max(1e-9f, peak))), text, callbackRequested ? "yes" : "no");
    p->on_main_thread(p);

    // 3: the state: saved, the program changed, loaded back, the same chord: the drone again
    Buffer saved;
    clap_ostream_t os{&saved, writeStream};
    st->save(p, &os);
    std::printf("3 state: %zu bytes of JSON\n", saved.data.size());
    L.clear(), R.clear();
    play(p, 0.1, {}, {{0.0, 0.01, program, 0, 0}}, L, R); // to Init
    clap_istream_t is{&saved, readStream};
    const bool loaded = st->load(p, &is);
    L.clear(), R.clear();
    peak = play(p, 4.0, {{0.1, 3.0, 33, 0.8}, {0.1, 3.0, 40, 0.8}}, {}, L, R);
    writeWav(dir + "/clap_3_state_reloaded.wav", L, R);
    pp->get_value(p, index, &v), pp->value_to_text(p, index, v, text, sizeof text);
    std::printf("   loaded %s, INDEX %s, peak %.1f dBFS\n", loaded ? "yes" : "no", text, 20 * std::log10(static_cast<double>(std::max(1e-9f, peak))));

    // 4: the editor (clap.gui, floating): shown for a few seconds while a chord plays and OP2's position moves
    if (const auto *gui = static_cast<const clap_plugin_gui_t *>(p->get_extension(p, CLAP_EXT_GUI)); gui && getenv("CLAP_RENDER_GUI"))
    {
        const char *api = nullptr;
        bool floating = false;
        gui->get_preferred_api(p, &api, &floating);
        const bool made = gui->create(p, api, true) && gui->show(p);
        std::printf("4 editor (%s, floating): %s\n", api ? api : "?", made ? "shown" : "NOT shown");
        L.clear(), R.clear();
        for (int k = 0; k < 6 && made; ++k) // in real time-ish: the panel draws what it hears
        {
            play(p, 1.0, {{0.0, 0.95, 45, 0.8}, {0.0, 0.95, 52, 0.8}, {0.0, 0.95, 57, 0.8}},
                 pos2 == CLAP_INVALID_ID ? std::vector<Auto>{} : std::vector<Auto>{{0.0, 1.0, pos2, k / 6.0, (k + 1) / 6.0}}, L, R);
            usleep(1000000);
        }
        if (made)
        {
            gui->hide(p);
            gui->destroy(p);
        }
    }
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    entry->deinit();
    dlclose(lib);
    return 0;
}
