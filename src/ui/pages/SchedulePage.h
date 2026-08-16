#pragma once
#include "../MainWindow.h"
namespace dhikr {
class SchedulePage : public PageBase {
public:
    SchedulePage();
    ~SchedulePage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y);
    bool OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;
    void OnSettingsChanged() override;
    void OnActivated() override;
private:
    void DrawWeekRow(ID2D1RenderTarget* rt, int dayIndex, float x, float y, float w);
    void DrawRowToggle(ID2D1RenderTarget* rt, float cx, float cy, bool on);
    void DrawSmallTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, int h, int m);
    void DrawAddQuietHoursBtn(ID2D1RenderTarget* rt, float x, float y);

    IDWriteFactory*    m_dwf      = nullptr;
    IDWriteTextFormat* m_tfLabel  = nullptr;
    IDWriteTextFormat* m_tfValue  = nullptr;
    IDWriteTextFormat* m_tfTitle  = nullptr;
    IDWriteTextFormat* m_tfBtn    = nullptr;
    IDWriteTextFormat* m_tfDay    = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;
    int   m_hoveredRow = -1;
    int   m_hoveredElement = -1; // 0-6: toggles, 10-16: start, 20-26: end, 30-36: freq, 40-46: menu, 100: add btn
    int   m_pressedElement = -1;
    
    // Layout cache for hit testing
    float m_contentX = 0;
    float m_contentY = 0;
    float m_contentW = 0;
    float m_rowH = 0;
    bool  m_hoveredSave = false;

    AppSettings m_pendingSettings;
    bool m_hasUnsavedChanges = false;

    static constexpr const wchar_t* kDays[7] = {
        L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
        L"Thursday", L"Friday", L"Saturday"
    };
};
} // namespace dhikr
