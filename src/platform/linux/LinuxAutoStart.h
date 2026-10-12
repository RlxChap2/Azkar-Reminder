#pragma once
#include "../IAutoStart.h"

namespace dhikr {

class LinuxAutoStart : public IAutoStart {
public:
    LinuxAutoStart() = default;
    ~LinuxAutoStart() override = default;

    bool IsAutoStartEnabled() const override;
    bool SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath = {}) override;

private:
    static std::filesystem::path GetAutostartDesktopFilePath();
};

} // namespace dhikr
