/**
 * @file obs_synth_one.hpp
 * @brief ObsSynth One: one oscillator with five engines (standard, wavetable, FM, granular, sample), its filter and envelopes,
 *        and a short master: the tube and the delay, each run only while it is heard.
 *
 * A product of its own over the same SynthEngine as ObsSynth4D (the voices, the oscillator chains, the engines), with
 * its own wiring: one chain of the four per voice is played (the other three are not rendered), no master filter,
 * no master effect slot. A small mod matrix is there and costs nothing until a route uses it: two LFOs (advanced only
 * while a route listens to one) and four routes from a source (an LFO, the envelopes, velocity, the wheel, the key, a random
 * number) to a destination of the oscillator (position, cutoff, resonance, pitch, level, pan, FM index, the filter envelope). The master is
 *   engine (oversampled) -> tube (if its mix > 0) -> decimation -> delay (if its mix > 0) -> volume and limiter.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "../../lib/synth/synth_engine.hpp"
#include "../../lib/synth/engine_profile.hpp"
#include "../../lib/synth/core/decimator.hpp"
#include "../../lib/synth/core/output_stage.hpp"
#include "../../lib/effects/delay.hpp"
#include "../../lib/effects/waveshaper.hpp"
#include "../../lib/oscillators/lfo.hpp"
#include "../obs_synth/obs_synth_mod_dest.hpp"
#include "../obs_synth/obs_synth_mod_source.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace synth
{
    class ObsSynthOne
    {
    public:
        static constexpr int OVERSAMPLE = 2;

        explicit ObsSynthOne(float sampleRate, int oversample = OVERSAMPLE)
            : m_sampleRate(sampleRate),
              m_os(std::clamp(oversample, 1, 2)),
              m_engine(sampleRate * static_cast<float>(m_os), WaveType::SAW),
              m_tube(sampleRate * static_cast<float>(m_os)),
              m_delay(sampleRate),
              m_output(sampleRate, m_volume)
        {
            m_osBuffer.reserve(8192 * 2 * static_cast<size_t>(m_os)); // blocks up to 8192 frames without an allocation on the audio thread
            m_decimator.init(sampleRate, m_os);
            m_engine.setSkipSilent(true); // the three chains not played are not rendered (their envelopes run)
            for (int c = 1; c < 4; ++c)
                chains([c](OscillatorChain &ch, int i) { if (i == c) ch.setLevel(0.0f); });
            m_tube.setMix(0.0f);
            m_delay.setMix(0.0f);
            m_delay.setPingPong(true);
            // the matrix: the sources each voice has, the wheel and the bend, which of them are bipolar
            {
                SynthEngine::VoiceSourceIds ids;
                ids.velocity = static_cast<int>(ObsSynthSrc::VELOCITY);
                ids.keytrack = static_cast<int>(ObsSynthSrc::KEYTRACK);
                ids.random = static_cast<int>(ObsSynthSrc::RANDOM);
                ids.filterEnv = static_cast<int>(ObsSynthSrc::FILTER_ENV1);
                ids.ampEnv = static_cast<int>(ObsSynthSrc::AMP_ENV1);
                m_engine.setVoiceSourceIds(ids);
            }
            m_engine.setSourceIds(static_cast<int>(ObsSynthSrc::AFTERTOUCH), static_cast<int>(ObsSynthSrc::MOD_WHEEL), static_cast<int>(ObsSynthSrc::PITCH_BEND));
            m_engine.getModMatrix().setBipolarCheck([](int src) { return isBipolarSource(static_cast<ObsSynthSrc>(src)); });
            const float osRate = sampleRate * static_cast<float>(m_os);
            m_lfos.emplace_back(2.0f, LFOWaveType::SINE, osRate);
            m_lfos.emplace_back(0.8f, LFOWaveType::TRIANGLE, osRate);
            m_engine.setPerSampleCallback([this]() {
                if (!m_lfoListened)
                    return; // no route listens to an LFO: it is not advanced
                auto &mm = m_engine.getModMatrix();
                for (size_t i = 0; i < m_lfos.size(); ++i)
                    mm.setSourceValue(static_cast<int>(ObsSynthSrc::LFO1) + static_cast<int>(i), m_lfos[i].getSample());
            });
        }

        // ============ Audio ============
        void generateStereoBuffer(float *buffer, int frames)
        {
            const int osFrames = frames * m_os;
            if (static_cast<size_t>(osFrames * 2) > m_osBuffer.capacity())
                m_osBuffer.reserve(static_cast<size_t>(osFrames * 2));
            m_osBuffer.resize(static_cast<size_t>(osFrames * 2));
            m_engine.generateStereoBuffer(m_osBuffer.data(), osFrames);
            if (m_tube.getMix() > 0.0f) // non-linear: at the oversampled rate
                m_tube.processStereo(m_osBuffer.data(), osFrames);
            if (m_os > 1)
                m_decimator.process(m_osBuffer.data(), buffer, frames);
            else
                std::copy(m_osBuffer.begin(), m_osBuffer.begin() + frames * 2, buffer);
            if (m_delay.getMix() > 0.0f)
                m_delay.processStereo(buffer, frames);
            m_output.process(buffer, frames, m_volume, 0.0f);
        }

        // ============ Profile ============
        /// The LFOs, the mod matrix and the voices' targets once per n samples (1: every sample)
        void setControlRate(int n)
        {
            m_engine.setControlRate(n);
            const float rate = m_sampleRate * static_cast<float>(m_os) / static_cast<float>(m_engine.getControlRate());
            for (auto &lfo : m_lfos)
                lfo.setSampleRate(rate);
        }
        int getControlRate() const { return m_engine.getControlRate(); }
        /// The polyphony played (1..32)
        void setVoiceLimit(int n) { m_engine.getAllocator().setVoiceLimit(n); }
        int getVoiceLimit() const { return m_engine.getAllocator().getVoiceLimit(); }
        /// A profile's voices and control rate (the oversampling is the constructor's: ObsSynthOne(rate, p.oversample));
        /// the silent chains are skipped in every profile (the One plays one chain of four)
        void applyProfile(const EngineProfile &p)
        {
            setControlRate(p.controlRate);
            setVoiceLimit(p.maxVoices);
        }
        int oversample() const { return m_os; }
        int sounding() const { return m_engine.getActiveVoiceCount(); }

        // ============ MIDI ============
        void noteOn(uint8_t note, float velocity) { m_engine.noteOn(note, velocity); }
        void noteOff(uint8_t note) { m_engine.noteOff(note); }
        void allNotesOff() { m_engine.getAllocator().allNotesOff(); }
        void setModWheel(float v) { m_engine.setModWheel(v); }
        void setPitchBend(float semitones) { m_engine.setPitchBend(semitones); }
        void setChannelAftertouch(float v) { m_engine.setChannelAftertouch(v); }
        void setExpression(float) {} // nothing listens to it here

        // ============ The oscillator (the first chain) and its filter ============
        void setWaveType(WaveType w) { chain([w](OscillatorChain &c) { c.setWaveType(w); }); }
        void setPartType(PartType p) { voices([p](auto &v) { v.getChain(0).setPartType(p); }); }
        void setCoarseTune(float semi) { m_coarse = semi; chain([t = m_coarse + m_fine](OscillatorChain &c) { c.setCoarseTune(t); }); }
        void setFineTune(float semi) { m_fine = semi; chain([t = m_coarse + m_fine](OscillatorChain &c) { c.setCoarseTune(t); }); }
        void setLevel(float v) { chain([v](OscillatorChain &c) { c.setLevel(v); }); }
        void setPan(float zeroToOne) { chain([zeroToOne](OscillatorChain &c) { c.setPan(zeroToOne); }); }
        void setCutoff(float hz) { chain([hz](OscillatorChain &c) { c.setCutoff(hz); }); }
        void setResonance(float r) { chain([r](OscillatorChain &c) { c.setResonance(r); }); }
        void setHPFCutoff(float hz) { chain([hz](OscillatorChain &c) { c.setHPFCutoff(hz); }); }
        void setFilterType(BiquadType t) { chain([t](OscillatorChain &c) { c.setFilterType(t); }); }
        void setKeyTrack(float a) { chain([a](OscillatorChain &c) { c.setKeyTrackAmount(a); }); }
        void setFilterEnvAmount(float a) { chain([a](OscillatorChain &c) { c.setFilterEnvAmount(a); }); }
        void setFilterAttack(float s) { chain([s](OscillatorChain &c) { c.setFilterAttack(s); }); }
        void setFilterDecay(float s) { chain([s](OscillatorChain &c) { c.setFilterDecay(s); }); }
        void setFilterSustain(float l) { chain([l](OscillatorChain &c) { c.setFilterSustain(l); }); }
        void setFilterRelease(float s) { chain([s](OscillatorChain &c) { c.setFilterRelease(s); }); }
        void setAmpAttack(float s) { chain([s](OscillatorChain &c) { c.setAmpAttack(s); }); }
        void setAmpDecay(float s) { chain([s](OscillatorChain &c) { c.setAmpDecay(s); }); }
        void setAmpSustain(float l) { chain([l](OscillatorChain &c) { c.setAmpSustain(l); }); }
        void setAmpRelease(float s) { chain([s](OscillatorChain &c) { c.setAmpRelease(s); }); }

        // --- wavetable ---
        void loadWtBank(const std::string &path) { voices([&path](auto &v) { v.getChain(0).loadWtBank(path, 0); }); }
        int wtTableCount() { return m_engine.getAllocator().getVoice(0).getChain(0).getWtTableCount(); }
        void setWtMorphPos(float pos) { chain([pos](OscillatorChain &c) { c.setWtMorphPosition(pos); }); }
        // --- granular: param 0 position (0..1), 1 size (s), 2 density (per s), 3 spray (0..1) ---
        void setGrainSource(const std::shared_ptr<const GrainSource> &src) { chain([&src](OscillatorChain &c) { c.setGrainSource(src); }); }
        void setGrain(int param, float v)
        {
            chain([param, v](OscillatorChain &c) {
                switch (param)
                {
                case 0: c.setGrainPosition(v); break;
                case 1: c.setGrainSize(v); break;
                case 2: c.setGrainDensity(v); break;
                default: c.setGrainSpray(v); break;
                }
            });
        }
        // --- sample ---
        void setSampleSource(const std::shared_ptr<const GrainSource> &src) { chain([&src](OscillatorChain &c) { c.setSampleSource(src); }); }
        void setSampleStart(float p) { chain([p](OscillatorChain &c) { c.setSampleStart(p); }); }
        void setSampleLoop(bool on) { chain([on](OscillatorChain &c) { c.setSampleLoop(on); }); }
        void setSampleRoot(float midiNote) { chain([midiNote](OscillatorChain &c) { c.setSampleRoot(midiNote); }); }
        // --- FM ---
        void setFMAlgorithm(FMAlgorithm a) { chain([a](OscillatorChain &c) { c.setFMAlgorithm(a); }); }
        void setFMModulatorRatio(float r) { chain([r](OscillatorChain &c) { c.setFMModulatorRatio(r); }); }
        void setFMModulationIndex(float i) { chain([i](OscillatorChain &c) { c.setFMModulationIndex(i); }); }
        void setFMFeedback(float f) { chain([f](OscillatorChain &c) { c.setFMFeedback(f); }); }
        void setFMOperatorRatio(int op, float r) { chain([op, r](OscillatorChain &c) { c.setFMOperatorRatio(op, r); }); }
        void setFMOperatorLevel(int op, float l) { chain([op, l](OscillatorChain &c) { c.setFMOperatorLevel(op, l); }); }

        // ============ Modulation (opt-in) ============
        // the sources and destinations as the plugin lists them (0 is "off" for a source)
        static constexpr int NUM_ROUTES = 4;
        static constexpr int SRC_CHOICES = 9, DST_CHOICES = 8;
        static int sourceId(int choice)
        {
            switch (choice)
            {
            case 1: return static_cast<int>(ObsSynthSrc::LFO1);
            case 2: return static_cast<int>(ObsSynthSrc::LFO2);
            case 3: return static_cast<int>(ObsSynthSrc::FILTER_ENV1);
            case 4: return static_cast<int>(ObsSynthSrc::AMP_ENV1);
            case 5: return static_cast<int>(ObsSynthSrc::VELOCITY);
            case 6: return static_cast<int>(ObsSynthSrc::MOD_WHEEL);
            case 7: return static_cast<int>(ObsSynthSrc::KEYTRACK);
            case 8: return static_cast<int>(ObsSynthSrc::RANDOM);
            default: return -1;
            }
        }
        static int destId(int choice)
        {
            switch (choice)
            {
            case 0: return static_cast<int>(ObsSynthDst::OSC1_WAVEPOS);
            case 1: return static_cast<int>(ObsSynthDst::OSC1_CUTOFF);
            case 2: return static_cast<int>(ObsSynthDst::OSC1_RESO);
            case 3: return static_cast<int>(ObsSynthDst::OSC1_PITCH);
            case 4: return static_cast<int>(ObsSynthDst::OSC1_LEVEL);
            case 5: return static_cast<int>(ObsSynthDst::OSC1_PAN);
            case 6: return static_cast<int>(ObsSynthDst::OSC1_FM_INDEX);
            default: return static_cast<int>(ObsSynthDst::OSC1_FENV_AMT);
            }
        }
        void setLfoRate(int i, float hz)
        {
            if (i >= 0 && i < static_cast<int>(m_lfos.size()))
                m_lfos[static_cast<size_t>(i)].setFrequency(hz);
        }
        void setLfoWave(int i, int shape)
        {
            if (i >= 0 && i < static_cast<int>(m_lfos.size()))
                m_lfos[static_cast<size_t>(i)].setWaveType(static_cast<LFOWaveType>(std::clamp(shape, 0, 4)));
        }
        /// A route: slot 0..3, a source choice (0: none), a destination choice, an amount -1..1
        void setRoute(int slot, int srcChoice, int dstChoice, float amount)
        {
            if (slot < 0 || slot >= NUM_ROUTES)
                return;
            m_routes[static_cast<size_t>(slot)] = {srcChoice, dstChoice, amount};
            auto &mm = m_engine.getModMatrix();
            const int src = sourceId(srcChoice);
            mm.setSlot(slot, src < 0 ? 0 : src, destId(dstChoice), amount);
            mm.setSlotEnabled(slot, src >= 0 && amount != 0.0f);
            m_lfoListened = false;
            for (const Route &r : m_routes)
                if ((r.src == 1 || r.src == 2) && r.amount != 0.0f)
                    m_lfoListened = true;
            if (!m_lfoListened) // nothing listens: the LFOs' last values must not stay
                for (int i = 0; i < 2; ++i)
                    mm.setSourceValue(static_cast<int>(ObsSynthSrc::LFO1) + i, 0.0f);
        }

        // ============ The master ============
        void setDrive(float v) { m_tube.setDrive(v); }
        void setAsymmetry(float v) { m_tube.setAsymmetry(v); }
        void setTubeMix(float v) { m_tube.setMix(v); }
        void setDelayTimeL(float s) { m_delay.setDelayTimeL(s); }
        void setDelayTimeR(float s) { m_delay.setDelayTimeR(s); }
        void setDelayMix(float v) { m_delay.setMix(v); }
        void setDelayFeedback(float v) { m_delay.setFeedback(v); }
        void setDelayDamping(float v) { m_delay.setDamping(v); }
        void setDelayPingPong(bool v) { m_delay.setPingPong(v); }
        void setVolume(float v) { m_volume = v; }

    private:
        template <typename F>
        void chain(F f) // the first chain of every voice
        {
            m_engine.getAllocator().configureChains([&f](OscillatorChain &c, int i) {
                if (i == 0)
                    f(c);
            });
        }
        template <typename F>
        void chains(F f) { m_engine.getAllocator().configureChains(f); }
        template <typename F>
        void voices(F f) { m_engine.getAllocator().configureVoices(f); }

        float m_sampleRate;
        int m_os;
        SynthEngine m_engine;
        TubeSaturation m_tube;
        StereoDelay m_delay;
        float m_volume{0.8f};
        OutputStage m_output;
        StereoDecimator m_decimator;
        std::vector<float> m_osBuffer;
        float m_coarse{0.0f}, m_fine{0.0f};
        struct Route
        {
            int src = 0, dst = 0;
            float amount = 0.0f;
        };
        std::array<Route, NUM_ROUTES> m_routes{};
        std::vector<LFO> m_lfos;
        bool m_lfoListened{false};
    };
} // namespace synth
