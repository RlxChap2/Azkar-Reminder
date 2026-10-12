#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <string>

namespace dhikr {

class ResourceLoader {
public:
    // Load a PNG / bitmap image directly from an embedded executable resource into an ID2D1Bitmap
    // If targetWidth and targetHeight are specified (>0), uses WIC Fant/HighQualityCubic downsampler
    static ID2D1Bitmap* LoadBitmapFromResource(
        HINSTANCE hInstance,
        int resourceId,
        IWICImagingFactory* pWicFactory,
        ID2D1RenderTarget* pRenderTarget,
        const wchar_t* resourceType = RT_RCDATA,
        UINT targetWidth = 0,
        UINT targetHeight = 0
    );

    // Load raw UTF-8 string data from an embedded executable resource
    static std::string LoadStringFromResource(
        HINSTANCE hInstance,
        int resourceId,
        const wchar_t* resourceType = RT_RCDATA
    );
};

} // namespace dhikr
