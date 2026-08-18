#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <memory>
#include "../core/ContentEngine.h"

namespace dhikr {

// ── Popup animation state ─────────────────────────────────────────────────────
enum class PopupAnimState { Hidden, Entering, Visible, Hovering, Closing };

// ── PopupWindow ───────────────────────────────────────────────────────────────
// Layered, topmost, non-activating popup notification.
// Matches the supplied design reference pixel-accurately.
class PopupWindow {
public:
    static PopupWindow& Instance();

    bool Create(HINSTANCE hInstance);

    // Show with given content item
    void Show(const ContentItem* item);

    // Hide immediately
    void Hide();

    // Check if currently visible
    bool IsVisible() const { return m_state != PopupAnimState::Hidden; }

    // Window proc (static -> member dispatch)
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    PopupWindow(const PopupWindow&) = delete;
    PopupWindow& operator=(const PopupWindow&) = delete;

private:
    PopupWindow() = default;
    ~PopupWindow();

    // Window management
    void RegisterClass(HINSTANCE hInstance);
    bool CreateLayeredWindow(HINSTANCE hInstance);

    // Position calculation
    RECT CalculatePosition(int width, int height);
    HMONITOR GetTargetMonitor();

    // Rendering
    bool InitD2D();
    void ReleaseD2D();
    void Render(float animProgress);  // 0.0–1.0 for enter, 1.0–0.0 for exit
    void DrawBackground(ID2D1RenderTarget* rt, float alpha, float slideOffset);
    void DrawAccentBar(ID2D1RenderTarget* rt, float alpha);
    void DrawCrescentMark(ID2D1RenderTarget* rt, float alpha);
    void DrawContent(ID2D1RenderTarget* rt, float alpha);
    void DrawCloseButton(ID2D1RenderTarget* rt, float alpha);
    void DrawPauseButton(ID2D1RenderTarget* rt, float alpha);
    void DrawGrainTexture(ID2D1RenderTarget* rt, float alpha);
    void UpdateLayered(float alpha, float slideOffset);

    // Message handlers
    LRESULT OnTimer(UINT_PTR timerId);
    LRESULT OnMouseMove(int x, int y);
    LRESULT OnMouseLeave();
    LRESULT OnLButtonDown(int x, int y);
    LRESULT OnSetCursor(HWND hwnd, UINT hitTest, UINT message);
    LRESULT OnDpiChanged(UINT newDpi, const RECT* newRect);

    // Helpers
    bool HitTestClose(int x, int y) const;
    bool HitTestPause(int x, int y) const;
    void StartEnterAnim();
    void StartExitAnim();
    void SetupAutoDismiss();
    void CancelAutoDismiss();
    float ComputeContentHeight(const ContentItem& item);

    // ── State ──────────────────────────────────────────────────────────────
    HWND              m_hwnd        = nullptr;
    HINSTANCE         m_hInst       = nullptr;
    PopupAnimState    m_state       = PopupAnimState::Hidden;

    // Animation progress 0.0–1.0
    float             m_animT       = 0.f;
    DWORD             m_animStart   = 0;
    float             m_animDuration = 300.f;

    // Hover state
    bool              m_hovering    = false;
    bool              m_mouseTracking = false;
    float             m_hoverT      = 0.f;

    // Content
    const ContentItem* m_item       = nullptr;

    // Dimensions (DPI-scaled)
    int               m_width       = 420;
    int               m_height      = 120;
    int               m_screenX     = 0;
    int               m_screenY     = 0; // final resting position
    float             m_dpiScale    = 1.f;

    // Button rects (logical px)
    RECT              m_closeRect   {};
    RECT              m_pauseRect   {};

    // D2D resources
    ID2D1Factory*          m_d2dFactory        = nullptr;
    ID2D1DCRenderTarget*   m_dcRenderTarget    = nullptr;
    IDWriteFactory*        m_dwriteFactory     = nullptr;
    IDWriteTextFormat*     m_textFmtMain       = nullptr;
    IDWriteTextFormat*     m_textFmtMainLarge  = nullptr;
    IDWriteTextFormat*     m_textFmtMainMedium = nullptr;
    IDWriteTextFormat*     m_textFmtMainCompact= nullptr;
    IDWriteTextFormat*     m_textFmtSub        = nullptr;
    IDWriteTextFormat*     m_textFmtSmall      = nullptr;
    IWICImagingFactory*    m_wicFactory        = nullptr;
    ID2D1Bitmap*           m_logoBitmap        = nullptr;

    // Grain bitmap (pre-generated)
    void              GenerateGrainBitmap();
    ID2D1Bitmap*      m_grainBitmap = nullptr;

    static constexpr wchar_t kClassName[] = L"DhikrPopup";
};

} // namespace dhikr
