#include "WindowChrome.h"
#include "LogoRenderer.h"
#include "../DesignSystem.h"
#include "../../core/ApplicationMetadata.h"

namespace dhikr {

void WindowChrome::UpdateMetrics(float dpiScale, int windowWidth, int windowHeight) {
    m_dpiScale = dpiScale;
    m_width = windowWidth;
    m_height = windowHeight;
}

D2D1_RECT_F WindowChrome::GetCaptionBtnRect(int index) const {
    float btnW = ds::Dims::CaptionBtnW * m_dpiScale;
    float th = ds::Dims::TitleBarH * m_dpiScale;
    float startX = m_width - btnW * 3;
    return D2D1::RectF(startX + (index - 1) * btnW, 0, startX + index * btnW, th);
}

LRESULT WindowChrome::HitTest(HWND hwnd, int x, int y) {
    POINT pt = {x, y};
    ScreenToClient(hwnd, &pt);

    // If outside the window client, it's not our problem (though WM_NCHITTEST shouldn't send that usually)
    if (pt.y < 0 || pt.y > m_height || pt.x < 0 || pt.x > m_width) return HTNOWHERE;

    float th = ds::Dims::TitleBarH * m_dpiScale;
    float border = 4.0f * m_dpiScale;

    // Resizing borders
    if (pt.y < border) {
        if (pt.x < border) return HTTOPLEFT;
        if (pt.x > m_width - border) return HTTOPRIGHT;
        return HTTOP;
    }
    if (pt.y > m_height - border) {
        if (pt.x < border) return HTBOTTOMLEFT;
        if (pt.x > m_width - border) return HTBOTTOMRIGHT;
        return HTBOTTOM;
    }
    if (pt.x < border) return HTLEFT;
    if (pt.x > m_width - border) return HTRIGHT;

    // Title bar area
    if (pt.y < th) {
        // Caption buttons
        float btnW = ds::Dims::CaptionBtnW * m_dpiScale;
        float startX = m_width - btnW * 3;
        if (pt.x >= startX) return HTCLIENT; // Let client handle the buttons (WM_MOUSEMOVE/LBUTTONDOWN)
        
        return HTCAPTION; // Draggable area
    }

    return HTCLIENT; // Normal application area
}

void WindowChrome::Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fontApp, IDWriteTextFormat* fontIcon, ID2D1Bitmap* logo) {
    float th = ds::Dims::TitleBarH * m_dpiScale;
    
    // Background
    brush->SetColor(ds::Sidebar());
    rt->FillRectangle(D2D1::RectF(0, 0, (float)m_width, th), brush);
    brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(0, th), D2D1::Point2F((float)m_width, th), brush, 0.5f);

    // Application Logo and Title
    float logoSize = 20.f * m_dpiScale;
    float startX = 16.f * m_dpiScale;
    float textX = startX;

    D2D1_RECT_F logoRect = D2D1::RectF(startX, (th - logoSize) / 2.f, startX + logoSize, (th + logoSize) / 2.f);
    if (logo) {
        D2D1_SIZE_F sz = logo->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(logoRect, sz.width, sz.height);
        rt->DrawBitmap(logo, fitRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, logoRect, 1.0f);
    }
    textX += logoSize + 8.f * m_dpiScale;

    if (fontApp) {
        brush->SetColor(ds::TextPrimary());
        D2D1_RECT_F textRect = D2D1::RectF(textX, 0, 400.f * m_dpiScale, th);
        DWRITE_PARAGRAPH_ALIGNMENT pa = fontApp->GetParagraphAlignment();
        fontApp->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt->DrawTextW(ApplicationMetadata::AppName, (UINT32)wcslen(ApplicationMetadata::AppName), fontApp, textRect, brush);
        fontApp->SetParagraphAlignment(pa);
    }

    // Caption Buttons
    if (!fontIcon) return;
    
    DWRITE_TEXT_ALIGNMENT ta = fontIcon->GetTextAlignment();
    DWRITE_PARAGRAPH_ALIGNMENT pa = fontIcon->GetParagraphAlignment();
    fontIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fontIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    for (int i = 1; i <= 3; ++i) { // 1=min, 2=max, 3=close
        D2D1_RECT_F btnRect = GetCaptionBtnRect(i);
        
        if (m_pressedBtn == i) {
            brush->SetColor(i == 3 ? ds::Rgba(0xC4, 0x2B, 0x1C) : ds::Rgba(0x33, 0x33, 0x33));
            rt->FillRectangle(btnRect, brush);
        } else if (m_hoveredBtn == i) {
            brush->SetColor(i == 3 ? ds::Rgba(0xE8, 0x11, 0x23) : ds::CardHover());
            rt->FillRectangle(btnRect, brush);
        }
        
        brush->SetColor((m_hoveredBtn == i && i == 3) ? ds::Rgba(0xFF, 0xFF, 0xFF) : ds::TextPrimary());
        const wchar_t* glyph = (i == 1) ? ds::Icon::Minimize : (i == 2) ? ds::Icon::Maximize : ds::Icon::Close;
        rt->DrawTextW(glyph, 1, fontIcon, btnRect, brush);
    }

    fontIcon->SetTextAlignment(ta);
    fontIcon->SetParagraphAlignment(pa);
}

bool WindowChrome::OnMouseMove(float x, float y) {
    int hover = 0;
    for (int i = 1; i <= 3; ++i) {
        D2D1_RECT_F r = GetCaptionBtnRect(i);
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            hover = i;
            break;
        }
    }
    if (m_hoveredBtn != hover) {
        m_hoveredBtn = hover;
        return true;
    }
    return false;
}

bool WindowChrome::OnMouseLeave() {
    if (m_hoveredBtn != 0 || m_pressedBtn != 0) {
        m_hoveredBtn = 0;
        m_pressedBtn = 0;
        return true;
    }
    return false;
}

bool WindowChrome::OnLButtonDown(float x, float y) {
    if (m_hoveredBtn != 0) {
        m_pressedBtn = m_hoveredBtn;
        return true;
    }
    return false;
}

bool WindowChrome::OnLButtonUp(HWND hwnd, float x, float y) {
    int pressed = m_pressedBtn;
    m_pressedBtn = 0;
    if (pressed != 0 && pressed == m_hoveredBtn) {
        if (pressed == 1) ShowWindow(hwnd, SW_MINIMIZE);
        else if (pressed == 2) {
            if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
            else ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else if (pressed == 3) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return true;
    }
    return pressed != 0;
}

}
