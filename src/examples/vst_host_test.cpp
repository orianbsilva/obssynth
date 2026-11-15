//------------------------------------------------------------------------
// VST3 Test Host - Com ASIO e MIDI Real
//
// Host de teste instrumentado para desenvolvimento rápido do sintetizador.
// Permite testar o motor de áudio com baixa latência via ASIO e entrada
// MIDI real de dispositivos externos.
//
// Funcionalidades:
// - Saída de áudio via ASIO (baixa latência)
// - Entrada MIDI de dispositivos reais
// - Fallback para teclado quando MIDI não disponível
// - Métricas de performance em tempo real
// - Export para WAV para análise
//------------------------------------------------------------------------

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <conio.h>
#include <iomanip>
#include <set>
#include <mutex>
#include <cmath>

// Host base com MIDI e Audio comum
#include "host_base.hpp"

// Synth core includes
#include "oscillators/polyphonic_oscillator.hpp"
#include "utils/wav_writer.hpp"

namespace synth
{

    //------------------------------------------------------------------------
    // VSTHostTest - Host de teste com ASIO e MIDI (herda de HostBase)
    //------------------------------------------------------------------------
    class VSTHostTest : public HostBase
    {
    public:
        static constexpr int SAMPLE_RATE = 48000;
        static constexpr int BUFFER_SIZE = 256; // Lower for ASIO low latency

        VSTHostTest()
            : oscillator_(WaveType::SINE, static_cast<float>(SAMPLE_RATE)), processTimeUs_(0), bufferCount_(0), midiNoteCount_(0)
        {
            // Configurar ADSR padrão - típico para synth lead
            oscillator_.setADSR(0.01f, 0.15f, 0.7f, 0.3f);
        }

        ~VSTHostTest()
        {
            stop();
        }

        bool initialize()
        {
            printBanner();

            std::cout << "Sample Rate: " << SAMPLE_RATE << " Hz\n";
            std::cout << "Buffer Size: " << BUFFER_SIZE << " samples\n";
            std::cout << "Latency:     " << std::fixed << std::setprecision(2)
                      << (BUFFER_SIZE * 1000.0f / SAMPLE_RATE) << " ms\n";
            std::cout << "\n";

            // 1. Inicializar MIDI (da classe base)
            initializeMidiBase();

            // Configurar callbacks MIDI específicos
            if (isMidiAvailable())
            {
                setupMidiCallbacks();
            }

            // 2. Inicializar Audio (da classe base) com callback
            initializeAudioBase([this](float *buffer, int bufferSize, int numChannels)
                                { processAudio(buffer, bufferSize, numChannels); });

            std::cout << "\n";
            return true;
        }

        void run()
        {
            setRunning(true);

            printHelp();
            printStatus();

            // Iniciar áudio
            startAudio();

            // Loop principal - processa input do teclado
            std::cout << "\n[READY] Waiting for MIDI/keyboard input...\n\n";

            while (isRunning())
            {
                if (_kbhit())
                {
                    int key = _getch();
                    handleKeyPress(key);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            // Parar áudio
            stopAudio();
        }

        void stop()
        {
            shutdownBase();
        }

    private:
        PolyphonicOscillator oscillator_;
        std::mutex oscillatorMutex_;

        // Métricas de performance
        std::atomic<int64_t> processTimeUs_;
        std::atomic<uint64_t> bufferCount_;
        std::atomic<float> peakLevel_{0.0f};
        std::atomic<int> activeVoices_{0};
        std::atomic<uint64_t> midiNoteCount_;

        // Test tone
        std::atomic<bool> testToneActive_{false};
        float testTonePhase_ = 0.0f;

        // Estado do sintetizador
        WaveType currentWaveType_ = WaveType::SINE;
        float masterVolume_ = 0.7f;
        float attackTime_ = 0.01f;
        float decayTime_ = 0.15f;
        float sustainLevel_ = 0.7f;
        float releaseTime_ = 0.3f;

        // Notas do teclado (fallback)
        const std::vector<std::pair<int, int>> keyToNote_ = {
            {'a', 48}, {'s', 50}, {'d', 52}, {'f', 53}, {'g', 55}, {'h', 57}, {'j', 59}, {'k', 60}, {'q', 60}, {'w', 62}, {'e', 64}, {'r', 65}, {'t', 67}, {'y', 69}, {'u', 71}, {'i', 72}};
        std::set<int> activeKeys_;

        //--------------------------------------------------------------------
        // Callback de processamento de áudio
        //--------------------------------------------------------------------
        void processAudio(float *buffer, int bufferSize, int numChannels)
        {
            auto start = std::chrono::high_resolution_clock::now();

            // Test tone mode
            if (testToneActive_)
            {
                for (int i = 0; i < bufferSize; i++)
                {
                    float sample = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * testTonePhase_);
                    testTonePhase_ += 1.0f / SAMPLE_RATE;
                    if (testTonePhase_ > 1.0f)
                        testTonePhase_ -= 1.0f;
                    buffer[i * 2] = sample;     // Left
                    buffer[i * 2 + 1] = sample; // Right
                }
            }
            else
            {
                std::lock_guard<std::mutex> lock(oscillatorMutex_);

                // Gerar áudio estéreo interleaved
                oscillator_.generateStereoBuffer(buffer, bufferSize);

                // Aplicar master volume
                for (int i = 0; i < bufferSize * numChannels; i++)
                {
                    buffer[i] *= masterVolume_;
                }
            }

            // Calcular nível de pico para monitoramento
            float peak = 0.0f;
            for (int i = 0; i < bufferSize * numChannels; i++)
            {
                float abs_val = std::abs(buffer[i]);
                if (abs_val > peak)
                    peak = abs_val;
            }
            peakLevel_.store(peak);

            auto end = std::chrono::high_resolution_clock::now();
            processTimeUs_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            bufferCount_++;

            // Print nível a cada 100 buffers (~0.5s)
            if (bufferCount_ % 100 == 0 && peak > 0.001f)
            {
                int bars = static_cast<int>(peak * 40);
                std::cout << "\r[AUDIO] ";
                for (int i = 0; i < 40; i++)
                {
                    std::cout << (i < bars ? "#" : "-");
                }
                std::cout << " " << std::fixed << std::setprecision(3) << peak << "      " << std::flush;
            }
        }

        void printBanner()
        {
            std::cout << "\n";
            std::cout << "================================================================\n";
            std::cout << "          VST3 TEST HOST - ASIO + MIDI Edition\n";
            std::cout << "          ObsSynth Development Environment\n";
            std::cout << "================================================================\n";
            std::cout << "\n";
        }

        void setupMidiCallbacks()
        {
            std::cout << "[DEBUG] Setting up MIDI callbacks...\n";

            // Note On/Off callback
            getMidiInterface()->setNoteCallback([this](const MidiNote &note)
                                                {
            // Print imediatamente para debug
            std::cout << "\n>>> MIDI NOTE: ch=" << static_cast<int>(note.channel)
                      << " note=" << static_cast<int>(note.note)
                      << " vel=" << static_cast<int>(note.velocity)
                      << " on=" << (note.is_note_on ? "YES" : "NO") << std::endl;
            
            std::lock_guard<std::mutex> lock(oscillatorMutex_);
            
            if (note.is_note_on && note.velocity > 0) {
                float velocity = note.velocity / 127.0f;
                oscillator_.noteOn(note.note, velocity);
                midiNoteCount_++;
                activeVoices_ = oscillator_.getActiveVoiceCount();
                
                // Calcular frequência para referência
                float freq = 440.0f * std::pow(2.0f, (note.note - 69) / 12.0f);
                std::cout << "[SYNTH] Note ON: " << static_cast<int>(note.note) 
                          << " (" << std::fixed << std::setprecision(1) << freq << " Hz)"
                          << " vel=" << static_cast<int>(note.velocity)
                          << " voices=" << activeVoices_.load() << std::endl;
            } else {
                oscillator_.noteOff(note.note);
                activeVoices_ = oscillator_.getActiveVoiceCount();
                std::cout << "[SYNTH] Note OFF: " << static_cast<int>(note.note)
                          << " voices=" << activeVoices_.load() << std::endl;
            } });

            std::cout << "[DEBUG] Note callback registered\n";

            // Control Change callback
            getMidiInterface()->setControlCallback([this](uint8_t channel, uint8_t cc, uint8_t value)
                                                   {
            float normalizedValue = value / 127.0f;
            
            switch (cc) {
                case 1:  // Mod wheel
                    std::cout << "\r[MIDI] CC1 (Mod): " << static_cast<int>(value) << "        " << std::flush;
                    break;
                    
                case 7:  // Volume
                    masterVolume_ = normalizedValue;
                    std::cout << "\r[MIDI] Volume: " << static_cast<int>(masterVolume_ * 100) << "%        " << std::flush;
                    break;
                    
                case 74: // Filter cutoff (common)
                    std::cout << "\r[MIDI] CC74 (Cutoff): " << static_cast<int>(value) << "        " << std::flush;
                    break;
                    
                case 73: // Attack
                    attackTime_ = normalizedValue * 2.0f;
                    updateADSR();
                    break;
                    
                case 75: // Decay
                    decayTime_ = normalizedValue * 2.0f;
                    updateADSR();
                    break;
                    
                case 76: // Sustain
                    sustainLevel_ = normalizedValue;
                    updateADSR();
                    break;
                    
                case 72: // Release
                    releaseTime_ = normalizedValue * 3.0f;
                    updateADSR();
                    break;
            } });

            // Pitch Bend callback
            getMidiInterface()->setPitchBendCallback([this](uint8_t channel, int value)
                                                     {
            float normalizedBend = value / 8192.0f;
            std::cout << "\r[MIDI] Pitch Bend: " << std::fixed << std::setprecision(2) 
                      << normalizedBend << "        " << std::flush; });
        }

        void updateADSR()
        {
            std::lock_guard<std::mutex> lock(oscillatorMutex_);
            oscillator_.setADSR(attackTime_, decayTime_, sustainLevel_, releaseTime_);
        }

        void handleKeyPress(int key)
        {
            // Se MIDI não disponível, usar teclado para notas
            if (!isMidiAvailable())
            {
                for (const auto &[k, note] : keyToNote_)
                {
                    if (key == k || key == toupper(k))
                    {
                        if (activeKeys_.find(k) == activeKeys_.end())
                        {
                            std::lock_guard<std::mutex> lock(oscillatorMutex_);
                            oscillator_.noteOn(static_cast<uint8_t>(note), 0.8f);
                            activeKeys_.insert(k);
                            std::cout << "\r[KB] Note ON: " << note << " (" << (char)k << ")         " << std::flush;
                        }
                        return;
                    }
                }
            }

            // Note off com espaço
            if (key == ' ')
            {
                std::lock_guard<std::mutex> lock(oscillatorMutex_);
                oscillator_.allNotesOff();
                activeKeys_.clear();
                std::cout << "\r[ALL NOTES OFF]                    " << std::flush;
                return;
            }

            // Controles
            switch (key)
            {
            case '1':
                setWaveType(WaveType::SINE);
                break;
            case '2':
                setWaveType(WaveType::SAW);
                break;
            case '3':
                setWaveType(WaveType::SQUARE);
                break;
            case '4':
                setWaveType(WaveType::TRIANGLE);
                break;
            case '5':
                setWaveType(WaveType::FM_SYNTHESIS);
                break;

            case '+':
            case '=':
                masterVolume_ = std::min(1.0f, masterVolume_ + 0.05f);
                std::cout << "\r[VOLUME] " << static_cast<int>(masterVolume_ * 100) << "%           " << std::flush;
                break;

            case '-':
                masterVolume_ = std::max(0.0f, masterVolume_ - 0.05f);
                std::cout << "\r[VOLUME] " << static_cast<int>(masterVolume_ * 100) << "%           " << std::flush;
                break;

            case 'p':
            case 'P':
                printStatus();
                break;

            case 'm':
            case 'M':
                printMetrics();
                break;

            case 'x':
            case 'X':
                exportWav();
                break;

            case 'l':
            case 'L':
                listDevices();
                break;

            case 27: // ESC
                setRunning(false);
                break;

            case '?':
                printHelp();
                break;

            case '0': // Test tone toggle
                testToneActive_ = !testToneActive_;
                if (testToneActive_)
                {
                    std::cout << "\n[TEST TONE] 440 Hz sine wave ACTIVE - press 0 to stop\n"
                              << std::flush;
                }
                else
                {
                    std::cout << "\n[TEST TONE] Stopped\n"
                              << std::flush;
                }
                break;
            }
        }

        void setWaveType(WaveType type)
        {
            std::lock_guard<std::mutex> lock(oscillatorMutex_);
            currentWaveType_ = type;
            oscillator_.setWaveType(type);

            const char *names[] = {"SINE", "SAW", "SQUARE", "TRIANGLE", "PULSE", "NOISE", "RAMP_UP", "RAMP_DOWN", "PCM", "FM"};
            int idx = static_cast<int>(type);
            if (idx < 10)
            {
                std::cout << "\r[WAVE] " << names[idx] << "                    " << std::flush;
            }
        }

        void listDevices()
        {
            std::cout << "\n--- AVAILABLE DEVICES ---\n";

            // MIDI devices
            int numMidi = midiInGetNumDevs();
            std::cout << "MIDI Input Devices (" << numMidi << "):\n";
            for (int i = 0; i < numMidi; i++)
            {
                MIDIINCAPS caps;
                if (midiInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                {
                    std::cout << "  [" << i << "] " << caps.szPname << "\n";
                }
            }

            // Audio devices
            std::cout << "\nAudio Output:\n";
            if (isAudioAvailable())
            {
                AudioSink::listDevices();
            }

            std::cout << "\n";
        }

        void printHelp()
        {
            std::cout << "\n";
            std::cout << "================================================================\n";
            std::cout << "                      KEYBOARD CONTROLS\n";
            std::cout << "================================================================\n";
            std::cout << "  A-K, Q-I  : Play notes (when no MIDI device)\n";
            std::cout << "  SPACE     : All notes off\n";
            std::cout << "  0         : Toggle test tone (440 Hz sine)\n";
            std::cout << "  1-5       : Waveform (Sine/Saw/Square/Tri/FM)\n";
            std::cout << "  +/-       : Volume up/down\n";
            std::cout << "  P         : Print current status\n";
            std::cout << "  M         : Print performance metrics\n";
            std::cout << "  L         : List available devices\n";
            std::cout << "  X         : Export 3 seconds to WAV\n";
            std::cout << "  ESC       : Exit\n";
            std::cout << "  ?         : Show this help\n";
            std::cout << "----------------------------------------------------------------\n";
            std::cout << "  MIDI CC: 7=Vol, 73=Atk, 75=Dec, 76=Sus, 72=Rel\n";
            std::cout << "================================================================\n";
            std::cout << "\n";
        }

        void printStatus()
        {
            const char *waveNames[] = {"SINE", "SAW", "SQUARE", "TRIANGLE", "PULSE", "NOISE", "RAMP_UP", "RAMP_DOWN", "PCM", "FM"};
            int waveIdx = static_cast<int>(currentWaveType_);

            std::cout << "\n";
            std::cout << "========================================\n";
            std::cout << "          SYNTHESIZER STATUS\n";
            std::cout << "========================================\n";
            std::cout << "  Waveform:  " << (waveIdx < 10 ? waveNames[waveIdx] : "OTHER") << "\n";
            std::cout << "  Volume:    " << static_cast<int>(masterVolume_ * 100) << "%\n";
            std::cout << "----------------------------------------\n";
            std::cout << "  Attack:    " << std::fixed << std::setprecision(3) << attackTime_ << " sec\n";
            std::cout << "  Decay:     " << decayTime_ << " sec\n";
            std::cout << "  Sustain:   " << static_cast<int>(sustainLevel_ * 100) << "%\n";
            std::cout << "  Release:   " << releaseTime_ << " sec\n";
            std::cout << "----------------------------------------\n";
            std::cout << "  Audio:     " << (isAsioAvailable() ? "ASIO" : (isAudioAvailable() ? "DirectSound/WaveOut" : "None")) << "\n";
            std::cout << "  MIDI:      " << (isMidiAvailable() ? "Connected" : "Keyboard Fallback") << "\n";
            std::cout << "========================================\n";
            std::cout << "\n";
        }

        void printMetrics()
        {
            float avgProcessTime = static_cast<float>(processTimeUs_);
            float bufferTimeUs = BUFFER_SIZE * 1000000.0f / SAMPLE_RATE;
            float cpuUsage = (avgProcessTime / bufferTimeUs) * 100.0f;

            std::cout << "\n";
            std::cout << "========================================\n";
            std::cout << "        PERFORMANCE METRICS\n";
            std::cout << "========================================\n";
            std::cout << "  Buffers:     " << bufferCount_.load() << "\n";
            std::cout << "  MIDI Notes:  " << midiNoteCount_.load() << "\n";
            std::cout << "  Process:     " << std::fixed << std::setprecision(1) << avgProcessTime << " us\n";
            std::cout << "  Buffer:      " << bufferTimeUs << " us\n";
            std::cout << "  CPU Usage:   " << cpuUsage << " %\n";
            std::cout << "  Latency:     " << (BUFFER_SIZE * 1000.0f / SAMPLE_RATE) << " ms\n";
            std::cout << "========================================\n";
            std::cout << "\n";
        }

        void exportWav()
        {
            std::cout << "\n[EXPORT] Generating 3 seconds of audio...\n";

            int totalSamples = SAMPLE_RATE * 3;
            std::vector<float> stereoBuffer(totalSamples * 2);

            {
                std::lock_guard<std::mutex> lock(oscillatorMutex_);

                // Play a chord
                oscillator_.noteOn(60, 0.7f); // C4
                oscillator_.noteOn(64, 0.6f); // E4
                oscillator_.noteOn(67, 0.5f); // G4

                // Gerar áudio
                oscillator_.generateStereoBuffer(stereoBuffer.data(), totalSamples);

                // Aplicar volume
                for (int i = 0; i < totalSamples * 2; i++)
                {
                    stereoBuffer[i] *= masterVolume_;
                }

                oscillator_.allNotesOff();
            }

            // Exportar
            std::string filename = "vst_host_test_export.wav";

            WAVConfig config;
            config.sample_rate = SAMPLE_RATE;
            config.num_channels = 2;
            config.bits_per_sample = 16;

            int result = WAVWriter::writeFile(filename, stereoBuffer, totalSamples, config);

            if (result == 0)
            {
                std::cout << "[EXPORT] Saved to: " << filename << "\n";
                std::cout << "[EXPORT] Duration: 3.0 seconds, " << totalSamples << " samples\n\n";
            }
            else
            {
                std::cout << "[EXPORT] Failed to save file!\n";
            }
        }
    };

} // namespace synth

//------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------
int main()
{
    std::cout << "Starting VST3 Test Host with ASIO + MIDI...\n";

    synth::VSTHostTest host;

    if (!host.initialize())
    {
        std::cerr << "Failed to initialize host\n";
        return 1;
    }

    host.run();

    std::cout << "\nVST3 Test Host terminated.\n";
    return 0;
}
