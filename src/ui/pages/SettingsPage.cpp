#include "SettingsPage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include "../../core/Scheduler.h"
#include "../../core/Logger.h"
#include "../../core/Localization.h"
#include "../../audio/AudioEngine.h"
#include "../../update/Updater.h"
#include "../../core/ApplicationMetadata.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <commdlg.h>
#include <shellapi.h>

namespace dhikr {

SettingsPage::SettingsPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    bool isRtl = Localization::Instance().IsRTL();
    const wchar_t* fontName = isRtl ? L"Noto Sans Arabic" : L"Noto Sans";
    const wchar_t* locale   = isRtl ? L"ar-SA" : L"en-US";

    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(fontName, nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, locale, out);
    };
    mk(ds::FontSize::PageTitle, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfTitle);
    mk(ds::FontSize::TabLabel,  DWRITE_FONT_WEIGHT_REGULAR,   &m_tfTab);
    mk(ds::FontSize::Body,      DWRITE_FONT_WEIGHT_REGULAR,   &m_tfLabel);
    mk(ds::FontSize::Body,      DWRITE_FONT_WEIGHT_REGULAR,   &m_tfValue);
    mk(ds::FontSize::BtnLabel,  DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);
    if (m_tfTab) {
        m_tfTab->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfTab->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfBtn) {
        m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfTitle) {
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

SettingsPage::~SettingsPage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfTab)   { m_tfTab->Release();   m_tfTab   = nullptr; }
    if (m_tfLabel) { m_tfLabel->Release(); m_tfLabel = nullptr; }
    if (m_tfValue) { m_tfValue->Release(); m_tfValue = nullptr; }
    if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void SettingsPage::DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover) {
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float kd = ds::Dims::ToggleKnobD * m_dpiScale;
    float rx = th / 2.f;
    if (hover && !on) m_brush->SetColor(ds::Border());
    else m_brush->SetColor(on ? (hover ? ds::Accent::Primary(0.85f) : ds::Accent::Primary()) : ds::Border());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+tw,y+th), rx, rx), m_brush);
    float kx = on ? (x+tw-kd-(th-kd)/2.f) : (x+(th-kd)/2.f);
    float ky = y + (th-kd)/2.f;
    m_brush->SetColor(ds::Rgba(0xFF,0xFF,0xFF));
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx+kd/2.f,ky+kd/2.f),kd/2.f,kd/2.f),m_brush);
}

void SettingsPage::DrawLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text, float controlReserveW) {
    if (!m_tfLabel || !m_brush) return;
    m_brush->SetColor(ds::TextPrimary());
    m_tfLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    bool isRtl = Localization::Instance().IsRTL();
    float ctrl = controlReserveW * m_dpiScale;
    float gap  = 16.f * m_dpiScale;
    float labelH = 24.f * m_dpiScale;

    m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    float lx, lRight;
    if (isRtl) {
        lx     = m_contentX + ctrl + gap;
        lRight = m_contentX + m_contentW;
    } else {
        lx     = m_contentX;
        lRight = m_contentX + m_contentW - ctrl - gap;
    }

    rt->DrawTextW(text, (UINT32)wcslen(text), m_tfLabel,
        D2D1::RectF(lx, y, lRight, y + labelH), m_brush);
}

void SettingsPage::DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* val, bool hover) {
    float rx = ds::Dims::InputRadius * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(hover ? ds::TextMuted() : ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = isRtl
            ? D2D1::RectF(r.left + 24*m_dpiScale, r.top, r.right - 10*m_dpiScale, r.bottom)
            : D2D1::RectF(r.left + 10*m_dpiScale, r.top, r.right - 24*m_dpiScale, r.bottom);
        rt->DrawTextW(val, (UINT32)wcslen(val), m_tfValue, tr, m_brush);
    }
    // Chevron
    m_brush->SetColor(ds::TextMuted());
    float cx = isRtl ? (r.left + 14 * m_dpiScale) : (r.right - 14 * m_dpiScale);
    float cy = (r.top + r.bottom) / 2.f;
    rt->DrawLine(D2D1::Point2F(cx-4*m_dpiScale, cy-2*m_dpiScale), D2D1::Point2F(cx, cy+2*m_dpiScale), m_brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx, cy+2*m_dpiScale), D2D1::Point2F(cx+4*m_dpiScale, cy-2*m_dpiScale), m_brush, 1.5f);
}

void SettingsPage::DrawButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r,
                               const wchar_t* label, bool accent, bool hover, bool pressed) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    D2D1_RECT_F drawR = r;
    
    if (pressed) {
        float shrink = 1.0f * m_dpiScale;
        drawR.left += shrink; drawR.top += shrink;
        drawR.right -= shrink; drawR.bottom -= shrink;
    }
    
    if (accent) {
        m_brush->SetColor(hover ? ds::Accent::Primary(0.9f) : ds::Accent::Primary());
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        m_brush->SetColor(ds::Rgba(0x08,0x0A,0x09));
    } else {
        m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        m_brush->SetColor(ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush, 1.f);
        m_brush->SetColor(ds::TextPrimary());
    }
    if (m_tfBtn) rt->DrawTextW(label, (UINT32)wcslen(label), m_tfBtn, drawR, m_brush);
}

void SettingsPage::DrawSlider(ID2D1RenderTarget* rt, float x, float y, float w, float value) {
    float trackH = 4.f * m_dpiScale;
    float ky = y + (ds::Dims::ToggleH * m_dpiScale - trackH) / 2.f;
    float rx = trackH / 2.f;

    // Track
    m_brush->SetColor(ds::Border());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, ky, x+w, ky+trackH), rx, rx), m_brush);
    // Fill
    float fillW = w * value;
    m_brush->SetColor(ds::Accent::Primary());
    if (fillW > 0) rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, ky, x+fillW, ky+trackH), rx, rx), m_brush);
    // Knob
    float knobR = 7.f * m_dpiScale;
    float kx = x + fillW;
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, ky + trackH/2.f), knobR, knobR), m_brush);
    m_brush->SetColor(ds::Background());
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, ky + trackH/2.f), knobR*0.5f, knobR*0.5f), m_brush);
}

void SettingsPage::DrawAccentColorPicker(ID2D1RenderTarget* rt, float x, float y, float w) {
    struct { uint8_t r, g, b; const wchar_t* name; } presets[] = {
        {0xB7,0xF3,0x4A, L"Lime"},
        {0x4A,0xDE,0x80, L"Green"},
        {0x60,0xA5,0xFA, L"Blue"},
        {0xC0,0x84,0xFC, L"Purple"},
        {0xFB,0xBF,0x24, L"Amber"},
    };

    auto& ac = Settings::Instance().Get().accentColor;

    float dotSize = 22.f * m_dpiScale;
    float gap     = 10.f * m_dpiScale;

    for (int i = 0; i < 5; ++i) {
        float cx = x + i * (dotSize + gap);
        float cy = y + dotSize / 2.f;

        auto& p = presets[i];
        m_brush->SetColor(ds::Rgba(p.r, p.g, p.b));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + dotSize/2.f, cy), dotSize/2.f, dotSize/2.f), m_brush);

        // Check mark if selected
        if (ac.r == p.r && ac.g == p.g && ac.b == p.b) {
            m_brush->SetColor(ds::Rgba(0x08,0x0A,0x09));
            float arm = 5.f * m_dpiScale;
            rt->DrawLine(D2D1::Point2F(cx+dotSize/2.f-arm, cy), D2D1::Point2F(cx+dotSize/2.f, cy+arm), m_brush, 2.f);
            rt->DrawLine(D2D1::Point2F(cx+dotSize/2.f, cy+arm), D2D1::Point2F(cx+dotSize/2.f+arm, cy-arm), m_brush, 2.f);
        }
    }
}

void SettingsPage::DrawPositionGrid(ID2D1RenderTarget* rt, float x, float y) {
    auto& pos = m_pendingSettings.popupPosition;
    float dotSize  = 18.f * m_dpiScale;
    float cellSize = dotSize + 8.f * m_dpiScale;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = row * 3 + col;
            float cx = x + col * cellSize + dotSize / 2.f;
            float cy = y + row * cellSize + dotSize / 2.f;

            bool selected = ((int)pos == idx);
            m_brush->SetColor(selected ? ds::Accent::Primary() : ds::Border());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotSize/2.f, dotSize/2.f), m_brush);
        }
    }
}

void SettingsPage::DrawTabBar(ID2D1RenderTarget* rt, float x, float y, float w) {
    float tabH  = 36.f * m_dpiScale;
    float tabW  = w / kTabCount;
    bool isRtl  = Localization::Instance().IsRTL();

    // Tab bar background
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+tabH), 6*m_dpiScale, 6*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+tabH), 6*m_dpiScale, 6*m_dpiScale), m_brush, 0.5f);

    for (int i = 0; i < kTabCount; ++i) {
        int tabIdx = isRtl ? (kTabCount - 1 - i) : i;
        D2D1_RECT_F tabR = D2D1::RectF(x + i*tabW, y, x + (i+1)*tabW, y + tabH);
        bool active = ((int)m_activeTab == tabIdx);
        bool hover  = (m_hoveredTab == tabIdx);

        if (active) {
            m_brush->SetColor(ds::ActiveNavBg());
            rt->FillRoundedRectangle(D2D1::RoundedRect(tabR, 6*m_dpiScale, 6*m_dpiScale), m_brush);

            // Bottom accent line
            m_brush->SetColor(ds::Accent::Primary());
            rt->DrawLine(D2D1::Point2F(tabR.left+8*m_dpiScale, tabR.bottom-1.5f),
                         D2D1::Point2F(tabR.right-8*m_dpiScale, tabR.bottom-1.5f), m_brush, 2.f);
        }

        if (m_tfTab) {
            m_brush->SetColor(active ? ds::Accent::Primary() : (hover ? ds::TextPrimary() : ds::TextSecondary()));
            const wchar_t* keys[] = { L"settings.general", L"settings.popup", L"settings.sound", L"settings.system" };
            std::wstring tStr = Localization::Instance().Get(keys[tabIdx]);
            rt->DrawTextW(tStr.c_str(), (UINT32)tStr.size(), m_tfTab, tabR, m_brush);
        }
    }
}

void SettingsPage::DrawGeneralTab(ID2D1RenderTarget* rt, float x, float y, float w) {
    auto& s = m_pendingSettings;
    float fieldW = 180.f;
    float rowH   = 44.f * m_dpiScale;
    float fh     = ds::Dims::InputH * m_dpiScale;
    bool isRtl   = Localization::Instance().IsRTL();
    float fwPx   = fieldW * m_dpiScale;

    auto ctrlX = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // Language
    DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"settings.language").c_str(), fieldW);
    const wchar_t* lang = s.language == L"ar" ? L"العربية" : (s.language == L"en" ? L"English" : L"System Default");
    float fX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(fX, y, fX + fwPx, y + fh), lang, m_hoveredElement == 1);
    y += rowH;

    // Start with Windows
    float togW = ds::Dims::ToggleW;
    DrawLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"settings.start_with_windows").c_str(), togW);
    fX = ctrlX(togW);
    DrawToggle(rt, fX, y, s.startWithWindows, m_hoveredElement == 3);
    y += rowH;

    // Accent Color
    float accentW = 200.f;
    DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"settings.accent_color").c_str(), accentW);
    fX = ctrlX(accentW);
    DrawAccentColorPicker(rt, fX, y, accentW * m_dpiScale);
}

void SettingsPage::DrawSoundTab(ID2D1RenderTarget* rt, float x, float y, float w) {
    auto& s = m_pendingSettings;
    float rowH   = 44.f * m_dpiScale;
    float fieldW = 180.f;
    float fh     = ds::Dims::InputH * m_dpiScale;
    bool isRtl   = Localization::Instance().IsRTL();
    float fwPx   = fieldW * m_dpiScale;

    auto ctrlX = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // Enable Sound
    float togW = ds::Dims::ToggleW;
    DrawLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"settings.enable_sound").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), y, s.soundEnabled, m_hoveredElement == 11);
    y += rowH;

    // Volume
    float sliderReserve = fieldW + 64.f;
    DrawLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"settings.volume").c_str(), sliderReserve);
    
    if (isRtl) {
        float pctW = 40.f * m_dpiScale;
        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            wchar_t buf[8]; swprintf_s(buf, L"%d%%", s.soundVolume);
            rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue,
                D2D1::RectF(x, y, x + pctW, y + fh), m_brush);
        }
        float sliderX = x + pctW + 8.f * m_dpiScale;
        DrawSlider(rt, sliderX, y, fwPx, s.soundVolume / 100.f);
    } else {
        float sliderX = x + w - fwPx;
        DrawSlider(rt, sliderX, y, fwPx, s.soundVolume / 100.f);
        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            wchar_t buf[8]; swprintf_s(buf, L"%d%%", s.soundVolume);
            rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue,
                D2D1::RectF(sliderX - 48.f*m_dpiScale, y, sliderX - 8.f*m_dpiScale, y + fh), m_brush);
        }
    }
    y += rowH;

    // Notification Sound Selection
    std::wstring soundName = s.customSoundPath.empty() ?
        Localization::Instance().Get(L"settings.sound_default") :
        std::filesystem::path(s.customSoundPath).filename().wstring();
    
    float soundControlReserve = 340.f;
    DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"settings.notification_sound").c_str(), soundControlReserve);
    
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float browseW = 150.f * m_dpiScale;
    float resetW = 36.f * m_dpiScale;

    if (isRtl) {
        float curX = x;
        std::wstring browseStr = Localization::Instance().Get(L"settings.choose_sound");
        DrawButton(rt, D2D1::RectF(curX, y, curX + browseW, y + btnH), browseStr.c_str(), false, m_hoveredElement == 13, m_pressedElement == 13);
        curX += browseW + 8.f * m_dpiScale;

        if (!s.customSoundPath.empty()) {
            DrawButton(rt, D2D1::RectF(curX, y, curX + resetW, y + btnH), L"↺", false, m_hoveredElement == 14, m_pressedElement == 14);
            curX += resetW + 8.f * m_dpiScale;
        }

        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            rt->DrawTextW(soundName.c_str(), (UINT32)soundName.size(), m_tfValue,
                D2D1::RectF(curX, y + 6*m_dpiScale, x + w, y + btnH), m_brush);
        }
    } else {
        float curX = x + w;
        if (!s.customSoundPath.empty()) {
            curX -= resetW;
            DrawButton(rt, D2D1::RectF(curX, y, curX + resetW, y + btnH), L"↺", false, m_hoveredElement == 14, m_pressedElement == 14);
            curX -= 8.f * m_dpiScale;
        }

        curX -= browseW;
        std::wstring browseStr = Localization::Instance().Get(L"settings.choose_sound");
        DrawButton(rt, D2D1::RectF(curX, y, curX + browseW, y + btnH), browseStr.c_str(), false, m_hoveredElement == 13, m_pressedElement == 13);
        curX -= 12.f * m_dpiScale;

        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            rt->DrawTextW(soundName.c_str(), (UINT32)soundName.size(), m_tfValue,
                D2D1::RectF(x, y + 6*m_dpiScale, curX, y + btnH), m_brush);
        }
    }
    y += rowH + 6*m_dpiScale;

    // Test Sound button
    float testBtnW = 140.f;
    float bX = ctrlX(testBtnW);
    std::wstring testStr = L"▶  " + Localization::Instance().Get(L"settings.test_sound");
    DrawButton(rt, D2D1::RectF(bX, y, bX + testBtnW*m_dpiScale, y + btnH), testStr.c_str(), false, m_hoveredElement == 12, m_pressedElement == 12);
}

void SettingsPage::DrawPopupTab(ID2D1RenderTarget* rt, float x, float y, float w) {
    auto& s = m_pendingSettings;
    float rowH   = 44.f * m_dpiScale;
    float fieldW = 180.f;
    float fh     = ds::Dims::InputH * m_dpiScale;
    bool isRtl   = Localization::Instance().IsRTL();
    float fwPx   = fieldW * m_dpiScale;

    auto ctrlX = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // Position picker
    DrawLabel(rt, x, y + 8*m_dpiScale, Localization::Instance().Get(L"settings.position").c_str(), 100.f);
    DrawPositionGrid(rt, ctrlX(100.f), y);
    y += rowH + 28*m_dpiScale;

    // Duration
    DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"settings.duration").c_str(), fieldW);
    static const wchar_t* durLabels[] = { L"2s", L"3s", L"5s", L"7s", L"10s", L"15s" };
    int durIdx = 2;
    if (s.popupDurationSec == 2)  durIdx = 0;
    else if (s.popupDurationSec == 3)  durIdx = 1;
    else if (s.popupDurationSec == 5)  durIdx = 2;
    else if (s.popupDurationSec == 7)  durIdx = 3;
    else if (s.popupDurationSec == 10) durIdx = 4;
    else if (s.popupDurationSec == 15) durIdx = 5;
    float ddX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(ddX, y, ddX + fwPx, y + fh), durLabels[durIdx], m_hoveredElement == 21);
    y += rowH;

    // Animation
    float togW = ds::Dims::ToggleW;
    DrawLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"settings.animations").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), y, s.animationEnabled, m_hoveredElement == 22);
}

void SettingsPage::DrawSystemTab(ID2D1RenderTarget* rt, float x, float y, float w) {
    auto& s = m_pendingSettings;
    float rowH = 44.f * m_dpiScale;
    float fh   = ds::Dims::InputH * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float btnW = 180.f;
    float bwPx = btnW * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    auto ctrlX = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // Full-screen distraction suppression toggle
    float togW = ds::Dims::ToggleW;
    DrawLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"settings.fullscreen_suppression").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), y, s.suppressDuringFullscreen, m_hoveredElement == 33);
    y += rowH;

    // Full-screen behavior dropdown (when enabled)
    if (s.suppressDuringFullscreen) {
        float fieldW = 220.f;
        float fwPx   = fieldW * m_dpiScale;
        DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"settings.fullscreen_behavior").c_str(), fieldW);
        const wchar_t* modeKeys[] = { L"settings.fullscreen_queue", L"settings.fullscreen_suppress", L"settings.fullscreen_win_notify" };
        int modeIdx = std::clamp(s.fullscreenSuppressionMode, 0, 2);
        std::wstring modeStr = Localization::Instance().Get(modeKeys[modeIdx]);
        float fX = ctrlX(fieldW);
        DrawDropdown(rt, D2D1::RectF(fX, y, fX + fwPx, y + fh), modeStr.c_str(), m_hoveredElement == 34);
        y += rowH;
    }

    // Version label + value
    DrawLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"about.version").c_str(), 100.f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextSecondary());
        float vX = ctrlX(100.f);
        rt->DrawTextW(ApplicationMetadata::AppVersion, (UINT32)wcslen(ApplicationMetadata::AppVersion),
            m_tfValue, D2D1::RectF(vX, y + 6*m_dpiScale, vX + 100.f*m_dpiScale, y + rowH), m_brush);
    }
    y += rowH;

    // Check for Updates button
    float cbX = ctrlX(btnW);
    std::wstring updateStr = Localization::Instance().Get(L"settings.check_updates");
    DrawButton(rt, D2D1::RectF(cbX, y, cbX + bwPx, y + btnH), updateStr.c_str(), false, m_hoveredElement == 31, m_pressedElement == 31);
    y += rowH;

    // Reset settings (destructive)
    float resetW = 180.f;
    float rwPx   = resetW * m_dpiScale;
    float resetX = ctrlX(resetW);
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    bool hover   = (m_hoveredElement == 32);
    bool pressed = (m_pressedElement == 32);
    m_brush->SetColor(pressed ? ds::Rgba(0xC0,0x40,0x40,0.25f) : (hover ? ds::Rgba(0xC0,0x40,0x40,0.15f) : ds::Surface()));
    D2D1_RECT_F rr = D2D1::RectF(resetX, y, resetX + rwPx, y + btnH);
    rt->FillRoundedRectangle(D2D1::RoundedRect(rr, rx, rx), m_brush);
    m_brush->SetColor(ds::Rgba(0xC0,0x40,0x40,0.8f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rr, rx, rx), m_brush, 1.f);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0xE8,0x40,0x40));
        std::wstring resetStr = Localization::Instance().Get(L"settings.reset_all");
        rt->DrawTextW(resetStr.c_str(), (UINT32)resetStr.size(), m_tfBtn, rr, m_brush);
    }
}

void SettingsPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfValue) m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfTab) m_tfTab->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;
    m_contentX = x; m_contentY = y; m_contentW = w;

    // Title
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring title = Localization::Instance().Get(L"settings.title");
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, D2D1::RectF(x,y,x+w,y+28*m_dpiScale), m_brush);
    }
    y += 40.f * m_dpiScale;

    // Tab bar
    float tabH = 36.f * m_dpiScale;
    DrawTabBar(rt, x, y, w);
    y += tabH + ds::Space::L * m_dpiScale;

    // Separator
    m_brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(x,y), D2D1::Point2F(x+w,y), m_brush, 0.5f);
    y += ds::Space::L * m_dpiScale;

    // Active tab content
    switch (m_activeTab) {
        case Tab::General:   DrawGeneralTab(rt, x, y, w);   break;
        case Tab::Popup:     DrawPopupTab(rt, x, y, w);     break;
        case Tab::Sound:     DrawSoundTab(rt, x, y, w);     break;
        case Tab::System:    DrawSystemTab(rt, x, y, w);    break;
    }

    // Save button (bottom right or bottom left if RTL)
    if (m_hasUnsavedChanges) {
        float saveBtnW = 150.f * m_dpiScale;
        float saveBtnH = ds::Dims::BtnH * m_dpiScale;
        float sx = isRtl ? bounds.left + pad : bounds.right - pad - saveBtnW;
        float sy = bounds.bottom - pad - saveBtnH;
        std::wstring saveStr = Localization::Instance().Get(L"reminders.save_star");
        DrawButton(rt, D2D1::RectF(sx, sy, sx+saveBtnW, sy+saveBtnH), saveStr.c_str(), true, m_hoveredSave, false);
    }
}

bool SettingsPage::OnLButtonDown(int x, int y) {
    float pad  = ds::Space::XL * m_dpiScale;
    float cx   = m_contentX;
    float cy   = m_contentY + 40.f * m_dpiScale;
    float w    = m_contentW;
    float tabH = 36.f * m_dpiScale;
    float tabW = w / kTabCount;
    bool isRtl = Localization::Instance().IsRTL();

    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
    };

    // 1. Hit test tab bar
    if (x >= cx && x <= cx + w && y >= cy && y <= cy + tabH) {
        int col = (int)((x - cx) / tabW);
        if (col >= 0 && col < kTabCount) {
            int tabIdx = isRtl ? (kTabCount - 1 - col) : col;
            m_activeTab = (Tab)tabIdx;
            MainWindow::Instance().Invalidate();
            return true;
        }
    }

    float startY = m_contentY + 40*m_dpiScale + tabH + ds::Space::L*m_dpiScale * 2;
    float rowH   = 44.f * m_dpiScale;
    float fh     = ds::Dims::InputH * m_dpiScale;
    float btnH   = ds::Dims::BtnH * m_dpiScale;

    // 2. General Tab
    if (m_activeTab == Tab::General) {
        float fieldW = 180.f;
        float togW   = ds::Dims::ToggleW;
        float accentW= 200.f;

        // Language
        float langX = ctrlLeft(fieldW);
        if (x >= langX && x <= langX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"System Default");
            AppendMenuW(hMenu, MF_STRING, 2, L"English");
            AppendMenuW(hMenu, MF_STRING, 3, L"العربية");
            POINT pt{ (LONG)langX, (LONG)(startY + fh) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
            DestroyMenu(hMenu);
            if (cmd > 0) {
                auto& s = m_pendingSettings;
                if (cmd == 1) s.language = L"system";
                else if (cmd == 2) s.language = L"en";
                else if (cmd == 3) s.language = L"ar";
                m_hasUnsavedChanges = true;
                MainWindow::Instance().Invalidate();
            }
            return true;
        }
        startY += rowH;

        // Start with Windows
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) {
            m_pressedElement = 3;
            auto& s = m_pendingSettings;
            s.startWithWindows = !s.startWithWindows;
            Application::Instance().SetStartWithWindows(s.startWithWindows);
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
        startY += rowH;

        // Accent Color dots (Instant 1-Click Update)
        float dotSize = 22.f * m_dpiScale;
        float gap     = 10.f * m_dpiScale;
        float ax = ctrlLeft(accentW);
        for (int i = 0; i < 5; ++i) {
            float dcx = ax + i * (dotSize + gap);
            if (x >= dcx && x <= dcx + dotSize && y >= startY && y <= startY + dotSize) {
                struct { uint8_t r, g, b; } presets[] = {
                    {0xB7,0xF3,0x4A}, {0x4A,0xDE,0x80}, {0x60,0xA5,0xFA}, {0xC0,0x84,0xFC}, {0xFB,0xBF,0x24}
                };
                m_pendingSettings.accentColor = Color(presets[i].r, presets[i].g, presets[i].b);
                Settings::Instance().Get().accentColor = m_pendingSettings.accentColor;
                Settings::Instance().Save();
                MainWindow::Instance().OnSettingsChanged();
                MainWindow::Instance().Invalidate();
                return true;
            }
        }
    }

    // 3. Sound Tab
    if (m_activeTab == Tab::Sound) {
        float togW = ds::Dims::ToggleW;
        float fieldW = 180.f;
        float btnW = 140.f;

        // Enable Sound Toggle
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) {
            m_pressedElement = 11;
            auto& s = m_pendingSettings;
            s.soundEnabled = !s.soundEnabled;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
        startY += rowH;

        // Volume Slider
        float sliderX = isRtl ? (m_contentX + 40.f * m_dpiScale + 8.f * m_dpiScale) : ctrlLeft(fieldW);
        float fwPx = fieldW * m_dpiScale;
        if (x >= sliderX && x <= sliderX + fwPx && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) {
            m_isDraggingVolume = true;
            auto& s = m_pendingSettings;
            float t = (x - sliderX) / fwPx;
            s.soundVolume = std::clamp((int)(t * 100), 0, 100);
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
        startY += rowH;

        // Custom Sound Browse / Reset Buttons
        float browseW = 150.f * m_dpiScale;
        float resetW  = 36.f * m_dpiScale;
        auto& s = m_pendingSettings;

        if (isRtl) {
            float curX = m_contentX;
            // Browse button
            if (x >= curX && x <= curX + browseW && y >= startY && y <= startY + btnH) {
                m_pressedElement = 13;
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = Application::Instance().MainWindow();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav;*.wma;*.m4a)\0*.mp3;*.wav;*.wma;*.m4a\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    s.customSoundPath = szFile;
                    Settings::Instance().Get().customSoundPath = s.customSoundPath;
                    AudioEngine::Instance().ReloadSound();
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                }
                return true;
            }
            curX += browseW + 8.f * m_dpiScale;

            if (!s.customSoundPath.empty()) {
                if (x >= curX && x <= curX + resetW && y >= startY && y <= startY + btnH) {
                    m_pressedElement = 14;
                    s.customSoundPath = L"";
                    Settings::Instance().Get().customSoundPath = s.customSoundPath;
                    AudioEngine::Instance().ReloadSound();
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                    return true;
                }
            }
        } else {
            float curX = m_contentX + m_contentW;
            if (!s.customSoundPath.empty()) {
                curX -= resetW;
                if (x >= curX && x <= curX + resetW && y >= startY && y <= startY + btnH) {
                    m_pressedElement = 14;
                    s.customSoundPath = L"";
                    Settings::Instance().Get().customSoundPath = s.customSoundPath;
                    AudioEngine::Instance().ReloadSound();
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                    return true;
                }
                curX -= 8.f * m_dpiScale;
            }

            curX -= browseW;
            if (x >= curX && x <= curX + browseW && y >= startY && y <= startY + btnH) {
                m_pressedElement = 13;
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = Application::Instance().MainWindow();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav;*.wma;*.m4a)\0*.mp3;*.wav;*.wma;*.m4a\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    s.customSoundPath = szFile;
                    Settings::Instance().Get().customSoundPath = s.customSoundPath;
                    AudioEngine::Instance().ReloadSound();
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                }
                return true;
            }
        }
        startY += rowH + 6*m_dpiScale;

        // Test Sound Button
        float bX = ctrlLeft(btnW);
        if (x >= bX && x <= bX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) {
            m_pressedElement = 12;
            AudioEngine::Instance().PlayReminder();
            MainWindow::Instance().Invalidate();
            return true;
        }
    }

    // 4. Popup Tab
    if (m_activeTab == Tab::Popup) {
        float fieldW = 180.f;
        float togW   = ds::Dims::ToggleW;

        // Position grid
        float gridX = ctrlLeft(100.f);
        float dotSize = 18.f * m_dpiScale;
        float cellSize = dotSize + 8.f * m_dpiScale;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                float dcx = gridX + c * cellSize + dotSize / 2.f;
                float dcy = startY + r * cellSize + dotSize / 2.f;
                if (x >= dcx - 12*m_dpiScale && x <= dcx + 12*m_dpiScale && y >= dcy - 12*m_dpiScale && y <= dcy + 12*m_dpiScale) {
                    auto& s = m_pendingSettings;
                    s.popupPosition = (PopupPosition)(r * 3 + c);
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                    return true;
                }
            }
        }
        startY += rowH + 28*m_dpiScale;

        // Duration Dropdown
        float durX = ctrlLeft(fieldW);
        if (x >= durX && x <= durX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"2s");
            AppendMenuW(hMenu, MF_STRING, 2, L"3s");
            AppendMenuW(hMenu, MF_STRING, 3, L"5s");
            AppendMenuW(hMenu, MF_STRING, 4, L"7s");
            AppendMenuW(hMenu, MF_STRING, 5, L"10s");
            AppendMenuW(hMenu, MF_STRING, 6, L"15s");
            
            POINT pt{ (LONG)durX, (LONG)(startY + fh) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
            DestroyMenu(hMenu);
            
            if (cmd > 0) {
                auto& s = m_pendingSettings;
                int map[] = { 2, 3, 5, 7, 10, 15 };
                s.popupDurationSec = map[cmd - 1];
                m_hasUnsavedChanges = true;
                MainWindow::Instance().Invalidate();
            }
            return true;
        }
        startY += rowH;

        // Animation Toggle
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) {
            auto& s = m_pendingSettings;
            s.animationEnabled = !s.animationEnabled;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
    }

    // 5. System Tab
    if (m_activeTab == Tab::System) {
        auto& s = m_pendingSettings;
        float togW = ds::Dims::ToggleW;
        float btnW = 180.f;

        // Fullscreen suppression toggle
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) {
            m_pressedElement = 33;
            s.suppressDuringFullscreen = !s.suppressDuringFullscreen;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
        startY += rowH;

        // Fullscreen behavior dropdown (if enabled)
        if (s.suppressDuringFullscreen) {
            float fieldW = 220.f;
            float fX = ctrlLeft(fieldW);
            if (x >= fX && x <= fX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, 1, Localization::Instance().Get(L"settings.fullscreen_queue").c_str());
                AppendMenuW(hMenu, MF_STRING, 2, Localization::Instance().Get(L"settings.fullscreen_suppress").c_str());
                AppendMenuW(hMenu, MF_STRING, 3, Localization::Instance().Get(L"settings.fullscreen_win_notify").c_str());
                POINT pt{ (LONG)fX, (LONG)(startY + fh) };
                ClientToScreen(Application::Instance().MainWindow(), &pt);
                int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
                DestroyMenu(hMenu);
                if (cmd >= 1 && cmd <= 3) {
                    s.fullscreenSuppressionMode = cmd - 1;
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                }
                return true;
            }
            startY += rowH;
        }

        startY += rowH; // Skip version line

        // Check for Updates
        float cbX = ctrlLeft(btnW);
        if (x >= cbX && x <= cbX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) {
            m_pressedElement = 31;
            Updater::Instance().CheckNow(Application::Instance().MainWindow());
            MainWindow::Instance().Invalidate();
            return true;
        }
        startY += rowH;

        // Reset Settings Button with confirmation
        float resetX = ctrlLeft(btnW);
        if (x >= resetX && x <= resetX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) {
            m_pressedElement = 32;
            std::wstring title = Localization::Instance().Get(L"settings.reset_confirm_title");
            std::wstring msg = Localization::Instance().Get(L"settings.reset_confirm_msg");
            int res = MessageBoxW(Application::Instance().MainWindow(), msg.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING);
            if (res == IDYES) {
                AppSettings def;
                def.language = Settings::Instance().Get().language; // preserve language
                Settings::Instance().Update(def);
                Settings::Instance().Save();
                m_pendingSettings = def;
                m_hasUnsavedChanges = false;
                MainWindow::Instance().OnSettingsChanged();
                Scheduler::Instance().Recalculate();
                MainWindow::Instance().Invalidate();
            }
            return true;
        }
    }

    // 6. Save Button (at bottom)
    if (m_hasUnsavedChanges) {
        float saveBtnW = 150.f * m_dpiScale;
        float saveBtnH = ds::Dims::BtnH * m_dpiScale;
        RECT rc; GetClientRect(Application::Instance().MainWindow(), &rc);
        float sx = isRtl ? (rc.left + pad) : (rc.right - pad - saveBtnW);
        float sy = (float)rc.bottom - pad - saveBtnH;
        if (x >= sx && x <= sx + saveBtnW && y >= sy && y <= sy + saveBtnH) {
            bool languageChanged = (Settings::Instance().GetCopy().language != m_pendingSettings.language);
            
            Settings::Instance().Update(m_pendingSettings);
            Settings::Instance().Save();
            m_hasUnsavedChanges = false;
            
            if (languageChanged) {
                Localization::Instance().SetLanguage(m_pendingSettings.language);
                MainWindow::Instance().OnSettingsChanged();
            }
            
            MainWindow::Instance().Invalidate();
            Scheduler::Instance().Recalculate();
            LOG_INFO("Settings saved");
            return true;
        }
    }

    return false;
}

bool SettingsPage::OnLButtonUp(int x, int y) {
    m_pressedElement = -1;
    if (m_isDraggingVolume) {
        m_isDraggingVolume = false;
        m_hasUnsavedChanges = true;
        if (m_activeTab == Tab::Sound) {
            AudioEngine::Instance().PlayReminder();
        }
    }
    return true;
}

bool SettingsPage::OnMouseMove(int x, int y) {
    int oldHover = m_hoveredElement;
    int oldTab = m_hoveredTab;
    bool oldSave = m_hoveredSave;

    float cy   = m_contentY + 40.f * m_dpiScale;
    float w    = m_contentW;
    float tabH = 36.f * m_dpiScale;
    float tabW = w / kTabCount;
    bool isRtl = Localization::Instance().IsRTL();

    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
    };

    // Tab bar hover
    if (x >= m_contentX && x <= m_contentX + w && y >= cy && y <= cy + tabH) {
        int col = (int)((x - m_contentX) / tabW);
        m_hoveredTab = isRtl ? (kTabCount - 1 - col) : col;
    } else {
        m_hoveredTab = -1;
    }

    m_hoveredElement = -1;

    float startY = m_contentY + 40*m_dpiScale + tabH + ds::Space::L*m_dpiScale * 2;
    float rowH   = 44.f * m_dpiScale;
    float fh     = ds::Dims::InputH * m_dpiScale;
    float btnH   = ds::Dims::BtnH * m_dpiScale;

    if (m_activeTab == Tab::General) {
        float fieldW = 180.f;
        float togW   = ds::Dims::ToggleW;
        float langX  = ctrlLeft(fieldW);
        if (x >= langX && x <= langX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) m_hoveredElement = 1;
        startY += rowH;
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 3;
    } else if (m_activeTab == Tab::Sound) {
        float togW = ds::Dims::ToggleW;
        float fieldW = 180.f;
        float btnW = 140.f;
        float browseW = 150.f * m_dpiScale;
        float resetW  = 36.f * m_dpiScale;
        auto& s = m_pendingSettings;

        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 11;
        startY += rowH;

        float sliderX = isRtl ? (m_contentX + 40.f * m_dpiScale + 8.f * m_dpiScale) : ctrlLeft(fieldW);
        float fwPx = fieldW * m_dpiScale;
        if (m_isDraggingVolume) {
            float t = (x - sliderX) / fwPx;
            m_pendingSettings.soundVolume = std::clamp((int)(t * 100), 0, 100);
            MainWindow::Instance().Invalidate();
        }
        if (x >= sliderX && x <= sliderX + fwPx && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 15;
        startY += rowH;

        // Custom Sound Browse / Reset Buttons Hover
        if (isRtl) {
            float curX = m_contentX;
            if (x >= curX && x <= curX + browseW && y >= startY && y <= startY + btnH) m_hoveredElement = 13;
            curX += browseW + 8.f * m_dpiScale;
            if (!s.customSoundPath.empty()) {
                if (x >= curX && x <= curX + resetW && y >= startY && y <= startY + btnH) m_hoveredElement = 14;
            }
        } else {
            float curX = m_contentX + m_contentW;
            if (!s.customSoundPath.empty()) {
                curX -= resetW;
                if (x >= curX && x <= curX + resetW && y >= startY && y <= startY + btnH) m_hoveredElement = 14;
                curX -= 8.f * m_dpiScale;
            }
            curX -= browseW;
            if (x >= curX && x <= curX + browseW && y >= startY && y <= startY + btnH) m_hoveredElement = 13;
        }
        startY += rowH + 6*m_dpiScale;

        float bX = ctrlLeft(btnW);
        if (x >= bX && x <= bX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) m_hoveredElement = 12;
    } else if (m_activeTab == Tab::Popup) {
        float fieldW = 180.f;
        float togW   = ds::Dims::ToggleW;
        startY += rowH + 28*m_dpiScale;
        float durX = ctrlLeft(fieldW);
        if (x >= durX && x <= durX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) m_hoveredElement = 21;
        startY += rowH;
        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 22;
    } else if (m_activeTab == Tab::System) {
        auto& s = m_pendingSettings;
        float togW = ds::Dims::ToggleW;
        float btnW = 180.f;

        float togX = ctrlLeft(togW);
        if (x >= togX && x <= togX + togW * m_dpiScale && y >= startY && y <= startY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 33;
        startY += rowH;

        if (s.suppressDuringFullscreen) {
            float fieldW = 220.f;
            float fX = ctrlLeft(fieldW);
            if (x >= fX && x <= fX + fieldW * m_dpiScale && y >= startY && y <= startY + fh) m_hoveredElement = 34;
            startY += rowH;
        }

        startY += rowH; // Skip version line
        float cbX = ctrlLeft(btnW);
        if (x >= cbX && x <= cbX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) m_hoveredElement = 31;
        startY += rowH;
        float resetX = ctrlLeft(btnW);
        if (x >= resetX && x <= resetX + btnW * m_dpiScale && y >= startY && y <= startY + btnH) m_hoveredElement = 32;
    }

    // Save button hover
    m_hoveredSave = false;
    if (m_hasUnsavedChanges) {
        float pad = ds::Space::XL * m_dpiScale;
        float saveBtnW = 150.f * m_dpiScale;
        float saveBtnH = ds::Dims::BtnH * m_dpiScale;
        RECT rc; GetClientRect(Application::Instance().MainWindow(), &rc);
        float sx = isRtl ? (rc.left + pad) : (rc.right - pad - saveBtnW);
        float sy = (float)rc.bottom - pad - saveBtnH;
        if (x >= sx && x <= sx + saveBtnW && y >= sy && y <= sy + saveBtnH) {
            m_hoveredSave = true;
        }
    }

    return (m_hoveredElement != oldHover || m_hoveredTab != oldTab || m_hoveredSave != oldSave);
}

void SettingsPage::OnMouseLeave() { 
    m_hoveredTab = -1;
    m_hoveredElement = -1;
    m_pressedElement = -1;
    m_isDraggingVolume = false;
    m_hoveredSave = false;
}

void SettingsPage::OnSettingsChanged() {
    if (!m_hasUnsavedChanges) {
        m_pendingSettings = Settings::Instance().GetCopy();
    }
}

void SettingsPage::OnActivated() {
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

bool SettingsPage::HitTestInteractive(int x, int y) {
    return m_hoveredTab >= 0 || m_hoveredElement >= 0 || m_hoveredSave;
}

} // namespace dhikr
