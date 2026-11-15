// main_screen.hpp - Synth viewer application class
#pragma once

#include <vector>
#include <array>
#include <memory>
#include <chrono>
#include "platform_window.hpp"
#include "gl_renderer.hpp"
#include "wave_capture.hpp"
#include "midi_learn.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "../../products/obs_synth/obs_synth_4d.hpp"
#include "mod_matrix_grid.hpp"
#include "../common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"

namespace synth
{
    class SynthViewer
    {
    public:
        SynthViewer();
        ~SynthViewer();
        int run();

    private:
        // Initialization helpers
        bool initWindow();
        void initAudio();
        void initUIValues();
        void initMIDI();

        // Frame lifecycle
        void frame();
        void drawUI();
        void drawModMatrixPanel(int x, int y, int w, int h);
        void drawPresetInfoPanel(int x, int y, int w, int h);
        void drawScopePanel(int x, int y, int w, int h);
        void drawTransferCurvePanel(int x, int y, int w, int h);
        void drawParam(int x, int y, int labelW, int valueW, int h,
                       const char *label, const char *value);
        void updateFPS();

        // Data builders
        std::vector<Point> buildWaveform(int samples);
        std::vector<Point> buildEnvelopeShape(float noteDurSec, int res);

        // MIDI callbacks
        void onMidiNote(const MidiNote &note);
        void onMidiControl(uint8_t controller, uint8_t value);

        // Key handling
        void onKeyPress(int vk);

        // Mouse handling
        void onMouseClick(int button, int mx, int my);
        void onMouseMove(int mx, int my);
        void onMouseUp(int button, int mx, int my);
        void onMouseWheel(int delta, int mx, int my);

        // Members
        PlatformWindow m_window;
        GLRenderer m_renderer;

        ObsSynth4D m_synth; // Product: 4-osc synth with LFOs, presets, mod matrix
        // Multiple MIDI devices — one MidiInterface per open device
        std::vector<std::unique_ptr<MidiInterface>> m_midiDevices;
        AudioSink m_audioSink;

        std::vector<Point> m_wave;
        std::vector<Point> m_envShape;

        // UI element counts
        static constexpr int FADER_COUNT = 18; // 0-8: faders (CC71-79), 9-17: encoders (CC21-29)
        static constexpr int KNOB_COLS = 4;
        static constexpr int KNOB_ROWS = 4;
        static constexpr int KNOB_COUNT = KNOB_COLS * KNOB_ROWS;
        static constexpr int MOD_MATRIX_SLOTS = 8;

        std::array<float, FADER_COUNT> m_faderVals{};
        std::array<float, KNOB_COUNT> m_knobVals{};

        // MIDI pickup state
        std::array<bool, FADER_COUNT> m_faderPicked{};
        std::array<float, FADER_COUNT> m_lastHwFaderNorm{};
        static constexpr float PICKUP_THRESHOLD = 0.02f;

        // ModMatrix UI state
        int m_selectedModSlot{0};
        std::array<float, MOD_MATRIX_SLOTS> m_modSlotAmounts{};
        ModMatrixGrid m_modGrid;
        bool m_showModMatrix{false}; // toggle modal overlay with 'M' key

        // Grid geometry (stored during draw for click hit-testing)
        int m_gridX{0}, m_gridY{0}; // bottom-left of grid cells in window coords
        int m_gridCellW{0}, m_gridCellH{0};

        // Timing / FPS
        std::chrono::high_resolution_clock::time_point m_lastTime;
        int m_frames{0};
        double m_accum{0.0};

        // Internal helpers
        static float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

        // Waveform capture for oscilloscope display
        WaveCapture m_waveCapture;

        // ===================== Signal Flow Knob UI =====================
        static constexpr int NUM_OSCS = 4;
        static constexpr int KNOB_SZ = 38; // knobs to fit 2x2 osc panels

        // Per-oscillator knob parameters
        enum OscParam
        {
            OSC_WAVE = 0,  // discrete: 0-3 (sine/saw/square/triangle)
            OSC_TUNE,      // -24..+24 semitones
            OSC_FINE_TUNE, // -1..+1 semitone (fine)
            OSC_CUTOFF,    // 20..20000 Hz
            OSC_RESO,      // 0..1
            OSC_HPF,       // 20..2000 Hz (HPF cutoff)
            OSC_FILT_TYPE, // 0=LPF 1=HPF 2=BPF 3=Notch (discrete)
            OSC_KEY_TRACK, // 0..1
            OSC_FILT_ATK,  // Filter Env Attack
            OSC_FILT_DEC,  // Filter Env Decay
            OSC_FILT_SUS,  // Filter Env Sustain
            OSC_FILT_REL,  // Filter Env Release
            OSC_AMP_ATK,   // Amp Env Attack
            OSC_AMP_DEC,   // Amp Env Decay
            OSC_AMP_SUS,   // Amp Env Sustain
            OSC_AMP_REL,   // Amp Env Release
            OSC_FX_P1,     // Effect param 1
            OSC_FX_P2,     // Effect param 2
            OSC_FX_P3,     // Effect param 3
            OSC_LEVEL,     // 0..1
            OSC_PAN,       // -1..+1
            // PartType / WaveTable / FM
            OSC_PART_TYPE,    // 0=STD 1=WT 2=FM
            OSC_WT_MORPH,     // wavetable morph position [0..1] normalised
            OSC_FM_ALGORITHM, // FMAlgorithm enum (0-4)
            OSC_FM_MOD_RATIO, // modulator freq ratio
            OSC_FM_MOD_INDEX, // modulation index (0-20)
            OSC_FM_FEEDBACK,  // feedback (0-1)
            OSC_FM_OP2_RATIO, // operator 2 ratio
            OSC_FM_OP2_LEVEL, // operator 2 level
            OSC_PARAM_COUNT
        };

        // Global section knob parameters
        enum GlobalParam
        {
            // Master Filter (post-mix, pre-delay)
            GLB_MF_CUTOFF,
            GLB_MF_RESO,
            GLB_MF_TYPE,   // 0=LPF 1=HPF 2=BPF 3=Notch
            GLB_MF_BYPASS, // 0/1 toggle
                           // Stereo Delay
            GLB_DLY_L,
            GLB_DLY_R,
            GLB_DLY_FB,
            GLB_DLY_MIX,
            GLB_DLY_PING,
            GLB_DLY_DMP,
            // Tube
            GLB_TUBE_DRV,
            GLB_TUBE_ASYM,
            GLB_TUBE_MIX,
            // MFX (master effect slot)
            GLB_MFX_P1,
            GLB_MFX_P2,
            GLB_MFX_P3,
            // Master
            GLB_VOLUME,
            GLB_PARAM_COUNT
        };

        struct KnobDef
        {
            const char *label;
            float minVal;
            float maxVal;
            float defaultVal;
        };

        static constexpr KnobDef OSC_KNOB_DEFS[OSC_PARAM_COUNT] = {
            {"Wave", 0.0f, 3.0f, 0.0f},
            {"Tune", -24.0f, 24.0f, 0.0f},
            {"Fine", -1.0f, 1.0f, 0.0f},           // bipolar: almost -1 to +1 semitone
            {"Cutoff", 20.0f, 20000.0f, 10000.0f}, // OSC_CUTOFF
            {"Reso", 0.0f, 1.0f, 0.0f},            // OSC_RESO
            {"HPF", 20.0f, 2000.0f, 20.0f},        // OSC_HPF
            {"FType", 0.0f, 3.0f, 0.0f},           // OSC_FILT_TYPE
            {"KeyTrk", 0.0f, 1.0f, 0.0f},          // OSC_KEY_TRACK
            {"Atk", 0.001f, 2.0f, 0.01f},          // Filter Env
            {"Dec", 0.001f, 2.0f, 0.2f},
            {"Sus", 0.0f, 1.0f, 0.5f},
            {"Rel", 0.01f, 5.0f, 0.3f},
            {"Atk", 0.001f, 2.0f, 0.01f}, // Amp Env
            {"Dec", 0.001f, 2.0f, 0.2f},
            {"Sus", 0.0f, 1.0f, 0.7f},
            {"Rel", 0.01f, 5.0f, 0.3f},
            {"P1", 0.0f, 1.0f, 0.5f}, // Effect
            {"P2", 0.0f, 1.0f, 0.5f},
            {"P3", 0.0f, 1.0f, 0.5f},
            {"Level", 0.0f, 1.0f, 0.8f},
            {"Pan", -1.0f, 1.0f, 0.0f},
            // PartType / WT / FM
            {"Part", 0.0f, 2.0f, 0.0f},     // OSC_PART_TYPE
            {"Morph", 0.0f, 1.0f, 0.0f},    // OSC_WT_MORPH
            {"Algo", 0.0f, 4.0f, 1.0f},     // OSC_FM_ALGORITHM (TWO_OP_FEEDBACK=1)
            {"M.Ratio", 0.1f, 16.0f, 1.0f}, // OSC_FM_MOD_RATIO
            {"FM.Idx", 0.0f, 20.0f, 1.0f},  // OSC_FM_MOD_INDEX
            {"FB", 0.0f, 1.0f, 0.0f},       // OSC_FM_FEEDBACK
            {"O2.R", 0.1f, 16.0f, 2.0f},    // OSC_FM_OP2_RATIO
            {"O2.L", 0.0f, 1.0f, 0.5f},     // OSC_FM_OP2_LEVEL
        };

        static constexpr KnobDef GLB_KNOB_DEFS[GLB_PARAM_COUNT] = {
            // Master Filter
            {"Cutoff", 20.0f, 20000.0f, 20000.0f}, // GLB_MF_CUTOFF
            {"Reso", 0.0f, 1.0f, 0.0f},            // GLB_MF_RESO
            {"FType", 0.0f, 3.0f, 0.0f},           // GLB_MF_TYPE
            {"Bypass", 0.0f, 1.0f, 0.0f},          // GLB_MF_BYPASS
            // Delay
            {"L", 0.01f, 2.0f, 0.375f},
            {"R", 0.01f, 2.0f, 0.5f},
            {"F.Back", 0.0f, 0.95f, 0.35f},
            {"Mix", 0.0f, 1.0f, 0.25f},
            {"Ping", 0.0f, 1.0f, 1.0f},
            {"Damp", 0.0f, 1.0f, 0.4f},
            {"Drive", 0.5f, 8.0f, 1.8f},
            {"Asym", 0.0f, 1.0f, 0.3f},
            {"Mix", 0.0f, 1.0f, 0.4f},
            {"P1", 0.0f, 1.0f, 0.5f},
            {"P2", 0.0f, 1.0f, 0.5f},
            {"P3", 0.0f, 1.0f, 0.5f},
            {"Vol", 0.0f, 1.0f, 0.8f},
        };

        // Values storage
        float m_oscKnobValues[NUM_OSCS][OSC_PARAM_COUNT]{};
        float m_glbKnobValues[GLB_PARAM_COUNT]{};
        std::array<std::string, 4> m_pcmPaths{};
        std::array<std::string, 4> m_wtPaths{}; // loaded WT bank paths

        void initKnobValues();
        void applyOscKnobValue(int osc, int param);
        void applyGlbKnobValue(int param);
        // Pull all parameter values from the live synth engine into m_oscKnobValues / m_glbKnobValues.
        // Called every ImGui frame so MIDI CC changes are immediately reflected in the UI.
        void syncKnobsFromEngine();

        // Drawing
        void drawOscPanel(int x, int y, int w, int h, int oscIdx);
        void drawMixerPanel(int x, int y, int w, int h);
        void drawDelayPanel(int x, int y, int w, int h);
        void drawTubePanel(int x, int y, int w, int h);
        void drawMFXPanel(int x, int y, int w, int h);
        void drawMiniModMatrix(int x, int y, int w, int h);

        // ── ImGui-based UI layer ──────────────────────────────────────────────
        // Called from frame() instead of the legacy OpenGL draw methods.
        void drawImguiUI();
        void drawImguiOscPanel(int oscIdx);
        void drawImguiMixerPanel();
        void drawImguiDelayPanel();
        void drawImguiTubePanel();
        void drawImguiMFXPanel();
        void drawImguiMasterFilterPanel();
        void drawImguiLFOPanel();
        void drawImguiXYPad();
        void drawImguiScopePanel();
        void drawImguiModMatrix();

        // XY pad state (0..1 each axis)
        float m_xyPad[2]{0.5f, 0.5f};

        // Knob interaction state
        int m_dragKnobIdx{-1}; // flat index into all knobs or -1
        int m_dragStartY{0};
        float m_dragStartValue{0.0f};
        int m_hoverKnobIdx{-1};

        // Helper: draw knob + label + register for hit-test
        void registerKnob(int kx, int ky, int sz, float val, float minV, float maxV,
                          const char *label, int flatIdx, int oscIdx, int paramIdx);

        // Geometry for hit-testing (stored per-frame)
        struct KnobRect
        {
            int x, y, size;
            int oscIdx;   // -1 for global
            int paramIdx; // OscParam or GlobalParam
        };
        static constexpr int MAX_KNOBS = NUM_OSCS * OSC_PARAM_COUNT + GLB_PARAM_COUNT;
        KnobRect m_knobRects[MAX_KNOBS]{};
        int m_knobRectCount{0};

        int hitTestKnob(int mx, int my) const;
        float getKnobValue(int flatIdx) const;
        void setKnobValue(int flatIdx, float v);
        const KnobDef &getKnobDef(int flatIdx) const;

        // ── MIDI Learn ────────────────────────────────────────────────────────
        MidiLearnMap m_midiLearnMap;
        bool m_midiLearnActive{false};             ///< MAP mode engaged
        MidiLearnTarget m_midiLearnTarget{-1, -1}; ///< awaiting CC for this target
        void applyMidiLearnCC(uint8_t cc, float normalizedValue);
        void drawImguiMidiLearnOverlay();
    };

    int runOpenGLViewer();
}
