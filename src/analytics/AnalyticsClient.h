#pragma once
#include <windows.h>
#include <string>

namespace dhikr {

// ── AnalyticsClient ────────────────────────────────────────────────────────────
// Flushes queued anonymous events to a simple HTTP endpoint.
// Only operates when analyticsEnabled = true.
// No personal information is ever sent.
class AnalyticsClient {
public:
    static AnalyticsClient& Instance();

    // Schedule a periodic flush (every 24h)
    void ScheduleFlush(HWND hwnd);

    // Flush queued events now (called on WM_ANALYTICS_FLUSH)
    void Flush();

    AnalyticsClient(const AnalyticsClient&) = delete;
    AnalyticsClient& operator=(const AnalyticsClient&) = delete;

private:
    AnalyticsClient() = default;
    ~AnalyticsClient() = default;

    // Simple HTTP POST via WinHTTP
    bool PostEvents(const std::string& jsonBody);

    std::wstring m_endpoint = L"https://dhikr-analytics.example.com/events"; // placeholder
};

} // namespace dhikr
