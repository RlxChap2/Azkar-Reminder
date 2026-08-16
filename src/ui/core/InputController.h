#pragma once
#include <vector>
#include <memory>
#include "UIControl.h"

namespace dhikr {

class InputController {
public:
    static InputController& Instance() {
        static InputController instance;
        return instance;
    }

    void AddControl(std::shared_ptr<UIControl> control) {
        m_controls.push_back(control);
    }

    void ClearControls() {
        m_controls.clear();
    }
    
    const std::vector<std::shared_ptr<UIControl>>& GetControls() const {
        return m_controls;
    }

    bool HasHoveredControl() const {
        for (const auto& ctrl : m_controls) {
            if (ctrl->IsHovered()) return true;
        }
        return false;
    }

    // Input routing (returns true if a visual state changed, requiring a repaint)
    bool OnMouseMove(float x, float y) {
        bool needsRepaint = false;
        for (auto& ctrl : m_controls) {
            if (ctrl->OnMouseMove(x, y)) needsRepaint = true;
        }
        return needsRepaint;
    }

    bool OnMouseLeave() {
        bool needsRepaint = false;
        for (auto& ctrl : m_controls) {
            if (ctrl->OnMouseLeave()) needsRepaint = true;
        }
        return needsRepaint;
    }

    bool OnLButtonDown(float x, float y) {
        bool needsRepaint = false;
        for (auto& ctrl : m_controls) {
            if (ctrl->OnLButtonDown(x, y)) needsRepaint = true;
        }
        return needsRepaint;
    }

    bool OnLButtonUp(float x, float y) {
        bool needsRepaint = false;
        for (auto& ctrl : m_controls) {
            if (ctrl->OnLButtonUp(x, y)) needsRepaint = true;
        }
        return needsRepaint;
    }

private:
    InputController() = default;
    std::vector<std::shared_ptr<UIControl>> m_controls;
};

}
