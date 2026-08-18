#include "Database.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace dhikr {

Database& Database::Instance() {
    static Database instance;
    return instance;
}

bool Database::Open(const std::wstring& dbPath) {
    if (m_db) {
        LOG_WARN("Database already open");
        return true;
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Path(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, utf8Path.data(), size, nullptr, nullptr);
    LOG_INFO("Opening SQLite database at: " + utf8Path);

    int rc = sqlite3_open_v2(utf8Path.c_str(), &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR(std::string("SQLite open failed: ") + sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    LOG_INFO("Executing pragmas...");
    // Performance pragmas
    ExecuteSimple("PRAGMA journal_mode=WAL;");
    ExecuteSimple("PRAGMA synchronous=NORMAL;");
    ExecuteSimple("PRAGMA foreign_keys=ON;");
    ExecuteSimple("PRAGMA cache_size=-4096;"); // 4MB cache

    LOG_INFO("Creating schema...");
    if (!CreateSchema()) {
        LOG_ERROR("Failed to create database schema");
        return false;
    }

    LOG_INFO("Database opened successfully");
    return true;
}

void Database::Close() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
        LOG_INFO("Database closed");
    }
}

bool Database::CreateSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS reminder_events (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp   TEXT NOT NULL,
            content_id  TEXT NOT NULL,
            category    TEXT NOT NULL DEFAULT '',
            action      TEXT NOT NULL DEFAULT 'delivered'
        );

        CREATE INDEX IF NOT EXISTS idx_reminder_timestamp ON reminder_events(timestamp);
        CREATE INDEX IF NOT EXISTS idx_reminder_date ON reminder_events(substr(timestamp,1,10));


    )";
    return ExecuteSimple(sql);
}

bool Database::ExecuteSimple(const char* sql) {
    if (!m_db) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR(std::string("SQLite exec error: ") + (errMsg ? errMsg : "unknown"));
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string Database::CurrentISO8601() {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::string Database::TodayString() {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

bool Database::InsertReminderEvent(const std::string& contentId,
                                    const std::string& category,
                                    const std::string& action) {
    if (!m_db) return false;
    const char* sql =
        "INSERT INTO reminder_events (timestamp, content_id, category, action) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, CurrentISO8601().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, contentId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, action.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

int Database::GetTodayDeliveredCount() {
    if (!m_db) return 0;
    std::string today = TodayString();
    const char* sql =
        "SELECT COUNT(*) FROM reminder_events "
        "WHERE action='delivered' AND substr(timestamp,1,10)=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, today.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int Database::GetTotalCount() {
    if (!m_db) return 0;
    const char* sql = "SELECT COUNT(*) FROM reminder_events WHERE action='delivered';";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int Database::GetWeekCount() {
    if (!m_db) return 0;
    const char* sql =
        "SELECT COUNT(*) FROM reminder_events "
        "WHERE action='delivered' AND timestamp >= datetime('now', '-7 days');";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int Database::GetCurrentStreakDays() {
    if (!m_db) return 0;
    // Count consecutive days back from today that have at least one delivered reminder
    const char* sql =
        "SELECT DISTINCT substr(timestamp,1,10) as day "
        "FROM reminder_events WHERE action='delivered' "
        "ORDER BY day DESC LIMIT 365;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    std::vector<std::string> days;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* d = (const char*)sqlite3_column_text(stmt, 0);
        if (d) days.push_back(d);
    }
    sqlite3_finalize(stmt);

    if (days.empty()) return 0;

    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    int streak = 0;
    for (size_t i = 0; i < days.size(); ++i) {
        std::tm expected = tm;
        expected.tm_mday -= (int)i;
        expected.tm_isdst = -1; // Let mktime determine DST for the shifted day
        mktime(&expected);
        std::ostringstream ss;
        ss << std::put_time(&expected, "%Y-%m-%d");
        if (days[i] == ss.str()) {
            ++streak;
        } else {
            break;
        }
    }
    return streak;
}

std::vector<DailyStat> Database::GetDailyStats(int days) {
    std::vector<DailyStat> result;
    if (!m_db) return result;

    const char* sql =
        "SELECT substr(timestamp,1,10) as day, "
        "SUM(CASE WHEN action='delivered' THEN 1 ELSE 0 END) as delivered, "
        "SUM(CASE WHEN action='dismissed' THEN 1 ELSE 0 END) as dismissed, "
        "SUM(CASE WHEN action='snoozed' THEN 1 ELSE 0 END) as snoozed "
        "FROM reminder_events "
        "WHERE timestamp >= datetime('now', ? || ' days') "
        "GROUP BY day ORDER BY day DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    std::string daysParam = "-" + std::to_string(days);
    sqlite3_bind_text(stmt, 1, daysParam.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DailyStat stat;
        stat.date      = (const char*)sqlite3_column_text(stmt, 0);
        stat.delivered = sqlite3_column_int(stmt, 1);
        stat.dismissed = sqlite3_column_int(stmt, 2);
        stat.snoozed   = sqlite3_column_int(stmt, 3);
        result.push_back(stat);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::string> Database::GetRecentContentIds(int limit) {
    std::vector<std::string> result;
    if (!m_db) return result;

    const char* sql =
        "SELECT content_id FROM reminder_events "
        "ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return result;
}


bool Database::Vacuum() {
    return ExecuteSimple("VACUUM;");
}

bool Database::Checkpoint() {
    return ExecuteSimple("PRAGMA wal_checkpoint(TRUNCATE);");
}

} // namespace dhikr
