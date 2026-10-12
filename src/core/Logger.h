#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <ctime>
#include <iomanip>

namespace dhikr {

enum class LogLevel { Info, Warning, Error };

class Logger {
public:
    static Logger& Instance();

    void Initialize(const std::filesystem::path& logDirectory);
    void Log(LogLevel level, const std::string& message);
    void Info(const std::string& msg)    { Log(LogLevel::Info,    msg); }
    void Warn(const std::string& msg)    { Log(LogLevel::Warning, msg); }
    void Error(const std::string& msg)   { Log(LogLevel::Error,   msg); }

    // Prevent copy
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();

    void RotateIfNeeded();
    std::string CurrentTimestamp();
    std::string LevelString(LogLevel level);

    std::mutex            m_mutex;
    std::ofstream         m_file;
    std::filesystem::path m_logDir;
    size_t           m_currentSize = 0;
    int              m_fileIndex = 0;
    static constexpr size_t kMaxFileSize = 1 * 1024 * 1024; // 1 MB
    static constexpr int    kMaxFiles    = 3;
};

} // namespace dhikr

// Convenience macros
#define LOG_INFO(msg)  dhikr::Logger::Instance().Info(msg)
#define LOG_WARN(msg)  dhikr::Logger::Instance().Warn(msg)
#define LOG_ERROR(msg) dhikr::Logger::Instance().Error(msg)
