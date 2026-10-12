#include <QApplication>
#include <QFontDatabase>
#include <iostream>

#include "core/PlatformPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "core/Database.h"
#include "core/ContentEngine.h"
#include "core/Scheduler.h"
#include "core/Localization.h"

#include "ui/qt/MainWindow.h"
#include "ui/qt/TrayController.h"
#include "ui/qt/ThemeManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Azkar Reminder");
    app.setApplicationVersion("1.2.0");
    app.setOrganizationName("Azkar");
    app.setQuitOnLastWindowClosed(false);

    bool startInBackground = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--background" || arg == "-b") {
            startInBackground = true;
        }
    }

    // 1. Initialize Directories & Logging
    dhikr::PlatformPaths::EnsureDirectoriesExist();
    dhikr::Logger::Instance().Initialize(dhikr::PlatformPaths::GetLogsDir());
    LOG_INFO("Starting Azkar Reminder v1.2.0 (Qt 6 Cross-Platform)");

    // 2. Load Settings & Localization
    dhikr::Settings::Instance().Load(dhikr::PlatformPaths::GetConfigDir());
    dhikr::Localization::Instance().SetLanguage(
        dhikr::Settings::Instance().Get().language == L"en" ? L"en" : L"ar"
    );

    if (dhikr::Localization::Instance().IsRTL()) {
        QApplication::setLayoutDirection(Qt::RightToLeft);
    } else {
        QApplication::setLayoutDirection(Qt::LeftToRight);
    }

    // 3. Initialize Database
    auto dbPath = dhikr::PlatformPaths::GetDataDir() / "history.db";
    dhikr::Database::Instance().Open(dbPath);

    // 4. Load Dhikr Content
    auto dataContent = dhikr::PlatformPaths::GetDataDir() / "content.md";
    if (std::filesystem::exists(dataContent)) {
        dhikr::ContentEngine::Instance().Load(dataContent);
    } else if (std::filesystem::exists("assets/content.md")) {
        dhikr::ContentEngine::Instance().Load("assets/content.md");
    } else if (std::filesystem::exists("content.md")) {
        dhikr::ContentEngine::Instance().Load("content.md");
    } else {
        dhikr::ContentEngine::Instance().LoadFromResource();
    }

    // 5. Create UI & Tray
    dhikr::qt::MainWindow mainWindow;
    dhikr::qt::TrayController trayController(&mainWindow);
    mainWindow.SetTrayController(&trayController);

    trayController.Show();

    if (!startInBackground) {
        mainWindow.show();
    }

    LOG_INFO("Application event loop started");
    int exitCode = app.exec();

    // 6. Graceful cleanup
    dhikr::Scheduler::Instance().Stop();
    dhikr::Database::Instance().Close();
    LOG_INFO("Application exited cleanly with code: " + std::to_string(exitCode));

    return exitCode;
}
