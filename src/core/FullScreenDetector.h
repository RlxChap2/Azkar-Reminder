#pragma once
#include <windows.h>
#include <shellapi.h>

namespace dhikr {

class FullScreenDetector {
public:
    static FullScreenDetector& Instance();

    // Returns true if a true fullscreen / gaming / presentation / video application is active on the screen
    bool IsFullScreenActive() const;

private:
    FullScreenDetector() = default;
    ~FullScreenDetector() = default;
};

} // namespace dhikr
