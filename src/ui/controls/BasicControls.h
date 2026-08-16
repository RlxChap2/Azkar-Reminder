#pragma once
#include "../core/UIControl.h"
#include "../DesignSystem.h"

namespace dhikr {

class ButtonControl : public UIControl {
public:
    ButtonControl(const std::wstring& label) : m_label(label) {}

    void Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* font) override {
        if (!m_enabled) {
            brush->SetColor(ds::Card());
        } else if (m_pressed) {
            brush->SetColor(ds::Card()); // slightly different for pressed could be ds::Background()
        } else if (m_hovered) {
            brush->SetColor(ds::CardHover());
        } else {
            brush->SetColor(ds::Sidebar());
        }

        rt->FillRoundedRectangle(D2D1::RoundedRect(m_bounds, 4.f, 4.f), brush);
        
        brush->SetColor(ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(m_bounds, 4.f, 4.f), brush, 1.0f);

        if (font) {
            brush->SetColor(m_enabled ? ds::TextPrimary() : ds::TextSecondary());
            DWRITE_PARAGRAPH_ALIGNMENT pa = font->GetParagraphAlignment();
            DWRITE_TEXT_ALIGNMENT ta = font->GetTextAlignment();
            font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            
            rt->DrawTextW(m_label.c_str(), (UINT32)m_label.length(), font, m_bounds, brush);
            
            font->SetParagraphAlignment(pa);
            font->SetTextAlignment(ta);
        }
    }

private:
    std::wstring m_label;
};

class ToggleControl : public UIControl {
public:
    ToggleControl(bool initial = false) : m_isOn(initial) {}

    void SetIsOn(bool on) { m_isOn = on; }
    bool IsOn() const { return m_isOn; }

    void Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat*) override {
        float w = m_bounds.right - m_bounds.left;
        float h = m_bounds.bottom - m_bounds.top;
        if (w <= 0 || h <= 0) return;
        float r = h / 2.0f;

        // Track
        if (!m_enabled) {
            brush->SetColor(ds::Border());
            rt->FillRoundedRectangle(D2D1::RoundedRect(m_bounds, r, r), brush);
        } else if (m_isOn) {
            brush->SetColor(m_hovered ? ds::Accent::Primary(0.9f) : ds::Accent::Primary());
            rt->FillRoundedRectangle(D2D1::RoundedRect(m_bounds, r, r), brush);
        } else {
            brush->SetColor(m_hovered ? ds::CardHover() : ds::Surface());
            rt->FillRoundedRectangle(D2D1::RoundedRect(m_bounds, r, r), brush);
            brush->SetColor(ds::Border());
            rt->DrawRoundedRectangle(D2D1::RoundedRect(m_bounds, r, r), brush, 0.75f);
        }

        // Knob
        float pad = 2.5f * (h / 20.0f);
        float knobDiameter = h - pad * 2.f;
        float knobR = knobDiameter / 2.f;
        float knobY = m_bounds.top + pad + knobR;
        float knobX = m_isOn ? (m_bounds.right - pad - knobR) : (m_bounds.left + pad + knobR);

        brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, knobY), knobR, knobR), brush);
    }

private:
    bool m_isOn = false;
};

}
