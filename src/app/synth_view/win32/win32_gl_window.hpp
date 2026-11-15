#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace synth
{

    class Win32GLWindow
    {
    public:
        Win32GLWindow() = default;
        ~Win32GLWindow();
        bool create(const char *title, int width, int height);
        bool pumpMessages();
        void makeContextCurrent();
        void swapBuffers();
        bool isRunning() const { return running_; }
        int width() const { return width_; }
        int height() const { return height_; }

        void setKeyCallback(std::function<void(int)> cb) { key_callback_ = std::move(cb); }
        void setMouseCallback(std::function<void(int, int, int)> cb) { mouse_callback_ = std::move(cb); }
        void setMouseMoveCallback(std::function<void(int, int)> cb) { mouse_move_callback_ = std::move(cb); }
        void setMouseUpCallback(std::function<void(int, int, int)> cb) { mouse_up_callback_ = std::move(cb); }
        void setWheelCallback(std::function<void(int, int, int)> cb) { wheel_callback_ = std::move(cb); }

        // Expose HWND for ImGui init
        HWND hwnd() const { return hwnd_; }
        // Hook called at the start of WndProc — return true to consume the message
        void setWndProcHook(std::function<bool(HWND, UINT, WPARAM, LPARAM)> hook)
        {
            wndproc_hook_ = std::move(hook);
        }

    private:
        static LRESULT CALLBACK WndProcSetup(HWND, UINT, WPARAM, LPARAM);
        static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
        LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);

        HWND hwnd_{nullptr};
        HDC hdc_{nullptr};
        HGLRC hglrc_{nullptr};
        bool running_{true};
        int width_{0};
        int height_{0};
        std::function<void(int)> key_callback_;
        std::function<void(int, int, int)> mouse_callback_;    // button, x, y
        std::function<void(int, int)> mouse_move_callback_;    // x, y
        std::function<void(int, int, int)> mouse_up_callback_; // button, x, y
        std::function<void(int, int, int)> wheel_callback_;
        std::function<bool(HWND, UINT, WPARAM, LPARAM)> wndproc_hook_; // delta, x, y
    };

} // namespace synth
