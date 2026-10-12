#pragma once
#include "../IFullScreenDetector.h"

namespace dhikr {

class LinuxFullScreenDetector : public IFullScreenDetector {
public:
    LinuxFullScreenDetector();
    ~LinuxFullScreenDetector() override;

    FullScreenState CheckFullScreen() override;

private:
    bool m_isWayland = false;
};

} // namespace dhikr
