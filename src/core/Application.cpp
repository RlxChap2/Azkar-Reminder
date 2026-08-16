#include "Application.h"
#include "Logger.h"
#include "Settings.h"
#include "Database.h"
#include "ContentEngine.h"
#include "Scheduler.h"
#include "ApplicationMetadata.h"
#include "Localization.h"
#include "../ui/MainWindow.h"
#include "../ui/TrayIcon.h"
#include "../ui/OnboardingFlow.h"
#include "../audio/AudioEngine.h"
#include "../analytics/AnalyticsClient.h"
#include "../update/Updater.h"
#include "../../src/resource.h"
#include <shlobj.h>
#include <sstream>
#include <filesystem>

namespace dhikr {

Application& Application::Instance() {
    static Application instance;
    return instance;
}

bool Application::Initialize(HINSTANCE hInstance) {
    if (m_initialized) return true;
    m_hInstance = hInstance;

    // 1. App data path first (needed by logger)
    if (!InitAppDataPath()) return false;

    // 2. Logger
    Logger::Instance().Initialize(m_appDataPath + L"\\logs");
    char verBuf[64];
    WideCharToMultiByte(CP_UTF8, 0, ApplicationMetadata::AppVersion, -1, verBuf, sizeof(verBuf), nullptr, nullptr);
    LOG_INFO(std::string("=== AzkarReminder v") + verBuf + " starting ===");

    // 3. Settings & Localization
    Settings::Instance().Load(m_appDataPath);
    std::wstring lang = Settings::Instance().Get().language;
    if (lang == L"system") {
        lang = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ARABIC) ? L"ar" : L"en";
    }
    Localization::Instance().SetLanguage(lang);

    // 4. Database
    if (!Database::Instance().Open(m_appDataPath + L"\\history.db")) {
        LOG_WARN("Database unavailable — statistics and history disabled");
    }

    // 5. DirectX factories
    if (!InitDirectX()) {
        LOG_ERROR("DirectX initialization failed");
        return false;
    }

    // 6. Content engine — load user's saved content first, then fall back to assets/content.md
    {
        std::wstring userContent = m_appDataPath + L"\\content.json";
        ContentEngine::Instance().SetSavePath(userContent);
        ContentLoadResult loadResult = ContentLoadResult::FileNotFound;
        if (std::filesystem::exists(userContent)) {
            loadResult = ContentEngine::Instance().Load(userContent);
            if (loadResult == ContentLoadResult::Success && !ContentEngine::Instance().IsEmpty()) {
                LOG_INFO("Loaded user content.json successfully (" + std::to_string(ContentEngine::Instance().Count()) + " items)");
            } else {
                loadResult = ContentLoadResult::EmptyContent;
            }
        }

        if (loadResult != ContentLoadResult::Success || ContentEngine::Instance().IsEmpty()) {
            loadResult = ContentEngine::Instance().LoadFromMarkdownResource(m_hInstance, IDR_CONTENT_MD);
            if (loadResult == ContentLoadResult::Success && !ContentEngine::Instance().IsEmpty()) {
                LOG_INFO("Loaded default content from embedded resource (" + std::to_string(ContentEngine::Instance().Count()) + " items)");
                ContentEngine::Instance().Save(); // Persist initial database to user's AppData
            } else {
                LOG_WARN("Failed to load embedded content resource, checking local files...");
                wchar_t exePathBuf[MAX_PATH];
                GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
                std::wstring exeDir = exePathBuf;
                size_t lastSlash = exeDir.find_last_of(L"\\/");
                if (lastSlash != std::wstring::npos) {
                    exeDir = exeDir.substr(0, lastSlash);
                }

                std::wstring mdCandidates[] = {
                    exeDir + L"\\content.md",
                    exeDir + L"\\assets\\content.md",
                    L"content.md"
                };

                for (const auto& mdPath : mdCandidates) {
                    if (std::filesystem::exists(mdPath)) {
                        loadResult = ContentEngine::Instance().LoadFromMarkdownFile(mdPath);
                        if (loadResult == ContentLoadResult::Success && !ContentEngine::Instance().IsEmpty()) {
                            LOG_INFO("Loaded fallback content from markdown file (" + std::to_string(ContentEngine::Instance().Count()) + " items)");
                            ContentEngine::Instance().Save();
                            break;
                        }
                    }
                }
            }
        }

        if (ContentEngine::Instance().IsEmpty()) {
            LOG_ERROR("ContentEngine has 0 items loaded!");
        }
    }

    // 7. Audio engine
    AudioEngine::Instance().Initialize(m_hInstance);

    // 8. Sync startup registry with settings
    SetStartWithWindows(Settings::Instance().Get().startWithWindows);

    m_initialized = true;
    return true;
}

bool Application::InitAppDataPath() {
    wchar_t* appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) {
        return false;
    }
    m_appDataPath = std::wstring(appData) + L"\\Azkar Reminder";
    std::wstring oldPath = std::wstring(appData) + L"\\Dhikr Reminder";
    CoTaskMemFree(appData);

    // Create directory if needed
    if (!CreateDirectoryW(m_appDataPath.c_str(), nullptr)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }

    // Auto-migrate settings/database from old folder if needed
    std::wstring oldDb = oldPath + L"\\dhikr.db";
    std::wstring newDb = m_appDataPath + L"\\dhikr.db";
    if (GetFileAttributesW(newDb.c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW(oldDb.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(oldDb.c_str(), newDb.c_str(), TRUE);
    }
    std::wstring oldSettings = oldPath + L"\\settings.json";
    std::wstring newSettings = m_appDataPath + L"\\settings.json";
    if (GetFileAttributesW(newSettings.c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW(oldSettings.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(oldSettings.c_str(), newSettings.c_str(), TRUE);
    }

    return true;
}

bool Application::InitDirectX() {
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        &m_d2dFactory);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D2D factory");
        return false;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DWrite factory");
        return false;
    }

    return true;
}

int Application::Run() {
    auto& settings = Settings::Instance().Get();

    // Show onboarding on first launch
    if (settings.firstLaunch) {
        OnboardingFlow onboarding;
        onboarding.Show(m_hInstance);
        settings.firstLaunch = false;
        Settings::Instance().Save();
    }

    // Create main window
    if (!MainWindow::Instance().Create(m_hInstance)) {
        LOG_ERROR("Failed to create main window");
        return 1;
    }
    m_mainHwnd = MainWindow::Instance().GetHwnd();

    // Show the window unless launched with --background.
    // The --background flag is used by the startup registry entry so the
    // app silently starts in the tray without opening a visible window.
    bool startBackground = (wcsstr(GetCommandLineW(), L"--background") != nullptr);
    if (!startBackground) {
        MainWindow::Instance().Show();
    }

    // Create tray icon
    TrayIcon::Instance().Create(m_mainHwnd, m_hInstance);

    // Start scheduler — it posts WM_REMINDER_TRIGGER to main window
    Scheduler::Instance().Start(m_mainHwnd);

    // Queue startup analytics event (if enabled)
    if (settings.analyticsEnabled) {
        char aVer[64];
        WideCharToMultiByte(CP_UTF8, 0, ApplicationMetadata::AppVersion, -1, aVer, sizeof(aVer), nullptr, nullptr);
        Database::Instance().QueueAnalyticsEvent("app_start", aVer);
        AnalyticsClient::Instance().ScheduleFlush(m_mainHwnd);
    }

    // Check for updates (async, once per 24h)
    Updater::Instance().CheckIfDue(m_mainHwnd);

    // ── Message loop ───────────────────────────────────────────────────────
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

void Application::Shutdown() {
    if (m_shuttingDown) return;
    m_shuttingDown = true;

    LOG_INFO("Application shutting down");

    Scheduler::Instance().Stop();
    TrayIcon::Instance().Remove();
    AudioEngine::Instance().Shutdown();

    // Final analytics flush
    auto& settings = Settings::Instance().Get();
    if (settings.analyticsEnabled) {
        char sVer[64];
        WideCharToMultiByte(CP_UTF8, 0, ApplicationMetadata::AppVersion, -1, sVer, sizeof(sVer), nullptr, nullptr);
        Database::Instance().QueueAnalyticsEvent("app_stop", sVer);
    }

    Updater::Instance().Shutdown();
    Database::Instance().Checkpoint();
    Database::Instance().Close();

    Settings::Instance().Save();

    if (m_dwriteFactory) { m_dwriteFactory->Release(); m_dwriteFactory = nullptr; }
    if (m_d2dFactory)    { m_d2dFactory->Release();    m_d2dFactory    = nullptr; }

    LOG_INFO("Application shutdown complete");
}

bool Application::SetStartWithWindows(bool enable) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;

    bool result;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" --background";
        result = (RegSetValueExW(key, L"AzkarReminder", 0, REG_SZ,
            (const BYTE*)cmd.c_str(),
            (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        // Clean up legacy key if it existed
        RegDeleteValueW(key, L"DhikrReminder");
    } else {
        result = (RegDeleteValueW(key, L"AzkarReminder") == ERROR_SUCCESS ||
                  GetLastError() == ERROR_FILE_NOT_FOUND);
        RegDeleteValueW(key, L"DhikrReminder");
    }
    RegCloseKey(key);
    return result;
}

bool Application::GetStartsWithWindows() const {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type = REG_SZ, size = 0;
    bool exists = (RegQueryValueExW(key, L"AzkarReminder",
                   nullptr, &type, nullptr, &size) == ERROR_SUCCESS ||
                   RegQueryValueExW(key, L"DhikrReminder",
                   nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(key);
    return exists;
}

} // namespace dhikr
