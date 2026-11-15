#pragma once
// obssynthct1's sound: the ObsSynthCT1 engine, its audio and MIDI in (common/platform_io.hpp: as v1 on each system,
// what SETUP chose; Linux: ObsSynthCT1:input). The audio callback is the engine's only writer: the UI and MIDI
// threads queue commands, applied at the start of each block; the engine's state for the panel (levels, the
// operators' positions now) comes back as atomics.
#include <array>
#include <memory>
#include <vector>

namespace synth
{
    namespace io
    {
        struct Settings;
        class AudioOut;
        class MidiIn;
    }
    namespace ct1
    {
        class Table;
    }

    namespace ct1app
    {
        class Sound
        {
        public:
            Sound();
            ~Sound();
            // banks: the wavetables (built before, shared with the panel's pictures); params: physical values
            bool start(const std::vector<std::shared_ptr<const ct1::Table>> &banks, const float *params, const io::Settings &settings);

            void setParam(int id, float value); // physical
            void noteOn(int note, float velocity);
            void noteOff(int note);
            void allNotesOff();

            float outPeak(int side) const;
            float livePos(int op) const; // the newest voice's position of an operator, -1 none
            int sounding() const;
            // the CCs that came in, oldest first (the CC overlay)
            bool pollCc(int &channel, int &cc, float &value);

            // remote: a plugin plays (ObsSynthCT1.clap); link is how to reach it: its socket's fd ("3"), or on Windows its
            // two pipes ("<read handle>,<write handle>"). What the panel changes goes there; what the host changes
            // (automation, its preset list, a session loaded) comes back through pollRemote.
            bool connect(const char *link);
            bool remote() const;
            bool remoteAlive() const;
            bool pollRemote(int &id, float &value); // id: a parameter, or -1 with value the preset (PROGRAM)

            bool reopenAudio(const io::Settings &settings);
            const io::AudioOut &audioOut() const;
            io::MidiIn &midiIn();

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
    } // namespace ct1app
} // namespace synth
