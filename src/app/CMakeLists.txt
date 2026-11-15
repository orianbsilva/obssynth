# App CMakeLists.txt - Professional Audio Applications
cmake_minimum_required(VERSION 3.20)

# ==============================================================================
# COMMON UTILITIES - Shared utilities for all applications
# ==============================================================================

add_library(app_common
    common/platform_utils.cpp
    common/platform_utils.hpp
)

target_include_directories(app_common PUBLIC common ../lib)
target_link_libraries(app_common PUBLIC synth_core)

# ==============================================================================
# SYNTH_CLI - Professional MIDI Synthesizer Application
# ==============================================================================

add_executable(synth_cli
    synth_cli/synth_cli.cpp
)

target_include_directories(synth_cli PRIVATE synth_cli common ../lib)
target_link_libraries(synth_cli PRIVATE synth_core app_common)

# ==============================================================================
# SYNTH_CLI_V2 - SynthEngine-based CLI with Per-Voice Filtering
# ==============================================================================

add_executable(synth_cli_v2
    synth_cli_v2/synth_cli_v2.cpp
)

target_include_directories(synth_cli_v2 PRIVATE synth_cli_v2 synth_cli common ../lib)
target_link_libraries(synth_cli_v2 PRIVATE synth_core app_common)

# ==============================================================================
# DEAR IMGUI - OpenGL2 renderer + Win32 platform backend (synth_view, Windows)
# Fetched automatically so a clean clone configures without a manual bootstrap
# step (previously required `cmake -B sandbox/imgui_test/build sandbox/imgui_test`,
# which CI didn't run — see issue #13).
# ==============================================================================

# synth_view (ImGui on WGL) is the Windows panel. On Linux the panels are SDL3 on Wayland (obssynth4d, obsdrums):
# nothing here needs X11 or GL, and ImGui is not even fetched.
option(SYNTH_VIEW "Build synth_view (Windows: ImGui + OpenGL)" ON)
set(SYNTH_VIEW_ENABLED OFF)
if(SYNTH_VIEW AND WIN32)
    set(SYNTH_VIEW_ENABLED ON)
endif()

if(SYNTH_VIEW_ENABLED)
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_SRC_DIR "${imgui_SOURCE_DIR}")

if(WIN32)
    # ImGui static library (OpenGL2 + Win32 backends — compatible with legacy GL context)
    add_library(imgui_synth_view STATIC
        ${IMGUI_SRC_DIR}/imgui.cpp
        ${IMGUI_SRC_DIR}/imgui_draw.cpp
        ${IMGUI_SRC_DIR}/imgui_tables.cpp
        ${IMGUI_SRC_DIR}/imgui_widgets.cpp
        ${IMGUI_SRC_DIR}/imgui_demo.cpp
        ${IMGUI_SRC_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_SRC_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_include_directories(imgui_synth_view PUBLIC
        ${IMGUI_SRC_DIR}
        ${IMGUI_SRC_DIR}/backends
    )
    target_link_libraries(imgui_synth_view PUBLIC opengl32)
endif()

# ==============================================================================
# SYNTH_VIEW - OpenGL Waveform Viewer Application
# ==============================================================================

if(WIN32)
    add_executable(synth_view
        synth_view/main_screen.cpp
        synth_view/main_screen_imgui.cpp
        synth_view/win32/win32_gl_window.cpp
        synth_view/gl_renderer.cpp
        synth_view/imgui_theme.cpp
        synth_view/widgets/knob.cpp
        synth_view/widgets/fader.cpp
        synth_view/widgets/envelope.cpp
        synth_view/win32/synth_view.rc   # application icon resource
    )
    target_compile_definitions(synth_view PRIVATE SYNTH_GL_VIEWER_MAIN)
    target_include_directories(synth_view PRIVATE
        synth_view
        synth_view/widgets
        ${IMGUI_SRC_DIR}
        ${IMGUI_SRC_DIR}/backends
    )
    target_link_libraries(synth_view PRIVATE synth_core imgui_synth_view opengl32 gdi32 dwmapi imm32)
    # Hide the console window — GUI subsystem, keep main() as entry point
    set_target_properties(synth_view PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(synth_view PRIVATE /ENTRY:mainCRTStartup)
endif()
endif() # SYNTH_VIEW_ENABLED

# ==============================================================================
# PLATFORM-SPECIFIC LINKING
# ==============================================================================

if(UNIX AND NOT APPLE)
    # Linux - Link ALSA
    find_package(PkgConfig REQUIRED)

    # ALSA for audio
    pkg_check_modules(ALSA REQUIRED alsa)
    target_link_libraries(synth_cli PRIVATE ${ALSA_LIBRARIES})
    target_include_directories(synth_cli PRIVATE ${ALSA_INCLUDE_DIRS})

    if(TARGET synth_view)
        target_link_libraries(synth_view PRIVATE ${ALSA_LIBRARIES})
        target_include_directories(synth_view PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()

    # (X11 was linked here "for keyboard input": synth_cli reads the terminal, nothing of X11 is called)

    # Threading support
    find_package(Threads REQUIRED)
    target_link_libraries(synth_cli PRIVATE Threads::Threads)
    if(TARGET synth_view)
        target_link_libraries(synth_view PRIVATE Threads::Threads)
    endif()

elseif(WIN32)
    # Windows - Link WinMM and other Windows APIs
    target_link_libraries(synth_cli PRIVATE winmm.lib)
    if(TARGET synth_view)
        target_link_libraries(synth_view PRIVATE winmm.lib)
    endif()
endif()

# ==============================================================================
# OUTPUT CONFIGURATION
# ==============================================================================

# Set output directory for all applications
set_target_properties(synth_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(TARGET synth_view)
    set_target_properties(synth_view PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Copy synth_cli to root build directory for easy access
add_custom_command(TARGET synth_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:synth_cli>
    "${CMAKE_BINARY_DIR}/synth_cli${CMAKE_EXECUTABLE_SUFFIX}"
    COMMENT "Copying synth_cli to build root for easy access"
)

# ==============================================================================
# The products' panels: obssynth4d and obsdrums, drawn as hardware with no ImGui: SDL3 (window, input, 2D
# renderer), SDL3_image (SVG), SDL3_ttf (words). The system's when it has them (Debian: libsdl3-dev,
# libsdl3-image-dev, libsdl3-ttf-dev); else, with OBS_FETCH_SDL (Windows, CI), fetched and built static at the
# versions Carbonian ships. Audio and MIDI as v1 had them (common/platform_io.hpp): ASIO, DirectSound, WaveOut
# on Windows; PulseAudio, ALSA on Linux.
# ==============================================================================
option(OBS_PANELS "Build the products' panels (obssynth4d, obsdrums)" ON)
option(OBS_FETCH_SDL "Fetch SDL3, SDL3_image and SDL3_ttf when the system has none" ON)
if(OBS_PANELS)
    find_package(SDL3 CONFIG QUIET)
    find_package(SDL3_image CONFIG QUIET)
    find_package(SDL3_ttf CONFIG QUIET)
    if(NOT (SDL3_FOUND AND SDL3_image_FOUND AND SDL3_ttf_FOUND) AND OBS_FETCH_SDL)
        include(FetchContent)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        if(UNIX AND NOT APPLE) # Wayland only, no X11 (Carbonian); without Wayland's dev files, a build with no windows
            set(SDL_X11 OFF CACHE BOOL "" FORCE)
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(OBS_WAYLAND QUIET wayland-client wayland-egl wayland-cursor xkbcommon egl)
            if(NOT OBS_WAYLAND_FOUND)
                message(WARNING "panels: no Wayland dev files (libwayland-dev libxkbcommon-dev libegl-dev wayland-protocols): "
                                "the fetched SDL3 opens no windows")
                set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
            endif()
        endif()
        set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE) # PNG and JPG by stb: no libpng, no libjpeg
        foreach(fmt AVIF JXL TIF WEBP)
            set(SDLIMAGE_${fmt} OFF CACHE BOOL "" FORCE)
        endforeach()
        set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE) # FreeType from its submodule
        set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
        set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
        set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.10 GIT_SHALLOW TRUE)
        FetchContent_Declare(SDL3_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git GIT_TAG release-3.2.4 GIT_SHALLOW TRUE)
        FetchContent_Declare(SDL3_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG release-3.2.2 GIT_SHALLOW TRUE
                             GIT_SUBMODULES external/freetype)
        FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)
        message(STATUS "panels: SDL3 3.2.10, SDL3_image 3.2.4, SDL3_ttf 3.2.2 fetched, static")
    endif()
    if(TARGET SDL3::SDL3 AND TARGET SDL3_image::SDL3_image AND TARGET SDL3_ttf::SDL3_ttf)
        find_package(Threads REQUIRED)
        set(OBS_PANEL_LIBS SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf synth_core Threads::Threads)
        set(OBS_PANEL_INCLUDES ../lib)
        if(UNIX AND NOT APPLE)
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(OBS_ALSA REQUIRED alsa)
            list(APPEND OBS_PANEL_LIBS ${OBS_ALSA_LIBRARIES})
            list(APPEND OBS_PANEL_INCLUDES ${OBS_ALSA_INCLUDE_DIRS})
        endif()

        add_executable(obssynth4d obssynth4d/main.cpp obssynth4d/sound.cpp obssynth4d/presets.cpp common/panel_app.cpp common/host_window.cpp)
        add_executable(obsdrums obsdrums/main.cpp obsdrums/sound.cpp common/panel_app.cpp common/host_window.cpp) # the same parts (common/panel_kit.hpp)
        add_executable(obssynthct1 obssynthct1/main.cpp obssynthct1/sound.cpp common/panel_app.cpp common/host_window.cpp)
        add_executable(obssynthone obssynthone/main.cpp obssynthone/sound.cpp common/panel_app.cpp common/host_window.cpp)
        target_compile_definitions(obssynthone PRIVATE SYNTH_PCM_DIR="${CMAKE_SOURCE_DIR}/pcm")
        foreach(panel obssynth4d obsdrums obssynthct1 obssynthone)
            target_link_libraries(${panel} PRIVATE ${OBS_PANEL_LIBS})
            target_include_directories(${panel} PRIVATE ${OBS_PANEL_INCLUDES})
            if(WIN32)
                target_compile_definitions(${panel} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
            endif()
        endforeach()
        message(STATUS "panels: obssynth4d, obsdrums, obssynthct1, obssynthone")
    else()
        message(STATUS "panels: off (no SDL3, SDL3_image, SDL3_ttf; OBS_FETCH_SDL=ON fetches them)")
    endif()
endif()
