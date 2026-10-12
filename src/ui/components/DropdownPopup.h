#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <functional>

namespace dhikr {

struct DropdownItem {
    int id = 0;
    std::wstring text;
    bool isSelected = false;
};

class DropdownPopup {
public:
    static DropdownPopup& Instance();

    using OnItemSelected = std::function<void(int id)>;

    // Shows the custom dark dropdown at screen coordinates
    void Show(HWND parent, int screenX, int screenY, float minWidth,
              const std::vector<DropdownItem>& items, OnItemSelected callback);

    void Close();
    bool IsOpen() const { return m_hwnd != nullptr; }

private:
    DropdownPopup();
    ~DropdownPopup();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Render();

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    ID2D1HwndRenderTarget* m_rt = nullptr;
    IDWriteFactory* m_dwf = nullptr;
    IDWriteTextFormat* m_tfItem = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;

    std::vector<DropdownItem> m_items;
    OnItemSelected m_callback;

    int m_hoveredIndex = -1;
    float m_width = 180.f;
    float m_height = 100.f;
    float m_itemH = 34.f;
    float m_dpiScale = 1.0f;
    float m_scrollY = 0.f;
    float m_maxScroll = 0.f;
    bool m_isRtl = false;
};

} // namespace dhikr
