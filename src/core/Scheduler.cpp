#include "Scheduler.h"
#include "Settings.h"
#include "Logger.h"
#include "../../src/resource.h"
#include "../ui/TrayIcon.h"
#include <algorithm>
#include <cstring>

namespace dhikr {

Scheduler& Scheduler::Instance() {
    static Scheduler instance;
    return instance;
}

void Scheduler::Start(HWND hwnd) {
    if (m_running) return;
    m_hwnd = hwnd;

    // Create synchronisation primitives
    m_stopEvent   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_recalcEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_timerHandle = CreateWaitableTimerW(nullptr, FALSE, nullptr); // auto-reset

    m_running = true;
    m_thread  = std::thread(&Scheduler::WorkerThread, this);

    LOG_INFO("Scheduler started");
}

void Scheduler::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_stopEvent != INVALID_HANDLE_VALUE)   SetEvent(m_stopEvent);
    if (m_thread.joinable()) m_thread.join();

    if (m_timerHandle != INVALID_HANDLE_VALUE) { CloseHandle(m_timerHandle); m_timerHandle = INVALID_HANDLE_VALUE; }
    if (m_stopEvent   != INVALID_HANDLE_VALUE) { CloseHandle(m_stopEvent);   m_stopEvent   = INVALID_HANDLE_VALUE; }
    if (m_recalcEvent != INVALID_HANDLE_VALUE) { CloseHandle(m_recalcEvent); m_recalcEvent = INVALID_HANDLE_VALUE; }

    LOG_INFO("Scheduler stopped");
}

void Scheduler::Recalculate() {
    if (m_recalcEvent != INVALID_HANDLE_VALUE) SetEvent(m_recalcEvent);
}

void Scheduler::TriggerNow() {
    if (m_hwnd) PostMessageW(m_hwnd, WM_REMINDER_TRIGGER, 0, 0);
}

void Scheduler::Pause(int durationMinutes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (durationMinutes <= 0) {
        // Pause until end of day
        time_t now = time(nullptr);
        struct tm lt;
        localtime_s(&lt, &now);
        lt.tm_hour = 23; lt.tm_min = 59; lt.tm_sec = 59;
        m_pauseUntil = mktime(&lt);
    } else {
        m_pauseUntil = time(nullptr) + durationMinutes * 60;
    }
    m_state = ScheduleState::Paused;

    // Persist pause state
    auto& s = Settings::Instance().Get();
    s.paused = true;
    s.pauseUntilUnix = (int)m_pauseUntil;
    Settings::Instance().Save();

    Recalculate();
    LOG_INFO("Reminders paused for " + std::to_string(durationMinutes) + " minutes");
}

void Scheduler::PauseUntilTomorrow() {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    // Midnight tomorrow
    lt.tm_mday += 1;
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    int mins = (int)(mktime(&lt) - now) / 60;
    Pause(mins);
}

void Scheduler::Resume() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pauseUntil = 0;
    m_state = ScheduleState::Active;
    auto& s = Settings::Instance().Get();
    s.paused = false;
    s.pauseUntilUnix = 0;
    Settings::Instance().Save();
    Recalculate();
    LOG_INFO("Reminders resumed");
}

bool Scheduler::IsPaused() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pauseUntil == 0) return false;
    return time(nullptr) < m_pauseUntil;
}

ScheduleState Scheduler::GetState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

time_t Scheduler::GetNextReminderTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nextFire;
}

void Scheduler::OnSystemWake() {
    LOG_INFO("System wake detected — recalculating scheduler");
    Recalculate();
}

// ── Worker thread ─────────────────────────────────────────────────────────────
void Scheduler::WorkerThread() {
    HANDLE waitHandles[3] = { m_stopEvent, m_recalcEvent, m_timerHandle };

    while (m_running) {
        // Compute the next scheduled time
        time_t nextFire = ComputeNextReminderTime();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_nextFire = nextFire;
        }

        if (nextFire == 0) {
            // No reminder scheduled — wait for recalculation signal or stop
            DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0) break; // stop
            continue; // recalc
        }

        // Set the waitable timer to fire at nextFire
        LARGE_INTEGER dueTime;
        time_t now = time(nullptr);
        long long delay100ns = (long long)(nextFire - now) * 10000000LL;
        if (delay100ns <= 0) delay100ns = 1; // fire almost immediately
        dueTime.QuadPart = -delay100ns; // relative time (negative = relative to now)

        SetWaitableTimer(m_timerHandle, &dueTime, 0, nullptr, nullptr, FALSE);

        DWORD result = WaitForMultipleObjects(3, waitHandles, FALSE, INFINITE);

        if (result == WAIT_OBJECT_0) {
            break; // stop event
        } else if (result == WAIT_OBJECT_0 + 1) {
            continue; // recalc event — recompute and reset timer
        } else if (result == WAIT_OBJECT_0 + 2) {
            // Timer fired — verify we're still in schedule
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                // Check pause expired?
                if (m_pauseUntil != 0 && time(nullptr) >= m_pauseUntil) {
                    m_pauseUntil = 0;
                    auto& s = Settings::Instance().Get();
                    s.paused = false;
                    s.pauseUntilUnix = 0;
                    Settings::Instance().Save();
                    TrayIcon::Instance().SetPaused(false);
                }
            }

            // Final check before firing
            auto& s = Settings::Instance().Get();
            if (s.remindersEnabled && !IsPaused()) {
                if (m_hwnd) PostMessageW(m_hwnd, WM_REMINDER_TRIGGER, 0, 0);
            }
        }
    }
}

// ── Schedule calculation ─────────────────────────────────────────────────────
time_t Scheduler::ComputeNextReminderTime() const {
    auto s = Settings::Instance().GetCopy(); // thread-safe

    if (!s.remindersEnabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const_cast<Scheduler*>(this)->m_state = ScheduleState::Disabled;
        return 0;
    }

    // Check pause
    if (m_pauseUntil != 0) {
        time_t now = time(nullptr);
        if (now < m_pauseUntil) {
            std::lock_guard<std::mutex> lock(m_mutex);
            const_cast<Scheduler*>(this)->m_state = ScheduleState::Paused;
            return m_pauseUntil;
        }
    }

    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);

    time_t nextFireTime = 0;

    // Check from yesterday (overnight schedules) up to 7 days ahead
    for (int daysAhead = -1; daysAhead <= 7; ++daysAhead) {
        int searchDow = (lt.tm_wday + daysAhead) % 7;
        if (searchDow < 0) searchDow += 7;
        
        auto& ds = s.weeklySchedule[searchDow];
        if (!ds.enabled) continue;

        struct tm midnight = lt;
        midnight.tm_mday += daysAhead;
        midnight.tm_hour = 0; midnight.tm_min = 0; midnight.tm_sec = 0;
        mktime(&midnight); // Normalize

        int startMin = ds.startHour * 60 + ds.startMin;
        int endMin = ds.endHour * 60 + ds.endMin;
        int duration = (endMin > startMin) ? (endMin - startMin) : (24 * 60 - startMin + endMin);
        if (duration == 0) duration = 24 * 60; // 00:00 to 00:00 means 24h

        int freq = ds.frequencyMinutes;
        if (freq <= 0) freq = 30;

        for (int k = 0; ; ++k) {
            int t = startMin + k * freq;
            if (t >= startMin + duration) break; // Out of this day's window

            struct tm fireTm = midnight;
            fireTm.tm_min += t;
            time_t fireTime = mktime(&fireTm);

            if (fireTime > now) {
                struct tm fireLt;
                localtime_s(&fireLt, &fireTime);
                if (!IsQuietTime(s, fireLt)) {
                    nextFireTime = fireTime;
                    break;
                }
            }
        }
        if (nextFireTime != 0) break;
    }

    // Determine current state based on "now"
    ScheduleState currentState = ScheduleState::OutOfWindow;
    bool inWindow = false;
    for (int daysAhead = -1; daysAhead <= 0; ++daysAhead) {
        int searchDow = (lt.tm_wday + daysAhead) % 7;
        if (searchDow < 0) searchDow += 7;
        auto& ds = s.weeklySchedule[searchDow];
        if (!ds.enabled) continue;

        struct tm midnight = lt;
        midnight.tm_mday += daysAhead;
        midnight.tm_hour = 0; midnight.tm_min = 0; midnight.tm_sec = 0;
        mktime(&midnight);

        int startMin = ds.startHour * 60 + ds.startMin;
        int endMin = ds.endHour * 60 + ds.endMin;
        int duration = (endMin > startMin) ? (endMin - startMin) : (24 * 60 - startMin + endMin);
        if (duration == 0) duration = 24 * 60;
        
        struct tm startTm = midnight;
        startTm.tm_min += startMin;
        time_t absStart = mktime(&startTm);
        time_t absEnd = absStart + duration * 60;

        if (now >= absStart && now < absEnd) {
            inWindow = true;
            break;
        }
    }

    if (inWindow) {
        if (IsQuietTime(s, lt)) currentState = ScheduleState::QuietHours;
        else currentState = ScheduleState::Active;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const_cast<Scheduler*>(this)->m_state = currentState;
    }

    return nextFireTime;
}

bool Scheduler::IsQuietTime(const dhikr::AppSettings& s, const struct tm& lt) const {
    int nowMinutes = lt.tm_hour * 60 + lt.tm_min;
    for (auto& qp : s.quietPeriods) {
        int startMin = qp.startHour * 60 + qp.startMin;
        int endMin   = qp.endHour   * 60 + qp.endMin;
        if (startMin <= endMin) {
            if (nowMinutes >= startMin && nowMinutes < endMin) return true;
        } else {
            // Spans midnight
            if (nowMinutes >= startMin || nowMinutes < endMin) return true;
        }
    }
    return false;
}

} // namespace dhikr
