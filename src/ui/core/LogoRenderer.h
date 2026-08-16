#pragma once
#include <d2d1.h>
#include <windows.h>
#include <memory>

namespace dhikr {

class LogoRenderer {
public:
    // Draw dynamic procedural vector logo scaled and colored to the current accent color
    static void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dest, float alpha = 1.0f);

    // Helper to calculate an aspect-ratio-preserved centered destination rectangle inside a bounding box
    static inline D2D1_RECT_F FitRect(const D2D1_RECT_F& bounds, float imgW, float imgH) {
        float boxW = bounds.right - bounds.left;
        float boxH = bounds.bottom - bounds.top;
        if (boxW <= 0.0f || boxH <= 0.0f || imgW <= 0.0f || imgH <= 0.0f) return bounds;
        float scale = (boxW / imgW < boxH / imgH) ? (boxW / imgW) : (boxH / imgH);
        float drawW = imgW * scale;
        float drawH = imgH * scale;
        float left  = bounds.left + (boxW - drawW) * 0.5f;
        float top   = bounds.top  + (boxH - drawH) * 0.5f;
        return D2D1::RectF(left, top, left + drawW, top + drawH);
    }

    // Create or update a dynamic GDI HICON using the current accent color for tray / window icon
    static HICON CreateAccentIcon(int sizePx);
};

} // namespace dhikr
