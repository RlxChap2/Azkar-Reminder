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
#include "../update/Updater.h"
#include "../../src/resource.h"
#include <shlobj.h>
#include <sstream>
#include <filesystem>
#include <comdef.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

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
                LOG_ERROR("Failed to load embedded content resource. Ensure IDR_CONTENT_MD is properly compiled into the executable.");
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

    // Auto-migrate settings/database/content from the legacy path ONLY if needed
    // DO NOT migrate from executable path as it breaks installations
    
    std::wstring newDb = m_appDataPath + L"\\history.db";
    std::wstring newSettings = m_appDataPath + L"\\settings.json";
    std::wstring newContent = m_appDataPath + L"\\content.json";

    auto migrateFile = [](const std::wstring& oldFile, const std::wstring& newFile) {
        if (GetFileAttributesW(newFile.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW(oldFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(oldFile.c_str(), newFile.c_str(), TRUE);
            return true;
        }
        return false;
    };

    // Migrate database (older version used dhikr.db, new version expects history.db)
    if (!migrateFile(oldPath + L"\\history.db", newDb)) {
        migrateFile(oldPath + L"\\dhikr.db", newDb);
    }
    
    migrateFile(oldPath + L"\\settings.json", newSettings);
    migrateFile(oldPath + L"\\content.json", newContent);

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

    // Check for updates (async, once per 24h)
    // NOTE: Updater::CheckIfDue was intentionally removed to avoid ML heuristics.
    // The application no longer phones home silently in the background.

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

    Updater::Instance().Shutdown();
    Database::Instance().Checkpoint();
    Database::Instance().Close();

    Settings::Instance().Save();

    if (m_dwriteFactory) { m_dwriteFactory->Release(); m_dwriteFactory = nullptr; }
    if (m_d2dFactory)    { m_d2dFactory->Release();    m_d2dFactory    = nullptr; }

    LOG_INFO("Application shutdown complete");
}

bool Application::SetStartWithWindows(bool enable) {
    // 1. Clean up legacy registry keys unconditionally
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, L"AzkarReminder");
        RegDeleteValueW(key, L"DhikrReminder");
        RegCloseKey(key);
    }

    // 2. COM initialization
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool coInit = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) {
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) {
        pService->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    const wchar_t* taskName = L"Azkar Reminder";

    if (!enable) {
        pRootFolder->DeleteTask(_bstr_t(taskName), 0);
        pRootFolder->Release();
        pService->Release();
        if (coInit) CoUninitialize();
        return true;
    }

    pRootFolder->DeleteTask(_bstr_t(taskName), 0); // remove old if exists

    ITaskDefinition* pTask = NULL;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) {
        pRootFolder->Release();
        pService->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IPrincipal* pPrincipal = NULL;
    hr = pTask->get_Principal(&pPrincipal);
    if (SUCCEEDED(hr)) {
        pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_LUA);
        pPrincipal->Release();
    }

    IRegistrationInfo* pRegInfo = NULL;
    pTask->get_RegistrationInfo(&pRegInfo);
    if (pRegInfo) {
        pRegInfo->put_Author(_bstr_t(L"Ahmed Mohamed Kamel"));
        pRegInfo->put_Description(_bstr_t(L"Start Azkar Reminder automatically when Windows starts"));
        pRegInfo->Release();
    }

    ITaskSettings* pSettings = NULL;
    pTask->get_Settings(&pSettings);
    if (pSettings) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S")); // 0 = no limit
        pSettings->Release();
    }

    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTask->get_Triggers(&pTriggerCollection);
    if (SUCCEEDED(hr)) {
        ITrigger* pTrigger = NULL;
        hr = pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
        if (SUCCEEDED(hr)) {
            ILogonTrigger* pLogonTrigger = NULL;
            pTrigger->QueryInterface(IID_ILogonTrigger, (void**)&pLogonTrigger);
            if (pLogonTrigger) {
                pLogonTrigger->put_Id(_bstr_t(L"Trigger1"));
                pLogonTrigger->Release();
            }
            pTrigger->Release();
        }
        pTriggerCollection->Release();
    }

    IActionCollection* pActionCollection = NULL;
    hr = pTask->get_Actions(&pActionCollection);
    if (SUCCEEDED(hr)) {
        IAction* pAction = NULL;
        hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        if (SUCCEEDED(hr)) {
            IExecAction* pExecAction = NULL;
            pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
            if (pExecAction) {
                wchar_t exePath[MAX_PATH];
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                pExecAction->put_Path(_bstr_t(exePath));
                pExecAction->put_Arguments(_bstr_t(L"--background"));
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActionCollection->Release();
    }

    IRegisteredTask* pRegisteredTask = NULL;
    hr = pRootFolder->RegisterTaskDefinition(
        _bstr_t(taskName),
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(),
        _variant_t(),
        TASK_LOGON_INTERACTIVE_TOKEN,
        _variant_t(L""),
        &pRegisteredTask);

    if (pRegisteredTask) pRegisteredTask->Release();
    pTask->Release();
    pRootFolder->Release();
    pService->Release();
    if (coInit) CoUninitialize();

    return SUCCEEDED(hr);
}

bool Application::GetStartsWithWindows() const {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool coInit = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) {
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) {
        pService->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IRegisteredTask* pRegisteredTask = NULL;
    hr = pRootFolder->GetTask(_bstr_t(L"Azkar Reminder"), &pRegisteredTask);
    
    bool exists = false;
    if (SUCCEEDED(hr) && pRegisteredTask) {
        exists = true;
        pRegisteredTask->Release();
    }

    pRootFolder->Release();
    pService->Release();
    if (coInit) CoUninitialize();

    return exists;
}

} // namespace dhikr
