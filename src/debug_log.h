#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>

namespace headtracking {

// Pre-config bootstrap logs (DllMain, pre-Initialize) write through; once
// Config::log_to_file is loaded, Plugin::Initialize calls SetFileLogging
// to honour the user's preference.
inline std::atomic<bool>& FileLoggingFlag() {
    static std::atomic<bool> enabled{true};
    return enabled;
}

inline void SetFileLogging(bool enabled) {
    FileLoggingFlag().store(enabled, std::memory_order_release);
}

inline const char* LogPath() {
    static std::string path = []() {
        char buf[1024] = {};
        // GetModuleFileNameA does not guarantee null-termination on truncation
        // (it returns the buffer size and sets ERROR_INSUFFICIENT_BUFFER), so
        // construct the string from the returned length, not the raw buffer.
        DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
        if (len == 0 || len >= sizeof(buf)) return std::string("HeadTracking_debug.log");
        std::string p(buf, len);
        auto slash = p.find_last_of("\\/");
        if (slash != std::string::npos) p = p.substr(0, slash + 1);
        else p.clear();
        return p + "HeadTracking_debug.log";
    }();
    return path.c_str();
}

inline void Log(const char* fmt, ...) {
    if (!FileLoggingFlag().load(std::memory_order_acquire)) return;
    static std::mutex m;
    std::lock_guard<std::mutex> g(m);
    FILE* f = std::fopen(LogPath(), "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

}  // namespace headtracking

#define HT_LOG(...) ::headtracking::Log(__VA_ARGS__)
