#pragma once
// The CC overlay: a controller's knobs, faders and buttons laid over a panel's controls. A map per controller
// (the factory's here, the person's in ~/.local/share/<product>/controllers/*.json, theirs winning by name) says
// which CC moves which control; the control is named by an id the panel resolves ("pad.sel.TUNE", "osc.2.LEVEL",
// "@bank+"), so a map can follow the panel's selection and banks. The map in use is the first whose `match` is in
// the name of a MIDI client present (/proc/asound/seq/clients; on Windows the MIDI input devices), else the first without one.
//   Absolute controls pick up: after the panel moves a control (a preset, a bank, the mouse) the knob does nothing
//   until it reaches the value or crosses it, so nothing jumps.
//   Learn: the panel arms a control (overlay shown, a click); the next CC is bound to it and the map saved as theirs.
//   Notes: a map may give the controller's pad notes to the panel's pads (a drum machine), for that client alone.
// The panel's thread only: the MIDI thread hands the CCs over (Sound's queue).
#include "../../vendor/json.hpp"
#include "data_home.hpp"
#ifdef _WIN32
#include "../../lib/utils/platform_midi_reader.hpp"
#endif

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace ccov
{
    using json = nlohmann::json;

    struct Binding
    {
        int cc = -1;
        int channel = -1; // 0..15, -1 any
        std::string target;
    };

    struct Map
    {
        std::string name, match; // match: a piece of the controller's MIDI client name ("" any)
        std::vector<Binding> cc;
        std::array<int, 128> notes; // a note -> the panel's pad, -1 none
        std::string path;           // the person's file, "" the factory's
        Map() { notes.fill(-1); }
    };

    // what a target is to the panel: a value with its range, or a press (a button, an action). With set, the
    // value goes there instead (v, if any, only tells where the panel has it, for the pick-up): a mod source the
    // panel has no knob for, several knobs at once.
    struct Target
    {
        float *v = nullptr;
        float lo = 0, hi = 1;
        std::function<void()> press;
        std::function<void(float)> set;
        std::string label; // shown when the controller moves it
        explicit operator bool() const { return v || press || set; }
    };

    inline std::string dataDir(const std::string &product)
    {
        return synth::dataHome() + "/" + product + "/controllers";
    }

    inline json toJson(const Map &m)
    {
        json j;
        j["name"] = m.name;
        j["match"] = m.match;
        json cc = json::array();
        for (const Binding &b : m.cc)
        {
            json e = {{"cc", b.cc}, {"target", b.target}};
            if (b.channel >= 0)
                e["channel"] = b.channel + 1;
            cc.push_back(e);
        }
        j["cc"] = cc;
        json notes = json::object();
        for (int n = 0; n < 128; ++n)
            if (m.notes[static_cast<size_t>(n)] >= 0)
                notes[std::to_string(n)] = m.notes[static_cast<size_t>(n)];
        if (!notes.empty())
            j["notes"] = notes;
        return j;
    }

    inline bool fromJson(const json &j, Map &m)
    {
        try
        {
            m = Map{};
            m.name = j.value("name", "Controller");
            m.match = j.value("match", "");
            if (j.contains("cc"))
                for (const json &e : j["cc"])
                    m.cc.push_back({std::clamp(e.value("cc", 0), 0, 127), e.value("channel", 0) - 1, e.value("target", "")});
            if (j.contains("notes"))
                for (auto it = j["notes"].begin(); it != j["notes"].end(); ++it)
                {
                    const int n = std::atoi(it.key().c_str());
                    if (n >= 0 && n < 128)
                        m.notes[static_cast<size_t>(n)] = it.value().get<int>();
                }
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    // the MIDI clients present: number and name
    struct Client
    {
        int id;
        std::string name;
    };
    inline std::vector<Client> seqClients()
    {
        std::vector<Client> v;
#ifdef _WIN32 // the MIDI input devices, numbered as platform_io.hpp opens them
        for (int i = 0; i < synth::MidiInterface::getDeviceCount(); ++i)
            v.push_back({i, synth::MidiInterface::getDeviceName(i)});
#else
        std::ifstream in("/proc/asound/seq/clients");
        std::string line;
        while (std::getline(in, line))
            if (line.rfind("Client ", 0) == 0)
            {
                const auto q0 = line.find('"'), q1 = line.find('"', q0 + 1);
                if (q0 != std::string::npos && q1 != std::string::npos)
                    v.push_back({std::atoi(line.c_str() + 7), line.substr(q0 + 1, q1 - q0 - 1)});
            }
#endif
        return v;
    }

    class Overlay
    {
    public:
        std::vector<Map> maps;
        int active = 0;
        int client = -1;   // the controller's MIDI client, when the map matched one (its notes go to the pads)
        bool shown = false; // the badges on the panel
        std::string arm;    // a target waiting for its CC (learn)
        std::string moved;  // the label of what the controller moved last, and when (the panel shows it a moment)
        uint64_t movedAt = 0;

        Overlay() { retarget(); }

        // the factory's maps, then the person's (one of theirs with a factory name replaces it); then the one in use
        void load(const std::string &product, std::vector<Map> factory)
        {
            product_ = product;
            maps = std::move(factory);
            std::error_code ec;
            std::vector<Map> mine;
            for (const auto &e : std::filesystem::directory_iterator(dataDir(product), ec))
                if (e.path().extension() == ".json")
                {
                    std::ifstream in(e.path());
                    json j;
                    try
                    {
                        in >> j;
                    }
                    catch (...)
                    {
                        continue;
                    }
                    Map m;
                    if (fromJson(j, m))
                    {
                        m.path = e.path().string();
                        mine.push_back(m);
                    }
                }
            for (Map &m : mine)
            {
                auto same = std::find_if(maps.begin(), maps.end(), [&](const Map &f) { return f.name == m.name; });
                if (same != maps.end())
                    *same = m;
                else
                    maps.push_back(m);
            }
            detect();
            retarget();
        }

        // the map of a controller present, else the first that matches anything
        void detect()
        {
            const auto clients = seqClients();
            const int was = active, wasClient = client;
            active = -1;
            client = -1;
            for (size_t i = 0; i < maps.size() && active < 0; ++i)
                if (!maps[i].match.empty())
                    for (const Client &c : clients)
                        if (c.name.find(maps[i].match) != std::string::npos)
                        {
                            active = static_cast<int>(i);
                            client = c.id;
                            break;
                        }
            for (size_t i = 0; i < maps.size() && active < 0; ++i)
                if (maps[i].match.empty())
                    active = static_cast<int>(i);
            if (active < 0)
                active = 0;
            if (active != was || client != wasClient) // the same map: the knobs keep what they picked up
                retarget();
        }

        const Map *map() const { return maps.empty() ? nullptr : &maps[static_cast<size_t>(active)]; }
        std::string name() const { return maps.empty() ? std::string("NONE") : maps[static_cast<size_t>(active)].name; }

        // after the panel moved what the controls point at (a bank, a preset): every absolute control picks up again
        void retarget()
        {
            caught_.fill(false);
            prev_.fill(-1.0f);
        }

        // a CC from the controller: learns it, or moves its targets. True when it did something.
        bool onCc(int channel, int cc, float n, const std::function<Target(const std::string &)> &resolve)
        {
            if (maps.empty() || cc < 0 || cc > 127)
                return false;
            Map &m = maps[static_cast<size_t>(active)];
            if (!arm.empty())
            {
                learn(m, cc, arm);
                prev_[static_cast<size_t>(cc)] = n; // a button's release, next, is not a press
                const Target t = resolve(arm);
                moved = "CC " + std::to_string(cc) + " → " + (t.label.empty() ? arm : t.label);
                arm.clear();
                retarget();
                return true;
            }
            bool did = false;
            for (const Binding &b : m.cc)
            {
                if (b.cc != cc || (b.channel >= 0 && b.channel != channel))
                    continue;
                const Target t = resolve(b.target);
                if (t.press)
                {
                    if (n >= 0.5f && prev_[static_cast<size_t>(cc)] < 0.5f)
                        t.press();
                    did = true;
                }
                else if (t.v)
                {
                    const float cur = std::clamp((*t.v - t.lo) / (t.hi - t.lo), 0.0f, 1.0f);
                    const float p = prev_[static_cast<size_t>(cc)];
                    bool &ok = caught_[static_cast<size_t>(cc)];
                    if (!ok && (std::fabs(n - cur) <= 0.025f || (p >= 0 && (p - cur) * (n - cur) <= 0)))
                        ok = true;
                    if (ok && t.set)
                        t.set(t.lo + (t.hi - t.lo) * n);
                    else if (ok)
                        *t.v = t.lo + (t.hi - t.lo) * n;
                    did = true;
                }
                else if (t.set) // nothing on the panel to pick up from: it follows at once
                {
                    caught_[static_cast<size_t>(cc)] = true;
                    t.set(t.lo + (t.hi - t.lo) * n);
                    did = true;
                }
                if (!t.label.empty())
                    moved = t.label;
            }
            prev_[static_cast<size_t>(cc)] = n;
            return did;
        }

        // the CC of the first of these ids that has one (a control's names: "pad.3.TUNE", "pad.sel.TUNE"), -1 none
        int ccOf(const std::vector<std::string> &ids) const
        {
            if (maps.empty())
                return -1;
            for (const std::string &id : ids)
                for (const Binding &b : maps[static_cast<size_t>(active)].cc)
                    if (b.target == id)
                        return b.cc;
            return -1;
        }
        bool caught(int cc) const { return cc >= 0 && cc < 128 && caught_[static_cast<size_t>(cc)]; }

        // forget the CCs of a control, in the map in use (saved as theirs)
        void forget(const std::vector<std::string> &ids)
        {
            if (maps.empty())
                return;
            auto &v = maps[static_cast<size_t>(active)].cc;
            v.erase(std::remove_if(v.begin(), v.end(), [&](const Binding &b) { return std::find(ids.begin(), ids.end(), b.target) != ids.end(); }), v.end());
            save();
        }

        // the map in use, to the person's folder (a factory map so becomes theirs, by the same name)
        bool save()
        {
            if (maps.empty())
                return false;
            Map &m = maps[static_cast<size_t>(active)];
            std::error_code ec;
            std::filesystem::create_directories(dataDir(product_), ec);
            if (m.path.empty())
            {
                std::string slug;
                for (char c : m.name)
                    if (std::isalnum(static_cast<unsigned char>(c)))
                        slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    else if (!slug.empty() && slug.back() != '-')
                        slug += '-';
                while (!slug.empty() && slug.back() == '-')
                    slug.pop_back();
                m.path = dataDir(product_) + "/" + (slug.empty() ? std::string("controller") : slug) + ".json";
            }
            std::ofstream out(m.path);
            if (!out)
                return false;
            out << toJson(m).dump(2);
            return static_cast<bool>(out);
        }

        // the next map, by hand (the overlay's name in the panel)
        void next()
        {
            if (maps.empty())
                return;
            active = (active + 1) % static_cast<int>(maps.size());
            client = -1;
            for (const Client &c : seqClients())
                if (!maps[static_cast<size_t>(active)].match.empty() && c.name.find(maps[static_cast<size_t>(active)].match) != std::string::npos)
                    client = c.id;
            retarget();
        }

    private:
        std::string product_;
        std::array<bool, 128> caught_{};
        std::array<float, 128> prev_{};

        void learn(Map &m, int cc, const std::string &target)
        {
            // one control per CC, one CC per control
            m.cc.erase(std::remove_if(m.cc.begin(), m.cc.end(), [&](const Binding &b) { return b.cc == cc || b.target == target; }), m.cc.end());
            m.cc.push_back({cc, -1, target});
            save();
        }
    };
} // namespace ccov
