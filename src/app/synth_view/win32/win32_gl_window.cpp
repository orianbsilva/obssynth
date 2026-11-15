#include "win32_gl_window.hpp"
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

// Application icon resource ID (defined in synth_view.rc)
#define IDI_APPICON 101

namespace synth
{

    Win32GLWindow::~Win32GLWindow()
    {
        if (hglrc_)
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc_);
            hglrc_ = nullptr;
        }
        if (hwnd_)
            DestroyWindow(hwnd_);
    }

    LRESULT CALLBACK Win32GLWindow::WndProcSetup(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        if (m == WM_NCCREATE)
        {
            auto cs = reinterpret_cast<CREATESTRUCT *>(l);
            auto self = reinterpret_cast<Win32GLWindow *>(cs->lpCreateParams);
            SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)self);
            SetWindowLongPtr(h, GWLP_WNDPROC, (LONG_PTR)&Win32GLWindow::WndProcThunk);
            return self->wndProc(h, m, w, l);
        }
        return DefWindowProc(h, m, w, l);
    }
    LRESULT CALLBACK Win32GLWindow::WndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        auto self = reinterpret_cast<Win32GLWindow *>(GetWindowLongPtr(h, GWLP_USERDATA));
        return self ? self->wndProc(h, m, w, l) : DefWindowProc(h, m, w, l);
    }
    LRESULT Win32GLWindow::wndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
    {
        // Let ImGui (or any registered hook) process the message first
        if (wndproc_hook_ && wndproc_hook_(h, msg, w, l))
            return 1;

        switch (msg)
        {
        case WM_CLOSE:
            running_ = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            width_ = LOWORD(l);
            height_ = HIWORD(l);
            return 0;
        case WM_KEYDOWN:
            if (key_callback_)
                key_callback_((int)w);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(h);
            if (mouse_callback_)
                mouse_callback_(0, (int)(short)LOWORD(l), height_ - (int)(short)HIWORD(l));
            return 0;
        case WM_RBUTTONDOWN:
            if (mouse_callback_)
                mouse_callback_(1, (int)(short)LOWORD(l), height_ - (int)(short)HIWORD(l));
            return 0;
        case WM_MOUSEMOVE:
            if (mouse_move_callback_)
                mouse_move_callback_((int)(short)LOWORD(l), height_ - (int)(short)HIWORD(l));
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            if (mouse_up_callback_)
                mouse_up_callback_(0, (int)(short)LOWORD(l), height_ - (int)(short)HIWORD(l));
            return 0;
        case WM_MOUSEWHEEL:
            if (wheel_callback_)
            {
                int delta = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
                POINT pt = {(int)(short)LOWORD(l), (int)(short)HIWORD(l)};
                ScreenToClient(h, &pt);
                wheel_callback_(delta, pt.x, height_ - pt.y);
            }
            return 0;
        default:
            return DefWindowProc(h, msg, w, l);
        }
    }

    bool Win32GLWindow::create(const char *title, int w, int h)
    {
        width_ = w;
        height_ = h;
        running_ = true;
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = &WndProcSetup;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"SynthGLWindowCls";
        // Load application icon from embedded resource (synth_view.rc / obssynth.ico)
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hIconSm = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_APPICON));
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        std::wstring wtitle(title, title + strlen(title));
        hwnd_ = CreateWindowW(L"SynthGLWindowCls", wtitle.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr, nullptr, wc.hInstance, this);
        if (!hwnd_)
            return false;
        hdc_ = GetDC(hwnd_);
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 16;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;
        int pf = ChoosePixelFormat(hdc_, &pfd);
        if (!pf)
            return false;
        if (!SetPixelFormat(hdc_, pf, &pfd))
            return false;
        hglrc_ = wglCreateContext(hdc_);
        if (!hglrc_)
            return false;
        if (!wglMakeCurrent(hdc_, hglrc_))
            return false;
        return true;
    }

    bool Win32GLWindow::pumpMessages()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return running_;
    }
    void Win32GLWindow::makeContextCurrent()
    {
        if (hdc_ && hglrc_)
            wglMakeCurrent(hdc_, hglrc_);
    }
    void Win32GLWindow::swapBuffers()
    {
        if (hdc_)
            SwapBuffers(hdc_);
    }

} // namespace synth
