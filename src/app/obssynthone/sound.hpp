#pragma once
// obssynthone's own sound, when the panel stands alone (no --remote): the ObsSynthOne engine, its audio out and MIDI in
// (common/platform_io.hpp; Linux: ObsSynthOne:input), as obssynth4d and obssynthct1 have. The engine's profile is the
// machine's (lib/synth/engine_profile.hpp: lite on a 32-bit ARM, OBS_PROFILE=lite|full). The audio callback is the
// engine's only writer: the UI and the MIDI threads queue commands, applied at the start of each block.
#include <memory>
#include <string>
#include <vector>

namespace synth
{
    namespace io
    {
        struct Settings;
        class AudioOut;
        class MidiIn;
    }

    namespace oneapp
    {
        class Sound
        {
        public:
            Sound();
            ~Sound();
            // values: the parameters, 0..1 (one_params.hpp's ids); bank: the wavetable file ("" none); samples: the
            // recordings the sample engine can play, by index (the panel shows the same list); grain: the granular
            // engine's recording ("" none)
            bool start(const float *values, const std::string &bank, const std::vector<std::string> &samples, const std::string &grain,
                       const io::Settings &settings);

            void setParam(int id, float value); // 0..1
            void noteOn(int note, float velocity);
            void noteOff(int note);
            void allNotesOff();

            float outPeak(int side) const;
            int sounding() const;
            const char *profile() const; // "lite" or "full"
            bool pollCc(int &channel, int &cc, float &value); // the CCs that came in, oldest first (the CC overlay)

            bool reopenAudio(const io::Settings &settings);
            const io::AudioOut &audioOut() const;
            io::MidiIn &midiIn();

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
    } // namespace oneapp
} // namespace synth
