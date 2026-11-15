
cmake_minimum_required(VERSION 3.20)

# Allow FetchContent dependencies that have old cmake_minimum_required (< 3.5)
# This is needed for doctest v2.4.x and similar legacy CMakeLists files
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "" FORCE)
endif()

# Set Clang as default compiler if not explicitly specified
if(NOT DEFINED ENV{CXX} AND NOT CMAKE_CXX_COMPILER)
    find_program(CLANG_FOUND clang++)
    if(CLANG_FOUND)
        set(CMAKE_CXX_COMPILER clang++)
        set(CMAKE_C_COMPILER clang)
        # Set clang-specific flags early for proper configuration
        set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++")
        message(STATUS "Using Clang as default compiler")
    else()
        message(STATUS "Clang not found, falling back to default compiler")
    endif()
elseif(DEFINED ENV{CXX})
    message(STATUS "Using explicitly set compiler: $ENV{CXX}")
endif()

project(Synth VERSION 1.0.0 LANGUAGES CXX)

# 32-bit ARM (a Raspberry Pi). Here, after project(): before it CMAKE_SYSTEM_PROCESSOR is not known yet, and
# a native build on a Pi 2 came out for the compiler's default (Raspbian's: armv6, no NEON). A build that gives
# its own -march keeps it (CI: armv6, armv7, armv7+NEON); else an armv7 machine gets armv7-a with NEON-VFPv4
# (every Pi from the 2 on has it).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7|armv7l|armv7-a)$")
    set(ARMV7 TRUE)
    if(NOT CMAKE_CXX_FLAGS MATCHES "-march=")
        message(STATUS "ARMv7: -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard (give a -march to choose another)")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
    else()
        message(STATUS "ARMv7: the build's own flags (${CMAKE_CXX_FLAGS})")
    endif()
endif()

# Enable CTest/Testing
include(CTest)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific flags for float-only precision
if(MSVC AND OBS_VST3)
    # clap-wrapper and the VST3 SDK are built on the static runtime: everything with them (a plugin then needs
    # no VC++ redistributable either)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(MSVC)
    add_compile_options(/fp:fast /W4 /FS)
    # Enable AVX2 SIMD instructions (implies SSE4.2, SSE4.1, AVX)
    add_compile_options(/arch:AVX2)
    # Disable specific warnings that might interfere with our code
    add_compile_options(/wd4100) # unreferenced formal parameter
    # Release-only: aggressive inlining + intrinsics + function-level linking + WPO/LTO
    add_compile_options($<$<CONFIG:Release>:/Ob3>)  # more aggressive inlining than /O2
    add_compile_options($<$<CONFIG:Release>:/Oi>)   # replace sinf/cosf/fabsf with CPU intrinsics
    add_compile_options($<$<CONFIG:Release>:/Gy>)   # function-level linking (dead code removal)
    add_compile_options($<$<CONFIG:Release>:/GL>)   # whole-program optimization
    add_link_options($<$<CONFIG:Release>:/LTCG>)    # link-time code generation (works with /GL)
else()
    add_compile_options(-Wdouble-promotion -Wfloat-conversion)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-ffloat-store)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang-specific flags - use libc++ instead of libstdc++
        add_compile_options(-stdlib=libc++)
        add_link_options(-stdlib=libc++)
    endif()
    add_compile_options(-Wall -Wextra -pedantic)
endif()


# Platform-specific settings
if(WIN32)
    # Windows platform settings
    set(PLATFORM_LIBS
        winmm
        ole32
        oleaut32
        uuid
    )

    set(PLATFORM_AUDIO_SOURCES
        src/lib/audio/waveout_backend.cpp
        src/lib/audio/wasapi_backend.cpp
        src/lib/audio/asio_backend.cpp
        src/lib/audio/directsound_backend.cpp
    )

    set(PLATFORM_GRAPHICS_SOURCES
        src/lib/graph/win32_gl_window.cpp
    )

    # Windows-specific definitions
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)

elseif(UNIX AND NOT APPLE)
    # Linux (including Raspberry Pi)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(PULSE libpulse-simple)
    # no X11 and no GL: the engine, synth_cli, the tests and the panels (SDL3 on Wayland) need neither

    set(PLATFORM_LIBS
        ${ALSA_LIBRARIES}
        pthread
    )
    if(PULSE_FOUND)
        list(APPEND PLATFORM_LIBS ${PULSE_LIBRARIES})
    endif()

    include_directories(
        ${ALSA_INCLUDE_DIRS}
    )
    if(PULSE_FOUND)
        include_directories(${PULSE_INCLUDE_DIRS})
    endif()

    set(PLATFORM_AUDIO_SOURCES
        src/lib/audio/alsa_backend.cpp
    )
    if(PULSE_FOUND)
        list(APPEND PLATFORM_AUDIO_SOURCES src/lib/audio/pulseaudio_backend.cpp)
        add_definitions(-DHAVE_PULSEAUDIO)
    endif()

    # Raspberry Pi ARMv7-specific tweaks
    if(ARMV7)
        message(STATUS "Applying Raspberry Pi ARMv7 (32-bit) build settings")
        # Optionally, you can add Pi-specific libraries or defines here
        add_definitions(-DRPI_ARMV7)
        # If using Broadcom OpenGL ES, add libraries here (uncomment if needed):
        # find_library(BCM_HOST bcm_host)
        # if(BCM_HOST)
        #     list(APPEND PLATFORM_LIBS ${BCM_HOST})
        # endif()
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
# Documentation for Raspberry Pi cross-compilation

# To cross-compile for Raspberry Pi ARMv7 (32-bit) from x86_64 Linux:
# 1. Install the cross toolchain: sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
# 2. Create a toolchain file (e.g. cmake/toolchain-rpi-armv7.cmake):
#
# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR armv7l)
# set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
# set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
# set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
# set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
# set(CMAKE_CXX_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
#
# 3. Configure with:
#    cmake -B build-rpi -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rpi-armv7.cmake -DCMAKE_BUILD_TYPE=Release
# 4. Build with:
#    cmake --build build-rpi --config Release
# 5. Copy build/bin/ to your Raspberry Pi and run!

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/lib)

# Source files for the core library
set(SYNTH_CORE_SOURCES
    src/lib/oscillators/oscillator.cpp
    src/lib/oscillators/polyphonic_oscillator.cpp
    src/lib/oscillators/pcm_waveform.cpp
    src/lib/oscillators/adsr_envelope.cpp
    src/lib/oscillators/lfo.cpp
    src/lib/audio/audio_sink.cpp
    src/lib/audio/audio_backend.hpp
    ${PLATFORM_AUDIO_SOURCES}
    src/lib/utils/wav_writer.cpp
    src/lib/utils/wav_reader.cpp
    src/lib/utils/platform_midi_common.cpp
    src/lib/utils/platform_midi_win.cpp
    src/lib/utils/platform_midi_linux.cpp
    src/lib/utils/logger.cpp
    src/lib/utils/log_manager.cpp
    src/lib/effects/reverb.cpp
    src/lib/effects/tube_reverb.cpp
    src/lib/effects/delay.cpp
    src/lib/effects/waveshaper.cpp
    src/lib/effects/filter.cpp
    src/lib/effects/mixer.cpp
    src/lib/effects/gain.cpp
    src/lib/effects/chorus.cpp
    src/lib/effects/overdrive.cpp
    src/lib/patch/audio_patch.cpp
    src/lib/patch/patch_dsl.cpp
    src/lib/patch/patch_nodes.cpp
    src/lib/devices/midi_device.cpp
    src/lib/devices/launchpadx.cpp
    src/lib/devices/launchkey_mk4.cpp
    src/lib/devices/apc_key25_mk2.cpp
    src/lib/devices/apc_key25_mk1.cpp
)

set(SYNTH_CORE_HEADERS
    src/lib/math/synt_math.hpp
    src/lib/oscillators/oscillator.hpp
    src/lib/oscillators/lfo.hpp
    src/lib/oscillators/polyphonic_oscillator.hpp
    src/lib/oscillators/adsr_envelope.hpp
    src/lib/audio/audio_sink.hpp
    src/lib/audio/platform_audio.hpp
    src/lib/utils/wav_writer.hpp
    src/lib/utils/platform_midi_reader.hpp
    src/lib/utils/logger.hpp
    src/lib/utils/log_manager.hpp
    src/lib/effects/reverb.hpp
    src/lib/effects/tube_reverb.hpp
    src/lib/effects/filter.hpp
    src/lib/effects/mixer.hpp
    src/lib/effects/gain.hpp
    src/lib/effects/chorus.hpp
    src/lib/effects/overdrive.hpp
    src/lib/patch/audio_patch.hpp
    src/lib/patch/patch_dsl.hpp
    src/lib/patch/patch_nodes.hpp
    src/lib/patch/patch_system.hpp
    src/lib/devices/midi_device.hpp
    src/lib/devices/launchpadx.hpp
    src/lib/devices/launchkey_mk4.hpp
    src/lib/devices/apc_key25_mk2.hpp
    src/lib/devices/apc_key25_mk1.hpp
)

# Create the core synthesizer library
add_library(synth_core STATIC ${SYNTH_CORE_SOURCES} ${SYNTH_CORE_HEADERS})
target_link_libraries(synth_core ${PLATFORM_LIBS})

# Set properties for the library
set_target_properties(synth_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON  # it goes into the CLAP plugins (shared objects) too
)

# Add source subdirectories
add_subdirectory(src)

# Unit tests (doctest framework)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Performance benchmarks (cross-platform)
if(BUILD_TESTING)
    add_subdirectory(benchmarks)
endif()

# Ensure synth_cli target is available for packaging
if(NOT TARGET synth_cli)
    message(WARNING "synth_cli target not found - package installation may be incomplete")
endif()

# Optional: Install targets
install(TARGETS synth_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${SYNTH_CORE_HEADERS}
    DESTINATION include/synth
)

# Optional: Create a package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SynthConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SynthConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SynthConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Synth
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SynthConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SynthConfigVersion.cmake"
    DESTINATION lib/cmake/Synth
)

# ==============================================================================
# PACKAGE INSTALL CONFIGURATION
# ==============================================================================

# Install synth_cli executable
if(TARGET synth_cli)
    install(TARGETS synth_cli
        RUNTIME DESTINATION bin
        COMPONENT Applications
    )
endif()

# Install PCM folder and all its contents
install(DIRECTORY pcm/
    DESTINATION pcm
    COMPONENT Data
    FILES_MATCHING PATTERN "*.wav"
)

# Install package documentation
install(FILES PACKAGE_README.md
    DESTINATION .
    COMPONENT Documentation
    RENAME README.md
)

# Create a complete package that includes both executable and data
set(CPACK_PACKAGE_NAME "SynthCLI")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Synth CLI Audio Synthesizer")
set(CPACK_PACKAGE_VENDOR "Synth Project")
set(CPACK_PACKAGE_CONTACT "synth@example.com")

# Component descriptions
set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Synth CLI Application")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "The main synthesizer command-line application")
set(CPACK_COMPONENT_DATA_DISPLAY_NAME "PCM Waveform Data")
set(CPACK_COMPONENT_DATA_DESCRIPTION "Sample waveform data files for the synthesizer")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "Package documentation and usage instructions")

# Set components to install by default
set(CPACK_COMPONENTS_ALL Applications Data Documentation)

# Platform-specific package generators
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Synth CLI Synthesizer")
    set(CPACK_NSIS_HELP_LINK "https://github.com/orianbsilva/obssynth")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/orianbsilva/obssynth")
    set(CPACK_NSIS_CONTACT "synth@example.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Synth Project")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# Include CPack
include(CPack)
