#pragma once

    #include <windows.h>
    #include <GL/gl.h>

namespace synth {
    // Platform-specific window handle type
    using WindowHandle = HWND;

    // OpenGL context management
    class GLContext {
    public:
        static bool initializeGL();
        static void shutdownGL();
        static WindowHandle createWindow(int width, int height, const char* title);
        static void destroyWindow(WindowHandle handle);
        static bool makeContextCurrent(WindowHandle handle);
        static void swapBuffers(WindowHandle handle);
        static bool shouldClose(WindowHandle handle);
        static void pollEvents();
    };
}
