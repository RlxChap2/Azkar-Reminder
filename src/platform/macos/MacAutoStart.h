#pragma once
#include "../IAutoStart.h"

namespace dhikr {

class MacAutoStart : public IAutoStart {
public:
    MacAutoStart() = default;
    ~MacAutoStart() override = default;

    bool IsAutoStartEnabled() const override;
    bool SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath = {}) override;

private:
    static std::filesystem::path GetLaunchAgentPlistPath();
};

} // namespace dhikr
