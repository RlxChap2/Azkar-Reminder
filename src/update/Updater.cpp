#include "Updater.h"
#include <shellapi.h>

namespace dhikr {

Updater& Updater::Instance() {
    static Updater instance;
    return instance;
}

void Updater::CheckNow(HWND hwnd) {
    // Open the latest releases page in the default browser.
    ShellExecuteW(hwnd, L"open", L"https://github.com/ahmedkamel6/Azkar-Reminder/releases/latest", nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace dhikr
