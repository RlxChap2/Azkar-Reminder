#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

namespace dhikr {

class WindowChrome {
public:
    static WindowChrome& Instance() {
        static WindowChrome instance;
        return instance;
    }

    // Must be called when the DPI or window size changes
    void UpdateMetrics(float dpiScale, int windowWidth, int windowHeight);

    // Hit testing for WM_NCHITTEST
    LRESULT HitTest(HWND hwnd, int x, int y);

    // Render the title bar and caption buttons
    void Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fontApp, IDWriteTextFormat* fontIcon, ID2D1Bitmap* logo);

    // Input handlers specifically for caption buttons
    bool OnMouseMove(float x, float y);
    bool OnMouseLeave();
    bool OnLButtonDown(float x, float y);
    bool OnLButtonUp(HWND hwnd, float x, float y);

private:
    WindowChrome() = default;

    float m_dpiScale = 1.0f;
    int m_width = 0;
    int m_height = 0;

    int m_hoveredBtn = 0; // 0=none, 1=min, 2=max, 3=close
    int m_pressedBtn = 0;

    D2D1_RECT_F GetCaptionBtnRect(int index) const;
};

}
