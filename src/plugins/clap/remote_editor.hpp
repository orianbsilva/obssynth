#pragma once
// A plugin's editor as a program of its own: the product's panel (obssynthct1 --remote LINK ...), spawned with a
// two-way link: on Linux/macOS one end of a socket as its fd 3 (LINK "3"), on Windows two pipes it inherits (LINK
// "<read handle>,<write handle>"). The panel draws in a window of its own (Wayland: floating) or inside the host's
// (Windows: --parent HWND). The host stays free of SDL, of the panel's threads and of its crashes. Lines of text go
// both ways (the plugin and the panel agree on them); an io thread reads the panel's, and every ~30 ms asks the
// plugin what to send (its levels, the values the host changed). When the panel quits by itself, onExit says so.
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace synth::clap
{
    class RemoteEditor
    {
    public:
        static constexpr const char *LINK = "{link}"; // in open()'s args: replaced by the link's own description

        std::function<void(const std::string &)> onLine; // a line from the panel (the io thread)
        std::function<std::string()> tick;               // what to send now (the io thread)
        std::function<void()> onExit;                     // the panel quit by itself (the io thread)

        ~RemoteEditor() { close(); }

        bool running() const { return running_; }
        const std::string &lastError() const { return error_; } // why open() failed

        // the panel, if it is not running: program and its arguments (LINK where the link goes)
        bool open(const std::string &program, std::vector<std::string> args)
        {
            if (running_)
                return true;
            close(); // what a panel that quit by itself left
#if defined(_WIN32)
            SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE toPanelR = nullptr, toPanelW = nullptr, fromPanelR = nullptr, fromPanelW = nullptr;
            if (!CreatePipe(&toPanelR, &toPanelW, &sa, 0))
            {
                error_ = "CreatePipe failed, error " + std::to_string(GetLastError());
                return false;
            }
            if (!CreatePipe(&fromPanelR, &fromPanelW, &sa, 0))
            {
                error_ = "CreatePipe failed, error " + std::to_string(GetLastError());
                CloseHandle(toPanelR), CloseHandle(toPanelW);
                return false;
            }
            SetHandleInformation(toPanelW, HANDLE_FLAG_INHERIT, 0); // our ends stay ours
            SetHandleInformation(fromPanelR, HANDLE_FLAG_INHERIT, 0);
            const std::string link = std::to_string(reinterpret_cast<uintptr_t>(toPanelR)) + "," + std::to_string(reinterpret_cast<uintptr_t>(fromPanelW));
            std::wstring cmd = quote(widen(program));
            for (auto &a : args)
                cmd += L" " + quote(widen(a == LINK ? link : a));

            // only the panel's two ends are inherited, none of the host's other handles
            SIZE_T attrSize = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
            std::vector<char> attrBuf(attrSize);
            auto *attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
            HANDLE inherit[2] = {toPanelR, fromPanelW};
            const bool listed = InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize) &&
                                UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, nullptr, nullptr);
            STARTUPINFOEXW si{};
            si.StartupInfo.cb = sizeof si;
            si.lpAttributeList = listed ? attrs : nullptr;
            PROCESS_INFORMATION pi{};
            const std::wstring exe = widen(program);
            const BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                           CREATE_NO_WINDOW | (listed ? EXTENDED_STARTUPINFO_PRESENT : 0), nullptr, nullptr, &si.StartupInfo, &pi);
            if (listed)
                DeleteProcThreadAttributeList(attrs);
            CloseHandle(toPanelR), CloseHandle(fromPanelW); // the panel's ends: its own now
            if (!ok)
            {
                error_ = "CreateProcess failed, error " + std::to_string(GetLastError()) + " (" + program + ")";
                CloseHandle(toPanelW), CloseHandle(fromPanelR);
                return false;
            }
            CloseHandle(pi.hThread);
            proc_ = pi.hProcess;
            rd_ = fromPanelR, wr_ = toPanelW;
#elif defined(__unix__) || defined(__APPLE__)
            int sv[2];
            if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0)
                return false;
            posix_spawn_file_actions_t fa;
            posix_spawn_file_actions_init(&fa);
            posix_spawn_file_actions_adddup2(&fa, sv[1], 3); // the panel's end, as fd 3 (dup2 clears CLOEXEC)
            std::vector<std::string> all{program};
            for (auto &a : args)
                all.push_back(a == LINK ? std::string("3") : a);
            std::vector<char *> argv;
            for (auto &a : all)
                argv.push_back(a.data());
            argv.push_back(nullptr);
            pid_t pid = 0;
            const int r = posix_spawn(&pid, program.c_str(), &fa, nullptr, argv.data(), environ);
            posix_spawn_file_actions_destroy(&fa);
            ::close(sv[1]);
            if (r != 0)
            {
                error_ = "posix_spawn failed, error " + std::to_string(r) + " (" + program + ")";
                ::close(sv[0]);
                return false;
            }
            fd_ = sv[0];
            pid_ = pid;
#else
            (void)program, (void)args;
            return false;
#endif
            running_ = true;
            stop_ = false;
            io_ = std::thread([this] { loop(); });
            return true;
        }

        // the panel away (the host closed the editor, or the plugin goes)
        void close()
        {
            stop_ = true;
            if (io_.joinable())
                io_.join();
#if defined(_WIN32)
            if (wr_)
                CloseHandle(wr_); // the panel reads its end closed: it quits
            wr_ = nullptr;
            if (proc_)
            {
                if (WaitForSingleObject(proc_, 1000) != WAIT_OBJECT_0)
                    TerminateProcess(proc_, 0);
                CloseHandle(proc_);
                proc_ = nullptr;
            }
            if (rd_)
                CloseHandle(rd_);
            rd_ = nullptr;
#elif defined(__unix__) || defined(__APPLE__)
            if (pid_ > 0)
            {
                ::kill(pid_, SIGTERM);
                for (int i = 0; i < 50 && ::waitpid(pid_, nullptr, WNOHANG) == 0; ++i)
                    ::usleep(10000);
                if (::waitpid(pid_, nullptr, WNOHANG) == 0)
                {
                    ::kill(pid_, SIGKILL);
                    ::waitpid(pid_, nullptr, 0);
                }
                pid_ = 0;
            }
            if (fd_ >= 0)
                ::close(fd_);
            fd_ = -1;
#endif
            running_ = false;
        }

    private:
        std::atomic<bool> running_{false}, stop_{false};
        std::thread io_;
        std::string error_;
#if defined(_WIN32)
        HANDLE proc_ = nullptr, rd_ = nullptr, wr_ = nullptr;

        static std::wstring widen(const std::string &s)
        {
            if (s.empty())
                return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
            return w;
        }
        static std::wstring quote(const std::wstring &s) { return L"\"" + s + L"\""; } // paths with spaces (Program Files)

        // the bytes waiting from the panel, without blocking; false: the panel is gone
        bool readSome(std::string &buf)
        {
            DWORD avail = 0;
            if (!PeekNamedPipe(rd_, nullptr, 0, nullptr, &avail, nullptr))
                return false;
            while (avail > 0)
            {
                char chunk[4096];
                DWORD got = 0;
                if (!ReadFile(rd_, chunk, avail < sizeof chunk ? avail : static_cast<DWORD>(sizeof chunk), &got, nullptr) || got == 0)
                    return false;
                buf.append(chunk, got);
                avail -= got;
            }
            return true;
        }
        bool writeAll(const std::string &out)
        {
            size_t done = 0;
            while (done < out.size())
            {
                DWORD w = 0;
                if (!WriteFile(wr_, out.data() + done, static_cast<DWORD>(out.size() - done), &w, nullptr) || w == 0)
                    return false;
                done += w;
            }
            return true;
        }
        bool wait33() { return WaitForSingleObject(proc_, 33) == WAIT_TIMEOUT; } // false: the panel ended
#elif defined(__unix__) || defined(__APPLE__)
        int fd_ = -1;
        pid_t pid_ = 0;

        bool readSome(std::string &buf)
        {
            pollfd pf{fd_, POLLIN, 0};
            const int n = ::poll(&pf, 1, 0);
            if (n <= 0)
                return true;
            char chunk[4096];
            const ssize_t r = ::read(fd_, chunk, sizeof chunk);
            if (r <= 0)
                return false;
            buf.append(chunk, static_cast<size_t>(r));
            return true;
        }
        bool writeAll(const std::string &out)
        {
            size_t done = 0;
            while (done < out.size())
            {
                const ssize_t w = ::write(fd_, out.data() + done, out.size() - done);
                if (w <= 0)
                    return false;
                done += static_cast<size_t>(w);
            }
            return true;
        }
        bool wait33()
        {
            pollfd pf{fd_, POLLIN, 0};
            ::poll(&pf, 1, 33);
            return true;
        }
#else
        bool readSome(std::string &) { return false; }
        bool writeAll(const std::string &) { return false; }
        bool wait33() { return false; }
#endif

        void loop()
        {
            std::string buf;
            bool exited = false;
            while (!stop_)
            {
                if (!readSome(buf))
                {
                    exited = true;
                    break;
                }
                for (size_t nl; (nl = buf.find('\n')) != std::string::npos; buf.erase(0, nl + 1))
                    if (onLine)
                        onLine(buf.substr(0, nl));
                if (tick && !writeAll(tick()))
                {
                    exited = true;
                    break;
                }
                if (!wait33())
                {
                    exited = true;
                    break;
                }
            }
            if (exited && !stop_)
            {
#if defined(__unix__) || defined(__APPLE__)
                ::waitpid(pid_, nullptr, 0);
                pid_ = 0;
#endif
                running_ = false;
                if (onExit)
                    onExit();
            }
        }
    };
} // namespace synth::clap
