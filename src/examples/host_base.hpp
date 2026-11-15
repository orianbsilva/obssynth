//------------------------------------------------------------------------
// host_base.hpp - Classe base para hosts VST com MIDI e ASIO
//
// Fornece funcionalidade comum para:
// - Inicialização e gerenciamento de MIDI
// - Inicialização e gerenciamento de Audio (ASIO/DirectSound)
// - Detecção automática de Launchpad X
// - Seleção de driver VB-Matrix VASIO-32
//------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <atomic>

#include "audio/audio_sink.hpp"
#include "audio/audio_backend.hpp"
#include "utils/platform_midi_reader.hpp"

namespace synth
{

    //------------------------------------------------------------------------
    // AudioCallback type
    //------------------------------------------------------------------------
    using AudioCallback = std::function<void(float *buffer, int frames, int channels)>;

    //------------------------------------------------------------------------
    // HostBase - Classe base com MIDI e Audio comum
    //------------------------------------------------------------------------
    class HostBase
    {
    public:
        static constexpr int DEFAULT_SAMPLE_RATE = 48000;
        static constexpr int DEFAULT_BUFFER_SIZE = 512;
        static constexpr int NUM_CHANNELS = 2;

        HostBase() : running_(false), midiAvailable_(false), audioAvailable_(false), asioAvailable_(false) {}

        virtual ~HostBase()
        {
            shutdownBase();
        }

        // Getters
        bool isMidiAvailable() const { return midiAvailable_; }
        bool isAudioAvailable() const { return audioAvailable_; }
        bool isAsioAvailable() const { return asioAvailable_; }
        bool isRunning() const { return running_; }
        MidiInterface *getMidiInterface() { return midiInterface_.get(); }
        AudioSink *getAudioSink() { return audioSink_.get(); }

        // Control
        void setRunning(bool running) { running_ = running; }

    protected:
        std::atomic<bool> running_;

        // Audio
        std::unique_ptr<AudioSink> audioSink_;
        bool audioAvailable_;
        bool asioAvailable_;

        // MIDI
        std::unique_ptr<MidiInterface> midiInterface_;
        bool midiAvailable_;

        //--------------------------------------------------------------------
        // Inicializar MIDI com detecção automática de Launchpad X
        //--------------------------------------------------------------------
        void initializeMidiBase()
        {
            std::cout << "--- MIDI INITIALIZATION ---\n";

            midiInterface_ = std::make_unique<MidiInterface>();

            if (midiInterface_->init() == 0)
            {
                int numDevices = midiInGetNumDevs();
                std::cout << "MIDI Devices found: " << numDevices << "\n";

                int launchpadIndex = -1;
                for (UINT i = 0; i < static_cast<UINT>(numDevices); i++)
                {
                    MIDIINCAPSA caps;
                    if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                    {
                        std::string name = caps.szPname;
                        std::cout << "  [" << i << "] " << name;

                        // Procurar Launchpad X (LPX MIDI)
                        if (name.find("LPX") != std::string::npos ||
                            name.find("Launchpad X") != std::string::npos ||
                            name.find("Launchpad") != std::string::npos)
                        {
                            if (launchpadIndex == -1)
                            {
                                launchpadIndex = static_cast<int>(i);
                                std::cout << " <-- Launchpad X";
                            }
                        }
                        std::cout << "\n";
                    }
                }

                // Usar Launchpad X se encontrado, senão primeiro dispositivo
                int deviceToOpen = (launchpadIndex >= 0) ? launchpadIndex : 0;

                if (numDevices > 0 && midiInterface_->open(deviceToOpen) == 0)
                {
                    midiAvailable_ = true;

                    MIDIINCAPSA caps;
                    if (midiInGetDevCapsA(deviceToOpen, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                    {
                        std::cout << "[OK] MIDI opened: " << caps.szPname << "\n";
                    }
                    else
                    {
                        std::cout << "[OK] MIDI input opened\n";
                    }
                }
                else
                {
                    std::cout << "[WARN] No MIDI device available\n";
                }
            }

            if (!midiAvailable_)
            {
                std::cout << "[INFO] MIDI not available\n";
            }
        }

        //--------------------------------------------------------------------
        // Inicializar Audio com ASIO preferido (VB-Matrix VASIO-32)
        //--------------------------------------------------------------------
        void initializeAudioBase(AudioCallback callback = nullptr)
        {
            std::cout << "\n--- AUDIO INITIALIZATION ---\n";

            audioSink_ = std::make_unique<AudioSink>();

            // Selecionar driver ASIO específico: VB-Matrix VASIO-32
            std::cout << "Selecting ASIO driver: VB-Matrix VASIO-32...\n";
            if (selectAsioDriverByName("VASIO-32"))
            {
                std::cout << "[OK] VB-Matrix VASIO-32 selected\n";
            }
            else
            {
                std::cout << "[WARN] VB-Matrix VASIO-32 not found, will use first available\n";
            }

            // Tentar ASIO
            std::cout << "Initializing ASIO backend...\n";
            audioSink_->setBackend(AudioBackendType::ASIO);

            if (audioSink_->init() == 0)
            {
                audioAvailable_ = true;
                asioAvailable_ = true;
                std::cout << "[OK] ASIO backend initialized\n";
            }
            else
            {
                std::cout << "[WARN] ASIO not available, trying DirectSound...\n";

                // Fallback para DirectSound
                audioSink_->setBackend(AudioBackendType::DirectSound);
                if (audioSink_->init() == 0)
                {
                    audioAvailable_ = true;
                    std::cout << "[OK] DirectSound backend initialized\n";
                }
                else
                {
                    // Fallback para WaveOut
                    std::cout << "[WARN] DirectSound failed, trying WaveOut...\n";
                    audioSink_->setBackend(AudioBackendType::WaveOut);
                    if (audioSink_->init() == 0)
                    {
                        audioAvailable_ = true;
                        std::cout << "[OK] WaveOut backend initialized\n";
                    }
                    else
                    {
                        std::cout << "[ERROR] No audio backend available!\n";
                    }
                }
            }

            // Configurar callback se fornecido
            if (audioAvailable_ && callback)
            {
                audioSink_->setAudioCallback(callback);
            }
        }

        //--------------------------------------------------------------------
        // Iniciar audio
        //--------------------------------------------------------------------
        void startAudio()
        {
            if (audioSink_ && audioAvailable_)
            {
                audioSink_->start();
            }
        }

        //--------------------------------------------------------------------
        // Parar audio
        //--------------------------------------------------------------------
        void stopAudio()
        {
            if (audioSink_)
            {
                audioSink_->stop();
            }
        }

        //--------------------------------------------------------------------
        // Shutdown base
        //--------------------------------------------------------------------
        void shutdownBase()
        {
            running_ = false;

            if (midiInterface_)
            {
                midiInterface_->close();
            }

            stopAudio();
        }

        //--------------------------------------------------------------------
        // Listar dispositivos disponíveis
        //--------------------------------------------------------------------
        void listDevices()
        {
            std::cout << "\n--- AVAILABLE DEVICES ---\n";

            // MIDI devices
            int numMidi = midiInGetNumDevs();
            std::cout << "MIDI Input Devices (" << numMidi << "):\n";
            for (int i = 0; i < numMidi; i++)
            {
                MIDIINCAPSA caps;
                if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                {
                    std::cout << "  [" << i << "] " << caps.szPname << "\n";
                }
            }

            // Audio devices
            std::cout << "\nAudio Output:\n";
            if (audioSink_)
            {
                AudioSink::listDevices();
            }

            std::cout << "\n";
        }
    };

} // namespace synth
