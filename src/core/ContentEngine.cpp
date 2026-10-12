#include "ContentEngine.h"
#include "Logger.h"
#include "Database.h"
#ifdef _WIN32
#include "../resource.h"
#endif
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <filesystem>

using json = nlohmann::json;

namespace dhikr {

ContentEngine& ContentEngine::Instance() {
    static ContentEngine instance;
    return instance;
}

static inline void SafeLocalTime(struct tm* tm, const time_t* time) {
#ifdef _WIN32
    localtime_s(tm, time);
#else
    localtime_r(time, tm);
#endif
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
#ifdef _WIN32
    // Strict UTF-8 conversion without null-terminator overrun
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), (int)str.size(), nullptr, 0);
    if (size > 0) {
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), (int)str.size(), result.data(), size);
        return result;
    }
    
    // Fallback to system ANSI code page
    size = MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), nullptr, 0);
    if (size > 0) {
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), result.data(), size);
        return result;
    }
    return L"";
#else
    std::wstring result;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            result.push_back((wchar_t)c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size()) {
            wchar_t wc = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
            result.push_back(wc);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size()) {
            wchar_t wc = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            result.push_back(wc);
            i += 3;
        } else {
            i += 1;
        }
    }
    return result;
#endif
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), result.data(), size, nullptr, nullptr);
        return result;
    }
    return "";
#else
    std::string result;
    for (wchar_t wc : wstr) {
        if (wc < 0x80) {
            result.push_back((char)wc);
        } else if (wc < 0x800) {
            result.push_back((char)(0xC0 | (wc >> 6)));
            result.push_back((char)(0x80 | (wc & 0x3F)));
        } else {
            result.push_back((char)(0xE0 | (wc >> 12)));
            result.push_back((char)(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back((char)(0x80 | (wc & 0x3F)));
        }
    }
    return result;
#endif
}

// ── Load from file ────────────────────────────────────────────────────────────
ContentLoadResult ContentEngine::Load(const std::filesystem::path& contentFilePath) {
    bool isMarkdown = (contentFilePath.extension() == ".md" || contentFilePath.extension() == ".MD");

    // Peek the first character to detect if a .md file is actually JSON
    if (isMarkdown) {
        std::ifstream peekF(contentFilePath, std::ios::binary);
        if (peekF.is_open()) {
            char bom[3] = {};
            peekF.read(bom, 3);
            if (!(bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')) {
                peekF.seekg(0);
            }
            char firstChar = 0;
            while (peekF.get(firstChar)) {
                if (!isspace(firstChar)) break;
            }
            if (firstChar == '[' || firstChar == '{') {
                isMarkdown = false;
            }
        }
    }

    if (isMarkdown) {
        return LoadFromMarkdownFile(contentFilePath);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastFilePath = contentFilePath;

    std::vector<ContentItem> parsed;
    std::wstring error;
    ContentLoadResult result = ParseAndValidate(contentFilePath, parsed, error);

    if (result == ContentLoadResult::Success) {
        m_items = std::move(parsed);
        m_lastError.clear();

        // Seed recent IDs from database history
        auto recentIds = Database::Instance().GetRecentContentIds(kHistoryWindow);
        m_recentIds.clear();
        for (auto& id : recentIds) m_recentIds.push_back(id);

        LOG_INFO("Content loaded: " + std::to_string(m_items.size()) + " items");
    } else {
        m_lastError = error;
        LOG_WARN(std::string("Content load failed: ") + WideToUtf8(m_lastError));
        // Keep previous content if any; do not replace with empty/invalid set
    }

    return result;
}

ContentLoadResult ContentEngine::LoadFromResource() {
#ifdef _WIN32
    return LoadFromMarkdownResource(nullptr, IDR_CONTENT_MD);
#else
    if (std::filesystem::exists("assets/content.md")) {
        return LoadFromMarkdownFile(std::filesystem::path("assets/content.md"));
    }
    if (std::filesystem::exists("content.md")) {
        return LoadFromMarkdownFile(std::filesystem::path("content.md"));
    }
    return ContentLoadResult::FileNotFound;
#endif
}

#ifdef _WIN32
ContentLoadResult ContentEngine::LoadFromMarkdownResource(HINSTANCE hInstance, int resourceId) {
    if (!hInstance) hInstance = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hRes) {
        m_lastError = L"Embedded content resource not found";
        return ContentLoadResult::FileNotFound;
    }
    HGLOBAL hData = LoadResource(hInstance, hRes);
    if (!hData) {
        m_lastError = L"Embedded content resource failed to load";
        return ContentLoadResult::FileNotFound;
    }
    const char* pBuf = reinterpret_cast<const char*>(LockResource(hData));
    DWORD sz = SizeofResource(hInstance, hRes);
    if (!pBuf || sz == 0) {
        m_lastError = L"Embedded content resource is empty";
        return ContentLoadResult::EmptyContent;
    }
    std::string mdText(pBuf, sz);
    return LoadFromMarkdownString(mdText);
}
#endif

ContentLoadResult ContentEngine::LoadFromMarkdownString(const std::string& markdownText) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::istringstream stream(markdownText);
    std::string line;
    std::vector<ContentItem> items;
    std::string currentCategory = "General";

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // Skip UTF-8 BOM if on first line
        if (items.empty() && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
            if (line.empty()) continue;
        }

        // Check for category (e.g. ## Section)
        if (line.size() >= 2 && line[0] == '#' && line[1] == '#') {
            size_t start = 2;
            while (start < line.size() && (line[start] == '#' || line[start] == ' ' || line[start] == '\t')) start++;
            if (start < line.size()) {
                currentCategory = line.substr(start);
                while (!currentCategory.empty() && (currentCategory.back() == ' ' || currentCategory.back() == '\t' || currentCategory.back() == '\r')) {
                    currentCategory.pop_back();
                }
            }
            continue;
        }

        // Check for list item (e.g. 1. text or - text or * text)
        size_t textStart = std::string::npos;
        std::string idPrefix = "";

        size_t dotPos = line.find(". ");
        if (dotPos != std::string::npos && dotPos > 0) {
            bool isNum = true;
            for (size_t i = 0; i < dotPos; ++i) {
                if (!isdigit(line[i])) { isNum = false; break; }
            }
            if (isNum) {
                textStart = dotPos + 2;
                idPrefix = line.substr(0, dotPos);
            }
        }

        if (textStart == std::string::npos && line.size() > 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            textStart = 2;
        }

        if (textStart != std::string::npos) {
            ContentItem item;
            item.id = currentCategory + "_" + std::to_string(items.size() + 1);
            item.text = Utf8ToWide(line.substr(textStart));
            item.category = currentCategory;
            item.priority = 1;
            item.enabled = true;
            if (!item.text.empty()) items.push_back(std::move(item));
        }
    }

    if (items.empty()) {
        m_lastError = L"No valid content found in markdown data";
        return ContentLoadResult::EmptyContent;
    }

    m_items = std::move(items);
    m_lastError.clear();

    auto recentIds = Database::Instance().GetRecentContentIds(kHistoryWindow);
    m_recentIds.clear();
    for (auto& id : recentIds) m_recentIds.push_back(id);

    LOG_INFO("Content loaded from markdown memory stream: " + std::to_string(m_items.size()) + " items");
    return ContentLoadResult::Success;
}

ContentLoadResult ContentEngine::LoadFromMarkdownFile(const std::filesystem::path& filePath) {
    m_lastFilePath = filePath;
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        m_lastError = L"Content file not found";
        return ContentLoadResult::FileNotFound;
    }
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return LoadFromMarkdownString(str);
}

ContentLoadResult ContentEngine::Reload() {
    if (m_lastFilePath.empty()) return LoadFromResource();
    return Load(m_lastFilePath);
}

void ContentEngine::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Always save to the dedicated AppData path if set; otherwise fall back to source path
    const auto& saveTo = m_savePath.empty() ? m_lastFilePath : m_savePath;
    if (saveTo.empty()) return;

    json j = json::array();
    for (const auto& item : m_items) {
        json obj;
        obj["id"] = item.id;
        obj["category"] = item.category;
        obj["text"] = WideToUtf8(item.text);
        if (!item.transliteration.empty()) obj["transliteration"] = WideToUtf8(item.transliteration);
        if (!item.description.empty()) obj["description"] = WideToUtf8(item.description);
        if (!item.source.empty()) obj["source"] = WideToUtf8(item.source);
        if (item.priority != 1) obj["priority"] = item.priority;
        if (item.repetitions > 1) obj["repetitions"] = item.repetitions;
        if (!item.enabled) obj["enabled"] = false;
        j.push_back(obj);
    }

    // Write to temp then rename atomically
    auto tmpPath = saveTo;
    tmpPath += ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out.is_open()) return;
        std::string jsonStr = j.dump(4);
        out << jsonStr;
        out.flush();
        if (out.fail()) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, saveTo, ec);
    if (ec) {
        std::filesystem::copy_file(tmpPath, saveTo, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath, ec);
    }
}

void ContentEngine::AddItem(const ContentItem& item) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.insert(m_items.begin(), item);
    }
    Save();
}

const ContentItem* ContentEngine::GetNextPreview() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_items.empty()) return nullptr;
    if (m_current && m_current->enabled) return m_current;
    for (const auto& it : m_items) {
        if (it.enabled) return &it;
    }
    return &m_items[0];
}

ContentLoadResult ContentEngine::ParseAndValidate(const std::filesystem::path& filePath,
                                                   std::vector<ContentItem>& out,
                                                   std::wstring& errorOut) {
    // Open file as UTF-8
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        errorOut = L"Content file not found";
        return ContentLoadResult::FileNotFound;
    }

    // Skip UTF-8 BOM if present
    char bom[3] = {};
    f.read(bom, 3);
    if (!(bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')) {
        f.seekg(0);
    }

    json root;
    try {
        f >> root;
    } catch (const json::exception& e) {
        errorOut = Utf8ToWide(std::string("JSON parse error: ") + e.what());
        return ContentLoadResult::ParseError;
    }

    // Validate structure (support both {"items": [...]} and top-level [...])
    json itemsArray;
    if (root.is_array()) {
        itemsArray = root;
    } else if (root.is_object() && root.contains("items") && root["items"].is_array()) {
        itemsArray = root["items"];
    } else {
        errorOut = L"Invalid schema: missing 'items' array";
        return ContentLoadResult::InvalidSchema;
    }

    std::set<std::string> seenIds;
    std::vector<ContentItem> items;

    for (auto& entry : itemsArray) {
        if (!entry.is_object()) continue;

        // Extract text (supporting standard 'text' as well as third-party Arabic DB formats)
        std::string rawText;
        if (entry.contains("text") && entry["text"].is_string()) rawText = entry["text"].get<std::string>();
        else if (entry.contains("content") && entry["content"].is_string()) rawText = entry["content"].get<std::string>();
        else if (entry.contains("body") && entry["body"].is_string()) rawText = entry["body"].get<std::string>();
        else if (entry.contains("arabic") && entry["arabic"].is_string()) rawText = entry["arabic"].get<std::string>();
        else if (entry.contains("dua") && entry["dua"].is_string()) rawText = entry["dua"].get<std::string>();

        if (rawText.empty()) continue;

        ContentItem item;
        item.text = Utf8ToWide(rawText);
        if (item.text.empty()) continue;

        // Extract ID
        if (entry.contains("id") && entry["id"].is_string()) {
            item.id = entry["id"].get<std::string>();
        } else if (entry.contains("id") && entry["id"].is_number_integer()) {
            item.id = std::to_string(entry["id"].get<int>());
        } else {
            item.id = "item_" + std::to_string(seenIds.size() + 1);
        }

        // Skip duplicate IDs
        if (seenIds.count(item.id)) continue;
        seenIds.insert(item.id);

        // Optional fields (with third-party aliases)
        if (entry.contains("category") && entry["category"].is_string())
            item.category = entry["category"].get<std::string>();
        else if (entry.contains("title") && entry["title"].is_string())
            item.category = entry["title"].get<std::string>();
        else if (entry.contains("section") && entry["section"].is_string())
            item.category = entry["section"].get<std::string>();
        else if (entry.contains("chapter") && entry["chapter"].is_string())
            item.category = entry["chapter"].get<std::string>();

        if (entry.contains("transliteration") && entry["transliteration"].is_string())
            item.transliteration = Utf8ToWide(entry["transliteration"].get<std::string>());
        else if (entry.contains("translation") && entry["translation"].is_string())
            item.transliteration = Utf8ToWide(entry["translation"].get<std::string>());

        if (entry.contains("description") && entry["description"].is_string())
            item.description = Utf8ToWide(entry["description"].get<std::string>());
        else if (entry.contains("fadl") && entry["fadl"].is_string())
            item.description = Utf8ToWide(entry["fadl"].get<std::string>());
        else if (entry.contains("meaning") && entry["meaning"].is_string())
            item.description = Utf8ToWide(entry["meaning"].get<std::string>());

        if (entry.contains("source") && entry["source"].is_string())
            item.source = Utf8ToWide(entry["source"].get<std::string>());
        else if (entry.contains("reference") && entry["reference"].is_string())
            item.source = Utf8ToWide(entry["reference"].get<std::string>());
        else if (entry.contains("hadith") && entry["hadith"].is_string())
            item.source = Utf8ToWide(entry["hadith"].get<std::string>());
        else if (entry.contains("surah") && entry["surah"].is_string())
            item.source = Utf8ToWide(entry["surah"].get<std::string>());

        if (entry.contains("priority") && entry["priority"].is_number_integer())
            item.priority = std::clamp(entry["priority"].get<int>(), 1, 5);

        // Recitation repetition count (1, 3, 33, 100)
        if (entry.contains("repetitions") && entry["repetitions"].is_number_integer())
            item.repetitions = std::max(1, entry["repetitions"].get<int>());
        else if (entry.contains("repeat") && entry["repeat"].is_number_integer())
            item.repetitions = std::max(1, entry["repeat"].get<int>());
        else if (entry.contains("count") && entry["count"].is_number_integer())
            item.repetitions = std::max(1, entry["count"].get<int>());

        if (entry.contains("enabled") && entry["enabled"].is_boolean())
            item.enabled = entry["enabled"].get<bool>();

        if (item.enabled) {
            items.push_back(std::move(item));
        }
    }

    if (items.empty()) {
        errorOut = L"No valid enabled content items found";
        return ContentLoadResult::EmptyContent;
    }

    out = std::move(items);
    return ContentLoadResult::Success;
}

// ── Weighted random selection ─────────────────────────────────────────────────
double ContentEngine::ComputeWeight(const ContentItem& item,
                                     size_t recentlyShownIndex) const {
    // Base weight: clamp priority between 1 and 5 (priority 1 = weight 5.0, priority 5 = weight 1.0)
    int p = std::clamp(item.priority, 1, 5);
    double weight = 6.0 - p;

    // ── Time-of-Day Context Awareness ──────────────────────────────────────────
    time_t nowTime = time(nullptr);
    struct tm lt{};
    SafeLocalTime(&lt, &nowTime);
    int hour = lt.tm_hour; // 0..23

    auto containsKeyword = [&](const std::wstring& kw) {
        return item.text.find(kw) != std::wstring::npos ||
               item.description.find(kw) != std::wstring::npos;
    };
    auto containsCategory = [&](const std::string& catKw) {
        return item.category.find(catKw) != std::string::npos;
    };

    // 1. Morning (الصباح / أذكار النهار): 04:00 - 11:59 (Fajr to Dhuhr)
    if (hour >= 4 && hour < 12) {
        if (containsKeyword(L"أصبحنا") || containsKeyword(L"صباح") || containsCategory("صباح") || containsCategory("morning")) {
            weight *= 5.0; // Strongly prioritize morning adhkar
        } else if (containsKeyword(L"أمسينا") || containsCategory("مساء") || containsCategory("evening") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress evening and sleep adhkar in the morning
        } else {
            weight *= 0.9; // General daily content can appear
        }
    }
    // 2. Midday / Afternoon (الكونتنت العادي لباقي اليوم): 12:00 - 16:59
    else if (hour >= 12 && hour < 17) {
        if (containsCategory("morning") || containsKeyword(L"أصبحنا") || containsCategory("evening") || containsKeyword(L"أمسينا") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress morning, evening, and sleep adhkar during midday
        } else {
            weight *= 3.5; // Strongly prioritize general daily content for rest of the day
        }
    }
    // 3. Evening (المساء / أذكار المساء): 17:00 - 21:59
    else if (hour >= 17 && hour < 22) {
        if (containsKeyword(L"أمسينا") || containsKeyword(L"مساء") || containsCategory("مساء") || containsCategory("evening")) {
            weight *= 5.0; // Strongly prioritize evening adhkar
        } else if (containsKeyword(L"أصبحنا") || containsCategory("صباح") || containsCategory("morning") || containsCategory("sleep")) {
            weight *= 0.001; // Suppress morning and sleep adhkar in the evening
        } else {
            weight *= 0.9; // General daily content can appear
        }
    }
    // 4. Night / Sleep / Early Dawn (الليل والنوم): 22:00 - 03:59
    else {
        if (containsCategory("sleep") || containsCategory("night_prayer") ||
            containsKeyword(L"أموت وأحيا") || containsKeyword(L"نوم") || containsKeyword(L"قيام")) {
            weight *= 4.5; // Prioritize sleep and night adhkar
        } else if (containsCategory("morning") || containsCategory("evening")) {
            weight *= 0.001; // Suppress morning and evening adhkar at night
        } else {
            weight *= 1.0; // General daily dhikr can appear
        }
    }

    // Reduce weight heavily if recently shown
    // Items closer to the front of m_recentIds (= more recently shown) are penalized more
    for (size_t i = 0; i < m_recentIds.size(); ++i) {
        if (m_recentIds[i] == item.id) {
            // Exponential decay based on position (0 = most recent)
            double recency = (double)(m_recentIds.size() - i) / (double)m_recentIds.size();
            weight *= (1.0 - 0.95 * recency); // most recent → down to 5% weight
            break;
        }
    }

    return std::max(weight, 0.01); // never zero
}

const ContentItem* ContentEngine::GetNext() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_items.empty()) return nullptr;

    // Build weights
    std::vector<double> weights(m_items.size());
    for (size_t i = 0; i < m_items.size(); ++i) {
        weights[i] = ComputeWeight(m_items[i], i);
    }

    // Weighted random selection
    double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> dist(0.0, total);
    double pick = dist(rng);

    size_t selected = 0;
    double cumulative = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (pick <= cumulative) {
            selected = i;
            break;
        }
    }

    m_current = &m_items[selected];

    // Update history
    m_recentIds.push_front(m_current->id);
    while (m_recentIds.size() > kHistoryWindow) m_recentIds.pop_back();

    return m_current;
}

const ContentItem* ContentEngine::GetCurrent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current;
}

std::vector<std::string> ContentEngine::GetCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::set<std::string> cats;
    for (auto& item : m_items) {
        if (!item.category.empty()) cats.insert(item.category);
    }
    return std::vector<std::string>(cats.begin(), cats.end());
}

} // namespace dhikr
