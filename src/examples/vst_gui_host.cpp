//------------------------------------------------------------------------
// VST3 GUI Host - Carregamento direto via Windows API
//
// Carrega plugins VST3 usando LoadLibrary diretamente, sem depender
// das bibliotecas de hosting do SDK.
//------------------------------------------------------------------------

#include <windows.h>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <mutex>
#include <cmath>
#include <chrono>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>

// For screenshots
#include <wingdi.h>
#pragma comment(lib, "gdi32.lib")

// VST3 SDK interfaces only (no hosting classes)
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/gui/iplugview.h"

// Define IIDs needed (normally in funknown.cpp from SDK)
namespace Steinberg
{
    DEF_CLASS_IID(FUnknown)
    DEF_CLASS_IID(IPluginBase)
    DEF_CLASS_IID(IPlugFrame)
    DEF_CLASS_IID(IPlugView)

    namespace Vst
    {
        DEF_CLASS_IID(IComponent)
        DEF_CLASS_IID(IAudioProcessor)
        DEF_CLASS_IID(IEditController)
        DEF_CLASS_IID(IEventList)
        DEF_CLASS_IID(IComponentHandler)
        DEF_CLASS_IID(IParamValueQueue)
        DEF_CLASS_IID(IParameterChanges)
    }
}

// Host base com MIDI e Audio comum
#include "host_base.hpp"
#include "utils/wav_writer.hpp"

using namespace Steinberg;

namespace synth
{

    //------------------------------------------------------------------------
    // SimpleParamValueQueue - Fila de mudanças para um parâmetro
    //------------------------------------------------------------------------
    class SimpleParamValueQueue : public Vst::IParamValueQueue
    {
    public:
        SimpleParamValueQueue(Vst::ParamID id) : id_(id), refCount_(1) {}

        tresult PLUGIN_API queryInterface(const TUID _iid, void **obj) override
        {
            if (memcmp(_iid, Vst::IParamValueQueue::iid.toTUID(), 16) == 0 ||
                memcmp(_iid, FUnknown::iid.toTUID(), 16) == 0)
            {
                *obj = static_cast<Vst::IParamValueQueue *>(this);
                addRef();
                return kResultOk;
            }
            *obj = nullptr;
            return kNoInterface;
        }
        uint32 PLUGIN_API addRef() override { return ++refCount_; }
        uint32 PLUGIN_API release() override
        {
            uint32 count = --refCount_;
            if (count == 0)
                delete this;
            return count;
        }

        Vst::ParamID PLUGIN_API getParameterId() override { return id_; }
        int32 PLUGIN_API getPointCount() override { return static_cast<int32>(points_.size()); }

        tresult PLUGIN_API getPoint(int32 index, int32 &sampleOffset, Vst::ParamValue &value) override
        {
            if (index < 0 || index >= static_cast<int32>(points_.size()))
                return kInvalidArgument;
            sampleOffset = points_[index].first;
            value = points_[index].second;
            return kResultOk;
        }

        tresult PLUGIN_API addPoint(int32 sampleOffset, Vst::ParamValue value, int32 &index) override
        {
            index = static_cast<int32>(points_.size());
            points_.push_back({sampleOffset, value});
            return kResultOk;
        }

    private:
        Vst::ParamID id_;
        std::vector<std::pair<int32, Vst::ParamValue>> points_;
        std::atomic<uint32> refCount_;
    };

    //------------------------------------------------------------------------
    // SimpleParameterChanges - Container de mudanças de parâmetros
    //------------------------------------------------------------------------
    class SimpleParameterChanges : public Vst::IParameterChanges
    {
    public:
        SimpleParameterChanges() : refCount_(1) {}

        tresult PLUGIN_API queryInterface(const TUID _iid, void **obj) override
        {
            if (memcmp(_iid, Vst::IParameterChanges::iid.toTUID(), 16) == 0 ||
                memcmp(_iid, FUnknown::iid.toTUID(), 16) == 0)
            {
                *obj = static_cast<Vst::IParameterChanges *>(this);
                addRef();
                return kResultOk;
            }
            *obj = nullptr;
            return kNoInterface;
        }
        uint32 PLUGIN_API addRef() override { return ++refCount_; }
        uint32 PLUGIN_API release() override
        {
            uint32 count = --refCount_;
            if (count == 0)
                delete this;
            return count;
        }

        int32 PLUGIN_API getParameterCount() override { return static_cast<int32>(queues_.size()); }

        Vst::IParamValueQueue *PLUGIN_API getParameterData(int32 index) override
        {
            if (index < 0 || index >= static_cast<int32>(queues_.size()))
                return nullptr;
            return queues_[index].get();
        }

        Vst::IParamValueQueue *PLUGIN_API addParameterData(const Vst::ParamID &id, int32 &index) override
        {
            index = static_cast<int32>(queues_.size());
            queues_.push_back(std::make_unique<SimpleParamValueQueue>(id));
            return queues_.back().get();
        }

        void addChange(Vst::ParamID id, Vst::ParamValue value)
        {
            for (auto &q : queues_)
            {
                if (q->getParameterId() == id)
                {
                    int32 idx;
                    q->addPoint(0, value, idx);
                    return;
                }
            }
            int32 idx;
            auto *queue = addParameterData(id, idx);
            queue->addPoint(0, value, idx);
        }

        bool empty() const { return queues_.empty(); }

    private:
        std::vector<std::unique_ptr<SimpleParamValueQueue>> queues_;
        std::atomic<uint32> refCount_;
    };

    //------------------------------------------------------------------------
    // SimpleComponentHandler - Recebe mudanças de parâmetro da GUI
    //------------------------------------------------------------------------
    class SimpleComponentHandler : public Vst::IComponentHandler
    {
    public:
        SimpleComponentHandler() : refCount_(1) {}

        tresult PLUGIN_API queryInterface(const TUID _iid, void **obj) override
        {
            if (memcmp(_iid, Vst::IComponentHandler::iid.toTUID(), 16) == 0 ||
                memcmp(_iid, FUnknown::iid.toTUID(), 16) == 0)
            {
                *obj = static_cast<Vst::IComponentHandler *>(this);
                addRef();
                return kResultOk;
            }
            *obj = nullptr;
            return kNoInterface;
        }
        uint32 PLUGIN_API addRef() override { return ++refCount_; }
        uint32 PLUGIN_API release() override
        {
            uint32 count = --refCount_;
            if (count == 0)
                delete this;
            return count;
        }

        tresult PLUGIN_API beginEdit(Vst::ParamID /*id*/) override { return kResultOk; }

        tresult PLUGIN_API performEdit(Vst::ParamID id, Vst::ParamValue value) override
        {
            std::cout << "[HANDLER] performEdit id=" << id << " value=" << value << "\n"
                      << std::flush;
            std::lock_guard<std::mutex> lock(mutex_);
            pending_[id] = value; // store latest value; overwrite if repeated
            return kResultOk;
        }

        tresult PLUGIN_API endEdit(Vst::ParamID /*id*/) override { return kResultOk; }

        tresult PLUGIN_API restartComponent(int32 /*flags*/) override { return kResultOk; }

        // Called from audio thread — atomically drains pending changes
        std::unordered_map<Vst::ParamID, Vst::ParamValue> drainChanges()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return std::move(pending_);
        }

    private:
        std::mutex mutex_;
        std::unordered_map<Vst::ParamID, Vst::ParamValue> pending_;
        std::atomic<uint32> refCount_;
    };

    //------------------------------------------------------------------------
    // SimpleEventList - Implementação simples de IEventList para enviar MIDI
    //------------------------------------------------------------------------
    class SimpleEventList : public Vst::IEventList
    {
    public:
        SimpleEventList() : refCount_(1) {}

        // FUnknown
        tresult PLUGIN_API queryInterface(const TUID _iid, void **obj) override
        {
            if (memcmp(_iid, Vst::IEventList::iid.toTUID(), 16) == 0 ||
                memcmp(_iid, FUnknown::iid.toTUID(), 16) == 0)
            {
                *obj = static_cast<Vst::IEventList *>(this);
                addRef();
                return kResultOk;
            }
            *obj = nullptr;
            return kNoInterface;
        }
        uint32 PLUGIN_API addRef() override { return ++refCount_; }
        uint32 PLUGIN_API release() override
        {
            uint32 count = --refCount_;
            if (count == 0)
                delete this;
            return count;
        }

        // IEventList
        int32 PLUGIN_API getEventCount() override { return static_cast<int32>(events_.size()); }

        tresult PLUGIN_API getEvent(int32 index, Vst::Event &e) override
        {
            if (index < 0 || index >= static_cast<int32>(events_.size()))
                return kInvalidArgument;
            e = events_[index];
            return kResultOk;
        }

        tresult PLUGIN_API addEvent(Vst::Event &e) override
        {
            events_.push_back(e);
            return kResultOk;
        }

        void clear() { events_.clear(); }

        void addNoteOn(int16 pitch, float velocity, int32 sampleOffset = 0)
        {
            Vst::Event e = {};
            e.type = Vst::Event::kNoteOnEvent;
            e.sampleOffset = sampleOffset;
            e.noteOn.channel = 0;
            e.noteOn.pitch = pitch;
            e.noteOn.velocity = velocity;
            e.noteOn.noteId = pitch;
            events_.push_back(e);
        }

        void addNoteOff(int16 pitch, float velocity = 0.0f, int32 sampleOffset = 0)
        {
            Vst::Event e = {};
            e.type = Vst::Event::kNoteOffEvent;
            e.sampleOffset = sampleOffset;
            e.noteOff.channel = 0;
            e.noteOff.pitch = pitch;
            e.noteOff.velocity = velocity;
            e.noteOff.noteId = pitch;
            events_.push_back(e);
        }

    private:
        std::vector<Vst::Event> events_;
        std::atomic<uint32> refCount_;
    };

    //------------------------------------------------------------------------
    // PlugFrame - Implementação do IPlugFrame
    //------------------------------------------------------------------------
    class PlugFrame : public IPlugFrame
    {
    public:
        PlugFrame(HWND hwnd) : hwnd_(hwnd), refCount_(1) {}

        tresult PLUGIN_API queryInterface(const TUID _iid, void **obj) override
        {
            if (memcmp(_iid, IPlugFrame::iid.toTUID(), 16) == 0 ||
                memcmp(_iid, FUnknown::iid.toTUID(), 16) == 0)
            {
                *obj = static_cast<IPlugFrame *>(this);
                addRef();
                return kResultOk;
            }
            *obj = nullptr;
            return kNoInterface;
        }

        uint32 PLUGIN_API addRef() override { return ++refCount_; }

        uint32 PLUGIN_API release() override
        {
            uint32 count = --refCount_;
            if (count == 0)
                delete this;
            return count;
        }

        tresult PLUGIN_API resizeView(IPlugView *view, ViewRect *newSize) override
        {
            if (!view || !newSize)
                return kInvalidArgument;

            int width = newSize->right - newSize->left;
            int height = newSize->bottom - newSize->top;

            RECT rect = {0, 0, width, height};
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

            SetWindowPos(hwnd_, nullptr, 0, 0,
                         rect.right - rect.left, rect.bottom - rect.top,
                         SWP_NOMOVE | SWP_NOZORDER);

            return kResultOk;
        }

    private:
        HWND hwnd_;
        std::atomic<uint32> refCount_;
    };

    //------------------------------------------------------------------------
    // VST3 Host Simples - GUI Only (herda de HostBase)
    //------------------------------------------------------------------------
    class VST3GUIHost : public HostBase
    {
    public:
        static constexpr int SAMPLE_RATE = 48000;
        static constexpr int BLOCK_SIZE = 512;

        VST3GUIHost() : hwnd_(nullptr), plugView_(nullptr),
                        hModule_(nullptr), component_(nullptr), controller_(nullptr),
                        processor_(nullptr), factory_(nullptr), plugFrame_(nullptr),
                        componentHandler_(nullptr) {}

        Vst::IAudioProcessor *getProcessor() { return processor_; }

        ~VST3GUIHost()
        {
            shutdown();
        }

        bool initialize(const std::string &pluginPath)
        {
            printBanner();

            // 1. Carregar o plugin VST3
            if (!loadPlugin(pluginPath))
            {
                std::cout << "[ERROR] Failed to load plugin\n";
                return false;
            }

            // 2. Inicializar MIDI (da classe base)
            initializeMidiBase();

            // Configurar callback MIDI específico para VST3
            if (isMidiAvailable())
            {
                getMidiInterface()->setNoteCallback([this](const MidiNote &note)
                                                    {
                                                        std::cout << "[MIDI] Note " << (note.is_note_on ? "ON" : "OFF")
                                                                  << " " << static_cast<int>(note.note)
                                                                  << " vel=" << static_cast<int>(note.velocity) << "\n";
                                                        
                                                        // Enviar nota MIDI para o plugin
                                                        if (processor_) {
                                                            std::lock_guard<std::mutex> lock(notesMutex_);
                                                            pendingNotes_.push_back(note);
                                                        } });
            }

            // 3. Inicializar Audio (da classe base) com callback que processa via VST3
            initializeAudioBase([this](float *buffer, int frames, int channels)
                                { processAudioVST3(buffer, frames, channels); });

            // 4. Criar janela
            if (!createWindow())
            {
                std::cout << "[ERROR] Failed to create window\n";
                return false;
            }

            // 5. Anexar GUI do plugin
            if (!attachPluginView())
            {
                std::cout << "[INFO] Plugin has no GUI or GUI attachment failed\n";
                // Continua mesmo sem GUI
            }

            return true;
        }

        void run()
        {
            setRunning(true);

            startAudio();

            std::cout << "\n[READY] Plugin GUI is open. Close the window to exit.\n";
            std::cout << "[INFO] MIDI input and audio processing active.\n";
            std::cout << "\n--- DEBUG KEYS (focus plugin window) ---\n";
            std::cout << "  R     : Toggle WAV recording (max 30s)\n";
            std::cout << "  S/F5  : Take manual screenshot\n";
            std::cout << "  A     : Toggle auto-screenshot (every 5s)\n";
            std::cout << "----------------------------------------\n\n";

            MSG msg = {};
            while (isRunning() && GetMessage(&msg, nullptr, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            stopAudio();
        }

        void shutdown()
        {
            setRunning(false);

            if (plugView_)
            {
                plugView_->removed();
                plugView_->release();
                plugView_ = nullptr;
            }

            if (controller_)
            {
                controller_->terminate();
                controller_->release();
                controller_ = nullptr;
            }

            if (componentHandler_)
            {
                componentHandler_->release();
                componentHandler_ = nullptr;
            }

            if (processor_)
            {
                processor_->release();
                processor_ = nullptr;
            }

            if (component_)
            {
                component_->setActive(false);
                component_->terminate();
                component_->release();
                component_ = nullptr;
            }

            if (factory_)
            {
                factory_->release();
                factory_ = nullptr;
            }

            // Shutdown MIDI e Audio via classe base
            shutdownBase();

            if (hwnd_)
            {
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }

            if (hModule_)
            {
                // Chamar exit function se existir
                auto exitProc = (bool(PLUGIN_API *)())GetProcAddress(hModule_, "ExitDll");
                if (exitProc)
                    exitProc();

                FreeLibrary(hModule_);
                hModule_ = nullptr;
            }
        }

    private:
        // VST3
        HMODULE hModule_;
        IPluginFactory *factory_;
        Vst::IComponent *component_;
        Vst::IEditController *controller_;
        Vst::IAudioProcessor *processor_;
        IPlugView *plugView_;
        PlugFrame *plugFrame_;
        SimpleComponentHandler *componentHandler_;

        // MIDI pending notes
        std::vector<MidiNote> pendingNotes_;
        std::mutex notesMutex_;

        // Audio buffers
        std::vector<float> outputBufferL_;
        std::vector<float> outputBufferR_;

        // Event list for VST3
        SimpleEventList eventList_;

        // Debug: Audio recording
        std::atomic<bool> isRecording_{false};
        std::vector<float> recordBuffer_;
        std::mutex recordMutex_;
        int recordSampleCount_ = 0;
        static constexpr int MAX_RECORD_SECONDS = 30;

        // Debug: Screenshot
        int screenshotCounter_ = 0;
        std::atomic<bool> autoScreenshot_{true}; // Auto screenshot enabled by default
        static constexpr UINT_PTR TIMER_AUTO_SCREENSHOT = 1;
        static constexpr UINT AUTO_SCREENSHOT_INTERVAL_MS = 5000; // 5 seconds

        // Window
        HWND hwnd_;

        void printBanner()
        {
            std::cout << "\n";
            std::cout << "================================================================\n";
            std::cout << "          VST3 GUI HOST - Plugin Interface Viewer\n";
            std::cout << "          ObsSynth Development Environment\n";
            std::cout << "================================================================\n";
            std::cout << "\n";
        }

        //--------------------------------------------------------------------
        // Debug: Get timestamp string for filenames
        //--------------------------------------------------------------------
        std::string getTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
            localtime_s(&tm_buf, &time);
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
            return oss.str();
        }

        //--------------------------------------------------------------------
        // Debug: Start/Stop audio recording
        //--------------------------------------------------------------------
        void startRecording()
        {
            if (isRecording_)
                return;

            std::lock_guard<std::mutex> lock(recordMutex_);
            recordBuffer_.clear();
            recordBuffer_.reserve(SAMPLE_RATE * 2 * MAX_RECORD_SECONDS); // Stereo
            recordSampleCount_ = 0;
            isRecording_ = true;
            std::cout << "\n[REC] Recording started (max " << MAX_RECORD_SECONDS << "s)...\n";
        }

        void stopRecording()
        {
            if (!isRecording_)
                return;

            isRecording_ = false;

            // Save to WAV file
            std::string filename = "debug_audio_" + getTimestamp() + ".wav";

            std::lock_guard<std::mutex> lock(recordMutex_);
            if (recordBuffer_.empty())
            {
                std::cout << "[REC] No audio recorded\n";
                return;
            }

            WAVConfig config = WAVWriter::createConfig(SAMPLE_RATE, 2, 16);
            int result = WAVWriter::writeFile(filename, recordBuffer_,
                                              static_cast<int>(recordBuffer_.size()), config);

            if (result == 0)
            {
                float duration = static_cast<float>(recordSampleCount_) / SAMPLE_RATE;
                std::cout << "[REC] Saved: " << filename << " ("
                          << std::fixed << std::setprecision(2) << duration << "s, "
                          << recordBuffer_.size() << " samples)\n";
            }
            else
            {
                std::cout << "[REC] Error saving: " << filename << "\n";
            }
            recordBuffer_.clear();
        }

        void toggleRecording()
        {
            if (isRecording_)
            {
                stopRecording();
            }
            else
            {
                startRecording();
            }
        }

        //--------------------------------------------------------------------
        // Debug: Take screenshot of plugin window
        //--------------------------------------------------------------------
        void takeScreenshot()
        {
            if (!hwnd_)
                return;

            RECT rect;
            GetClientRect(hwnd_, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            if (width <= 0 || height <= 0)
                return;

            HDC hdcScreen = GetDC(nullptr); // Screen DC for compatibility
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
            HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

            // PrintWindow with PW_RENDERFULLCONTENT captures Direct2D/DX composited content
            // Force a repaint first to ensure content is rendered
            RedrawWindow(hwnd_, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
            BOOL printed = PrintWindow(hwnd_, hdcMem, PW_CLIENTONLY | 0x00000002 /* PW_RENDERFULLCONTENT */);
            if (!printed)
            {
                // Fallback: BitBlt from screen coordinates
                POINT ptOrigin = {0, 0};
                ClientToScreen(hwnd_, &ptOrigin);
                BitBlt(hdcMem, 0, 0, width, height, hdcScreen, ptOrigin.x, ptOrigin.y, SRCCOPY);
            }

            // Generate filename
            std::string filename = "debug_screenshot_" + getTimestamp() + "_" + std::to_string(++screenshotCounter_) + ".bmp";

            // Save as BMP
            BITMAPINFOHEADER bi = {};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = width;
            bi.biHeight = -height; // Top-down
            bi.biPlanes = 1;
            bi.biBitCount = 24;
            bi.biCompression = BI_RGB;

            int rowSize = ((width * 3 + 3) / 4) * 4; // Align to 4 bytes
            int imageSize = rowSize * height;
            std::vector<uint8_t> pixels(imageSize);

            GetDIBits(hdcMem, hBitmap, 0, height, pixels.data(), (BITMAPINFO *)&bi, DIB_RGB_COLORS);

            // Write BMP file
            BITMAPFILEHEADER bf = {};
            bf.bfType = 0x4D42; // 'BM'
            bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
            bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

            std::ofstream file(filename, std::ios::binary);
            file.write(reinterpret_cast<char *>(&bf), sizeof(bf));
            file.write(reinterpret_cast<char *>(&bi), sizeof(bi));
            file.write(reinterpret_cast<char *>(pixels.data()), imageSize);
            file.close();

            // Cleanup
            SelectObject(hdcMem, hOld);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            ReleaseDC(nullptr, hdcScreen);

            std::cout << "[SCREENSHOT] Saved: " << filename << " (" << width << "x" << height << ")\n";
        }

        //--------------------------------------------------------------------
        // Processar Audio via VST3 IAudioProcessor
        //--------------------------------------------------------------------
        void processAudioVST3(float *buffer, int frames, int channels)
        {
            if (!processor_)
            {
                memset(buffer, 0, frames * channels * sizeof(float));
                return;
            }

            // Ensure buffers are sized
            if (outputBufferL_.size() < static_cast<size_t>(frames))
            {
                outputBufferL_.resize(frames);
                outputBufferR_.resize(frames);
            }

            // Clear output buffers
            memset(outputBufferL_.data(), 0, frames * sizeof(float));
            memset(outputBufferR_.data(), 0, frames * sizeof(float));

            // Process pending MIDI notes
            eventList_.clear();
            {
                std::lock_guard<std::mutex> lock(notesMutex_);
                for (const auto &note : pendingNotes_)
                {
                    if (note.is_note_on && note.velocity > 0)
                    {
                        eventList_.addNoteOn(note.note, note.velocity / 127.0f);
                    }
                    else
                    {
                        eventList_.addNoteOff(note.note);
                    }
                }
                pendingNotes_.clear();
            }

            // Setup process data
            Vst::ProcessData processData;
            processData.processMode = Vst::kRealtime;
            processData.symbolicSampleSize = Vst::kSample32;
            processData.numSamples = frames;
            processData.numInputs = 0;
            processData.numOutputs = 1;
            processData.inputs = nullptr;

            // Drain GUI parameter changes and pass to processor
            SimpleParameterChanges paramChanges;
            if (componentHandler_)
            {
                auto pending = componentHandler_->drainChanges();
                for (auto &[id, value] : pending)
                {
                    std::cout << "[AUDIO] Routing param id=" << id << " value=" << value << " to processor\n"
                              << std::flush;
                    paramChanges.addChange(id, value);
                }
            }
            processData.inputParameterChanges = paramChanges.empty() ? nullptr : &paramChanges;

            processData.outputParameterChanges = nullptr;
            processData.inputEvents = &eventList_; // Send MIDI events!
            processData.outputEvents = nullptr;
            processData.processContext = nullptr;

            // Setup output bus
            Vst::AudioBusBuffers outputBus;
            outputBus.numChannels = 2;
            outputBus.silenceFlags = 0;
            float *outputChannels[2] = {outputBufferL_.data(), outputBufferR_.data()};
            outputBus.channelBuffers32 = outputChannels;
            processData.outputs = &outputBus;

            // Process audio
            processor_->process(processData);

            // Interleave output to buffer
            for (int i = 0; i < frames; i++)
            {
                buffer[i * 2] = outputBufferL_[i];
                buffer[i * 2 + 1] = outputBufferR_[i];
            }

            // Debug: Record audio if enabled
            if (isRecording_)
            {
                std::lock_guard<std::mutex> lock(recordMutex_);
                for (int i = 0; i < frames; i++)
                {
                    recordBuffer_.push_back(outputBufferL_[i]);
                    recordBuffer_.push_back(outputBufferR_[i]);
                }
                recordSampleCount_ += frames;

                // Auto-stop after MAX_RECORD_SECONDS
                if (recordSampleCount_ >= SAMPLE_RATE * MAX_RECORD_SECONDS)
                {
                    stopRecording();
                }
            }
        }

        //--------------------------------------------------------------------
        // Carregar Plugin VST3 (LoadLibrary direto)
        //--------------------------------------------------------------------
        bool loadPlugin(const std::string &path)
        {
            std::cout << "--- LOADING VST3 PLUGIN ---\n";

            // Encontrar o bundle VST3
            std::filesystem::path pluginPath(path);
            std::filesystem::path dllPath;

            if (pluginPath.extension() == ".vst3")
            {
                // É um bundle, procurar a DLL dentro
                dllPath = pluginPath / "Contents" / "x86_64-win" / (pluginPath.stem().string() + ".vst3");
                if (!std::filesystem::exists(dllPath))
                {
                    // Tentar arquitetura alternativa
                    dllPath = pluginPath / "Contents" / "x86-win" / (pluginPath.stem().string() + ".vst3");
                }
            }
            else
            {
                dllPath = pluginPath;
            }

            std::cout << "DLL Path: " << dllPath.string() << "\n";

            if (!std::filesystem::exists(dllPath))
            {
                std::cout << "[ERROR] Plugin DLL not found at: " << dllPath.string() << "\n";
                return false;
            }

            // Carregar DLL
            hModule_ = LoadLibraryW(dllPath.wstring().c_str());
            if (!hModule_)
            {
                DWORD err = GetLastError();
                std::cout << "[ERROR] LoadLibrary failed with error: " << err << "\n";
                return false;
            }

            std::cout << "[OK] DLL loaded\n";

            // Chamar InitDll se existir
            auto initProc = (bool(PLUGIN_API *)())GetProcAddress(hModule_, "InitDll");
            if (initProc)
            {
                if (!initProc())
                {
                    std::cout << "[WARN] InitDll returned false\n";
                }
            }

            // Obter GetPluginFactory
            auto getFactory = (IPluginFactory * (PLUGIN_API *)()) GetProcAddress(hModule_, "GetPluginFactory");
            if (!getFactory)
            {
                std::cout << "[ERROR] GetPluginFactory not found\n";
                FreeLibrary(hModule_);
                hModule_ = nullptr;
                return false;
            }

            factory_ = getFactory();
            if (!factory_)
            {
                std::cout << "[ERROR] GetPluginFactory returned null\n";
                return false;
            }

            std::cout << "[OK] Plugin factory obtained\n";

            // Enumerar classes
            int32 classCount = factory_->countClasses();
            std::cout << "Available classes: " << classCount << "\n";

            PClassInfo classInfo;
            TUID componentCID = {0};
            TUID controllerCID = {0};
            bool foundComponent = false;

            for (int32 i = 0; i < classCount; i++)
            {
                if (factory_->getClassInfo(i, &classInfo) == kResultOk)
                {
                    std::cout << "  [" << i << "] " << classInfo.name
                              << " (" << classInfo.category << ")\n";

                    // Procurar componente de áudio
                    if (strcmp(classInfo.category, kVstAudioEffectClass) == 0)
                    {
                        memcpy(componentCID, classInfo.cid, sizeof(TUID));
                        foundComponent = true;
                        std::cout << "       ^ Audio Processor found\n";
                    }
                }
            }

            if (!foundComponent)
            {
                std::cout << "[ERROR] No audio processor class found\n";
                return false;
            }

            // Criar componente
            if (factory_->createInstance(componentCID, Vst::IComponent::iid, (void **)&component_) != kResultOk)
            {
                std::cout << "[ERROR] Failed to create component instance\n";
                return false;
            }

            std::cout << "[OK] Component created\n";

            // Inicializar componente
            if (component_->initialize(nullptr) != kResultOk)
            {
                std::cout << "[ERROR] Component initialization failed\n";
                return false;
            }

            std::cout << "[OK] Component initialized\n";

            // Obter controller CID
            if (component_->getControllerClassId(controllerCID) == kResultOk)
            {
                // Criar controller separado
                if (factory_->createInstance(controllerCID, Vst::IEditController::iid, (void **)&controller_) == kResultOk)
                {
                    controller_->initialize(nullptr);
                    std::cout << "[OK] Separate controller created\n";
                }
            }

            // Se não há controller separado, tentar query interface
            if (!controller_)
            {
                if (component_->queryInterface(Vst::IEditController::iid, (void **)&controller_) == kResultOk)
                {
                    std::cout << "[OK] Using combined component/controller\n";
                }
                else
                {
                    std::cout << "[WARN] No controller available - no GUI possible\n";
                }
            }

            // Registar ComponentHandler para que a GUI passe parâmetros ao processor
            if (controller_)
            {
                componentHandler_ = new SimpleComponentHandler();
                if (controller_->setComponentHandler(componentHandler_) == kResultOk)
                    std::cout << "[OK] ComponentHandler registered\n";
                else
                    std::cout << "[WARN] setComponentHandler failed\n";
            }

            // Obter IAudioProcessor
            if (component_->queryInterface(Vst::IAudioProcessor::iid, (void **)&processor_) != kResultOk)
            {
                std::cout << "[ERROR] Failed to get IAudioProcessor\n";
                return false;
            }
            std::cout << "[OK] Audio processor obtained\n";

            // Configurar processamento de áudio
            Vst::ProcessSetup setup;
            setup.processMode = Vst::kRealtime;
            setup.symbolicSampleSize = Vst::kSample32;
            setup.maxSamplesPerBlock = BLOCK_SIZE;
            setup.sampleRate = SAMPLE_RATE;

            if (processor_->setupProcessing(setup) != kResultOk)
            {
                std::cout << "[WARN] setupProcessing failed\n";
            }

            // Ativar componente
            component_->setActive(true);
            std::cout << "[OK] Component activated\n";

            return true;
        }

        //--------------------------------------------------------------------
        // Criar Janela
        //--------------------------------------------------------------------
        bool createWindow()
        {
            std::cout << "\n--- CREATING GUI WINDOW ---\n";

            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
            wc.lpszClassName = L"VST3GUIHost";

            RegisterClassExW(&wc);

            hwnd_ = CreateWindowExW(
                0, L"VST3GUIHost", L"ObsSynth - VST3 Plugin",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                nullptr, nullptr, GetModuleHandle(nullptr), this);

            if (!hwnd_)
            {
                std::cout << "[ERROR] CreateWindow failed\n";
                return false;
            }

            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);

            // Start auto-screenshot timer
            if (autoScreenshot_)
            {
                SetTimer(hwnd_, TIMER_AUTO_SCREENSHOT, AUTO_SCREENSHOT_INTERVAL_MS, nullptr);
                std::cout << "[DEBUG] Auto-screenshot enabled (every " << (AUTO_SCREENSHOT_INTERVAL_MS / 1000) << "s)\n";
            }

            std::cout << "[OK] Window created\n";
            return true;
        }

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto host = reinterpret_cast<VST3GUIHost *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            switch (msg)
            {
            case WM_SIZE:
                if (host && host->plugView_)
                {
                    ViewRect rect = {0, 0, LOWORD(lParam), HIWORD(lParam)};
                    host->plugView_->onSize(&rect);
                }
                return 0;

            case WM_CLOSE:
                if (host)
                {
                    // Stop auto-screenshot timer
                    KillTimer(hwnd, TIMER_AUTO_SCREENSHOT);
                    // Stop recording before closing
                    if (host->isRecording_)
                    {
                        host->stopRecording();
                    }
                    host->setRunning(false);
                }
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_TIMER:
                if (host && wParam == TIMER_AUTO_SCREENSHOT && host->autoScreenshot_)
                {
                    host->takeScreenshot();
                }
                return 0;

            case WM_KEYDOWN:
                if (host)
                {
                    switch (wParam)
                    {
                    case 'R': // Toggle recording
                        host->toggleRecording();
                        return 0;
                    case 'S': // Manual screenshot
                        host->takeScreenshot();
                        return 0;
                    case VK_F5: // Also screenshot with F5
                        host->takeScreenshot();
                        return 0;
                    case 'A': // Toggle auto-screenshot
                        host->autoScreenshot_ = !host->autoScreenshot_;
                        if (host->autoScreenshot_)
                        {
                            SetTimer(hwnd, TIMER_AUTO_SCREENSHOT, AUTO_SCREENSHOT_INTERVAL_MS, nullptr);
                            std::cout << "[DEBUG] Auto-screenshot ENABLED\n";
                        }
                        else
                        {
                            KillTimer(hwnd, TIMER_AUTO_SCREENSHOT);
                            std::cout << "[DEBUG] Auto-screenshot DISABLED\n";
                        }
                        return 0;
                    }
                }
                break;
            }

            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        //--------------------------------------------------------------------
        // Anexar View do Plugin
        //--------------------------------------------------------------------
        bool attachPluginView()
        {
            if (!controller_)
            {
                std::cout << "[WARN] No controller - cannot create view\n";
                return false;
            }

            // Criar view
            plugView_ = controller_->createView(Vst::ViewType::kEditor);
            if (!plugView_)
            {
                std::cout << "[WARN] Controller has no editor view\n";
                return false;
            }

            std::cout << "[OK] Plugin view created\n";

            // Verificar suporte Win32
            if (plugView_->isPlatformTypeSupported(kPlatformTypeHWND) != kResultOk)
            {
                std::cout << "[ERROR] Plugin doesn't support Win32\n";
                plugView_->release();
                plugView_ = nullptr;
                return false;
            }

            // Criar frame
            plugFrame_ = new PlugFrame(hwnd_);
            plugView_->setFrame(plugFrame_);

            // Obter tamanho
            ViewRect size;
            if (plugView_->getSize(&size) == kResultOk)
            {
                int w = size.right - size.left;
                int h = size.bottom - size.top;

                RECT rect = {0, 0, w, h};
                AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

                SetWindowPos(hwnd_, nullptr, 0, 0,
                             rect.right - rect.left,
                             rect.bottom - rect.top,
                             SWP_NOMOVE | SWP_NOZORDER);

                std::cout << "[OK] Window resized to " << w << "x" << h << "\n";
            }

            // Anexar
            if (plugView_->attached(hwnd_, kPlatformTypeHWND) != kResultOk)
            {
                std::cout << "[ERROR] Failed to attach view to window\n";
                plugView_->release();
                plugView_ = nullptr;
                return false;
            }

            std::cout << "[OK] Plugin GUI attached!\n";

            // Pump message loop to let VSTGUI render at least one frame, then screenshot
            Sleep(100);
            MSG msg = {};
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            Sleep(800); // Give Direct2D time to compose
            takeScreenshot();
            std::cout << "[DEBUG] Initial screenshot captured\n";

            return true;
        }
    };

} // namespace synth

//------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    std::cout << "VST3 GUI Host v1.0\n";
    std::cout << "==================\n\n";

    // Determinar caminho do plugin
    std::string pluginPath;

    if (argc > 1)
    {
        pluginPath = argv[1];
    }
    else
    {
        // Caminhos padrão
        std::vector<std::string> searchPaths = {
            "build/VST3/Release/ObsSynth.vst3",
            "build/VST3/Debug/ObsSynth.vst3",
        };

        // Adicionar caminho instalado
        const char *localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData)
        {
            searchPaths.push_back(std::string(localAppData) + "/Programs/Common/VST3/ObsSynth.vst3");
        }

        // Procurar
        for (const auto &p : searchPaths)
        {
            if (std::filesystem::exists(p))
            {
                pluginPath = p;
                break;
            }
        }

        if (pluginPath.empty())
        {
            std::cout << "Usage: vst_gui_host.exe [path_to_vst3_plugin]\n\n";
            std::cout << "Searched paths:\n";
            for (const auto &p : searchPaths)
            {
                std::cout << "  - " << p << "\n";
            }
            std::cout << "\nPress Enter to exit...\n";
            std::cin.get();
            return 1;
        }
    }

    std::cout << "Loading: " << pluginPath << "\n\n";

    synth::VST3GUIHost host;

    if (!host.initialize(pluginPath))
    {
        std::cout << "\nFailed to initialize. Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    host.run();

    std::cout << "\n[EXIT] Goodbye!\n";
    return 0;
}
