#include "AnalyticsClient.h"
#include "../core/Settings.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include <winhttp.h>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace dhikr {

AnalyticsClient& AnalyticsClient::Instance() {
    static AnalyticsClient instance;
    return instance;
}

void AnalyticsClient::ScheduleFlush(HWND hwnd) {
    // Check every 24h by posting ourselves with a timer
    // This is handled by the main message loop
    (void)hwnd; // actual timer set via SetTimer in main window if needed
}

void AnalyticsClient::Flush() {
    auto& s = Settings::Instance().Get();
    if (!s.analyticsEnabled) {
        Database::Instance().ClearAnalyticsQueue();
        return;
    }

    auto events = Database::Instance().DequeueAnalyticsEvents(50);
    if (events.empty()) return;

    // Build simple JSON array
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < events.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"event\":\"" << events[i].eventType << "\","
           << "\"data\":\"" << events[i].appVersion << "\","
           << "\"ts\":\"" << events[i].timestamp << "\"}";
    }
    ss << "]";

    if (PostEvents(ss.str())) {
        LOG_INFO("Analytics: flushed " + std::to_string(events.size()) + " events");
    } else {
        // Re-queue on failure
        LOG_WARN("Analytics: flush failed, events re-queued");
        Database::Instance().RequeueAnalyticsEvents(events);
    }
}

bool AnalyticsClient::PostEvents(const std::string& jsonBody) {
    // WinHTTP implementation — gracefully no-ops if server unreachable
    HINTERNET hSession = WinHttpOpen(L"AzkarReminder/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Parse endpoint URL
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize  = sizeof(urlComp);
    wchar_t scheme[16], host[128], path[256];
    urlComp.lpszScheme    = scheme; urlComp.dwSchemeLength = 16;
    urlComp.lpszHostName  = host;   urlComp.dwHostNameLength = 128;
    urlComp.lpszUrlPath   = path;   urlComp.dwUrlPathLength  = 256;
    WinHttpCrackUrl(m_endpoint.c_str(), 0, 0, &urlComp);

    bool secure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    HINTERNET hConnect = WinHttpConnect(hSession, host, urlComp.nPort, 0);
    bool result = false;

    if (hConnect) {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path,
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            secure ? WINHTTP_FLAG_SECURE : 0);
        if (hRequest) {
            std::wstring headers = L"Content-Type: application/json";
            std::string  body    = jsonBody;
            result = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0) &&
                WinHttpReceiveResponse(hRequest, nullptr);
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return result;
}

} // namespace dhikr
