#include "audio_backend.hpp"
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <combaseapi.h>

// ============================================================================
// ASIO SDK 2.3 Implementation with Real Driver Support
// ============================================================================

namespace synth
{
    // ------------------------------------------------------------------------
    // ASIO Types and Constants (from Steinberg ASIO SDK 2.3)
    // ------------------------------------------------------------------------
    typedef double ASIOSampleRate;
    typedef long ASIOBool;
    typedef long ASIOError;
    typedef long ASIOSampleType;

    enum ASIOError_e
    {
        ASE_OK = 0,
        ASE_SUCCESS = 0x3f4847a0,
        ASE_NotPresent = -1000,
        ASE_HWMalfunction,
        ASE_InvalidParameter,
        ASE_InvalidMode,
        ASE_SPNotAdvancing,
        ASE_NoClock,
        ASE_NoMemory
    };

    enum ASIOBool_e
    {
        ASIOFalse = 0,
        ASIOTrue = 1
    };

    enum ASIOSampleType_e
    {
        ASIOSTInt16MSB = 0,
        ASIOSTInt24MSB = 1,
        ASIOSTInt32MSB = 2,
        ASIOSTFloat32MSB = 3,
        ASIOSTFloat64MSB = 4,
        ASIOSTInt32MSB16 = 8,
        ASIOSTInt32MSB18 = 9,
        ASIOSTInt32MSB20 = 10,
        ASIOSTInt32MSB24 = 11,
        ASIOSTInt16LSB = 16,
        ASIOSTInt24LSB = 17,
        ASIOSTInt32LSB = 18,
        ASIOSTFloat32LSB = 19,
        ASIOSTFloat64LSB = 20,
        ASIOSTInt32LSB16 = 24,
        ASIOSTInt32LSB18 = 25,
        ASIOSTInt32LSB20 = 26,
        ASIOSTInt32LSB24 = 27,
        ASIOSTDSDInt8LSB1 = 32,
        ASIOSTDSDInt8MSB1 = 33,
        ASIOSTDSDInt8NER8 = 40
    };

    struct ASIOBufferInfo
    {
        ASIOBool isInput;
        long channelNum;
        void *buffers[2];
    };

    struct ASIOChannelInfo
    {
        long channel;
        ASIOBool isInput;
        ASIOBool isActive;
        long channelGroup;
        ASIOSampleType type;
        char name[32];
    };

    struct ASIODriverInfo
    {
        long asioVersion;
        long driverVersion;
        char name[32];
        char errorMessage[124];
        void *sysRef;
    };

    struct ASIOCallbacks
    {
        void (*bufferSwitch)(long doubleBufferIndex, ASIOBool directProcess);
        void (*sampleRateDidChange)(ASIOSampleRate sRate);
        long (*asioMessage)(long selector, long value, void *message, double *opt);
        void *(*bufferSwitchTimeInfo)(void *params, long doubleBufferIndex, ASIOBool directProcess);
    };

    struct ASIOClockSource
    {
        long index;
        long associatedChannel;
        long associatedGroup;
        ASIOBool isCurrentSource;
        char name[32];
    };

    struct ASIOSamples
    {
        unsigned long hi;
        unsigned long lo;
    };

    struct ASIOTimeStamp
    {
        unsigned long hi;
        unsigned long lo;
    };

    // ASIO Message selectors
    enum
    {
        kAsioSelectorSupported = 1,
        kAsioEngineVersion,
        kAsioResetRequest,
        kAsioBufferSizeChange,
        kAsioResyncRequest,
        kAsioLatenciesChanged,
        kAsioSupportsTimeInfo,
        kAsioSupportsTimeCode
    };

    // ------------------------------------------------------------------------
    // IASIO Interface, after the Steinberg ASIO SDK 2.3.4 (iasiodrv.h), used under its GPL-3.0 option
    // ------------------------------------------------------------------------
    interface IASIO : public IUnknown
    {
        virtual ASIOBool init(void *sysHandle) = 0;
        virtual void getDriverName(char *name) = 0;
        virtual long getDriverVersion() = 0;
        virtual void getErrorMessage(char *string) = 0;
        virtual ASIOError start() = 0;
        virtual ASIOError stop() = 0;
        virtual ASIOError getChannels(long *numInputChannels, long *numOutputChannels) = 0;
        virtual ASIOError getLatencies(long *inputLatency, long *outputLatency) = 0;
        virtual ASIOError getBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity) = 0;
        virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) = 0;
        virtual ASIOError getSampleRate(ASIOSampleRate * sampleRate) = 0;
        virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) = 0;
        virtual ASIOError getClockSources(ASIOClockSource * clocks, long *numSources) = 0;
        virtual ASIOError setClockSource(long reference) = 0;
        virtual ASIOError getSamplePosition(ASIOSamples * sPos, ASIOTimeStamp * tStamp) = 0;
        virtual ASIOError getChannelInfo(ASIOChannelInfo * info) = 0;
        virtual ASIOError createBuffers(ASIOBufferInfo * bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks) = 0;
        virtual ASIOError disposeBuffers() = 0;
        virtual ASIOError controlPanel() = 0;
        virtual ASIOError future(long selector, void *opt) = 0;
        virtual ASIOError outputReady() = 0;
    };

// ------------------------------------------------------------------------
// ASIO Driver Registry Management
// ------------------------------------------------------------------------
#define ASIO_PATH "software\\asio"
#define COM_CLSID "clsid"
#define ASIODRV_DESC "description"
#define INPROC_SERVER "InprocServer32"
#define MAXPATHLEN 512
#define MAXDRVNAMELEN 128

    struct AsioDriverInfo
    {
        int id;
        CLSID clsid;
        char dllPath[MAXPATHLEN];
        char name[MAXDRVNAMELEN];
        IASIO *driver;
    };

    // ------------------------------------------------------------------------
    // Real ASIO Driver Manager - Enumerates and connects to system drivers
    // ------------------------------------------------------------------------
    class RealAsioDriverManager
    {
    private:
        std::vector<AsioDriverInfo> drivers_;
        IASIO *currentDriver_ = nullptr;
        int currentDriverIndex_ = -1;
        bool comInitialized_ = false;

        // Find DLL path from CLSID in registry
        bool findDriverPath(const char *clsidStr, char *dllPath, int pathSize)
        {
            HKEY hkEnum, hksub, hkpath;
            char databuf[512];
            DWORD datatype, datasize;
            bool found = false;

            char clsidLower[64];
            strncpy_s(clsidLower, clsidStr, sizeof(clsidLower) - 1);
            CharLowerBuffA(clsidLower, static_cast<DWORD>(strlen(clsidLower)));

            if (RegOpenKeyA(HKEY_CLASSES_ROOT, COM_CLSID, &hkEnum) == ERROR_SUCCESS)
            {
                DWORD index = 0;
                while (RegEnumKeyA(hkEnum, index++, databuf, sizeof(databuf)) == ERROR_SUCCESS)
                {
                    CharLowerBuffA(databuf, static_cast<DWORD>(strlen(databuf)));
                    if (strcmp(databuf, clsidLower) == 0)
                    {
                        if (RegOpenKeyExA(hkEnum, databuf, 0, KEY_READ, &hksub) == ERROR_SUCCESS)
                        {
                            if (RegOpenKeyExA(hksub, INPROC_SERVER, 0, KEY_READ, &hkpath) == ERROR_SUCCESS)
                            {
                                datatype = REG_SZ;
                                datasize = static_cast<DWORD>(pathSize);
                                if (RegQueryValueExA(hkpath, nullptr, nullptr, &datatype,
                                                     reinterpret_cast<LPBYTE>(dllPath), &datasize) == ERROR_SUCCESS)
                                {
                                    // Verify file exists
                                    if (GetFileAttributesA(dllPath) != INVALID_FILE_ATTRIBUTES)
                                    {
                                        found = true;
                                    }
                                }
                                RegCloseKey(hkpath);
                            }
                            RegCloseKey(hksub);
                        }
                        break;
                    }
                }
                RegCloseKey(hkEnum);
            }
            return found;
        }

    public:
        RealAsioDriverManager()
        {
            // Initialize COM
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            comInitialized_ = (hr == S_OK || hr == S_FALSE);

            enumerateDrivers();
        }

        ~RealAsioDriverManager()
        {
            closeDriver();

            if (comInitialized_)
            {
                CoUninitialize();
            }
        }

        // Enumerate all ASIO drivers from registry
        void enumerateDrivers()
        {
            drivers_.clear();

            HKEY hkEnum;
            if (RegOpenKeyA(HKEY_LOCAL_MACHINE, ASIO_PATH, &hkEnum) != ERROR_SUCCESS)
            {
                std::cout << "No ASIO drivers found in registry (HKLM\\SOFTWARE\\ASIO)\n";
                return;
            }

            char keyname[MAXDRVNAMELEN];
            DWORD index = 0;
            int driverId = 0;

            while (RegEnumKeyA(hkEnum, index++, keyname, sizeof(keyname)) == ERROR_SUCCESS)
            {
                HKEY hksub;
                if (RegOpenKeyExA(hkEnum, keyname, 0, KEY_READ, &hksub) == ERROR_SUCCESS)
                {
                    char clsidStr[64] = {0};
                    char description[MAXDRVNAMELEN] = {0};
                    DWORD datatype = REG_SZ;
                    DWORD datasize = sizeof(clsidStr);

                    if (RegQueryValueExA(hksub, COM_CLSID, nullptr, &datatype,
                                         reinterpret_cast<LPBYTE>(clsidStr), &datasize) == ERROR_SUCCESS)
                    {
                        AsioDriverInfo info = {};
                        info.id = driverId++;

                        // Convert CLSID string to CLSID struct
                        WCHAR wClsid[64];
                        MultiByteToWideChar(CP_ACP, 0, clsidStr, -1, wClsid, 64);
                        if (CLSIDFromString(wClsid, &info.clsid) == S_OK)
                        {
                            // Get driver description
                            datatype = REG_SZ;
                            datasize = sizeof(description);
                            if (RegQueryValueExA(hksub, ASIODRV_DESC, nullptr, &datatype,
                                                 reinterpret_cast<LPBYTE>(description), &datasize) == ERROR_SUCCESS)
                            {
                                strncpy_s(info.name, description, MAXDRVNAMELEN - 1);
                            }
                            else
                            {
                                strncpy_s(info.name, keyname, MAXDRVNAMELEN - 1);
                            }

                            // Find DLL path
                            if (findDriverPath(clsidStr, info.dllPath, MAXPATHLEN))
                            {
                                info.driver = nullptr;
                                drivers_.push_back(info);
                            }
                        }
                    }
                    RegCloseKey(hksub);
                }
            }

            RegCloseKey(hkEnum);

            std::cout << "Found " << drivers_.size() << " ASIO driver(s):\n";
            for (const auto &drv : drivers_)
            {
                std::cout << "  [" << drv.id << "] " << drv.name << "\n";
            }
        }

        int getNumDrivers() const { return static_cast<int>(drivers_.size()); }

        const char *getDriverName(int index) const
        {
            if (index >= 0 && index < static_cast<int>(drivers_.size()))
            {
                return drivers_[index].name;
            }
            return nullptr;
        }

        // Open and initialize a specific ASIO driver
        bool openDriver(int index, void *sysHandle = nullptr)
        {
            if (index < 0 || index >= static_cast<int>(drivers_.size()))
            {
                std::cerr << "Invalid ASIO driver index: " << index << "\n";
                return false;
            }

            // Close any existing driver
            closeDriver();

            auto &info = drivers_[index];

            // Create COM instance of the driver
            IASIO *asio = nullptr;
            HRESULT hr = CoCreateInstance(info.clsid, nullptr, CLSCTX_INPROC_SERVER,
                                          info.clsid, reinterpret_cast<void **>(&asio));

            if (FAILED(hr) || !asio)
            {
                std::cerr << "Failed to create ASIO driver instance: 0x" << std::hex << hr << std::dec << "\n";
                return false;
            }

            // Initialize the driver
            if (!asio->init(sysHandle ? sysHandle : GetDesktopWindow()))
            {
                char errorMsg[128];
                asio->getErrorMessage(errorMsg);
                std::cerr << "ASIO driver init failed: " << errorMsg << "\n";
                asio->Release();
                return false;
            }

            currentDriver_ = asio;
            currentDriverIndex_ = index;
            info.driver = asio;

            char driverName[64];
            asio->getDriverName(driverName);
            std::cout << "ASIO Driver opened: " << driverName << " (v" << asio->getDriverVersion() << ")\n";

            return true;
        }

        void closeDriver()
        {
            if (currentDriver_)
            {
                currentDriver_->stop();
                currentDriver_->disposeBuffers();
                currentDriver_->Release();
                currentDriver_ = nullptr;

                if (currentDriverIndex_ >= 0 && currentDriverIndex_ < static_cast<int>(drivers_.size()))
                {
                    drivers_[currentDriverIndex_].driver = nullptr;
                }
                currentDriverIndex_ = -1;
            }
        }

        IASIO *getDriver() { return currentDriver_; }
        int getCurrentDriverIndex() const { return currentDriverIndex_; }
    };

    // ------------------------------------------------------------------------
    // Global driver manager instance
    // ------------------------------------------------------------------------
    static RealAsioDriverManager *g_driverManager = nullptr;

    // Ensure driver manager exists
    static RealAsioDriverManager &getDriverManager()
    {
        if (!g_driverManager)
        {
            g_driverManager = new RealAsioDriverManager();
        }
        return *g_driverManager;
    }

    // ------------------------------------------------------------------------
    // Mock ASIO Driver (fallback when no real drivers available)
    // ------------------------------------------------------------------------
    class MockAsioDriver
    {
    private:
        bool initialized_ = false;
        ASIOCallbacks callbacks_;
        std::vector<ASIOBufferInfo> bufferInfos_;
        long bufferSize_ = 512;
        ASIOSampleRate sampleRate_ = 48000.0;
        std::thread audioThread_;
        std::atomic<bool> running_{false};

    public:
        ASIOError init(ASIODriverInfo *info)
        {
            if (info)
            {
                info->asioVersion = 2;
                info->driverVersion = 1;
                strcpy_s(info->name, "Mock ASIO Driver (No real drivers found)");
                strcpy_s(info->errorMessage, "No error");
                info->sysRef = nullptr;
            }
            initialized_ = true;
            return ASE_OK;
        }

        ASIOError exit()
        {
            stop();
            initialized_ = false;
            return ASE_OK;
        }

        ASIOError getChannels(long *numInputChannels, long *numOutputChannels)
        {
            if (!initialized_)
                return ASE_NotPresent;
            *numInputChannels = 2;
            *numOutputChannels = 2;
            return ASE_OK;
        }

        ASIOError getBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity)
        {
            if (!initialized_)
                return ASE_NotPresent;
            *minSize = 64;
            *maxSize = 8192;
            *preferredSize = 512;
            *granularity = 1;
            return ASE_OK;
        }

        ASIOError canSampleRate(ASIOSampleRate sampleRate)
        {
            if (!initialized_)
                return ASE_NotPresent;
            return (sampleRate >= 22050.0 && sampleRate <= 192000.0) ? ASE_OK : ASE_NoClock;
        }

        ASIOError getSampleRate(ASIOSampleRate *sampleRate)
        {
            if (!initialized_)
                return ASE_NotPresent;
            *sampleRate = sampleRate_;
            return ASE_OK;
        }

        ASIOError setSampleRate(ASIOSampleRate sampleRate)
        {
            if (!initialized_)
                return ASE_NotPresent;
            if (sampleRate >= 22050.0 && sampleRate <= 192000.0)
            {
                sampleRate_ = sampleRate;
                return ASE_OK;
            }
            return ASE_NoClock;
        }

        ASIOError getLatencies(long *inputLatency, long *outputLatency)
        {
            if (!initialized_)
                return ASE_NotPresent;
            *inputLatency = bufferSize_;
            *outputLatency = bufferSize_;
            return ASE_OK;
        }

        ASIOError getChannelInfo(ASIOChannelInfo *info)
        {
            if (!initialized_)
                return ASE_NotPresent;
            if (!info)
                return ASE_InvalidParameter;

            sprintf_s(info->name, "Channel %ld", info->channel);
            info->isActive = ASIOTrue;
            info->channelGroup = 0;
            info->type = ASIOSTFloat32LSB;
            return ASE_OK;
        }

        ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks)
        {
            if (!initialized_)
                return ASE_NotPresent;
            if (!bufferInfos || !callbacks)
                return ASE_InvalidParameter;

            bufferSize_ = bufferSize;
            callbacks_ = *callbacks;
            bufferInfos_.clear();

            for (long i = 0; i < numChannels; ++i)
            {
                ASIOBufferInfo info = bufferInfos[i];
                size_t bufferSizeBytes = bufferSize * sizeof(float);
                info.buffers[0] = _aligned_malloc(bufferSizeBytes, 16);
                info.buffers[1] = _aligned_malloc(bufferSizeBytes, 16);

                if (!info.buffers[0] || !info.buffers[1])
                    return ASE_NoMemory;

                memset(info.buffers[0], 0, bufferSizeBytes);
                memset(info.buffers[1], 0, bufferSizeBytes);

                bufferInfos_.push_back(info);
                bufferInfos[i] = info;
            }

            return ASE_OK;
        }

        ASIOError disposeBuffers()
        {
            if (!initialized_)
                return ASE_NotPresent;

            stop();

            for (auto &info : bufferInfos_)
            {
                if (info.buffers[0])
                    _aligned_free(info.buffers[0]);
                if (info.buffers[1])
                    _aligned_free(info.buffers[1]);
            }
            bufferInfos_.clear();

            return ASE_OK;
        }

        ASIOError start()
        {
            if (!initialized_)
                return ASE_NotPresent;
            if (running_.load())
                return ASE_OK;

            running_.store(true);
            audioThread_ = std::thread([this]()
                                       {
                long bufferIndex = 0;
                const auto bufferDuration = std::chrono::microseconds(
                    static_cast<long long>((bufferSize_ * 1000000.0) / sampleRate_));

                while (running_.load())
                {
                    auto start = std::chrono::steady_clock::now();

                    if (callbacks_.bufferSwitch)
                    {
                        callbacks_.bufferSwitch(bufferIndex, ASIOTrue);
                    }

                    bufferIndex = 1 - bufferIndex;

                    auto elapsed = std::chrono::steady_clock::now() - start;
                    if (elapsed < bufferDuration)
                    {
                        std::this_thread::sleep_for(bufferDuration - elapsed);
                    }
                } });

            return ASE_OK;
        }

        ASIOError stop()
        {
            if (!running_.load())
                return ASE_OK;

            running_.store(false);
            if (audioThread_.joinable())
            {
                audioThread_.join();
            }

            return ASE_OK;
        }
    };

    // Global instances
    static MockAsioDriver *g_mockDriver = nullptr;
    static bool g_usingRealDriver = false;

    // ------------------------------------------------------------------------
    // ASIO API Functions - Route to real driver or mock
    // ------------------------------------------------------------------------
    extern "C"
    {
        ASIOError ASIOInit(ASIODriverInfo *info)
        {
            auto &mgr = getDriverManager();

            // Se já há um driver aberto (via selectAsioDriverByName), usar ele
            if (mgr.getDriver() != nullptr)
            {
                g_usingRealDriver = true;
                if (info)
                {
                    IASIO *drv = mgr.getDriver();
                    drv->getDriverName(info->name);
                    info->driverVersion = drv->getDriverVersion();
                    info->asioVersion = 2;
                    strcpy_s(info->errorMessage, "OK - using pre-selected driver");
                }
                return ASE_OK;
            }

            // Tentar abrir primeiro driver disponível
            if (mgr.getNumDrivers() > 0)
            {
                for (int i = 0; i < mgr.getNumDrivers(); ++i)
                {
                    if (mgr.openDriver(i, info ? info->sysRef : nullptr))
                    {
                        g_usingRealDriver = true;

                        if (info)
                        {
                            IASIO *drv = mgr.getDriver();
                            drv->getDriverName(info->name);
                            info->driverVersion = drv->getDriverVersion();
                            info->asioVersion = 2;
                            strcpy_s(info->errorMessage, "OK");
                        }
                        return ASE_OK;
                    }
                }
            }

            // Fallback to mock driver
            std::cout << "No real ASIO drivers available, using mock driver\n";
            if (!g_mockDriver)
                g_mockDriver = new MockAsioDriver();

            g_usingRealDriver = false;
            return g_mockDriver->init(info);
        }

        ASIOError ASIOExit()
        {
            if (g_usingRealDriver)
            {
                getDriverManager().closeDriver();
                return ASE_OK;
            }

            if (g_mockDriver)
            {
                auto result = g_mockDriver->exit();
                delete g_mockDriver;
                g_mockDriver = nullptr;
                return result;
            }
            return ASE_NotPresent;
        }

        ASIOError ASIOStart()
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->start() : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->start() : ASE_NotPresent;
        }

        ASIOError ASIOStop()
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->stop() : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->stop() : ASE_NotPresent;
        }

        ASIOError ASIOGetChannels(long *numInputChannels, long *numOutputChannels)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->getChannels(numInputChannels, numOutputChannels) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->getChannels(numInputChannels, numOutputChannels) : ASE_NotPresent;
        }

        ASIOError ASIOGetBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->getBufferSize(minSize, maxSize, preferredSize, granularity) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->getBufferSize(minSize, maxSize, preferredSize, granularity) : ASE_NotPresent;
        }

        ASIOError ASIOCanSampleRate(ASIOSampleRate sampleRate)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->canSampleRate(sampleRate) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->canSampleRate(sampleRate) : ASE_NotPresent;
        }

        ASIOError ASIOGetSampleRate(ASIOSampleRate *sampleRate)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->getSampleRate(sampleRate) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->getSampleRate(sampleRate) : ASE_NotPresent;
        }

        ASIOError ASIOSetSampleRate(ASIOSampleRate sampleRate)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->setSampleRate(sampleRate) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->setSampleRate(sampleRate) : ASE_NotPresent;
        }

        ASIOError ASIOGetLatencies(long *inputLatency, long *outputLatency)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->getLatencies(inputLatency, outputLatency) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->getLatencies(inputLatency, outputLatency) : ASE_NotPresent;
        }

        ASIOError ASIOGetChannelInfo(ASIOChannelInfo *info)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->getChannelInfo(info) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->getChannelInfo(info) : ASE_NotPresent;
        }

        ASIOError ASIOCreateBuffers(ASIOBufferInfo *bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks)
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->createBuffers(bufferInfos, numChannels, bufferSize, callbacks) : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->createBuffers(bufferInfos, numChannels, bufferSize, callbacks) : ASE_NotPresent;
        }

        ASIOError ASIODisposeBuffers()
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->disposeBuffers() : ASE_NotPresent;
            }
            return g_mockDriver ? g_mockDriver->disposeBuffers() : ASE_NotPresent;
        }

        ASIOError ASIOControlPanel()
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->controlPanel() : ASE_NotPresent;
            }
            return ASE_NotPresent; // Mock doesn't have control panel
        }

        ASIOError ASIOOutputReady()
        {
            if (g_usingRealDriver)
            {
                IASIO *drv = getDriverManager().getDriver();
                return drv ? drv->outputReady() : ASE_NotPresent;
            }
            return ASE_OK; // Mock always ready
        }
    }

    // ------------------------------------------------------------------------
    // Utility Functions
    // ------------------------------------------------------------------------

    // List available ASIO drivers
    std::vector<std::string> listAsioDrivers()
    {
        std::vector<std::string> names;
        auto &mgr = getDriverManager();
        for (int i = 0; i < mgr.getNumDrivers(); ++i)
        {
            const char *name = mgr.getDriverName(i);
            if (name)
            {
                names.push_back(name);
            }
        }
        return names;
    }

    // Select a specific ASIO driver by index
    bool selectAsioDriver(int index)
    {
        auto &mgr = getDriverManager();
        return mgr.openDriver(index);
    }

    // Select ASIO driver by name (partial match)
    bool selectAsioDriverByName(const std::string &nameContains)
    {
        auto &mgr = getDriverManager();
        for (int i = 0; i < mgr.getNumDrivers(); ++i)
        {
            const char *name = mgr.getDriverName(i);
            if (name && std::string(name).find(nameContains) != std::string::npos)
            {
                return mgr.openDriver(i);
            }
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // ASIO Backend Implementation (AudioSink compatible)
    // ------------------------------------------------------------------------
    class AsioBackend : public IAudioBackend
    {
    private:
        std::atomic<bool> isRunning_{false};
        std::atomic<bool> isInitialized_{false};
        AudioCallback audioCallback_;

        std::vector<ASIOBufferInfo> asioBufferInfos_;
        ASIOCallbacks asioCallbacks_;

        long inputChannels_ = 0;
        long outputChannels_ = 0;
        long bufferSize_ = 512;
        ASIOSampleRate sampleRate_ = 48000;

        std::vector<float> tempBuffer_;

        static AsioBackend *instance_;

        static void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
        {
            if (instance_)
            {
                instance_->processBuffer(doubleBufferIndex, directProcess == ASIOTrue);
            }
        }

        static void sampleRateDidChange(ASIOSampleRate sRate)
        {
            if (instance_)
            {
                instance_->sampleRate_ = sRate;
                std::cout << "ASIO sample rate changed to: " << sRate << " Hz\n";
            }
        }

        static long asioMessage(long selector, long value, void * /*message*/, double * /*opt*/)
        {
            switch (selector)
            {
            case kAsioSelectorSupported:
                switch (value)
                {
                case kAsioResetRequest:
                case kAsioEngineVersion:
                case kAsioResyncRequest:
                case kAsioLatenciesChanged:
                case kAsioSupportsTimeInfo:
                case kAsioSupportsTimeCode:
                    return 1;
                default:
                    return 0;
                }
            case kAsioEngineVersion:
                return 2;
            case kAsioResetRequest:
            case kAsioResyncRequest:
            case kAsioLatenciesChanged:
                return 1;
            case kAsioSupportsTimeInfo:
            case kAsioSupportsTimeCode:
                return 1;
            default:
                return 0;
            }
        }

        static void *bufferSwitchTimeInfo(void * /*params*/, long doubleBufferIndex, ASIOBool directProcess)
        {
            bufferSwitch(doubleBufferIndex, directProcess);
            return nullptr;
        }

        void processBuffer(long doubleBufferIndex, bool /*directProcess*/)
        {
            if (!audioCallback_ || !isRunning_.load())
                return;

            size_t sampleCount = static_cast<size_t>(bufferSize_);
            const int channels = 2;
            tempBuffer_.resize(sampleCount * channels);
            std::fill(tempBuffer_.begin(), tempBuffer_.end(), 0.0f);

            audioCallback_(tempBuffer_.data(), bufferSize_, channels);

            if (asioBufferInfos_.size() >= 2)
            {
                void *leftBuf = asioBufferInfos_[0].buffers[doubleBufferIndex];
                void *rightBuf = asioBufferInfos_[1].buffers[doubleBufferIndex];

                if (leftBuf && rightBuf)
                {
                    // Get sample type to handle format conversion
                    ASIOChannelInfo channelInfo = {};
                    channelInfo.channel = 0;
                    channelInfo.isInput = ASIOFalse;
                    ASIOGetChannelInfo(&channelInfo);

                    // Convert based on sample type
                    switch (channelInfo.type)
                    {
                    case ASIOSTFloat32LSB:
                    {
                        float *left = static_cast<float *>(leftBuf);
                        float *right = static_cast<float *>(rightBuf);
                        for (long i = 0; i < bufferSize_; ++i)
                        {
                            left[i] = tempBuffer_[i * 2];
                            right[i] = tempBuffer_[i * 2 + 1];
                        }
                        break;
                    }
                    case ASIOSTInt32LSB:
                    {
                        int32_t *left = static_cast<int32_t *>(leftBuf);
                        int32_t *right = static_cast<int32_t *>(rightBuf);
                        for (long i = 0; i < bufferSize_; ++i)
                        {
                            // Convert float [-1, 1] to int32
                            left[i] = static_cast<int32_t>(tempBuffer_[i * 2] * 2147483647.0f);
                            right[i] = static_cast<int32_t>(tempBuffer_[i * 2 + 1] * 2147483647.0f);
                        }
                        break;
                    }
                    case ASIOSTInt24LSB:
                    {
                        uint8_t *left = static_cast<uint8_t *>(leftBuf);
                        uint8_t *right = static_cast<uint8_t *>(rightBuf);
                        for (long i = 0; i < bufferSize_; ++i)
                        {
                            int32_t lval = static_cast<int32_t>(tempBuffer_[i * 2] * 8388607.0f);
                            int32_t rval = static_cast<int32_t>(tempBuffer_[i * 2 + 1] * 8388607.0f);
                            left[i * 3 + 0] = lval & 0xFF;
                            left[i * 3 + 1] = (lval >> 8) & 0xFF;
                            left[i * 3 + 2] = (lval >> 16) & 0xFF;
                            right[i * 3 + 0] = rval & 0xFF;
                            right[i * 3 + 1] = (rval >> 8) & 0xFF;
                            right[i * 3 + 2] = (rval >> 16) & 0xFF;
                        }
                        break;
                    }
                    case ASIOSTInt16LSB:
                    {
                        int16_t *left = static_cast<int16_t *>(leftBuf);
                        int16_t *right = static_cast<int16_t *>(rightBuf);
                        for (long i = 0; i < bufferSize_; ++i)
                        {
                            left[i] = static_cast<int16_t>(tempBuffer_[i * 2] * 32767.0f);
                            right[i] = static_cast<int16_t>(tempBuffer_[i * 2 + 1] * 32767.0f);
                        }
                        break;
                    }
                    default:
                        // Unknown format, try float
                        {
                            float *left = static_cast<float *>(leftBuf);
                            float *right = static_cast<float *>(rightBuf);
                            for (long i = 0; i < bufferSize_; ++i)
                            {
                                left[i] = tempBuffer_[i * 2];
                                right[i] = tempBuffer_[i * 2 + 1];
                            }
                        }
                        break;
                    }
                }
            }

            // Signal output ready for drivers that support it
            ASIOOutputReady();
        }

    public:
        AsioBackend()
        {
            if (instance_ != nullptr)
            {
                throw std::runtime_error("Only one ASIO backend instance is supported");
            }
            instance_ = this;

            asioCallbacks_.bufferSwitch = &AsioBackend::bufferSwitch;
            asioCallbacks_.sampleRateDidChange = &AsioBackend::sampleRateDidChange;
            asioCallbacks_.asioMessage = &AsioBackend::asioMessage;
            asioCallbacks_.bufferSwitchTimeInfo = &AsioBackend::bufferSwitchTimeInfo;
        }

        ~AsioBackend() override
        {
            stop();

            if (isInitialized_.load())
            {
                ASIODisposeBuffers();
                ASIOExit();
            }

            instance_ = nullptr;
        }

        int init() override
        {
            if (isInitialized_.load())
                return 0;

            std::cout << "Initializing ASIO backend...\n";

            ASIODriverInfo driverInfo;
            std::memset(&driverInfo, 0, sizeof(ASIODriverInfo));
            driverInfo.asioVersion = 2;
            driverInfo.sysRef = GetDesktopWindow();

            ASIOError result = ASIOInit(&driverInfo);
            if (result != ASE_OK)
            {
                std::cerr << "Failed to initialize ASIO driver: " << driverInfo.errorMessage << "\n";
                return -1;
            }

            std::cout << "ASIO Driver: " << driverInfo.name << " (Version " << driverInfo.driverVersion << ")\n";

            result = ASIOGetChannels(&inputChannels_, &outputChannels_);
            if (result != ASE_OK)
            {
                std::cerr << "Failed to get ASIO channels\n";
                ASIOExit();
                return -1;
            }
            std::cout << "ASIO Channels: " << inputChannels_ << " input, " << outputChannels_ << " output\n";

            if (outputChannels_ < 2)
            {
                std::cerr << "Need at least 2 output channels\n";
                ASIOExit();
                return -1;
            }

            long minSize, maxSize, preferredSize, granularity;
            result = ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity);
            if (result != ASE_OK)
            {
                std::cerr << "Failed to get ASIO buffer sizes\n";
                ASIOExit();
                return -1;
            }

            // the driver's preferred size, or the one the person chose, within the driver's range and steps
            bufferSize_ = preferredSize;
            if (audioRequest().frames > 0)
            {
                long want = std::clamp(static_cast<long>(audioRequest().frames), minSize, maxSize);
                if (granularity > 0)
                    want = minSize + (want - minSize) / granularity * granularity;
                else if (granularity == -1) // powers of two
                {
                    long p = minSize;
                    while (p * 2 <= want)
                        p *= 2;
                    want = p;
                }
                bufferSize_ = want;
            }
            std::cout << "ASIO Buffer: " << bufferSize_ << " samples (range: " << minSize << "-" << maxSize << ")\n";

            result = ASIOSetSampleRate(sampleRate_);
            if (result != ASE_OK)
            {
                std::cerr << "Failed to set sample rate to " << sampleRate_ << "\n";
            }

            ASIOSampleRate currentRate;
            if (ASIOGetSampleRate(&currentRate) == ASE_OK)
            {
                std::cout << "ASIO Sample rate: " << currentRate << " Hz\n";
                sampleRate_ = currentRate;
            }

            // Setup stereo output buffers
            asioBufferInfos_.resize(2);
            asioBufferInfos_[0].isInput = ASIOFalse;
            asioBufferInfos_[0].channelNum = 0;
            asioBufferInfos_[0].buffers[0] = nullptr;
            asioBufferInfos_[0].buffers[1] = nullptr;

            asioBufferInfos_[1].isInput = ASIOFalse;
            asioBufferInfos_[1].channelNum = 1;
            asioBufferInfos_[1].buffers[0] = nullptr;
            asioBufferInfos_[1].buffers[1] = nullptr;

            result = ASIOCreateBuffers(asioBufferInfos_.data(), 2, bufferSize_, &asioCallbacks_);
            if (result != ASE_OK)
            {
                std::cerr << "Failed to create ASIO buffers\n";
                ASIOExit();
                return -1;
            }

            long inputLatency, outputLatency;
            if (ASIOGetLatencies(&inputLatency, &outputLatency) == ASE_OK)
            {
                double latencyMs = (static_cast<double>(outputLatency) / sampleRate_) * 1000.0;
                std::cout << "ASIO Output latency: " << latencyMs << " ms (" << outputLatency << " samples)\n";
            }

            for (int i = 0; i < 2; ++i)
            {
                ASIOChannelInfo channelInfo;
                channelInfo.channel = i;
                channelInfo.isInput = ASIOFalse;

                if (ASIOGetChannelInfo(&channelInfo) == ASE_OK)
                {
                    std::cout << "Output " << i << ": " << channelInfo.name << " (type: " << channelInfo.type << ")\n";
                }
            }

            isInitialized_.store(true);
            std::cout << "ASIO backend initialized successfully\n";
            return 0;
        }

        int start() override
        {
            if (!isInitialized_.load())
            {
                std::cerr << "ASIO backend not initialized\n";
                return -1;
            }

            if (isRunning_.load())
                return 0;

            std::cout << "Starting ASIO audio...\n";
            isRunning_.store(true);

            ASIOError result = ASIOStart();
            if (result != ASE_OK)
            {
                std::cerr << "Failed to start ASIO\n";
                isRunning_.store(false);
                return -1;
            }

            std::cout << "ASIO audio started\n";
            return 0;
        }

        void stop() override
        {
            if (!isRunning_.load())
                return;

            std::cout << "Stopping ASIO audio...\n";
            isRunning_.store(false);
            ASIOStop();
            std::cout << "ASIO audio stopped\n";
        }

        void setCallback(AudioCallback cb) override
        {
            audioCallback_ = cb;
        }

        float getSampleRate() const override
        {
            return static_cast<float>(sampleRate_);
        }
    };

    AsioBackend *AsioBackend::instance_ = nullptr;

    std::unique_ptr<IAudioBackend> createAsioBackend()
    {
        try
        {
            return std::make_unique<AsioBackend>();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to create ASIO backend: " << e.what() << "\n";
            return nullptr;
        }
    }

} // namespace synth

#endif // _WIN32
