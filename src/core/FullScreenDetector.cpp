#include "FullScreenDetector.h"
#include <cstring>

namespace dhikr {

FullScreenDetector& FullScreenDetector::Instance() {
    static FullScreenDetector instance;
    return instance;
}

bool FullScreenDetector::IsFullScreenActive() const {
    // 1. Check Windows user notification state (Direct3D full-screen, presentation mode, etc.)
    QUERY_USER_NOTIFICATION_STATE qState;
    if (SUCCEEDED(SHQueryUserNotificationState(&qState))) {
        if (qState == QUNS_RUNNING_D3D_FULL_SCREEN ||
            qState == QUNS_PRESENTATION_MODE ||
            qState == QUNS_BUSY) {
            return true;
        }
    }

    // 2. Inspect the current foreground window
    HWND fg = GetForegroundWindow();
    if (!fg || !IsWindow(fg) || !IsWindowVisible(fg) || IsIconic(fg)) {
        return false;
    }

    // Never suppress for our own application windows
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId()) {
        return false;
    }

    // Exclude Windows shell / desktop windows
    HWND hDesktop = GetDesktopWindow();
    HWND hShell   = GetShellWindow();
    if (fg == hDesktop || fg == hShell) {
        return false;
    }

    wchar_t cls[64] = { 0 };
    if (GetClassNameW(fg, cls, 64) > 0) {
        if (wcscmp(cls, L"Progman") == 0 ||
            wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) {
            return false;
        }
    }

    // Check if the foreground window bounds completely cover its monitor
    HMONITOR hMon = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
    if (!hMon) return false;

    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi)) return false;

    RECT rcFg;
    if (!GetWindowRect(fg, &rcFg)) return false;

    bool coversMonitor = (rcFg.left <= mi.rcMonitor.left &&
                          rcFg.top  <= mi.rcMonitor.top &&
                          rcFg.right >= mi.rcMonitor.right &&
                          rcFg.bottom >= mi.rcMonitor.bottom);

    if (!coversMonitor) {
        return false;
    }

    // Distinguish true fullscreen / borderless applications from normal maximized windows
    // (e.g. on systems with auto-hidden taskbars)
    LONG style = GetWindowLongW(fg, GWL_STYLE);

    if ((style & WS_CAPTION) == WS_CAPTION) {
        POINT ptTopLeft = { 0, 0 };
        ClientToScreen(fg, &ptTopLeft);

        // If client area starts below the top margin, it has a standard title bar (normal maximized app)
        if (ptTopLeft.y > mi.rcMonitor.top + 10) {
            return false;
        }
    }

    // True fullscreen window (borderless game, full-screen video, presentation, etc.)
    return true;
}

} // namespace dhikr
