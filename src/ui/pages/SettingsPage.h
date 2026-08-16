#pragma once
#include "../MainWindow.h"
namespace dhikr {
class SettingsPage : public PageBase {
public:
    SettingsPage();
    ~SettingsPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y);
    bool OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;
    void OnSettingsChanged() override;
    void OnActivated() override;
private:
    enum class Tab { General=0, Popup, Sound, System };
    void DrawTabBar(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawGeneralTab(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawSoundTab(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawSystemTab(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawPopupTab(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover = false);
    void DrawLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text, float controlReserveW = 200.f);
    void DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* val, bool hover = false);
    void DrawButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* label, bool accent, bool hover = false, bool pressed = false);
    void DrawSlider(ID2D1RenderTarget* rt, float x, float y, float w, float value);
    void DrawAccentColorPicker(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawPositionGrid(ID2D1RenderTarget* rt, float x, float y);

    Tab   m_activeTab   = Tab::General;
    int   m_hoveredTab  = -1;
    int   m_hoveredElement = -1;
    int   m_pressedElement = -1;
    bool  m_isDraggingVolume = false;
    float m_dpiScale    = 1.f;
    float m_contentX=0, m_contentY=0, m_contentW=0;
    bool  m_hoveredSave = false;

    AppSettings m_pendingSettings;
    bool m_hasUnsavedChanges = false;

    IDWriteFactory*    m_dwf    = nullptr;
    IDWriteTextFormat* m_tfTitle  = nullptr;
    IDWriteTextFormat* m_tfTab    = nullptr;
    IDWriteTextFormat* m_tfLabel  = nullptr;
    IDWriteTextFormat* m_tfValue  = nullptr;
    IDWriteTextFormat* m_tfBtn    = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;

    static constexpr int kTabCount = 4;
};
} // namespace dhikr
