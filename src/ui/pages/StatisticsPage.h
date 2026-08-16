#pragma once
#include <string>
#include "../MainWindow.h"
#include "../../core/Database.h"
#include <vector>
namespace dhikr {
class StatisticsPage : public PageBase {
public:
    StatisticsPage();
    ~StatisticsPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    void OnSettingsChanged() override;
private:
    void DrawStatCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* label, const std::wstring& val);
    void DrawSimpleLineChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawTopDhikrs(ID2D1RenderTarget* rt, float x, float y, float w);

    IDWriteFactory*    m_dwf    = nullptr;
    IDWriteTextFormat* m_tfTitle  = nullptr;
    IDWriteTextFormat* m_tfLabel  = nullptr;
    IDWriteTextFormat* m_tfValue  = nullptr;
    IDWriteTextFormat* m_tfBig    = nullptr;
    IDWriteTextFormat* m_tfArabic = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;
    int m_total = -1;
    int m_week = -1;
    int m_streak = -1;
    std::vector<dhikr::DailyStat> m_dailyStats;
};
} // namespace dhikr
