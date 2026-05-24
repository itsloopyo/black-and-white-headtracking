// bw-headtracking-launcher.exe
//
// Spawns runblack.exe (Black & White, 2001) in suspended state, injects
// HeadTracking.dll via CreateRemoteThread(LoadLibraryW), then resumes. All
// CLI args are forwarded to runblack.exe.
//
// Both this launcher and HeadTracking.dll must be built x86 to match the
// 32-bit game. CMake refuses to configure as x64.

#include <Windows.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")

namespace {

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    return buf;
}

bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool Inject(HANDLE process, const std::wstring& dllPath) {
    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return false;
    if (!WriteProcessMemory(process, remote, dllPath.c_str(), bytes, nullptr)) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(k32, "LoadLibraryW"));
    if (!loadLib) { VirtualFreeEx(process, remote, 0, MEM_RELEASE); return false; }
    HANDLE thr = CreateRemoteThread(process, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!thr) { VirtualFreeEx(process, remote, 0, MEM_RELEASE); return false; }
    WaitForSingleObject(thr, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeThread(thr, &exitCode);
    CloseHandle(thr);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return exitCode != 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const std::wstring dir = ExeDir();
    const std::wstring exe = dir + L"\\runblack.exe";
    const std::wstring dll = dir + L"\\HeadTracking.dll";

    if (!FileExists(exe)) {
        fwprintf(stderr, L"runblack.exe not found next to launcher: %ls\n", exe.c_str());
        return 1;
    }
    if (!FileExists(dll)) {
        fwprintf(stderr, L"HeadTracking.dll not found next to launcher: %ls\n", dll.c_str());
        return 1;
    }

    std::wstring cmdline;
    cmdline.reserve(512);
    cmdline += L"\"";
    cmdline += exe;
    cmdline += L"\"";
    for (int i = 1; i < argc; ++i) {
        cmdline += L" \"";
        cmdline += argv[i];
        cmdline += L"\"";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, dir.c_str(), &si, &pi)) {
        fwprintf(stderr, L"CreateProcess failed (%lu)\n", GetLastError());
        return 1;
    }

    if (!Inject(pi.hProcess, dll)) {
        fwprintf(stderr, L"DLL injection failed (%lu)\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}
