#include "platform_utils.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

namespace examples {

#ifdef __linux__
struct termios PlatformUtils::original_termios_{};
bool PlatformUtils::termios_saved_ = false;
#endif

bool PlatformUtils::kbhit() {
#ifdef _WIN32
    return _kbhit() != 0;
#elif defined(__linux__)
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return true;
    }

    return false;
#else
    return false;
#endif
}

int PlatformUtils::getch() {
#ifdef _WIN32
    return _getch();
#elif defined(__linux__)
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
#else
    return getchar();
#endif
}

void PlatformUtils::sleep(int ms) {
#ifdef _WIN32
    Sleep(ms);
#elif defined(__linux__)
    usleep(ms * 1000);
#else
    // Fallback for other platforms
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

void PlatformUtils::initialize() {
#ifdef __linux__
    // Save original terminal settings
    if (tcgetattr(STDIN_FILENO, &original_termios_) == 0) {
        termios_saved_ = true;
    }
#endif
    // Windows doesn't need special initialization for our use case
}

void PlatformUtils::cleanup() {
#ifdef __linux__
    // Restore original terminal settings
    if (termios_saved_) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios_);
        termios_saved_ = false;
    }
#endif
    // Windows doesn't need special cleanup for our use case
}

void PlatformUtils::printHeader(const char* title, const char* description) {
    size_t title_len = strlen(title);
    size_t desc_len = description ? strlen(description) : 0;
    size_t max_len = std::max(title_len, desc_len);

    // Ensure minimum width of 40 characters
    size_t width = std::max(max_len + 4, size_t(40));

    std::cout << std::string(width, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(width, '=') << "\n";

    if (description && desc_len > 0) {
        std::cout << description << "\n";
        std::cout << std::string(width, '-') << "\n";
    }
    std::cout << "\n";
}

void PlatformUtils::printControls(const char* controls[]) {
    std::cout << "Controls:\n";
    std::cout << "---------\n";

    int i = 0;
    while (controls[i] != nullptr) {
        std::cout << "  " << controls[i] << "\n";
        i++;
    }
    std::cout << "\n";
}

void PlatformUtils::waitForInput(const char* message) {
    std::cout << message;
    getch();
    std::cout << "\n";
}

} // namespace examples
