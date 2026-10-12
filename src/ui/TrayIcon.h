#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include "../core/Settings.h"

namespace dhikr {

// ── TrayIcon ──────────────────────────────────────────────────────────────────
// Manages the Windows system tray icon and context menu.
// All menu items post WM_COMMAND messages to hwndParent.
class TrayIcon {
public:
    static TrayIcon& Instance();

    bool Create(HWND hwndParent, HINSTANCE hInstance);
    void Remove();

    // Update tooltip with next reminder time
    void UpdateTooltip(const std::wstring& text);

    // Show balloon notification
    void ShowBalloon(const std::wstring& title, const std::wstring& msg, DWORD dwInfoFlags = NIIF_INFO);

    // Handle WM_TRAY_CALLBACK messages
    void HandleMessage(HWND hwnd, LPARAM lParam);

    // Reflect active/paused state in tray icon
    void SetPaused(bool paused);

    // Update dynamic accent color icon
    void UpdateIcon(ColorTheme themeOverride = static_cast<ColorTheme>(-1));

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

private:
    TrayIcon() = default;
    ~TrayIcon() { Remove(); }

    void ShowContextMenu(HWND hwnd);

    NOTIFYICONDATAW m_nid  {};
    HWND            m_hwnd = nullptr;
    HINSTANCE       m_hInst= nullptr;
    HICON           m_dynamicIcon = nullptr;
    bool            m_created = false;
    bool            m_paused  = false;
};

} // namespace dhikr
