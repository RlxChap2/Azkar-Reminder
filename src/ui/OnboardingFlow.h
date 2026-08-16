#pragma once
#include <windows.h>

namespace dhikr {

// ── OnboardingFlow ────────────────────────────────────────────────────────────
// Shown only on first launch.
// Simple native dialog: 3 pages — Welcome, Content File, Schedule.
// No fancy UI — keeps it minimal and non-intrusive.
class OnboardingFlow {
public:
    // Returns when the user closes the wizard
    void Show(HINSTANCE hInstance);

    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    int m_page = 0; // 0=Welcome, 1=Content, 2=Schedule, 3=Done
    HINSTANCE m_hInst = nullptr;

    void DrawPage(HWND hwnd, int page);
    void OnNext(HWND hwnd);
    void OnBack(HWND hwnd);
    void OnBrowseContent(HWND hwnd);
};

} // namespace dhikr
