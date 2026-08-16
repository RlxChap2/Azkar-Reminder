#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>

namespace dhikr {

class TimePickerPopup {
public:
    static TimePickerPopup& Instance();

    using OnTimeSelected = std::function<void(int hour, int minute)>;

    // Shows the popup near the specified screen coordinates
    void Show(HWND parent, int x, int y, int initialHour, int initialMinute, OnTimeSelected callback);

    void Close();

private:
    TimePickerPopup();
    ~TimePickerPopup();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Render();
    void DrawArrowBtn(ID2D1RenderTarget* rt, float cx, float cy, bool up, bool hover);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    ID2D1HwndRenderTarget* m_rt = nullptr;
    IDWriteFactory* m_dwf = nullptr;
    IDWriteTextFormat* m_tfBig = nullptr;
    IDWriteTextFormat* m_tfBtn = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;

    OnTimeSelected m_callback;

    int m_hour = 0;
    int m_minute = 0;
    
    // UI state
    // 1: h up, 2: h down, 3: m up, 4: m down, 5: am/pm, 10: cancel, 11: apply
    int m_hoveredElement = -1;
    int m_pressedElement = -1;

    float m_dpiScale = 1.0f;
};

} // namespace dhikr
