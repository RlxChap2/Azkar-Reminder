#pragma once
#include <vector>
#include <string>
#include "../../core/Settings.h"
#include "../../core/Database.h"

namespace dhikr {

enum class PageId {
    Home = 0,
    Reminders = 1,
    Library = 2,
    Settings = 3,
    About = 4
};

// Centralized state model for the UI thread.
// This prevents blocking SQLite or disk reads during WM_PAINT.
struct AppUIState {
    PageId currentPage = PageId::Home;
    
    // Cached Database statistics
    int todayDelivered = 0;
    int streakDays = 0;
    int totalDelivered = 0;
    int thisWeek = 0;
    std::vector<DailyStat> dailyStats;
    
    // Cached Settings
    AppSettings settings;
};

class UIStateManager {
public:
    static UIStateManager& Instance() {
        static UIStateManager instance;
        return instance;
    }
    
    const AppUIState& GetState() const { return m_state; }
    
    // Asynchronous refresh trigger
    void RefreshStateFromBackendAsync();
    
    // Used by Background worker to update the UI state and invalidate UI
    void CommitState(const AppUIState& newState);
    
    // Fast path to just change page
    void SetPage(PageId page);

private:
    UIStateManager() = default;
    AppUIState m_state;
};

}
