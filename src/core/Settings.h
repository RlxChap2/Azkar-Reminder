#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace dhikr {

// ── Notification position ────────────────────────────────────────────────────
enum class PopupPosition {
    TopLeft = 0, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

// ── Popup monitor target ────────────────────────────────────────────────────
enum class PopupMonitor { Primary = 0, Active, Specific };

// ── Day schedule entry ───────────────────────────────────────────────────────
struct DaySchedule {
    bool    enabled   = true;
    int     startHour = 8,  startMin = 0;
    int     endHour   = 23, endMin   = 0;
    int     frequencyMinutes = 30;
};

// ── Quiet hours entry ────────────────────────────────────────────────────────
struct QuietPeriod {
    int startHour = 12, startMin = 0;
    int endHour   = 14, endMin   = 0;
};

// ── Accent color preset ──────────────────────────────────────────────────────
struct Color {
    uint8_t r, g, b;
    Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
};

// ── Application settings ─────────────────────────────────────────────────────
struct AppSettings {
    // General
    std::wstring language       = L"en";  // "en", "ar", "system"
    bool         startWithWindows = true;
    std::wstring theme          = L"dark"; // "dark", "light", "system"
    Color        accentColor    = { 0xB7, 0xF3, 0x4A };

    // Reminders
    bool         remindersEnabled = true;
    DaySchedule  weeklySchedule[7]; // 0=Sunday
    std::vector<QuietPeriod> quietPeriods;

    // Popup
    PopupPosition popupPosition  = PopupPosition::TopCenter;
    PopupMonitor  popupMonitor   = PopupMonitor::Primary;
    int           specificMonitor = 0;
    int           popupDurationSec = 5;
    bool          animationEnabled = true;

    // Sound
    bool          soundEnabled    = true;
    int           soundVolume     = 80; // 0–100
    std::wstring  customSoundPath = L""; // Empty = default chime

    // Full-Screen & Distraction Awareness
    bool          suppressDuringFullscreen  = true;
    int           fullscreenSuppressionMode = 0; // 0 = Queue until exit, 1 = Suppress completely, 2 = Windows notification only

    // Privacy
    std::wstring  installationId   = L"";  // anonymous UUID, generated once

    // Internal
    bool          firstLaunch       = true;
    bool          paused            = false;
    int           pauseUntilUnix    = 0;      // 0 = not paused
    bool          analyticsEnabled  = false;  // internal disabled
    long long     lastUpdateCheck   = 0;      // Unix timestamp

    // Update checker — persisted so we can show the prompt after restart
    std::wstring  pendingUpdateVersion = L""; // e.g. L"1.1.0", empty = no pending
    std::wstring  pendingUpdateUrl     = L""; // HTTPS URL to DhikrReminderSetup.exe
};

// ── Settings manager ─────────────────────────────────────────────────────────
class Settings {
public:
    static Settings& Instance();

    // Load from file; creates defaults if not found
    bool Load(const std::wstring& appDataPath);

    // Persist immediately
    bool Save();

    // Accessors (Legacy - not thread safe against concurrent UI updates)
    AppSettings&       Get() { return m_settings; }
    const AppSettings& Get() const { return m_settings; }

    // Thread-safe settings access
    AppSettings GetCopy() const;
    void Update(const AppSettings& newSettings);

    // Change notification
    using ChangeCallback = std::function<void()>;
    void OnChanged(ChangeCallback cb) { m_changeCallbacks.push_back(cb); }
    void NotifyChanged();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    Settings() = default;

    std::wstring                m_filePath;
    AppSettings                 m_settings;
    mutable std::mutex          m_mutex;
    std::vector<ChangeCallback> m_changeCallbacks;
};

} // namespace dhikr
