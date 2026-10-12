#define MINIAUDIO_IMPLEMENTATION
#include "../../../third_party/miniaudio.h"

#include "MiniAudioEngine.h"
#include "../../core/Logger.h"
#include "../../core/Settings.h"
#include "../../core/PlatformPaths.h"

namespace dhikr {

#ifndef _WIN32
std::unique_ptr<IAudioEngine> IAudioEngine::Create() {
    return std::make_unique<MiniAudioEngine>();
}
#endif

MiniAudioEngine& MiniAudioEngine::Instance() {
    static MiniAudioEngine instance;
    return instance;
}

MiniAudioEngine::MiniAudioEngine() = default;

MiniAudioEngine::~MiniAudioEngine() {
    Shutdown();
}

bool MiniAudioEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    m_engine = new ma_engine();
    ma_engine_config config = ma_engine_config_init();

    ma_result result = ma_engine_init(&config, m_engine);
    if (result != MA_SUCCESS) {
        LOG_WARN("Failed to initialize miniaudio engine: " + std::to_string(result) + " (running in silent fallback mode)");
        delete m_engine;
        m_engine = nullptr;
        m_initialized = false;
        return false;
    }

    m_initialized = true;
    LOG_INFO("MiniAudio engine initialized successfully");
    return true;
}

void MiniAudioEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sound) {
        ma_sound_uninit(m_sound);
        delete m_sound;
        m_sound = nullptr;
    }
    if (m_engine) {
        ma_engine_uninit(m_engine);
        delete m_engine;
        m_engine = nullptr;
    }
    m_initialized = false;
}

std::filesystem::path MiniAudioEngine::ResolveSoundFilePath() const {
    const auto& s = Settings::Instance().Get();

    // 1. Custom sound path if configured and exists
    if (!s.customSoundPath.empty()) {
        std::filesystem::path p(s.customSoundPath);
        if (std::filesystem::exists(p)) return p;
    }

    // 2. Data directory audio asset
    auto dataDirAudio = PlatformPaths::GetDataDir() / "audio" / "diode_matrix.mp3";
    if (std::filesystem::exists(dataDirAudio)) return dataDirAudio;

    // 3. Local relative paths
    if (std::filesystem::exists("audio/diode_matrix.mp3")) return "audio/diode_matrix.mp3";
    if (std::filesystem::exists("assets/audio/diode_matrix.mp3")) return "assets/audio/diode_matrix.mp3";
    if (std::filesystem::exists("../assets/audio/diode_matrix.mp3")) return "../assets/audio/diode_matrix.mp3";

    return {};
}

void MiniAudioEngine::ReloadSound() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_engine) return;

    if (m_sound) {
        ma_sound_uninit(m_sound);
        delete m_sound;
        m_sound = nullptr;
    }

    auto path = ResolveSoundFilePath();
    if (path.empty()) {
        LOG_WARN("No reminder audio file found to reload");
        return;
    }

    m_sound = new ma_sound();
    ma_result res = ma_sound_init_from_file(m_engine, path.string().c_str(), 0, nullptr, nullptr, m_sound);
    if (res != MA_SUCCESS) {
        LOG_WARN("Failed loading sound file: " + path.string() + ", error=" + std::to_string(res));
        delete m_sound;
        m_sound = nullptr;
        return;
    }

    m_loadedPath = path;
    LOG_INFO("Loaded reminder audio: " + path.string());
}

void MiniAudioEngine::PlayReminder(float volumeOverride) {
    if (!Settings::Instance().Get().soundEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized && !Initialize()) return;

    float vol = volumeOverride >= 0.0f ? volumeOverride : (Settings::Instance().Get().soundVolume / 100.0f);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    auto currentPath = ResolveSoundFilePath();
    if (currentPath.empty()) {
        LOG_WARN("Cannot play reminder chime: no audio asset found");
        return;
    }

    // Direct fire-and-forget playback
    ma_engine_set_volume(m_engine, vol);
    ma_result res = ma_engine_play_sound(m_engine, currentPath.string().c_str(), nullptr);
    if (res != MA_SUCCESS) {
        LOG_WARN("Playback error: " + std::to_string(res));
    }
}

void MiniAudioEngine::Stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_engine) return;
    if (m_sound) {
        ma_sound_stop(m_sound);
        ma_sound_seek_to_pcm_frame(m_sound, 0);
    }
}

bool MiniAudioEngine::IsAvailable() const {
    return m_initialized.load();
}

} // namespace dhikr
