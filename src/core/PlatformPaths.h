#pragma once
#include <filesystem>
#include <string>

namespace dhikr {

class PlatformPaths {
public:
    // User configuration directory (settings.json)
    // Windows: %LOCALAPPDATA%\Azkar Reminder
    // Linux: $XDG_CONFIG_HOME/azkar-reminder or ~/.config/azkar-reminder
    // macOS: ~/Library/Application Support/Azkar Reminder
    static std::filesystem::path GetConfigDir();

    // User data directory (history.db, content.json)
    // Windows: %LOCALAPPDATA%\Azkar Reminder
    // Linux: $XDG_DATA_HOME/azkar-reminder or ~/.local/share/azkar-reminder
    // macOS: ~/Library/Application Support/Azkar Reminder
    static std::filesystem::path GetDataDir();

    // Logs directory
    // Windows: %LOCALAPPDATA%\Azkar Reminder\logs
    // Linux: $XDG_STATE_HOME/azkar-reminder/logs or ~/.local/state/azkar-reminder/logs
    // macOS: ~/Library/Logs/Azkar Reminder
    static std::filesystem::path GetLogsDir();

    // Ensure all standard directories exist
    static bool EnsureDirectoriesExist();
};

} // namespace dhikr
