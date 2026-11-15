#include "main_screen.hpp"
#include "../common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/audio/audio_backend.hpp"
#include "../lib/synth/core/effect_factory.hpp"
#include "imgui_theme.hpp"
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <gl/GL.h>

namespace synth
{

    SynthViewer::SynthViewer()
        : m_synth((float)SAMPLE_RATE)
    {
        m_faderPicked.fill(false);
        m_lastHwFaderNorm.fill(0.0f);
        m_modSlotAmounts.fill(0.0f);
    }

    SynthViewer::~SynthViewer()
    {
        for (auto &dev : m_midiDevices)
            dev->close();
        m_midiDevices.clear();
        m_audioSink.stop();
        m_audioSink.cleanup();
    }

    bool SynthViewer::initWindow()
    {
        // Tall enough that each OSC column fits knobs/envs without scrolling
        if (!m_window.create("Synth Viewer", 1920, 1180))
        {
            MessageBoxW(nullptr, L"Failed to create OpenGL window", L"Error", MB_OK);
            return false;
        }
        m_window.setKeyCallback([this](int vk)
                                { onKeyPress(vk); });
        m_window.setMouseCallback([this](int btn, int mx, int my)
                                  { onMouseClick(btn, mx, my); });
        m_window.setMouseMoveCallback([this](int mx, int my)
                                      { onMouseMove(mx, my); });
        m_window.setMouseUpCallback([this](int btn, int mx, int my)
                                    { onMouseUp(btn, mx, my); });
        m_window.setWheelCallback([this](int delta, int mx, int my)
                                  { onMouseWheel(delta, mx, my); });
        return true;
    }

    void SynthViewer::initAudio()
    {
        m_wave = buildWaveform(1024);
        m_envShape = buildEnvelopeShape(1.2f, 2048);
        // Use ASIO with VB-Audio Matrix VASIO-32 driver for lowest latency
        selectAsioDriverByName("VASIO-32");
        m_audioSink.setBackend(AudioBackendType::ASIO);
        int audioResult = m_audioSink.init();
        if (audioResult != 0)
        {
            std::cerr << "[AUDIO] ASIO failed, trying DirectSound..." << std::endl;
            m_audioSink.setBackend(AudioBackendType::DirectSound);
            audioResult = m_audioSink.init();
        }
        if (audioResult != 0)
        {
            std::cerr << "[AUDIO] DirectSound failed, trying WaveOut..." << std::endl;
            m_audioSink.setBackend(AudioBackendType::WaveOut);
            audioResult = m_audioSink.init();
        }
        std::cerr << "[AUDIO] init result: " << audioResult << std::endl;
        // bufferSize is FRAMES (samples per channel), matching ASIO/WaveOut/ALSA/Pulse
        m_audioSink.setAudioCallback([this](float *buffer, int frames, int channels)
                                     {
            if (channels == 2)
            {
                m_synth.generateStereoBuffer(buffer, frames);
                m_waveCapture.writeStereo(buffer, frames);
            }
            else
                m_synth.generateBuffer(buffer, frames); });
        int startResult = m_audioSink.start();
        std::cerr << "[AUDIO] start result: " << startResult << std::endl;
    }

    void SynthViewer::initUIValues()
    {
        auto normRange = [](float v, float lo, float hi)
        { return (hi - lo) > 0.f ? (v - lo) / (hi - lo) : 0.f; };
        // Pull actual current synth parameter values
        float a = 0;   // m_voice.getAttackTime();   // expected range 0.005 .. 1.0
        float d = 0;   // m_voice.getDecayTime();    // expected range 0.005 .. 2.0
        float s = 0;   // m_voice.getSustainLevel(); // expected range 0.05 .. 1.0
        float r = 0;   // m_voice.getReleaseTime();  // expected range 0.01 .. 3.0
        float amp = 0; // m_voice.getAmplitude();  // expected range 0 .. 1

        m_faderVals[0] = normRange(a, 0.005f, 1.0f);
        m_faderVals[1] = normRange(d, 0.005f, 2.0f);
        m_faderVals[2] = normRange(s, 0.05f, 1.0f);
        m_faderVals[3] = normRange(r, 0.01f, 3.0f);
        m_faderVals[8] = normRange(amp, 0.0f, 1.0f);

        for (int i = 5; i < FADER_COUNT - 1; ++i)
            m_faderVals[i] = 0.0f;
        // Encoder pickup initial values (indices 9-16 for CC 21-28)
        // Pan knobs start at center (0.5 in normalized 0..1 space = pan 0.0)
        m_faderVals[9] = 0.5f;  // CC21 Osc1 Pan center
        m_faderVals[10] = 0.5f; // CC22 Osc2 Pan center
        m_faderVals[11] = 0.5f; // CC23 Osc3 Pan center
        m_faderVals[12] = 0.5f; // CC24 Osc4 Pan center
        m_faderVals[13] = 0.0f; // CC25 LFO1 Rate (min)
        m_faderVals[14] = 0.0f; // CC26 Delay FB
        m_faderVals[15] = 0.0f; // CC27 Tube Drive
        m_faderVals[16] = 0.0f; // CC28 Tube Mix
        m_faderVals[17] = 0.0f; // CC29 Tube Chaos
        for (int i = 0; i < KNOB_COUNT; ++i)
            m_knobVals[i] = (float)i / 15.0f;
        // Seed hardware last positions to GUI values so pickup requires crossing
        for (int i = 0; i < FADER_COUNT; ++i)
            m_lastHwFaderNorm[i] = m_faderVals[i];
    }

    void SynthViewer::initMIDI()
    {
        // Probe device count using a temporary instance
        {
            MidiInterface probe;
            if (probe.init() != 0)
                return;
        }
        int devices = MidiInterface::getDeviceCount();
        if (devices <= 0)
            return;

        // Helper: extract the "base name" of a MIDI device.
        // WinMM often exposes the same physical device twice:
        //   "Foo"  and  "MIDIIN2 (Foo)"
        // We keep only the first occurrence of each base name.
        auto baseName = [](const std::string &n) -> std::string
        {
            // WinMM duplicate pattern: "MIDIIN" + digits + " (" + original + ")"
            const std::string pfx = "MIDIIN";
            if (n.size() > pfx.size() + 3 && n.substr(0, pfx.size()) == pfx)
            {
                size_t k = pfx.size();
                while (k < n.size() && std::isdigit((unsigned char)n[k]))
                    ++k;
                if (k > pfx.size() && k + 2 < n.size() &&
                    n[k] == ' ' && n[k + 1] == '(')
                {
                    auto close = n.rfind(')');
                    if (close != std::string::npos && close > k + 2)
                        return n.substr(k + 2, close - k - 2);
                }
            }
            return n;
        };

        std::vector<std::string> openedBaseNames;

        // Prefer controller-like names first (Launchkey / Launchpad / APC), then others.
        std::vector<int> order;
        order.reserve(static_cast<size_t>(devices));
        auto score = [](const std::string &n) -> int
        {
            std::string u = n;
            for (char &c : u)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (u.find("LAUNCH") != std::string::npos)
                return 0;
            if (u.find("APC") != std::string::npos)
                return 1;
            if (u.find("KEY") != std::string::npos)
                return 2;
            return 10;
        };
        for (int i = 0; i < devices; ++i)
            order.push_back(i);
        std::sort(order.begin(), order.end(), [&](int a, int b)
                  {
            int sa = score(MidiInterface::getDeviceName(a));
            int sb = score(MidiInterface::getDeviceName(b));
            if (sa != sb) return sa < sb;
            return a < b; });

        // Open every real MIDI endpoint; skip WinMM duplicates.
        for (int i : order)
        {
            std::string name = MidiInterface::getDeviceName(i);
            std::string base = baseName(name);

            bool dup = false;
            for (auto &b : openedBaseNames)
                if (b == base)
                {
                    dup = true;
                    break;
                }
            // On Linux each subdevice has a distinct path in the name — keep them all.
            // Only skip true WinMM-style duplicates.
            if (dup && name.find("MIDIIN") == 0)
            {
                std::fprintf(stderr, "[MIDI] Skipping duplicate device %d: %s\n", i, name.c_str());
                continue;
            }

            std::fprintf(stderr, "[MIDI] Device %d: %s\n", i, name.c_str());

            auto dev = std::make_unique<MidiInterface>();
            if (dev->init() != 0)
            {
                std::fprintf(stderr, "[MIDI] init() failed for device %d\n", i);
                continue;
            }

            dev->setNoteCallback([this](const MidiNote &n)
                                 { onMidiNote(n); });
            dev->setControlCallback([this, devId = i, name = name](uint8_t ch, uint8_t cc, uint8_t val)
                                    {
                std::fprintf(stderr, "[MIDI CC] #%d %-20s  ch=%d  cc=%3d  val=%3d\n",
                             devId, name.c_str(), (int)ch, (int)cc, (int)val);
                onMidiControl(cc, val); });
            dev->setChannelAftertouchCallback([this](uint8_t /*ch*/, uint8_t pressure)
                                              { m_synth.setChannelAftertouch((float)pressure / 127.0f); });
            dev->setPitchBendCallback([this](uint8_t /*ch*/, int value)
                                      {
                // value is typically -8192..+8191 after parseMidiMessage pitch bend
                float semitones = (float)value / 8192.0f * 2.0f;
                m_synth.setPitchBend(semitones); });

            if (dev->open(i) == 0)
            {
                std::fprintf(stderr, "[MIDI] Opened device %d: %s\n", i, name.c_str());
                openedBaseNames.push_back(base);
                m_midiDevices.push_back(std::move(dev));
            }
            else
            {
                std::fprintf(stderr, "[MIDI] Failed to open device %d: %s\n", i, name.c_str());
            }
        }

        // Keyboard fallback only if nothing real opened (and only one instance)
        if (m_midiDevices.empty())
        {
            std::fprintf(stderr, "[MIDI] No hardware MIDI ports — keyboard fallback\n");
            auto dev = std::make_unique<MidiInterface>();
            if (dev->init() == 0)
            {
                dev->setNoteCallback([this](const MidiNote &n)
                                     { onMidiNote(n); });
                if (dev->open(-1) == 0)
                    m_midiDevices.push_back(std::move(dev));
            }
        }

        std::fprintf(stderr, "[MIDI] %d device(s) active\n", (int)m_midiDevices.size());
    }

    std::vector<Point> SynthViewer::buildWaveform(int /*samples*/)
    {
        std::vector<Point> pts;
        // pts.reserve(samples);
        // float inv = 1.0f / (samples - 1);
        // Oscillator *rawOsc = m_voice.getOscillator();
        // // IMPORTANT: Do not advance the live oscillator here; visualization should
        // // be side-effect free. Use peekSampleAtOffset to avoid injecting discontinuities.
        // for (int i = 0; i < samples; ++i)
        // {
        //     float s = rawOsc ? rawOsc->peekSampleAtOffset(i) : 0.0f;
        //     pts.push_back({i * inv, s});
        // }
        return pts;
    }

    std::vector<Point> SynthViewer::buildEnvelopeShape(float /*noteDurSec*/, int /*res*/)
    {
        std::vector<Point> pts;
        // if (res <= 1)
        //     return pts;
        // pts.reserve(res);
        // const ADSREnvelope *env = m_voice.getEnvelope();
        // if (!env)
        //     return pts;
        // int total = (int)(noteDurSec * env->getSampleRate());
        // if (total <= 0)
        //     return pts;
        // int rel = (int)(total * 0.6f);
        // int stride = total / (res - 1);
        // if (stride < 1)
        //     stride = 1;
        // for (int i = 0, captured = 0; i < total; ++i)
        // {
        //     float a = env->peekSampleAt(i, rel);
        //     if (i % stride == 0 && captured < res)
        //     {
        //         float x = (float)captured / (float)(res - 1);
        //         pts.push_back({x, a});
        //         ++captured;
        //     }
        // }
        // if (!pts.empty() && (int)pts.size() < res)
        //     pts.push_back({1.0f, pts.back().y});
        return pts;
    }

    void SynthViewer::onMidiNote(const MidiNote &note)
    {
        std::cerr << "[MIDI] note=" << (int)note.note << " vel=" << (int)note.velocity
                  << " on=" << note.is_note_on << std::endl;
        if (note.is_note_on)
            m_synth.noteOn(note.note, (float)note.velocity / 127.0f);
        else
            m_synth.noteOff(note.note);

        m_wave = buildWaveform(1024);
    }

    void SynthViewer::onMidiControl(uint8_t controller, uint8_t value)
    {
        float hwNorm = (float)value / 127.0f;

        // ─── MIDI Learn: assign CC to the currently targeted knob ───────────
        if (m_midiLearnActive && m_midiLearnTarget.isValid())
        {
            m_midiLearnMap.assign(controller, m_midiLearnTarget);
            std::fprintf(stderr, "[MIDILearn] CC%d -> osc=%d param=%d\n",
                         controller, m_midiLearnTarget.oscIdx, m_midiLearnTarget.paramIdx);
            m_midiLearnTarget = {-1, -1};
            m_midiLearnActive = false;
            return;
        }

        // ─── MIDI Map: apply mapped CC to parameter ─────────────────────────
        {
            MidiLearnTarget tgt = m_midiLearnMap.findByCC(controller);
            if (tgt.isValid())
            {
                applyMidiLearnCC(controller, hwNorm);
                return;
            }
        }

        auto tryPickup = [&](int idx, float hw, auto applyParam)
        {
            // If not yet picked, test if hardware crossed the current GUI value
            if (!m_faderPicked[idx])
            {
                float guiVal = m_faderVals[idx];
                float lastHw = m_lastHwFaderNorm[idx];
                // Crossing detection: (last - gui)*(current - gui) <= 0 OR within threshold distance
                bool crossed = (lastHw - guiVal) * (hw - guiVal) <= 0.0f;
                bool closeEnough = std::fabs(hw - guiVal) <= PICKUP_THRESHOLD;
                if (crossed || closeEnough)
                {
                    m_faderPicked[idx] = true; // latch
                }
            }
            m_lastHwFaderNorm[idx] = hw;
            if (m_faderPicked[idx])
            {
                // Apply parameter & mirror GUI value to hardware
                applyParam();
                m_faderVals[idx] = hw;
            }
        };

        // switch (controller)
        // {
        // // ============ Mod Wheel ============
        // case 1:
        //     m_synth.setModWheel(hwNorm);
        //     std::fprintf(stderr, "[CC1] ModWheel=%.3f\n", hwNorm);
        //     break;

        // // ============ Encoders (CC 21-28) ============
        // // K1-K4: Per-oscillator Pan
        // case 21:
        //     tryPickup(9, hwNorm, [&]()
        //               {
        //         float pan = hwNorm * 2.0f - 1.0f;
        //         m_synth.getEngine().getAllocator().configureVoices([pan](auto &voice)
        //                                                            { voice.setPan(0, pan); });
        //         std::fprintf(stderr, "[CC21] Osc1 Pan=%.3f\n", pan); });
        //     break;
        // case 22:
        //     tryPickup(10, hwNorm, [&]()
        //               {
        //         float pan = hwNorm * 2.0f - 1.0f;
        //         m_synth.getEngine().getAllocator().configureVoices([pan](auto &voice)
        //                                                            { voice.setPan(1, pan); });
        //         std::fprintf(stderr, "[CC22] Osc2 Pan=%.3f\n", pan); });
        //     break;
        // case 23:
        //     tryPickup(11, hwNorm, [&]()
        //               {
        //         float pan = hwNorm * 2.0f - 1.0f;
        //         m_synth.getEngine().getAllocator().configureVoices([pan](auto &voice)
        //                                                            { voice.setPan(2, pan); });
        //         std::fprintf(stderr, "[CC23] Osc3 Pan=%.3f\n", pan); });
        //     break;
        // case 24:
        //     tryPickup(12, hwNorm, [&]()
        //               {
        //         float pan = hwNorm * 2.0f - 1.0f;
        //         m_synth.getEngine().getAllocator().configureVoices([pan](auto &voice)
        //                                                            { voice.setPan(3, pan); });
        //         std::fprintf(stderr, "[CC24] Osc4 Pan=%.3f\n", pan); });
        //     break;
        // // K5: LFO1 Rate (0.1 - 20 Hz)
        // case 25:
        //     tryPickup(13, hwNorm, [&]()
        //               {
        //         m_synth.getLFO(0).setFrequency(0.1f + hwNorm * 19.9f);
        //         std::fprintf(stderr, "[CC25] LFO1 Rate=%.3f\n", 0.1f + hwNorm * 19.9f); });
        //     break;
        // // K6: Delay Feedback (0 - 0.95)
        // case 26:
        //     tryPickup(14, hwNorm, [&]()
        //               {
        //         m_synth.getMasterDelay().setFeedback(hwNorm * 0.95f);
        //         std::fprintf(stderr, "[CC26] Delay FB=%.3f\n", hwNorm * 0.95f); });
        //     break;
        // // K7: Tube Drive (0.5 - 8.0)
        // case 27:
        //     tryPickup(15, hwNorm, [&]()
        //               {
        //         m_synth.getMasterSaturation().setDrive(0.5f + hwNorm * 7.5f);
        //         std::fprintf(stderr, "[CC27] Tube Drive=%.3f\n", 0.5f + hwNorm * 7.5f); });
        //     break;
        // // K8: Tube Mix (0 - 1)
        // case 28:
        //     tryPickup(16, hwNorm, [&]()
        //               {
        //         m_synth.getMasterSaturation().setMix(hwNorm);
        //         std::fprintf(stderr, "[CC28] Tube Mix=%.3f\n", hwNorm); });
        //     break;
        // // K9: Tube Chaos (0 - 1)
        // case 29:
        //     tryPickup(17, hwNorm, [&]()
        //               {
        //         m_synth.getMasterSaturation().setChaos(hwNorm);
        //         std::fprintf(stderr, "[CC29] Tube Chaos=%.3f\n", hwNorm); });
        //     break;

        // // ============ Faders (CC 71-79) ============
        // // F1-F4: Per-oscillator Level
        // case 71:
        //     m_synth.getEngine().getAllocator().configureVoices([hwNorm](auto &voice)
        //                                                        { voice.setLevel(0, hwNorm); });
        //     std::fprintf(stderr, "[CC71] Osc1 Level=%.3f\n", hwNorm);
        //     break;
        // case 72:
        //     m_synth.getEngine().getAllocator().configureVoices([hwNorm](auto &voice)
        //                                                        { voice.setLevel(1, hwNorm); });
        //     std::fprintf(stderr, "[CC72] Osc2 Level=%.3f\n", hwNorm);
        //     break;
        // case 73:
        //     m_synth.getEngine().getAllocator().configureVoices([hwNorm](auto &voice)
        //                                                        { voice.setLevel(2, hwNorm); });
        //     std::fprintf(stderr, "[CC73] Osc3 Level=%.3f\n", hwNorm);
        //     break;
        // case 74:
        //     m_synth.getEngine().getAllocator().configureVoices([hwNorm](auto &voice)
        //                                                        { voice.setLevel(3, hwNorm); });
        //     std::fprintf(stderr, "[CC74] Osc4 Level=%.3f\n", hwNorm);
        //     break;
        // // CC34-37: WT Morph per OSC 1-4
        // case 34:
        // case 35:
        // case 36:
        // case 37:
        // {
        //     int osc = (int)controller - 34;
        //     m_oscKnobValues[osc][OSC_WT_MORPH] = hwNorm;
        //     applyOscKnobValue(osc, OSC_WT_MORPH);
        //     std::fprintf(stderr, "[CC%d] OSC%d WT Morph=%.3f\n", (int)controller, osc + 1, hwNorm);
        //     break;
        // }

        // // CC30-33: Assignable mod matrix sources CC_A – CC_D
        // case 30:
        //     m_synth.getModMatrix().setSourceValue((int)ObsSynthSrc::CC_A, hwNorm);
        //     std::fprintf(stderr, "[CC30] CC_A=%.3f\n", hwNorm);
        //     break;
        // case 31:
        //     m_synth.getModMatrix().setSourceValue((int)ObsSynthSrc::CC_B, hwNorm);
        //     std::fprintf(stderr, "[CC31] CC_B=%.3f\n", hwNorm);
        //     break;
        // case 32:
        //     m_synth.getModMatrix().setSourceValue((int)ObsSynthSrc::CC_C, hwNorm);
        //     std::fprintf(stderr, "[CC32] CC_C=%.3f\n", hwNorm);
        //     break;
        // case 33:
        //     m_synth.getModMatrix().setSourceValue((int)ObsSynthSrc::CC_D, hwNorm);
        //     std::fprintf(stderr, "[CC33] CC_D=%.3f\n", hwNorm);
        //     break;

        // // F5: Filter Cutoff (100 - 20000 Hz)
        // case 75:
        //     m_synth.setCutoff(100.0f + hwNorm * 19900.0f);
        //     std::fprintf(stderr, "[CC75] Cutoff=%.1f\n", 100.0f + hwNorm * 19900.0f);
        //     break;
        // // F6: Filter Resonance (0 - 1)
        // case 76:
        //     m_synth.setResonance(hwNorm);
        //     std::fprintf(stderr, "[CC76] Reso=%.3f\n", hwNorm);
        //     break;
        // // F7: Amp Attack (0.001 - 2s)
        // case 77:
        //     m_synth.getEngine().setAttackTime(0.001f + hwNorm * 2.0f);
        //     std::fprintf(stderr, "[CC77] Attack=%.3f\n", 0.001f + hwNorm * 2.0f);
        //     break;
        // // F8: Amp Release (0.01 - 5s)
        // case 78:
        //     m_synth.getEngine().setReleaseTime(0.01f + hwNorm * 5.0f);
        //     std::fprintf(stderr, "[CC78] Release=%.3f\n", 0.01f + hwNorm * 5.0f);
        //     break;
        // // F9: Delay Mix (0 - 1)
        // case 79:
        //     m_synth.getMasterDelay().setMix(hwNorm);
        //     std::fprintf(stderr, "[CC79] Delay Mix=%.3f\n", hwNorm);
        //     break;

        // default:
        //     break;
        // }
        // Rebuild envelope preview if any envelope param changed or amplitude adjusted after pickup
        m_envShape = buildEnvelopeShape(1.2f, 2048);
    }

    void SynthViewer::onKeyPress(int vk)
    {
        switch (vk)
        {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            m_synth.selectPreset(vk - '1');
            break;
        case VK_UP:
        {
            int next = m_synth.getCurrentPreset() - 1;
            if (next < 0)
                next = m_synth.getPresetCount() - 1;
            m_synth.selectPreset(next);
            syncKnobsFromEngine();
            break;
        }
        case VK_DOWN:
        {
            int next = (m_synth.getCurrentPreset() + 1) % m_synth.getPresetCount();
            m_synth.selectPreset(next);
            syncKnobsFromEngine();
            break;
        }
        // F1-F4: Cycle effect type on osc 1-4
        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        {
            int oscIdx = vk - VK_F1;
            auto &voice = m_synth.getEngine().getAllocator().getVoice(0);
            EffectType cur = voice.getChain(oscIdx).getEffectSlot().getEffectType();
            EffectType next = nextEffectType(cur);
            float sr = (float)SAMPLE_RATE;
            m_synth.getEngine().getAllocator().configureVoices([oscIdx, next, sr](auto &v)
                                                               {
                if (next == EffectType::NONE)
                    v.getChain(oscIdx).getEffectSlot().clearEffect();
                else
                    v.getChain(oscIdx).getEffectSlot().setEffect(createEffect(next, sr)); });
            std::cerr << "[FX] Osc" << (oscIdx + 1) << " -> " << effectTypeToString(next) << std::endl;
            break;
        }
        // F5: Cycle master effect slot (MFX)
        case VK_F5:
        {
            m_synth.cycleMasterFX();
            std::cerr << "[F5] MFX -> " << effectTypeToString(m_synth.getMasterFX().getEffectType()) << std::endl;
            break;
        }
        // T: Cycle tube model
        case 'T':
        {
            TubeModel cur = m_synth.getMasterSaturation().getModel();
            TubeModel next = nextTubeModel(cur);
            m_synth.getMasterSaturation().setModel(next);
            std::cerr << "[TUBE] Model -> " << tubeModelToStr(next) << std::endl;
            break;
        }
        // M: Toggle mod matrix overlay
        case 'M':
            m_showModMatrix = !m_showModMatrix;
            break;
        default:
            break;
        }
    }

    void SynthViewer::onMouseClick(int button, int mx, int my)
    {
        // Check signal-flow knobs first
        if (button == 0)
        {
            int knobIdx = hitTestKnob(mx, my);
            if (knobIdx >= 0)
            {
                m_dragKnobIdx = knobIdx;
                m_dragStartY = my;
                m_dragStartValue = getKnobValue(knobIdx);
                return;
            }
        }

        // Hit-test against mod matrix grid (only when visible)
        if (!m_showModMatrix)
            return;
        if (m_gridCellW <= 0 || m_gridCellH <= 0)
            return;
        int relX = mx - m_gridX;
        int relY = my - m_gridY;
        if (relX < 0 || relY < 0)
            return;
        int col = relX / m_gridCellW;
        int rowFromBottom = relY / m_gridCellH;
        if (col >= ModMatrixGrid::COLS || rowFromBottom >= ModMatrixGrid::ROWS)
            return;
        // Grid draws row 0 at top, GL y=0 at bottom → invert
        int row = ModMatrixGrid::ROWS - 1 - rowFromBottom;

        // Get source/dest IDs from grid definitions
        int srcId = static_cast<int>(GRID_ROWS[row].src_id);
        int dstId = static_cast<int>(GRID_COLS[col].dst_id);

        auto &mm = m_synth.getModMatrix();

        // Find existing slot for this source→dest pair
        int existingSlot = -1;
        for (int i = 0; i < MAX_MOD_SLOTS; ++i)
        {
            const auto *slot = mm.getSlot(i);
            if (slot && slot->enabled && slot->source == srcId && slot->destination == dstId)
            {
                existingSlot = i;
                break;
            }
        }

        if (button == 1) // Right click: remove
        {
            if (existingSlot >= 0)
                mm.removeRoute(existingSlot);
            return;
        }

        // Left click: cycle amount 0.1 → 0.2 → 0.3 → 0.4 → remove
        constexpr float AMOUNTS[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
                                     -0.1f, -0.2f, -0.3f, -0.4f, -0.5f, -0.6f, -0.7f, -0.8f, -0.9f, -1.0f};
        constexpr int NUM_AMOUNTS = 20;

        if (existingSlot < 0)
        {
            // No route exists → add with first amount
            mm.addRoute(srcId, dstId, AMOUNTS[0]);
        }
        else
        {
            const auto *slot = mm.getSlot(existingSlot);
            float cur = slot->amount;
            // Find current in cycle
            int nextIdx = 0;
            for (int i = 0; i < NUM_AMOUNTS; ++i)
            {
                if (std::fabs(cur - AMOUNTS[i]) < 0.01f)
                {
                    nextIdx = i + 1;
                    break;
                }
            }
            if (nextIdx >= NUM_AMOUNTS)
                mm.removeRoute(existingSlot); // cycle past last → remove
            else
                mm.setSlotAmount(existingSlot, AMOUNTS[nextIdx]);
        }
    }

    void SynthViewer::onMouseWheel(int delta, int mx, int my)
    {
        // Check if wheel is over a knob
        int knobIdx = hitTestKnob(mx, my);
        if (knobIdx >= 0)
        {
            const auto &def = getKnobDef(knobIdx);
            float step = (def.maxVal - def.minVal) * 0.01f;
            float newVal = getKnobValue(knobIdx) + (float)delta * step;
            if (newVal < def.minVal)
                newVal = def.minVal;
            if (newVal > def.maxVal)
                newVal = def.maxVal;
            setKnobValue(knobIdx, newVal);
            return;
        }

        // Fine-tune mod matrix route amounts with mouse wheel (±0.01 per tick)
        if (m_gridCellW <= 0 || m_gridCellH <= 0)
            return;
        int relX = mx - m_gridX;
        int relY = my - m_gridY;
        if (relX < 0 || relY < 0)
            return;
        int col = relX / m_gridCellW;
        int rowFromBottom = relY / m_gridCellH;
        if (col >= ModMatrixGrid::COLS || rowFromBottom >= ModMatrixGrid::ROWS)
            return;
        int row = ModMatrixGrid::ROWS - 1 - rowFromBottom;

        int srcId = static_cast<int>(GRID_ROWS[row].src_id);
        int dstId = static_cast<int>(GRID_COLS[col].dst_id);

        auto &mm = m_synth.getModMatrix();

        // Find existing slot for this source→dest pair
        int existingSlot = -1;
        for (int i = 0; i < MAX_MOD_SLOTS; ++i)
        {
            const auto *slot = mm.getSlot(i);
            if (slot && slot->enabled && slot->source == srcId && slot->destination == dstId)
            {
                existingSlot = i;
                break;
            }
        }

        constexpr float STEP = 0.01f;
        float change = (float)delta * STEP;

        if (existingSlot < 0)
        {
            // No route: create one with the wheel increment
            if (delta != 0)
                mm.addRoute(srcId, dstId, change);
        }
        else
        {
            float cur = mm.getSlot(existingSlot)->amount;
            float next = cur + change;
            // Clamp to [-1, 1]
            if (next > 1.0f)
                next = 1.0f;
            if (next < -1.0f)
                next = -1.0f;
            // Remove if crosses zero (within step tolerance)
            if (std::fabs(next) < STEP * 0.5f)
                mm.removeRoute(existingSlot);
            else
                mm.setSlotAmount(existingSlot, next);
        }
    }

    // ===================== Signal Flow Knob UI =====================

    void SynthViewer::initKnobValues()
    {
        for (int osc = 0; osc < NUM_OSCS; ++osc)
            for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                m_oscKnobValues[osc][p] = OSC_KNOB_DEFS[p].defaultVal;
        for (int p = 0; p < GLB_PARAM_COUNT; ++p)
            m_glbKnobValues[p] = GLB_KNOB_DEFS[p].defaultVal;

        // Reflect the current engine state so ImGui shows real values
        syncKnobsFromEngine();
    }

    void SynthViewer::syncKnobsFromEngine()
    {
        // Read per-osc values directly from voice 0 chains.
        // Voice 0 is always configured and reflects any CC changes made at runtime.
        auto &allocator = m_synth.getEngine().getAllocator();
        for (int osc = 0; osc < NUM_OSCS; ++osc)
        {
            const auto &chain = allocator.getVoice(0).getChain(osc);
            m_oscKnobValues[osc][OSC_WAVE] = (float)chain.getWaveType();
            // Decompose total tuning: Tune = nearest integer semitone, Fine = fractional (-0.5..+0.5)
            float totalTune = chain.getCoarseTune();
            m_oscKnobValues[osc][OSC_TUNE] = roundf(totalTune);
            m_oscKnobValues[osc][OSC_FINE_TUNE] = totalTune - roundf(totalTune);
            m_oscKnobValues[osc][OSC_CUTOFF] = chain.getCutoff();
            m_oscKnobValues[osc][OSC_RESO] = chain.getResonance();
            m_oscKnobValues[osc][OSC_HPF] = chain.getHPFCutoff();
            m_oscKnobValues[osc][OSC_FILT_TYPE] = (float)chain.getFilterType();
            m_oscKnobValues[osc][OSC_KEY_TRACK] = chain.getKeyTrackAmount();
            m_oscKnobValues[osc][OSC_FILT_ATK] = chain.getFilterAttack();
            m_oscKnobValues[osc][OSC_FILT_DEC] = chain.getFilterDecay();
            m_oscKnobValues[osc][OSC_FILT_SUS] = chain.getFilterSustain();
            m_oscKnobValues[osc][OSC_FILT_REL] = chain.getFilterRelease();
            m_oscKnobValues[osc][OSC_AMP_ATK] = chain.getAmpAttack();
            m_oscKnobValues[osc][OSC_AMP_DEC] = chain.getAmpDecay();
            m_oscKnobValues[osc][OSC_AMP_SUS] = chain.getAmpSustain();
            m_oscKnobValues[osc][OSC_AMP_REL] = chain.getAmpRelease();
            m_oscKnobValues[osc][OSC_LEVEL] = chain.getLevel();
            // chain pan is 0..1 (0=left, 0.5=center, 1=right); UI knob is -1..+1
            m_oscKnobValues[osc][OSC_PAN] = chain.getPan() * 2.0f - 1.0f;
            // FX params have no chain getter — leave at previous/default

            // PartType / WT / FM
            m_oscKnobValues[osc][OSC_PART_TYPE] = (float)chain.getPartType();
            m_oscKnobValues[osc][OSC_WT_MORPH] = chain.getWtMorphPosition();
            m_oscKnobValues[osc][OSC_FM_ALGORITHM] = (float)chain.getFMAlgorithm();
            m_oscKnobValues[osc][OSC_FM_MOD_RATIO] = chain.getFMModulatorRatio();
            m_oscKnobValues[osc][OSC_FM_MOD_INDEX] = chain.getFMModulationIndex();
            m_oscKnobValues[osc][OSC_FM_FEEDBACK] = chain.getFMFeedback();
            m_oscKnobValues[osc][OSC_FM_OP2_RATIO] = chain.getFMOperatorRatio(2);
            m_oscKnobValues[osc][OSC_FM_OP2_LEVEL] = chain.getFMOperatorLevel(2);
        }

        // Global — read from live master effect objects
        auto &delay = m_synth.getMasterDelay();
        m_glbKnobValues[GLB_DLY_L] = delay.getDelayTimeLSec();
        m_glbKnobValues[GLB_DLY_R] = delay.getDelayTimeRSec();
        m_glbKnobValues[GLB_DLY_FB] = delay.getFeedback();
        m_glbKnobValues[GLB_DLY_MIX] = delay.getMix();
        m_glbKnobValues[GLB_DLY_DMP] = delay.getDamping();
        m_glbKnobValues[GLB_DLY_PING] = delay.getPingPong() ? 1.0f : 0.0f;

        m_glbKnobValues[GLB_TUBE_DRV] = m_synth.getDrive();
        m_glbKnobValues[GLB_TUBE_ASYM] = m_synth.getAsymmetry();
        m_glbKnobValues[GLB_TUBE_MIX] = m_synth.getMasterSaturation().getMix();

        m_glbKnobValues[GLB_VOLUME] = m_synth.getMasterVolume();

        // Master Filter
        m_glbKnobValues[GLB_MF_CUTOFF] = m_synth.getMasterFilterCutoff();
        m_glbKnobValues[GLB_MF_RESO] = m_synth.getMasterFilterResonance();
        m_glbKnobValues[GLB_MF_TYPE] = (float)m_synth.getMasterFilterType();
        m_glbKnobValues[GLB_MF_BYPASS] = m_synth.isMasterFilterBypassed() ? 1.0f : 0.0f;
    }

    void SynthViewer::applyOscKnobValue(int osc, int param)
    {
        float v = m_oscKnobValues[osc][param];
        switch (param)
        {
        case OSC_WAVE:
            m_synth.setOscWaveType(osc, static_cast<WaveType>((int)v));
            break;
        case OSC_TUNE:
            // Snap Tune to integer semitones; Fine adds the fractional offset [0,1)
            m_oscKnobValues[osc][OSC_TUNE] = floorf(v + 0.5f); // round to nearest int
            m_synth.setOscCoarseTune(osc, m_oscKnobValues[osc][OSC_TUNE] + m_oscKnobValues[osc][OSC_FINE_TUNE]);
            break;
        case OSC_FINE_TUNE:
            // Fine is unipolar [0,1): adds fractional semitones on top of integer Tune
            m_synth.setOscCoarseTune(osc, m_oscKnobValues[osc][OSC_TUNE] + v);
            break;
        case OSC_CUTOFF:
            m_synth.setOscCutoff(osc, v);
            break;
        case OSC_RESO:
            m_synth.setOscResonance(osc, v);
            break;
        case OSC_HPF:
            m_synth.setOscHPFCutoff(osc, v);
            break;
        case OSC_FILT_TYPE:
            m_synth.setOscFilterType(osc, static_cast<BiquadType>((int)(v + 0.5f)));
            break;
        case OSC_KEY_TRACK:
            m_synth.setOscKeyTrack(osc, v);
            break;
        case OSC_FILT_ATK:
            m_synth.setOscFilterAttack(osc, v);
            break;
        case OSC_FILT_DEC:
            m_synth.setOscFilterDecay(osc, v);
            break;
        case OSC_FILT_SUS:
            m_synth.setOscFilterSustain(osc, v);
            break;
        case OSC_FILT_REL:
            m_synth.setOscFilterRelease(osc, v);
            break;
        case OSC_AMP_ATK:
            m_synth.setOscAmpAttack(osc, v);
            break;
        case OSC_AMP_DEC:
            m_synth.setOscAmpDecay(osc, v);
            break;
        case OSC_AMP_SUS:
            m_synth.setOscAmpSustain(osc, v);
            break;
        case OSC_AMP_REL:
            m_synth.setOscAmpRelease(osc, v);
            break;
        case OSC_FX_P1:
        case OSC_FX_P2:
        case OSC_FX_P3:
            // Effect params - future use
            break;
        case OSC_LEVEL:
            m_synth.setOscLevel(osc, v);
            break;
        case OSC_PAN:
            // UI knob is -1..+1; chain stores 0..1 (0=left, 0.5=center, 1=right)
            m_synth.setOscPan(osc, (v + 1.0f) * 0.5f);
            break;
        case OSC_PART_TYPE:
            m_synth.setOscPartType(osc, static_cast<PartType>((int)(v + 0.5f)));
            break;
        case OSC_WT_MORPH:
        {
            // v is [0..1] normalised; convert to table-space [0..tableCount-1]
            auto &chain = m_synth.getEngine().getAllocator().getVoice(0).getChain(osc);
            int tc = chain.getWtTableCount();
            float pos = v * (tc > 1 ? (float)(tc - 1) : 1.0f);
            m_synth.setOscWtMorphPos(osc, pos);
            break;
        }
        case OSC_FM_ALGORITHM:
            m_synth.setOscFMAlgorithm(osc, static_cast<FMAlgorithm>((int)(v + 0.5f)));
            break;
        case OSC_FM_MOD_RATIO:
            m_synth.setOscFMModulatorRatio(osc, v);
            break;
        case OSC_FM_MOD_INDEX:
            m_synth.setOscFMModulationIndex(osc, v);
            break;
        case OSC_FM_FEEDBACK:
            m_synth.setOscFMFeedback(osc, v);
            break;
        case OSC_FM_OP2_RATIO:
            m_synth.setOscFMOperatorRatio(osc, 2, v);
            break;
        case OSC_FM_OP2_LEVEL:
            m_synth.setOscFMOperatorLevel(osc, 2, v);
            break;
        }
    }

    void SynthViewer::applyGlbKnobValue(int param)
    {
        float v = m_glbKnobValues[param];
        switch (param)
        {
        case GLB_MF_CUTOFF:
            m_synth.setMasterFilterCutoff(v);
            break;
        case GLB_MF_RESO:
            m_synth.setMasterFilterResonance(v);
            break;
        case GLB_MF_TYPE:
            m_synth.setMasterFilterType(static_cast<BiquadType>((int)(v + 0.5f)));
            break;
        case GLB_MF_BYPASS:
            m_synth.setMasterFilterBypassed(v > 0.5f);
            break;
        case GLB_DLY_L:
            m_synth.setDelayTimeL(v);
            break;
        case GLB_DLY_R:
            m_synth.setDelayTimeR(v);
            break;
        case GLB_DLY_FB:
            m_synth.setDelayFeedback(v);
            break;
        case GLB_DLY_MIX:
            m_synth.setDelayMix(v);
            break;
        case GLB_DLY_PING:
            m_synth.setDelayPingPong(v > 0.5f);
            break;
        case GLB_DLY_DMP:
            m_synth.setDelayDamping(v);
            break;
        case GLB_TUBE_DRV:
            m_synth.setDrive(v);
            break;
        case GLB_TUBE_ASYM:
            m_synth.setAsymmetry(v);
            break;
        case GLB_TUBE_MIX:
            m_synth.setTubeMix(v);
            break;
        case GLB_MFX_P1:
        case GLB_MFX_P2:
        case GLB_MFX_P3:
            // MFX params - future use
            break;
        case GLB_VOLUME:
            m_synth.setMasterVolume(v);
            break;
        }
    }

    float SynthViewer::getKnobValue(int flatIdx) const
    {
        if (flatIdx < NUM_OSCS * OSC_PARAM_COUNT)
        {
            int osc = flatIdx / OSC_PARAM_COUNT;
            int param = flatIdx % OSC_PARAM_COUNT;
            return m_oscKnobValues[osc][param];
        }
        int glbParam = flatIdx - NUM_OSCS * OSC_PARAM_COUNT;
        return m_glbKnobValues[glbParam];
    }

    void SynthViewer::setKnobValue(int flatIdx, float v)
    {
        if (flatIdx < NUM_OSCS * OSC_PARAM_COUNT)
        {
            int osc = flatIdx / OSC_PARAM_COUNT;
            int param = flatIdx % OSC_PARAM_COUNT;
            m_oscKnobValues[osc][param] = v;
            applyOscKnobValue(osc, param);
        }
        else
        {
            int glbParam = flatIdx - NUM_OSCS * OSC_PARAM_COUNT;
            m_glbKnobValues[glbParam] = v;
            applyGlbKnobValue(glbParam);
        }
    }

    const SynthViewer::KnobDef &SynthViewer::getKnobDef(int flatIdx) const
    {
        if (flatIdx < NUM_OSCS * OSC_PARAM_COUNT)
            return OSC_KNOB_DEFS[flatIdx % OSC_PARAM_COUNT];
        return GLB_KNOB_DEFS[flatIdx - NUM_OSCS * OSC_PARAM_COUNT];
    }

    void SynthViewer::applyMidiLearnCC(uint8_t cc, float normalizedValue)
    {
        MidiLearnTarget tgt = m_midiLearnMap.findByCC(cc);
        if (!tgt.isValid())
            return;

        int oscIdx = tgt.oscIdx;
        int paramIdx = tgt.paramIdx;

        if (oscIdx >= 0 && oscIdx < NUM_OSCS && paramIdx >= 0 && paramIdx < OSC_PARAM_COUNT)
        {
            const auto &def = OSC_KNOB_DEFS[paramIdx];
            float scaled = def.minVal + normalizedValue * (def.maxVal - def.minVal);
            m_oscKnobValues[oscIdx][paramIdx] = scaled;
            applyOscKnobValue(oscIdx, paramIdx);
        }
    }

    int SynthViewer::hitTestKnob(int mx, int my) const
    {
        for (int i = 0; i < m_knobRectCount; ++i)
        {
            const auto &kr = m_knobRects[i];
            int cx = kr.x + kr.size / 2;
            int cy = kr.y + kr.size / 2;
            int dx = mx - cx;
            int dy = my - cy;
            int r = kr.size / 2 + 3;
            if (dx * dx + dy * dy <= r * r)
                return i;
        }
        return -1;
    }

    void SynthViewer::onMouseMove(int mx, int my)
    {
        if (m_dragKnobIdx >= 0)
        {
            int dy = my - m_dragStartY;
            float sensitivity = 200.0f;
            if (GetKeyState(VK_SHIFT) & 0x8000)
                sensitivity = 800.0f;
            const auto &def = getKnobDef(m_dragKnobIdx);
            float range = def.maxVal - def.minVal;
            float delta = (float)dy / sensitivity * range;
            float newVal = m_dragStartValue + delta;
            if (newVal < def.minVal)
                newVal = def.minVal;
            if (newVal > def.maxVal)
                newVal = def.maxVal;
            setKnobValue(m_dragKnobIdx, newVal);
        }
        else
        {
            m_hoverKnobIdx = hitTestKnob(mx, my);
        }
    }

    void SynthViewer::onMouseUp(int button, int mx, int my)
    {
        (void)button;
        (void)mx;
        (void)my;
        m_dragKnobIdx = -1;
    }

    // Helper: draw a material knob with label and register for hit-testing
    void SynthViewer::registerKnob(int kx, int ky, int sz, float val, float minV, float maxV,
                                   const char *label, int flatIdx, int oscIdx, int paramIdx)
    {
        bool isActive = (m_dragKnobIdx == flatIdx);
        bool isHovered = (m_hoverKnobIdx == flatIdx);
        m_renderer.drawMaterialKnob(kx, ky, sz, val, minV, maxV, isActive, isHovered);
        if (label)
            m_renderer.drawLabel(kx, ky + sz + 1, sz, 10, label);
        if (m_knobRectCount < MAX_KNOBS)
        {
            m_knobRects[m_knobRectCount] = {kx, ky, sz, oscIdx, paramIdx};
            m_knobRectCount++;
        }
    }

    void SynthViewer::drawOscPanel(int x, int y, int w, int h, int oscIdx)
    {
        m_renderer.drawPanelFrame(x, y, w, h, nullptr, 8);

        int K = KNOB_SZ;
        int pad = 6;
        int labelH = 12;
        int baseFlat = oscIdx * OSC_PARAM_COUNT;

        // === Left column: Wave, Tune, Fine Tune (vertically) ===
        int leftX = x + pad;
        int colW = K + 8;
        {
            int ky = y + h - pad - K - labelH;
            registerKnob(leftX, ky, K,
                         m_oscKnobValues[oscIdx][OSC_WAVE], OSC_KNOB_DEFS[OSC_WAVE].minVal, OSC_KNOB_DEFS[OSC_WAVE].maxVal,
                         "Wave", baseFlat + OSC_WAVE, oscIdx, OSC_WAVE);

            ky -= K + labelH + 4;
            registerKnob(leftX, ky, K,
                         m_oscKnobValues[oscIdx][OSC_TUNE], OSC_KNOB_DEFS[OSC_TUNE].minVal, OSC_KNOB_DEFS[OSC_TUNE].maxVal,
                         "Tune", baseFlat + OSC_TUNE, oscIdx, OSC_TUNE);

            ky -= K + labelH + 4;
            registerKnob(leftX, ky, K,
                         m_oscKnobValues[oscIdx][OSC_FINE_TUNE], OSC_KNOB_DEFS[OSC_FINE_TUNE].minVal, OSC_KNOB_DEFS[OSC_FINE_TUNE].maxVal,
                         "Fine", baseFlat + OSC_FINE_TUNE, oscIdx, OSC_FINE_TUNE);
        }

        // === Filter Env section (2x2 grid) ===
        int secX = leftX + colW + pad;
        int secW = K * 2 + pad;
        m_renderer.drawLabel(secX, y + h - 2, secW, labelH, "FILTER ENV");
        {
            int row1Y = y + h - pad - K - labelH - 4;
            int row2Y = row1Y - K - labelH - 2;
            // Row 1: Attack, Decay
            registerKnob(secX, row1Y, K,
                         m_oscKnobValues[oscIdx][OSC_FILT_ATK], OSC_KNOB_DEFS[OSC_FILT_ATK].minVal, OSC_KNOB_DEFS[OSC_FILT_ATK].maxVal,
                         "Atk", baseFlat + OSC_FILT_ATK, oscIdx, OSC_FILT_ATK);
            registerKnob(secX + K + pad, row1Y, K,
                         m_oscKnobValues[oscIdx][OSC_FILT_DEC], OSC_KNOB_DEFS[OSC_FILT_DEC].minVal, OSC_KNOB_DEFS[OSC_FILT_DEC].maxVal,
                         "Dec", baseFlat + OSC_FILT_DEC, oscIdx, OSC_FILT_DEC);
            // Row 2: Sustain, Release
            registerKnob(secX, row2Y, K,
                         m_oscKnobValues[oscIdx][OSC_FILT_SUS], OSC_KNOB_DEFS[OSC_FILT_SUS].minVal, OSC_KNOB_DEFS[OSC_FILT_SUS].maxVal,
                         "Sus", baseFlat + OSC_FILT_SUS, oscIdx, OSC_FILT_SUS);
            registerKnob(secX + K + pad, row2Y, K,
                         m_oscKnobValues[oscIdx][OSC_FILT_REL], OSC_KNOB_DEFS[OSC_FILT_REL].minVal, OSC_KNOB_DEFS[OSC_FILT_REL].maxVal,
                         "Rel", baseFlat + OSC_FILT_REL, oscIdx, OSC_FILT_REL);
        }

        // === Amp Env section (2x2 grid) ===
        int ampX = secX + secW + pad * 2;
        m_renderer.drawLabel(ampX, y + h - 2, secW, labelH, "AMP ENV");
        {
            int row1Y = y + h - pad - K - labelH - 4;
            int row2Y = row1Y - K - labelH - 2;
            registerKnob(ampX, row1Y, K,
                         m_oscKnobValues[oscIdx][OSC_AMP_ATK], OSC_KNOB_DEFS[OSC_AMP_ATK].minVal, OSC_KNOB_DEFS[OSC_AMP_ATK].maxVal,
                         "Atk", baseFlat + OSC_AMP_ATK, oscIdx, OSC_AMP_ATK);
            registerKnob(ampX + K + pad, row1Y, K,
                         m_oscKnobValues[oscIdx][OSC_AMP_DEC], OSC_KNOB_DEFS[OSC_AMP_DEC].minVal, OSC_KNOB_DEFS[OSC_AMP_DEC].maxVal,
                         "Dec", baseFlat + OSC_AMP_DEC, oscIdx, OSC_AMP_DEC);
            registerKnob(ampX, row2Y, K,
                         m_oscKnobValues[oscIdx][OSC_AMP_SUS], OSC_KNOB_DEFS[OSC_AMP_SUS].minVal, OSC_KNOB_DEFS[OSC_AMP_SUS].maxVal,
                         "Sus", baseFlat + OSC_AMP_SUS, oscIdx, OSC_AMP_SUS);
            registerKnob(ampX + K + pad, row2Y, K,
                         m_oscKnobValues[oscIdx][OSC_AMP_REL], OSC_KNOB_DEFS[OSC_AMP_REL].minVal, OSC_KNOB_DEFS[OSC_AMP_REL].maxVal,
                         "Rel", baseFlat + OSC_AMP_REL, oscIdx, OSC_AMP_REL);
        }

        // === Effect section (display + P1 P2 P3) ===
        int fxX = ampX + secW + pad * 2;
        int fxW = K * 3 + pad * 2 + 10;
        m_renderer.drawLabel(fxX, y + h - 2, fxW, labelH, "EFFECT");
        {
            // Effect display box (placeholder)
            int boxH = K + 4;
            int boxY = y + h - pad - boxH - labelH - 4;
            m_renderer.drawPanelFrame(fxX, boxY, fxW - 4, boxH, nullptr, 4);
            // P1, P2, P3 knobs below
            int pY = boxY - K - labelH - 4;
            for (int pi = 0; pi < 3; ++pi)
            {
                int px = fxX + pi * (K + pad);
                int pParam = OSC_FX_P1 + pi;
                registerKnob(px, pY, K,
                             m_oscKnobValues[oscIdx][pParam], OSC_KNOB_DEFS[pParam].minVal, OSC_KNOB_DEFS[pParam].maxVal,
                             OSC_KNOB_DEFS[pParam].label, baseFlat + pParam, oscIdx, pParam);
            }
        }

        // === Right column: Level, Pan ===
        int rightX = x + w - pad - K;
        {
            int ky = y + h - pad - K - labelH;
            registerKnob(rightX, ky, K,
                         m_oscKnobValues[oscIdx][OSC_LEVEL], OSC_KNOB_DEFS[OSC_LEVEL].minVal, OSC_KNOB_DEFS[OSC_LEVEL].maxVal,
                         "Level", baseFlat + OSC_LEVEL, oscIdx, OSC_LEVEL);

            ky -= K + labelH + 8;
            registerKnob(rightX, ky, K,
                         m_oscKnobValues[oscIdx][OSC_PAN], OSC_KNOB_DEFS[OSC_PAN].minVal, OSC_KNOB_DEFS[OSC_PAN].maxVal,
                         "Pan", baseFlat + OSC_PAN, oscIdx, OSC_PAN);
        }
    }

    void SynthViewer::drawMixerPanel(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "MIXER", 8);
        int pad = 8;
        int faderW = 20;
        int K = KNOB_SZ;
        int baseFlat = NUM_OSCS * OSC_PARAM_COUNT;
        // Reserve right side for master volume fader
        int volW = faderW + 16;
        int mixAreaW = w - pad * 2 - volW;
        int spacing = mixAreaW / NUM_OSCS;

        // Per-osc faders with pan knobs above
        int panKnobSz = K - 8;
        int panY = y + h - pad - panKnobSz - 2;
        int faderTop = panY - 6;
        int fy = y + 24;
        int fh = faderTop - fy - 4;

        for (int osc = 0; osc < NUM_OSCS; ++osc)
        {
            int cx = x + pad + osc * spacing + spacing / 2;
            // Pan knob (small, at top of mixer)
            int pkx = cx - panKnobSz / 2;
            int flatPan = osc * OSC_PARAM_COUNT + OSC_PAN;
            registerKnob(pkx, panY, panKnobSz,
                         m_oscKnobValues[osc][OSC_PAN], OSC_KNOB_DEFS[OSC_PAN].minVal, OSC_KNOB_DEFS[OSC_PAN].maxVal,
                         nullptr, flatPan, osc, OSC_PAN);
            // Level fader
            int fx = cx - faderW / 2;
            float lvl = m_oscKnobValues[osc][OSC_LEVEL];
            m_renderer.drawFader(fx, fy, faderW, fh, lvl, 0.0f, 1.0f, true);
        }

        // Master volume fader (rightmost)
        int volX = x + w - pad - volW + (volW - faderW) / 2;
        m_renderer.drawFader(volX, fy, faderW, fh, m_glbKnobValues[GLB_VOLUME], 0.0f, 1.0f, true);
        // Volume knob as interactive element
        registerKnob(volX - 4, panY, panKnobSz + 8,
                     m_glbKnobValues[GLB_VOLUME], GLB_KNOB_DEFS[GLB_VOLUME].minVal, GLB_KNOB_DEFS[GLB_VOLUME].maxVal,
                     "Vol", baseFlat + GLB_VOLUME, -1, GLB_VOLUME);
    }

    void SynthViewer::drawDelayPanel(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "STEREO DELAY", 8);
        int K = KNOB_SZ;
        int pad = 6;
        int baseFlat = NUM_OSCS * OSC_PARAM_COUNT;
        int col2 = K + pad;

        // Row 1: L, R
        int row1Y = y + h - pad - K - 10;
        registerKnob(x + pad, row1Y, K,
                     m_glbKnobValues[GLB_DLY_L], GLB_KNOB_DEFS[GLB_DLY_L].minVal, GLB_KNOB_DEFS[GLB_DLY_L].maxVal,
                     "L", baseFlat + GLB_DLY_L, -1, GLB_DLY_L);
        registerKnob(x + pad + col2, row1Y, K,
                     m_glbKnobValues[GLB_DLY_R], GLB_KNOB_DEFS[GLB_DLY_R].minVal, GLB_KNOB_DEFS[GLB_DLY_R].maxVal,
                     "R", baseFlat + GLB_DLY_R, -1, GLB_DLY_R);

        // Row 2: F.Back, Mix
        int row2Y = row1Y - K - 14;
        registerKnob(x + pad, row2Y, K,
                     m_glbKnobValues[GLB_DLY_FB], GLB_KNOB_DEFS[GLB_DLY_FB].minVal, GLB_KNOB_DEFS[GLB_DLY_FB].maxVal,
                     "F.Back", baseFlat + GLB_DLY_FB, -1, GLB_DLY_FB);
        registerKnob(x + pad + col2, row2Y, K,
                     m_glbKnobValues[GLB_DLY_MIX], GLB_KNOB_DEFS[GLB_DLY_MIX].minVal, GLB_KNOB_DEFS[GLB_DLY_MIX].maxVal,
                     "Mix", baseFlat + GLB_DLY_MIX, -1, GLB_DLY_MIX);

        // Row 3: Ping Pong, Damp
        int row3Y = row2Y - K - 14;
        registerKnob(x + pad, row3Y, K,
                     m_glbKnobValues[GLB_DLY_PING], GLB_KNOB_DEFS[GLB_DLY_PING].minVal, GLB_KNOB_DEFS[GLB_DLY_PING].maxVal,
                     "Ping", baseFlat + GLB_DLY_PING, -1, GLB_DLY_PING);
        registerKnob(x + pad + col2, row3Y, K,
                     m_glbKnobValues[GLB_DLY_DMP], GLB_KNOB_DEFS[GLB_DLY_DMP].minVal, GLB_KNOB_DEFS[GLB_DLY_DMP].maxVal,
                     "Damp", baseFlat + GLB_DLY_DMP, -1, GLB_DLY_DMP);
    }

    void SynthViewer::drawTubePanel(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "TUBE", 8);
        int K = KNOB_SZ;
        int pad = 8;
        int baseFlat = NUM_OSCS * OSC_PARAM_COUNT;
        int cx = x + (w - K) / 2;

        // Drive (top)
        int ky = y + h - pad - K - 10;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_TUBE_DRV], GLB_KNOB_DEFS[GLB_TUBE_DRV].minVal, GLB_KNOB_DEFS[GLB_TUBE_DRV].maxVal,
                     "Drive", baseFlat + GLB_TUBE_DRV, -1, GLB_TUBE_DRV);
        // Asymmetry (middle)
        ky -= K + 16;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_TUBE_ASYM], GLB_KNOB_DEFS[GLB_TUBE_ASYM].minVal, GLB_KNOB_DEFS[GLB_TUBE_ASYM].maxVal,
                     "Asym", baseFlat + GLB_TUBE_ASYM, -1, GLB_TUBE_ASYM);
        // Mix (bottom)
        ky -= K + 16;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_TUBE_MIX], GLB_KNOB_DEFS[GLB_TUBE_MIX].minVal, GLB_KNOB_DEFS[GLB_TUBE_MIX].maxVal,
                     "Mix", baseFlat + GLB_TUBE_MIX, -1, GLB_TUBE_MIX);
    }

    void SynthViewer::drawMFXPanel(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "MFX", 8);
        int K = KNOB_SZ;
        int pad = 8;
        int baseFlat = NUM_OSCS * OSC_PARAM_COUNT;
        int cx = x + (w - K) / 2;

        // P1 (top)
        int ky = y + h - pad - K - 10;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_MFX_P1], GLB_KNOB_DEFS[GLB_MFX_P1].minVal, GLB_KNOB_DEFS[GLB_MFX_P1].maxVal,
                     "Drive", baseFlat + GLB_MFX_P1, -1, GLB_MFX_P1);
        // P2 (middle)
        ky -= K + 16;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_MFX_P2], GLB_KNOB_DEFS[GLB_MFX_P2].minVal, GLB_KNOB_DEFS[GLB_MFX_P2].maxVal,
                     "Asym", baseFlat + GLB_MFX_P2, -1, GLB_MFX_P2);
        // P3 (bottom)
        ky -= K + 16;
        registerKnob(cx, ky, K,
                     m_glbKnobValues[GLB_MFX_P3], GLB_KNOB_DEFS[GLB_MFX_P3].minVal, GLB_KNOB_DEFS[GLB_MFX_P3].maxVal,
                     "Mix", baseFlat + GLB_MFX_P3, -1, GLB_MFX_P3);
    }

    void SynthViewer::drawMiniModMatrix(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "MOD MATRIX (IN USE)", 6);

        // Show only active mod routes as a compact list
        auto &mm = m_synth.getModMatrix();
        int lineH = 12;
        int pad = 6;
        int ty = y + h - 18; // start below title
        int maxLines = (h - 24) / lineH;
        int drawn = 0;

        for (int i = 0; i < MAX_MOD_SLOTS && drawn < maxLines; ++i)
        {
            const auto *slot = mm.getSlot(i);
            if (!slot || !slot->enabled)
                continue;

            // Draw a colored bar representing the route
            float amt = slot->amount;
            float absAmt = amt < 0.0f ? -amt : amt;

            // Bar background
            int bx = x + pad;
            int bw = w - pad * 2;
            int by = ty - lineH + 2;

            // Amount bar (green for positive, red for negative)
            if (amt > 0.0f)
                glColor4f(0.2f, 0.7f, 0.3f, 0.6f);
            else
                glColor4f(0.7f, 0.2f, 0.2f, 0.6f);

            int barW = (int)(absAmt * (float)bw);
            glViewport(bx, by, barW, lineH - 2);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glBegin(GL_QUADS);
            glVertex2f(-1.0f, -1.0f);
            glVertex2f(1.0f, -1.0f);
            glVertex2f(1.0f, 1.0f);
            glVertex2f(-1.0f, 1.0f);
            glEnd();

            ty -= lineH;
            drawn++;
        }

        if (drawn == 0)
        {
            m_renderer.drawLabel(x + pad, y + h / 2 - 6, w - pad * 2, 12, "No routes");
        }
    }

    // ===================== End Signal Flow Knob UI =====================

    void SynthViewer::drawModMatrixPanel(int x, int y, int w, int h)
    {
        m_renderer.drawPanelFrame(x, y, w, h, "MOD MATRIX", 14);

        // Sync grid from current mod matrix state (reads real source values)
        m_modGrid.syncFromMatrix(m_synth.getModMatrix());

        int padding = 16;
        int labelW = 60;  // Row label width (enough for "MODENV")
        int titleH = 24;  // Space reserved for panel title at top
        int headerH = 40; // Column headers height (group + sub-label)

        // Available drawing area (below title, above bottom padding)
        int gridX = x + padding + labelW;
        int gridW = w - padding * 2 - labelW;
        int gridH = h - padding * 2 - headerH - titleH;

        // Cell size (fit to available space)
        int cellW = gridW / ModMatrixGrid::COLS;
        int cellH = gridH / ModMatrixGrid::ROWS;
        if (cellW < 8)
            cellW = 8;
        if (cellH < 8)
            cellH = 8;
        if (cellW > 44)
            cellW = 44;
        if (cellH > 50)
            cellH = 50;

        // Header area starts below the panel title
        int headerY = y + h - padding - titleH - headerH;

        // Store grid geometry for mouse hit-testing
        int totalGridW = ModMatrixGrid::COLS * cellW;
        int totalGridH = ModMatrixGrid::ROWS * cellH;
        int gridDrawY = headerY - totalGridH;
        m_gridX = gridX;
        m_gridY = gridDrawY;
        m_gridCellW = cellW;
        m_gridCellH = cellH;

        // Draw column group headers (centered, capped width per char)
        for (int g = 0; g < m_modGrid.getNumGroups(); ++g)
        {
            const auto &grp = m_modGrid.getGroup(g);
            int gx = gridX + grp.start_col * cellW;
            int gw = grp.col_count * cellW;
            // Cap label width so text doesn't stretch (max ~10px per char)
            int labelLen = (int)std::strlen(grp.label);
            int maxLabelW = labelLen * 12;
            int drawW = (gw < maxLabelW) ? gw : maxLabelW;
            int drawX = gx + (gw - drawW) / 2; // center
            m_renderer.drawLabel(drawX, headerY + 22, drawW, 14, grp.label);
        }

        // Draw column sub-labels (P C R L per osc) - single char, small fixed width
        for (int c = 0; c < ModMatrixGrid::COLS; ++c)
        {
            int cx = gridX + c * cellW;
            // Center a small square for single-char labels
            int subW = (cellW < 14) ? cellW - 2 : 12;
            int subX = cx + (cellW - subW) / 2;
            m_renderer.drawLabel(subX, headerY + 2, subW, 14, m_modGrid.getColLabel(c));
        }

        // Set up viewport for the grid area (draw cells directly with GL)
        glViewport(gridX, gridDrawY, totalGridW, totalGridH);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(ModMatrixGrid::COLS), 0.0, static_cast<double>(ModMatrixGrid::ROWS), -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw grid background
        glBegin(GL_QUADS);
        glColor3f(0.06f, 0.06f, 0.08f);
        glVertex2f(0.0f, 0.0f);
        glVertex2f((float)ModMatrixGrid::COLS, 0.0f);
        glVertex2f((float)ModMatrixGrid::COLS, (float)ModMatrixGrid::ROWS);
        glVertex2f(0.0f, (float)ModMatrixGrid::ROWS);
        glEnd();

        // Draw cells as filled circles
        constexpr int ARC_SEGS = 12;
        constexpr float PI = 3.14159265f;
        constexpr float RADIUS = 0.28f; // smaller than cell half-size

        for (int r = 0; r < ModMatrixGrid::ROWS; ++r)
        {
            for (int c = 0; c < ModMatrixGrid::COLS; ++c)
            {
                const auto &cell = m_modGrid.getCell(r, c);
                float cx = (float)c + 0.5f;
                float cy = (float)(ModMatrixGrid::ROWS - 1 - r) + 0.5f;

                if (cell.enabled)
                {
                    float v = cell.value;
                    float intensity = std::fabs(v);
                    if (intensity > 1.0f)
                        intensity = 1.0f;
                    float bright = 0.25f + intensity * 0.75f;

                    if (v >= 0.0f)
                        glColor3f(0.0f, bright * 0.7f, bright * 0.95f);
                    else
                        glColor3f(bright * 0.95f, 0.0f, bright * 0.45f);

                    // Filled circle
                    glBegin(GL_TRIANGLE_FAN);
                    glVertex2f(cx, cy);
                    for (int s = 0; s <= ARC_SEGS; ++s)
                    {
                        float angle = 2.0f * PI * (float)s / (float)ARC_SEGS;
                        glVertex2f(cx + cosf(angle) * RADIUS, cy + sinf(angle) * RADIUS);
                    }
                    glEnd();
                }
                else
                {
                    // Empty cell: small dim dot
                    glColor3f(0.15f, 0.15f, 0.18f);
                    glBegin(GL_QUADS);
                    glVertex2f(cx - 0.05f, cy - 0.05f);
                    glVertex2f(cx + 0.05f, cy - 0.05f);
                    glVertex2f(cx + 0.05f, cy + 0.05f);
                    glVertex2f(cx - 0.05f, cy + 0.05f);
                    glEnd();
                }
            }
        }

        // Draw thin grid lines (every row and column for visual separation)
        glBegin(GL_LINES);
        glColor4f(0.2f, 0.2f, 0.28f, 0.5f);
        for (int r = 1; r < ModMatrixGrid::ROWS; ++r)
        {
            glVertex2f(0.0f, (float)r);
            glVertex2f((float)ModMatrixGrid::COLS, (float)r);
        }
        for (int c = 1; c < ModMatrixGrid::COLS; ++c)
        {
            glVertex2f((float)c, 0.0f);
            glVertex2f((float)c, (float)ModMatrixGrid::ROWS);
        }
        glEnd();

        // Draw group separator lines (brighter)
        glBegin(GL_LINES);
        glColor3f(0.35f, 0.35f, 0.5f);
        for (int g = 0; g < m_modGrid.getNumGroups(); ++g)
        {
            const auto &grp = m_modGrid.getGroup(g);
            float gx = (float)grp.start_col;
            glVertex2f(gx, 0.0f);
            glVertex2f(gx, (float)ModMatrixGrid::ROWS);
        }
        // Right edge
        glVertex2f((float)ModMatrixGrid::COLS, 0.0f);
        glVertex2f((float)ModMatrixGrid::COLS, (float)ModMatrixGrid::ROWS);
        glEnd();

        // Restore full window viewport
        glViewport(0, 0, m_window.width(), m_window.height());
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw row labels (using drawLabel with full window coords)
        for (int r = 0; r < ModMatrixGrid::ROWS; ++r)
        {
            int ry = headerY - (r + 1) * cellH;
            if (ry < y + padding)
                break;
            // Cap label width per char count to avoid stretching
            const char *rowText = m_modGrid.getRowLabel(r);
            int rLen = (int)std::strlen(rowText);
            int rLabelW = rLen * 10;
            if (rLabelW > labelW - 8)
                rLabelW = labelW - 8;
            int rLabelH = 14; // fixed height for consistent proportions
            int rLabelY = ry + (cellH - rLabelH) / 2;
            m_renderer.drawLabel(x + padding, rLabelY, rLabelW, rLabelH, rowText);
        }
    }

    void SynthViewer::drawParam(int px, int py, int labelW, int valueW, int h,
                                const char *label, const char *value)
    {
        int labelLen = (int)std::strlen(label);
        int lw = labelLen * 6;
        if (lw > labelW)
            lw = labelW;
        m_renderer.drawLabel(px, py, lw, h, label);
        int valueLen = (int)std::strlen(value);
        int vw = valueLen * 6;
        if (vw > valueW)
            vw = valueW;
        m_renderer.drawLabel(px + labelW, py, vw, h, value);
    }

    void SynthViewer::drawPresetInfoPanel(int x, int y, int w, int h)
    {
        // Panel frame
        const char *presetName = m_synth.getPresetName();
        m_renderer.drawPanelFrame(x, y, w, h, presetName ? presetName : "PRESET", 14);

        int padding = 4;
        int lineH = 10;
        int lineSpacing = lineH + 3;
        int colW = w / 3;
        int labelFixedW = 38;
        int valueFixedW = colW - labelFixedW - padding * 2;

        // Three-column layout
        int colX[3] = {x + padding, x + colW + padding, x + colW * 2 + padding};
        int curY[3] = {y + h - 28, y + h - 28, y + h - 28};
        int col = 0;

        char buf[80];
        auto &mm = m_synth.getModMatrix();

        auto drawLine = [&](const char *label, float value)
        {
            if (curY[col] < y + padding)
                return;
            std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
            drawParam(colX[col], curY[col], labelFixedW, valueFixedW, lineH, label, buf);
            curY[col] -= lineSpacing;
        };

        auto drawModLine = [&](const char *label, float base, int destId)
        {
            if (curY[col] < y + padding)
                return;
            float live = base + mm.getModValue(destId);
            std::snprintf(buf, sizeof(buf), "%.2f>%.2f",
                          static_cast<double>(base), static_cast<double>(live));
            drawParam(colX[col], curY[col], labelFixedW, valueFixedW, lineH, label, buf);
            curY[col] -= lineSpacing;
        };

        auto drawText = [&](const char *label, const char *value)
        {
            if (curY[col] < y + padding)
                return;
            drawParam(colX[col], curY[col], labelFixedW, valueFixedW, lineH, label, value);
            curY[col] -= lineSpacing;
        };

        auto drawSection = [&](const char *title)
        {
            if (curY[col] < y + padding)
                return;
            curY[col] -= 4;
            int len = (int)std::strlen(title);
            int lw = len * 6;
            if (lw > colW - padding * 2)
                lw = colW - padding * 2;
            m_renderer.drawLabel(colX[col], curY[col], lw, lineH, title);
            curY[col] -= lineSpacing;
        };

        const Preset4D *p = m_synth.getCurrentPresetData();
        if (!p)
            return;

        // Oscillator param offsets within each osc block of 17
        constexpr int OFF_PITCH = 0;
        constexpr int OFF_LEVEL = 3;
        constexpr int OFF_PAN = 4;
        constexpr int OFF_CUTOFF = 7;
        constexpr int OFF_RESO = 8;
        constexpr int OFF_FENV = 9;
        constexpr int OFF_ATK = 10;
        constexpr int OFF_DEC = 11;
        constexpr int OFF_SUS = 12;
        constexpr int OFF_REL = 13;

        float liveCutoff = m_synth.getEngine().getCutoff();
        float liveReso = m_synth.getEngine().getResonance();

        // ============ COL 1: OSC1 + ENV1, OSC2 + ENV2 ============
        col = 0;
        for (int i = 0; i < 2; ++i)
        {
            int base = i * 17;
            std::snprintf(buf, sizeof(buf), "-- OSC%d --", i + 1);
            drawSection(buf);
            drawText("Wave", waveTypeToStr(p->waves[i]));
            {
                auto &voice = m_synth.getEngine().getAllocator().getVoice(0);
                auto fxType = voice.getChain(i).getEffectSlot().getEffectType();
                drawText("FX", effectTypeToString(fxType));
            }
            {
                auto &voice = m_synth.getEngine().getAllocator().getVoice(0);
                float liveLevel = voice.getChain(i).getLevel();
                float livePan = voice.getChain(i).getPan();
                drawModLine("Lvl", liveLevel, base + OFF_LEVEL);
                drawModLine("Pan", livePan, base + OFF_PAN);
            }
            drawModLine("Cut", liveCutoff, base + OFF_CUTOFF);
            drawModLine("Res", liveReso, base + OFF_RESO);
            drawModLine("FEn", p->filterEnvAmounts[i], base + OFF_FENV);
            drawModLine("Pit", p->coarseTune[i], base + OFF_PITCH);
            // Envelope inline
            drawModLine("aA", p->ampA[i], base + OFF_ATK);
            drawModLine("aD", p->ampD[i], base + OFF_DEC);
            drawModLine("aS", p->ampS[i], base + OFF_SUS);
            drawModLine("aR", p->ampR[i], base + OFF_REL);
            drawLine("fA", p->fltA[i]);
            drawLine("fD", p->fltD[i]);
            drawLine("fS", p->fltS[i]);
            drawLine("fR", p->fltR[i]);
        }

        // ============ COL 2: OSC3 + ENV3, OSC4 + ENV4 ============
        col = 1;
        for (int i = 2; i < 4; ++i)
        {
            int base = i * 17;
            std::snprintf(buf, sizeof(buf), "-- OSC%d --", i + 1);
            drawSection(buf);
            drawText("Wave", waveTypeToStr(p->waves[i]));
            {
                auto &voice = m_synth.getEngine().getAllocator().getVoice(0);
                auto fxType = voice.getChain(i).getEffectSlot().getEffectType();
                drawText("FX", effectTypeToString(fxType));
            }
            {
                auto &voice = m_synth.getEngine().getAllocator().getVoice(0);
                float liveLevel = voice.getChain(i).getLevel();
                float livePan = voice.getChain(i).getPan();
                drawModLine("Lvl", liveLevel, base + OFF_LEVEL);
                drawModLine("Pan", livePan, base + OFF_PAN);
            }
            drawModLine("Cut", liveCutoff, base + OFF_CUTOFF);
            drawModLine("Res", liveReso, base + OFF_RESO);
            drawModLine("FEn", p->filterEnvAmounts[i], base + OFF_FENV);
            drawModLine("Pit", p->coarseTune[i], base + OFF_PITCH);
            // Envelope inline
            drawModLine("aA", p->ampA[i], base + OFF_ATK);
            drawModLine("aD", p->ampD[i], base + OFF_DEC);
            drawModLine("aS", p->ampS[i], base + OFF_SUS);
            drawModLine("aR", p->ampR[i], base + OFF_REL);
            drawLine("fA", p->fltA[i]);
            drawLine("fD", p->fltD[i]);
            drawLine("fS", p->fltS[i]);
            drawLine("fR", p->fltR[i]);
        }

        // ============ COL 3: LFO, DELAY, TUBE/MFX, SRC ============
        col = 2;

        // LFO config
        drawSection("-- LFO --");
        for (int i = 0; i < 4; ++i)
        {
            if (curY[col] < y + padding)
                break;
            float val = mm.getSourceValue((int)ObsSynthSrc::LFO1 + i);
            char lfoLabel[8];
            std::snprintf(lfoLabel, sizeof(lfoLabel), "L%d", i + 1);
            std::snprintf(buf, sizeof(buf), "%s %.1fHz>%.2f",
                          lfoWaveToStr(m_synth.getLFO(i).getWaveType()),
                          static_cast<double>(m_synth.getLFO(i).getFrequency()),
                          static_cast<double>(val));
            drawParam(colX[col], curY[col], labelFixedW, valueFixedW, lineH, lfoLabel, buf);
            curY[col] -= lineSpacing;
        }

        // Delay
        drawSection("-- DELAY --");
        drawLine("TL", m_synth.getMasterDelay().getDelayTimeLSec());
        drawLine("TR", m_synth.getMasterDelay().getDelayTimeRSec());
        drawLine("FB", p->delay.feedback);
        drawLine("Mix", p->delay.mix);
        drawLine("Damp", p->delay.damping);

        // Master FX Slot - F5 cycles effect type
        drawSection("-- MFX --");
        drawText("FX", effectTypeToString(m_synth.getMasterFX().getEffectType()));
        drawLine("Mix", m_synth.getMasterFX().getMix());

        // Tube Saturation - T cycles model
        drawSection("-- TUBE --");
        drawText("Mod", tubeModelToStr(m_synth.getMasterSaturation().getModel()));
        drawModLine("Drv", p->tube.drive, (int)ObsSynthDst::TUBE_DRIVE);
        drawModLine("Asy", p->tube.asymmetry, (int)ObsSynthDst::TUBE_ASYM);
        drawModLine("Chs", p->tube.chaos, (int)ObsSynthDst::TUBE_CHAOS);
        drawLine("Mix", p->tube.mix);

        // Sources
        drawSection("-- SRC --");
        drawLine("Mod", mm.getSourceValue((int)ObsSynthSrc::MOD_WHEEL));
        drawLine("AT", mm.getSourceValue((int)ObsSynthSrc::AFTERTOUCH));
        drawLine("PB", mm.getSourceValue((int)ObsSynthSrc::PITCH_BEND));
        drawLine("Vel", mm.getSourceValue((int)ObsSynthSrc::VELOCITY));
    }

    // ========================================================================
    // Oscilloscope panel - displays final output waveform
    // ========================================================================
    void SynthViewer::drawScopePanel(int x, int y, int w, int h)
    {
        if (w <= 0 || h <= 0)
            return;

        // Panel background
        glViewport(x, y, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, 1.0, -1.0, 1.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Dark background
        glColor4f(0.05f, 0.06f, 0.08f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(0.0f, 1.0f);
        glEnd();

        // Grid lines
        glColor3f(0.15f, 0.15f, 0.2f);
        glBegin(GL_LINES);
        // Horizontal center
        glVertex2f(0.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
        // Vertical center
        glVertex2f(0.5f, -1.0f);
        glVertex2f(0.5f, 1.0f);
        // Quarter lines
        glVertex2f(0.0f, 0.5f);
        glVertex2f(1.0f, 0.5f);
        glVertex2f(0.0f, -0.5f);
        glVertex2f(1.0f, -0.5f);
        glEnd();

        // Read waveform from ring buffer
        float samples[WaveCapture::DISPLAY_SAMPLES];
        m_waveCapture.readTriggered(samples);

        // Draw waveform
        glColor3f(0.2f, 0.9f, 0.4f);
        glBegin(GL_LINE_STRIP);
        float inv = 1.0f / (WaveCapture::DISPLAY_SAMPLES - 1);
        for (int i = 0; i < WaveCapture::DISPLAY_SAMPLES; ++i)
        {
            float xp = (float)i * inv;
            float yp = samples[i]; // already in [-1, 1]
            glVertex2f(xp, yp);
        }
        glEnd();

        // Border
        glColor3f(0.3f, 0.35f, 0.4f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(0.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(0.0f, 1.0f);
        glEnd();
    }

    // ========================================================================
    // Transfer Curve panel - displays tube saturation f(x) curve
    // ========================================================================
    void SynthViewer::drawTransferCurvePanel(int x, int y, int w, int h)
    {
        if (w <= 0 || h <= 0)
            return;

        glViewport(x, y, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Dark background
        glColor4f(0.05f, 0.05f, 0.07f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(-1.0f, 1.0f);
        glEnd();

        // Grid: diagonal reference (linear = no distortion)
        glColor3f(0.2f, 0.2f, 0.25f);
        glBegin(GL_LINES);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(1.0f, 1.0f);
        // Axes
        glVertex2f(-1.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
        glVertex2f(0.0f, -1.0f);
        glVertex2f(0.0f, 1.0f);
        glEnd();

        // Plot transfer curve: sweep x from -1 to 1
        auto &sat = m_synth.getMasterSaturation();
        constexpr int CURVE_POINTS = 128;
        glColor3f(0.9f, 0.5f, 0.2f); // warm orange
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= CURVE_POINTS; ++i)
        {
            float xIn = -1.0f + 2.0f * (float)i / (float)CURVE_POINTS;
            float yOut = sat.plotTransferCurve(xIn * sat.getDrive());
            // Compensate same as process() does
            float compensation = 1.0f / std::max(sat.getDrive() * 0.6f, 1.0f);
            yOut *= compensation;
            glVertex2f(xIn, yOut);
        }
        glEnd();

        // Border
        glColor3f(0.3f, 0.3f, 0.35f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(1.0f, -1.0f);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(-1.0f, 1.0f);
        glEnd();
    }

    void SynthViewer::drawUI()
    {
        // Legacy immediate-mode layout used on Linux (and as non-ImGui fallback)
        const int W = m_window.width();
        const int H = m_window.height();
        const int margin = 8;
        const int gap = 6;

        // Row 1: 4 oscillator panels
        const int oscRowH = static_cast<int>(H * 0.62f);
        const int oscW = (W - margin * 2 - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            const int x = margin + i * (oscW + gap);
            const int y = H - margin - oscRowH;
            drawOscPanel(x, y, oscW, oscRowH, i);
        }

        // Row 2: mixer | delay | tube | mfx | scope | transfer
        const int bottomY = margin;
        const int bottomH = H - oscRowH - margin * 3;
        if (bottomH < 40)
            return;

        int x = margin;
        auto place = [&](int w, auto &&fn)
        {
            if (x + w > W - margin)
                return;
            fn(x, bottomY, w, bottomH);
            x += w + gap;
        };

        place(140, [this](int px, int py, int pw, int ph)
              { drawMixerPanel(px, py, pw, ph); });
        place(120, [this](int px, int py, int pw, int ph)
              { drawDelayPanel(px, py, pw, ph); });
        place(120, [this](int px, int py, int pw, int ph)
              { drawTubePanel(px, py, pw, ph); });
        place(100, [this](int px, int py, int pw, int ph)
              { drawMFXPanel(px, py, pw, ph); });
        place(220, [this](int px, int py, int pw, int ph)
              { drawScopePanel(px, py, pw, ph); });
        place(160, [this](int px, int py, int pw, int ph)
              { drawTransferCurvePanel(px, py, pw, ph); });

        // Optional mod matrix overlay (toggled with M)
        if (m_showModMatrix)
        {
            const int mw = W / 2;
            const int mh = H / 2;
            drawModMatrixPanel((W - mw) / 2, (H - mh) / 2, mw, mh);
        }

        // Preset strip on top
        drawPresetInfoPanel(margin, H - margin - 28, W - margin * 2, 24);
    }

    void SynthViewer::frame()
    {
        // ── ImGui frame (Win32 platform backend + OpenGL2) ────────────
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawImguiUI();

        ImGui::Render();

        int w = m_window.width();
        int h = m_window.height();
        glViewport(0, 0, w, h);
        glClearColor(0.051f, 0.051f, 0.071f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        m_window.swapBuffers();
        updateFPS();
    }

    void SynthViewer::updateFPS()
    {
        if (m_frames == 0)
            m_lastTime = std::chrono::high_resolution_clock::now();
        m_frames++;
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - m_lastTime).count();
        m_accum += dt;
        m_lastTime = now;
        if (m_accum >= 1.0)
        {
            double fps = m_frames / m_accum;
            m_frames = 0;
            m_accum = 0.0;
            const char *presetName = m_synth.getPresetName();
            std::wstring title = L"ObsSynth4D";
            if (presetName && presetName[0] != '\0')
            {
                std::string pn(presetName);
                title += L" \u2014 ";
                title += std::wstring(pn.begin(), pn.end());
            }
            title += L"  [" + std::to_wstring((int)fps) + L" fps]";

            // Audio CPU metrics (smoothed over 1s window via last reported values)
            float audioMs = m_synth.getLastAudioMs();
            float audioCpu = m_synth.getLastAudioCpuPct();
            // Format: "  audio: 0.42ms  cpu: 12%"
            wchar_t audioStats[64];
            swprintf_s(audioStats, 64, L"   audio %.2fms  CPU %.0f%%", audioMs, audioCpu);
            title += audioStats;

            SetWindowTextW(m_window.hwnd(), title.c_str());
        }
    }

    int SynthViewer::run()
    {
        if (!initWindow())
            return -1;
        std::cerr << "[RUN] Window created" << std::endl;

        // ── ImGui initialisation ─────────────────────────────────────────────
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ApplySynthViewTheme();

        // Hook WndProc so ImGui processes Win32 messages (mouse / keyboard)
        m_window.setWndProcHook([](HWND h, UINT m, WPARAM w, LPARAM l) -> bool
                                {
            extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
            return ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0; });

        ImGui_ImplWin32_Init(m_window.hwnd());
        ImGui_ImplOpenGL2_Init();
        // ─────────────────────────────────────────────────────────────────────

        initAudio();
        initUIValues();
        initKnobValues();
        initMIDI();

        std::cerr << "[RUN] Entering main loop" << std::endl;
        while (m_window.pumpMessages())
            frame();
        std::cerr << "[RUN] Loop exited" << std::endl;

        // ── ImGui shutdown ───────────────────────────────────────────────────
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return 0;
    }

    int runOpenGLViewer()
    {
        SynthViewer app;
        return app.run();
    }

} // namespace synth

#ifdef SYNTH_GL_VIEWER_MAIN
int main() { return synth::runOpenGLViewer(); }
#endif
