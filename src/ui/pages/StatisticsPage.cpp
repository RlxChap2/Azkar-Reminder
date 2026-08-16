#include "StatisticsPage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Database.h"
#include "../../core/ContentEngine.h"
#include "../../core/Localization.h"
#include <algorithm>
#include <sstream>

namespace dhikr {

StatisticsPage::StatisticsPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, const wchar_t* font, const wchar_t* locale, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(font, nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, locale, out);
    };
    mk(ds::FontSize::PageTitle, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Noto Sans", L"en-US", &m_tfTitle);
    mk(ds::FontSize::CardTitle, DWRITE_FONT_WEIGHT_REGULAR,   L"Noto Sans", L"en-US", &m_tfLabel);
    mk(ds::FontSize::MedNumber, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Noto Sans", L"en-US", &m_tfValue);
    mk(ds::FontSize::BigNumber, DWRITE_FONT_WEIGHT_BOLD,      L"Noto Sans", L"en-US", &m_tfBig);
    mk(ds::FontSize::Body,      DWRITE_FONT_WEIGHT_REGULAR,   L"Noto Sans Arabic", L"ar-SA", &m_tfArabic);
}

StatisticsPage::~StatisticsPage() {
    if (m_tfTitle)  { m_tfTitle->Release();  m_tfTitle  = nullptr; }
    if (m_tfLabel)  { m_tfLabel->Release();  m_tfLabel  = nullptr; }
    if (m_tfValue)  { m_tfValue->Release();  m_tfValue  = nullptr; }
    if (m_tfBig)    { m_tfBig->Release();    m_tfBig    = nullptr; }
    if (m_tfArabic) { m_tfArabic->Release(); m_tfArabic = nullptr; }
    if (m_brush)    { m_brush->Release();    m_brush    = nullptr; }
}

void StatisticsPage::DrawStatCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r,
                                   const wchar_t* label, const std::wstring& val) {
    float rx = ds::Dims::CardRadius * m_dpiScale;
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);

    float pad = ds::Dims::CardPadding * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Top accent bar
    m_brush->SetColor(ds::Accent::Primary(0.85f));
    float ax = isRtl ? (r.right - 3.f * m_dpiScale) : r.left;
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ax, r.top, ax + 3.f * m_dpiScale, r.top + 24.f * m_dpiScale), 2, 2), m_brush);

    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextSecondary());
        rt->DrawTextW(label, (UINT32)wcslen(label), m_tfLabel,
            D2D1::RectF(r.left + pad, r.top + pad - 2.f*m_dpiScale, r.right - pad, r.top + pad + 16.f*m_dpiScale), m_brush);
    }
    if (m_tfBig) {
        m_tfBig->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextPrimary());
        rt->DrawTextW(val.c_str(), (UINT32)val.size(), m_tfBig,
            D2D1::RectF(r.left + pad, r.top + pad + 18.f * m_dpiScale, r.right - pad, r.bottom - 6.f*m_dpiScale), m_brush);
    }
}

void StatisticsPage::DrawSimpleLineChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    auto stats = m_dailyStats;
    if (stats.empty()) return;

    std::vector<int> vals;
    for (auto& s : stats) vals.push_back(s.delivered);
    while ((int)vals.size() < 14) vals.push_back(0);
    if (vals.size() > 14) vals.resize(14);
    std::reverse(vals.begin(), vals.end());

    int maxVal = 1;
    for (auto v : vals) maxVal = std::max(maxVal, v);

    float w = r.right - r.left;
    float h = r.bottom - r.top;
    float stepX = w / (float)(vals.size() - 1);
    bool isRtl = Localization::Instance().IsRTL();

    // Subtle horizontal grid lines
    m_brush->SetColor(ds::BorderSubtle());
    for (int i = 0; i < 4; ++i) {
        float gy = r.top + h * (i / 3.f);
        rt->DrawLine(D2D1::Point2F(r.left, gy), D2D1::Point2F(r.right, gy), m_brush, 0.5f);
    }

    // Sparkline Path
    ID2D1PathGeometry* path = nullptr;
    Application::Instance().D2DFactory()->CreatePathGeometry(&path);
    if (path) {
        ID2D1GeometrySink* sink = nullptr;
        path->Open(&sink);
        if (sink) {
            bool first = true;
            for (size_t i = 0; i < vals.size(); ++i) {
                float px = isRtl ? (r.right - stepX * i) : (r.left + stepX * i);
                float py = r.bottom - 4.f * m_dpiScale - ((float)vals[i] / (float)maxVal) * (h - 12.f * m_dpiScale);
                if (first) { sink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW); first = false; }
                else sink->AddLine(D2D1::Point2F(px, py));
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close(); sink->Release();
        }
        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawGeometry(path, m_brush, 2.0f);

        // Data point glowing circles
        for (size_t i = 0; i < vals.size(); ++i) {
            float px = isRtl ? (r.right - stepX * i) : (r.left + stepX * i);
            float py = r.bottom - 4.f * m_dpiScale - ((float)vals[i] / (float)maxVal) * (h - 12.f * m_dpiScale);
            
            // Outer glow
            m_brush->SetColor(ds::Accent::Primary(0.25f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 5.5f * m_dpiScale, 5.5f * m_dpiScale), m_brush);
            // Center dot
            m_brush->SetColor(ds::Accent::Primary());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 2.5f * m_dpiScale, 2.5f * m_dpiScale), m_brush);
        }
        path->Release();
    }
}

void StatisticsPage::DrawTopDhikrs(ID2D1RenderTarget* rt, float x, float y, float w) {
    auto& items = ContentEngine::Instance().GetAllItems();
    if (items.empty()) return;

    bool isRtl = Localization::Instance().IsRTL();

    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextSecondary());
        std::wstring sectionTitle = Localization::Instance().Get(L"stats.top_dhikrs");
        rt->DrawTextW(sectionTitle.c_str(), (UINT32)sectionTitle.size(), m_tfLabel,
            D2D1::RectF(x, y, x + w, y + 20.f * m_dpiScale), m_brush);
    }
    y += 24.f * m_dpiScale;

    size_t count = std::min((size_t)3, items.size());
    float itemH = 46.f * m_dpiScale;
    float rx = ds::Dims::CardRadius * m_dpiScale;

    for (size_t i = 0; i < count; ++i) {
        auto& item = items[i];
        D2D1_RECT_F ir = D2D1::RectF(x, y, x + w, y + itemH);

        m_brush->SetColor(ds::Card());
        rt->FillRoundedRectangle(D2D1::RoundedRect(ir, rx, rx), m_brush);
        m_brush->SetColor(ds::BorderSubtle());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(ir, rx, rx), m_brush, 0.5f);

        // Number badge
        std::wstring rankStr = L"#" + std::to_wstring(i + 1);
        float numW = 28.f * m_dpiScale;
        float nx = isRtl ? (x + w - numW - 12.f * m_dpiScale) : (x + 12.f * m_dpiScale);
        if (m_tfLabel) {
            m_brush->SetColor(ds::Accent::Primary());
            rt->DrawTextW(rankStr.c_str(), (UINT32)rankStr.size(), m_tfLabel,
                D2D1::RectF(nx, y + 14.f*m_dpiScale, nx + numW, y + itemH), m_brush);
        }

        // Dhikr text
        if (m_tfArabic) {
            m_brush->SetColor(ds::TextPrimary());
            m_tfArabic->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
            D2D1_RECT_F tr = isRtl
                ? D2D1::RectF(x + 12.f * m_dpiScale, y + 12.f * m_dpiScale, x + w - numW - 20.f * m_dpiScale, y + itemH)
                : D2D1::RectF(x + numW + 20.f * m_dpiScale, y + 12.f * m_dpiScale, x + w - 12.f * m_dpiScale, y + itemH);
            rt->DrawTextW(item.text.c_str(), (UINT32)std::min((size_t)80, item.text.size()), m_tfArabic, tr, m_brush);
        }

        y += itemH + 8.f * m_dpiScale;
    }
}

void StatisticsPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &m_brush);

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;
    bool isRtl = Localization::Instance().IsRTL();

    if (m_tfTitle) {
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextPrimary());
        std::wstring title = Localization::Instance().Get(L"statistics.title");
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, D2D1::RectF(x, y, x + w, y + 28.f * m_dpiScale), m_brush);
    }
    y += 40.f * m_dpiScale;

    // Stat cards (2×2)
    float cardW = (w - ds::Space::M * m_dpiScale) / 2.f;
    float cardH = 80.f * m_dpiScale;
    float gap   = ds::Space::M * m_dpiScale;

    if (m_total == -1) {
        m_total = Database::Instance().GetTotalCount();
        m_week = Database::Instance().GetWeekCount();
        m_streak = Database::Instance().GetCurrentStreakDays();
        m_dailyStats = Database::Instance().GetDailyStats(30);
    }

    int today  = m_dailyStats.empty() ? 0 : m_dailyStats[0].delivered;
    int week   = m_week;
    int total  = m_total;
    int streak = m_streak;

    std::wstring t_today  = Localization::Instance().Get(L"statistics.today");
    std::wstring t_week   = Localization::Instance().Get(L"statistics.this_week");
    std::wstring t_total  = Localization::Instance().Get(L"statistics.total");
    std::wstring t_streak = Localization::Instance().Get(L"statistics.streak");

    float c1X = isRtl ? (x + cardW + gap) : x;
    float c2X = isRtl ? x : (x + cardW + gap);

    DrawStatCard(rt, D2D1::RectF(c1X, y, c1X + cardW, y + cardH),
        t_today.c_str(), std::to_wstring(today));
    DrawStatCard(rt, D2D1::RectF(c2X, y, c2X + cardW, y + cardH),
        t_week.c_str(), std::to_wstring(week));

    y += cardH + gap;

    DrawStatCard(rt, D2D1::RectF(c1X, y, c1X + cardW, y + cardH),
        t_total.c_str(), std::to_wstring(total));
    DrawStatCard(rt, D2D1::RectF(c2X, y, c2X + cardW, y + cardH),
        t_streak.c_str(), std::to_wstring(streak));

    y += cardH + ds::Space::L * m_dpiScale;

    // Chart area
    float chartH = 100.f * m_dpiScale;
    float rx = ds::Dims::CardRadius * m_dpiScale;
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + chartH + 34.f * m_dpiScale), rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + chartH + 34.f * m_dpiScale), rx, rx), m_brush, 0.5f);

    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextSecondary());
        std::wstring chartTitle = Localization::Instance().Get(L"stats.daily_chart");
        rt->DrawTextW(chartTitle.c_str(), (UINT32)chartTitle.size(), m_tfLabel,
            D2D1::RectF(x + 16.f * m_dpiScale, y + 10.f * m_dpiScale, x + w - 16.f * m_dpiScale, y + 26.f * m_dpiScale), m_brush);
    }

    DrawSimpleLineChart(rt, D2D1::RectF(x + 16.f * m_dpiScale, y + 30.f * m_dpiScale,
        x + w - 16.f * m_dpiScale, y + 30.f * m_dpiScale + chartH));

    y += chartH + 34.f * m_dpiScale + ds::Space::M * m_dpiScale;

    // Top dhikrs list
    DrawTopDhikrs(rt, x, y, w);
}

void StatisticsPage::OnSettingsChanged() {
    m_total = -1;
    m_week = -1;
    m_streak = -1;
    m_dailyStats.clear();
}

} // namespace dhikr
