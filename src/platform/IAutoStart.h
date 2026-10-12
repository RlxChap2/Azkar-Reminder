#pragma once
#include <filesystem>
#include <memory>
#include <string>

namespace dhikr {

class IAutoStart {
public:
    virtual ~IAutoStart() = default;

    // Returns whether application autostart is enabled in the OS
    virtual bool IsAutoStartEnabled() const = 0;

    // Enables or disables autostart. If appPath is empty, uses current executable path.
    virtual bool SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath = {}) = 0;

    // Factory to instantiate platform-specific implementation
    static std::unique_ptr<IAutoStart> Create();
};

} // namespace dhikr
