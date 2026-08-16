#include "SchedulePage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include "../../core/Scheduler.h"
#include "../../core/Logger.h"
#include "../components/TimePickerPopup.h"
#include "../../core/Localization.h"

namespace dhikr {

SchedulePage::SchedulePage() {
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
    mk(ds::FontSize::InputText,  DWRITE_FONT_WEIGHT_REGULAR,   &m_tfValue);
    mk(ds::FontSize::BtnLabel,   DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);
    mk(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfDay);

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfValue) m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfDay) m_tfDay->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    if (m_tfBtn) { m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

SchedulePage::~SchedulePage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfLabel) { m_tfLabel->Release(); m_tfLabel = nullptr; }
    if (m_tfValue) { m_tfValue->Release(); m_tfValue = nullptr; }
    if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
    if (m_tfDay)   { m_tfDay->Release();   m_tfDay   = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void SchedulePage::DrawRowToggle(ID2D1RenderTarget* rt, float cx, float cy, bool on) {
    float tw = 34.f * m_dpiScale;
    float th = 20.f * m_dpiScale;
    float kd = 15.f * m_dpiScale;
    float rx  = th / 2.f;
    m_brush->SetColor(on ? ds::Accent::Primary() : ds::Border());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx, cy, cx+tw, cy+th), rx, rx), m_brush);
    float kx = on ? (cx+tw-kd-(th-kd)/2.f) : (cx+(th-kd)/2.f);
    float ky = cy + (th-kd)/2.f;
    m_brush->SetColor(ds::Rgba(0xFF,0xFF,0xFF));
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx+kd/2.f, ky+kd/2.f), kd/2.f, kd/2.f), m_brush);
}

void SchedulePage::DrawSmallTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, int h, int m) {
    float rx = ds::Dims::InputRadius * m_dpiScale;
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);
    if (m_tfValue) {
        wchar_t buf[12];
        swprintf_s(buf, L"%02d:%02d %s", h%12==0?12:h%12, m, h<12?L"AM":L"PM");
        m_brush->SetColor(ds::TextPrimary());
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = D2D1::RectF(r.left+6*m_dpiScale, r.top, r.right-4*m_dpiScale, r.bottom);
        rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue, tr, m_brush);
    }
}

void SchedulePage::DrawWeekRow(ID2D1RenderTarget* rt, int dayIndex, float x, float y, float w) {
    auto& s = m_pendingSettings;
    auto& ds_ref = s.weeklySchedule[dayIndex];
    bool active = dayIndex == m_hoveredRow;
    bool isRtl = Localization::Instance().IsRTL();

    // Row hover background
    if (active) {
        m_brush->SetColor(ds::CardHover());
        rt->FillRectangle(D2D1::RectF(x, y, x+w, y+36*m_dpiScale), m_brush);
    }

    // Row separator
    m_brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(x, y+36*m_dpiScale), D2D1::Point2F(x+w, y+36*m_dpiScale), m_brush, 0.5f);

    float cy = y + 8*m_dpiScale;
    float fh = 28.f * m_dpiScale;
    float fw = 90.f * m_dpiScale;
    auto dayColor = ds_ref.enabled ? ds::TextPrimary() : ds::TextMuted();

    if (isRtl) {
        float cx = x + w - 44*m_dpiScale;
        // Toggle
        DrawRowToggle(rt, cx, cy, ds_ref.enabled);
        cx -= 100*m_dpiScale;

        // Day name
        m_brush->SetColor(dayColor);
        if (m_tfDay) {
            m_tfDay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            D2D1_RECT_F dr = D2D1::RectF(cx, y+4*m_dpiScale, cx+90*m_dpiScale, y+36*m_dpiScale);
            const wchar_t* dkeys[] = { L"schedule.sun", L"schedule.mon", L"schedule.tue", L"schedule.wed", L"schedule.thu", L"schedule.fri", L"schedule.sat" };
            std::wstring dstr = Localization::Instance().Get(dkeys[dayIndex]);
            rt->DrawTextW(dstr.c_str(), (UINT32)dstr.size(), m_tfDay, dr, m_brush);
        }
        cx -= (fw + 8*m_dpiScale);

        // Start time
        DrawSmallTimeField(rt, D2D1::RectF(cx, cy, cx+fw, cy+fh), ds_ref.startHour, ds_ref.startMin);
        cx -= (fw + 8*m_dpiScale);

        // End time
        DrawSmallTimeField(rt, D2D1::RectF(cx, cy, cx+fw, cy+fh), ds_ref.endHour, ds_ref.endMin);
        cx -= (80.f*m_dpiScale + 8*m_dpiScale);

        // Frequency label
        static const wchar_t* freqNames[] = { L"15 min", L"30 min", L"45 min", L"60 min", L"2h", L"Custom" };
        int fi = 1;
        if (ds_ref.frequencyMinutes == 15) fi=0;
        else if (ds_ref.frequencyMinutes == 30) fi=1;
        else if (ds_ref.frequencyMinutes == 45) fi=2;
        else if (ds_ref.frequencyMinutes == 60) fi=3;
        else if (ds_ref.frequencyMinutes == 120) fi=4;
        else fi=5;

        bool fHover = (m_hoveredElement == 30 + dayIndex);
        m_brush->SetColor(fHover ? ds::CardHover() : ds::Surface());
        float ddW = 80.f * m_dpiScale;
        D2D1_RECT_F ddR = D2D1::RectF(cx, cy, cx+ddW, cy+fh);
        rt->FillRoundedRectangle(D2D1::RoundedRect(ddR, 4*m_dpiScale, 4*m_dpiScale), m_brush);
        m_brush->SetColor(ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(ddR, 4*m_dpiScale, 4*m_dpiScale), m_brush, 0.5f);
        m_brush->SetColor(dayColor);
        if (m_tfValue) {
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawTextW(freqNames[fi], (UINT32)wcslen(freqNames[fi]), m_tfValue, ddR, m_brush);
        }

        // "..." dots button
        cx -= 24*m_dpiScale;
        bool mHover = (m_hoveredElement == 40 + dayIndex);
        m_brush->SetColor(mHover ? ds::TextPrimary() : ds::TextMuted());
        for (int i = 0; i < 3; ++i) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + i*5*m_dpiScale, cy + fh/2.f), 1.5f*m_dpiScale, 1.5f*m_dpiScale), m_brush);
        }
    } else {
        float cx = x + 10*m_dpiScale;
        // Toggle
        DrawRowToggle(rt, cx, cy, ds_ref.enabled);
        cx += 44*m_dpiScale;

        // Day name
        m_brush->SetColor(dayColor);
        if (m_tfDay) {
            m_tfDay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F dr = D2D1::RectF(cx, y+4*m_dpiScale, cx+90*m_dpiScale, y+36*m_dpiScale);
            const wchar_t* dkeys[] = { L"schedule.sun", L"schedule.mon", L"schedule.tue", L"schedule.wed", L"schedule.thu", L"schedule.fri", L"schedule.sat" };
            std::wstring dstr = Localization::Instance().Get(dkeys[dayIndex]);
            rt->DrawTextW(dstr.c_str(), (UINT32)dstr.size(), m_tfDay, dr, m_brush);
        }
        cx += 100*m_dpiScale;

        // Start time
        DrawSmallTimeField(rt, D2D1::RectF(cx, cy, cx+fw, cy+fh), ds_ref.startHour, ds_ref.startMin);
        cx += fw + 8*m_dpiScale;

        // End time
        DrawSmallTimeField(rt, D2D1::RectF(cx, cy, cx+fw, cy+fh), ds_ref.endHour, ds_ref.endMin);
        cx += fw + 8*m_dpiScale;

        // Frequency label
        static const wchar_t* freqNames[] = { L"15 min", L"30 min", L"45 min", L"60 min", L"2h", L"Custom" };
        int fi = 1;
        if (ds_ref.frequencyMinutes == 15) fi=0;
        else if (ds_ref.frequencyMinutes == 30) fi=1;
        else if (ds_ref.frequencyMinutes == 45) fi=2;
        else if (ds_ref.frequencyMinutes == 60) fi=3;
        else if (ds_ref.frequencyMinutes == 120) fi=4;
        else fi=5;

        bool fHover = (m_hoveredElement == 30 + dayIndex);
        m_brush->SetColor(fHover ? ds::CardHover() : ds::Surface());
        float ddW = 80.f * m_dpiScale;
        D2D1_RECT_F ddR = D2D1::RectF(cx, cy, cx+ddW, cy+fh);
        rt->FillRoundedRectangle(D2D1::RoundedRect(ddR, 4*m_dpiScale, 4*m_dpiScale), m_brush);
        m_brush->SetColor(ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(ddR, 4*m_dpiScale, 4*m_dpiScale), m_brush, 0.5f);
        m_brush->SetColor(dayColor);
        if (m_tfValue) {
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F lr = D2D1::RectF(cx+6*m_dpiScale, cy, cx+ddW-6*m_dpiScale, cy+fh);
            rt->DrawTextW(freqNames[fi], (UINT32)wcslen(freqNames[fi]), m_tfValue, lr, m_brush);
        }

        // "..." dots button
        cx += ddW + 8*m_dpiScale;
        bool mHover = (m_hoveredElement == 40 + dayIndex);
        m_brush->SetColor(mHover ? ds::TextPrimary() : ds::TextMuted());
        for (int i = 0; i < 3; ++i) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + i*5*m_dpiScale, cy + fh/2.f), 1.5f*m_dpiScale, 1.5f*m_dpiScale), m_brush);
        }
    }
}

void SchedulePage::DrawAddQuietHoursBtn(ID2D1RenderTarget* rt, float x, float y) {
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float btnW = 180.f * m_dpiScale;
    float rx   = ds::Dims::BtnRadius * m_dpiScale;

    bool hover = (m_hoveredElement == 100);
    bool pressed = (m_pressedElement == 100);

    m_brush->SetColor(pressed ? ds::Card() : (hover ? ds::CardHover() : ds::Surface()));
    D2D1_RECT_F r = D2D1::RectF(x, y, x+btnW, y+btnH);
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Accent::Primary());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 1.f);

    // "+" icon
    float cx = x + 16*m_dpiScale, cy = y + btnH/2.f;
    rt->DrawLine(D2D1::Point2F(cx-4*m_dpiScale,cy), D2D1::Point2F(cx+4*m_dpiScale,cy), m_brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx,cy-4*m_dpiScale), D2D1::Point2F(cx,cy+4*m_dpiScale), m_brush, 1.5f);

    if (m_tfBtn) {
        m_brush->SetColor(ds::Accent::Primary());
        D2D1_RECT_F lr = D2D1::RectF(x+24*m_dpiScale, y, x+btnW, y+btnH);
        std::wstring lbl = Localization::Instance().Get(L"schedule.add_quiet_hours");
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfBtn, lr, m_brush);
    }
}

void SchedulePage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfValue) m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfDay) m_tfDay->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    // Title
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring title = Localization::Instance().Get(L"schedule.title");
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, D2D1::RectF(x,y,x+w,y+28*m_dpiScale), m_brush);
    }
    y += 40.f * m_dpiScale;

    // Column headers
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(ds::TextSecondary());
        std::wstring h1 = Localization::Instance().Get(L"reminders.start_time");
        std::wstring h2 = Localization::Instance().Get(L"reminders.end_time");
        std::wstring h3 = Localization::Instance().Get(L"reminders.frequency");
        const wchar_t* headers[] = { h1.c_str(), h2.c_str(), h3.c_str() };
        
        if (isRtl) {
            float cx = x + w - 44*m_dpiScale - 100*m_dpiScale - 98*m_dpiScale;
            float offsets[] = { 0, -98*m_dpiScale, -186*m_dpiScale };
            for (int i = 0; i < 3; ++i) {
                D2D1_RECT_F hr = D2D1::RectF(cx+offsets[i], y, cx+offsets[i]+90*m_dpiScale, y+16*m_dpiScale);
                rt->DrawTextW(headers[i], (UINT32)wcslen(headers[i]), m_tfLabel, hr, m_brush);
            }
        } else {
            float cx = x + 10*m_dpiScale + 44*m_dpiScale + 100*m_dpiScale;
            float offsets[] = { 0, 98*m_dpiScale, 198*m_dpiScale };
            for (int i = 0; i < 3; ++i) {
                D2D1_RECT_F hr = D2D1::RectF(cx+offsets[i], y, cx+offsets[i]+90*m_dpiScale, y+16*m_dpiScale);
                rt->DrawTextW(headers[i], (UINT32)wcslen(headers[i]), m_tfLabel, hr, m_brush);
            }
        }
    }
    y += 20.f * m_dpiScale;
    
    m_contentX = x;
    m_contentY = y;
    m_contentW = w;
    m_rowH = 36.f * m_dpiScale;

    // Weekly rows
    for (int i = 0; i < 7; ++i) {
        DrawWeekRow(rt, i, x, y + i * m_rowH, w);
    }

    y += 7 * m_rowH + ds::Space::L * m_dpiScale;
    
    // Add quiet hours button (left side)
    float qhBtnX = isRtl ? (x + w - 180.f * m_dpiScale) : x;
    DrawAddQuietHoursBtn(rt, qhBtnX, y);
    
    // Save button (right side)
    float btnW = 150.f * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float saveBtnX = isRtl ? x : (x + w - btnW);
    D2D1_RECT_F saveR = D2D1::RectF(saveBtnX, y, saveBtnX + btnW, y + btnH);
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    m_brush->SetColor(m_hoveredSave ? ds::Accent::Dark() : ds::Accent::Primary());
    rt->FillRoundedRectangle(D2D1::RoundedRect(saveR, rx, rx), m_brush);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0x08, 0x0A, 0x09));
        std::wstring sv = Localization::Instance().Get(m_hasUnsavedChanges ? L"reminders.save_star" : L"reminders.save");
        rt->DrawTextW(sv.c_str(), (UINT32)sv.size(), m_tfBtn, saveR, m_brush);
    }
}

bool SchedulePage::OnLButtonDown(int x, int y) {
    if (m_hoveredElement >= 0) {
        m_pressedElement = m_hoveredElement;
        
        // Execute immediate actions
        if (m_hoveredElement >= 0 && m_hoveredElement < 7) {
            // Toggle day
            m_pendingSettings.weeklySchedule[m_hoveredElement].enabled = !m_pendingSettings.weeklySchedule[m_hoveredElement].enabled;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        } else if (m_hoveredElement >= 10 && m_hoveredElement < 17) {
            // Start time
            int day = m_hoveredElement - 10;
            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                m_pendingSettings.weeklySchedule[day].startHour,
                m_pendingSettings.weeklySchedule[day].startMin,
                [this, day](int h, int m) {
                    m_pendingSettings.weeklySchedule[day].startHour = h;
                    m_pendingSettings.weeklySchedule[day].startMin = m;
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                });
            return true;
        } else if (m_hoveredElement >= 20 && m_hoveredElement < 27) {
            // End time
            int day = m_hoveredElement - 20;
            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                m_pendingSettings.weeklySchedule[day].endHour,
                m_pendingSettings.weeklySchedule[day].endMin,
                [this, day](int h, int m) {
                    m_pendingSettings.weeklySchedule[day].endHour = h;
                    m_pendingSettings.weeklySchedule[day].endMin = m;
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                });
            return true;
        } else if (m_hoveredElement >= 30 && m_hoveredElement < 37) {
            // Frequency
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"15 min");
            AppendMenuW(hMenu, MF_STRING, 2, L"30 min");
            AppendMenuW(hMenu, MF_STRING, 3, L"45 min");
            AppendMenuW(hMenu, MF_STRING, 4, L"60 min");
            AppendMenuW(hMenu, MF_STRING, 5, L"2h");
            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, Application::Instance().MainWindow(), nullptr);
            DestroyMenu(hMenu);
            if (cmd > 0) {
                int freqs[] = { 15, 30, 45, 60, 120 };
                m_pendingSettings.weeklySchedule[m_hoveredElement - 30].frequencyMinutes = freqs[cmd - 1];
                m_hasUnsavedChanges = true;
                MainWindow::Instance().Invalidate();
            }
            return true;
        } else if (m_hoveredElement == 100) {
            // Add Quiet Hours
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
            
            POINT pt{ x, y + 20 };
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
    }
    
    // Save button click
    if (m_hoveredSave && m_hasUnsavedChanges) {
        Settings::Instance().Update(m_pendingSettings);
        Settings::Instance().Save();
        m_hasUnsavedChanges = false;
        MainWindow::Instance().OnSettingsChanged();
        Scheduler::Instance().Recalculate();
        MainWindow::Instance().Invalidate();
        LOG_INFO("Schedule settings saved");
        return true;
    }
    
    return false;
}

bool SchedulePage::OnLButtonUp(int x, int y) {
    m_pressedElement = -1;
    return false;
}

bool SchedulePage::OnMouseMove(int x, int y) {
    float startY = m_contentY;
    float endY = m_contentY + 7 * m_rowH;
    int oldElem = m_hoveredElement;
    int oldRow  = m_hoveredRow;
    bool oldSave = m_hoveredSave;

    m_hoveredElement = -1;
    m_hoveredRow = -1;
    m_hoveredSave = false;

    if (x >= m_contentX && x <= m_contentX + m_contentW && y >= startY && y < endY) {
        int row = (int)((y - startY) / m_rowH);
        if (row >= 0 && row < 7) {
            m_hoveredRow = row;
            float rowY = startY + row * m_rowH;
            float cy = rowY + 8*m_dpiScale;
            float fw = 90.f * m_dpiScale;
            float fh = 28.f * m_dpiScale;
            bool isRtl = Localization::Instance().IsRTL();

            if (isRtl) {
                float cx = m_contentX + m_contentW - 44*m_dpiScale;
                if (x >= cx && x <= cx + 34.f * m_dpiScale && y >= cy && y <= cy + 20.f * m_dpiScale) {
                    m_hoveredElement = row; // toggle
                }
                cx -= 100*m_dpiScale;
                cx -= (fw + 8*m_dpiScale);
                if (x >= cx && x <= cx + fw && y >= cy && y <= cy + fh) m_hoveredElement = 10 + row; // start time
                cx -= (fw + 8*m_dpiScale);
                if (x >= cx && x <= cx + fw && y >= cy && y <= cy + fh) m_hoveredElement = 20 + row; // end time
                cx -= (80.f*m_dpiScale + 8*m_dpiScale);
                float ddW = 80.f * m_dpiScale;
                if (x >= cx && x <= cx + ddW && y >= cy && y <= cy + fh) m_hoveredElement = 30 + row; // freq
                cx -= 24*m_dpiScale;
                if (x >= cx && x <= cx + 20*m_dpiScale && y >= cy && y <= cy + fh) m_hoveredElement = 40 + row; // menu
            } else {
                float cx = m_contentX + 10*m_dpiScale;
                if (x >= cx && x <= cx + 34.f * m_dpiScale && y >= cy && y <= cy + 20.f * m_dpiScale) {
                    m_hoveredElement = row; // toggle
                }
                cx += 44*m_dpiScale + 100*m_dpiScale;
                if (x >= cx && x <= cx+fw && y >= cy && y <= cy+fh) m_hoveredElement = 10 + row; // start time
                cx += fw + 8*m_dpiScale;
                if (x >= cx && x <= cx+fw && y >= cy && y <= cy+fh) m_hoveredElement = 20 + row; // end time
                cx += fw + 8*m_dpiScale;
                float ddW = 80.f * m_dpiScale;
                if (x >= cx && x <= cx+ddW && y >= cy && y <= cy+fh) m_hoveredElement = 30 + row; // freq
                cx += ddW + 8*m_dpiScale;
                if (x >= cx && x <= cx+20*m_dpiScale && y >= cy && y <= cy+fh) m_hoveredElement = 40 + row; // menu
            }
        }
    }
    
    // Hit test add btn
    float btnY = endY + ds::Space::L * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    float qhBtnX = isRtl ? (m_contentX + m_contentW - 180.f * m_dpiScale) : m_contentX;
    if (x >= qhBtnX && x <= qhBtnX + 180.f*m_dpiScale && y >= btnY && y <= btnY + ds::Dims::BtnH*m_dpiScale) {
        m_hoveredElement = 100;
    }
    
    // Hit test save btn
    float btnW = 150.f * m_dpiScale;
    float saveBtnX = isRtl ? m_contentX : (m_contentX + m_contentW - btnW);
    if (x >= saveBtnX && x <= saveBtnX + btnW && y >= btnY && y <= btnY + ds::Dims::BtnH*m_dpiScale) {
        m_hoveredSave = true;
    }
    
    return (m_hoveredElement != oldElem || m_hoveredRow != oldRow || m_hoveredSave != oldSave);
}

void SchedulePage::OnMouseLeave() { 
    m_hoveredRow = -1;
    m_hoveredElement = -1;
    m_pressedElement = -1;
    m_hoveredSave = false;
}

void SchedulePage::OnSettingsChanged() {
    if (!m_hasUnsavedChanges) {
        m_pendingSettings = Settings::Instance().GetCopy();
    }
}

void SchedulePage::OnActivated() {
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

bool SchedulePage::HitTestInteractive(int x, int y) {
    return m_hoveredElement >= 0 || m_hoveredSave;
}

} // namespace dhikr
