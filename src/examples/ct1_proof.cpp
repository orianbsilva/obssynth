// ct1_proof: ObsSynthCT1's factory presets (products/obs_ct1/obs_synth_ct1.hpp), each played a phrase made for it
// and written as a 48 kHz stereo WAV, to hear the engine without the panel.
// usage: ct1_proof [pcm folder] [out folder]
#include "../products/obs_ct1/obs_synth_ct1.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace synth::ct1;

namespace
{
    constexpr float SR = 48000.0f;

    struct Note
    {
        float at, len;
        int note;
        float vel;
    };

    bool writeWav(const std::string &path, const std::vector<float> &lr)
    {
        FILE *f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;
        const uint32_t frames = static_cast<uint32_t>(lr.size() / 2), data = frames * 4, rate = 48000;
        auto u32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };
        std::fwrite("RIFF", 1, 4, f);
        u32(36 + data);
        std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16), u16(1), u16(2), u32(rate), u32(rate * 4), u16(4), u16(16);
        std::fwrite("data", 1, 4, f);
        u32(data);
        for (float v : lr)
        {
            const int16_t s = static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
            std::fwrite(&s, 2, 1, f);
        }
        std::fclose(f);
        return true;
    }

    std::vector<Note> chord(float at, float len, std::initializer_list<int> ns, float vel = 0.75f)
    {
        std::vector<Note> v;
        for (int n : ns)
            v.push_back({at, len, n, vel});
        return v;
    }
    std::vector<Note> line(float step, float len, std::initializer_list<int> ns, float vel = 0.8f)
    {
        std::vector<Note> v;
        float t = 0;
        for (int n : ns)
            v.push_back({t, len, n, vel}), t += step;
        return v;
    }
    std::vector<Note> join(std::initializer_list<std::vector<Note>> parts)
    {
        std::vector<Note> v;
        for (const auto &p : parts)
            v.insert(v.end(), p.begin(), p.end());
        return v;
    }

    // each preset's phrase and its length
    std::pair<std::vector<Note>, float> phrase(const std::string &name)
    {
        if (name == "Chain Bass")
            return {line(0.1875f, 0.15f, {36, 36, 48, 36, 39, 36, 43, 41, 36, 36, 48, 36, 34, 34, 46, 38}), 3.6f};
        if (name == "Growl Bass")
            return {{{0.0f, 1.4f, 28, 0.9f}, {1.5f, 0.6f, 31, 0.9f}, {2.2f, 0.6f, 26, 0.9f}, {3.0f, 1.6f, 28, 0.9f}}, 5.2f};
        if (name == "Fixed Metal")
            return {line(0.1875f, 0.1f, {48, 60, 55, 48, 67, 60, 52, 72, 48, 60, 55, 48, 64, 60, 79, 84}), 3.8f};
        if (name == "Table Bell")
            return {line(0.6f, 0.4f, {72, 79, 76, 84, 74, 79, 71, 76}, 0.75f), 8.5f};
        if (name == "Harmonic Scan Lead")
            return {line(0.5f, 0.46f, {57, 60, 64, 67, 69, 67, 64, 62, 60, 57}), 6.5f};
        if (name == "Audio Rate Scan")
            return {{{0.0f, 1.8f, 45, 0.8f}, {2.0f, 1.8f, 57, 0.8f}, {4.0f, 1.8f, 52, 0.8f}}, 6.6f};
        if (name == "Table EP")
            return {join({chord(0.0f, 1.3f, {52, 59, 62, 67}), chord(1.5f, 1.3f, {50, 57, 60, 65}, 0.6f), chord(3.0f, 2.2f, {48, 55, 59, 64}, 0.85f)}), 7.0f};
        if (name == "Glass Organ")
            return {join({chord(0.0f, 2.2f, {48, 55, 60, 64}), chord(2.4f, 2.2f, {45, 52, 57, 60}), chord(4.8f, 2.6f, {41, 48, 53, 57, 60})}), 9.0f};
        if (name == "Two Tables Pad" || name == "Five Ops Pad")
            return {join({chord(0.0f, 4.0f, {45, 52, 57, 60, 64}, 0.65f), chord(4.2f, 4.0f, {41, 48, 53, 57, 64}, 0.65f)}), 11.0f};
        if (name == "Cross Feedback" || name == "Ring Drone")
            return {chord(0.0f, 8.0f, {33, 40}, 0.8f), 10.5f};
        return {line(0.5f, 0.42f, {48, 51, 55, 58, 60, 58, 55, 51}, 0.85f), 4.8f};
    }
} // namespace

int main(int argc, char **argv)
{
    const std::string pcm = argc > 1 ? argv[1] : "pcm", dir = argc > 2 ? argv[2] : ".";
    const auto banks = loadBanks(pcm);
    std::printf("%zu banks:", banks.size());
    for (const auto &b : banks)
        std::printf(" %s (%d)", b->name().c_str(), b->frames());
    std::printf("\n");
    int k = 0;
    for (const Preset &pr : factoryPresets())
    {
        Engine e(SR, banks);
        e.setAll(resolve(pr, banks));
        const auto [notes, seconds] = phrase(pr.name);
        const int block = 256, total = static_cast<int>(seconds * SR);
        std::vector<float> out(static_cast<size_t>(total) * 2), buf(block * 2);
        for (int done = 0; done < total; done += block)
        {
            const float t0 = static_cast<float>(done) / SR, t1 = static_cast<float>(done + block) / SR;
            for (const Note &n : notes)
            {
                if (n.at >= t0 && n.at < t1)
                    e.noteOn(n.note, n.vel);
                if (n.at + n.len >= t0 && n.at + n.len < t1)
                    e.noteOff(n.note);
            }
            const int m = std::min(block, total - done);
            e.render(buf.data(), m);
            std::copy(buf.begin(), buf.begin() + m * 2, out.begin() + done * 2);
        }
        float peak = 0;
        for (float v : out)
            peak = std::max(peak, std::fabs(v));
        std::string file = pr.name;
        for (auto &c : file)
            c = c == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        char path[512];
        std::snprintf(path, sizeof path, "%s/ct1_%02d_%s.wav", dir.c_str(), k++, file.c_str());
        writeWav(path, out);
        std::printf("%-26s %5.1f s  peak %5.1f dBFS\n", pr.name.c_str(), static_cast<double>(seconds), 20 * std::log10(std::max(1e-9, static_cast<double>(peak))));
    }
    return 0;
}
