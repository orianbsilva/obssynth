#pragma once
// Where a recording named in a preset or a kit is read from: a path as it is; a name from the user's samples folder,
// else from the folders shipped with the program (its pcm/: beside a plugin, among a VST3's resources, beside a panel),
// which the program names once at its start (addSampleDir). Off the audio thread (the filesystem).
#include <filesystem>
#include <string>
#include <vector>

namespace synth
{
    inline std::vector<std::string> &sampleDirs() // the shipped folders, in order (named at the start, then only read)
    {
        static std::vector<std::string> dirs;
        return dirs;
    }

    inline void addSampleDir(const std::string &dir)
    {
        for (const auto &d : sampleDirs())
            if (d == dir)
                return;
        sampleDirs().push_back(dir);
    }

    // a panel's: its pcm/ (installed, or in a plugin's folder), and the source tree's from build/src/app (Release: one
    // folder deeper)
    inline void addSampleDirsBeside(const std::string &programDir)
    {
        for (const char *up : {"pcm", "../../../pcm", "../../../../pcm"})
        {
            const std::filesystem::path d = (std::filesystem::path(programDir) / up).lexically_normal();
            std::error_code ec;
            if (std::filesystem::is_directory(d, ec))
                addSampleDir(d.string());
        }
    }

    inline bool isSamplePath(const std::string &name)
    {
        return name.find('/') != std::string::npos || name.find('\\') != std::string::npos;
    }

    // name: a file name or a path; userDir: the user's samples folder. A name found nowhere: the user's folder's path
    inline std::string findSample(const std::string &name, const std::string &userDir)
    {
        if (name.empty() || isSamplePath(name))
            return name;
        std::error_code ec;
        const std::string mine = userDir + "/" + name;
        if (std::filesystem::exists(mine, ec))
            return mine;
        for (const auto &d : sampleDirs())
        {
            const std::string f = d + "/" + name;
            if (std::filesystem::exists(f, ec))
                return f;
        }
        return mine;
    }

    // the recordings a user can pick: the user's folder's .wav files, then the shipped ones (vcsl-*.wav: the recordings
    // among the wavetables), each name once
    inline std::vector<std::string> listSampleNames(const std::string &userDir)
    {
        std::vector<std::string> v;
        auto add = [&v](const std::string &dir, bool shipped) {
            std::error_code ec;
            for (const auto &f : std::filesystem::directory_iterator(dir, ec))
            {
                std::string ext = f.path().extension().string();
                for (auto &c : ext)
                    c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                const std::string n = f.path().filename().string();
                if (ext != ".wav" || (shipped && n.rfind("vcsl-", 0) != 0))
                    continue;
                bool seen = false;
                for (const auto &x : v)
                    seen = seen || x == n;
                if (!seen)
                    v.push_back(n);
            }
        };
        add(userDir, false);
        for (const auto &d : sampleDirs())
            add(d, true);
        return v;
    }
} // namespace synth
