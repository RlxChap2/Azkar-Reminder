#pragma once
#include "../IFullScreenDetector.h"

namespace dhikr {

class WindowsFullScreenDetector : public IFullScreenDetector {
public:
    WindowsFullScreenDetector() = default;
    ~WindowsFullScreenDetector() override = default;

    FullScreenState CheckFullScreen() override;
    bool IsFullScreenActive() override;
};

} // namespace dhikr
