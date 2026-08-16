#pragma once
#include <windows.h>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <ctime>
#include "Settings.h"

namespace dhikr {

// ── Schedule state ────────────────────────────────────────────────────────────
enum class ScheduleState {
    Active,       // Reminders are running normally
    Paused,       // User-initiated pause (with end time)
    OutOfWindow,  // Current time is outside the configured window
    QuietHours,   // Currently in a quiet period
    Disabled      // Reminders disabled entirely
};

// ── Scheduler ─────────────────────────────────────────────────────────────────
// Event-driven: uses a background thread that sleeps via WaitForSingleObject
// on a kernel waitable timer. No polling loop.
class Scheduler {
public:
    static Scheduler& Instance();

    // hwnd receives WM_REMINDER_TRIGGER when it's time for a reminder
    void Start(HWND hwnd);
    void Stop();

    // Recalculate next reminder immediately (call after settings change)
    void Recalculate();

    // Trigger a reminder right now (for testing / "Next Reminder" menu)
    void TriggerNow();

    // Pause control
    void Pause(int durationMinutes); // 0 = indefinite (until tomorrow)
    void PauseUntilTomorrow();
    void Resume();
    bool IsPaused() const;

    // Current state & next fire time
    ScheduleState GetState() const;
    time_t        GetNextReminderTime() const;

    // Called from main window on WM_POWERBROADCAST PBT_APMRESUMEAUTOMATIC
    void OnSystemWake();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

private:
    Scheduler() = default;
    ~Scheduler() { Stop(); }

    void WorkerThread();

    // Returns time_t of next reminder, or 0 if none scheduled in range
    time_t ComputeNextReminderTime() const;

    // Returns true if `lt` falls inside a quiet period defined in `s`
    bool IsQuietTime(const dhikr::AppSettings& s, const struct tm& lt) const;

    // Returns true if `t` is within today's scheduled window
    bool IsInDayWindow(const struct tm& lt) const;

    // Returns start of next window for a given day
    time_t NextWindowStart(const struct tm& lt, int daysAhead = 0) const;

    HWND            m_hwnd       = nullptr;
    HANDLE          m_timerHandle = INVALID_HANDLE_VALUE;
    HANDLE          m_stopEvent   = INVALID_HANDLE_VALUE;
    HANDLE          m_recalcEvent = INVALID_HANDLE_VALUE;
    std::thread     m_thread;
    std::atomic<bool> m_running   { false };

    mutable std::mutex m_mutex;
    time_t          m_nextFire    = 0;
    ScheduleState   m_state       = ScheduleState::Disabled;

    time_t          m_pauseUntil  = 0; // 0 = not paused
};

} // namespace dhikr
