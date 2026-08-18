#pragma once
#include <windows.h>

namespace dhikr {

// ── Updater ──────────────────────────────────────────────────────────────────
// Opens the GitHub Releases page in the default web browser.
// All automatic background checking and WinHTTP usage has been removed 
// to prevent static ML heuristics (e.g. RDPWrap.gen).
class Updater {
public:
    static Updater& Instance();

    // Opens the releases page in the user's default browser
    void CheckNow(HWND hwnd);

    // No-op, kept for API compatibility
    void Shutdown() {}

    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

private:
    Updater() = default;
    ~Updater() = default;
};

} // namespace dhikr
