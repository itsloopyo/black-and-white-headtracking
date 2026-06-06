#include <Windows.h>
#include <thread>
#include <cmath>
#include <atomic>

#include "plugin.h"
#include "camera_hook.h"
#include "engine_addresses.h"
#include "debug_log.h"
#include "version.h"

namespace {

struct FindWindowCtx {
    DWORD  pid;
    HWND   hwnd;
};

BOOL CALLBACK FindMainWindowProc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<FindWindowCtx*>(lp);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    RECT r;
    if (!GetWindowRect(hwnd, &r)) return TRUE;
    if ((r.right - r.left) < 200 || (r.bottom - r.top) < 200) return TRUE;
    ctx->hwnd = hwnd;
    return FALSE;
}

bool CenterOnce(HWND hwnd) {
    RECT wr;
    if (!GetWindowRect(hwnd, &wr)) return false;
    const int ww = wr.right - wr.left;
    const int wh = wr.bottom - wr.top;

    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return false;
    const int mw = mi.rcWork.right - mi.rcWork.left;
    const int mh = mi.rcWork.bottom - mi.rcWork.top;
    if (ww >= mw && wh >= mh) return false;  // already fills monitor

    const int nx = mi.rcWork.left + (mw - ww) / 2;
    const int ny = mi.rcWork.top  + (mh - wh) / 2;
    if (wr.left == nx && wr.top == ny) return false;

    SetWindowPos(hwnd, nullptr, nx, ny, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    HT_LOG("[main] centered game window at (%d,%d) size %dx%d", nx, ny, ww, wh);
    return true;
}

// Unaccelerated mouse motion, accumulated from WM_INPUT. Deriving cursor speed
// from the OS cursor-position delta runs it through pointer acceleration
// ("Enhance pointer precision") and the speed slider, so fast swings get
// amplified non-linearly - the cursor feels inconsistent and sometimes very
// fast. Raw counts are 1:1 and acceleration-free, and they don't need the
// SetCursorPos->GetCursorPos recenter round-trip (a second jitter source).
std::atomic<long> g_rawDx{0};
std::atomic<long> g_rawDy{0};

LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg != WM_INPUT) return DefWindowProcW(hwnd, msg, wparam, lparam);
    RAWINPUT ri;
    UINT size = sizeof(ri);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &ri, &size,
                        sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)
        || ri.header.dwType != RIM_TYPEMOUSE) {
        return 0;
    }
    const RAWMOUSE& m = ri.data.mouse;
    if (m.usFlags & MOUSE_MOVE_ABSOLUTE) {
        // RDP / tablet / some VMs report absolute coords; convert to a delta.
        static long lastX = 0, lastY = 0;
        static bool have = false;
        if (have) {
            g_rawDx.fetch_add(m.lLastX - lastX, std::memory_order_relaxed);
            g_rawDy.fetch_add(m.lLastY - lastY, std::memory_order_relaxed);
        }
        lastX = m.lLastX; lastY = m.lLastY; have = true;
    } else {
        g_rawDx.fetch_add(m.lLastX, std::memory_order_relaxed);
        g_rawDy.fetch_add(m.lLastY, std::memory_order_relaxed);
    }
    return 0;
}

// Message-only window registered as a background raw-input sink, so the cage
// gets mouse counts regardless of which window the OS thinks owns the cursor.
HWND CreateRawInputSink() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = RawInputWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"BWHeadTrackingRawInput";
    RegisterClassW(&wc);  // ERROR_CLASS_ALREADY_EXISTS is fine; CreateWindow still works
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return nullptr;
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;  // generic desktop
    rid.usUsage     = 0x02;  // mouse
    rid.dwFlags     = RIDEV_INPUTSINK;
    rid.hwndTarget  = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        DestroyWindow(hwnd);
        return nullptr;
    }
    return hwnd;
}

// B&W runs windowed under modern Windows and never calls ClipCursor itself,
// so a hard mouse swing flies straight out of the client rect onto adjacent
// windows. Cage the cursor to the client rect while the game has foreground;
// release on focus loss so Alt+Tab and overlapping windows still work.
void UpdateCursorCage(HWND hwnd, bool useRaw) {
    static bool s_caged = false;
    static bool s_virtualMode = false;
    static double s_virtualX = 0.0;
    static double s_virtualY = 0.0;
    const bool foreground = (GetForegroundWindow() == hwnd) && !IsIconic(hwnd);
    if (!foreground) {
        if (s_caged) { ClipCursor(nullptr); s_caged = false; }
        if (s_virtualMode) { headtracking::SetVirtualCursor(false, 0, 0); s_virtualMode = false; }
        // INPUTSINK keeps accumulating while we're backgrounded; drop it so we
        // don't apply a stored swing as one jump when the game regains focus.
        g_rawDx.store(0, std::memory_order_relaxed);
        g_rawDy.store(0, std::memory_order_relaxed);
        return;
    }
    RECT client;
    if (!GetClientRect(hwnd, &client)) return;
    POINT tl{ client.left, client.top };
    POINT br{ client.right, client.bottom };
    if (!ClientToScreen(hwnd, &tl) || !ClientToScreen(hwnd, &br)) return;
    RECT cage{ tl.x, tl.y, br.x, br.y };
    ClipCursor(&cage);
    s_caged = true;

    POINT centerClient{ (client.right - client.left) / 2, (client.bottom - client.top) / 2 };
    POINT centerScreen = centerClient;
    if (!ClientToScreen(hwnd, &centerScreen)) return;

    if (!s_virtualMode) {
        s_virtualX = static_cast<double>(*reinterpret_cast<int*>(headtracking::kCursorXAddr));
        s_virtualY = static_cast<double>(*reinterpret_cast<int*>(headtracking::kCursorYAddr));
        SetCursorPos(centerScreen.x, centerScreen.y);
        g_rawDx.store(0, std::memory_order_relaxed);
        g_rawDy.store(0, std::memory_order_relaxed);
        s_virtualMode = true;
    }

    int dx = 0, dy = 0;
    if (useRaw) {
        dx = static_cast<int>(g_rawDx.exchange(0, std::memory_order_relaxed));
        dy = static_cast<int>(g_rawDy.exchange(0, std::memory_order_relaxed));
        SetCursorPos(centerScreen.x, centerScreen.y);
    } else {
        POINT cursorScreen;
        if (!GetCursorPos(&cursorScreen)) return;
        POINT cursorClient = cursorScreen;
        if (!ScreenToClient(hwnd, &cursorClient)) return;
        dx = cursorClient.x - centerClient.x;
        dy = cursorClient.y - centerClient.y;
        if (dx != 0 || dy != 0) SetCursorPos(centerScreen.x, centerScreen.y);
    }
    if (dx != 0 || dy != 0) {
        s_virtualX += dx;
        s_virtualY += dy;
    }

    // No clamp at all - the game hand cursor can go anywhere, including
    // off-screen. Menu-screen visibility is a known regression the user
    // explicitly accepts.
    headtracking::SetVirtualCursor(true,
        static_cast<int>(std::lround(s_virtualX)),
        static_cast<int>(std::lround(s_virtualY)));
}

DWORD WINAPI CenterWindowThread(LPVOID) {
    // Match Skyrim's timing: give the game time to create + size its window
    // before we start searching. B&W (DirectDraw) creates a small bootstrap
    // window first, then resizes; if we fire too early we either reject by
    // size filter or center the wrong window.
    Sleep(2000);

    const DWORD pid = GetCurrentProcessId();
    HWND target = nullptr;
    for (int i = 0; i < 300 && !target; ++i) {  // up to ~30s
        FindWindowCtx ctx{pid, nullptr};
        EnumWindows(FindMainWindowProc, reinterpret_cast<LPARAM>(&ctx));
        target = ctx.hwnd;
        if (!target) Sleep(100);
    }
    if (!target) {
        HT_LOG("[main] center-window: timed out finding game window");
        return 0;
    }
    HT_LOG("[main] center-window: found hwnd=%p", target);

    // Center, then keep re-centering for a few seconds in case the game
    // moves/resizes its window during its own init (mode-set, SetCooperativeLevel,
    // etc). Stop once the window has held its centered position for one tick.
    bool last_moved = CenterOnce(target);
    for (int i = 0; i < 50; ++i) {
        Sleep(200);
        const bool moved = CenterOnce(target);
        if (!moved && !last_moved) break;
        last_moved = moved;
    }

    const HWND rawSink = CreateRawInputSink();
    const bool useRaw = (rawSink != nullptr);
    HT_LOG("[main] relative cursor cage active (raw input: %s)", useRaw ? "yes" : "no");
    while (IsWindow(target)) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            DispatchMessageW(&msg);
        }
        UpdateCursorCage(target, useRaw);
        Sleep(5);
    }
    if (rawSink) DestroyWindow(rawSink);
    ClipCursor(nullptr);
    return 0;
}

}  // namespace

static DWORD WINAPI BootstrapThread(LPVOID) {
    headtracking::OpenLogFile();
    HT_LOG("[main] HeadTracking %s loaded into pid %lu",
           HEADTRACKING_VERSION_STRING, GetCurrentProcessId());

    // Hooks must be armed before the game's main thread resumes, otherwise
    // we miss DirectDrawCreate. Launcher inject -> DllMain -> this thread
    // all run with the main thread suspended; we initialize synchronously.
    if (!headtracking::GetPlugin().Initialize()) {
        HT_LOG("[main] plugin initialization failed");
    }

    CreateThread(nullptr, 0, CenterWindowThread, nullptr, 0, nullptr);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            // reserved != nullptr means the process is terminating: every other
            // thread (receiver, hotkey poller, cursor cage) has already been
            // killed by the OS, possibly mid-syscall or holding the log mutex.
            // Joining those dead threads or disabling hooks now can hang the
            // game on exit. Only tear down on a genuine FreeLibrary unload,
            // which for an injected mod effectively never happens; otherwise
            // let the OS reclaim everything.
            if (reserved == nullptr) {
                headtracking::GetPlugin().Shutdown();
            }
            break;
    }
    return TRUE;
}
