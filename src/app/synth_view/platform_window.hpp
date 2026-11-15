#pragma once

// synth_view is the Windows panel (Win32 + WGL + ImGui). On Linux the panel is obssynth4d: SDL3 on Wayland, no X11.
#include "win32/win32_gl_window.hpp"
namespace synth
{
    using PlatformWindow = Win32GLWindow;
}
