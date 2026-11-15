#pragma once

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#elif defined(__linux__)
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdio>
#endif

namespace examples
{

    /**
     * @brief Cross-platform utilities for example applications
     */
    class PlatformUtils
    {
    public:
        /**
         * @brief Check if a key has been pressed (non-blocking)
         * @return true if a key is available to read, false otherwise
         */
        static bool kbhit();

        /**
         * @brief Get a character from keyboard input
         * @return The character that was pressed
         */
        static int getch();

        /**
         * @brief Sleep for a specified number of milliseconds
         * @param ms Number of milliseconds to sleep
         */
        static void sleep(int ms);

        /**
         * @brief Initialize platform-specific resources (call at start of main)
         */
        static void initialize();

        /**
         * @brief Cleanup platform-specific resources (call at end of main)
         */
        static void cleanup();

        /**
         * @brief Print a standardized header for examples
         * @param title The title of the example
         * @param description Optional description
         */
        static void printHeader(const char *title, const char *description = nullptr);

        /**
         * @brief Print standardized controls help
         * @param controls Array of control descriptions (nullptr terminated)
         */
        static void printControls(const char *controls[]);

        /**
         * @brief Wait for user input before continuing
         * @param message Message to display (optional)
         */
        static void waitForInput(const char *message = "Press any key to continue...");

    private:
#ifdef __linux__
        static struct termios original_termios_;
        static bool termios_saved_;
#endif
    };

} // namespace examples
