#pragma once
// The panels' colours, as the person chose them in SETUP (settings.json, "look": {"palette": ...}): "" the
// system's (Carbonian's palette file, followed when it changes; where there is none, the panel's own neutral
// look), or one of Carbonian's palettes by name, carried inside (builtin_palettes.hpp): a plugin in a DAW on
// Windows has no Carbonian to follow.
#include "builtin_palettes.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace panelkit
{
    inline const char *kSystemPalette = "/var/lib/carbonian/palette/palette";

    inline void parsePalette(std::istream &in, std::map<std::string, std::string> &p)
    {
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos)
                continue;
            p[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    // the chosen palette's keys (background, surface, accent, ...); empty: the panel's own look
    inline std::map<std::string, std::string> paletteValues(const std::string &choice)
    {
        std::map<std::string, std::string> p;
        if (choice.empty())
        {
            std::ifstream f(kSystemPalette);
            if (f)
                parsePalette(f, p);
            return p;
        }
        for (const BuiltinPalette &b : kBuiltinPalettes)
            if (choice == b.name)
            {
                std::istringstream s(b.kv);
                parsePalette(s, p);
            }
        return p;
    }

    inline bool hasSystemPalette()
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(kSystemPalette, ec);
    }

    // the choices in order: "" (the system's), then the palettes by name
    inline std::vector<std::string> paletteChoices()
    {
        std::vector<std::string> v{""};
        for (const BuiltinPalette &b : kBuiltinPalettes)
            v.push_back(b.name);
        return v;
    }

    // what SETUP shows for a choice
    inline std::string paletteLabel(const std::string &choice)
    {
        if (choice.empty())
        {
            if (!hasSystemPalette())
                return "NEUTRAL (NO CARBONIAN HERE)";
            const auto p = paletteValues("");
            const auto it = p.find("label");
            return "THE SYSTEM'S  ·  " + (it != p.end() ? it->second : std::string("CARBONIAN"));
        }
        for (const BuiltinPalette &b : kBuiltinPalettes)
            if (choice == b.name)
                return b.label;
        return choice;
    }
} // namespace panelkit
