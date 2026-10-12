#pragma once
#include <memory>

namespace dhikr {

enum class FullScreenState {
    NotFullScreen, // Confirmed no fullscreen app active
    FullScreen,    // Fullscreen app (game, presentation, full video) is active
    Unknown,       // Could not determine at this moment
    Unsupported    // Not supported by the window server (e.g., restricted Wayland compositors)
};

class IFullScreenDetector {
public:
    virtual ~IFullScreenDetector() = default;

    // Returns the explicit full screen state according to platform capabilities
    virtual FullScreenState CheckFullScreen() = 0;

    // Convenience method: returns true ONLY if CheckFullScreen() == FullScreenState::FullScreen
    virtual bool IsFullScreenActive() {
        return CheckFullScreen() == FullScreenState::FullScreen;
    }

    // Factory to instantiate platform-specific detector
    static std::unique_ptr<IFullScreenDetector> Create();
};

} // namespace dhikr
