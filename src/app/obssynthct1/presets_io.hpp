#pragma once
// ObsSynthCT1's presets: the factory's (obs_synth_ct1.hpp) and the person's, JSON in <data>/obssynthct1/presets:
// every parameter by name (a file from an older version: what it does not say stays the default), the WT
// operators' banks by name.
#include "../../products/obs_ct1/obs_synth_ct1.hpp"
#include "../../vendor/json.hpp"
#include "../common/data_home.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace synth::ct1app
{
    inline std::string presetsDir() { return dataHome() + "/obssynthct1/presets"; }

    inline nlohmann::json presetToJson(const ct1::Preset &p)
    {
        nlohmann::json j;
        j["name"] = p.name;
        nlohmann::json params = nlohmann::json::object();
        const auto &info = ct1::paramInfo();
        for (size_t i = 0; i < info.size(); ++i)
            params[info[i].name] = p.params[i];
        j["params"] = params;
        j["banks"] = p.bank;
        return j;
    }

    inline bool presetFromJson(const nlohmann::json &j, ct1::Preset &p)
    {
        try
        {
            p.name = j.value("name", "Untitled");
            p.params = ct1::defaultParams();
            if (j.contains("params"))
                for (auto it = j["params"].begin(); it != j["params"].end(); ++it)
                {
                    const int id = ct1::paramId(it.key());
                    if (id >= 0)
                        p.params[static_cast<size_t>(id)] = it.value().get<float>();
                }
            if (j.contains("banks"))
                for (size_t o = 0; o < p.bank.size() && o < j["banks"].size(); ++o)
                    p.bank[o] = j["banks"][o].get<std::string>();
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    struct PresetEntry
    {
        std::string name, path; // path "": the factory's (index factory)
        int factory = -1;
    };

    inline std::vector<PresetEntry> listPresets()
    {
        std::vector<PresetEntry> v;
        const auto f = ct1::factoryPresets();
        for (size_t i = 0; i < f.size(); ++i)
            v.push_back({f[i].name, "", static_cast<int>(i)});
        std::vector<PresetEntry> mine;
        std::error_code ec;
        for (const auto &e : std::filesystem::directory_iterator(presetsDir(), ec))
            if (e.path().extension() == ".json")
            {
                std::ifstream in(e.path());
                nlohmann::json j;
                try
                {
                    in >> j;
                }
                catch (...)
                {
                    continue;
                }
                ct1::Preset p;
                if (presetFromJson(j, p))
                    mine.push_back({p.name, e.path().string(), -1});
            }
        std::sort(mine.begin(), mine.end(), [](const PresetEntry &a, const PresetEntry &b) { return a.name < b.name; });
        v.insert(v.end(), mine.begin(), mine.end());
        return v;
    }

    inline bool loadPreset(const PresetEntry &e, ct1::Preset &p)
    {
        if (e.factory >= 0)
        {
            p = ct1::factoryPresets()[static_cast<size_t>(e.factory)];
            return true;
        }
        std::ifstream in(e.path);
        nlohmann::json j;
        try
        {
            in >> j;
        }
        catch (...)
        {
            return false;
        }
        return presetFromJson(j, p);
    }

    inline bool savePreset(const ct1::Preset &p, std::string &path)
    {
        std::string slug;
        for (char c : p.name)
            if (std::isalnum(static_cast<unsigned char>(c)))
                slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            else if (!slug.empty() && slug.back() != '-')
                slug += '-';
        while (!slug.empty() && slug.back() == '-')
            slug.pop_back();
        std::error_code ec;
        std::filesystem::create_directories(presetsDir(), ec);
        path = presetsDir() + "/" + (slug.empty() ? std::string("preset") : slug) + ".json";
        std::ofstream out(path);
        if (!out)
            return false;
        out << presetToJson(p).dump(2);
        return static_cast<bool>(out);
    }
} // namespace synth::ct1app
