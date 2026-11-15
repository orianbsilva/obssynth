/**
 * @file platform_audio.hpp
 * @brief Cross-platform audio backend definitions and includes
 *
 * Provides platform-specific audio API includes and backend type definitions
 * for Windows (WinMM, WASAPI, DirectSound, ASIO) and Linux (ALSA, PulseAudio).
 *
 * Handles conditional compilation and linking of platform audio libraries:
 * - Windows: WinMM, WASAPI, DirectSound with COM support
 * - Linux: ALSA with optional PulseAudio support
 * - Automatic library linking on Windows platforms
 *
 * @warning This file must be included before any platform-specific audio code
 * @see AudioSink for cross-platform audio output implementation
 */

#pragma once

// Platform-specific audio includes and definitions
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <comdef.h>
#include <dsound.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#elif defined(__linux__)
#include <alsa/asoundlib.h>
#ifdef HAVE_PULSEAUDIO
#include <pulse/simple.h>
#include <pulse/error.h>
#endif
#else
#error "Unsupported platform"
#endif

namespace synth
{
        /**
         * @brief Supported audio backend types per platform
         *
         * Enumeration of available audio backends with platform-specific values.
         * Used to select and configure the appropriate audio driver for optimal
         * performance and compatibility.
         *
         * Windows backends:
         * - WaveOut: Windows Multimedia API (high compatibility, higher latency)
         * - WASAPI: Windows Audio Session API (low latency, modern)
         * - ASIO: Audio Stream Input/Output (professional, lowest latency)
         * - DirectSound: Legacy DirectX audio (deprecated)
         *
         * Linux backends:
         * - ALSA: Advanced Linux Sound Architecture (standard)
         * - PulseAudio: High-level audio server (when available)
         *
         * @code
         * AudioBackendType backend = AudioBackendType::WASAPI;  // Windows
         * AudioBackendType backend = AudioBackendType::ALSA;    // Linux
         * @endcode
         */
        enum class AudioBackendType
        {
#ifdef _WIN32
                WaveOut,    ///< Windows Multimedia WaveOut API
                WASAPI,     ///< Windows Audio Session API
                ASIO,       ///< Audio Stream Input/Output
                DirectSound ///< DirectSound (legacy)
#elif defined(__linux__)
                ALSA ///< Advanced Linux Sound Architecture
#ifdef HAVE_PULSEAUDIO
                ,
                PulseAudio ///< PulseAudio server (optional)
#endif
#endif
        };
}
