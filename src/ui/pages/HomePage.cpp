#include "HomePage.h"
#include "../DesignSystem.h"
#include "../TrayIcon.h"
#include "../../core/Application.h"
#include "../../core/Database.h"
#include "../../core/Scheduler.h"
#include "../../core/Settings.h"
#include "../../core/Localization.h"
#include "../../core/ContentEngine.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace dhikr {

HomePage::HomePage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;

    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    auto create = [&](float size, DWRITE_FONT_WEIGHT wt, const wchar_t* locale,
                      IDWriteTextFormat** out) {
        dwf->CreateTextFormat(L"Noto Sans Arabic", nullptr, wt,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size * m_dpiScale, locale, out);
        // DWrite performs automatic system font fallback for missing glyphs
        // (including Latin and Arabic) without any explicit fallback setup.
    };

    create(ds::FontSize::PageTitle,  DWRITE_FONT_WEIGHT_SEMI_BOLD, L"en-US", &m_tfGreet);
    create(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_REGULAR,   L"en-US", &m_tfSub);
    create(ds::FontSize::BigNumber,  DWRITE_FONT_WEIGHT_BOLD,      L"en-US", &m_tfBigNum);
    create(ds::FontSize::MedNumber,  DWRITE_FONT_WEIGHT_SEMI_BOLD, L"en-US", &m_tfMedNum);
    create(ds::FontSize::CardTitle,  DWRITE_FONT_WEIGHT_REGULAR,   L"en-US", &m_tfLabel);
    create(ds::FontSize::Small,      DWRITE_FONT_WEIGHT_REGULAR,   L"en-US", &m_tfSmall);
    create(ds::FontSize::MedNumber,  DWRITE_FONT_WEIGHT_SEMI_BOLD, L"en-US", &m_tfTime);

    if (m_tfBigNum) m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (m_tfMedNum) m_tfMedNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfGreet) m_tfGreet->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSub) m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSmall) m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfTime) m_tfTime->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    m_masterToggle = std::make_shared<ToggleControl>(Settings::Instance().Get().remindersEnabled);
    m_masterToggle->OnClick = []() {
        auto& s = Settings::Instance().Get();
        s.remindersEnabled = !s.remindersEnabled;
        Settings::Instance().Save();
        UIStateManager::Instance().RefreshStateFromBackendAsync();
    };
    // Note: Since HomePage doesn't have an exclusive InputController context yet, 
    // we'll just forward events from HomePage's methods to m_masterToggle.
}

HomePage::~HomePage() {
    if (m_tfGreet) { m_tfGreet->Release(); m_tfGreet = nullptr; }
    if (m_tfSub)   { m_tfSub->Release();   m_tfSub   = nullptr; }
    if (m_tfBigNum){ m_tfBigNum->Release();m_tfBigNum= nullptr; }
    if (m_tfMedNum){ m_tfMedNum->Release();m_tfMedNum= nullptr; }
    if (m_tfLabel) { m_tfLabel->Release(); m_tfLabel = nullptr; }
    if (m_tfSmall) { m_tfSmall->Release(); m_tfSmall = nullptr; }
    if (m_tfTime)  { m_tfTime->Release();  m_tfTime  = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
    if (m_flameGeometry) { m_flameGeometry->Release(); m_flameGeometry = nullptr; }
}

void HomePage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_d2dFactory_) {
        rt->GetFactory(&m_d2dFactory_);
    }

    if (!m_brush) {
        rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);
    }

    if (m_todayDelivered == -1) {
        m_todayDelivered = Database::Instance().GetTodayDeliveredCount();
        m_streak = Database::Instance().GetCurrentStreakDays();
        m_total = Database::Instance().GetWeekCount();
        m_dailyStats = Database::Instance().GetDailyStats(7);
    }

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfGreet) m_tfGreet->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSub) m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSmall) m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfTime) m_tfTime->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    float badgeW = 180.f * m_dpiScale;
    if (isRtl) {
        DrawStatusBadge(rt, x, y + 4.f * m_dpiScale, badgeW);
        DrawGreeting(rt, D2D1::RectF(x + badgeW + ds::Space::M * m_dpiScale, y, x + w, y + 54.f * m_dpiScale));
    } else {
        DrawGreeting(rt, D2D1::RectF(x, y, x + w - badgeW - ds::Space::M * m_dpiScale, y + 54.f * m_dpiScale));
        DrawStatusBadge(rt, x + w - badgeW, y + 4.f * m_dpiScale, badgeW);
    }

    y += 66.f * m_dpiScale;

    float heroH = 100.f * m_dpiScale;
    DrawNextReminderHero(rt, D2D1::RectF(x, y, x + w, y + heroH));
    y += heroH + 14.f * m_dpiScale;

    float summaryH = 92.f * m_dpiScale;
    DrawDailySummaryCard(rt, D2D1::RectF(x, y, x + w, y + summaryH));
    y += summaryH + 14.f * m_dpiScale;

    float timelineH = 68.f * m_dpiScale;
    DrawProgressTimeline(rt, D2D1::RectF(x, y, x + w, y + timelineH));
}

void HomePage::DrawGreeting(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    if (!m_tfGreet || !m_tfSub || !m_brush) return;
    bool isRtl = Localization::Instance().IsRTL();

    m_brush->SetColor(ds::TextPrimary());
    std::wstring greetStr = Localization::Instance().Get(L"home.greeting");
    if (m_tfGreet) {
        m_tfGreet->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfGreet->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_tfGreet->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F gr = D2D1::RectF(r.left, r.top, r.right, r.top + 28.f * m_dpiScale);
        rt->DrawTextW(greetStr.c_str(), (UINT32)greetStr.size(), m_tfGreet, gr, m_brush);
    }

    m_brush->SetColor(ds::TextSecondary());
    std::wstring subStr = Localization::Instance().Get(L"home.subtitle");
    if (m_tfSub) {
        m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F sr = D2D1::RectF(r.left, r.top + 30.f * m_dpiScale, r.right, r.top + 52.f * m_dpiScale);
        rt->DrawTextW(subStr.c_str(), (UINT32)subStr.size(), m_tfSub, sr, m_brush);
    }
}

void HomePage::DrawStatusBadge(ID2D1RenderTarget* rt, float x, float y, float w) {
    if (!m_brush || !m_masterToggle) return;
    
    float badgeH = 36.f * m_dpiScale;
    D2D1_RECT_F r = D2D1::RectF(x, y, x + w, y + badgeH);
    m_statusBadgeRect = r;

    float rx = badgeH / 2.f;
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.75f);

    bool isOn = Settings::Instance().Get().remindersEnabled;
    bool isRtl = Localization::Instance().IsRTL();

    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float ty = y + (badgeH - th) / 2.f;

    if (isRtl) {
        float tx = r.left + 8.f * m_dpiScale;
        m_masterToggle->SetBounds(D2D1::RectF(tx, ty, tx + tw, ty + th));
        m_masterToggle->SetIsOn(isOn);
        m_masterToggle->Draw(rt, m_brush, nullptr);

        if (m_tfSub) {
            m_tfSub->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
            m_tfSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

            std::wstring statLabel = Localization::Instance().Get(L"home.status");
            std::wstring statVal = Localization::Instance().Get(isOn ? L"home.active" : L"home.paused");

            float textRight = r.right - 14.f * m_dpiScale;
            float textLeft  = tx + tw + 6.f * m_dpiScale;

            std::wstring fullStr = statLabel + L": " + statVal;
            m_brush->SetColor(isOn ? ds::Accent::Primary() : ds::TextSecondary());
            rt->DrawTextW(fullStr.c_str(), (UINT32)fullStr.size(), m_tfSub, D2D1::RectF(textLeft, y, textRight, y + badgeH), m_brush);
        }
    } else {
        float tx = r.right - 8.f * m_dpiScale - tw;
        m_masterToggle->SetBounds(D2D1::RectF(tx, ty, tx + tw, ty + th));
        m_masterToggle->SetIsOn(isOn);
        m_masterToggle->Draw(rt, m_brush, nullptr);

        if (m_tfSub) {
            m_tfSub->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
            m_tfSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

            std::wstring statLabel = Localization::Instance().Get(L"home.status");
            std::wstring statVal = Localization::Instance().Get(isOn ? L"home.active" : L"home.paused");

            float lx = r.left + 14.f * m_dpiScale;
            
            float labelW = 45.f * m_dpiScale;
            IDWriteTextLayout* layout = nullptr;
            if (m_dwf && SUCCEEDED(m_dwf->CreateTextLayout(statLabel.c_str(), (UINT32)statLabel.size(), m_tfSub, 200.f, badgeH, &layout))) {
                DWRITE_TEXT_METRICS tm;
                layout->GetMetrics(&tm);
                labelW = tm.width;
                layout->Release();
            }
            
            m_brush->SetColor(ds::TextSecondary());
            rt->DrawTextW(statLabel.c_str(), (UINT32)statLabel.size(), m_tfSub, D2D1::RectF(lx, y, lx + labelW + 4.f * m_dpiScale, y + badgeH), m_brush);

            float vx = lx + labelW + 6.f * m_dpiScale;
            m_brush->SetColor(isOn ? ds::Accent::Primary() : ds::TextMuted());
            rt->DrawTextW(statVal.c_str(), (UINT32)statVal.size(), m_tfSub, D2D1::RectF(vx, y, tx - 4.f * m_dpiScale, y + badgeH), m_brush);
        }
    }
}

void HomePage::DrawCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    float rx = ds::Dims::CardRadius * m_dpiScale;
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.75f);
}

void HomePage::DrawNextReminderHero(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    DrawCard(rt, r);
    if (!m_brush) return;

    float pad = ds::Dims::CardPadding * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    time_t next = Scheduler::Instance().GetNextReminderTime();
    std::wstring timeStr = L"—";
    std::wstring inStr   = L"";

    if (next > 0) {
        struct tm lt;
        localtime_s(&lt, &next);
        wchar_t tbuf[16];
        wcsftime(tbuf, 16, L"%I:%M %p", &lt);
        timeStr = tbuf;

        time_t now = time(nullptr);
        long long diff = (long long)(next - now);
        if (diff > 0) {
            long long mins = diff / 60;
            if (isRtl) {
                if (mins < 60) {
                    inStr = L"بعد " + std::to_wstring(mins) + L" دقيقة";
                } else {
                    inStr = L"بعد " + std::to_wstring(mins/60) + L"س " + std::to_wstring(mins%60) + L"د";
                }
            } else {
                if (mins < 60) {
                    inStr = L"In " + std::to_wstring(mins) + L" minutes";
                } else {
                    inStr = L"In " + std::to_wstring(mins/60) + L"h " +
                            std::to_wstring(mins%60) + L"m";
                }
            }
        }
    }

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.next_reminder");
        D2D1_RECT_F lr = isRtl
            ? D2D1::RectF(r.left + pad + 150.f * m_dpiScale, r.top + 12.f * m_dpiScale, r.right - pad, r.top + 28.f * m_dpiScale)
            : D2D1::RectF(r.left + pad, r.top + 12.f * m_dpiScale, r.right - pad - 150.f * m_dpiScale, r.top + 28.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // Large Time
    m_brush->SetColor(ds::TextPrimary());
    if (m_tfBigNum) {
        m_tfBigNum->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = isRtl
            ? D2D1::RectF(r.right - pad - 260.f * m_dpiScale, r.top + 28.f * m_dpiScale, r.right - pad, r.top + 64.f * m_dpiScale)
            : D2D1::RectF(r.left + pad, r.top + 28.f * m_dpiScale, r.left + pad + 260.f * m_dpiScale, r.top + 64.f * m_dpiScale);
        rt->DrawTextW(timeStr.c_str(), (UINT32)timeStr.size(), m_tfBigNum, tr, m_brush);
    }

    // Upcoming Dhikr preview subtitle
    const ContentItem* previewItem = ContentEngine::Instance().GetNextPreview();
    if (previewItem && !previewItem->text.empty() && m_tfSmall) {
        m_brush->SetColor(ds::Accent::Primary(0.85f));
        m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        
        std::wstring snippet = previewItem->text.substr(0, 55);
        if (previewItem->text.size() > 55) snippet += L"...";
        std::wstring preview = L"« " + snippet + L" »";
            
        D2D1_RECT_F prText = isRtl
            ? D2D1::RectF(r.left + pad + 150.f * m_dpiScale, r.top + 68.f * m_dpiScale, r.right - pad, r.bottom - 8.f * m_dpiScale)
            : D2D1::RectF(r.left + pad, r.top + 68.f * m_dpiScale, r.right - pad - 150.f * m_dpiScale, r.bottom - 8.f * m_dpiScale);
        rt->DrawTextW(preview.c_str(), (UINT32)preview.size(), m_tfSmall, prText, m_brush);
    }

    // Countdown Badge on the opposite side
    if (!inStr.empty() && m_tfSmall) {
        float pillW = 130.f * m_dpiScale;
        float pillH = 28.f * m_dpiScale;
        float px = isRtl ? (r.left + pad) : (r.right - pad - pillW);
        float py = r.top + 28.f * m_dpiScale;

        D2D1_RECT_F pr = D2D1::RectF(px, py, px + pillW, py + pillH);
        float rx = pillH / 2.f;

        // Subtle pill background
        m_brush->SetColor(ds::Rgba(0x1B, 0x27, 0x1C));
        rt->FillRoundedRectangle(D2D1::RoundedRect(pr, rx, rx), m_brush);
        m_brush->SetColor(ds::Rgba(0x2A, 0x3D, 0x2C));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pr, rx, rx), m_brush, 0.75f);

        // Accent dot
        float dotR = 3.f * m_dpiScale;
        float dotX = isRtl ? (pr.right - 12.f * m_dpiScale) : (pr.left + 12.f * m_dpiScale);
        float dotY = py + pillH / 2.f;
        m_brush->SetColor(ds::Accent::Primary());
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), dotR, dotR), m_brush);

        // Badge Text
        m_tfSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        D2D1_RECT_F textR = isRtl
            ? D2D1::RectF(pr.left + 4.f * m_dpiScale, py, pr.right - 16.f * m_dpiScale, py + pillH)
            : D2D1::RectF(pr.left + 16.f * m_dpiScale, py, pr.right - 4.f * m_dpiScale, py + pillH);
        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawTextW(inStr.c_str(), (UINT32)inStr.size(), m_tfSmall, textR, m_brush);
    }
}

void HomePage::DrawDailySummaryCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    DrawCard(rt, r);
    if (!m_brush) return;

    float pad = ds::Dims::CardPadding * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    float totalW = r.right - r.left;
    float colW = totalW / 3.f;

    int delivered = m_todayDelivered;
    auto& ds_ref = Settings::Instance().Get();
    int dow = 0;
    { struct tm lt; time_t now = time(nullptr); localtime_s(&lt, &now); dow = lt.tm_wday; }
    auto& daysch = ds_ref.weeklySchedule[dow];
    int windowMin = (daysch.endHour*60+daysch.endMin) - (daysch.startHour*60+daysch.startMin);
    int goal = (windowMin > 0 && daysch.frequencyMinutes > 0) ? windowMin / daysch.frequencyMinutes : 20;
    goal = std::max(1, goal);
    float progress = std::min(1.f, (float)delivered / (float)goal);

    float c1X = isRtl ? (r.left + colW * 2.f) : r.left;
    D2D1_RECT_F c1R = D2D1::RectF(c1X + pad, r.top + pad, c1X + colW - pad, r.bottom - pad);

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.today");
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, D2D1::RectF(c1R.left, c1R.top, c1R.right, c1R.top + 16.f * m_dpiScale), m_brush);
    }

    m_brush->SetColor(ds::TextPrimary());
    std::wstring countStr = std::to_wstring(delivered) + (isRtl ? L" \\ " : L" / ") + std::to_wstring(goal);
    if (m_tfMedNum) {
        m_tfMedNum->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfMedNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        rt->DrawTextW(countStr.c_str(), (UINT32)countStr.size(), m_tfMedNum, D2D1::RectF(c1R.left, c1R.top + 16.f * m_dpiScale, c1R.right, c1R.top + 42.f * m_dpiScale), m_brush);
    }

    int pct = (int)(progress * 100.f);
    std::wstring pctStr = std::to_wstring(pct) + L"% " + Localization::Instance().Get(L"home.completed");
    if (m_tfSmall) {
        m_brush->SetColor(ds::TextSecondary());
        m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        rt->DrawTextW(pctStr.c_str(), (UINT32)pctStr.size(), m_tfSmall, D2D1::RectF(c1R.left, c1R.bottom - 18.f * m_dpiScale, c1R.right, c1R.bottom - 6.f * m_dpiScale), m_brush);
    }
    D2D1_RECT_F barR = D2D1::RectF(c1R.left, c1R.bottom - 4.f * m_dpiScale, c1R.right, c1R.bottom);
    DrawProgressBar(rt, barR, progress);

    float div1X = isRtl ? (r.left + colW * 2.f) : (r.left + colW);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(div1X, r.top + 14.f * m_dpiScale), D2D1::Point2F(div1X, r.bottom - 14.f * m_dpiScale), m_brush, 0.75f);

    float c2X = r.left + colW;
    D2D1_RECT_F c2R = D2D1::RectF(c2X + pad, r.top + pad, c2X + colW - pad, r.bottom - pad);

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.streak");
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, D2D1::RectF(c2R.left, c2R.top, c2R.right, c2R.top + 16.f * m_dpiScale), m_brush);
    }

    if (!m_flameGeometry && m_d2dFactory_) {
        m_d2dFactory_->CreatePathGeometry(&m_flameGeometry);
        if (m_flameGeometry) {
            ID2D1GeometrySink* sink = nullptr;
            m_flameGeometry->Open(&sink);
            float fs = 15.f * m_dpiScale;
            if (sink) {
                sink->BeginFigure(D2D1::Point2F(fs*0.5f, 0.f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(fs*0.9f, fs*0.4f),
                    D2D1::Point2F(fs, fs*0.7f),
                    D2D1::Point2F(fs*0.7f, fs)));
                sink->AddLine(D2D1::Point2F(fs*0.3f, fs));
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(0.f, fs*0.7f),
                    D2D1::Point2F(fs*0.1f, fs*0.4f),
                    D2D1::Point2F(fs*0.5f, 0.f)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close(); sink->Release();
            }
        }
    }

    float flameOffset = 0.f;
    if (m_flameGeometry) {
        m_brush->SetColor(ds::Rgba(0xF5, 0x82, 0x20));
        float flameX = isRtl ? (c2R.right - 16.f * m_dpiScale) : c2R.left;
        rt->SetTransform(D2D1::Matrix3x2F::Translation(flameX, c2R.top + 20.f * m_dpiScale));
        rt->FillGeometry(m_flameGeometry, m_brush);
        rt->SetTransform(D2D1::Matrix3x2F::Identity());
        flameOffset = 20.f * m_dpiScale;
    }

    m_brush->SetColor(ds::TextPrimary());
    std::wstring streakStr = std::to_wstring(m_streak);
    if (m_tfMedNum) {
        m_tfMedNum->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfMedNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F srNum = isRtl
            ? D2D1::RectF(c2R.left, c2R.top + 16.f * m_dpiScale, c2R.right - flameOffset, c2R.top + 42.f * m_dpiScale)
            : D2D1::RectF(c2R.left + flameOffset, c2R.top + 16.f * m_dpiScale, c2R.right, c2R.top + 42.f * m_dpiScale);
        rt->DrawTextW(streakStr.c_str(), (UINT32)streakStr.size(), m_tfMedNum, srNum, m_brush);
    }

    if (m_tfSmall) {
        m_brush->SetColor(ds::TextSecondary());
        m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring days = Localization::Instance().Get(L"home.days");
        D2D1_RECT_F dR = D2D1::RectF(c2R.left, c2R.bottom - 18.f * m_dpiScale, c2R.right, c2R.bottom - 6.f * m_dpiScale);
        rt->DrawTextW(days.c_str(), (UINT32)days.size(), m_tfSmall, dR, m_brush);
    }

    float div2X = isRtl ? (r.left + colW) : (r.left + colW * 2.f);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(div2X, r.top + 14.f * m_dpiScale), D2D1::Point2F(div2X, r.bottom - 14.f * m_dpiScale), m_brush, 0.75f);

    float c3X = isRtl ? r.left : (r.left + colW * 2.f);
    D2D1_RECT_F c3R = D2D1::RectF(c3X + pad, r.top + pad, c3X + colW - pad, r.bottom - pad);

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.total");
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, D2D1_RECT_F{c3R.left, c3R.top, c3R.right, c3R.top + 16.f * m_dpiScale}, m_brush);
    }

    m_brush->SetColor(ds::TextPrimary());
    std::wstring totalStr = std::to_wstring(m_total);
    if (m_tfMedNum) {
        m_tfMedNum->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfMedNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F numR = isRtl
            ? D2D1::RectF(c3R.left + 50.f * m_dpiScale, c3R.top + 16.f * m_dpiScale, c3R.right, c3R.top + 42.f * m_dpiScale)
            : D2D1::RectF(c3R.left, c3R.top + 16.f * m_dpiScale, c3R.right - 50.f * m_dpiScale, c3R.top + 42.f * m_dpiScale);
        rt->DrawTextW(totalStr.c_str(), (UINT32)totalStr.size(), m_tfMedNum, numR, m_brush);
    }

    if (m_tfSmall) {
        m_brush->SetColor(ds::TextSecondary());
        m_tfSmall->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring tw = Localization::Instance().Get(L"home.this_week");
        D2D1_RECT_F twR = isRtl
            ? D2D1::RectF(c3R.left + 50.f * m_dpiScale, c3R.bottom - 18.f * m_dpiScale, c3R.right, c3R.bottom)
            : D2D1::RectF(c3R.left, c3R.bottom - 18.f * m_dpiScale, c3R.right - 50.f * m_dpiScale, c3R.bottom);
        rt->DrawTextW(tw.c_str(), (UINT32)tw.size(), m_tfSmall, twR, m_brush);
    }

    float chartW = 44.f * m_dpiScale;
    float chartH = 34.f * m_dpiScale;
    float chartX = isRtl ? c3R.left : (c3R.right - chartW);
    float chartY = c3R.top + (c3R.bottom - c3R.top - chartH) / 2.f + 4.f * m_dpiScale;
    DrawMiniBarChart(rt, D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH));
}

void HomePage::DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, float progress) {
    float rx = ds::Dims::ProgressH * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(ds::Border());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    if (progress > 0.01f) {
        float fillW = (r.right - r.left) * progress;
        D2D1_RECT_F fill = isRtl
            ? D2D1::RectF(r.right - fillW, r.top, r.right, r.bottom)
            : D2D1::RectF(r.left, r.top, r.left + fillW, r.bottom);
        m_brush->SetColor(ds::Accent::Primary());
        rt->FillRoundedRectangle(D2D1::RoundedRect(fill, rx, rx), m_brush);
    }
}

void HomePage::DrawMiniBarChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    auto stats = m_dailyStats;

    int maxVal = 1;
    std::vector<int> vals;
    for (auto& s : stats) { vals.push_back(s.delivered); maxVal = std::max(maxVal, s.delivered); }
    while ((int)vals.size() < 7) vals.push_back(0);
    std::reverse(vals.begin(), vals.end());

    float chartH = r.bottom - r.top;
    float chartW = r.right  - r.left;
    float barW   = chartW / 7.f * 0.6f;
    float gap    = chartW / 7.f;

    for (int i = 0; i < 7; ++i) {
        float normH = (float)vals[i] / (float)maxVal;
        float bh    = std::max(2.f * m_dpiScale, normH * chartH * 0.8f);
        float bx    = r.left + gap * i + (gap - barW) / 2.f;
        float by    = r.bottom - bh;

        m_brush->SetColor(i == 6 ? ds::Accent::Primary() : ds::Accent::Muted(0.5f));
        D2D1_RECT_F bar = D2D1::RectF(bx, by, bx + barW, r.bottom);
        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 2*m_dpiScale, 2*m_dpiScale), m_brush);
    }
}

void HomePage::DrawProgressTimeline(ID2D1RenderTarget* rt, const D2D1_RECT_F& area) {
    if (!m_brush) return;

    float pad = ds::Dims::CardPadding * m_dpiScale;
    float rx  = ds::Dims::CardRadius  * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(area, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(area, rx, rx), m_brush, 0.75f);

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.todays_progress");
        D2D1_RECT_F lr = D2D1::RectF(area.left+pad, area.top+10.f*m_dpiScale, area.right-pad, area.top+26.f*m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    struct tm lt; time_t now = time(nullptr); localtime_s(&lt, &now);
    auto& s = Settings::Instance().Get();
    int dow = lt.tm_wday;
    auto& ds_ref = s.weeklySchedule[dow];
    int startMin = ds_ref.startHour * 60 + ds_ref.startMin;
    int endMin   = ds_ref.endHour   * 60 + ds_ref.endMin;
    int nowMin   = lt.tm_hour * 60 + lt.tm_min;
    int totalMin = std::max(1, endMin - startMin);

    float lineY   = area.top + (area.bottom - area.top) * 0.62f;
    float lineLeft = area.left + pad + 20.f * m_dpiScale;
    float lineRight= area.right - pad - 20.f * m_dpiScale;

    m_brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(lineLeft, lineY), D2D1::Point2F(lineRight, lineY), m_brush, 1.f);

    constexpr int kCheckpoints = 5;
    float dotR = 5.f * m_dpiScale;

    static const wchar_t* kLabels[5] = { L"8 AM", L"10 AM", L"12 PM", L"4 PM", L"8 PM" };

    for (int i = 0; i < kCheckpoints; ++i) {
        float t  = (float)i / (float)(kCheckpoints - 1);
        float cx = isRtl ? (lineRight - (lineRight - lineLeft) * t) : (lineLeft + (lineRight - lineLeft) * t);
        float cy = lineY;

        int checkMin = startMin + (int)(t * totalMin);
        bool passed  = nowMin > checkMin;
        bool current = !passed && (i == 0 || (nowMin > startMin + (int)(((float)(i-1)/(float)(kCheckpoints-1)) * totalMin)));

        if (passed) {
            m_brush->SetColor(ds::Accent::Primary());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush);
        } else if (current) {
            m_brush->SetColor(ds::Accent::Primary(0.3f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR*1.4f, dotR*1.4f), m_brush);
            m_brush->SetColor(ds::Accent::Primary());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush);
        } else {
            m_brush->SetColor(ds::BorderSubtle());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush);
        }

        if (m_tfSmall) {
            m_brush->SetColor(passed ? ds::TextPrimary() : ds::TextMuted());
            m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            D2D1_RECT_F lr = D2D1::RectF(cx - 24.f * m_dpiScale, cy + dotR + 4.f * m_dpiScale, cx + 24.f * m_dpiScale, cy + dotR + 18.f * m_dpiScale);
            rt->DrawTextW(kLabels[i], (UINT32)wcslen(kLabels[i]), m_tfSmall, lr, m_brush);
        }
    }
}

bool HomePage::OnLButtonDown(int x, int y) {
    if ((m_masterToggle && m_masterToggle->HitTest((float)x, (float)y)) ||
        (x >= m_statusBadgeRect.left && x <= m_statusBadgeRect.right &&
         y >= m_statusBadgeRect.top && y <= m_statusBadgeRect.bottom)) {
        bool newState = !Settings::Instance().Get().remindersEnabled;
        Settings::Instance().Get().remindersEnabled = newState;
        Settings::Instance().Save();
        m_masterToggle->SetIsOn(newState);
        Scheduler::Instance().Recalculate();
        TrayIcon::Instance().SetPaused(!newState);
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}
bool HomePage::OnMouseMove(int x, int y) {
    if (m_masterToggle) {
        return m_masterToggle->OnMouseMove((float)x, (float)y);
    }
    return false;
}
void HomePage::OnMouseLeave() { 
    if (m_masterToggle) m_masterToggle->OnMouseLeave();
}
void HomePage::OnSettingsChanged() {
    m_todayDelivered = -1;
    m_streak = -1;
    m_total = -1;
    m_dailyStats.clear();
}

bool HomePage::HitTestInteractive(int x, int y) {
    if (m_masterToggle && m_masterToggle->HitTest((float)x, (float)y)) return true;
    if (x >= m_statusBadgeRect.left && x <= m_statusBadgeRect.right &&
        y >= m_statusBadgeRect.top && y <= m_statusBadgeRect.bottom) return true;
    return false;
}

ID2D1Factory* HomePage::m_d2dFactory_ = nullptr;

} // namespace dhikr
