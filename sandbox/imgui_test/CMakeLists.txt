cmake_minimum_required(VERSION 3.20)
project(imgui_sandbox CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Fetch Dear ImGui ─────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ─── Fetch imgui-knobs (header-only, ~300 lines) ──────────────────────────────
FetchContent_Declare(
    imgui_knobs
    GIT_REPOSITORY https://github.com/altschuler/imgui-knobs.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui_knobs)

# ─── ImGui library target ─────────────────────────────────────────────────────
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    # Win32 + OpenGL3 backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Use ImGui's built-in GL3 loader (imgui_impl_opengl3_loader.h) — no GLEW/GLAD needed
target_link_libraries(imgui PUBLIC opengl32)

# ─── imgui-knobs target ───────────────────────────────────────────────────────
add_library(imgui_knobs_lib STATIC
    ${imgui_knobs_SOURCE_DIR}/imgui-knobs.cpp
)
target_include_directories(imgui_knobs_lib PUBLIC ${imgui_knobs_SOURCE_DIR})
target_link_libraries(imgui_knobs_lib PUBLIC imgui)

# ─── Sandbox executable ───────────────────────────────────────────────────────
add_executable(imgui_sandbox WIN32
    main.cpp
    theme.cpp
    synth_ui.cpp
    widgets/knob.cpp
    widgets/fader.cpp
    widgets/envelope.cpp
    widgets/mod_matrix.cpp
)

target_include_directories(imgui_sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_knobs_SOURCE_DIR}
)

target_link_libraries(imgui_sandbox PRIVATE
    imgui
    imgui_knobs_lib
    opengl32
    gdi32
    dwmapi
    imm32
)

# MSVC: Unicode, no console window
if(MSVC)
    target_compile_options(imgui_sandbox PRIVATE /W3 /utf-8)
    target_compile_definitions(imgui_sandbox PRIVATE _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Output to bin/
set_target_properties(imgui_sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
