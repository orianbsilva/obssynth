#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// WGL extension function pointers for OpenGL 3.3 context creation
typedef HGLRC(WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
typedef BOOL(WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int *, const float *, UINT, int *, UINT *);

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x00000001

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

#include "theme.hpp"
#include "synth_ui.hpp"

// Forward declaration for ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ─── Globals ──────────────────────────────────────────────────────────────────
static HWND g_hwnd = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_hglrc = nullptr;

// ─── Create OpenGL 3.3 Core Context ──────────────────────────────────────────
static bool CreateGL33Context(HDC hdc)
{
    // Step 1: Set a dummy pixel format to get WGL extensions
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(hdc, &pfd);
    if (!fmt || !SetPixelFormat(hdc, fmt, &pfd))
        return false;

    // Step 2: Create legacy context to load WGL extensions
    HGLRC legacy = wglCreateContext(hdc);
    if (!legacy)
        return false;
    wglMakeCurrent(hdc, legacy);

    // Step 3: Load wglCreateContextAttribsARB
    auto wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!wglCreateContextAttribsARB)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacy);
        return false;
    }

    // Step 4: Create OpenGL 3.3 Core Profile context
    const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0};
    g_hglrc = wglCreateContextAttribsARB(hdc, nullptr, attribs);

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);

    if (!g_hglrc)
        return false;
    return wglMakeCurrent(hdc, g_hglrc) == TRUE;
}

// ─── Window Procedure ─────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── WinMain ──────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SynthSandbox";
    RegisterClassExW(&wc);

    // Create window
    g_hwnd = CreateWindowExW(
        0,
        L"SynthSandbox",
        L"Synth UI Sandbox — Dear ImGui",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1440, 900,
        nullptr, nullptr, hInstance, nullptr);

    g_hdc = GetDC(g_hwnd);

    if (!CreateGL33Context(g_hdc))
    {
        MessageBoxW(g_hwnd, L"Failed to create OpenGL 3.3 context.", L"Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    // ─── ImGui Setup ──────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplySynthTheme();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ─── Synth UI state ───────────────────────────────────────────────────────
    SynthUIState ui_state;
    ui_state.Init();

    // ─── Main Loop ────────────────────────────────────────────────────────────
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawSynthUI(ui_state);

        ImGui::Render();

        RECT rect;
        GetClientRect(g_hwnd, &rect);
        glViewport(0, 0, rect.right - rect.left, rect.bottom - rect.top);
        glClearColor(0.051f, 0.051f, 0.071f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(g_hdc);
    }

    // ─── Cleanup ──────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_hglrc);
    ReleaseDC(g_hwnd, g_hdc);
    DestroyWindow(g_hwnd);

    return 0;
}
