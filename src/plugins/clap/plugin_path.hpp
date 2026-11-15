#pragma once
// Where this plugin's file is (a .clap, or the .vst3 that carries it), and the folders its data may be beside:
// <dir>/pcm, <dir>/../pcm (a build), <dir>/../Resources/pcm (a VST3 bundle: Contents/<arch>/X.vst3 and Contents/Resources).
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace synth::clap
{
    inline std::filesystem::path pluginDir()
    {
#if defined(_WIN32)
        HMODULE m = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&pluginDir), &m))
        {
            wchar_t buf[32768];
            const DWORD n = GetModuleFileNameW(m, buf, 32768);
            if (n > 0 && n < 32768)
                return std::filesystem::path(std::wstring(buf, n)).parent_path();
        }
#elif defined(__unix__) || defined(__APPLE__)
        Dl_info info{};
        if (dladdr(reinterpret_cast<void *>(&pluginDir), &info) && info.dli_fname)
            return std::filesystem::path(info.dli_fname).parent_path();
#endif
        return ".";
    }

    // where to look for the wavetable banks: $OBS_PCM_DIR first, then beside the plugin
    inline std::vector<std::filesystem::path> pcmDirs()
    {
        const auto d = pluginDir();
        std::vector<std::filesystem::path> dirs;
        if (const char *e = std::getenv("OBS_PCM_DIR"))
            dirs.emplace_back(e);
        dirs.push_back(d / "pcm");
        dirs.push_back(d / ".." / "Resources" / "pcm"); // a VST3 folder: the banks among its resources
        dirs.push_back(d / ".." / "pcm");
        return dirs;
    }
} // namespace synth::clap
