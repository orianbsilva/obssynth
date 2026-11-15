#pragma once
// The panel's sound: the ObsSynth4D engine, its audio (PulseAudio, else ALSA) and MIDI in
// (ObsSynth4D:input, for gridseq or a controller). The audio callback is the engine's only writer: the UI
// and MIDI threads queue commands (sound.cpp), applied at the start of each block. Values in engine units
// (params.hpp).
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

    class Sound
    {
    public:
        Sound();
        ~Sound();
        // before the audio: the banks (one per oscillator, may be empty) and every value; then audio and MIDI
        // grain: the recording the granular oscillators read (one for the four)
        bool start(const std::array<std::string, 4> &banks, const std::string &grain, const float osc[4][48], const float glb[48], const io::Settings &settings);
        // SETUP: the output again, as chosen (false: nothing opened); what is open; the MIDI in
        bool reopenAudio(const io::Settings &settings);
        const io::AudioOut &audioOut() const;
        io::MidiIn &midiIn();

        void setOsc(int osc, int param, float value);
        void setGlobal(int param, float value);
        void setRoute(int slot, int source, int dest, float amount, bool enabled);
        // what a preset sets that the panel has no control for: the mod envelopes (id env*4 + 0..3: A D S R),
        // the tube's model (12) and chaos (13); and the vector: on (14), loop (15), points (16), point i's
        // t, x, y (17 + i*3 + 0..2)
        void setExtra(int id, float value);
        // a recording for an oscillator's grains, read on the caller's thread; the engine takes it at its next
        // block (false: the last one is not taken yet, try again). Keep src alive: the old ones are freed here.
        bool setGrainSource(int osc, std::shared_ptr<const GrainSource> src);
        // the CCs that came in, oldest first (the panel's CC overlay)
        bool pollCc(int &channel, int &cc, float &value);
        // a controller's value from the panel, as if it came on MIDI (the CC overlay's mod sources: 1, 16..19)
        void control(int cc, float value);
        void noteOn(int note, float velocity);
        void noteOff(int note);

        // remote: a plugin plays (ObsSynth4D.clap/.vst3) and this only shows and turns; link: how to reach it (its
        // socket's fd "3", on Windows its two pipes "<read>,<write>"). Before start(), which then opens no audio nor
        // MIDI. What the panel changes goes there; what the host changes comes back: a value (pollRemote: the
        // plugin's id, o * OSC_PARAM_COUNT + p, then the globals; 0..1), or the whole state (pollWhole: its JSON, on
        // opening, a program, a session loaded). The meters, the vector and the scope follow the plugin's engine.
        bool connect(const char *link);
        bool remote() const;
        bool remoteAlive() const;
        bool pollRemote(int &id, float &value);
        bool pollWhole(std::string &json);
        // remote: an oscillator's recording, by its name (the plugin reads it; setGrainSource then only shows it)
        void setGrainFile(int osc, const std::string &file);

        int tables(int osc) const; // a bank's table count, known from start
        float source(int id) const;
        void vectorNow(float &x, float &y) const; // where the newest voice is in the vector square // a mod source's value now (the audio thread publishes them each block)
        // the last samples played (left), for the scope: read without a lock, a frame may tear
        void scope(float *out, int n) const;
        std::string debugLine(); // what the audio thread did since the last call (OBS_PANEL_DEBUG)

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace synth
