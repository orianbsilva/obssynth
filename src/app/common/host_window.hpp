#pragma once
// A panel inside a plugin host's window: on Windows the host gives the plugin an HWND to draw in, and the panel (a
// program of its own) makes its SDL window a child of it, filling it. Elsewhere there is nothing to go into (Wayland
// has no foreign windows): embedInHost says false and the panel stays a window of its own.
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>

namespace synth::hostwin
{
    // before SDL_Init: the panel's pixels as the host's (per-monitor DPI), so the child is not stretched
    void prepare();
    // win: the panel's window (created hidden and borderless); parent: the host's (an HWND as a number)
    bool embedInHost(SDL_Window *win, uint64_t parent);
    // each frame: the panel's window the size of the host's (it may be 0 x 0 at first, and hosts resize it)
    void followHost(SDL_Window *win, uint64_t parent);
    // why embedInHost said false
    std::string lastError();
    // the host's window still there (when it is gone, the panel ends)
    bool hostAlive(uint64_t parent);
} // namespace synth::hostwin
