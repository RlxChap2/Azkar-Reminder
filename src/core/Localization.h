#pragma once
#include <string>
#include <functional>
#include <vector>
#include <mutex>

namespace dhikr {

class Localization {
public:
    static Localization& Instance();

    // Get a localized string by its key
    std::wstring Get(const std::wstring& key, const std::wstring& fallback = L"");

    // Change the language ("en" or "ar")
    void SetLanguage(const std::wstring& langCode);

    // Get the current language code
    std::wstring GetLanguage() const;

    // Check if the current language uses right-to-left layout
    bool IsRTL() const;

    // Callbacks for when the language changes
    using ChangeCallback = std::function<void()>;
    void OnLanguageChanged(ChangeCallback cb);

    Localization(const Localization&) = delete;
    Localization& operator=(const Localization&) = delete;

private:
    Localization();
    
    std::wstring m_currentLang;
    bool m_isRtl;
    mutable std::mutex m_mutex;
    std::vector<ChangeCallback> m_changeCallbacks;

    void NotifyChanged();
};

} // namespace dhikr
