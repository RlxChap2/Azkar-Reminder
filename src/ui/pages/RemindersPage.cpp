#include "RemindersPage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include "../../core/Scheduler.h"
#include "../../core/Logger.h"
#include "../../core/Localization.h"
#include "../components/TimePickerPopup.h"
#include <algorithm>
#include <sstream>

namespace dhikr {

RemindersPage::RemindersPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(L"Noto Sans", nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, L"en-US", out);
    };
    mk(ds::FontSize::PageTitle,  DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfTitle);
    mk(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_REGULAR,   &m_tfLabel);
    mk(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_REGULAR,   &m_tfValue);
    mk(ds::FontSize::BtnLabel,   DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);
    
    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfValue) m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    
    if (m_tfBtn) m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (m_tfBtn) m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

RemindersPage::~RemindersPage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfLabel) { m_tfLabel->Release(); m_tfLabel = nullptr; }
    if (m_tfValue) { m_tfValue->Release(); m_tfValue = nullptr; }
    if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void RemindersPage::DrawSectionLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text) {
    if (!m_tfLabel || !m_brush) return;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(ds::TextSecondary());
    m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    D2D1_RECT_F r = isRtl
        ? D2D1::RectF(m_contentX + 180.f * m_dpiScale, y, m_contentX + m_contentW, y + 20.f * m_dpiScale)
        : D2D1::RectF(m_contentX, y, m_contentX + m_contentW - 180.f * m_dpiScale, y + 20.f * m_dpiScale);
    rt->DrawTextW(text, (UINT32)wcslen(text), m_tfLabel, r, m_brush);
}

void RemindersPage::DrawSeparator(ID2D1RenderTarget* rt, float x, float y, float w) {
    m_brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), m_brush, 0.5f);
}

void RemindersPage::DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover) {
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float kd = ds::Dims::ToggleKnobD * m_dpiScale;
    float rx = th / 2.f;

    auto trackColor = on ? ds::Accent::Primary() : ds::Border();
    if (hover && !on) { trackColor.r += 0.05f; trackColor.g += 0.05f; }
    m_brush->SetColor(trackColor);
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x+tw, y+th), rx, rx), m_brush);

    float kx = on ? (x + tw - kd - (th-kd)/2.f) : (x + (th-kd)/2.f);
    float ky = y + (th - kd) / 2.f;
    m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF));
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx + kd/2.f, ky + kd/2.f), kd/2.f, kd/2.f), m_brush);
}

void RemindersPage::DrawTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* timeStr) {
    float rx = ds::Dims::InputRadius * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = isRtl
            ? D2D1::RectF(r.left + 28*m_dpiScale, r.top, r.right - 10*m_dpiScale, r.bottom)
            : D2D1::RectF(r.left + 10*m_dpiScale, r.top, r.right - 28*m_dpiScale, r.bottom);
        rt->DrawTextW(timeStr, (UINT32)wcslen(timeStr), m_tfValue, tr, m_brush);
    }
    // Clock icon hint
    m_brush->SetColor(ds::TextMuted());
    float cr = 5.f * m_dpiScale;
    float cx = isRtl ? (r.left + 16.f * m_dpiScale) : (r.right - 16.f * m_dpiScale);
    float cy = (r.top + r.bottom) / 2.f;
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), cr, cr), m_brush, 1.f);
    rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx, cy - cr*0.7f), m_brush, 1.f);
    rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx + cr*0.5f, cy), m_brush, 1.f);
}

void RemindersPage::DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* value) {
    float rx = ds::Dims::InputRadius * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = isRtl
            ? D2D1::RectF(r.left + 24*m_dpiScale, r.top, r.right - 10*m_dpiScale, r.bottom)
            : D2D1::RectF(r.left + 10*m_dpiScale, r.top, r.right - 24*m_dpiScale, r.bottom);
        rt->DrawTextW(value, (UINT32)wcslen(value), m_tfValue, tr, m_brush);
    }
    // Chevron
    m_brush->SetColor(ds::TextMuted());
    float cx = isRtl ? (r.left + 14 * m_dpiScale) : (r.right - 14 * m_dpiScale);
    float cy = (r.top + r.bottom) / 2.f;
    rt->DrawLine(D2D1::Point2F(cx-4*m_dpiScale, cy-2*m_dpiScale), D2D1::Point2F(cx, cy+2*m_dpiScale), m_brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx, cy+2*m_dpiScale), D2D1::Point2F(cx+4*m_dpiScale, cy-2*m_dpiScale), m_brush, 1.5f);
}

void RemindersPage::DrawSaveButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    m_brush->SetColor(m_hoverSave ? ds::Accent::Dark() : ds::Accent::Primary());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0x08, 0x0A, 0x09));
        std::wstring str = Localization::Instance().Get(m_hasUnsavedChanges ? L"reminders.save_star" : L"reminders.save");
        rt->DrawTextW(str.c_str(), (UINT32)str.size(), m_tfBtn, r, m_brush);
    }
}

void RemindersPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    auto& s = m_pendingSettings;
    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;
    bool isRtl = Localization::Instance().IsRTL();
    m_contentX = x; m_contentY = y; m_contentW = w;

    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // Page title
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = D2D1::RectF(x, y, x+w, y+28*m_dpiScale);
        std::wstring title = Localization::Instance().Get(L"nav.reminders");
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, tr, m_brush);
    }
    y += 40.f * m_dpiScale;
    DrawSeparator(rt, x, y, w);
    y += ds::Space::L * m_dpiScale;

    // Enable Reminders toggle
    DrawSectionLabel(rt, x, y + 4*m_dpiScale, Localization::Instance().Get(L"reminders.enable").c_str());
    float togX = ctrlLeft(ds::Dims::ToggleW);
    DrawToggle(rt, togX, y, s.remindersEnabled, m_hoverToggle);
    y += 36.f * m_dpiScale;

    // Start Time
    float fieldW = 140.f;
    float fieldH = ds::Dims::InputH * m_dpiScale;
    DrawSectionLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"reminders.start_time").c_str());
    wchar_t startBuf[16]; auto& sd = s.weeklySchedule[0];
    swprintf_s(startBuf, L"%02d:%02d %s", sd.startHour % 12 == 0 ? 12 : sd.startHour % 12,
               sd.startMin, sd.startHour < 12 ? L"AM" : L"PM");
    float fX = ctrlLeft(fieldW);
    DrawTimeField(rt, D2D1::RectF(fX, y, fX + fieldW * m_dpiScale, y + fieldH), startBuf);
    y += fieldH + ds::Space::M * m_dpiScale;

    // End Time
    DrawSectionLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"reminders.end_time").c_str());
    wchar_t endBuf[16];
    swprintf_s(endBuf, L"%02d:%02d %s", sd.endHour % 12 == 0 ? 12 : sd.endHour % 12,
               sd.endMin, sd.endHour < 12 ? L"AM" : L"PM");
    fX = ctrlLeft(fieldW);
    DrawTimeField(rt, D2D1::RectF(fX, y, fX + fieldW * m_dpiScale, y + fieldH), endBuf);
    y += fieldH + ds::Space::M * m_dpiScale;

    // Frequency
    float freqW = 168.f;
    DrawSectionLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"reminders.frequency").c_str());
    std::wstring min15 = L"15 " + Localization::Instance().Get(L"home.minutes");
    std::wstring min30 = L"30 " + Localization::Instance().Get(L"home.minutes");
    std::wstring min45 = L"45 " + Localization::Instance().Get(L"home.minutes");
    std::wstring hr1   = L"60 " + Localization::Instance().Get(L"home.minutes");
    std::wstring hr2   = L"120 " + Localization::Instance().Get(L"home.minutes");
    std::wstring custom= L"Custom";
    const wchar_t* kFreqLabels[] = { min15.c_str(), min30.c_str(), min45.c_str(), hr1.c_str(), hr2.c_str(), custom.c_str() };
    int freqIdx = 1;
    int freq = sd.frequencyMinutes;
    if (freq == 15) freqIdx = 0;
    else if (freq == 30) freqIdx = 1;
    else if (freq == 45) freqIdx = 2;
    else if (freq == 60) freqIdx = 3;
    else if (freq == 120) freqIdx = 4;
    else freqIdx = 5;
    float ddX = ctrlLeft(freqW);
    DrawDropdown(rt, D2D1::RectF(ddX, y, ddX + freqW * m_dpiScale, y + fieldH), kFreqLabels[freqIdx]);
    y += fieldH + ds::Space::M * m_dpiScale;

    // Quiet Hours
    float qhW = 180.f;
    DrawSectionLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"reminders.quiet_hours").c_str());
    std::wstring qhStr = s.quietPeriods.empty() ? L"—" :
        [&]() -> std::wstring {
            auto& qp = s.quietPeriods[0];
            wchar_t buf[32];
            swprintf_s(buf, L"%02d:%02d – %02d:%02d", 
                qp.startHour % 12 == 0 ? 12 : qp.startHour % 12, qp.startMin,
                qp.endHour % 12 == 0 ? 12 : qp.endHour % 12, qp.endMin);
            return buf;
        }();
    float fX2 = ctrlLeft(qhW);
    DrawTimeField(rt, D2D1::RectF(fX2, y, fX2 + qhW * m_dpiScale, y + fieldH), qhStr.c_str());
    y += fieldH + ds::Space::M * m_dpiScale;

    // Pause For
    float pauseW = 168.f;
    DrawSectionLabel(rt, x, y + 6*m_dpiScale, Localization::Instance().Get(L"reminders.pause_for").c_str());
    std::wstring pauseStr = Scheduler::Instance().IsPaused() ? Localization::Instance().Get(L"home.paused") : L"...";
    ddX = ctrlLeft(pauseW);
    DrawDropdown(rt, D2D1::RectF(ddX, y, ddX + pauseW * m_dpiScale, y + fieldH), pauseStr.c_str());
    y += fieldH + ds::Space::XL * m_dpiScale;

    // Action buttons row: Preview button (secondary) and Save button (accent)
    float previewW = 180.f * m_dpiScale;
    float prX = isRtl ? (x + w - previewW) : x;
    D2D1_RECT_F previewR = D2D1::RectF(prX, y, prX + previewW, y + ds::Dims::BtnH * m_dpiScale);
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(previewR, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(previewR, rx, rx), m_brush, 1.f);
    if (m_tfBtn) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring btnLabel = Localization::Instance().Get(L"reminders.preview");
        rt->DrawTextW(btnLabel.c_str(), (UINT32)btnLabel.size(), m_tfBtn, previewR, m_brush);
    }

    float btnW = 150.f * m_dpiScale;
    float svX = isRtl ? x : (x + w - btnW);
    DrawSaveButton(rt, D2D1::RectF(svX, y, svX + btnW, y + ds::Dims::BtnH * m_dpiScale));
}

D2D1_RECT_F RemindersPage::GetToggleRect() const {
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float y  = m_contentY + 40*m_dpiScale + ds::Space::L*m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    float tX = isRtl ? m_contentX : (m_contentX + m_contentW - tw);
    return D2D1::RectF(tX, y, tX + tw, y + th);
}

D2D1_RECT_F RemindersPage::GetSaveRect() const {
    float btnW = 150.f * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float y = m_contentY + 40*m_dpiScale + ds::Space::L*m_dpiScale + 36.f*m_dpiScale +
              (ds::Dims::InputH*m_dpiScale + ds::Space::M*m_dpiScale) * 4 + 
              ds::Dims::InputH*m_dpiScale + ds::Space::XL*m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    float sX = isRtl ? m_contentX : (m_contentX + m_contentW - btnW);
    return D2D1::RectF(sX, y, sX + btnW, y + btnH);
}

bool RemindersPage::OnLButtonDown(int x, int y) {
    bool isRtl = Localization::Instance().IsRTL();
    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
    };

    float cy = m_contentY + 40.f * m_dpiScale + ds::Space::L * m_dpiScale;
    
    // Toggle
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float togX = ctrlLeft(ds::Dims::ToggleW);
    if (x >= togX && x <= togX + tw && y >= cy && y <= cy + th) {
        m_pendingSettings.remindersEnabled = !m_pendingSettings.remindersEnabled;
        m_hasUnsavedChanges = true;
        MainWindow::Instance().Invalidate();
        return true;
    }
    
    cy += 36.f * m_dpiScale;
    float fieldW = 140.f;
    float fieldH = ds::Dims::InputH * m_dpiScale;
    
    // Start Time
    float stX = ctrlLeft(fieldW);
    if (x >= stX && x <= stX + fieldW * m_dpiScale && y >= cy && y <= cy + fieldH) {
        POINT pt{ (LONG)stX, (LONG)(cy + fieldH) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);
        TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, 
            m_pendingSettings.weeklySchedule[0].startHour, 
            m_pendingSettings.weeklySchedule[0].startMin, 
            [this](int h, int m) {
                for (int i=0; i<7; ++i) {
                    m_pendingSettings.weeklySchedule[i].startHour = h;
                    m_pendingSettings.weeklySchedule[i].startMin = m;
                }
                m_hasUnsavedChanges = true;
                MainWindow::Instance().Invalidate();
            });
        return true;
    }
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // End Time
    float etX = ctrlLeft(fieldW);
    if (x >= etX && x <= etX + fieldW * m_dpiScale && y >= cy && y <= cy + fieldH) {
        POINT pt{ (LONG)etX, (LONG)(cy + fieldH) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);
        TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, 
            m_pendingSettings.weeklySchedule[0].endHour, 
            m_pendingSettings.weeklySchedule[0].endMin, 
            [this](int h, int m) {
                for (int i=0; i<7; ++i) {
                    m_pendingSettings.weeklySchedule[i].endHour = h;
                    m_pendingSettings.weeklySchedule[i].endMin = m;
                }
                m_hasUnsavedChanges = true;
                MainWindow::Instance().Invalidate();
            });
        return true;
    }
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Frequency
    float freqW = 168.f;
    float frX = ctrlLeft(freqW);
    if (x >= frX && x <= frX + freqW * m_dpiScale && y >= cy && y <= cy + fieldH) {
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1, L"15 min");
        AppendMenuW(hMenu, MF_STRING, 2, L"30 min");
        AppendMenuW(hMenu, MF_STRING, 3, L"45 min");
        AppendMenuW(hMenu, MF_STRING, 4, L"60 min");
        AppendMenuW(hMenu, MF_STRING, 5, L"120 min");
        POINT pt{ (LONG)frX, (LONG)(cy + fieldH) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
        DestroyMenu(hMenu);
        if (cmd > 0) {
            int freqs[] = { 15, 30, 45, 60, 120 };
            int freq = freqs[cmd - 1];
            for (int i=0; i<7; ++i) m_pendingSettings.weeklySchedule[i].frequencyMinutes = freq;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
        }
        return true;
    }
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Quiet Hours
    float qhW = 180.f;
    float qhX = ctrlLeft(qhW);
    if (x >= qhX && x <= qhX + qhW * m_dpiScale && y >= cy && y <= cy + fieldH) {
        HMENU hMenu = CreatePopupMenu();
        for (int i = 0; i < 24; ++i) {
            wchar_t buf[64];
            int e = (i + 8) % 24;
            swprintf_s(buf, L"%02d:00 %s - %02d:00 %s", 
                i % 12 == 0 ? 12 : i % 12, i < 12 ? L"AM" : L"PM",
                e % 12 == 0 ? 12 : e % 12, e < 12 ? L"AM" : L"PM");
            AppendMenuW(hMenu, MF_STRING, i + 1, buf);
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 100, L"Disable Quiet Hours");
        
        POINT pt{ (LONG)qhX, (LONG)(cy + fieldH) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
        DestroyMenu(hMenu);
        
        if (cmd == 100) {
            m_pendingSettings.quietPeriods.clear();
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
        } else if (cmd > 0) {
            int startHour = cmd - 1;
            int endHour = (startHour + 8) % 24;
            m_pendingSettings.quietPeriods.clear();
            m_pendingSettings.quietPeriods.push_back({startHour, 0, endHour, 0});
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
        }
        return true;
    }
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Pause For
    float pauseW = 168.f;
    float psX = ctrlLeft(pauseW);
    if (x >= psX && x <= psX + pauseW * m_dpiScale && y >= cy && y <= cy + fieldH) {
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1, L"15 minutes");
        AppendMenuW(hMenu, MF_STRING, 2, L"30 minutes");
        AppendMenuW(hMenu, MF_STRING, 3, L"1 hour");
        AppendMenuW(hMenu, MF_STRING, 4, L"2 hours");
        AppendMenuW(hMenu, MF_STRING, 5, L"Until Tomorrow");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 100, L"Resume");
        
        POINT pt{ (LONG)psX, (LONG)(cy + fieldH) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
        DestroyMenu(hMenu);
        
        if (cmd > 0) {
            if (cmd == 100) {
                Scheduler::Instance().Resume();
            } else if (cmd == 5) {
                Scheduler::Instance().PauseUntilTomorrow();
            } else {
                int mins[] = { 15, 30, 60, 120 };
                Scheduler::Instance().Pause(mins[cmd - 1]);
            }
            m_pendingSettings = Settings::Instance().GetCopy();
            MainWindow::Instance().Invalidate();
        }
        return true;
    }
    cy += fieldH + ds::Space::XL * m_dpiScale;
    
    // Preview Button
    float previewW = 180.f * m_dpiScale;
    float prX = isRtl ? (m_contentX + m_contentW - previewW) : m_contentX;
    if (x >= prX && x <= prX + previewW && y >= cy && y <= cy + ds::Dims::BtnH * m_dpiScale) {
        Scheduler::Instance().TriggerNow();
        return true;
    }

    // Save Button
    auto saveR = GetSaveRect();
    if (x >= saveR.left && x <= saveR.right && y >= saveR.top && y <= saveR.bottom) {
        if (m_hasUnsavedChanges) {
            Settings::Instance().Update(m_pendingSettings);
            Settings::Instance().Save();
            m_hasUnsavedChanges = false;
            MainWindow::Instance().OnSettingsChanged();
            Scheduler::Instance().Recalculate();
            MainWindow::Instance().Invalidate();
            LOG_INFO("Reminder settings saved");
        }
        return true;
    }
    return false;
}

bool RemindersPage::OnMouseMove(int x, int y) {
    auto tr = GetToggleRect(); 
    m_hoverToggle = (x >= tr.left && x <= tr.right && y >= tr.top && y <= tr.bottom);
    auto sr = GetSaveRect();   
    m_hoverSave   = (x >= sr.left && x <= sr.right && y >= sr.top && y <= sr.bottom);
    return false;
}

void RemindersPage::OnMouseLeave() { 
    m_hoverToggle = false; 
    m_hoverSave   = false; 
}

void RemindersPage::OnSettingsChanged() {
    if (!m_hasUnsavedChanges) {
        m_pendingSettings = Settings::Instance().GetCopy();
    }
}

void RemindersPage::OnActivated() {
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

bool RemindersPage::HitTestInteractive(int x, int y) {
    bool isRtl = Localization::Instance().IsRTL();
    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
    };

    float cy = m_contentY + 40.f * m_dpiScale + ds::Space::L * m_dpiScale;
    
    // Toggle
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float togX = ctrlLeft(ds::Dims::ToggleW);
    if (x >= togX && x <= togX + tw && y >= cy && y <= cy + th) return true;
    
    cy += 36.f * m_dpiScale;
    float fieldW = 140.f;
    float fieldH = ds::Dims::InputH * m_dpiScale;
    
    // Start Time
    float stX = ctrlLeft(fieldW);
    if (x >= stX && x <= stX + fieldW * m_dpiScale && y >= cy && y <= cy + fieldH) return true;
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // End Time
    float etX = ctrlLeft(fieldW);
    if (x >= etX && x <= etX + fieldW * m_dpiScale && y >= cy && y <= cy + fieldH) return true;
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Frequency
    float freqW = 168.f;
    float frX = ctrlLeft(freqW);
    if (x >= frX && x <= frX + freqW * m_dpiScale && y >= cy && y <= cy + fieldH) return true;
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Quiet Hours
    float qhW = 180.f;
    float qhX = ctrlLeft(qhW);
    if (x >= qhX && x <= qhX + qhW * m_dpiScale && y >= cy && y <= cy + fieldH) return true;
    cy += fieldH + ds::Space::M * m_dpiScale;
    
    // Pause For
    float pauseW = 168.f;
    float psX = ctrlLeft(pauseW);
    if (x >= psX && x <= psX + pauseW * m_dpiScale && y >= cy && y <= cy + fieldH) return true;
    cy += fieldH + ds::Space::XL * m_dpiScale;
    
    // Preview Button
    float previewW = 180.f * m_dpiScale;
    float prX = isRtl ? (m_contentX + m_contentW - previewW) : m_contentX;
    if (x >= prX && x <= prX + previewW && y >= cy && y <= cy + ds::Dims::BtnH * m_dpiScale) return true;

    // Save Button
    auto saveR = GetSaveRect();
    if (x >= saveR.left && x <= saveR.right && y >= saveR.top && y <= saveR.bottom) return true;

    return false;
}

} // namespace dhikr
