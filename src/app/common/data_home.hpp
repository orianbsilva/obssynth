#pragma once
// Where the products keep what is the person's (presets, kits, samples, controller maps): Linux, $XDG_DATA_HOME
// or ~/.local/share; Windows, %APPDATA%. Each product its folder under it (obssynth4d/, obsdrums/).
#include <cstdlib>
#include <string>

namespace synth
{
    inline std::string dataHome()
    {
#ifdef _WIN32
        const char *app = std::getenv("APPDATA");
        if (app && *app)
            return app;
        const char *profile = std::getenv("USERPROFILE");
        return std::string(profile && *profile ? profile : ".") + "\\AppData\\Roaming";
#else
        const char *data = std::getenv("XDG_DATA_HOME");
        if (data && *data)
            return data;
        const char *home = std::getenv("HOME");
        return std::string(home && *home ? home : ".") + "/.local/share";
#endif
    }
} // namespace synth
