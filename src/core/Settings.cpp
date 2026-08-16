#include "Settings.h"
#include "Logger.h"
#include "../third_party/json.hpp"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <objbase.h>

using json = nlohmann::json;

namespace dhikr {

// Helper: UTF-8 <-> UTF-16
static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
    return result;
}

Settings& Settings::Instance() {
    static Settings instance;
    return instance;
}

// ── Helpers: Color <-> hex string ────────────────────────────────────────────
static std::string ColorToHex(const Color& c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

static Color HexToColor(const std::string& hex) {
    if (hex.size() < 7) return { 0xB7, 0xF3, 0x4A };
    auto fromHex = [](char h, char l) -> uint8_t {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        return (uint8_t)(hexVal(h) * 16 + hexVal(l));
    };
    return { fromHex(hex[1],hex[2]), fromHex(hex[3],hex[4]), fromHex(hex[5],hex[6]) };
}

// ── UUID generator for anonymous installation ID ──────────────────────────────
static std::wstring GenerateUUID() {
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t buf[64];
    StringFromGUID2(guid, buf, 64);
    return buf;
}

bool Settings::Load(const std::wstring& appDataPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = appDataPath + L"\\settings.json";

    // Generate installation ID if needed
    if (m_settings.installationId.empty()) {
        m_settings.installationId = GenerateUUID();
    }

    std::ifstream f(m_filePath);
    if (!f.is_open()) {
        LOG_INFO("Settings file not found — using defaults");
        return true; // First launch, defaults are fine
    }

    try {
        json j;
        f >> j;
        auto& s = m_settings;

        // General
        if (j.contains("language"))        s.language = Utf8ToWide(j["language"].get<std::string>());
        if (j.contains("startWithWindows")) s.startWithWindows = j["startWithWindows"].get<bool>();
        if (j.contains("theme"))           s.theme = Utf8ToWide(j["theme"].get<std::string>());
        if (j.contains("accentColor"))     s.accentColor = HexToColor(j["accentColor"].get<std::string>());
        if (j.contains("installationId")) {
            s.installationId = Utf8ToWide(j["installationId"].get<std::string>());
        }

        // Reminders enabled
        if (j.contains("remindersEnabled")) s.remindersEnabled = j["remindersEnabled"].get<bool>();

        // Weekly schedule (7 days)
        if (j.contains("weeklySchedule") && j["weeklySchedule"].is_array()) {
            auto& arr = j["weeklySchedule"];
            for (size_t i = 0; i < std::min((size_t)7, arr.size()); ++i) {
                auto& d = arr[i];
                auto& ds = s.weeklySchedule[i];
                if (d.contains("enabled"))          ds.enabled = d["enabled"].get<bool>();
                if (d.contains("startHour"))        ds.startHour = d["startHour"].get<int>();
                if (d.contains("startMin"))         ds.startMin = d["startMin"].get<int>();
                if (d.contains("endHour"))          ds.endHour = d["endHour"].get<int>();
                if (d.contains("endMin"))           ds.endMin = d["endMin"].get<int>();
                if (d.contains("frequencyMinutes")) ds.frequencyMinutes = d["frequencyMinutes"].get<int>();
            }
        }

        // Quiet periods
        if (j.contains("quietPeriods") && j["quietPeriods"].is_array()) {
            s.quietPeriods.clear();
            for (auto& qp : j["quietPeriods"]) {
                QuietPeriod q;
                if (qp.contains("startHour")) q.startHour = qp["startHour"].get<int>();
                if (qp.contains("startMin"))  q.startMin  = qp["startMin"].get<int>();
                if (qp.contains("endHour"))   q.endHour   = qp["endHour"].get<int>();
                if (qp.contains("endMin"))    q.endMin    = qp["endMin"].get<int>();
                s.quietPeriods.push_back(q);
            }
        }

        // Popup
        if (j.contains("popupPosition"))    s.popupPosition = (PopupPosition)j["popupPosition"].get<int>();
        if (j.contains("popupMonitor"))     s.popupMonitor = (PopupMonitor)j["popupMonitor"].get<int>();
        if (j.contains("specificMonitor"))  s.specificMonitor = j["specificMonitor"].get<int>();
        if (j.contains("popupDurationSec")) s.popupDurationSec = j["popupDurationSec"].get<int>();
        if (j.contains("animationEnabled")) s.animationEnabled = j["animationEnabled"].get<bool>();

        // Sound
        if (j.contains("soundEnabled"))    s.soundEnabled = j["soundEnabled"].get<bool>();
        if (j.contains("soundVolume"))     s.soundVolume = std::clamp(j["soundVolume"].get<int>(), 0, 100);
        if (j.contains("customSoundPath")) s.customSoundPath = Utf8ToWide(j["customSoundPath"].get<std::string>());

        // Full-screen & Distraction Awareness
        if (j.contains("suppressDuringFullscreen"))  s.suppressDuringFullscreen  = j["suppressDuringFullscreen"].get<bool>();
        if (j.contains("fullscreenSuppressionMode")) s.fullscreenSuppressionMode = std::clamp(j["fullscreenSuppressionMode"].get<int>(), 0, 2);


        // Internal
        if (j.contains("firstLaunch"))   s.firstLaunch = j["firstLaunch"].get<bool>();
        if (j.contains("paused"))        s.paused = j["paused"].get<bool>();
        if (j.contains("pauseUntilUnix")) s.pauseUntilUnix = j["pauseUntilUnix"].get<int>();

        LOG_INFO("Settings loaded successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to parse settings: ") + e.what());
        return false;
    }
}

bool Settings::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filePath.empty()) return false;

    try {
        auto& s = m_settings;
        json j;

        // General
        j["language"]        = WideToUtf8(s.language);
        j["startWithWindows"] = s.startWithWindows;
        j["theme"]           = "dark";
        j["accentColor"]     = ColorToHex(s.accentColor);
        j["installationId"]  = WideToUtf8(s.installationId);

        // Reminders
        j["remindersEnabled"] = s.remindersEnabled;

        // Weekly schedule
        json schedArr = json::array();
        for (int i = 0; i < 7; ++i) {
            auto& ds = s.weeklySchedule[i];
            json d;
            d["enabled"]          = ds.enabled;
            d["startHour"]        = ds.startHour;
            d["startMin"]         = ds.startMin;
            d["endHour"]          = ds.endHour;
            d["endMin"]           = ds.endMin;
            d["frequencyMinutes"] = ds.frequencyMinutes;
            schedArr.push_back(d);
        }
        j["weeklySchedule"] = schedArr;

        // Quiet periods
        json qpArr = json::array();
        for (auto& qp : s.quietPeriods) {
            json q;
            q["startHour"] = qp.startHour;
            q["startMin"]  = qp.startMin;
            q["endHour"]   = qp.endHour;
            q["endMin"]    = qp.endMin;
            qpArr.push_back(q);
        }
        j["quietPeriods"] = qpArr;

        // Popup
        j["popupPosition"]    = (int)s.popupPosition;
        j["popupMonitor"]     = (int)s.popupMonitor;
        j["specificMonitor"]  = s.specificMonitor;
        j["popupDurationSec"] = s.popupDurationSec;
        j["animationEnabled"] = s.animationEnabled;

        // Sound
        j["soundEnabled"]    = s.soundEnabled;
        j["soundVolume"]     = s.soundVolume;
        j["customSoundPath"] = WideToUtf8(s.customSoundPath);

        // Full-screen & Distraction Awareness
        j["suppressDuringFullscreen"]  = s.suppressDuringFullscreen;
        j["fullscreenSuppressionMode"] = s.fullscreenSuppressionMode;


        // Internal
        j["firstLaunch"]    = s.firstLaunch;
        j["paused"]         = s.paused;
        j["pauseUntilUnix"] = s.pauseUntilUnix;

        // Atomic write: write to temp file then rename
        std::wstring tmpPath = m_filePath + L".tmp";
        {
            std::ofstream f(tmpPath);
            if (!f.is_open()) {
                LOG_ERROR("Cannot open settings temp file for writing");
                return false;
            }
            std::string jsonStr = j.dump(2);
            f << jsonStr;
            f.flush();
            if (f.fail()) {
                LOG_ERROR("Failed to write settings to temp file");
                DeleteFileW(tmpPath.c_str());
                return false;
            }
        }
        // Atomic replace
        if (!MoveFileExW(tmpPath.c_str(), m_filePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            LOG_ERROR("Failed to atomically replace settings file");
            DeleteFileW(tmpPath.c_str());
            return false;
        }

        LOG_INFO("Settings saved");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to save settings: ") + e.what());
        return false;
    }
}

void Settings::NotifyChanged() {
    for (auto& cb : m_changeCallbacks) {
        if (cb) cb();
    }
}

AppSettings Settings::GetCopy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings;
}

void Settings::Update(const AppSettings& newSettings) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings = newSettings;
    }
    NotifyChanged();
}

} // namespace dhikr
