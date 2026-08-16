#pragma once
#include "../MainWindow.h"
#include <string>

namespace dhikr {

class AboutPage : public PageBase {
public:
    AboutPage();
    ~AboutPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnMouseMove(int x, int y) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;

private:
    void DrawCrescentLogo(ID2D1RenderTarget* rt, float cx, float cy, float radius);

    IDWriteFactory*    m_dwf     = nullptr;
    IDWriteTextFormat* m_tfTitle = nullptr;
    IDWriteTextFormat* m_tfSub   = nullptr;
    IDWriteTextFormat* m_tfBody  = nullptr;
    IDWriteTextFormat* m_tfSmall = nullptr;
    IDWriteTextFormat* m_tfBtn   = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;

    // Interaction state
    int m_hoveredBtn = 0; // 0=none, 1=email link, 2=copy btn
    int m_pressedBtn = 0;
    bool m_justCopied = false;
    DWORD m_copyTime = 0;

    D2D1_RECT_F m_rectEmail;
    D2D1_RECT_F m_rectCopy;
};

} // namespace dhikr
