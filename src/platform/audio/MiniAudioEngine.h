#pragma once
#include "../IAudioEngine.h"
#include <mutex>
#include <atomic>
#include <string>
#include <filesystem>

// Forward declaration of miniaudio types
struct ma_engine;
struct ma_sound;

namespace dhikr {

class MiniAudioEngine : public IAudioEngine {
public:
    static MiniAudioEngine& Instance();

    MiniAudioEngine();
    ~MiniAudioEngine() override;

    bool Initialize() override;
    void Shutdown() override;
    void PlayReminder(float volumeOverride = -1.0f) override;
    void Stop() override;
    void ReloadSound() override;
    bool IsAvailable() const override;

private:
    std::filesystem::path ResolveSoundFilePath() const;

    mutable std::mutex   m_mutex;
    ma_engine*           m_engine = nullptr;
    ma_sound*            m_sound  = nullptr;
    std::atomic<bool>    m_initialized{false};
    std::filesystem::path m_loadedPath;
};

} // namespace dhikr
