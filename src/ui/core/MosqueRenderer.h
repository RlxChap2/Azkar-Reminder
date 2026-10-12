#pragma once
#include <windows.h>
#include <d2d1.h>
#include <ctime>

namespace dhikr {

enum class TimePeriod {
    Morning,  // 05:00 - 11:30 (أذكار الصباح)
    Day,      // 11:30 - 17:30 (أذكار الظهيرة والعصر)
    Sunset,   // 17:30 - 19:30 (أذكار المغرب والغروب)
    Night     // 19:30 - 05:00 (أذكار المساء والليل)
};

class MosqueRenderer {
public:
    static TimePeriod GetCurrentPeriod();
    static const wchar_t* GetPeriodBadge(TimePeriod period);
    static void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds, float dpiScale);
};

} // namespace dhikr
