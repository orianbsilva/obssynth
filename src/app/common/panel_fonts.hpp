#pragma once
// The panels' faces: Inter for the words, JetBrains Mono for the values (the mock-ups'). Looked for next to the
// program (fonts/, to ship them), then where Linux has them, then DejaVu; on Windows, Segoe UI and Consolas.
#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace synth
{
    inline std::vector<std::string> panelFonts(bool bold, bool mono)
    {
        const std::string inter = bold ? "Inter-SemiBold.otf" : "Inter-Medium.otf";
        const std::string jet = bold ? "JetBrainsMono-Bold.ttf" : "JetBrainsMono-Medium.ttf";
        std::vector<std::string> v;
        if (const char *base = SDL_GetBasePath())
            v.push_back(std::string(base) + "fonts/" + (mono ? jet : inter));
#ifdef _WIN32
        const char *win = SDL_getenv("WINDIR");
        const std::string dir = std::string(win && *win ? win : "C:\\Windows") + "\\Fonts\\";
        v.push_back(dir + (mono ? (bold ? "consolab.ttf" : "consola.ttf") : (bold ? "seguisb.ttf" : "segoeui.ttf")));
        v.push_back(dir + (mono ? "consola.ttf" : "segoeui.ttf"));
        v.push_back(dir + "arial.ttf");
#else
        v.push_back(mono ? "/usr/share/fonts/truetype/jetbrains-mono/" + jet : "/usr/share/fonts/opentype/inter/" + inter);
        v.push_back(mono ? "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
                         : bold ? "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
                                : "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
#endif
        return v;
    }
} // namespace synth
