#pragma once
// A panel's end of its plugin's link (plugins/clap/remote_editor.hpp is the other): the panel runs as a plugin's
// editor (--remote LINK) and only shows and turns; the plugin plays. LINK: on Linux/macOS the socket's fd ("3"), on
// Windows the two pipes it inherited ("<read handle>,<write handle>"). Lines of text both ways: send() from any
// thread (a lock); what comes in goes to onLine, on the link's own reader thread. When the plugin goes, alive() is
// false.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <poll.h>
#include <unistd.h>
#endif

namespace synth::panel
{
    class RemoteLink
    {
    public:
        std::function<void(const std::string &)> onLine; // set before connect(); the reader thread calls it

        ~RemoteLink()
        {
            if (!linked_)
                return;
            alive_ = false;
            if (reader_.joinable())
                reader_.join();
#if defined(_WIN32)
            CloseHandle(rd_);
            CloseHandle(wr_);
#elif defined(__unix__) || defined(__APPLE__)
            ::close(fd_);
#endif
        }

        bool connect(const char *link)
        {
#if defined(_WIN32)
            unsigned long long r = 0, w = 0;
            if (std::sscanf(link, "%llu,%llu", &r, &w) != 2)
                return false;
            rd_ = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(r));
            wr_ = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(w));
#else
            fd_ = std::atoi(link);
#endif
            linked_ = true;
            alive_ = true;
            reader_ = std::thread([this] { read(); });
            return true;
        }
        bool linked() const { return linked_; }
        bool alive() const { return alive_; }

        void send(const std::string &line)
        {
            std::lock_guard<std::mutex> g(writeLock_);
            size_t done = 0;
            while (alive_ && done < line.size())
            {
#if defined(_WIN32)
                DWORD w = 0;
                if (!WriteFile(wr_, line.data() + done, static_cast<DWORD>(line.size() - done), &w, nullptr) || w == 0)
                {
                    alive_ = false;
                    break;
                }
                done += w;
#elif defined(__unix__) || defined(__APPLE__)
                const ssize_t w = ::write(fd_, line.data() + done, line.size() - done);
                if (w <= 0)
                {
                    alive_ = false;
                    break;
                }
                done += static_cast<size_t>(w);
#else
                alive_ = false;
#endif
            }
        }

    private:
        bool linked_ = false;
        std::atomic<bool> alive_{false};
        std::thread reader_;
        std::mutex writeLock_;
#if defined(_WIN32)
        HANDLE rd_ = nullptr, wr_ = nullptr;
#else
        int fd_ = -1;
#endif

        // some bytes from the plugin, waiting a little at most; false: it is gone
        bool receive(std::string &buf)
        {
            char chunk[16384];
#if defined(_WIN32)
            DWORD avail = 0;
            if (!PeekNamedPipe(rd_, nullptr, 0, nullptr, &avail, nullptr))
                return false;
            if (avail == 0)
            {
                Sleep(15);
                return true;
            }
            DWORD got = 0;
            if (!ReadFile(rd_, chunk, avail < sizeof chunk ? avail : static_cast<DWORD>(sizeof chunk), &got, nullptr) || got == 0)
                return false;
            buf.append(chunk, got);
            return true;
#elif defined(__unix__) || defined(__APPLE__)
            pollfd pf{fd_, POLLIN, 0};
            if (::poll(&pf, 1, 200) <= 0)
                return true;
            const ssize_t r = ::read(fd_, chunk, sizeof chunk);
            if (r <= 0)
                return false;
            buf.append(chunk, static_cast<size_t>(r));
            return true;
#else
            (void)buf, (void)chunk;
            return false;
#endif
        }

        void read()
        {
            std::string buf;
            while (alive_)
            {
                if (!receive(buf))
                {
                    alive_ = false;
                    break;
                }
                for (size_t nl; (nl = buf.find('\n')) != std::string::npos; buf.erase(0, nl + 1))
                    if (onLine)
                        onLine(buf.substr(0, nl));
            }
        }
    };
} // namespace synth::panel
