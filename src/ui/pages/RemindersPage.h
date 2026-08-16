#pragma once
#include "../MainWindow.h"
#include "../../core/Settings.h"

namespace dhikr {
class RemindersPage : public PageBase {
public:
    RemindersPage();
    ~RemindersPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;
    void OnSettingsChanged() override;
    void OnActivated() override;
private:
    void DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover);
    void DrawTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* time);
    void DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* value);
    void DrawSaveButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawSectionLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text);
    void DrawSeparator(ID2D1RenderTarget* rt, float x, float y, float w);
    D2D1_RECT_F GetToggleRect() const;
    D2D1_RECT_F GetSaveRect() const;

    IDWriteFactory*    m_dwf     = nullptr;
    IDWriteTextFormat* m_tfLabel = nullptr;
    IDWriteTextFormat* m_tfValue = nullptr;
    IDWriteTextFormat* m_tfTitle = nullptr;
    IDWriteTextFormat* m_tfBtn   = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;
    float m_contentX = 0, m_contentY = 0, m_contentW = 0;
    bool  m_hoverToggle = false;
    bool  m_hoverSave   = false;
    
    AppSettings m_pendingSettings;
    bool m_hasUnsavedChanges = false;
};
} // namespace dhikr
