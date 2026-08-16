#include "MainWindow.h"
#include "DesignSystem.h"
#include "TrayIcon.h"
#include "PopupWindow.h"
#include "core/LogoRenderer.h"
#include "core/ResourceLoader.h"
#include "pages/HomePage.h"
#include "pages/RemindersPage.h"
#include "pages/SchedulePage.h"
#include "pages/LibraryPage.h"
#include "pages/StatisticsPage.h"
#include "pages/SettingsPage.h"
#include "pages/AboutPage.h"
#include "../core/Application.h"
#include "../core/Settings.h"
#include "../core/Scheduler.h"
#include "../core/ContentEngine.h"
#include "../core/Logger.h"
#include "../core/Localization.h"
#include "../core/ApplicationMetadata.h"
#include "../update/Updater.h"
#include "../core/FullScreenDetector.h"
#include "../resource.h"
#include <algorithm>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <filesystem>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dhikr {

#define TIMER_FULLSCREEN_RESUME 1002



static std::wstring GetNavLabel(int i) {
    const wchar_t* keys[] = {
        L"nav.home", L"nav.reminders", L"nav.schedule", L"nav.library",
        L"nav.statistics", L"nav.settings", L"nav.about"
    };
    return Localization::Instance().Get(keys[i]);
}

MainWindow& MainWindow::Instance() {
    static MainWindow instance;
    return instance;
}

MainWindow::~MainWindow() {
    ReleaseD2D();
}

bool MainWindow::Create(HINSTANCE hInstance) {
    m_hInst = hInstance;

    HDC hdc = GetDC(nullptr);
    float dpiX = (float)GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    m_dpiScale = dpiX / 96.f;

    m_width  = (int)(820.f * m_dpiScale);
    m_height = (int)(560.f * m_dpiScale);

    RegisterClass(hInstance);
    if (!CreateWindow_(hInstance)) return false;
    if (!InitD2D()) return false;
    InitTextFormats();
    InitPages(hInstance);

    // Create popup window
    PopupWindow::Instance().Create(hInstance);

    // Also trigger update check in background (10 minutes after startup)
    Updater::Instance().CheckIfDue(m_hwnd);

    LOG_INFO("Main window created");
    return true;
}

void MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

    // Load icon resources; fall back to the system default app icon
    // if the .ico files were not embedded (e.g. setup_deps.bat failed).
    wc.hIcon   = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!wc.hIcon)   wc.hIcon   = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON_SMALL));
    if (!wc.hIconSm) wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

bool MainWindow::CreateWindow_(HINSTANCE hInstance) {
    // Center on primary monitor
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - m_width)  / 2;
    int y = (screenH - m_height) / 2;

    DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClassName,
        ApplicationMetadata::AppName,
        style,
        x, y, m_width, m_height,
        nullptr, nullptr, hInstance, this);

    if (m_hwnd) {
        // Enable DWM Immersive Dark Mode (modern Win10 / Win11)
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
        DwmSetWindowAttribute(m_hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &darkMode, sizeof(darkMode));

        // Request rounded corners on Windows 11
        DWORD cornerPref = 2; // DWMWCP_ROUNDED
        DwmSetWindowAttribute(m_hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

        COLORREF captionColor = RGB(16, 19, 16);
        DwmSetWindowAttribute(m_hwnd, 35 /* DWMWA_CAPTION_COLOR */, &captionColor, sizeof(captionColor));

        COLORREF borderColor = RGB(36, 41, 36);
        DwmSetWindowAttribute(m_hwnd, 34 /* DWMWA_BORDER_COLOR */, &borderColor, sizeof(borderColor));

        m_navRail = std::make_shared<NavigationRail>();
        float sw = ds::Dims::SidebarW * m_dpiScale;
        bool isRtl = Localization::Instance().IsRTL();
        float sbX = isRtl ? (m_width - sw) : 0.f;
        m_navRail->SetBounds(D2D1::RectF(sbX, ds::Dims::TitleBarH * m_dpiScale, sbX + sw, (float)m_height));
        m_navRail->OnNavigate = [this](PageId p) {
            LOG_INFO("[NAV] Navigation to page " + std::to_string(static_cast<int>(p)));
            UIStateManager::Instance().SetPage(p);
        };
        InputController::Instance().AddControl(m_navRail);
        
        HICON hBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
        HICON hSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
        if (hBig) SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        if (hSmall) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);

        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }

    LOG_INFO("[WINDOW] Main HWND = " + std::to_string((uintptr_t)m_hwnd));
    LOG_INFO("[WINDOW] Thread ID = " + std::to_string(GetCurrentThreadId()));

    return m_hwnd != nullptr;
}

bool MainWindow::InitD2D() {
    m_d2dFactory    = Application::Instance().D2DFactory();
    m_dwriteFactory = Application::Instance().DWriteFactory();

    D2D1_SIZE_U size = D2D1::SizeU(m_width, m_height);
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        rtp,
        D2D1::HwndRenderTargetProperties(m_hwnd, size,
            D2D1_PRESENT_OPTIONS_NONE),
        &m_renderTarget);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D2D render target");
        return false;
    }

    hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &m_brush);
    if (FAILED(hr) || !m_brush) {
        // Brush is required for all painting; treat this as a fatal init failure
        // so the caller can decide whether to retry or fall back to GDI.
        LOG_ERROR("Failed to create D2D solid brush");
        m_brush = nullptr;
        // We still return true so the window is shown; OnPaint has a GDI fallback.
    }

    // Initialize WIC and load logo from embedded resource at full native resolution
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
    if (SUCCEEDED(hr) && m_wicFactory && m_renderTarget) {
        m_logoBitmap = ResourceLoader::LoadBitmapFromResource(
            m_hInst, IDR_LOGO, m_wicFactory, m_renderTarget);
        if (m_logoBitmap) {
            LOG_INFO("MainWindow: Embedded logo loaded successfully from resource");
        } else {
            LOG_WARN("MainWindow: Failed to load embedded logo from resource");
        }
    }

    return true;
}

bool MainWindow::InitTextFormats() {
    auto tryCreateFormat = [&](IDWriteTextFormat** ppFmt,
                                DWRITE_FONT_WEIGHT weight, float size) {
        HRESULT hr = m_dwriteFactory->CreateTextFormat(
            L"Noto Sans", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size * m_dpiScale, L"en-US", ppFmt);
        if (FAILED(hr) || !*ppFmt) {
            m_dwriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr, weight,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                size * m_dpiScale, L"en-US", ppFmt);
        }
    };

    tryCreateFormat(&m_tfNavLabel, DWRITE_FONT_WEIGHT_REGULAR,  ds::FontSize::NavLabel);
    tryCreateFormat(&m_tfAppName,  DWRITE_FONT_WEIGHT_SEMI_BOLD, ds::FontSize::AppTitle);
    tryCreateFormat(&m_tfVersion,  DWRITE_FONT_WEIGHT_REGULAR,   ds::FontSize::Version);

    // Icon font — dynamically resolve Segoe Fluent Icons (Win 11) or Segoe MDL2 Assets (Win 10/11)
    const wchar_t* iconFontFamily = ds::Icon::GetFontFamily(m_dwriteFactory);
    m_dwriteFactory->CreateTextFormat(
        iconFontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        ds::Dims::NavIconSize * m_dpiScale, L"en-US", &m_tfIcon);

    if (m_tfIcon) {
        m_tfIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfVersion) m_tfVersion->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    
    // We update NavLabel reading direction dynamically in DrawSidebarItem or Draw.
    if (m_tfNavLabel) m_tfNavLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    return true;
}

void MainWindow::InitPages(HINSTANCE hInstance) {
    m_pages[PAGE_HOME]       = std::make_unique<HomePage>();
    m_pages[PAGE_REMINDERS]  = std::make_unique<RemindersPage>();
    m_pages[PAGE_SCHEDULE]   = std::make_unique<SchedulePage>();
    m_pages[PAGE_LIBRARY]    = std::make_unique<LibraryPage>();
    m_pages[PAGE_STATISTICS] = std::make_unique<StatisticsPage>();
    m_pages[PAGE_SETTINGS]   = std::make_unique<SettingsPage>();
    m_pages[PAGE_ABOUT]      = std::make_unique<AboutPage>();
}

void MainWindow::ReleaseD2D() {
    if (m_logoBitmap)    { m_logoBitmap->Release();    m_logoBitmap    = nullptr; }
    if (m_wicFactory)    { m_wicFactory->Release();    m_wicFactory    = nullptr; }
    if (m_tfVersion)     { m_tfVersion->Release();     m_tfVersion     = nullptr; }
    if (m_tfAppName)     { m_tfAppName->Release();     m_tfAppName     = nullptr; }
    if (m_tfNavLabel)    { m_tfNavLabel->Release();    m_tfNavLabel    = nullptr; }
    if (m_tfIcon)        { m_tfIcon->Release();        m_tfIcon        = nullptr; }
    if (m_brush)         { m_brush->Release();         m_brush         = nullptr; }
    if (m_renderTarget)  { m_renderTarget->Release();  m_renderTarget  = nullptr; }
}

void MainWindow::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    UpdateWindow(m_hwnd);
}

void MainWindow::Hide() {
    ShowWindow(m_hwnd, SW_HIDE);
}

void MainWindow::NavigateTo(int pageId) {
    if (pageId < 0 || pageId >= kNavItems) return;
    m_activePage = pageId;
    if (m_pages[m_activePage]) {
        m_pages[m_activePage]->OnActivated();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

D2D1_RECT_F MainWindow::SidebarRect() const {
    float sw = ds::Dims::SidebarW * m_dpiScale;
    float th = ds::Dims::TitleBarH * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    if (isRtl) return D2D1::RectF((float)m_width - sw, th, (float)m_width, (float)m_height);
    return D2D1::RectF(0, th, sw, (float)m_height);
}

D2D1_RECT_F MainWindow::ContentRect() const {
    float sw = ds::Dims::SidebarW * m_dpiScale;
    float th = ds::Dims::TitleBarH * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    if (isRtl) return D2D1::RectF(0, th, (float)m_width - sw, (float)m_height);
    return D2D1::RectF(sw, th, (float)m_width, (float)m_height);
}

// ── State ───────────────────────────────────────────────────────────────────
void MainWindow::OnPaint() {
    // Always call BeginPaint/EndPaint to validate (clear) the dirty region
    // reported by Windows. Failure to do so causes WM_PAINT to be sent
    // continuously, resulting in a perpetually blank window.
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    if (!m_renderTarget || !m_brush) {
        // ── GDI fallback ────────────────────────────────────────────────────
        // D2D is not ready (e.g. brush creation failed, or device lost).
        // Paint a solid dark background so the window is never blank white.
        if (hdc) {
            HBRUSH fb = CreateSolidBrush(RGB(0x10, 0x13, 0x10)); // ds::Dark::Surface
            if (fb) { FillRect(hdc, &ps.rcPaint, fb); DeleteObject(fb); }
            else    { FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(DKGRAY_BRUSH)); }
        }
        EndPaint(m_hwnd, &ps);
        return;
    }

    // ── D2D path ────────────────────────────────────────────────────────────
    // BeginPaint/EndPaint validate the dirty region; D2D draws independently
    // into the HwndRenderTarget which is bound to the same HWND.
    EndPaint(m_hwnd, &ps); // release the GDI DC — D2D doesn't use it

    m_renderTarget->BeginDraw();

    // Force-clear to solid background
    auto bg = ds::Background();
    m_renderTarget->Clear(bg);

    DrawBackground(m_renderTarget);
    DrawSidebar(m_renderTarget);
    WindowChrome::Instance().Draw(m_renderTarget, m_brush, m_tfAppName, m_tfIcon, m_logoBitmap);
    DrawCurrentPage(m_renderTarget);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseD2D();
        InitD2D();
        InitTextFormats();
        InvalidateRect(m_hwnd, nullptr, FALSE); // request a fresh repaint
    }
}

void MainWindow::DrawBackground(ID2D1RenderTarget* rt) {
    if (!m_brush) return;
    m_brush->SetColor(ds::Background());
    rt->FillRectangle(D2D1::RectF(0, 0, (float)m_width, (float)m_height), m_brush);
}

void MainWindow::DrawSidebar(ID2D1RenderTarget* rt) {
    if (!m_brush) return;
    auto sr = SidebarRect();

    // Sidebar background
    m_brush->SetColor(ds::Sidebar());
    rt->FillRectangle(sr, m_brush);

    // Right border (or left if RTL)
    m_brush->SetColor(ds::Border());
    if (Localization::Instance().IsRTL()) {
        rt->DrawLine(
            D2D1::Point2F(sr.left, sr.top),
            D2D1::Point2F(sr.left, (float)m_height),
            m_brush, 0.5f);
    } else {
        rt->DrawLine(
            D2D1::Point2F(sr.right, sr.top),
            D2D1::Point2F(sr.right, (float)m_height),
            m_brush, 0.5f);
    }

    // Nav items
    for (int i = 0; i < kNavItems; ++i) {
        std::wstring label = GetNavLabel(i);
        DrawSidebarItem(rt, i, label.c_str(),
            m_navRail->GetItemRect(i).top, i == m_activePage, i == m_navRail->GetHoveredItem());
    }

    DrawSidebarBottom(rt);
}

void MainWindow::DrawSidebarItem(ID2D1RenderTarget* rt, int index,
                                  const wchar_t* label, float y,
                                  bool active, bool hover) {
    auto r = m_navRail->GetItemRect(index);
    float sw = ds::Dims::SidebarW * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Active / hover pill
    float rx = 6.f * m_dpiScale;
    float pillPad = 6.f * m_dpiScale;
    D2D1_RECT_F pillRect = D2D1::RectF(r.left + pillPad, r.top + 2.f*m_dpiScale, r.right - pillPad, r.bottom - 2.f*m_dpiScale);

    if (active) {
        m_brush->SetColor(ds::ActiveNavBg());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, rx, rx), m_brush);

        // Accent indicator bar
        m_brush->SetColor(ds::Accent::Primary());
        float indW = 3.f * m_dpiScale;
        float indTop    = pillRect.top    + 6.f * m_dpiScale;
        float indBottom = pillRect.bottom - 6.f * m_dpiScale;
        D2D1_RECT_F bar = isRtl
            ? D2D1::RectF(pillRect.right - indW, indTop, pillRect.right, indBottom)
            : D2D1::RectF(pillRect.left,         indTop, pillRect.left + indW, indBottom);
        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.5f, 1.5f), m_brush);
    } else if (hover) {
        m_brush->SetColor(ds::CardHover());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, rx, rx), m_brush);
    }

    // Icon
    float iconSize = ds::Dims::NavIconSize * m_dpiScale;
    float cx = isRtl ? (r.right - 22.f * m_dpiScale - iconSize / 2.f) : (r.left + 22.f * m_dpiScale + iconSize / 2.f);
    float cy = r.top + (r.bottom - r.top) / 2.f;
    auto iconColor = active ? ds::Accent::Primary() : (hover ? ds::TextPrimary() : ds::TextSecondary());
    DrawNavIcon(rt, index, cx, cy, iconSize, iconColor);

    // Label
    if (m_tfNavLabel) {
        m_tfNavLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfNavLabel->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(active ? ds::TextPrimary() : (hover ? ds::TextPrimary() : ds::TextSecondary()));
        
        float textX = isRtl ? (r.left + 12.f * m_dpiScale) : (cx + iconSize / 2.f + 12.f * m_dpiScale);
        float textXEnd = isRtl ? (cx - iconSize / 2.f - 12.f * m_dpiScale) : (r.right - 12.f * m_dpiScale);
        
        D2D1_RECT_F textRect = D2D1::RectF(textX, r.top, textXEnd, r.bottom);
        rt->DrawTextW(label, (UINT32)wcslen(label), m_tfNavLabel, textRect, m_brush);
    }
}

void MainWindow::DrawNavIcon(ID2D1RenderTarget* rt, int pageId,
                               float cx, float cy, float size,
                               D2D1_COLOR_F color) {
    if (!m_tfIcon || !m_brush) return;
    m_brush->SetColor(color);
    
    const wchar_t* glyph = ds::Icon::Home;
    switch (pageId) {
        case PAGE_HOME:       glyph = ds::Icon::Home; break;
        case PAGE_REMINDERS:  glyph = ds::Icon::Reminders; break;
        case PAGE_SCHEDULE:   glyph = ds::Icon::Schedule; break;
        case PAGE_LIBRARY:    glyph = ds::Icon::Library; break;
        case PAGE_STATISTICS: glyph = ds::Icon::Statistics; break;
        case PAGE_SETTINGS:   glyph = ds::Icon::Settings; break;
        case PAGE_ABOUT:      glyph = ds::Icon::About; break;
    }

    // Measure to center exactly
    D2D1_RECT_F rect = D2D1::RectF(cx - size, cy - size, cx + size, cy + size);
    rt->DrawTextW(glyph, 1, m_tfIcon, rect, m_brush);
}

void MainWindow::DrawSidebarBottom(ID2D1RenderTarget* rt) {
    if (!m_brush) return;
    auto sr = SidebarRect();
    float pad = ds::Space::L * m_dpiScale;
    float bottomY = (float)m_height - 40.f * m_dpiScale;

    // Logo
    float cx = sr.left + (sr.right - sr.left) / 2.f;
    float cy = bottomY - 14.f * m_dpiScale;
    float size = 13.f * m_dpiScale;
    D2D1_RECT_F rect = D2D1::RectF(cx - size, cy - size, cx + size, cy + size);
    if (m_logoBitmap) {
        D2D1_SIZE_F sz = m_logoBitmap->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(rect, sz.width, sz.height);
        rt->DrawBitmap(m_logoBitmap, fitRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, rect, 1.0f);
    }

    // Version string
    if (m_tfVersion) {
        m_brush->SetColor(ds::TextMuted());
        std::wstring vstr = Localization::Instance().Get(L"about.version") + L" " + ApplicationMetadata::AppVersion;
        D2D1_RECT_F vr = D2D1::RectF(sr.left, bottomY + 8.f*m_dpiScale, sr.right, bottomY + 30.f*m_dpiScale);
        rt->DrawTextW(vstr.c_str(), (UINT32)vstr.size(), m_tfVersion, vr, m_brush);
    }
}

void MainWindow::DrawCurrentPage(ID2D1RenderTarget* rt) {
    auto cr = ContentRect();
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        m_pages[m_activePage]->Draw(rt, cr);
    }
}

// ── Message handlers ──────────────────────────────────────────────────────────
LRESULT MainWindow::OnSize(int w, int h) {
    m_width  = w;
    m_height = h;
    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(w, h));
    }
    
    WindowChrome::Instance().UpdateMetrics(m_dpiScale, w, h);
    if (m_navRail) {
        m_navRail->SetBounds(SidebarRect());
    }
    
    auto cr = ContentRect();
    for (auto& p : m_pages) {
        if (p) p->OnSize(cr);
    }
    return 0;
}

LRESULT MainWindow::OnCommand(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_TRAY_OPEN:
            Show();
            break;
        case ID_TRAY_PAUSE_15:   Scheduler::Instance().Pause(15);  TrayIcon::Instance().SetPaused(true);  break;
        case ID_TRAY_PAUSE_30:   Scheduler::Instance().Pause(30);  TrayIcon::Instance().SetPaused(true);  break;
        case ID_TRAY_PAUSE_60:   Scheduler::Instance().Pause(60);  TrayIcon::Instance().SetPaused(true);  break;
        case ID_TRAY_PAUSE_120:  Scheduler::Instance().Pause(120); TrayIcon::Instance().SetPaused(true);  break;
        case ID_TRAY_PAUSE_TODAY: Scheduler::Instance().PauseUntilTomorrow(); TrayIcon::Instance().SetPaused(true); break;
        case ID_TRAY_RESUME:     Scheduler::Instance().Resume();    TrayIcon::Instance().SetPaused(false); break;
        case ID_TRAY_SETTINGS:   Show(); NavigateTo(PAGE_SETTINGS); break;
        case ID_TRAY_STATISTICS: Show(); NavigateTo(PAGE_STATISTICS); break;
        case ID_TRAY_ABOUT:      Show(); NavigateTo(PAGE_ABOUT); break;
        case ID_TRAY_EXIT:
            Application::Instance().Shutdown();
            PostQuitMessage(0);
            break;
        case ID_TRAY_NEXT: {
            // Show next reminder immediately
            auto* item = ContentEngine::Instance().GetNext();
            if (item) PopupWindow::Instance().Show(item);
            break;
        }
    }
    return 0;
}

LRESULT MainWindow::OnReminder() {
    auto* item = ContentEngine::Instance().GetNext();
    if (item) {
        // Fullscreen distraction protection check
        if (Settings::Instance().Get().suppressDuringFullscreen && FullScreenDetector::Instance().IsFullScreenActive()) {
            int mode = Settings::Instance().Get().fullscreenSuppressionMode;
            if (mode == 2) {
                // Windows notification only
                LOG_INFO("[FULLSCREEN] Fullscreen active — delivering via quiet Windows notification");
                TrayIcon::Instance().ShowBalloon(ApplicationMetadata::AppName, item->text, NIIF_INFO);
                return 0;
            } else if (mode == 1) {
                // Suppress completely
                LOG_INFO("[FULLSCREEN] Fullscreen active — reminder suppressed");
                return 0;
            } else {
                // Queue until fullscreen ends (Default)
                LOG_INFO("[FULLSCREEN] Fullscreen active — queuing reminder until fullscreen ends");
                m_pendingFullscreenReminder = item;
                SetTimer(m_hwnd, TIMER_FULLSCREEN_RESUME, 2000, nullptr);
                return 0;
            }
        }

        PopupWindow::Instance().Show(item);
        // Update tray tooltip
        time_t next = Scheduler::Instance().GetNextReminderTime();
        if (next > 0) {
            struct tm lt;
            localtime_s(&lt, &next);
            wchar_t buf[64];
            wcsftime(buf, 64, L"Next reminder at %I:%M %p", &lt);
            TrayIcon::Instance().UpdateTooltip(buf);
        }
    } else {
        // No content loaded
        TrayIcon::Instance().ShowBalloon(ApplicationMetadata::AppName,
            L"Please import a content file to receive reminders.", NIIF_INFO);
    }
    return 0;
}

LRESULT MainWindow::OnSettingsChanged() {
    if (m_navRail) {
        m_navRail->SetBounds(SidebarRect());
    }
    WindowChrome::Instance().UpdateMetrics(m_dpiScale, m_width, m_height);

    // Update icons for Tray and Taskbar Window
    TrayIcon::Instance().UpdateIcon();
    HICON hBig = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
    HICON hSmall = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (hBig) SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
    if (hSmall) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);

    InitPages(m_hInst);
    auto cr = ContentRect();
    for (auto& p : m_pages) {
        if (p) {
            p->OnSize(cr);
            p->OnSettingsChanged();
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_FULLSCREEN_RESUME) {
        if (m_pendingFullscreenReminder) {
            if (!FullScreenDetector::Instance().IsFullScreenActive()) {
                KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
                const ContentItem* itemToShow = m_pendingFullscreenReminder;
                m_pendingFullscreenReminder = nullptr;
                LOG_INFO("[FULLSCREEN] Fullscreen mode ended — displaying queued reminder");
                PopupWindow::Instance().Show(itemToShow);
            }
        } else {
            KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
        }
        return 0;
    }
    return 0;
}

LRESULT MainWindow::OnMouseMove(int x, int y) {
    if (!m_mouseTracking) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_mouseTracking = true;
    }

    bool needsRepaint = false;
    if (WindowChrome::Instance().OnMouseMove((float)x, (float)y)) needsRepaint = true;
    if (InputController::Instance().OnMouseMove((float)x, (float)y)) needsRepaint = true;
    
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnMouseMove(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnMouseLeave() {
    m_mouseTracking = false;
    bool needsRepaint = false;
    if (WindowChrome::Instance().OnMouseLeave()) needsRepaint = true;
    if (InputController::Instance().OnMouseLeave()) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        m_pages[m_activePage]->OnMouseLeave();
    }
    
    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnMouseWheel(int delta) {
    bool needsRepaint = false;
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnMouseWheel(delta)) needsRepaint = true;
    }
    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnLButtonDown(int x, int y) {
    LOG_INFO("[INPUT] WM_LBUTTONDOWN hwnd=" + std::to_string((uintptr_t)m_hwnd) + " x=" + std::to_string(x) + " y=" + std::to_string(y));
    
    POINT pt;
    GetCursorPos(&pt);
    HWND wndFromPt = WindowFromPoint(pt);
    LOG_INFO("[INPUT] WindowFromPoint=" + std::to_string((uintptr_t)wndFromPt) + " MainWindow=" + std::to_string((uintptr_t)m_hwnd));

    bool needsRepaint = false;
    if (WindowChrome::Instance().OnLButtonDown((float)x, (float)y)) needsRepaint = true;
    if (InputController::Instance().OnLButtonDown((float)x, (float)y)) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnLButtonDown(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnLButtonUp(int x, int y) {
    bool needsRepaint = false;
    if (WindowChrome::Instance().OnLButtonUp(m_hwnd, (float)x, (float)y)) needsRepaint = true;
    if (InputController::Instance().OnLButtonUp((float)x, (float)y)) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnLButtonUp(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnSetCursor(HWND hwnd, UINT hitTest, UINT message) {
    if (hitTest == HTCLIENT) {
        bool hand = InputController::Instance().HasHoveredControl();
        if (!hand && m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);
            hand = m_pages[m_activePage]->HitTestInteractive(pt.x, pt.y);
        }
        SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    return DefWindowProcW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(hitTest, message));
}

LRESULT MainWindow::OnNcCalcSize(WPARAM wp, LPARAM lp) {
    if (wp == TRUE) {
        if (IsZoomed(m_hwnd)) {
            auto* pnc = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
            HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{ sizeof(mi) };
            if (GetMonitorInfoW(hMon, &mi)) {
                pnc->rgrc[0] = mi.rcWork;
            }
        }
        return 0;
    }
    return DefWindowProcW(m_hwnd, WM_NCCALCSIZE, wp, lp);
}

LRESULT MainWindow::OnNcHitTest(int x, int y) {
    LRESULT hit = WindowChrome::Instance().HitTest(m_hwnd, x, y);
    return hit;
}

LRESULT MainWindow::OnPowerBroadcast(WPARAM wp) {
    if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        Scheduler::Instance().OnSystemWake();
    }
    return TRUE;
}

LRESULT MainWindow::OnTrayCallback(LPARAM lp) {
    TrayIcon::Instance().HandleMessage(m_hwnd, lp);
    return 0;
}

LRESULT MainWindow::OnGetMinMaxInfo(MINMAXINFO* mmi) {
    mmi->ptMinTrackSize.x = (LONG)(ds::Dims::MainWindowMinW * m_dpiScale);
    mmi->ptMinTrackSize.y = (LONG)(ds::Dims::MainWindowMinH * m_dpiScale);
    return 0;
}

LRESULT MainWindow::OnClose() {
    // Hide to tray instead of closing
    Hide();
    return 0;
}

LRESULT MainWindow::OnDestroy() {
    KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
    m_pendingFullscreenReminder = nullptr;
    PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnUpdateAvailable() {
    const ReleaseInfo& info = Updater::Instance().PendingRelease();
    if (info.installerUrl.empty()) {
        std::wstring msg = Localization::Instance().Get(L"settings.update_available") + L"\n\n" +
                           Localization::Instance().Get(L"settings.version") + L": " + info.version.ToString();
        MessageBoxW(m_hwnd, msg.c_str(), Localization::Instance().Get(L"settings.title").c_str(), MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    std::wstring current(ApplicationMetadata::AppVersion);
    std::wstring newVer = info.version.ToString();
    std::wstring msg = Localization::Instance().Get(L"settings.update_available") + L"\n\n" +
                       Localization::Instance().Get(L"about.version") + L": " + current + L" -> " + newVer + L"\n\n" +
                       Localization::Instance().Get(L"settings.update_now") + L"?";

    int choice = MessageBoxW(m_hwnd, msg.c_str(),
        Localization::Instance().Get(L"settings.title").c_str(),
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);

    if (choice == IDYES) {
        HWND hwnd = m_hwnd;
        Updater::Instance().DownloadUpdate(
            info,
            [hwnd](int pct) {
                static int last = -1;
                if (pct - last >= 10 || pct == 100) {
                    LOG_INFO("Update download: " + std::to_string(pct) + "%");
                    last = pct;
                }
            },
            [hwnd](bool success, const std::wstring& path) {
                if (success) {
                    struct Ctx { std::wstring path; HWND hwnd; };
                    auto* ctx2 = new Ctx{ path, hwnd };
                    PostMessageW(hwnd, WM_APP + 100, 0, (LPARAM)ctx2);
                } else {
                    MessageBoxW(hwnd,
                        Localization::Instance().Get(L"common.error").c_str(),
                        Localization::Instance().Get(L"settings.title").c_str(), MB_ICONERROR | MB_OK);
                }
            });
    }
    return 0;
}

// ── Window procedure ──────────────────────────────────────────────────────────
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_PAINT:
            self->OnPaint();
            return 0;
        case WM_SIZE:             return self->OnSize(LOWORD(lp), HIWORD(lp));
        case WM_COMMAND:          return self->OnCommand(wp);
        case WM_REMINDER_TRIGGER: return self->OnReminder();
        case WM_SETTINGS_CHANGED: return self->OnSettingsChanged();
        case WM_TIMER:            return self->OnTimer((UINT_PTR)wp);
        case WM_MOUSEMOVE:        return self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_MOUSEWHEEL:       return self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        case WM_MOUSELEAVE:       return self->OnMouseLeave();
        case WM_LBUTTONDOWN:      return self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_LBUTTONUP:        return self->OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_CHAR: {
            if (self->m_activePage >= 0 && self->m_activePage < kNavItems && self->m_pages[self->m_activePage]) {
                if (self->m_pages[self->m_activePage]->OnChar((wchar_t)wp)) {
                    self->Invalidate();
                    return 0;
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_KEYDOWN: {
            if (self->m_activePage >= 0 && self->m_activePage < kNavItems && self->m_pages[self->m_activePage]) {
                if (self->m_pages[self->m_activePage]->OnKeyDown(wp)) {
                    self->Invalidate();
                    return 0;
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_SETCURSOR:        return self->OnSetCursor(hwnd, LOWORD(lp), HIWORD(lp));
        case WM_POWERBROADCAST:   return self->OnPowerBroadcast(wp);
        case WM_TRAY_CALLBACK:    return self->OnTrayCallback(lp);
        case WM_GETMINMAXINFO:    return self->OnGetMinMaxInfo((MINMAXINFO*)lp);
        case WM_NCCALCSIZE:       return self->OnNcCalcSize(wp, lp);
        case WM_NCPAINT:          return 0; // prevent standard border painting
        case WM_NCACTIVATE:       return TRUE; // handled — prevents default light non-client border painting
        case WM_NCHITTEST:        return self->OnNcHitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_CLOSE:            return self->OnClose();
        case WM_DESTROY:          return self->OnDestroy();
        case WM_ERASEBKGND:       return 1; // prevent flicker
        case WM_UPDATE_AVAILABLE: return self->OnUpdateAvailable();
        case WM_APP + 100: {
            // Download complete — prompt to install (on UI thread)
            struct Ctx { std::wstring path; HWND hwnd; };
            auto* ctx2 = reinterpret_cast<Ctx*>(lp);
            if (ctx2) {
                int r2 = MessageBoxW(hwnd,
                    L"\u062a\u0645 \u062a\u062d\u0645\u064a\u0644 \u0627\u0644\u062a\u062d\u062f\u064a\u062b.\n\u0647\u0644 \u062a\u0631\u064a\u062f \u062a\u062b\u0628\u064a\u062a\u0647 \u0627\u0644\u0622\u0646?\n(\u0633\u064a\u062a\u0645 \u0625\u063a\u0644\u0627\u0642 \u0627\u0644\u062a\u0637\u0628\u064a\u0642)",
                    L"\u062a\u062b\u0628\u064a\u062a \u0627\u0644\u062a\u062d\u062f\u064a\u062b",
                    MB_YESNO | MB_ICONQUESTION);
                if (r2 == IDYES)
                    Updater::Instance().LaunchInstaller(ctx2->path, hwnd);
                delete ctx2;
            }
            return 0;
        }
        case WM_APP + 101: {
            AppUIState* newState = reinterpret_cast<AppUIState*>(wp);
            if (newState) {
                UIStateManager::Instance().CommitState(*newState);
                delete newState;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        default:                  return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace dhikr
