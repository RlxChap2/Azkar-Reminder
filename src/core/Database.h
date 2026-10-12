#pragma once
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include "../third_party/sqlite3.h"

namespace dhikr {

// ── Daily stat row ────────────────────────────────────────────────────────────
struct DailyStat {
    std::string date;         // "YYYY-MM-DD"
    int         delivered;    // reminders delivered
    int         dismissed;    // user dismissed
    int         snoozed;      // user snoozed
};

// ── Reminder history row ──────────────────────────────────────────────────────
struct ReminderEvent {
    int64_t     id;
    std::string timestamp;    // ISO 8601
    std::string contentId;    // content item ID
    std::string category;
    std::string action;       // "delivered", "dismissed", "snoozed"
};


// ── Database wrapper ──────────────────────────────────────────────────────────
class Database {
public:
    static Database& Instance();

    bool Open(const std::filesystem::path& dbPath);
    void Close();

    // Reminder history
    bool InsertReminderEvent(const std::string& contentId,
                             const std::string& category,
                             const std::string& action);

    // Daily stats
    int  GetTodayDeliveredCount();
    int  GetTodayGoalCount();   // from settings
    int  GetTotalCount();
    int  GetCurrentStreakDays();
    int  GetWeekCount();

    std::vector<DailyStat>     GetDailyStats(int days = 30);
    std::vector<ReminderEvent> GetRecentEvents(int limit = 100);
    std::vector<std::string>   GetRecentContentIds(int limit = 50);
    std::vector<std::pair<std::string, int>> GetTopDhikrs(int limit = 5);

    // Maintenance
    bool Vacuum();
    bool Checkpoint();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    Database() = default;
    ~Database() { Close(); }

    bool ExecuteSimple(const char* sql);
    bool CreateSchema();
    std::string CurrentISO8601();
    std::string TodayString();

    sqlite3* m_db = nullptr;
};

} // namespace dhikr
