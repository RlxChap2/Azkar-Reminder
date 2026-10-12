#include "WindowsFullScreenDetector.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <cstring>

namespace dhikr {

std::unique_ptr<IFullScreenDetector> IFullScreenDetector::Create() {
    return std::make_unique<WindowsFullScreenDetector>();
}

FullScreenState WindowsFullScreenDetector::CheckFullScreen() {
    // 1. Query Windows user notification state
    QUERY_USER_NOTIFICATION_STATE qState;
    if (SUCCEEDED(SHQueryUserNotificationState(&qState))) {
        if (qState == QUNS_RUNNING_D3D_FULL_SCREEN ||
            qState == QUNS_PRESENTATION_MODE) {
            return FullScreenState::FullScreen;
        }
    }

    // 2. Inspect foreground window
    HWND fg = GetForegroundWindow();
    if (!fg || !IsWindow(fg) || !IsWindowVisible(fg) || IsIconic(fg)) {
        return FullScreenState::NotFullScreen;
    }

    // Don't suppress for our own application
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId()) {
        return FullScreenState::NotFullScreen;
    }

    // Desktop/shell windows
    HWND hDesktop = GetDesktopWindow();
    HWND hShell   = GetShellWindow();
    if (fg == hDesktop || fg == hShell) {
        return FullScreenState::NotFullScreen;
    }

    wchar_t cls[64] = { 0 };
    if (GetClassNameW(fg, cls, 64) > 0) {
        if (wcscmp(cls, L"Progman") == 0 ||
            wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) {
            return FullScreenState::NotFullScreen;
        }
    }

    // Check bounds against monitor
    HMONITOR hMon = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
    if (!hMon) return FullScreenState::NotFullScreen;

    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi)) return FullScreenState::NotFullScreen;

    RECT rcFg;
    if (!GetWindowRect(fg, &rcFg)) return FullScreenState::NotFullScreen;

    LONG w = rcFg.right  - rcFg.left;
    LONG h = rcFg.bottom - rcFg.top;
    LONG mw = mi.rcMonitor.right  - mi.rcMonitor.left;
    LONG mh = mi.rcMonitor.bottom - mi.rcMonitor.top;

    if (w >= mw && h >= mh) {
        LONG style = GetWindowLongW(fg, GWL_STYLE);
        // Exclude standard overlapping windows with titlebars/resizing frames
        if (!(style & WS_CAPTION)) {
            return FullScreenState::FullScreen;
        }
    }

    return FullScreenState::NotFullScreen;
}

bool WindowsFullScreenDetector::IsFullScreenActive() {
    return CheckFullScreen() == FullScreenState::FullScreen;
}

} // namespace dhikr
#endif
