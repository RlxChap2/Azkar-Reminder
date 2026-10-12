#pragma once
#include <windows.h>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>

namespace dhikr {

class AudioEngine {
public:
    static AudioEngine& Instance();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Play the notification reminder sound (optionally overriding volume 0.0 - 1.0)
    void PlayReminder(float volumeOverride = -1.0f);

    // Stop any playing sound
    void Stop();

    // Reload sound from custom path or default resource
    void ReloadSound();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

private:
    AudioEngine() = default;
    ~AudioEngine() { Shutdown(); }

    void GenerateDefaultWav();

    HINSTANCE m_hInstance  = nullptr;
    bool      m_initialized = false;
    
    IXAudio2*               m_pXAudio2 = nullptr;
    IXAudio2MasteringVoice* m_pMasterVoice = nullptr;
    IXAudio2SourceVoice*    m_pSourceVoice = nullptr;
    
    std::vector<BYTE> m_audioData;
    WAVEFORMATEX      m_wfx = {0};
};

} // namespace dhikr
