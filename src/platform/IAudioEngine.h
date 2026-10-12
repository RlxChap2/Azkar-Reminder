#pragma once
#include <filesystem>
#include <memory>
#include <string>

namespace dhikr {

class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    // Initialize audio device backend
    virtual bool Initialize() = 0;

    // Shutdown and release audio resources
    virtual void Shutdown() = 0;

    // Play notification chime (volumeOverride 0.0f - 1.0f, or -1.0f to use settings)
    virtual void PlayReminder(float volumeOverride = -1.0f) = 0;

    // Stop currently playing audio immediately
    virtual void Stop() = 0;

    // Reload sound file from custom path or default asset
    virtual void ReloadSound() = 0;

    // Returns true if audio device initialized and ready
    virtual bool IsAvailable() const = 0;

    // Factory method
    static std::unique_ptr<IAudioEngine> Create();
};

} // namespace dhikr
