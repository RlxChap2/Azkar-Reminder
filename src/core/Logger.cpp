#include "Logger.h"
#include <array>
#include <algorithm>

namespace dhikr {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) m_file.close();
}

void Logger::Initialize(const std::wstring& logDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logDir = logDirectory;

    namespace fs = std::filesystem;
    try {
        fs::create_directories(fs::path(logDirectory));
    } catch (...) {}

    // Find next available file index
    for (int i = 0; i < kMaxFiles; ++i) {
        auto path = fs::path(logDirectory) / (L"dhikr_" + std::to_wstring(i) + L".log");
        if (!fs::exists(path) || fs::file_size(path) < kMaxFileSize) {
            m_fileIndex = i;
            m_file.open(path, std::ios::app);
            m_currentSize = fs::exists(path) ? (size_t)fs::file_size(path) : 0;
            break;
        }
    }

    if (!m_file.is_open()) {
        // All files full — overwrite oldest
        m_fileIndex = 0;
        auto path = fs::path(logDirectory) / L"dhikr_0.log";
        m_file.open(path, std::ios::trunc);
        m_currentSize = 0;
    }

    // Write startup marker
    if (m_file.is_open()) {
        m_file << "\n── Logger initialized " << CurrentTimestamp() << " ──\n";
        m_file.flush();
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file.is_open()) return;

    RotateIfNeeded();

    std::string line = "[" + CurrentTimestamp() + "] [" + LevelString(level) + "] " + message + "\n";
    m_file << line;
    m_file.flush();
    m_currentSize += line.size();
}

void Logger::RotateIfNeeded() {
    if (m_currentSize < kMaxFileSize) return;

    m_file.close();
    m_fileIndex = (m_fileIndex + 1) % kMaxFiles;

    namespace fs = std::filesystem;
    auto path = fs::path(m_logDir) / (L"dhikr_" + std::to_wstring(m_fileIndex) + L".log");
    m_file.open(path, std::ios::trunc);
    m_currentSize = 0;

    m_file << "── Log rotated " << CurrentTimestamp() << " ──\n";
    m_file.flush();
}

std::string Logger::CurrentTimestamp() {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::LevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "?????";
    }
}

} // namespace dhikr
