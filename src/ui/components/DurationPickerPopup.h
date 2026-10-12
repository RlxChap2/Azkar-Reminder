#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include <string>

namespace dhikr {

class DurationPickerPopup {
public:
    static DurationPickerPopup& Instance();

    using OnDurationSelected = std::function<void(int minutes)>;

    // Shows the custom duration picker near the specified screen coordinates
    void Show(HWND parent, int x, int y, int initialMinutes, OnDurationSelected callback);
    void Close();
    bool IsOpen() const { return m_hwnd != nullptr; }

private:
    DurationPickerPopup();
    ~DurationPickerPopup();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Render();
    void DrawArrowBtn(ID2D1RenderTarget* rt, float cx, float cy, bool up, bool hover);
    void DrawPillBtn(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, const wchar_t* text, bool hover, bool active = false);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    ID2D1HwndRenderTarget* m_rt = nullptr;
    IDWriteFactory* m_dwf = nullptr;
    IDWriteTextFormat* m_tfTitle = nullptr;
    IDWriteTextFormat* m_tfBig = nullptr;
    IDWriteTextFormat* m_tfUnit = nullptr;
    IDWriteTextFormat* m_tfPill = nullptr;
    IDWriteTextFormat* m_tfBtn = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;

    OnDurationSelected m_callback;

    int m_minutes = 30;
    bool m_firstKeyInput = true;

    // UI hover and press state
    // 1: up arrow, 2: down arrow
    // 3: -5 pill, 4: -1 pill, 5: +1 pill, 6: +5 pill
    // 10: Cancel, 11: Apply
    // 20..27: Quick presets (1m, 5m, 10m, 15m, 20m, 30m, 45m, 60m)
    int m_hoveredElement = -1;
    int m_pressedElement = -1;

    float m_dpiScale = 1.0f;
};

} // namespace dhikr
