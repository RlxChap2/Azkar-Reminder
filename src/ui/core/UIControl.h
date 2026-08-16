#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include <string>

namespace dhikr {

class UIControl {
public:
    virtual ~UIControl() = default;

    virtual void Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* font) = 0;
    
    // Bounds management
    void SetBounds(const D2D1_RECT_F& r) { m_bounds = r; }
    const D2D1_RECT_F& GetBounds() const { return m_bounds; }
    
    virtual bool IsHovered() const { return m_hovered; }

    // Hit Testing
    virtual bool HitTest(float x, float y) const {
        return (x >= m_bounds.left && x <= m_bounds.right &&
                y >= m_bounds.top && y <= m_bounds.bottom);
    }

    // Input handlers
    virtual bool OnMouseMove(float x, float y) {
        if (!m_enabled) return false;
        bool wasHovered = m_hovered;
        m_hovered = HitTest(x, y);
        return m_hovered != wasHovered; // Return true if visual state changed
    }
    
    virtual bool OnMouseLeave() {
        if (!m_hovered && !m_pressed) return false;
        m_hovered = false;
        m_pressed = false;
        return true;
    }
    
    virtual bool OnLButtonDown(float x, float y) {
        if (!m_enabled) return false;
        if (HitTest(x, y)) {
            m_pressed = true;
            return true;
        }
        return false;
    }
    
    virtual bool OnLButtonUp(float x, float y) {
        if (!m_enabled) return false;
        bool wasPressed = m_pressed;
        m_pressed = false;
        
        if (wasPressed && HitTest(x, y)) {
            if (OnClick) OnClick();
            return true; // visual state changed and triggered click
        }
        return wasPressed; // returned true if we were pressed (so need redraw)
    }

    // Properties
    void SetEnabled(bool e) { m_enabled = e; }
    bool IsEnabled() const { return m_enabled; }

    std::function<void()> OnClick;

protected:
    D2D1_RECT_F m_bounds = {0,0,0,0};
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_enabled = true;
};

}
