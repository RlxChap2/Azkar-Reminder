#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "../core/Settings.h"

namespace dhikr::ds {

// ════════════════════════════════════════════════════════════════════════════
// DESIGN SYSTEM  —  Single source of truth for all visual tokens
// Pixel-accurate to the provided design reference.
// ════════════════════════════════════════════════════════════════════════════

// ── Color helpers ────────────────────────────────────────────────────────────
inline D2D1_COLOR_F Rgba(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
    return D2D1::ColorF(r / 255.f, g / 255.f, b / 255.f, a);
}
inline D2D1_COLOR_F Lerp(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
    return D2D1::ColorF(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t);
}
inline COLORREF D2DToColorRef(D2D1_COLOR_F c) {
    return RGB((int)(c.r*255), (int)(c.g*255), (int)(c.b*255));
}

// ════════════════════════════════════════════════════════════════════════════
// DARK MODE PALETTE (Clean, high-contrast, official dark theme)
// ════════════════════════════════════════════════════════════════════════════
namespace Dark {
    // Backgrounds (calm dark neutrals)
    inline D2D1_COLOR_F Background()      { return Rgba(0x08, 0x0A, 0x09, 1.0f); }
    inline D2D1_COLOR_F Sidebar()         { return Rgba(0x10, 0x13, 0x10, 1.0f); }
    inline D2D1_COLOR_F Surface()         { return Rgba(0x15, 0x19, 0x15, 1.0f); }
    inline D2D1_COLOR_F Card()            { return Rgba(0x15, 0x19, 0x15, 1.0f); }
    inline D2D1_COLOR_F CardHover()       { return Rgba(0x1B, 0x22, 0x1C, 1.0f); }
    inline D2D1_COLOR_F ActiveNavBg()     { return Rgba(0x18, 0x23, 0x19, 0.85f); }

    // Borders
    inline D2D1_COLOR_F Border()          { return Rgba(0x24, 0x29, 0x24); }
    inline D2D1_COLOR_F BorderSubtle()    { return Rgba(0x1A, 0x20, 0x1B); }

    // Text
    inline D2D1_COLOR_F TextPrimary()     { return Rgba(0xF1, 0xF4, 0xEF); }
    inline D2D1_COLOR_F TextSecondary()   { return Rgba(0x9C, 0xA3, 0x9C); }
    inline D2D1_COLOR_F TextMuted()       { return Rgba(0x65, 0x70, 0x66); }

    // Popup
    inline D2D1_COLOR_F PopupBg()         { return Rgba(0x0F, 0x14, 0x10, 0.98f); }
    inline D2D1_COLOR_F PopupBorder()     { return Rgba(0x24, 0x2F, 0x26); }

    // Buttons
    inline D2D1_COLOR_F BtnDanger()       { return Rgba(0xE8, 0x11, 0x23); }
    inline D2D1_COLOR_F BtnDangerHover()  { return Rgba(0xF1, 0x70, 0x7B); }
}

// ════════════════════════════════════════════════════════════════════════════
// ACCENT COLOR (customizable)
// ════════════════════════════════════════════════════════════════════════════
namespace Accent {
    inline D2D1_COLOR_F Primary(float alpha = 1.0f) {
        auto& c = Settings::Instance().Get().accentColor;
        return Rgba(c.r, c.g, c.b, alpha);
    }
    inline D2D1_COLOR_F Dark(float alpha = 1.0f) {
        auto& c = Settings::Instance().Get().accentColor;
        return Rgba((uint8_t)(c.r * 0.70f), (uint8_t)(c.g * 0.70f),
                    (uint8_t)(c.b * 0.70f), alpha);
    }
    inline D2D1_COLOR_F Glow(float alpha = 0.08f) {
        return Primary(alpha);
    }
    inline D2D1_COLOR_F Muted(float alpha = 1.0f) {
        auto& c = Settings::Instance().Get().accentColor;
        return Rgba((uint8_t)(c.r * 0.50f), (uint8_t)(c.g * 0.50f),
                    (uint8_t)(c.b * 0.50f), alpha);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// THEME ACCESSORS  (Permanent Dark Theme)
// ════════════════════════════════════════════════════════════════════════════
inline constexpr bool IsDark() { return true; }

inline D2D1_COLOR_F Background()    { return Dark::Background();    }
inline D2D1_COLOR_F Sidebar()       { return Dark::Sidebar();       }
inline D2D1_COLOR_F Surface()       { return Dark::Surface();       }
inline D2D1_COLOR_F Card()          { return Dark::Card();          }
inline D2D1_COLOR_F CardHover()     { return Dark::CardHover();     }
inline D2D1_COLOR_F ActiveNavBg()   { return Dark::ActiveNavBg();   }
inline D2D1_COLOR_F Border()        { return Dark::Border();        }
inline D2D1_COLOR_F BorderSubtle()  { return Dark::BorderSubtle();  }
inline D2D1_COLOR_F TextPrimary()   { return Dark::TextPrimary();   }
inline D2D1_COLOR_F TextSecondary() { return Dark::TextSecondary(); }
inline D2D1_COLOR_F TextMuted()     { return Dark::TextMuted();     }
inline D2D1_COLOR_F PopupBg()       { return Dark::PopupBg();       }
inline D2D1_COLOR_F PopupBorder()   { return Dark::PopupBorder();   }
inline D2D1_COLOR_F BtnDanger()     { return Dark::BtnDanger();     }
inline D2D1_COLOR_F BtnDangerHover(){ return Dark::BtnDangerHover(); }

// ════════════════════════════════════════════════════════════════════════════
// SPACING SYSTEM  (4px base grid)
// ════════════════════════════════════════════════════════════════════════════
namespace Space {
    constexpr float XS  =  4.f;
    constexpr float S   =  8.f;
    constexpr float M   = 12.f;
    constexpr float L   = 16.f;
    constexpr float XL  = 24.f;
    constexpr float XXL = 32.f;
    constexpr float X3L = 40.f;
    constexpr float X4L = 48.f;
}

// ════════════════════════════════════════════════════════════════════════════
// BORDER RADIUS
// ════════════════════════════════════════════════════════════════════════════
namespace Radius {
    constexpr float XS   = 3.f;
    constexpr float S    = 6.f;
    constexpr float M    = 8.f;
    constexpr float L    = 12.f;
    constexpr float XL   = 16.f;
    constexpr float Full = 999.f;
}

// ════════════════════════════════════════════════════════════════════════════
// DIMENSIONS
// ════════════════════════════════════════════════════════════════════════════
namespace Dims {
    // Main window
    constexpr float MainWindowW  = 880.f;
    constexpr float MainWindowH  = 620.f;
    constexpr float MainWindowMinW = 740.f;
    constexpr float MainWindowMinH = 500.f;

    // Custom Title bar
    constexpr float TitleBarH    = 36.f;
    constexpr float CaptionBtnW  = 46.f;

    // Sidebar
    constexpr float SidebarW     = 180.f;
    constexpr float NavItemH     = 42.f;
    constexpr float NavIconSize  = 16.f; // Segoe MDL2 icon size
    constexpr float NavIndicatorW = 3.f;

    // Popup
    constexpr float PopupW       = 420.f;
    constexpr float PopupH       = 100.f; // grows with content
    constexpr float PopupMinH    = 96.f;
    constexpr float PopupMaxH    = 180.f;
    constexpr float PopupAccentW = 3.f;
    constexpr float PopupRadius  = 16.f;
    constexpr float PopupIconSize = 24.f;
    constexpr float PopupMargin  = 16.f; // margin from screen edge

    // Cards
    constexpr float CardRadius   = 8.f;
    constexpr float CardPadding  = 16.f;

    // Toggles
    constexpr float ToggleW      = 42.f;
    constexpr float ToggleH      = 24.f;
    constexpr float ToggleKnobD  = 18.f;

    // Progress bar
    constexpr float ProgressH    = 4.f;

    // Buttons
    constexpr float BtnH         = 36.f;
    constexpr float BtnRadius    = 10.f;

    // Input
    constexpr float InputH       = 34.f;
    constexpr float InputRadius  = 6.f;
}

// ════════════════════════════════════════════════════════════════════════════
// FONT SIZES (in DWRITE points — approximate, adjust for DPI)
// ════════════════════════════════════════════════════════════════════════════
namespace FontSize {
    constexpr float AppTitle    = 13.f;
    constexpr float PageTitle   = 22.f; // 24px equivalent
    constexpr float CardTitle   = 12.f;
    constexpr float BigNumber   = 32.f;
    constexpr float MedNumber   = 22.f;
    constexpr float Body        = 13.f; // 14px equivalent
    constexpr float Secondary   = 11.5f;// 12px equivalent
    constexpr float Small       = 10.5f;
    constexpr float PopupMain   = 17.f;
    constexpr float PopupSub    = 11.f;
    constexpr float NavLabel    = 12.5f;
    constexpr float TabLabel    = 12.f;
    constexpr float BtnLabel    = 12.f;
    constexpr float InputText   = 12.f;
    constexpr float Version     = 10.f;
}

// ════════════════════════════════════════════════════════════════════════════
// ICONS (Segoe MDL2 Assets Unicode Codepoints)
// ════════════════════════════════════════════════════════════════════════════
namespace Icon {
    // Navigation
    constexpr const wchar_t* Home         = L"\xE80F";
    constexpr const wchar_t* Reminders    = L"\xEA8F"; // Ringer/Bell
    constexpr const wchar_t* Schedule     = L"\xE787"; // Calendar
    constexpr const wchar_t* Library      = L"\xE82D"; // Library/Book
    constexpr const wchar_t* Statistics   = L"\xE9D9"; // Chart
    constexpr const wchar_t* Settings     = L"\xE713"; // Gear
    constexpr const wchar_t* About        = L"\xE946"; // Info
    constexpr const wchar_t* Moon         = L"\xE708"; // Moon / ClearNight

    // Window controls
    constexpr const wchar_t* Minimize     = L"\xE921";
    constexpr const wchar_t* Maximize     = L"\xE922";
    constexpr const wchar_t* Restore      = L"\xE923";
    constexpr const wchar_t* Close        = L"\xE8BB";

    // General UI
    constexpr const wchar_t* More         = L"\xE712"; // ...
    constexpr const wchar_t* Add          = L"\xE710"; // +
    constexpr const wchar_t* ChevronDown  = L"\xE70D";
    constexpr const wchar_t* Play         = L"\xE768";
    constexpr const wchar_t* Refresh      = L"\xE72C";
    constexpr const wchar_t* Volume       = L"\xE767";

    // Safe icon font family resolution for any Windows PC (Win 10 & Win 11)
    inline const wchar_t* GetFontFamily(IDWriteFactory* dwf) {
        if (!dwf) return L"Segoe MDL2 Assets";
        IDWriteFontCollection* collection = nullptr;
        if (SUCCEEDED(dwf->GetSystemFontCollection(&collection, FALSE)) && collection) {
            UINT32 index = 0;
            BOOL exists = FALSE;
            if (SUCCEEDED(collection->FindFamilyName(L"Segoe Fluent Icons", &index, &exists)) && exists) {
                collection->Release();
                return L"Segoe Fluent Icons";
            }
            if (SUCCEEDED(collection->FindFamilyName(L"Segoe MDL2 Assets", &index, &exists)) && exists) {
                collection->Release();
                return L"Segoe MDL2 Assets";
            }
            collection->Release();
        }
        return L"Segoe MDL2 Assets";
    }
}

// ════════════════════════════════════════════════════════════════════════════
// ANIMATION TIMING (milliseconds)
// ════════════════════════════════════════════════════════════════════════════
namespace Anim {
    constexpr int PopupEnterMs   = 300;
    constexpr int PopupExitMs    = 250;
    constexpr int ThemeMs        = 200;
    constexpr int ToggleMs       = 160;
    constexpr int HoverMs        = 80;
    constexpr int NavMs          = 120;
    constexpr int PageFadeMs     = 200;
    constexpr int BtnHoverMs     = 120;
    constexpr int NavIndicatorMs = 150;
    constexpr int FrameIntervalMs = 16; // ~60fps during animations only
    constexpr float PopupSlideDistPx = 40.f;
}

// ════════════════════════════════════════════════════════════════════════════
// EASING
// ════════════════════════════════════════════════════════════════════════════
inline float EaseOut(float t) {
    return 1.f - (1.f - t) * (1.f - t);
}
inline float EaseIn(float t) {
    return t * t;
}
inline float EaseInOut(float t) {
    return t < 0.5f ? 2.f * t * t : 1.f - 2.f * (1.f - t) * (1.f - t);
}
inline float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * (t - 1.f) * (t - 1.f) * (t - 1.f) + c1 * (t - 1.f) * (t - 1.f);
}
inline float EaseOutCubic(float t) {
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}

// ════════════════════════════════════════════════════════════════════════════
// GRAIN TEXTURE  —  precomputed tiny noise, baked into popup surface
// ════════════════════════════════════════════════════════════════════════════
namespace Grain {
    constexpr float Opacity = 0.025f; // almost invisible
    constexpr int   Size    = 128;    // tile size
}

} // namespace dhikr::ds
