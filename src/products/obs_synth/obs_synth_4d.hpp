/**
 * @file obs_synth_4d.hpp
 * @brief ObsSynth4D - 4-oscillator polyphonic synthesizer product
 *
 * Product-level class wrapping SynthEngine with:
 * - 4 LFOs (sample-rate advanced via per-sample callback)
 * - Mod matrix configuration (source IDs, bipolar check)
 * - Preset management (chain config + mod routes)
 * - MIDI handling (note, CC, aftertouch, pitch bend)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "../../lib/synth/synth_engine.hpp"
#include "../../lib/synth/core/effect_slot.hpp"
#include "../../lib/synth/core/effect_factory.hpp"
#include "../../lib/synth/core/biquad_filter.hpp"
#include "../../lib/synth/core/decimator.hpp"
#include "../../lib/synth/core/output_stage.hpp"
#include "../../lib/oscillators/lfo.hpp"
#include "../../lib/synth/engine_profile.hpp"
#include "../../lib/effects/delay.hpp"
#include "../../lib/effects/waveshaper.hpp"
#include "../../lib/oscillators/adsr_envelope.hpp"
#include "obs_synth_mod_source.hpp"
#include "obs_synth_mod_dest.hpp"
#include "obs_synth_preset.hpp"
#include "obs_synth_preset_bank.hpp"
#include "obs_synth_preset_io.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#if defined(_MSC_VER)
#include <immintrin.h> // SSE2 .. AVX2 on MSVC (/arch:AVX2)
#define SYNTH_FORCEINLINE __forceinline
#elif defined(__SSE2__)
#include <immintrin.h>
#define SYNTH_FORCEINLINE inline __attribute__((always_inline))
#else
#define SYNTH_FORCEINLINE inline
#endif

namespace synth
{

    class ObsSynth4D
    {
    public:
        static constexpr int NUM_LFOS = 4;
        static constexpr int NUM_MOD_ENVS = 3;
        static constexpr int OVERSAMPLE = 2; // the internal render rate's multiplier, by default (a small machine's profile: 1)

        explicit ObsSynth4D(float sampleRate, int oversample = OVERSAMPLE)
            : m_engine(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2)), WaveType::SAW),
              m_lfos{
                  LFO(2.0f, LFOWaveType::SINE, sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
                  LFO(0.8f, LFOWaveType::TRIANGLE, sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
                  LFO(0.3f, LFOWaveType::SINE, sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
                  LFO(5.0f, LFOWaveType::SAW, sampleRate * static_cast<float>(std::clamp(oversample, 1, 2)))},
              m_masterFX(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
              m_masterFXR(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
              m_masterSaturation(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))), // non-linear: must run at OS rate
              m_masterDelay(sampleRate),                   // linear: native rate is fine
              m_masterFilterL(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2)), BiquadType::LOWPASS, 20000.0f, 0.0f),
              m_masterFilterR(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2)), BiquadType::LOWPASS, 20000.0f, 0.0f),
              m_modEnvs{ADSREnvelope(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
                        ADSREnvelope(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2))),
                        ADSREnvelope(sampleRate * static_cast<float>(std::clamp(oversample, 1, 2)))},
              m_sampleRate(sampleRate)
        {
            m_os = std::clamp(oversample, 1, 2);
            m_osBuffer.reserve(8192 * 2 * m_os); // blocks up to 8192 frames without an allocation on the audio thread
            m_fxL.reserve(8192 * m_os);
            m_fxR.reserve(8192 * m_os);
            // Init SSE2 stereo decimation filter
            // an 8th-order Butterworth at 20 kHz, four sections (it was one 2nd-order section at 21.6 kHz: what folds
            // back from 26-40 kHz lost 3-10 dB; now 18-48)
            m_decimator.init(sampleRate, m_os);
            m_output = OutputStage(sampleRate, m_masterVolume);
            // Configure master tube saturation
            m_masterSaturation.setDrive(1.8f);
            m_masterSaturation.setAsymmetry(0.3f);
            m_masterSaturation.setMix(0.4f);

            // Configure master delay defaults
            m_masterDelay.setDelayTimeL(0.375f); // dotted-eighth feel
            m_masterDelay.setDelayTimeR(0.5f);   // half-beat at 120bpm
            m_masterDelay.setFeedback(0.35f);
            m_masterDelay.setMix(0.25f);
            m_masterDelay.setPingPong(true);
            m_masterDelay.setDamping(0.4f);
            // Configure mod source IDs
            // the sources each voice has of its own: velocity, key, random, its envelopes, its mod envelopes
            {
                SynthEngine::VoiceSourceIds ids;
                ids.velocity = (int)ObsSynthSrc::VELOCITY;
                ids.keytrack = (int)ObsSynthSrc::KEYTRACK;
                ids.random = (int)ObsSynthSrc::RANDOM;
                ids.filterEnv = (int)ObsSynthSrc::FILTER_ENV1;
                ids.ampEnv = (int)ObsSynthSrc::AMP_ENV1;
                ids.modEnv = (int)ObsSynthSrc::MOD_ENV;
                m_engine.setVoiceSourceIds(ids);
            }
            m_engine.setSourceIds(
                (int)ObsSynthSrc::AFTERTOUCH,
                (int)ObsSynthSrc::MOD_WHEEL,
                (int)ObsSynthSrc::PITCH_BEND);

            // Set bipolar check for mod matrix
            m_engine.getModMatrix().setBipolarCheck([](int src_id)
                                                    { return isBipolarSource(static_cast<ObsSynthSrc>(src_id)); });

            // Register per-sample callback to advance LFOs and mod envelopes
            m_engine.setPerSampleCallback([this]()
                                          {
                auto &mm = m_engine.getModMatrix();
                for (int i = 0; i < NUM_LFOS; ++i)
                {
                    // the matrix on the LFOs: rate ±3 octaves, depth 0..2x
                    const float rateMod = mm.getModValue((int)ObsSynthDst::LFO1_RATE + i);
                    if (rateMod != m_lfoRateMod[i])
                    {
                        m_lfoRateMod[i] = rateMod;
                        m_lfos[i].setFrequency(m_lfoRate[i] * std::exp2(rateMod * 3.0f));
                    }
                    // depth: whole, unless a route reaches it; then the routes set it, 0..2 (a mod wheel fades the
                    // LFO in, as the presets meant; it was 1 + routes, full at the wheel's 0), without the LFO's own
                    // route to itself (that fed back at the sample rate)
                    const int depthDst = (int)ObsSynthDst::LFO1_DEPTH + i;
                    const float depth = mm.routesTo(depthDst) ? std::clamp(mm.getModValueWithout(depthDst, (int)ObsSynthSrc::LFO1 + i), 0.0f, 2.0f) : 1.0f;
                    mm.setSourceValue((int)ObsSynthSrc::LFO1 + i, m_lfos[i].getSample() * depth);
                } });

            // Load default preset
            selectPreset(0);
        }

        // ============ Audio Generation ============

        // Returns last generateStereoBuffer wall-clock time in milliseconds
        float getLastAudioMs() const { return m_lastAudioMs.load(std::memory_order_relaxed); }
        // Returns last buffer's CPU usage as percentage of its time budget
        float getLastAudioCpuPct() const { return m_lastAudioCpuPct.load(std::memory_order_relaxed); }

        void generateStereoBuffer(float *buffer, int frames)
        {
            const auto t0 = std::chrono::high_resolution_clock::now();

            // ── 1. Render at oversampled rate ─────────────────────────────────
            const int osFrames = frames * m_os;
            if (static_cast<size_t>(osFrames * 2) > m_osBuffer.capacity()) // (reserved: no allocation in the usual blocks)
                m_osBuffer.reserve(static_cast<size_t>(osFrames * 2));
            m_osBuffer.resize(static_cast<size_t>(osFrames * 2));
            m_engine.generateStereoBuffer(m_osBuffer.data(), osFrames);

            // Master FX at oversampled rate, a mono effect per side (one effect ran over L,R,L,R as one stream:
            // the sides mixed in its state, its times halved)
            if (m_masterFX.hasEffect())
            {
                m_fxL.resize(static_cast<size_t>(osFrames));
                m_fxR.resize(static_cast<size_t>(osFrames));
                for (int i = 0; i < osFrames; ++i)
                {
                    m_fxL[static_cast<size_t>(i)] = m_osBuffer[static_cast<size_t>(i) * 2];
                    m_fxR[static_cast<size_t>(i)] = m_osBuffer[static_cast<size_t>(i) * 2 + 1];
                }
                m_masterFX.processBuffer(m_fxL.data(), osFrames);
                if (m_masterFXR.hasEffect())
                    m_masterFXR.processBuffer(m_fxR.data(), osFrames);
                for (int i = 0; i < osFrames; ++i)
                {
                    m_osBuffer[static_cast<size_t>(i) * 2] = m_fxL[static_cast<size_t>(i)];
                    m_osBuffer[static_cast<size_t>(i) * 2 + 1] = m_fxR[static_cast<size_t>(i)];
                }
            }

            // Master filter at oversampled rate
            if (!m_masterFilterBypassed)
            {
                for (int i = 0; i < osFrames; ++i)
                {
                    m_osBuffer[i * 2] = m_masterFilterL.process(m_osBuffer[i * 2]);
                    m_osBuffer[i * 2 + 1] = m_masterFilterR.process(m_osBuffer[i * 2 + 1]);
                }
            }

            // Tube saturation at oversampled rate (non-linear — most benefit from OS)
            // the tube's modulation: from the last block's values to this one's, in steps of 64 (it jumped once per
            // block: stairs and clicks under an LFO)
            {
                const auto &mm = m_engine.getModMatrix();
                const float d1 = mm.getModValue((int)ObsSynthDst::TUBE_DRIVE), a1 = mm.getModValue((int)ObsSynthDst::TUBE_ASYM),
                            c1 = mm.getModValue((int)ObsSynthDst::TUBE_CHAOS);
                const bool moving = d1 != 0.0f || a1 != 0.0f || c1 != 0.0f || m_tubeMod[0] != 0.0f || m_tubeMod[1] != 0.0f || m_tubeMod[2] != 0.0f;
                for (int at = 0; at < osFrames; at += 64)
                {
                    const int n = std::min(64, osFrames - at);
                    if (moving)
                    {
                        const float f = static_cast<float>(at + n) / static_cast<float>(osFrames);
                        m_masterSaturation.setDrive(m_baseDrive + (m_tubeMod[0] + (d1 - m_tubeMod[0]) * f) * 4.0f);
                        m_masterSaturation.setAsymmetry(m_baseAsymmetry + (m_tubeMod[1] + (a1 - m_tubeMod[1]) * f) * 0.5f);
                        m_masterSaturation.setChaos(m_baseChaos + (m_tubeMod[2] + (c1 - m_tubeMod[2]) * f) * 0.5f);
                    }
                    m_masterSaturation.processStereo(m_osBuffer.data() + at * 2, n);
                }
                if (moving)
                {
                    m_masterSaturation.setDrive(m_baseDrive);
                    m_masterSaturation.setAsymmetry(m_baseAsymmetry);
                    m_masterSaturation.setChaos(m_baseChaos);
                }
                m_tubeMod[0] = d1;
                m_tubeMod[1] = a1;
                m_tubeMod[2] = c1;
            }

            // ── 2. Decimate OVERSAMPLE:1 — dedicated SSE2+AVX2 function ────
            if (m_os > 1)
                m_decimator.process(m_osBuffer.data(), buffer, frames);
            else // rendered at the output's rate: nothing to decimate
                std::copy(m_osBuffer.begin(), m_osBuffer.begin() + frames * 2, buffer);

            // ── 3. Delay + volume at native rate ──────────────────────────────
            m_masterDelay.processStereo(buffer, frames);

            const float volume = std::clamp(m_masterVolume + m_engine.getModMatrix().getModValue((int)ObsSynthDst::MASTER_VOL), 0.0f, 1.0f);
            const float pan = std::clamp(m_engine.getModMatrix().getModValue((int)ObsSynthDst::MASTER_PAN), -1.0f, 1.0f);

            // ── 4. The output: DC out, volume and balance ramped, the limiter (OutputStage)
            m_output.process(buffer, frames, volume, pan);

            // ── Metrics ───────────────────────────────────────────────────────
            const auto t1 = std::chrono::high_resolution_clock::now();
            const float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
            const float budgetMs = (static_cast<float>(frames) / m_sampleRate) * 1000.0f;
            m_lastAudioMs.store(elapsedMs, std::memory_order_relaxed);
            m_lastAudioCpuPct.store(budgetMs > 0.0f ? (elapsedMs / budgetMs) * 100.0f : 0.0f,
                                    std::memory_order_relaxed);
        }

        // mono: the stereo path (oversampling, the master chain) summed (it rendered the oversampled engine straight
        // into the buffer: an octave low, the envelopes twice slow, no master effects)
        void generateBuffer(float *buffer, int size)
        {
            float lr[512];
            for (int done = 0; done < size;)
            {
                const int n = std::min(256, size - done);
                generateStereoBuffer(lr, n);
                for (int i = 0; i < n; ++i)
                    buffer[done + i] = 0.5f * (lr[i * 2] + lr[i * 2 + 1]);
                done += n;
            }
        }

        // ============ MIDI Input ============

        void noteOn(uint8_t note, float velocity)
        {
            m_engine.noteOn(note, velocity);
        }

        void noteOff(uint8_t note)
        {
            m_engine.noteOff(note); // the mod envelopes are the voices' now, released with them
        }

        void setExpression(float value) { m_engine.getModMatrix().setSourceValue((int)ObsSynthSrc::EXPRESSION, std::clamp(value, 0.0f, 1.0f)); }
        void setCC(int which, float value) // CC A..D (0..3)
        {
            if (which >= 0 && which < 4)
                m_engine.getModMatrix().setSourceValue((int)ObsSynthSrc::CC_A + which, std::clamp(value, 0.0f, 1.0f));
        }
        void allNotesOff() { m_engine.getAllocator().allNotesOff(); }
        void setPolyAftertouch(uint8_t note, float value) { m_engine.setPolyAftertouch(note, value); }

        void setModWheel(float value)
        {
            m_engine.setModWheel(value);
        }

        void setChannelAftertouch(float value)
        {
            m_engine.setChannelAftertouch(value);
        }

        void setPitchBend(float semitones)
        {
            m_engine.setPitchBend(semitones);
        }

        void setCutoff(float hz)
        {
            m_engine.setCutoff(hz);
        }

        void setResonance(float value)
        {
            m_engine.setResonance(value);
        }

        // ============ Engine profile (a small machine) ============
        /// The modulation once per n internal samples (LFOs, matrix, voices' targets): see SynthEngine::setControlRate
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
        void applyProfile(const EngineProfile &p)
        {
            setControlRate(p.controlRate);
            setVoiceLimit(p.maxVoices);
            m_engine.setSkipSilent(p.skipSilent); // (the oversampling is the constructor's: ObsSynth4D(rate, p.oversample))
        }

        // ============ LFO Access ============

        LFO &getLFO(int index) { return m_lfos[index]; }
        const LFO &getLFO(int index) const { return m_lfos[index]; }

        // ============ Preset Management ============

        int getPresetCount() const { return PRESET_BANK_SIZE; }
        int getCurrentPreset() const { return m_currentPreset; }
        const char *getPresetName() const { return m_currentPresetName; }
        const Preset4D *getCurrentPresetData() const { return m_currentPresetData; }

        void selectPreset(int index)
        {
            if (index < 0 || index >= PRESET_BANK_SIZE)
                return;
            applyPreset(*PRESET_BANK[index]);
            m_currentPreset = index;
        }

        bool loadPresetFromJson(const std::string &filepath)
        {
            Preset4D preset;
            if (!loadPresetFromFile(filepath, preset))
                return false;
            applyPreset(preset);
            m_currentPreset = -1; // user preset, not from bank
            return true;
        }

        bool saveCurrentPresetToJson(const std::string &filepath) const
        {
            if (m_currentPreset >= 0 && m_currentPreset < PRESET_BANK_SIZE)
                return savePresetToFile(*PRESET_BANK[m_currentPreset], filepath);
            return false;
        }

        void applyPreset(const Preset4D &p)
        {
            m_currentPresetName = p.name;
            m_currentPresetData = &p;

            // Configure oscillator chains
            m_engine.getModMatrix().clear();
            m_engine.getAllocator().configureVoices([&](auto &voice)
                                                    {
                voice.setWaveType(p.waves);
                voice.setLevel(p.levels);
                voice.setPan(p.pans);
                voice.setCutoff(p.cutoffs);
                voice.setResonance(p.resonances);
                voice.setFilterEnvAmount(p.filterEnvAmounts);
                voice.setCoarseTune(p.coarseTune);
                voice.setAmpADSR(p.ampA, p.ampD, p.ampS, p.ampR);
                voice.setFilterADSR(p.fltA, p.fltD, p.fltS, p.fltR);
                // PartType + FM params
                for (int osc = 0; osc < PRESET_NUM_OSCS; ++osc)
                {
                    auto &chain = voice.getChain(osc);
                    chain.setPartType(p.partTypes[osc]);
                    chain.setWtMorphPosition(p.wtMorphPos[osc]);
                    chain.setGrainPosition(p.grain[osc].position);
                    chain.setGrainSize(p.grain[osc].size);
                    chain.setGrainDensity(p.grain[osc].density);
                    chain.setGrainSpray(p.grain[osc].spray);
                    if (p.partTypes[osc] == PartType::FM_ENGINE)
                    {
                        chain.setFMAlgorithm(p.fmParams[osc].algorithm);
                        chain.setFMModulatorRatio(p.fmParams[osc].modRatio);
                        chain.setFMModulationIndex(p.fmParams[osc].modIndex);
                        chain.setFMFeedback(p.fmParams[osc].feedback);
                        chain.setFMOperatorRatio(2, p.fmParams[osc].op2Ratio);
                        chain.setFMOperatorLevel(2, p.fmParams[osc].op2Level);
                    }
                } });

            // Configure LFOs
            for (int i = 0; i < NUM_LFOS; ++i)
            {
                setLFORate(i, p.lfos[i].rate);
                m_lfos[i].setWaveType(p.lfos[i].wave);
            }

            // Configure mod envelopes
            for (int i = 0; i < NUM_MOD_ENVS; ++i)
                setModEnv(i, p.modEnvs[i].attack, p.modEnvs[i].decay, p.modEnvs[i].sustain, p.modEnvs[i].release);

            // Configure delay
            m_masterDelay.setDelayTimeL(p.delay.timeL);
            m_masterDelay.setDelayTimeR(p.delay.timeR);
            m_masterDelay.setFeedback(p.delay.feedback);
            m_masterDelay.setMix(p.delay.mix);
            m_masterDelay.setDamping(p.delay.damping);
            m_masterDelay.setPingPong(p.delay.pingPong);

            // Configure tube saturation
            m_masterSaturation.setModel(p.tube.model);
            m_baseDrive = p.tube.drive;
            m_baseAsymmetry = p.tube.asymmetry;
            m_baseChaos = p.tube.chaos;
            m_masterSaturation.setDrive(m_baseDrive);
            m_masterSaturation.setAsymmetry(m_baseAsymmetry);
            m_masterSaturation.setChaos(m_baseChaos);
            m_masterSaturation.setMix(p.tube.mix);

            // Configure master filter
            m_masterFilterL.setCutoff(p.masterFilterCutoff);
            m_masterFilterR.setCutoff(p.masterFilterCutoff);
            m_masterFilterL.setResonance(p.masterFilterResonance);
            m_masterFilterR.setResonance(p.masterFilterResonance);

            for (int i = 0; i < PRESET_NUM_OSCS; ++i) // the tune's parts as the preset leaves them
            {
                m_coarse[i] = p.coarseTune[i];
                m_fine[i] = 0.0f;
            }

            // vector synthesis
            m_engine.setVectorEnabled(p.vector.on);
            m_engine.setVectorLoop(p.vector.loop);
            for (int i = 0; i < p.vector.count && i < 16; ++i)
                m_engine.setVectorPoint(i, p.vector.path[i].t, p.vector.path[i].x, p.vector.path[i].y);
            m_engine.setVectorCount(p.vector.count);

            // Apply mod routes
            for (int i = 0; i < p.numRoutes; ++i)
            {
                m_engine.getModMatrix().addRoute(
                    (int)p.routes[i].src,
                    (int)p.routes[i].dst,
                    p.routes[i].amount);
            }

            std::cerr << "[4D] Preset: " << p.name << std::endl;
        }

        // ============ Engine Access (for ModMatrix grid visualization etc) ============

        SynthEngine &getEngine() { return m_engine; }
        const SynthEngine &getEngine() const { return m_engine; }
        ModMatrix &getModMatrix() { return m_engine.getModMatrix(); }
        const ModMatrix &getModMatrix() const { return m_engine.getModMatrix(); }
        StereoDelay &getMasterDelay() { return m_masterDelay; }
        TubeSaturation &getMasterSaturation() { return m_masterSaturation; }
        EffectSlot &getMasterFX() { return m_masterFX; }
        const EffectSlot &getMasterFX() const { return m_masterFX; }

        // ============ Parameter Setters (for UI knobs) ============

        // --- Per-oscillator (chain) setters ---
        void setOscWaveType(int osc, WaveType w)
        {
            m_engine.getAllocator().configureChains([osc, w](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setWaveType(w); });
        }
        void loadOscPCM(int osc, const std::string &path)
        {
            m_engine.getAllocator().configureVoices([osc, path](auto &v)
                                                    { v.getChain(osc).loadPCMWaveform(path); });
        }
        // the chain's tune is coarse + fine (fine overwrote the coarse)
        void setOscCoarseTune(int osc, float semi)
        {
            if (osc < 0 || osc > 3)
                return;
            m_coarse[osc] = semi;
            const float t = m_coarse[osc] + m_fine[osc];
            m_engine.getAllocator().configureChains([osc, t](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setCoarseTune(t); });
        }
        void setOscFineTune(int osc, float semi)
        {
            if (osc < 0 || osc > 3)
                return;
            m_fine[osc] = semi;
            const float t = m_coarse[osc] + m_fine[osc];
            m_engine.getAllocator().configureChains([osc, t](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setCoarseTune(t); });
        }
        void setOscCutoff(int osc, float hz)
        {
            m_engine.getAllocator().configureChains([osc, hz](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setCutoff(hz); });
        }
        void setOscResonance(int osc, float r)
        {
            m_engine.getAllocator().configureChains([osc, r](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setResonance(r); });
        }
        void setOscFilterEnvAmount(int osc, float amt)
        {
            m_engine.getAllocator().configureChains([osc, amt](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterEnvAmount(amt); });
        }
        void setOscFilterAttack(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterAttack(s); });
        }
        void setOscFilterDecay(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterDecay(s); });
        }
        void setOscFilterSustain(int osc, float lvl)
        {
            m_engine.getAllocator().configureChains([osc, lvl](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterSustain(lvl); });
        }
        void setOscFilterRelease(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterRelease(s); });
        }
        void setOscAmpAttack(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setAmpAttack(s); });
        }
        void setOscAmpDecay(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setAmpDecay(s); });
        }
        void setOscAmpSustain(int osc, float lvl)
        {
            m_engine.getAllocator().configureChains([osc, lvl](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setAmpSustain(lvl); });
        }
        void setOscAmpRelease(int osc, float s)
        {
            m_engine.getAllocator().configureChains([osc, s](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setAmpRelease(s); });
        }
        void setOscLevel(int osc, float lvl)
        {
            m_engine.getAllocator().configureChains([osc, lvl](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setLevel(lvl); });
        }
        void setOscPan(int osc, float pan)
        {
            m_engine.getAllocator().configureChains([osc, pan](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setPan(pan); });
        }
        void setOscHPFCutoff(int osc, float hz)
        {
            m_engine.getAllocator().configureChains([osc, hz](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setHPFCutoff(hz); });
        }
        void setOscFilterType(int osc, BiquadType type)
        {
            m_engine.getAllocator().configureChains([osc, type](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFilterType(type); });
        }
        void setOscKeyTrack(int osc, float amount)
        {
            m_engine.getAllocator().configureChains([osc, amount](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setKeyTrackAmount(amount); });
        }

        // --- Granular ---
        // a recording for an oscillator's grains, shared by every voice; before the audio starts (it allocates)
        bool loadGrainSource(int osc, const std::string &path)
        {
            auto src = GrainSource::loadWav(path);
            if (!src)
                return false;
            std::shared_ptr<const GrainSource> shared = src;
            m_engine.getAllocator().configureChains([osc, &shared](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setGrainSource(shared); });
            return true;
        }
        // a recording already read (on another thread), for one oscillator: no allocation here (shared_ptr copies);
        // the caller keeps the old one alive, so it is never freed on the audio thread
        void setGrainSource(int osc, const std::shared_ptr<const GrainSource> &src)
        {
            m_engine.getAllocator().configureChains([osc, &src](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setGrainSource(src); });
        }

        // param 0 position (0..1), 1 size (s), 2 density (per s), 3 spray (0..1)
        void setOscGrain(int osc, int param, float v)
        {
            m_engine.getAllocator().configureChains([osc, param, v](OscillatorChain &c, int i)
                                                    {
                if (i != osc) return;
                switch (param)
                {
                case 0: c.setGrainPosition(v); break;
                case 1: c.setGrainSize(v); break;
                case 2: c.setGrainDensity(v); break;
                default: c.setGrainSpray(v); break;
                } });
        }

        // --- PartType setters ---
        void setOscPartType(int osc, PartType pt)
        {
            m_engine.getAllocator().configureVoices([osc, pt](auto &v)
                                                    { v.getChain(osc).setPartType(pt); });
        }

        // --- WaveTable setters ---
        void loadWtBank(int osc, const std::string &path, size_t samplesPerWave = 0)
        {
            m_engine.getAllocator().configureVoices([osc, path, samplesPerWave](auto &v)
                                                    { v.getChain(osc).loadWtBank(path, samplesPerWave); });
        }
        void setOscWtMorphPos(int osc, float pos)
        {
            m_engine.getAllocator().configureChains([osc, pos](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setWtMorphPosition(pos); });
        }

        // --- FM setters ---
        void setOscFMAlgorithm(int osc, FMAlgorithm algo)
        {
            m_engine.getAllocator().configureChains([osc, algo](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMAlgorithm(algo); });
        }
        void setOscFMModulatorRatio(int osc, float r)
        {
            m_engine.getAllocator().configureChains([osc, r](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMModulatorRatio(r); });
        }
        void setOscFMModulationIndex(int osc, float idx)
        {
            m_engine.getAllocator().configureChains([osc, idx](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMModulationIndex(idx); });
        }
        void setOscFMFeedback(int osc, float fb)
        {
            m_engine.getAllocator().configureChains([osc, fb](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMFeedback(fb); });
        }
        void setOscFMOperatorRatio(int osc, int op, float r)
        {
            m_engine.getAllocator().configureChains([osc, op, r](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMOperatorRatio(op, r); });
        }
        void setOscFMOperatorLevel(int osc, int op, float l)
        {
            m_engine.getAllocator().configureChains([osc, op, l](OscillatorChain &c, int i)
                                                    { if (i == osc) c.setFMOperatorLevel(op, l); });
        }

        // --- Master effects setters ---
        void setDrive(float v)
        {
            m_baseDrive = v;
            m_masterSaturation.setDrive(v);
        }
        void setAsymmetry(float v)
        {
            m_baseAsymmetry = v;
            m_masterSaturation.setAsymmetry(v);
        }
        void setChaos(float v)
        {
            m_baseChaos = v;
            m_masterSaturation.setChaos(v);
        }
        void setTubeMix(float v) { m_masterSaturation.setMix(v); }
        void setTubeModel(TubeModel m) { m_masterSaturation.setModel(m); }
        void setModEnv(int i, float a, float d, float s, float r)
        {
            if (i >= 0 && i < NUM_MOD_ENVS)
            {
                m_modEnvs[i].setADSR(a, d, s, r); // kept as the setting; the voices play it
                m_engine.getAllocator().configureVoices([&](auto &v) { v.setModEnvADSR(i, a, d, s, r); });
            }
        }
        void setDelayTimeL(float s) { m_masterDelay.setDelayTimeL(s); }
        void setDelayTimeR(float s) { m_masterDelay.setDelayTimeR(s); }
        void setDelayMix(float v) { m_masterDelay.setMix(v); }
        void setDelayFeedback(float v) { m_masterDelay.setFeedback(v); }
        void setDelayDamping(float v) { m_masterDelay.setDamping(v); }
        void setDelayPingPong(bool v) { m_masterDelay.setPingPong(v); }
        void setLFORate(int idx, float hz)
        {
            if (idx >= 0 && idx < NUM_LFOS)
            {
                m_lfoRate[idx] = hz;
                m_lfos[idx].setFrequency(hz * std::exp2(m_lfoRateMod[idx] * 3.0f));
            }
        }
        void setMasterVolume(float v) { m_masterVolume = v; }

        // Master filter (post-mix, pre-delay)
        void setMasterFilterCutoff(float hz)
        {
            hz = std::clamp(hz, 20.0f, 20000.0f);
            m_masterFilterCutoff = hz;
            m_masterFilterL.setCutoff(hz);
            m_masterFilterR.setCutoff(hz);
        }
        void setMasterFilterResonance(float r)
        {
            r = std::clamp(r, 0.0f, 1.0f);
            m_masterFilterReso = r;
            m_masterFilterL.setResonance(r);
            m_masterFilterR.setResonance(r);
        }
        void setMasterFilterType(BiquadType t)
        {
            m_masterFilterType = t;
            m_masterFilterL.setType(t);
            m_masterFilterR.setType(t);
        }
        void setMasterFilterBypassed(bool b) { m_masterFilterBypassed = b; }
        float getMasterFilterCutoff() const { return m_masterFilterCutoff; }
        float getMasterFilterResonance() const { return m_masterFilterReso; }
        BiquadType getMasterFilterType() const { return m_masterFilterType; }
        bool isMasterFilterBypassed() const { return m_masterFilterBypassed; }

        float getDrive() const { return m_baseDrive; }
        float getAsymmetry() const { return m_baseAsymmetry; }
        float getChaos() const { return m_baseChaos; }
        float getMasterVolume() const { return m_masterVolume; }
        /// The limiter's deepest gain since the last call (1: it did nothing), for a meter or a test
        float takeLimiterLowest()
        {
            return m_output.takeLimiterLowest();
        }

        /**
         * @brief Cycle the master effect slot to the next effect type
         */
        void cycleMasterFX()
        {
            setMasterEffect(nextEffectType(m_masterFX.getEffectType()));
        }

        /// The master effect: one per side, at the rate it runs (oversampled). Not on the audio thread (it allocates).
        void setMasterEffect(EffectType type)
        {
            if (type == EffectType::NONE)
            {
                m_masterFX.clearEffect();
                m_masterFXR.clearEffect();
                return;
            }
            m_masterFX.setEffect(createEffect(type, m_sampleRate * static_cast<float>(m_os)));
            m_masterFXR.setEffect(createEffect(type, m_sampleRate * static_cast<float>(m_os)));
        }
        void setMasterFXParameter(int index, float value)
        {
            m_masterFX.setParameter(index, value);
            m_masterFXR.setParameter(index, value);
        }
        void setMasterFXMix(float mix)
        {
            m_masterFX.setMix(mix);
            m_masterFXR.setMix(mix);
        }

    private:

    private:
        SynthEngine m_engine;
        LFO m_lfos[NUM_LFOS];
        EffectSlot m_masterFX;  // left (and what getMasterFX shows)
        EffectSlot m_masterFXR; // right
        std::vector<float> m_fxL, m_fxR;
        TubeSaturation m_masterSaturation;
        StereoDelay m_masterDelay;
        BiquadFilter m_masterFilterL;
        BiquadFilter m_masterFilterR;
        StereoDecimator m_decimator;      // the anti-alias low-pass and the way down from the oversampled rate
        OutputStage m_output{48000.0f};   // DC, volume and balance, limiter
        std::vector<float> m_osBuffer; // oversampled render buffer
        bool m_masterFilterBypassed{false};
        float m_masterFilterCutoff{20000.0f};
        float m_masterFilterReso{0.0f};
        BiquadType m_masterFilterType{BiquadType::LOWPASS};
        ADSREnvelope m_modEnvs[NUM_MOD_ENVS];
        float m_sampleRate;
        int m_os = OVERSAMPLE; // the render rate's multiplier: 2, or 1 on a small machine (the constructor's)
        std::atomic<float> m_lastAudioMs{0.0f};
        std::atomic<float> m_lastAudioCpuPct{0.0f};
        float m_baseDrive{1.8f};
        float m_baseAsymmetry{0.3f};
        float m_baseChaos{0.0f};
        float m_masterVolume{0.8f};
        float m_tubeMod[3]{};
        float m_coarse[4]{}, m_fine[4]{};
        float m_lfoRate[NUM_LFOS]{2.0f, 0.8f, 0.3f, 5.0f}; // the LFOs' rates, before the matrix
        float m_lfoRateMod[NUM_LFOS]{};
        int m_currentPreset{0};
        const char *m_currentPresetName{"Init"};
        const Preset4D *m_currentPresetData{nullptr};
    };

} // namespace synth
