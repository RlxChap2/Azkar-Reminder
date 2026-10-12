#pragma once
#include "../IAutoStart.h"

namespace dhikr {

class WindowsAutoStart : public IAutoStart {
public:
    WindowsAutoStart() = default;
    ~WindowsAutoStart() override = default;

    bool IsAutoStartEnabled() const override;
    bool SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath = {}) override;
};

} // namespace dhikr
