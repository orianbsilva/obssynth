#pragma once
// The person's kits: JSON files in ~/.local/share/obsdrums/kits, every pad (engine, circuit, knobs, level, pan, FX,
// note, choke, recording), the master and the routes. The factory's are in obs_drum_kits.hpp.
#include "../../products/obs_drum/obs_drum_kits.hpp"
#include "../../vendor/json.hpp"
#include "../common/data_home.hpp"
#include "../common/sample_find.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace synth::drums
{
    using json = nlohmann::json;

    inline std::string kitsDir()
    {
        return dataHome() + "/obsdrums/kits";
    }
    inline std::string samplesDir() // shared with the synth
    {
        return dataHome() + "/obssynth4d/samples";
    }
    inline std::string samplePathOf(const std::string &name) // the person's folder, else the shipped pcm/
    {
        return findSample(name, samplesDir());
    }

    inline json kitToJson(const DrumKit &k)
    {
        json j;
        j["name"] = k.name;
        json pads = json::array();
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const PadSettings &s = k.pads[static_cast<size_t>(i)];
            pads.push_back({{"engine", drumEngineName(s.engine)}, {"circuit", static_cast<int>(s.circuit)}, {"tune", s.p.tune}, {"decay", s.p.decay},
                            {"a", s.p.a}, {"b", s.p.b}, {"bent", s.p.bent}, {"level", s.level}, {"pan", s.pan}, {"fx", s.fx}, {"note", s.note},
                            {"choke", s.choke}, {"grainPos", s.grainPos}, {"sample", k.samples[static_cast<size_t>(i)]}});
        }
        j["pads"] = pads;
        const DrumMaster &m = k.master;
        j["master"] = {{"delayTime", m.delayTime}, {"delayFeedback", m.delayFeedback}, {"delayMix", m.delayMix}, {"reverbSize", m.reverbSize},
                       {"reverbDamp", m.reverbDamp}, {"reverbMix", m.reverbMix}, {"drive", m.drive}, {"glue", m.glue}, {"tubeMix", m.tubeMix},
                       {"volume", m.volume}};
        json routes = json::array();
        for (const DrumRoute &r : k.routes)
            routes.push_back({{"src", static_cast<int>(r.src)}, {"pad", static_cast<int>(r.pad)}, {"param", r.param}, {"amount", r.amount}});
        j["routes"] = routes;
        return j;
    }

    inline bool kitFromJson(const json &j, DrumKit &k)
    {
        try
        {
            k = DrumKit{};
            k.name = j.value("name", "Untitled");
            const DrumKit base = factoryDrumKits().front(); // what a file does not say
            k.pads = base.pads;
            if (j.contains("pads"))
                for (int i = 0; i < NUM_DRUM_PADS && i < static_cast<int>(j["pads"].size()); ++i)
                {
                    const json &p = j["pads"][static_cast<size_t>(i)];
                    PadSettings &s = k.pads[static_cast<size_t>(i)];
                    const std::string e = p.value("engine", "CIRCUIT");
                    for (int x = 0; x < NUM_DRUM_ENGINES; ++x)
                        if (e == drumEngineName(static_cast<DrumEngine>(x)))
                            s.engine = static_cast<DrumEngine>(x);
                    s.circuit = static_cast<DrumKind>(std::clamp(p.value("circuit", static_cast<int>(s.circuit)), 0, NUM_DRUM_KINDS - 1));
                    s.p.tune = p.value("tune", s.p.tune);
                    s.p.decay = p.value("decay", s.p.decay);
                    s.p.a = p.value("a", s.p.a);
                    s.p.b = p.value("b", s.p.b);
                    s.p.bent = p.value("bent", false);
                    s.level = p.value("level", s.level);
                    s.pan = p.value("pan", s.pan);
                    s.fx = p.value("fx", s.fx);
                    s.note = p.value("note", s.note);
                    s.choke = p.value("choke", s.choke);
                    s.grainPos = p.value("grainPos", s.grainPos);
                    k.samples[static_cast<size_t>(i)] = p.value("sample", std::string());
                }
            if (j.contains("master"))
            {
                const json &m = j["master"];
                DrumMaster &d = k.master;
                d.delayTime = m.value("delayTime", d.delayTime);
                d.delayFeedback = m.value("delayFeedback", d.delayFeedback);
                d.delayMix = m.value("delayMix", d.delayMix);
                d.reverbSize = m.value("reverbSize", d.reverbSize);
                d.reverbDamp = m.value("reverbDamp", d.reverbDamp);
                d.reverbMix = m.value("reverbMix", d.reverbMix);
                d.drive = m.value("drive", d.drive);
                d.glue = m.value("glue", d.glue);
                d.tubeMix = m.value("tubeMix", d.tubeMix);
                d.volume = m.value("volume", d.volume);
            }
            if (j.contains("routes"))
                for (const json &r : j["routes"])
                    k.routes.push_back({static_cast<ObsDrumSrc>(r.value("src", 0)), static_cast<DrumKind>(r.value("pad", 0)), r.value("param", 0), r.value("amount", 0.0f)});
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    struct KitEntry
    {
        std::string name, path; // path empty: the factory's (index factory)
        int factory = -1;
    };

    inline std::vector<KitEntry> listKits()
    {
        std::vector<KitEntry> v;
        const auto f = factoryDrumKits();
        for (size_t i = 0; i < f.size(); ++i)
            v.push_back({f[i].name, "", static_cast<int>(i)});
        std::vector<KitEntry> mine;
        std::error_code ec;
        for (const auto &e : std::filesystem::directory_iterator(kitsDir(), ec))
            if (e.path().extension() == ".json")
            {
                std::ifstream in(e.path());
                DrumKit k;
                json j;
                try
                {
                    in >> j;
                }
                catch (...)
                {
                    continue;
                }
                if (kitFromJson(j, k))
                    mine.push_back({k.name, e.path().string(), -1});
            }
        std::sort(mine.begin(), mine.end(), [](const KitEntry &a, const KitEntry &b) { return a.name < b.name; });
        v.insert(v.end(), mine.begin(), mine.end());
        return v;
    }

    inline bool loadKit(const KitEntry &e, DrumKit &k)
    {
        if (e.factory >= 0)
        {
            k = factoryDrumKits()[static_cast<size_t>(e.factory)];
            return true;
        }
        std::ifstream in(e.path);
        json j;
        try
        {
            in >> j;
        }
        catch (...)
        {
            return false;
        }
        return kitFromJson(j, k);
    }

    inline bool saveKit(const DrumKit &k, std::string &path)
    {
        std::string slug;
        for (char c : k.name)
            slug += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : (slug.empty() || slug.back() == '-' ? '\0' : '-');
        slug.erase(std::remove(slug.begin(), slug.end(), '\0'), slug.end());
        while (!slug.empty() && slug.back() == '-')
            slug.pop_back();
        std::error_code ec;
        std::filesystem::create_directories(kitsDir(), ec);
        path = kitsDir() + "/" + (slug.empty() ? std::string("kit") : slug) + ".json";
        std::ofstream out(path);
        if (!out)
            return false;
        out << kitToJson(k).dump(2);
        return static_cast<bool>(out);
    }
} // namespace synth::drums
