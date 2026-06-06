#pragma once

#include <Windows.h>
#include <string>

#include "cameraunlock/logging/file_log.h"

namespace headtracking {

// Opens HeadTracking_debug.log next to the game EXE (truncated each launch).
// Called once from the bootstrap thread before the first HT_LOG so pre-config
// lines are captured; Plugin::Initialize then calls SetFileLogging to honour
// the user's preference.
inline void OpenLogFile() {
    wchar_t buf[MAX_PATH] = {};
    // GetModuleFileNameW does not guarantee null-termination on truncation, so
    // bound the path by the returned length instead of reading the raw buffer.
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring dir;
    if (len > 0 && len < MAX_PATH) {
        std::wstring exe(buf, len);
        const auto slash = exe.find_last_of(L"\\/");
        if (slash != std::wstring::npos) dir = exe.substr(0, slash + 1);
    }
    cameraunlock::logging::Open(dir + L"HeadTracking_debug.log");
}

inline void SetFileLogging(bool enabled) {
    if (!enabled) cameraunlock::logging::Close();
}

}  // namespace headtracking

#define HT_LOG(...) ::cameraunlock::logging::Line(__VA_ARGS__)
