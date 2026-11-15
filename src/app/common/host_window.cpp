#include "host_window.hpp"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace synth::hostwin
{
    namespace
    {
        std::string error_;
    }
    std::string lastError() { return error_; }

#if defined(_WIN32)
    void prepare() { SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); } // as hosts are (SDL3 has no hint for it)

    bool embedInHost(SDL_Window *win, uint64_t parent)
    {
        HWND host = reinterpret_cast<HWND>(static_cast<uintptr_t>(parent));
        HWND child = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(win), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!child)
        {
            error_ = "no HWND from SDL";
            return false;
        }
        if (!host || !IsWindow(host))
        {
            error_ = "the host's HWND " + std::to_string(parent) + " is not a window";
            return false;
        }
        LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_CHILD | WS_CLIPSIBLINGS;
        SetWindowLongPtrW(child, GWL_STYLE, style);
        LONG_PTR ex = GetWindowLongPtrW(child, GWL_EXSTYLE);
        ex &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);
        SetWindowLongPtrW(child, GWL_EXSTYLE, ex);
        if (!SetParent(child, host))
        {
            error_ = "SetParent failed, error " + std::to_string(GetLastError());
            return false;
        }
        RECT rc{};
        GetClientRect(host, &rc);
        SetWindowPos(child, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        SDL_SetWindowSize(win, rc.right - rc.left, rc.bottom - rc.top); // SDL's own idea of it, the same
        return true;
    }

    bool hostAlive(uint64_t parent) { return IsWindow(reinterpret_cast<HWND>(static_cast<uintptr_t>(parent))) != FALSE; }

    void followHost(SDL_Window *win, uint64_t parent)
    {
        HWND host = reinterpret_cast<HWND>(static_cast<uintptr_t>(parent));
        HWND child = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(win), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!host || !child)
            return;
        RECT hr{}, cr{};
        GetClientRect(host, &hr);
        GetClientRect(child, &cr);
        const int w = hr.right - hr.left, h = hr.bottom - hr.top;
        if (w > 0 && h > 0 && (w != cr.right - cr.left || h != cr.bottom - cr.top))
        {
            SetWindowPos(child, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SDL_SetWindowSize(win, w, h);
        }
    }
#else
    void followHost(SDL_Window *, uint64_t) {}
    void prepare() {}
    bool embedInHost(SDL_Window *, uint64_t)
    {
        error_ = "no host windows to go into here (Windows only)";
        return false;
    }
    bool hostAlive(uint64_t) { return true; }
#endif
} // namespace synth::hostwin
