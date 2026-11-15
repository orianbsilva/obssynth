#pragma once
// What a plugin's editor did, for finding why it does not show: lines appended to <temp>/<name>.log (Windows:
// %TEMP%\ObsSynthCT1.log; elsewhere $TMPDIR or /tmp). The plugin and its panel write to the same file.
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

namespace synth::editorlog
{
    inline std::string path(const char *name)
    {
#ifdef _WIN32
        const char *t = std::getenv("TEMP");
        return std::string(t && *t ? t : ".") + "\\" + name + ".log";
#else
        const char *t = std::getenv("TMPDIR");
        return std::string(t && *t ? t : "/tmp") + "/" + name + ".log";
#endif
    }

    inline void say(const char *name, const char *who, const char *fmt, ...)
    {
        std::FILE *f = std::fopen(path(name).c_str(), "a");
        if (!f)
            return;
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
        char when[32];
        std::strftime(when, sizeof when, "%H:%M:%S", std::localtime(&t));
        std::fprintf(f, "%s.%03d [%s] ", when, ms, who);
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(f, fmt, ap);
        va_end(ap);
        std::fputc('\n', f);
        std::fclose(f);
    }
} // namespace synth::editorlog
