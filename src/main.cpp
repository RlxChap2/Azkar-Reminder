#include "core/Application.h"
#include "resource.h"
#include <windows.h>
#include <shobjidl.h>
#include <string>

// ── Entry point ───────────────────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      LPWSTR    lpCmdLine,
                      int       /*nCmdShow*/) {
    // ── Single instance enforcement ────────────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AzkarReminderSingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"DhikrMain", nullptr);
        
        // Handle --quit flag (from uninstaller)
        if (lpCmdLine && wcsstr(lpCmdLine, L"--quit") != nullptr) {
            if (existing) {
                // ID_TRAY_EXIT is 2008
                PostMessageW(existing, WM_COMMAND, 2008, 0);
            }
            CloseHandle(hMutex);
            return 0;
        }

        // Normal launch: Bring existing window to foreground
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // If --quit was passed but no instance was running, just exit
    if (lpCmdLine && wcsstr(lpCmdLine, L"--quit") != nullptr) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ── COM initialization (needed for tray, shell, dialogs) ──────────────
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // ── Set explicit AppUserModelID for Taskbar icon & shortcut binding ────
    SetCurrentProcessExplicitAppUserModelID(L"AhmedMohamedKamel.AzkarReminder");

    // ── DPI awareness ─────────────────────────────────────────────────────
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // ── Check for --background flag ────────────────────────────────────────
    bool startBackground = (lpCmdLine && wcsstr(lpCmdLine, L"--background") != nullptr);

    // ── Initialize application ─────────────────────────────────────────────
    auto& app = dhikr::Application::Instance();
    if (!app.Initialize(hInstance)) {
        MessageBoxW(nullptr,
            L"Azkar Reminder failed to start.\n\nPlease check the log file in %LOCALAPPDATA%\\Azkar Reminder\\logs.",
            L"Azkar Reminder — Startup Error",
            MB_ICONERROR | MB_OK);
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // ── Run ────────────────────────────────────────────────────────────────
    int result = app.Run();

    // ── Cleanup ────────────────────────────────────────────────────────────
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);

    return result;
}
