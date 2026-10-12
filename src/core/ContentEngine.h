#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dhikr {

// ── Content item ─────────────────────────────────────────────────────────────
struct ContentItem {
    std::string  id;
    std::string  category;
    std::wstring text;            // Main reminder text (Arabic/English)
    std::wstring transliteration; // Optional romanization
    std::wstring description;     // Short description / meaning
    std::wstring source;          // Reference source
    int          priority = 1;    // 1 (high) to 5 (low)
    int          repetitions = 1; // Recitation count (1, 3, 33, 100)
    bool         enabled  = true;
};

// ── Content load result ───────────────────────────────────────────────────────
enum class ContentLoadResult {
    Success,
    FileNotFound,
    ParseError,
    InvalidSchema,
    EmptyContent
};

// ── Content engine ────────────────────────────────────────────────────────────
class ContentEngine {
public:
    static ContentEngine& Instance();

    // Load content from JSON file; validates before replacing current content
    ContentLoadResult Load(const std::filesystem::path& contentFilePath);

    // Load content from embedded markdown resource
    ContentLoadResult LoadFromResource();
#ifdef _WIN32
    ContentLoadResult LoadFromMarkdownResource(HINSTANCE hInstance, int resourceId);
#endif

    // Load content from markdown string in memory
    ContentLoadResult LoadFromMarkdownString(const std::string& markdownText);

    // Load content from external markdown file
    ContentLoadResult LoadFromMarkdownFile(const std::filesystem::path& filePath);

    // Reload from the same path used in Load()
    ContentLoadResult Reload();

    // Get next content item using weighted-random history-aware selection
    const ContentItem* GetNext();

    // Peek at current/last item without advancing
    const ContentItem* GetCurrent() const;

    // All categories present in the loaded content
    std::vector<std::string> GetCategories() const;

    // All items (for the library page)
    const std::vector<ContentItem>& GetAllItems() const { return m_items; }
    const std::vector<ContentItem>& GetItems() const { return m_items; }
    std::vector<ContentItem>& GetItemsMutable() { return m_items; }

    // Add a new custom item and persist to disk
    void AddItem(const ContentItem& item);

    // Delete item safely by index or by ID
    bool DeleteItem(size_t index);
    bool DeleteItemById(const std::string& id);

    // Toggle enabled state safely
    bool ToggleItemEnabled(size_t index);

    // Clear all items safely
    void ClearAll();

    // Peek at next scheduled item for display/preview
    const ContentItem* GetNextPreview() const;

    // Item count
    size_t Count() const { return m_items.size(); }
    bool   IsEmpty() const { return m_items.empty(); }

    void Save();

    // Set the canonical AppData save path (called once at startup)
    void SetSavePath(const std::filesystem::path& path) { m_savePath = path; }

    // Last load error description
    std::wstring LastError() const { return m_lastError; }

    ContentEngine(const ContentEngine&) = delete;
    ContentEngine& operator=(const ContentEngine&) = delete;

private:
    ContentEngine() = default;

    ContentLoadResult ParseAndValidate(const std::filesystem::path& filePath,
                                       std::vector<ContentItem>& out,
                                       std::wstring& errorOut);

    double ComputeWeight(const ContentItem& item, size_t recentlyShownIndex) const;

    mutable std::mutex         m_mutex;
    std::vector<ContentItem>   m_items;
    const ContentItem*         m_current = nullptr;
    std::deque<std::string>    m_recentIds; // last N shown IDs
    std::filesystem::path      m_lastFilePath;  // source path used for loading
    std::filesystem::path      m_savePath;      // AppData path to persist content
    std::wstring               m_lastError;

    static constexpr size_t kHistoryWindow = 20; // avoid repeating last 20 items
};

} // namespace dhikr
