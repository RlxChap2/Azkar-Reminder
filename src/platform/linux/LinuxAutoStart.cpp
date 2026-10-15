#include "LinuxAutoStart.h"
#include "../../core/Logger.h"
#include <fstream>
#include <cstdlib>

#ifndef _WIN32
#ifndef __APPLE__

namespace dhikr {

std::unique_ptr<IAutoStart> IAutoStart::Create() {
    return std::make_unique<LinuxAutoStart>();
}

std::filesystem::path LinuxAutoStart::GetAutostartDesktopFilePath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path configDir;
    if (xdgConfig && *xdgConfig) {
        configDir = std::filesystem::path(xdgConfig);
    } else {
        const char* home = std::getenv("HOME");
        configDir = (home && *home) ? (std::filesystem::path(home) / ".config") : std::filesystem::path("/tmp");
    }
    return configDir / "autostart" / "azkar-reminder.desktop";
}

bool LinuxAutoStart::IsAutoStartEnabled() const {
    auto path = GetAutostartDesktopFilePath();
    if (!std::filesystem::exists(path)) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Hidden=true") != std::string::npos ||
            line.find("X-GNOME-Autostart-enabled=false") != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool LinuxAutoStart::SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath) {
    auto desktopPath = GetAutostartDesktopFilePath();
    std::error_code ec;

    if (!enable) {
        if (std::filesystem::exists(desktopPath)) {
            std::filesystem::remove(desktopPath, ec);
            if (ec) {
                LOG_ERROR("Failed to remove autostart desktop file: " + ec.message());
                return false;
            }
        }
        LOG_INFO("Linux autostart entry removed: " + desktopPath.string());
        return true;
    }

    // Determine binary execution path
    std::filesystem::path execPath = appPath;
    if (execPath.empty()) {
        const char* appimageEnv = std::getenv("APPIMAGE");
        if (appimageEnv && *appimageEnv) {
            execPath = appimageEnv;
        } else {
            try {
                execPath = std::filesystem::canonical("/proc/self/exe");
            } catch (...) {
                execPath = "/usr/bin/azkar-reminder";
            }
        }
    }

    auto autostartDir = desktopPath.parent_path();
    std::filesystem::create_directories(autostartDir, ec);
    if (ec) {
        LOG_ERROR("Failed to create autostart directory: " + ec.message());
        return false;
    }

    std::ofstream out(desktopPath);
    if (!out.is_open()) {
        LOG_ERROR("Failed to open autostart desktop file for writing: " + desktopPath.string());
        return false;
    }

    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Version=1.0\n";
    out << "Name=Azkar Reminder\n";
    out << "GenericName=Islamic Dhikr & Dua Reminder\n";
    out << "Comment=Quiet and beautiful Islamic Dhikr reminder\n";
    out << "Exec=\"" << execPath.string() << "\" --background\n";
    out << "Icon=azkar-reminder\n";
    out << "Terminal=false\n";
    out << "Categories=Utility;Clock;Qt;\n";
    out << "StartupNotify=false\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    out.flush();

    if (out.fail()) {
        LOG_ERROR("Failed writing desktop file content");
        return false;
    }

    LOG_INFO("Linux autostart desktop entry created: " + desktopPath.string());
    return true;
}

} // namespace dhikr

#endif // !__APPLE__
#endif // !_WIN32
