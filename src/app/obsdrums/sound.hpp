#pragma once
// obsdrums' sound: the ObsDrums engine, its audio (PulseAudio, else ALSA) and MIDI in (ObsDrums:input, where gridseq
// or a controller plays the pads by their notes). The audio callback is the engine's only writer: the UI and MIDI
// threads queue commands (sound.cpp), applied at the start of each block; recordings are read off the audio thread
// and handed over as pointers, the old ones kept here so none is freed there.
#include <array>
#include <memory>
#include <string>

namespace synth
{
    namespace io
    {
        struct Settings;
        class AudioOut;
        class MidiIn;
    }
    struct GrainSource;
    struct DrumKit;

    namespace drums
    {
        // a pad's settings, one at a time
        enum PadParam
        {
            P_ENGINE,
            P_CIRCUIT,
            P_TUNE,
            P_DECAY,
            P_A,
            P_B,
            P_BENT,
            P_LEVEL,
            P_PAN,
            P_FX,
            P_MUTE,
            P_SOLO,
            P_NOTE,
            P_CHOKE,
            P_GRAINPOS,
        };
        enum MasterParam
        {
            M_DELAY_TIME,
            M_DELAY_FB,
            M_DELAY_MIX,
            M_REV_SIZE,
            M_REV_DAMP,
            M_REV_MIX,
            M_DRIVE,
            M_GLUE,
            M_TUBE_MIX,
            M_VOLUME,
        };

        class Sound
        {
        public:
            Sound();
            ~Sound();
            // the kit before the audio starts, then audio and MIDI
            bool start(const DrumKit &kit, const std::array<std::shared_ptr<const GrainSource>, 16> &sources, const io::Settings &settings);
            // SETUP: the output again, as chosen (false: nothing opened); what is open; the MIDI in
            bool reopenAudio(const io::Settings &settings);
            const io::AudioOut &audioOut() const;
            io::MidiIn &midiIn();

            void setPad(int pad, int param, float value);
            void setMaster(int param, float value);
            void setRoute(int slot, int source, int dest, float amount, bool enabled);
            void clearRoutes();
            void hit(int pad, float velocity);
            // false: the engine has not taken the pad's last one yet (try again)
            bool setSource(int pad, std::shared_ptr<const GrainSource> src);

            // what came in on MIDI, for the panel (the CC overlay, the pad a hit chose), oldest first
            struct Seen
            {
                bool cc;       // a CC; else a note-on
                int channel;   // 0..15
                int number;    // the CC or the note
                int pad;       // a note: the pad it hit (-1 none)
                float value;   // 0..1
            };
            bool pollMidi(Seen &e);
            // the controller's notes (that MIDI client's alone) to pads: padOf[note], -1 none; client -1: no map
            void setNoteMap(int client, const std::array<int, 128> &padOf);

            // remote: a plugin plays (ObsDrums.clap/.vst3) and this only shows and turns; link: how to reach it (its
            // socket's fd "3", on Windows its two pipes "<read>,<write>"). Before start(), which then opens no audio
            // nor MIDI. The pads' and the master's values go as the plugin's parameters; what has none (notes, chokes,
            // routes, recordings) makes kitWanted() true once: then sendKit() the whole kit (kits_io.hpp's JSON). What
            // the host changes comes back: a value (pollRemote: pad -1 the master; param a PadParam or MasterParam),
            // or the whole kit (pollKit: its JSON, on opening, a program, a session loaded). The meters follow the
            // plugin's engine.
            bool connect(const char *link);
            bool remote() const;
            bool remoteAlive() const;
            bool kitWanted();
            void sendKit(const std::string &json);
            bool pollRemote(int &pad, int &param, float &value);
            bool pollKit(std::string &json);

            float padPeak(int pad) const;  // the pad's loudest sample of the last block
            float padFlash(int pad) const; // 1 at a hit, fading
            float outPeak(int side) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
    } // namespace drums
} // namespace synth
