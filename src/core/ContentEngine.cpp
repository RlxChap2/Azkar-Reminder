#include "ContentEngine.h"
#include "Logger.h"
#include "Database.h"
#include "../ui/core/ResourceLoader.h"
#include "../resource.h"
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <set>

using json = nlohmann::json;

namespace dhikr {

ContentEngine& ContentEngine::Instance() {
    static ContentEngine instance;
    return instance;
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    
    // Attempt strict UTF-8 conversion
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
    if (size > 0) {
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, result.data(), size);
        return result;
    }
    
    // Fallback to system ANSI code page (e.g., Windows-1256 for Arabic)
    size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, result.data(), size);
    return result;
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

// ── Load from file ────────────────────────────────────────────────────────────
ContentLoadResult ContentEngine::Load(const std::wstring& contentFilePath) {
    bool isMarkdown = false;
    if (contentFilePath.size() >= 3) {
        std::wstring ext = contentFilePath.substr(contentFilePath.size() - 3);
        if (ext == L".md" || ext == L".MD") {
            isMarkdown = true;
        }
    }

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
        char errBuf[512] = {0};
        WideCharToMultiByte(CP_UTF8, 0, m_lastError.c_str(), -1, errBuf, sizeof(errBuf), nullptr, nullptr);
        LOG_WARN(std::string("Content load failed: ") + errBuf);
        // Keep previous content if any; do not replace with empty/invalid set
    }

    return result;
}

ContentLoadResult ContentEngine::LoadFromResource() {
    return LoadFromMarkdownResource(nullptr, IDR_CONTENT_MD);
}

ContentLoadResult ContentEngine::LoadFromMarkdownResource(HINSTANCE hInstance, int resourceId) {
    std::string mdText = ResourceLoader::LoadStringFromResource(hInstance, resourceId);
    if (mdText.empty()) {
        m_lastError = L"Embedded content resource not found or empty";
        return ContentLoadResult::FileNotFound;
    }
    return LoadFromMarkdownString(mdText);
}

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
            while (start < line.size() && (line[start] == '#' || line[start] == ' ')) start++;
            if (start < line.size()) {
                currentCategory = line.substr(start);
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
            if (!idPrefix.empty()) {
                item.id = idPrefix + "_" + std::to_string(items.size());
            } else {
                item.id = "md_" + std::to_string(items.size());
            }
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

ContentLoadResult ContentEngine::LoadFromMarkdownFile(const std::wstring& filePath) {
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
    const std::wstring& saveTo = m_savePath.empty() ? m_lastFilePath : m_savePath;
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
        if (!item.enabled) obj["enabled"] = false;
        j.push_back(obj);
    }

    // Write to temp then rename atomically
    std::wstring tmpPath = saveTo + L".tmp";
    {
        std::ofstream out(tmpPath);
        if (!out.is_open()) return;
        std::string jsonStr = j.dump(4);
        out << jsonStr;
        out.flush();
        if (out.fail()) { DeleteFileW(tmpPath.c_str()); return; }
    }
    MoveFileExW(tmpPath.c_str(), saveTo.c_str(), MOVEFILE_REPLACE_EXISTING);
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



ContentLoadResult ContentEngine::ParseAndValidate(const std::wstring& filePath,
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
        // Required fields
        if (!entry.contains("id") || !entry.contains("text")) continue;
        if (!entry["id"].is_string() || !entry["text"].is_string()) continue;

        ContentItem item;
        item.id   = entry["id"].get<std::string>();
        item.text = Utf8ToWide(entry["text"].get<std::string>());

        // Skip empty text
        if (item.text.empty()) continue;

        // Skip duplicate IDs
        if (seenIds.count(item.id)) continue;
        seenIds.insert(item.id);

        // Optional fields
        if (entry.contains("category") && entry["category"].is_string())
            item.category = entry["category"].get<std::string>();

        if (entry.contains("transliteration") && entry["transliteration"].is_string())
            item.transliteration = Utf8ToWide(entry["transliteration"].get<std::string>());

        if (entry.contains("description") && entry["description"].is_string())
            item.description = Utf8ToWide(entry["description"].get<std::string>());

        if (entry.contains("source") && entry["source"].is_string())
            item.source = Utf8ToWide(entry["source"].get<std::string>());

        if (entry.contains("priority") && entry["priority"].is_number_integer())
            item.priority = std::clamp(entry["priority"].get<int>(), 1, 5);

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
    // Base weight: inverse of priority (priority 1 = weight 5, priority 5 = weight 1)
    double weight = 6.0 - item.priority;

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
