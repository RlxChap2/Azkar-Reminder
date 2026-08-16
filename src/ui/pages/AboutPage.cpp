#include "AboutPage.h"
#include "../MainWindow.h"
#include "../DesignSystem.h"
#include "../core/LogoRenderer.h"
#include "../../core/Application.h"
#include "../../core/ApplicationMetadata.h"
#include "../../core/Localization.h"
#include <shellapi.h>

namespace dhikr {

AboutPage::AboutPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, DWRITE_TEXT_ALIGNMENT ta, DWRITE_PARAGRAPH_ALIGNMENT pa, IDWriteTextFormat** out) {
        // Always use LTR for the text format here, we handle RTL placement manually or via standard DWrite if needed
        // but for About we explicitly want English/Arabic support, so we use Noto Sans Arabic if available.
        dwf->CreateTextFormat(L"Noto Sans Arabic", nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, L"en-US", out);
        if (*out) {
            (*out)->SetTextAlignment(ta);
            (*out)->SetParagraphAlignment(pa);
        }
    };
    mk(24.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfTitle);
    mk(ds::FontSize::Body, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSub);
    mk(ds::FontSize::Body, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfBody);
    mk(ds::FontSize::Small, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSmall);
    mk(ds::FontSize::BtnLabel, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfBtn);
}

AboutPage::~AboutPage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfSub)   { m_tfSub->Release();   m_tfSub   = nullptr; }
    if (m_tfBody)  { m_tfBody->Release();  m_tfBody  = nullptr; }
    if (m_tfSmall) { m_tfSmall->Release(); m_tfSmall = nullptr; }
    if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void AboutPage::DrawCrescentLogo(ID2D1RenderTarget* rt, float cx, float cy, float radius) {
    D2D1_RECT_F dest = D2D1::RectF(cx - radius, cy - radius, cx + radius, cy + radius);
    auto* bmp = MainWindow::Instance().GetLogoBitmap();
    if (bmp) {
        D2D1_SIZE_F sz = bmp->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(dest, sz.width, sz.height);
        rt->DrawBitmap(bmp, fitRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, dest, 1.0f);
    }
}

void AboutPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSub) m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBody) m_tfBody->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSmall) m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    float cx = (bounds.left + bounds.right)  / 2.f;
    float cy = (bounds.top  + bounds.bottom) / 2.f;
    float w  = bounds.right - bounds.left;

    // Centered layout, vertically
    float logoR = 56.f * m_dpiScale;
    float logoY = bounds.top + 40.f * m_dpiScale;

    DrawCrescentLogo(rt, cx, logoY + logoR, logoR);

    float y = logoY + logoR * 2 + 16.f * m_dpiScale;

    // App name
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring name = ApplicationMetadata::AppName;
        rt->DrawTextW(name.c_str(), (UINT32)name.size(), m_tfTitle,
            D2D1::RectF(bounds.left, y, bounds.right, y + 36.f * m_dpiScale), m_brush);
        y += 40.f * m_dpiScale;
    }

    // Version
    if (m_tfSub) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring version = Localization::Instance().Get(L"about.version") + L" " + ApplicationMetadata::AppVersion;
        rt->DrawTextW(version.c_str(), (UINT32)version.size(), m_tfSub,
            D2D1::RectF(bounds.left, y, bounds.right, y + 20.f * m_dpiScale), m_brush);
        y += 28.f * m_dpiScale;
    }

    // Tagline
    if (m_tfBody) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring tagline = Localization::Instance().Get(L"home.subtitle");
        rt->DrawTextW(tagline.c_str(), (UINT32)tagline.size(),
            m_tfBody, D2D1::RectF(bounds.left + 40*m_dpiScale, y, bounds.right - 40*m_dpiScale, y + 25.f * m_dpiScale), m_brush);
        y += 25.f * m_dpiScale;

        // Copyright (Force LTR so punctuation is correct)
        std::wstring copy = ApplicationMetadata::Copyright;
        auto rd = m_tfBody->GetReadingDirection();
        m_tfBody->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        rt->DrawTextW(copy.c_str(), (UINT32)copy.size(),
            m_tfBody, D2D1::RectF(bounds.left + 40*m_dpiScale, y, bounds.right - 40*m_dpiScale, y + 25.f * m_dpiScale), m_brush);
        m_tfBody->SetReadingDirection(rd);
        y += 35.f * m_dpiScale;
    }

    // Separator line
    m_brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(cx - 100*m_dpiScale, y), D2D1::Point2F(cx + 100*m_dpiScale, y), m_brush, 0.5f);
    y += 20.f * m_dpiScale;

    // Contact info
    if (m_tfSmall) {
        m_brush->SetColor(ds::TextMuted());
        std::wstring contactStr = Localization::Instance().Get(L"about.contact");
        rt->DrawTextW(contactStr.c_str(), (UINT32)contactStr.size(),
            m_tfSmall, D2D1::RectF(bounds.left, y, bounds.right, y + 20.f * m_dpiScale), m_brush);
        y += 24.f * m_dpiScale;

        // Draw Email (Clickable)
        std::wstring email = ApplicationMetadata::DeveloperEmail;
        float emailW = 220.f * m_dpiScale;
        float btnW = 60.f * m_dpiScale;
        float gap = 10.f * m_dpiScale;
        float totalW = emailW + gap + btnW;
        float startX = cx - totalW / 2.f;

        m_rectEmail = D2D1::RectF(startX, y, startX + emailW, y + 24.f * m_dpiScale);
        m_rectCopy  = D2D1::RectF(startX + emailW + gap, y, startX + totalW, y + 24.f * m_dpiScale);

        // Hover email
        if (m_hoveredBtn == 1) {
            m_brush->SetColor(ds::CardHover());
            rt->FillRoundedRectangle(D2D1::RoundedRect(m_rectEmail, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
        }

        // Email text (forced LTR)
        m_brush->SetColor(ds::Accent::Primary());
        DWRITE_TEXT_ALIGNMENT ta = m_tfSmall->GetTextAlignment();
        DWRITE_READING_DIRECTION rd = m_tfSmall->GetReadingDirection();
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfSmall->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT); // force LTR for email
        rt->DrawTextW(email.c_str(), (UINT32)email.size(), m_tfSmall, m_rectEmail, m_brush);
        m_tfSmall->SetTextAlignment(ta);
        m_tfSmall->SetReadingDirection(rd);

        // Copy button
        if (m_tfBtn) {
            D2D1_RECT_F cr = m_rectCopy;
            if (m_pressedBtn == 2) { cr.left += 1; cr.top += 1; cr.right -= 1; cr.bottom -= 1; }
            m_brush->SetColor(m_hoveredBtn == 2 ? ds::CardHover() : ds::Surface());
            rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
            m_brush->SetColor(ds::Border());
            rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush, 1.f);
            m_brush->SetColor(ds::TextPrimary());

            // Determine if we should show "Copied"
            std::wstring btnLabel = Localization::Instance().Get(L"about.copy");
            if (m_justCopied && GetTickCount() - m_copyTime < 2000) {
                btnLabel = Localization::Instance().Get(L"about.copied");
                m_brush->SetColor(ds::Accent::Primary());
            } else {
                m_justCopied = false;
            }

            rt->DrawTextW(btnLabel.c_str(), (UINT32)btnLabel.size(), m_tfBtn, cr, m_brush);
        }

        y += 40.f * m_dpiScale;

        // Sadaqah Message (Force LTR for English)
        m_brush->SetColor(ds::TextMuted());
        std::wstring sadaqah = ApplicationMetadata::SadaqahMessage;
        auto rd2 = m_tfSmall->GetReadingDirection();
        m_tfSmall->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        rt->DrawTextW(sadaqah.c_str(), (UINT32)sadaqah.size(),
            m_tfSmall, D2D1::RectF(bounds.left, y, bounds.right, y + 20.f * m_dpiScale), m_brush);
        m_tfSmall->SetReadingDirection(rd2);
    }
}

bool AboutPage::OnMouseMove(int x, int y) {
    float fx = (float)x; float fy = (float)y;
    int hover = 0;
    if (fx >= m_rectEmail.left && fx <= m_rectEmail.right && fy >= m_rectEmail.top && fy <= m_rectEmail.bottom) {
        hover = 1;
    } else if (fx >= m_rectCopy.left && fx <= m_rectCopy.right && fy >= m_rectCopy.top && fy <= m_rectCopy.bottom) {
        hover = 2;
    }

    if (m_hoveredBtn != hover) {
        m_hoveredBtn = hover;
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}

bool AboutPage::OnLButtonDown(int x, int y) {
    if (m_hoveredBtn != 0) {
        m_pressedBtn = m_hoveredBtn;
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}

bool AboutPage::OnLButtonUp(int x, int y) {
    int pressed = m_pressedBtn;
    m_pressedBtn = 0;
    if (pressed != 0 && pressed == m_hoveredBtn) {
        if (pressed == 1) { // Click email
            std::wstring mailto = L"mailto:" + std::wstring(ApplicationMetadata::DeveloperEmail);
            ShellExecuteW(nullptr, L"open", mailto.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (pressed == 2) { // Copy email
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                std::wstring email = ApplicationMetadata::DeveloperEmail;
                HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (email.size() + 1) * sizeof(wchar_t));
                if (hg) {
                    memcpy(GlobalLock(hg), email.c_str(), (email.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(hg);
                    SetClipboardData(CF_UNICODETEXT, hg);
                }
                CloseClipboard();
                m_justCopied = true;
                m_copyTime = GetTickCount();
            }
        }
        MainWindow::Instance().Invalidate();
        return true;
    }
    MainWindow::Instance().Invalidate();
    return pressed != 0;
}

void AboutPage::OnMouseLeave() {
    if (m_hoveredBtn != 0 || m_pressedBtn != 0) {
        m_hoveredBtn = 0;
        m_pressedBtn = 0;
        MainWindow::Instance().Invalidate();
    }
}

bool AboutPage::HitTestInteractive(int x, int y) {
    float fx = (float)x; float fy = (float)y;
    return (fx >= m_rectEmail.left && fx <= m_rectEmail.right && fy >= m_rectEmail.top && fy <= m_rectEmail.bottom) ||
           (fx >= m_rectCopy.left && fx <= m_rectCopy.right && fy >= m_rectCopy.top && fy <= m_rectCopy.bottom);
}

} // namespace dhikr
