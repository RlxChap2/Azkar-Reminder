#include "PopupWindow.h"
#include "DesignSystem.h"
#include "core/LogoRenderer.h"
#include "core/ResourceLoader.h"
#include "../core/Application.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include "../audio/AudioEngine.h"
#include "../core/Database.h"
#include "../core/Scheduler.h"
#include "../core/Localization.h"
#include "../../src/resource.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <dwmapi.h>
#include <shlwapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dhikr {

namespace {
    std::wstring Utf8ToWideLocal(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
        return result;
    }
}

PopupWindow& PopupWindow::Instance() {
    static PopupWindow instance;
    return instance;
}

PopupWindow::~PopupWindow() {
    ReleaseD2D();
    if (m_grainBitmap) { m_grainBitmap->Release(); m_grainBitmap = nullptr; }
    if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
}

// ── Window class registration ─────────────────────────────────────────────────
void PopupWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

bool PopupWindow::Create(HINSTANCE hInstance) {
    m_hInst = hInstance;
    RegisterClass(hInstance);

    // Use GetDpiForSystem temporarily
    m_dpiScale = GetDpiForSystem() / 96.f;

    // Create layered window (hidden initially)
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClassName,
        L"DhikrPopup",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, hInstance, this);

    if (!m_hwnd) {
        LOG_ERROR("Failed to create popup window");
        return false;
    }

    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi != 0) {
        m_dpiScale = dpi / 96.f;
    }

    // Note: DWM backdrop (Mica/Acrylic) is NOT compatible with WS_EX_LAYERED windows
    // that use UpdateLayeredWindow — they conflict and produce white artifact backgrounds.
    // The popup uses its own alpha-blended rendering via UpdateLayeredWindow instead.


    if (!InitD2D()) {
        // D2D initialisation failed (e.g. driver issue); popup will be skipped
        // gracefully — reminders will still be logged but the overlay won't show.
        LOG_WARN("Popup D2D init failed — popup rendering disabled");
        return true; // window exists; safe to continue without it
    }
    GenerateGrainBitmap(); // safe: already guards on m_dcRenderTarget

    LOG_INFO("Popup window created");
    return true;
}

// ── Grain texture (pre-generated noise bitmap) ────────────────────────────────
void PopupWindow::GenerateGrainBitmap() {
    constexpr int SIZE = ds::Grain::Size;
    if (!m_dcRenderTarget) return;

    std::vector<uint32_t> pixels(SIZE * SIZE);
    std::mt19937 rng(0xDEADBEEF); // deterministic seed
    std::uniform_int_distribution<int> noiseDist(0, 20);

    for (int i = 0; i < SIZE * SIZE; ++i) {
        uint8_t v = (uint8_t)noiseDist(rng);
        uint8_t a = (uint8_t)(v * 3); // alpha: subtle
        pixels[i] = (a << 24) | (v << 16) | (v << 8) | v;
    }

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );

    m_dcRenderTarget->CreateBitmap(
        D2D1::SizeU(SIZE, SIZE),
        pixels.data(),
        SIZE * 4,
        &props,
        &m_grainBitmap
    );
}

// ── Direct2D init ─────────────────────────────────────────────────────────────
bool PopupWindow::InitD2D() {
    m_d2dFactory    = Application::Instance().D2DFactory();
    m_dwriteFactory = Application::Instance().DWriteFactory();

    // DC render target (for UpdateLayeredWindow)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.f, 96.f);

    m_d2dFactory->CreateDCRenderTarget(&props, &m_dcRenderTarget);

    // Adaptive text formats (Noto Sans Arabic)
    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans Arabic", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.f * m_dpiScale,
        L"ar-SA", &m_textFmtMainLarge);
    if (m_textFmtMainLarge) {
        m_textFmtMainLarge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainLarge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainLarge->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainLarge->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans Arabic", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.f * m_dpiScale,
        L"ar-SA", &m_textFmtMainMedium);
    if (m_textFmtMainMedium) {
        m_textFmtMainMedium->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainMedium->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainMedium->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainMedium->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans Arabic", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.5f * m_dpiScale,
        L"ar-SA", &m_textFmtMainCompact);
    if (m_textFmtMainCompact) {
        m_textFmtMainCompact->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainCompact->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainCompact->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainCompact->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans Arabic", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        ds::FontSize::PopupMain * m_dpiScale,
        L"ar-SA", &m_textFmtMain);

    if (m_textFmtMain) {
        m_textFmtMain->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMain->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMain->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMain->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans Arabic", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        ds::FontSize::PopupSub * m_dpiScale,
        L"ar-SA", &m_textFmtSub);

    if (m_textFmtSub) {
        m_textFmtSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtSub->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Noto Sans", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        ds::FontSize::Small * m_dpiScale,
        L"en-US", &m_textFmtSmall);

    if (m_textFmtSmall) {
        m_textFmtSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    // Initialize WIC and load logo from embedded resource at full native resolution
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
    if (SUCCEEDED(hr) && m_wicFactory && m_dcRenderTarget) {
        m_logoBitmap = ResourceLoader::LoadBitmapFromResource(
            m_hInst, IDR_LOGO, m_wicFactory, m_dcRenderTarget);
        if (m_logoBitmap) {
            LOG_INFO("PopupWindow: Embedded logo loaded successfully from resource");
        } else {
            LOG_WARN("PopupWindow: Failed to load embedded logo from resource");
        }
    }

    return m_dcRenderTarget != nullptr;
}

void PopupWindow::ReleaseD2D() {
    if (m_logoBitmap)          { m_logoBitmap->Release();          m_logoBitmap          = nullptr; }
    if (m_wicFactory)          { m_wicFactory->Release();          m_wicFactory          = nullptr; }
    if (m_textFmtSmall)        { m_textFmtSmall->Release();        m_textFmtSmall        = nullptr; }
    if (m_textFmtSub)          { m_textFmtSub->Release();          m_textFmtSub          = nullptr; }
    if (m_textFmtMainCompact)  { m_textFmtMainCompact->Release();  m_textFmtMainCompact  = nullptr; }
    if (m_textFmtMainMedium)   { m_textFmtMainMedium->Release();   m_textFmtMainMedium   = nullptr; }
    if (m_textFmtMainLarge)    { m_textFmtMainLarge->Release();    m_textFmtMainLarge    = nullptr; }
    if (m_textFmtMain)         { m_textFmtMain->Release();         m_textFmtMain         = nullptr; }
    if (m_dcRenderTarget)      { m_dcRenderTarget->Release();      m_dcRenderTarget      = nullptr; }
}

// ── Show / Hide ───────────────────────────────────────────────────────────────
void PopupWindow::Show(const ContentItem* item) {
    if (!m_hwnd || !item) return;
    if (m_state != PopupAnimState::Hidden && m_state != PopupAnimState::Closing) {
        // Already visible — update content
        m_item = item;
        Render(1.f);
        // Restart the auto-dismiss timer so it doesn't dismiss early
        CancelAutoDismiss();
        SetupAutoDismiss();
        return;
    }

    m_item = item;

    // 1. Adaptive typography selection
    IDWriteTextFormat* mainFmt = m_textFmtMain;
    if (m_item->text.size() <= 40 && m_textFmtMainLarge) {
        mainFmt = m_textFmtMainLarge;
    } else if (m_item->text.size() <= 90 && m_textFmtMainMedium) {
        mainFmt = m_textFmtMainMedium;
    } else if (m_item->text.size() > 90 && m_textFmtMainCompact) {
        mainFmt = m_textFmtMainCompact;
    }

    // 2. Content-adaptive maximum wrap width based on text length & word count
    size_t textLen = m_item->text.size();
    float maxTextW = 320.f * m_dpiScale;
    if (textLen <= 25) {
        maxTextW = 280.f * m_dpiScale;
    } else if (textLen <= 55) {
        maxTextW = 380.f * m_dpiScale;
    } else if (textLen <= 100) {
        maxTextW = 480.f * m_dpiScale;
    } else if (textLen <= 180) {
        maxTextW = 580.f * m_dpiScale;
    } else {
        maxTextW = 680.f * m_dpiScale;
    }

    float leftPad   = 60.f * m_dpiScale; // Left accent + icon room
    float rightPad  = 44.f * m_dpiScale; // Right close button room
    float padY      = 16.f * m_dpiScale; // Vertical padding

    float desiredTextW = 0.f;
    float desiredTextH = 0.f;

    if (mainFmt && m_dwriteFactory) {
        IDWriteTextLayout* layout = nullptr;
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->text.c_str(), (UINT32)m_item->text.size(), mainFmt, maxTextW, 2000.f, &layout))) {
            DWRITE_TEXT_METRICS metrics;
            layout->GetMetrics(&metrics);
            layout->Release();
            desiredTextW = metrics.width;
            desiredTextH = metrics.height;
        }

        if (m_textFmtSub && !m_item->description.empty()) {
            if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->description.c_str(), (UINT32)m_item->description.size(), m_textFmtSub, maxTextW, 2000.f, &layout))) {
                DWRITE_TEXT_METRICS metrics;
                layout->GetMetrics(&metrics);
                layout->Release();
                desiredTextW = std::max(desiredTextW, metrics.width);
                desiredTextH += metrics.height + 4.f * m_dpiScale;
            }
        }
    }

    // Dynamic width & height tightly bounded with padding
    float finalW = desiredTextW + leftPad + rightPad + 16.f * m_dpiScale;
    float finalH = desiredTextH + padY * 2.f;

    float minW = 260.f * m_dpiScale;
    float maxW = 820.f * m_dpiScale;
    float minH = 64.f * m_dpiScale;
    float maxH = 320.f * m_dpiScale;

    m_width  = std::clamp((int)std::ceil(finalW), (int)minW, (int)maxW);
    m_height = std::clamp((int)std::ceil(finalH), (int)minH, (int)maxH);

    // Calculate screen position
    RECT pos = CalculatePosition(m_width, m_height);
    m_screenX = pos.left;
    m_screenY = pos.top;

    // Position and show the window (still transparent)
    SetWindowPos(m_hwnd, HWND_TOPMOST,
        m_screenX, m_screenY + (int)(ds::Anim::PopupSlideDistPx * m_dpiScale),
        m_width, m_height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Play notification sound
    AudioEngine::Instance().PlayReminder();

    // Start enter animation
    StartEnterAnim();

    LOG_INFO("Popup shown");
}

void PopupWindow::Hide() {
    if (!m_hwnd) return;
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
    StartExitAnim();
}

// ── Position calculation ──────────────────────────────────────────────────────
HMONITOR PopupWindow::GetTargetMonitor() {
    auto& s = Settings::Instance().Get();
    switch (s.popupMonitor) {
        case PopupMonitor::Active: {
            POINT pt;
            GetCursorPos(&pt);
            return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }
        case PopupMonitor::Specific: {
            // Enumerate monitors and pick the nth
            struct EnumData { int target; int current; HMONITOR result; };
            EnumData data = { s.specificMonitor, 0, nullptr };
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR mon, HDC, LPRECT, LPARAM lp) -> BOOL {
                auto* d = (EnumData*)lp;
                if (d->current == d->target) { d->result = mon; return FALSE; }
                ++d->current; return TRUE;
            }, (LPARAM)&data);
            if (data.result) return data.result;
            [[fallthrough]];
        }
        default:
            return MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    }
}

RECT PopupWindow::CalculatePosition(int w, int h) {
    HMONITOR hMon = GetTargetMonitor();
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    RECT& work = mi.rcWork;

    int margin = (int)(ds::Dims::PopupMargin * m_dpiScale);
    auto& s = Settings::Instance().Get();

    int x, y;
    int pw = work.right  - work.left;
    int ph = work.bottom - work.top;

    switch (s.popupPosition) {
        case PopupPosition::TopLeft:       x = work.left  + margin;       y = work.top  + margin; break;
        case PopupPosition::TopCenter:     x = work.left  + (pw - w) / 2; y = work.top  + margin; break;
        case PopupPosition::TopRight:      x = work.right - w - margin;   y = work.top  + margin; break;
        case PopupPosition::CenterLeft:    x = work.left  + margin;       y = work.top  + (ph - h) / 2; break;
        case PopupPosition::Center:        x = work.left  + (pw - w) / 2; y = work.top  + (ph - h) / 2; break;
        case PopupPosition::CenterRight:   x = work.right - w - margin;   y = work.top  + (ph - h) / 2; break;
        case PopupPosition::BottomLeft:    x = work.left  + margin;       y = work.bottom - h - margin; break;
        case PopupPosition::BottomCenter:  x = work.left  + (pw - w) / 2; y = work.bottom - h - margin; break;
        case PopupPosition::BottomRight:   x = work.right - w - margin;   y = work.bottom - h - margin; break;
        default:                           x = work.left  + (pw - w) / 2; y = work.top  + margin; break;
    }

    // Clamp to monitor work area
    x = std::clamp(x, static_cast<int>(work.left), static_cast<int>(work.right  - w));
    y = std::clamp(y, static_cast<int>(work.top),  static_cast<int>(work.bottom - h));

    return { x, y, x + w, y + h };
}

// ── Animation helpers ─────────────────────────────────────────────────────────
void PopupWindow::StartEnterAnim() {
    m_state = PopupAnimState::Entering;
    m_animT = 0.f;
    m_animStart = GetTickCount();
    m_animDuration = ds::Anim::PopupEnterMs;
    SetTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER, ds::Anim::FrameIntervalMs, nullptr);
}

void PopupWindow::StartExitAnim() {
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
    m_state = PopupAnimState::Closing;
    m_animT = 0.f;
    m_animStart = GetTickCount();
    m_animDuration = ds::Anim::PopupExitMs;
    SetTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT, ds::Anim::FrameIntervalMs, nullptr);
}

void PopupWindow::SetupAutoDismiss() {
    int durationMs = Settings::Instance().Get().popupDurationSec * 1000;
    SetTimer(m_hwnd, TIMER_POPUP_AUTODISMISS, durationMs, nullptr);
}

void PopupWindow::CancelAutoDismiss() {
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
}

// ── Rendering ─────────────────────────────────────────────────────────────────
void PopupWindow::Render(float animProgress) {
    if (!m_dcRenderTarget || !m_hwnd) return;

    bool entering = (m_state == PopupAnimState::Entering);
    bool exiting  = (m_state == PopupAnimState::Closing);

    float alpha = entering ? ds::EaseOutCubic(animProgress) : (exiting ? ds::EaseIn(1.f - animProgress) : 1.f);
    float slide = entering
        ? ds::Anim::PopupSlideDistPx * m_dpiScale * (1.f - ds::EaseOutBack(animProgress))
        : (exiting ? ds::Anim::PopupSlideDistPx * m_dpiScale * ds::EaseIn(animProgress) : 0.f);

    // Add hover brightness boost
    float hoverBoost = m_hovering ? 0.06f : 0.f;
    alpha = std::min(1.f, alpha + hoverBoost * 0.1f);

    UpdateLayered(alpha, slide);
}

void PopupWindow::UpdateLayered(float alpha, float slideOffset) {
    if (!m_dcRenderTarget) return;

    // Create a memory DC
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = m_width;
    bmi.bmiHeader.biHeight      = -m_height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Bind DC to D2D render target
    RECT rcBounds = { 0, 0, m_width, m_height };
    HRESULT hrBind = m_dcRenderTarget->BindDC(hdcMem, &rcBounds);
    if (FAILED(hrBind)) {
        // Cannot render this frame — clean up and return rather than crashing.
        LOG_WARN("Popup: BindDC failed, skipping frame");
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0, 0));

    // Draw all layers
    DrawBackground(m_dcRenderTarget, alpha, slideOffset);
    DrawGrainTexture(m_dcRenderTarget, alpha);
    DrawAccentBar(m_dcRenderTarget, alpha);
    DrawCrescentMark(m_dcRenderTarget, alpha);
    DrawContent(m_dcRenderTarget, alpha);
    DrawCloseButton(m_dcRenderTarget, alpha);
    DrawPauseButton(m_dcRenderTarget, alpha);

    m_dcRenderTarget->EndDraw();

    // UpdateLayeredWindow
    POINT ptSrc  = { 0, 0 };
    SIZE  szWnd  = { m_width, m_height };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT ptDest = { m_screenX, m_screenY + (int)slideOffset };

    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDest, &szWnd,
        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void PopupWindow::DrawBackground(ID2D1RenderTarget* rt, float alpha, float) {
    ID2D1SolidColorBrush* brush = nullptr;
    auto bgColor = ds::PopupBg();
    bgColor.a = alpha;
    rt->CreateSolidColorBrush(bgColor, &brush);
    if (!brush) return;

    float r = ds::Dims::PopupRadius * m_dpiScale;
    D2D1_ROUNDED_RECT bgRect{
        D2D1::RectF(0.f, 0.f, (float)m_width, (float)m_height),
        r, r };

    rt->FillRoundedRectangle(bgRect, brush);

    // Subtle 1px inner border
    auto borderColor = ds::PopupBorder();
    borderColor.a = alpha * 0.7f;
    brush->SetColor(borderColor);
    rt->DrawRoundedRectangle(bgRect, brush, 1.0f);

    brush->Release();
}

void PopupWindow::DrawGrainTexture(ID2D1RenderTarget* rt, float alpha) {
    if (!m_grainBitmap || alpha < 0.05f) return;
    (void)rt;
}

void PopupWindow::DrawAccentBar(ID2D1RenderTarget* rt, float alpha) {
    ID2D1SolidColorBrush* brush = nullptr;
    auto accentColor = ds::Accent::Primary(alpha);
    rt->CreateSolidColorBrush(accentColor, &brush);
    if (!brush) return;

    float barW = 4.5f * m_dpiScale;
    float padY = std::max(12.f * m_dpiScale, (float)m_height * 0.16f);
    float padX = 10.f * m_dpiScale;

    D2D1_ROUNDED_RECT barRect{
        D2D1::RectF(padX, padY, padX + barW, (float)m_height - padY),
        2.5f * m_dpiScale, 2.5f * m_dpiScale };
    rt->FillRoundedRectangle(barRect, brush);

    // Subtle lime glow effect behind bar
    auto glowColor = ds::Accent::Glow(alpha * 0.15f);
    brush->SetColor(glowColor);
    D2D1_ROUNDED_RECT glowRect{
        D2D1::RectF(padX - 3.f*m_dpiScale, padY - 3.f*m_dpiScale, padX + barW + 3.f*m_dpiScale, (float)m_height - padY + 3.f*m_dpiScale),
        5.f * m_dpiScale, 5.f * m_dpiScale };
    rt->FillRoundedRectangle(glowRect, brush);

    brush->Release();
}

void PopupWindow::DrawCrescentMark(ID2D1RenderTarget* rt, float alpha) {
    float cx     = 36.f * m_dpiScale;
    float cy     = (float)m_height * 0.5f;
    float radius = 13.f * m_dpiScale;

    D2D1_RECT_F dest = D2D1::RectF(cx - radius, cy - radius, cx + radius, cy + radius);
    if (m_logoBitmap) {
        D2D1_SIZE_F sz = m_logoBitmap->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(dest, sz.width, sz.height);
        rt->DrawBitmap(m_logoBitmap, fitRect, alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, dest, alpha);
    }
}

void PopupWindow::DrawContent(ID2D1RenderTarget* rt, float alpha) {
    if (!m_item) return;

    ID2D1SolidColorBrush* textBrush = nullptr;
    auto textColor = ds::TextPrimary();
    textColor.a = alpha;
    rt->CreateSolidColorBrush(textColor, &textBrush);
    if (!textBrush) return;

    float leftPad  = 60.f * m_dpiScale;
    float rightPad = 44.f * m_dpiScale;
    float padY     = 14.f * m_dpiScale;
    float w        = (float)m_width - leftPad - rightPad;

    // Select adaptive typography format
    IDWriteTextFormat* mainFmt = m_textFmtMain;
    if (m_item->text.size() <= 40 && m_textFmtMainLarge) {
        mainFmt = m_textFmtMainLarge;
    } else if (m_item->text.size() <= 90 && m_textFmtMainMedium) {
        mainFmt = m_textFmtMainMedium;
    } else if (m_item->text.size() > 90 && m_textFmtMainCompact) {
        mainFmt = m_textFmtMainCompact;
    }

    float mainH = 0.f;
    float subH  = 0.f;
    IDWriteTextLayout* mainLayout = nullptr;
    IDWriteTextLayout* subLayout  = nullptr;

    // Main text (Arabic, RTL, centered)
    if (mainFmt && !m_item->text.empty()) {
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->text.c_str(), (UINT32)m_item->text.size(), mainFmt, w, 2000.f, &mainLayout))) {
            DWRITE_TEXT_METRICS metrics;
            mainLayout->GetMetrics(&metrics);
            mainH = metrics.height;
        }
    }

    // Description / transliteration (smaller, secondary color)
    if (m_textFmtSub && !m_item->description.empty()) {
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->description.c_str(), (UINT32)m_item->description.size(), m_textFmtSub, w, 2000.f, &subLayout))) {
            DWRITE_TEXT_METRICS metrics;
            subLayout->GetMetrics(&metrics);
            subH = metrics.height + 4.f * m_dpiScale;
        }
    }

    float totalTextH = mainH + subH;
    float availH = (float)m_height;
    float yCursor = std::max(padY, (availH - totalTextH) / 2.f);

    if (mainLayout) {
        rt->DrawTextLayout(D2D1::Point2F(leftPad, yCursor), mainLayout, textBrush);
        yCursor += mainH + 4.f * m_dpiScale;
        mainLayout->Release();
    }

    if (subLayout) {
        auto subColor = ds::TextSecondary();
        subColor.a = alpha * 0.85f;
        textBrush->SetColor(subColor);
        rt->DrawTextLayout(D2D1::Point2F(leftPad, yCursor), subLayout, textBrush);
        subLayout->Release();
    }

    textBrush->Release();
}

void PopupWindow::DrawCloseButton(ID2D1RenderTarget* rt, float alpha) {
    float btnSize = 18.f * m_dpiScale;
    float margin  = 8.f * m_dpiScale;
    float cx = (float)m_width  - margin - btnSize / 2.f;
    float cy = margin + btnSize / 2.f;

    m_closeRect = {
        (int)(cx - btnSize/2), (int)(cy - btnSize/2),
        (int)(cx + btnSize/2), (int)(cy + btnSize/2) };

    ID2D1SolidColorBrush* brush = nullptr;
    auto color = ds::TextMuted();
    color.a = alpha * (m_hovering ? 0.8f : 0.5f);
    rt->CreateSolidColorBrush(color, &brush);
    if (!brush) return;

    float arm = 4.5f * m_dpiScale;
    rt->DrawLine(D2D1::Point2F(cx - arm, cy - arm), D2D1::Point2F(cx + arm, cy + arm), brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx + arm, cy - arm), D2D1::Point2F(cx - arm, cy + arm), brush, 1.5f);

    brush->Release();
}

void PopupWindow::DrawPauseButton(ID2D1RenderTarget* rt, float alpha) {
    float btnSize = 16.f * m_dpiScale;
    float margin  = 8.f * m_dpiScale;
    float cx = (float)m_width  - margin - btnSize / 2.f;
    float cy = (float)m_height - margin - btnSize / 2.f;

    m_pauseRect = {
        (int)(cx - btnSize/2), (int)(cy - btnSize/2),
        (int)(cx + btnSize/2), (int)(cy + btnSize/2) };

    ID2D1SolidColorBrush* brush = nullptr;
    auto color = ds::TextMuted();
    color.a = alpha * (m_hovering ? 0.7f : 0.4f);
    rt->CreateSolidColorBrush(color, &brush);
    if (!brush) return;

    // Pause icon (two vertical bars)
    float barW   = 2.5f * m_dpiScale;
    float barH   = 8.f * m_dpiScale;
    float spacing= 2.5f * m_dpiScale;
    D2D1_RECT_F left  = D2D1::RectF(cx - spacing - barW, cy - barH/2, cx - spacing,       cy + barH/2);
    D2D1_RECT_F right = D2D1::RectF(cx + spacing,         cy - barH/2, cx + spacing + barW, cy + barH/2);
    rt->FillRectangle(left,  brush);
    rt->FillRectangle(right, brush);

    brush->Release();
}

// ── Hit testing ───────────────────────────────────────────────────────────────
bool PopupWindow::HitTestClose(int x, int y) const {
    return x >= m_closeRect.left && x <= m_closeRect.right &&
           y >= m_closeRect.top  && y <= m_closeRect.bottom;
}

bool PopupWindow::HitTestPause(int x, int y) const {
    return x >= m_pauseRect.left && x <= m_pauseRect.right &&
           y >= m_pauseRect.top  && y <= m_pauseRect.bottom;
}

// ── Timer handler ─────────────────────────────────────────────────────────────
LRESULT PopupWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_POPUP_ANIM_ENTER) {
        DWORD elapsed = GetTickCount() - m_animStart;
        float t = std::min(1.f, (float)elapsed / m_animDuration);
        Render(t);
        if (t >= 1.f) {
            KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
            m_state = PopupAnimState::Visible;
            SetupAutoDismiss();

            // Record delivery
            if (m_item) {
                Database::Instance().InsertReminderEvent(m_item->id, m_item->category, "delivered");
            }
        }
    } else if (timerId == TIMER_POPUP_ANIM_EXIT) {
        DWORD elapsed = GetTickCount() - m_animStart;
        float t = std::min(1.f, (float)elapsed / m_animDuration);
        Render(t);
        if (t >= 1.f) {
            KillTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT);
            ShowWindow(m_hwnd, SW_HIDE);
            m_state = PopupAnimState::Hidden;
            m_item  = nullptr;
        }
    } else if (timerId == TIMER_POPUP_AUTODISMISS) {
        KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
        StartExitAnim();
    }
    return 0;
}

// ── Mouse handlers ────────────────────────────────────────────────────────────
LRESULT PopupWindow::OnMouseMove(int x, int y) {
    if (!m_mouseTracking) {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hwnd, 0 };
        TrackMouseEvent(&tme);
        m_mouseTracking = true;
    }

    bool wasHovering = m_hovering;
    m_hovering = true;
    if (!wasHovering) {
        // Pause the auto-dismiss timer while hovering
        CancelAutoDismiss();
        Render(1.f);
    }
    return 0;
}

LRESULT PopupWindow::OnMouseLeave() {
    m_hovering      = false;
    m_mouseTracking = false;
    // Resume auto-dismiss
    SetupAutoDismiss();
    Render(1.f);
    return 0;
}

LRESULT PopupWindow::OnLButtonDown(int x, int y) {
    if (HitTestClose(x, y)) {
        // Dismiss
        CancelAutoDismiss();
        if (m_item) {
            Database::Instance().InsertReminderEvent(m_item->id, m_item->category, "dismissed");
        }
        StartExitAnim();
        return 0;
    } else if (HitTestPause(x, y)) {
        // Quick pause 30 min via tray menu approach
        CancelAutoDismiss();
        StartExitAnim();
        Scheduler::Instance().Pause(30);
        return 0;
    } else {
        // Click anywhere else = dismiss
        CancelAutoDismiss();
        StartExitAnim();
    }
    return 0;
}

// ── Window procedure ──────────────────────────────────────────────────────────
LRESULT CALLBACK PopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PopupWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<PopupWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<PopupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_TIMER:        return self->OnTimer((UINT_PTR)wp);
        case WM_MOUSEMOVE:    return self->OnMouseMove(LOWORD(lp), HIWORD(lp));
        case WM_MOUSELEAVE:   return self->OnMouseLeave();
        case WM_LBUTTONDOWN:  return self->OnLButtonDown(LOWORD(lp), HIWORD(lp));
        case WM_NCHITTEST:
            return HTCLIENT;

        case WM_DESTROY:
            return 0;

        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wp);
            RECT* newRect = reinterpret_cast<RECT*>(lp);
            return self->OnDpiChanged(newDpi, newRect);
        }

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

LRESULT PopupWindow::OnDpiChanged(UINT newDpi, const RECT* newRect) {
    m_dpiScale = newDpi / 96.0f;

    SetWindowPos(m_hwnd, nullptr, newRect->left, newRect->top,
                 newRect->right - newRect->left, newRect->bottom - newRect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Recreate text formats with new DPI
    if (m_textFmtMainLarge) { m_textFmtMainLarge->Release(); m_textFmtMainLarge = nullptr; }
    if (m_textFmtMainMedium) { m_textFmtMainMedium->Release(); m_textFmtMainMedium = nullptr; }
    if (m_textFmtMainCompact) { m_textFmtMainCompact->Release(); m_textFmtMainCompact = nullptr; }
    if (m_textFmtMain) { m_textFmtMain->Release(); m_textFmtMain = nullptr; }
    if (m_textFmtSub) { m_textFmtSub->Release(); m_textFmtSub = nullptr; }
    if (m_textFmtSmall) { m_textFmtSmall->Release(); m_textFmtSmall = nullptr; }

    // This will skip factory creation but run format creation
    InitD2D();

    return 0;
}

} // namespace dhikr
