#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include "../MainWindow.h"
#include "../controls/BasicControls.h"
#include "../../core/Database.h"

namespace dhikr {

// ── HomePage ─────────────────────────────────────────────────────────────────
// Displays: greeting, active status, 4 stat cards, today's progress timeline.
// Matches the design reference exactly.
class HomePage : public PageBase {
public:
    HomePage();
    ~HomePage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    void OnSettingsChanged() override;
    bool HitTestInteractive(int x, int y) override;

private:
    std::shared_ptr<ToggleControl> m_masterToggle;
    void InitTextFormats(IDWriteFactory* dwf);
    void DrawGreeting(ID2D1RenderTarget* rt, const D2D1_RECT_F& area);
    void DrawStatusBadge(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawNextReminderHero(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawDailySummaryCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawProgressTimeline(ID2D1RenderTarget* rt, const D2D1_RECT_F& area);
    void DrawCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawMiniBarChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, float progress);

    float m_dpiScale = 1.f;
    bool  m_initialized = false;
    
    int   m_hoveredElement = -1;
    int   m_pressedElement = -1;
    D2D1_RECT_F m_statusBadgeRect = {0,0,0,0};

    int m_todayDelivered = -1;
    int m_streak = -1;
    int m_total = -1;
    std::vector<dhikr::DailyStat> m_dailyStats;

    IDWriteFactory*    m_dwf       = nullptr;
    IDWriteTextFormat* m_tfGreet   = nullptr; // large greeting
    IDWriteTextFormat* m_tfSub     = nullptr; // secondary text
    IDWriteTextFormat* m_tfBigNum  = nullptr; // big stat numbers
    IDWriteTextFormat* m_tfMedNum  = nullptr; // medium numbers
    IDWriteTextFormat* m_tfLabel   = nullptr; // card labels
    IDWriteTextFormat* m_tfSmall   = nullptr; // small meta text
    IDWriteTextFormat* m_tfTime    = nullptr; // time display

    ID2D1SolidColorBrush* m_brush  = nullptr;
    ID2D1PathGeometry* m_flameGeometry = nullptr;
    static ID2D1Factory* m_d2dFactory_;
};

} // namespace dhikr
