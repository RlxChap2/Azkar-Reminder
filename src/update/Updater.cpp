// Updater.cpp — GitHub Releases update checker
// Architecture:
//   CheckThread  → GitHub API → parse JSON → compare SemVer → post WM_UPDATE_AVAILABLE
//   DownloadThread → WinHTTP → stream to temp file → call DoneCallback
//   LaunchInstaller → ShellExecuteW installer with /SILENT → PostQuitMessage

#include "Updater.h"
#include "../core/Application.h"
#include "../core/ApplicationMetadata.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include "../core/Application.h"
#include "../../src/resource.h"
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace dhikr {

// ASCII-safe wstring → string conversion for log messages and URL components.
// GitHub API responses, version strings, and HTTPS URLs are always ASCII.
static std::string WideToNarrow(const std::wstring& s) {
    std::string r;
    r.reserve(s.size());
    for (wchar_t c : s) r += (char)(unsigned char)c;
    return r;
}

static std::string WideToNarrow(const wchar_t* p) {
    return p ? WideToNarrow(std::wstring(p)) : std::string{};
}

// ─────────────────────────────────────────────────────────────────────────────
//  SemVer
// ─────────────────────────────────────────────────────────────────────────────
SemVer SemVer::Parse(const std::wstring& s) {
    SemVer v;
    const wchar_t* p = s.c_str();
    if (*p == L'v' || *p == L'V') ++p;      // strip leading 'v'
    if (swscanf_s(p, L"%d.%d.%d", &v.major, &v.minor, &v.patch) < 1) {
        v.major = v.minor = v.patch = 0;
    }
    return v;
}

bool SemVer::operator>(const SemVer& o) const {
    if (major != o.major) return major > o.major;
    if (minor != o.minor) return minor > o.minor;
    return patch > o.patch;
}

bool SemVer::operator==(const SemVer& o) const {
    return major == o.major && minor == o.minor && patch == o.patch;
}

std::wstring SemVer::ToString() const {
    return std::to_wstring(major) + L"." +
           std::to_wstring(minor) + L"." +
           std::to_wstring(patch);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Updater singleton
// ─────────────────────────────────────────────────────────────────────────────
Updater& Updater::Instance() {
    static Updater instance;
    return instance;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public: trigger a background check
// ─────────────────────────────────────────────────────────────────────────────
void Updater::CheckIfDue(HWND hwnd) {
    auto& s = Settings::Instance().Get();
    time_t now = time(nullptr);
    // Check once per 24 h (86 400 s)
    if (now - s.lastUpdateCheck < 86400LL) {
        // If we already have a pending update stored from a previous check,
        // re-post the notification so the UI can show it.
        if (!s.pendingUpdateVersion.empty()) {
            m_pending.version    = SemVer::Parse(s.pendingUpdateVersion);
            m_pending.tag        = L"v" + s.pendingUpdateVersion;
            m_pending.installerUrl = s.pendingUpdateUrl;
            PostMessageW(hwnd, WM_UPDATE_AVAILABLE, 0, 0);
        }
        return;
    }
    m_hwnd = hwnd;
    auto* ctx = new CheckCtx{ this, hwnd };
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shuttingDown) { delete ctx; return; }
    if (m_checkThread) {
        if (WaitForSingleObject(m_checkThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(m_checkThread);
            m_checkThread = nullptr;
        } else {
            delete ctx;
            return; // Already checking
        }
    }
    m_checkThread = CreateThread(nullptr, 0, CheckThread, ctx, 0, nullptr);
    if (!m_checkThread) delete ctx;
}

void Updater::CheckNow(HWND hwnd) {
    m_hwnd = hwnd;
    auto* ctx = new CheckCtx{ this, hwnd };
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shuttingDown) { delete ctx; return; }
    if (m_checkThread) {
        if (WaitForSingleObject(m_checkThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(m_checkThread);
            m_checkThread = nullptr;
        } else {
            delete ctx;
            return; // Already checking
        }
    }
    m_checkThread = CreateThread(nullptr, 0, CheckThread, ctx, 0, nullptr);
    if (!m_checkThread) delete ctx;
}

void Updater::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shuttingDown = true;
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession); // Abort any pending network I/O
        m_hSession = nullptr;
    }
    if (m_checkThread) {
        WaitForSingleObject(m_checkThread, INFINITE);
        CloseHandle(m_checkThread);
        m_checkThread = nullptr;
    }
    if (m_downloadThread) {
        WaitForSingleObject(m_downloadThread, INFINITE);
        CloseHandle(m_downloadThread);
        m_downloadThread = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Background check thread
// ─────────────────────────────────────────────────────────────────────────────
DWORD WINAPI Updater::CheckThread(LPVOID param) {
    auto* ctx = reinterpret_cast<CheckCtx*>(param);
    Updater* self = ctx->self;
    HWND hwnd = ctx->hwnd;
    delete ctx;

    self->m_state.store(DownloadState::Checking);

    ReleaseInfo info;
    bool ok = self->FetchLatestRelease(info);

    // Update last-check timestamp regardless of outcome
    auto& s = Settings::Instance().Get();
    s.lastUpdateCheck = (long long)time(nullptr);
    Settings::Instance().Save();

    if (!ok) {
        self->m_state.store(DownloadState::Idle);
        return 0;
    }

    // Compare against the running version
    SemVer current = SemVer::Parse(ApplicationMetadata::AppVersion);
    if (info.version > current && !info.prerelease) {
        self->m_pending = info;

        // Persist so we can re-show the prompt without a network round-trip
        s.pendingUpdateVersion = info.version.ToString();
        s.pendingUpdateUrl     = info.installerUrl;
        Settings::Instance().Save();

        LOG_INFO("Update available: " + WideToNarrow(info.tag) +
            " -- " + WideToNarrow(info.installerUrl));

        PostMessageW(hwnd, WM_UPDATE_AVAILABLE, 0, 0);
    } else {
        // Clear any stale pending update
        s.pendingUpdateVersion = L"";
        s.pendingUpdateUrl     = L"";
        Settings::Instance().Save();
        LOG_INFO(std::string("Application is up to date (") + WideToNarrow(ApplicationMetadata::AppVersion) + ")");
    }

    self->m_state.store(DownloadState::Idle);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  GitHub Releases API fetch
//  GET https://api.github.com/repos/{owner}/{repo}/releases/latest
//  Accept: application/vnd.github+json
// ─────────────────────────────────────────────────────────────────────────────
bool Updater::FetchLatestRelease(ReleaseInfo& out) {
    // Build the path: /repos/{kGitHubRepo}/releases/latest
    // kGitHubRepo is "owner/repo"
    std::wstring repo(Application::kGitHubRepo);
    std::wstring path = L"/repos/" + repo + L"/releases/latest";

    // User-Agent header is mandatory for GitHub API
    std::wstring ua = std::wstring(L"AzkarReminder/") + ApplicationMetadata::AppVersion;

    HINTERNET hSession = WinHttpOpen(ua.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shuttingDown) {
            if (hSession) WinHttpCloseHandle(hSession);
            return false;
        }
        m_hSession = hSession;
    }

    if (!hSession) {
        LOG_WARN("Updater: WinHttpOpen failed");
        return false;
    }

    bool result = false;
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect) {
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hReq) {
            // Timeouts: resolve 10s, connect 10s, send 15s, receive 30s
            WinHttpSetTimeouts(hReq, 10000, 10000, 15000, 30000);

            // GitHub API requires Accept header
            const wchar_t* hdrs = L"Accept: application/vnd.github+json\r\n"
                                   L"X-GitHub-Api-Version: 2022-11-28\r\n";
            if (WinHttpSendRequest(hReq, hdrs, (DWORD)-1,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hReq, nullptr)) {

                // Read full response (GitHub latest-release JSON is ~10-40 KB)
                std::string body;
                body.reserve(32768);
                char buf[4096];
                DWORD read = 0;
                while (WinHttpReadData(hReq, buf, sizeof(buf) - 1, &read) && read > 0) {
                    body.append(buf, read);
                }

                if (!body.empty()) {
                    result = ParseGitHubResponse(body, out);
                } else {
                    LOG_WARN("Updater: empty response from GitHub API");
                }
            } else {
                LOG_WARN("Updater: WinHttpSendRequest/ReceiveResponse failed ("
                    + std::to_string(GetLastError()) + ")");
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConnect);
    } else {
        LOG_WARN("Updater: WinHttpConnect failed");
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_hSession == hSession) {
            WinHttpCloseHandle(hSession);
            m_hSession = nullptr;
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal JSON parser — avoids third-party dependencies.
//  Handles the specific structure of the GitHub releases/latest response.
// ─────────────────────────────────────────────────────────────────────────────
std::string Updater::JsonString(const std::string& json, const std::string& key) {
    // Finds the value of "key": "value" in a JSON object.
    // Does not handle nested objects for the same key name — sufficient for
    // the flat top-level fields we need (tag_name, name, html_url, prerelease).
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};

    pos += needle.size();
    // Skip whitespace and ':'
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) ++pos;

    if (pos >= json.size()) return {};

    if (json[pos] == '"') {
        // String value
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos; // skip escape
            }
            val += json[pos++];
        }
        return val;
    } else {
        // Non-string value (bool, number) — read until delimiter
        std::string val;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != '\n') {
            val += json[pos++];
        }
        // Trim whitespace
        while (!val.empty() && std::isspace((unsigned char)val.back())) val.pop_back();
        return val;
    }
}

std::string Updater::ExtractInstallerUrl(const std::string& assetsJson) {
    // Scan through the assets array for a browser_download_url ending in
    // "Setup.exe" or ".exe" from a name that matches the installer pattern.
    // We look for both "DhikrReminderSetup.exe" and any "*Setup.exe".
    std::string result;
    size_t pos = 0;

    while (pos < assetsJson.size()) {
        // Find next "browser_download_url"
        auto u = assetsJson.find("\"browser_download_url\"", pos);
        if (u == std::string::npos) break;

        // Also find the "name" just before this URL
        auto namePos = assetsJson.rfind("\"name\"", u);
        std::string assetName;
        if (namePos != std::string::npos) {
            // Extract name value
            auto colonPos = assetsJson.find(':', namePos + 6);
            if (colonPos != std::string::npos) {
                auto q1 = assetsJson.find('"', colonPos + 1);
                auto q2 = assetsJson.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    assetName = assetsJson.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }

        // Extract the URL value
        auto colonPos = assetsJson.find(':', u + 22);
        if (colonPos == std::string::npos) break;
        auto q1 = assetsJson.find('"', colonPos + 1);
        auto q2 = assetsJson.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) break;

        std::string url = assetsJson.substr(q1 + 1, q2 - q1 - 1);
        pos = q2 + 1;

        // Prefer an asset named like "DhikrReminderSetup.exe"
        bool isExe = (assetName.find("Setup.exe") != std::string::npos ||
                     (assetName.find(".exe") != std::string::npos &&
                      url.find("https://") == 0));
        if (isExe) {
            result = url;
            // Prefer the one with "Setup" in the name
            if (assetName.find("Setup") != std::string::npos) break;
        }
    }
    return result;
}

bool Updater::ParseGitHubResponse(const std::string& json, ReleaseInfo& out) {
    std::string tagName = JsonString(json, "tag_name");
    if (tagName.empty()) {
        LOG_WARN("Updater: JSON missing tag_name");
        return false;
    }

    std::wstring tagW(tagName.begin(), tagName.end());
    out.tag        = tagW;
    out.version    = SemVer::Parse(tagW);
    out.name       = std::wstring(JsonString(json, "name").begin(),
                                  JsonString(json, "name").end());
    out.prerelease = (JsonString(json, "prerelease") == "true");

    // Release notes page
    std::string htmlUrl = JsonString(json, "html_url");
    out.releaseNotesUrl = std::wstring(htmlUrl.begin(), htmlUrl.end());

    // Find the "assets" array and scan for installer URL
    auto assetsStart = json.find("\"assets\"");
    if (assetsStart != std::string::npos) {
        auto arrayStart = json.find('[', assetsStart);
        auto arrayEnd   = json.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string assetsArr = json.substr(arrayStart, arrayEnd - arrayStart + 1);
            std::string url = ExtractInstallerUrl(assetsArr);
            out.installerUrl = std::wstring(url.begin(), url.end());
        }
    }

    return out.version.major > 0 || out.version.minor > 0 || out.version.patch > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Download update installer (async)
// ─────────────────────────────────────────────────────────────────────────────
void Updater::DownloadUpdate(const ReleaseInfo& info,
                             ProgressCallback   progressCb,
                             DoneCallback       doneCb) {
    if (info.installerUrl.empty()) {
        if (doneCb) doneCb(false, L"");
        return;
    }
    auto* ctx = new DownloadCtx{ this, info, progressCb, doneCb };
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shuttingDown) { if (doneCb) doneCb(false, L""); delete ctx; return; }
    if (m_downloadThread) {
        if (WaitForSingleObject(m_downloadThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(m_downloadThread);
            m_downloadThread = nullptr;
        } else {
            if (doneCb) doneCb(false, L""); // Already downloading
            delete ctx;
            return;
        }
    }
    m_downloadThread = CreateThread(nullptr, 0, DownloadThread, ctx, 0, nullptr);
    if (!m_downloadThread) { if (doneCb) doneCb(false, L""); delete ctx; }
}

DWORD WINAPI Updater::DownloadThread(LPVOID param) {
    auto* ctx = reinterpret_cast<DownloadCtx*>(param);
    Updater* self = ctx->self;

    self->m_state.store(DownloadState::Downloading);

    // Build a temp file path: %TEMP%\AzkarReminderSetup_<ver>.exe
    wchar_t tmpDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpDir);
    std::wstring version = ctx->info.version.ToString();
    std::wstring tmpPath = std::wstring(tmpDir) + L"AzkarReminderSetup_" + version + L".exe";

    // Parse URL for WinHTTP
    const std::wstring& url = ctx->info.installerUrl;

    // Extract host and path from the HTTPS URL
    // Expected format: https://github.com/.../.exe
    std::wstring host, path;
    {
        size_t schemeEnd = url.find(L"://");
        if (schemeEnd == std::wstring::npos) {
            LOG_WARN("Updater: invalid installer URL");
            self->m_state.store(DownloadState::Error);
            if (ctx->done) ctx->done(false, L"");
            delete ctx;
            return 1;
        }
        size_t hostStart = schemeEnd + 3;
        size_t pathStart = url.find(L'/', hostStart);
        if (pathStart == std::wstring::npos) pathStart = url.size();
        host = url.substr(hostStart, pathStart - hostStart);
        path = (pathStart < url.size()) ? url.substr(pathStart) : L"/";
    }

    std::wstring ua = std::wstring(L"AzkarReminder/") + ApplicationMetadata::AppVersion;
    HINTERNET hSession = WinHttpOpen(ua.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    {
        std::lock_guard<std::mutex> lock(self->m_mutex);
        if (self->m_shuttingDown) {
            if (hSession) WinHttpCloseHandle(hSession);
            if (ctx->done) ctx->done(false, L"");
            delete ctx;
            return 1;
        }
        self->m_hSession = hSession;
    }

    bool success = false;
    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (hReq) {
                // Longer timeout for download
                WinHttpSetTimeouts(hReq, 10000, 10000, 30000, 60000);

                if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(hReq, nullptr)) {

                    // Get Content-Length for progress reporting
                    DWORD totalSize = 0;
                    DWORD bufLen = sizeof(totalSize);
                    WinHttpQueryHeaders(hReq,
                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &totalSize, &bufLen, nullptr);

                    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0,
                        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

                    if (hFile != INVALID_HANDLE_VALUE) {
                        char buf[65536];
                        DWORD read = 0, written = 0, totalRead = 0;
                        success = true;
                        while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0) {
                            WriteFile(hFile, buf, read, &written, nullptr);
                            totalRead += read;
                            if (totalSize > 0 && ctx->progress) {
                                int pct = (int)((double)totalRead / totalSize * 100.0);
                                ctx->progress(std::min(pct, 99));
                            }
                        }
                        CloseHandle(hFile);
                        if (ctx->progress) ctx->progress(100);
                    }
                }
                WinHttpCloseHandle(hReq);
            }
            WinHttpCloseHandle(hConnect);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(self->m_mutex);
        if (self->m_hSession == hSession) {
            if (hSession) WinHttpCloseHandle(hSession);
            self->m_hSession = nullptr;
        }
    }

    self->m_state.store(success ? DownloadState::Ready : DownloadState::Error);
    if (success) {
        self->m_downloadPath = tmpPath;
        LOG_INFO("Updater: installer downloaded to " + WideToNarrow(tmpPath));
    } else {
        LOG_WARN("Updater: download failed");
    }

    if (ctx->done) ctx->done(success, success ? tmpPath : L"");
    delete ctx;
    return success ? 0 : 1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Launch installer and quit
// ─────────────────────────────────────────────────────────────────────────────
void Updater::LaunchInstaller(const std::wstring& installerPath, HWND mainHwnd) {
    if (installerPath.empty()) return;

    // Verify the file actually exists before launching
    if (GetFileAttributesW(installerPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        LOG_WARN("Updater: installer file not found: " + WideToNarrow(installerPath));
        return;
    }

    LOG_INFO("Updater: launching installer " + WideToNarrow(installerPath));

    // /SILENT — Inno Setup: show progress, no wizard pages
    // /NORESTART — let the installer decide (we restart the app ourselves)
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas"; // request elevation
    sei.lpFile       = installerPath.c_str();
    sei.lpParameters = L"/SILENT /NORESTART";
    sei.nShow        = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        // Post quit message — the installer will re-launch the app after install
        PostMessageW(mainHwnd, WM_CLOSE, 0, 0);
    } else {
        LOG_WARN("Updater: ShellExecuteExW failed (" + std::to_string(GetLastError()) + ")");
    }
}

} // namespace dhikr
