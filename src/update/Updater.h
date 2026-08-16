#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace dhikr {

// ── Semantic version ─────────────────────────────────────────────────────────
struct SemVer {
    int major = 0, minor = 0, patch = 0;

    // Parse "1.2.3" (strips leading 'v' if present)
    static SemVer Parse(const std::wstring& s);

    bool operator>(const SemVer& o) const;
    bool operator==(const SemVer& o) const;
    std::wstring ToString() const;
};

// ── Release information from GitHub ─────────────────────────────────────────
struct ReleaseInfo {
    SemVer       version;
    std::wstring tag;           // e.g. "v1.1.0"
    std::wstring name;          // release title
    std::wstring installerUrl;  // HTTPS URL for DhikrReminderSetup.exe asset
    std::wstring releaseNotesUrl;
    bool         prerelease = false;
};

// ── Updater ──────────────────────────────────────────────────────────────────
// Checks GitHub Releases API once per 24 h (or on manual request).
// Never blocks the UI — all network I/O runs on a background thread.
// Download + install is fully user-initiated.
class Updater {
public:
    static Updater& Instance();

    // Download state
    enum class DownloadState { Idle, Checking, Downloading, Ready, Error };

    // ── Callbacks (called on background thread — PostMessage to UI if needed)
    using CheckCallback    = std::function<void(bool available, const ReleaseInfo&)>;
    using ProgressCallback = std::function<void(int percent)>;   // 0-100
    using DoneCallback     = std::function<void(bool success, const std::wstring& path)>;

    // Check if 24 h have elapsed; post WM_UPDATE_AVAILABLE to hwnd if newer
    void CheckIfDue(HWND hwnd);

    // Force an immediate check regardless of last-check timestamp
    void CheckNow(HWND hwnd);

    // Download the installer for the pending update (async)
    // progress: 0-100 percent; done: path to downloaded .exe or empty on error
    void DownloadUpdate(const ReleaseInfo& info,
                        ProgressCallback  progress,
                        DoneCallback      done);

    // Launch the installer (silently close the app first)
    // installerPath: full path to the downloaded DhikrReminderSetup.exe
    void LaunchInstaller(const std::wstring& installerPath, HWND mainHwnd);

    // Stop background operations and wait for threads to exit
    void Shutdown();

    // Access last fetched release info
    const ReleaseInfo& PendingRelease() const { return m_pending; }
    DownloadState      State()          const { return m_state.load(); }

    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

private:
    Updater() = default;
    ~Updater() = default;

    // ── Thread procs
    struct CheckCtx { Updater* self; HWND hwnd; };
    static DWORD WINAPI CheckThread(LPVOID param);

    struct DownloadCtx {
        Updater*         self;
        ReleaseInfo      info;
        ProgressCallback progress;
        DoneCallback     done;
    };
    static DWORD WINAPI DownloadThread(LPVOID param);

    // ── Network helpers
    // Fetches the latest stable release from GitHub Releases API.
    // Endpoint: GET https://api.github.com/repos/{owner}/{repo}/releases/latest
    bool FetchLatestRelease(ReleaseInfo& out);

    // Parse minimal JSON from GitHub API response (no third-party JSON lib)
    bool ParseGitHubResponse(const std::string& json, ReleaseInfo& out);

    // Find the Windows installer asset URL in a "assets" JSON array
    std::string ExtractInstallerUrl(const std::string& assetsJson);

    // Extract a string value for a JSON key in a flat object
    std::string JsonString(const std::string& json, const std::string& key);

    // ── State
    HWND             m_hwnd   = nullptr;
    ReleaseInfo      m_pending;
    std::atomic<DownloadState> m_state{ DownloadState::Idle };

    // Temporary path for the downloaded installer
    std::wstring     m_downloadPath;

    // Thread and cancellation management
    HANDLE           m_checkThread    = nullptr;
    HANDLE           m_downloadThread = nullptr;
    void*            m_hSession       = nullptr;
    std::mutex       m_mutex;
    bool             m_shuttingDown   = false;
};

} // namespace dhikr
