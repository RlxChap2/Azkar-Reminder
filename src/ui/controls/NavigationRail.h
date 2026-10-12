#pragma once
#include "../core/UIControl.h"
#include "../core/UIState.h"
#include "../DesignSystem.h"
#include <vector>
#include <string>

namespace dhikr {

class NavigationRail : public UIControl {
public:
    NavigationRail() {
        m_labels = {
            L"Home",
            L"Reminders",
            L"Dhikr Library",
            L"Settings",
            L"About"
        };
        m_icons = {
            ds::Icon::Home,
            ds::Icon::Reminders,
            ds::Icon::Library,
            ds::Icon::Settings,
            ds::Icon::About
        };
    }

    void SetDpiScale(float scale) { m_dpiScale = scale; }

    void Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fontApp) override {
        // Assume fontApp is for regular labels, we also need icon font and version font.
        // For simplicity we will handle those via specific Setters if needed, or MainWindow will draw the base and we just handle interaction.
        // Wait, UIControl allows drawing. Let's make it self-contained if possible, or MainWindow can provide the fonts.
        // For now, this is a conceptual placeholder as we might just handle the interaction bounds here and let MainWindow render it.
    }

    // Instead of full Draw override here, let's provide layout metrics so MainWindow can draw it, but NavigationRail handles input.
    D2D1_RECT_F GetItemRect(int index) const {
        float sw  = m_bounds.right - m_bounds.left;
        float ih  = 44.f * m_dpiScale;
        float top = m_bounds.top + 24.f * m_dpiScale + index * ih;
        return D2D1::RectF(m_bounds.left, top, m_bounds.left + sw, top + ih);
    }

    int HitTestItem(float x, float y) const {
        for (size_t i = 0; i < m_labels.size(); ++i) {
            D2D1_RECT_F r = GetItemRect((int)i);
            if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
                return (int)i;
            }
        }
        return -1;
    }

    bool OnMouseMove(float x, float y) override {
        int hover = HitTestItem(x, y);
        if (hover != m_hoveredItem) {
            m_hoveredItem = hover;
            return true;
        }
        return false;
    }

    bool OnMouseLeave() override {
        if (m_hoveredItem != -1 || m_pressedItem != -1) {
            m_hoveredItem = -1;
            m_pressedItem = -1;
            return true;
        }
        return false;
    }

    bool OnLButtonDown(float x, float y) override {
        int hit = HitTestItem(x, y);
        if (hit != -1) {
            m_pressedItem = hit;
            return true;
        }
        return false;
    }

    bool OnLButtonUp(float x, float y) override {
        int pressed = m_pressedItem;
        m_pressedItem = -1;
        if (pressed != -1 && pressed == HitTestItem(x, y)) {
            if (OnNavigate) OnNavigate((PageId)pressed);
            return true;
        }
        return pressed != -1;
    }

    int GetHoveredItem() const { return m_hoveredItem; }
    
    bool IsHovered() const override { return m_hoveredItem != -1; }

    std::function<void(PageId)> OnNavigate;

private:
    float m_dpiScale = 1.0f;
    std::vector<std::wstring> m_labels;
    std::vector<const wchar_t*> m_icons;
    int m_hoveredItem = -1;
    int m_pressedItem = -1;
};

}
