#pragma once
#include "../IFullScreenDetector.h"

namespace dhikr {

class MacFullScreenDetector : public IFullScreenDetector {
public:
    MacFullScreenDetector() = default;
    ~MacFullScreenDetector() override = default;

    FullScreenState CheckFullScreen() override;
};

} // namespace dhikr
