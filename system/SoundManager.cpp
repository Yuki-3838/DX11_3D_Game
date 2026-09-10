#include "SoundManager.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr int SAMPLE_RATE = 44100;
    constexpr int CHANNELS = 2;
    constexpr float PI_F = 3.14159265358979323846f;
    constexpr float BGM_MASTER_VOLUME = 0.42f;
    constexpr float SFX_MASTER_VOLUME = 0.45f;

    struct Voice
    {
        HWAVEOUT device = nullptr;
        WAVEHDR header{};
        std::vector<int16_t> samples;
        bool active = false;
    };

    enum class BgmTrack
    {
        None,
        Title,
        Game,
    };

    float Tone(float frequency, float time)
    {
        return std::sinf(2.0f * PI_F * frequency * time);
    }

    int16_t ToPcm(float value)
    {
        const float limited = std::tanh(value * 1.25f);
        return static_cast<int16_t>(std::clamp(limited, -1.0f, 1.0f) * 30000.0f);
    }

    std::vector<int16_t> GenerateAudio(
        float durationSeconds,
        const std::function<std::pair<float, float>(float)>& generator)
    {
        const size_t frameCount = static_cast<size_t>(durationSeconds * SAMPLE_RATE);
        std::vector<int16_t> samples(frameCount * CHANNELS);
        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            const float time = static_cast<float>(frame) / SAMPLE_RATE;
            const auto [left, right] = generator(time);
            samples[frame * CHANNELS] = ToPcm(left);
            samples[frame * CHANNELS + 1] = ToPcm(right);
        }
        return samples;
    }

    std::vector<int16_t> MakeTitleBgm()
    {
        constexpr float duration = 28.0f;
        constexpr float beat = 60.0f / 72.0f;
        constexpr std::array<float, 4> roots = { 110.0f, 98.0f, 82.41f, 123.47f };

        return GenerateAudio(duration, [=](float time) {
            const int chordIndex = static_cast<int>(time / (beat * 4.0f)) % 4;
            const float root = roots[chordIndex];
            const float chordTime = std::fmod(time, beat * 4.0f);
            const int step = static_cast<int>(chordTime / beat);
            const float stepTime = std::fmod(chordTime, beat);
            const float arpeggioNote = root * (step % 2 == 0 ? 2.0f : 3.0f);
            const float noteEnvelope = std::exp(-stepTime * 5.0f);
            const float slowEnvelope = 0.72f + 0.28f * std::sinf(time * 0.48f);

            const float pad = Tone(root, time) * 0.050f +
                Tone(root * 1.5f, time) * 0.022f +
                Tone(root * 2.0f, time) * 0.016f;
            const float arpeggio = Tone(arpeggioNote, time) * 0.035f * noteEnvelope;
            const float bass = Tone(root * 0.5f, time) * 0.035f;
            const float signal = (pad + arpeggio + bass) * slowEnvelope;
            return std::make_pair(signal, signal * 0.96f);
        });
    }

    std::vector<int16_t> MakeGameBgm()
    {
        // 112 BPMの短調ループとして、低音のパルス、裏拍、短い反復音を
        // 組み合わせ、静かなパッド音ではなく戦闘曲として聞こえるようにする。
        constexpr float duration = 36.0f;
        constexpr float beat = 60.0f / 112.0f;
        constexpr std::array<float, 4> roots = { 73.42f, 65.41f, 55.0f, 61.74f };

        return GenerateAudio(duration, [=](float time) {
            const int chordIndex = static_cast<int>(time / (beat * 4.0f)) % 4;
            const float root = roots[chordIndex];
            const float barTime = std::fmod(time, beat * 4.0f);
            const int beatIndex = static_cast<int>(barTime / beat);
            const float beatTime = std::fmod(barTime, beat);
            const float halfBeat = beat * 0.5f;
            const float halfBeatTime = std::fmod(time, halfBeat);
            const int stepIndex = static_cast<int>(time / halfBeat) % 8;

            const float pad = Tone(root, time) * 0.045f +
                Tone(root * 1.1892f, time) * 0.026f +
                Tone(root * 1.4983f, time) * 0.030f;
            const float bass = Tone(root * 0.5f, time) * 0.105f;
            const float fifth = Tone(root * 1.4983f, time) * 0.045f;
            const float tension = Tone(root * 2.3784f, time) * 0.022f;

            const float kickEnvelope = std::exp(-beatTime * 22.0f);
            const float kick = Tone(52.0f - 17.0f * std::min(beatTime * 7.0f, 1.0f), time) *
                0.17f * kickEnvelope;
            const bool backbeat = beatIndex == 1 || beatIndex == 3;
            const float snareEnvelope = backbeat ? std::exp(-beatTime * 30.0f) : 0.0f;
            const float snareNoise = std::sinf(time * 913.0f) * std::sinf(time * 1777.0f);
            const float snare = snareNoise * 0.13f * snareEnvelope;
            const float hatNoise = std::sinf(time * 6211.0f) * std::sinf(time * 8713.0f);
            const float hat = hatNoise * 0.030f * std::exp(-halfBeatTime * 42.0f);

            const float noteEnvelope = std::exp(-halfBeatTime * 15.0f);
            constexpr std::array<int, 8> pattern = { 0, 2, 4, 2, 0, 3, 4, 2 };
            const float note = root * (pattern[stepIndex] == 0 ? 2.0f :
                pattern[stepIndex] == 2 ? 2.3784f :
                pattern[stepIndex] == 3 ? 2.9966f : 3.0f);
            const float ostinato = Tone(note, time) * 0.060f * noteEnvelope;
            const float leadGate = (beatIndex == 0 || beatIndex == 2) ? 1.0f : 0.0f;
            const float lead = Tone(root * 4.0f, time) * 0.028f *
                std::exp(-beatTime * 12.0f) * leadGate;

            const float signal = pad + bass + fifth + tension + kick + snare + hat +
                ostinato + lead;
            return std::make_pair(signal * 0.98f, signal * 0.90f + ostinato * 0.18f);
        });
    }

    std::vector<int16_t> LoadWaveFile(const char* path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return {};

        char riff[4]{};
        char wave[4]{};
        uint32_t riffSize = 0;
        input.read(riff, sizeof(riff));
        input.read(reinterpret_cast<char*>(&riffSize), sizeof(riffSize));
        input.read(wave, sizeof(wave));
        if (!input || riffSize < 4 || std::memcmp(riff, "RIFF", sizeof(riff)) != 0 ||
            std::memcmp(wave, "WAVE", sizeof(wave)) != 0)
            return {};

        uint16_t audioFormat = 0;
        uint16_t channels = 0;
        uint32_t sampleRate = 0;
        uint16_t bitsPerSample = 0;
        std::vector<char> data;

        while (input)
        {
            char chunkId[4]{};
            uint32_t chunkSize = 0;
            input.read(chunkId, sizeof(chunkId));
            input.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
            if (!input)
                break;

            if (std::memcmp(chunkId, "fmt ", sizeof(chunkId)) == 0)
            {
                uint32_t byteRate = 0;
                uint16_t blockAlign = 0;
                input.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
                input.read(reinterpret_cast<char*>(&channels), sizeof(channels));
                input.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
                input.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
                input.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
                input.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));
                static_cast<void>(byteRate);
                static_cast<void>(blockAlign);
                if (chunkSize > 16)
                    input.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
            }
            else if (std::memcmp(chunkId, "data", sizeof(chunkId)) == 0)
            {
                data.resize(chunkSize);
                input.read(data.data(), static_cast<std::streamsize>(chunkSize));
            }
            else
            {
                input.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
            }

            if ((chunkSize & 1u) != 0)
                input.seekg(1, std::ios::cur);
        }

        if (audioFormat != WAVE_FORMAT_PCM || channels == 0 || bitsPerSample != 16 ||
            sampleRate != SAMPLE_RATE || data.empty())
            return {};

        const size_t bytesPerFrame = static_cast<size_t>(channels) * sizeof(int16_t);
        const size_t frameCount = data.size() / bytesPerFrame;
        const auto* source = reinterpret_cast<const int16_t*>(data.data());
        std::vector<int16_t> samples(frameCount * CHANNELS);
        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            const int16_t left = source[frame * channels];
            const int16_t right = channels > 1 ? source[frame * channels + 1] : left;
            samples[frame * CHANNELS] = left;
            samples[frame * CHANNELS + 1] = right;
        }
        return samples;
    }

    class RuntimeAudio
    {
    public:
        void Init()
        {
            m_initialized = true;
            LoadVolumeSettings();
            m_gameBgmSamples = LoadWaveFile("assets/audio/game_bgm.wav");
            m_swordSwingSamples = LoadWaveFile("assets/audio/sword_swing.wav");
            m_enemyRoarSamples = LoadWaveFile("assets/audio/dragon_roar.wav");
            m_dodgeSamples = LoadWaveFile("assets/audio/dodge.wav");
            m_dragonAttackSamples = LoadWaveFile("assets/audio/dragon_attack.wav");
        }

        void Update()
        {
            if (!m_initialized)
                return;

            LoopBgmIfFinished();
            for (Voice& voice : m_sfxVoices)
            {
                if (voice.active && (voice.header.dwFlags & WHDR_DONE) != 0)
                    CloseVoice(voice);
            }
        }

        void Shutdown()
        {
            StopBgm();
            for (Voice& voice : m_sfxVoices)
                CloseVoice(voice);
            m_swordSwingSamples.clear();
            m_enemyRoarSamples.clear();
            m_dodgeSamples.clear();
            m_dragonAttackSamples.clear();
            m_gameBgmSamples.clear();
            SaveVolumeSettings();
            m_initialized = false;
        }

        void PlayBgm(BgmTrack track)
        {
            if (!m_initialized && !InitializeOnDemand())
                return;
            if (m_bgm.active && m_bgmTrack == track)
                return;

            StopBgm();
            std::vector<int16_t> samples;
            if (track == BgmTrack::Game && !m_gameBgmSamples.empty())
                samples = m_gameBgmSamples;
            else if (track == BgmTrack::Title)
                samples = MakeTitleBgm();
            else
                samples = MakeGameBgm();
            if (OpenVoice(m_bgm, std::move(samples), m_bgmVolume))
                m_bgmTrack = track;
        }

        void StopBgm()
        {
            CloseVoice(m_bgm);
            m_bgmTrack = BgmTrack::None;
        }

        void PlaySwordSwing()
        {
            PlaySfx(m_swordSwingSamples);
        }

        void PlayEnemyRoar()
        {
            PlaySfx(m_enemyRoarSamples);
        }

        void PlayDodge()
        {
            PlaySfx(m_dodgeSamples);
        }

        void PlayDragonAttack()
        {
            PlaySfx(m_dragonAttackSamples);
        }

        float GetBgmVolume() const { return m_bgmVolume; }
        float GetSfxVolume() const { return m_sfxVolume; }

        void SetBgmVolume(float volume)
        {
            m_bgmVolume = std::clamp(volume, 0.0f, 1.0f);
            SaveVolumeSettings();
            if (m_initialized && m_bgm.active && m_bgmTrack != BgmTrack::None)
            {
                const BgmTrack track = m_bgmTrack;
                StopBgm();
                PlayBgm(track);
            }
        }

        void SetSfxVolume(float volume)
        {
            m_sfxVolume = std::clamp(volume, 0.0f, 1.0f);
            SaveVolumeSettings();
        }

        void ResetVolumes()
        {
            m_bgmVolume = BGM_MASTER_VOLUME;
            m_sfxVolume = SFX_MASTER_VOLUME;
            SaveVolumeSettings();
            if (m_initialized && m_bgm.active && m_bgmTrack != BgmTrack::None)
            {
                const BgmTrack track = m_bgmTrack;
                StopBgm();
                PlayBgm(track);
            }
        }

        void PlaySfx(const std::vector<int16_t>& samples)
        {
            if (!m_initialized && !InitializeOnDemand())
                return;
            if (samples.empty())
                return;

            Voice* selected = nullptr;
            for (Voice& voice : m_sfxVoices)
            {
                if (!voice.active)
                {
                    selected = &voice;
                    break;
                }
            }
            if (selected == nullptr)
            {
                selected = &m_sfxVoices.front();
                CloseVoice(*selected);
            }
            OpenVoice(*selected, std::vector<int16_t>(samples), m_sfxVolume);
        }

    private:
        void LoadVolumeSettings()
        {
            std::ifstream input("audio_settings.ini");
            std::string line;
            while (std::getline(input, line))
            {
                try
                {
                    if (line.rfind("bgm=", 0) == 0)
                        m_bgmVolume = std::stof(line.substr(4));
                    else if (line.rfind("sfx=", 0) == 0)
                        m_sfxVolume = std::stof(line.substr(4));
                }
                catch (...)
                {
                    // 設定ファイルの値が不正な場合は安全な初期値を維持する。
                }
            }
            m_bgmVolume = std::clamp(m_bgmVolume, 0.0f, 1.0f);
            m_sfxVolume = std::clamp(m_sfxVolume, 0.0f, 1.0f);
        }

        void SaveVolumeSettings() const
        {
            std::ofstream output("audio_settings.ini", std::ios::trunc);
            if (!output)
                return;
            output << std::fixed << std::setprecision(3);
            output << "bgm=" << m_bgmVolume << '\n';
            output << "sfx=" << m_sfxVolume << '\n';
        }

        bool InitializeOnDemand()
        {
            m_initialized = true;
            return true;
        }

        WAVEFORMATEX GetFormat() const
        {
            WAVEFORMATEX format{};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = CHANNELS;
            format.nSamplesPerSec = SAMPLE_RATE;
            format.wBitsPerSample = 16;
            format.nBlockAlign = static_cast<WORD>(CHANNELS * sizeof(int16_t));
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            return format;
        }

        bool OpenVoice(Voice& voice, std::vector<int16_t>&& samples, float volume)
        {
            CloseVoice(voice);
            voice.samples = std::move(samples);
            if (voice.samples.empty())
                return false;

            for (int16_t& sample : voice.samples)
            {
                const int scaled = static_cast<int>(std::lround(
                    static_cast<float>(sample) * volume));
                sample = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
            }

            const WAVEFORMATEX format = GetFormat();
            if (waveOutOpen(&voice.device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            {
                voice.samples.clear();
                return false;
            }

            voice.header = {};
            voice.header.lpData = reinterpret_cast<LPSTR>(voice.samples.data());
            voice.header.dwBufferLength = static_cast<DWORD>(voice.samples.size() * sizeof(int16_t));
            if (waveOutPrepareHeader(voice.device, &voice.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
                waveOutWrite(voice.device, &voice.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            {
                CloseVoice(voice);
                return false;
            }

            voice.active = true;
            return true;
        }

        void LoopBgmIfFinished()
        {
            if (!m_bgm.active || (m_bgm.header.dwFlags & WHDR_DONE) == 0)
                return;

            if (waveOutWrite(m_bgm.device, &m_bgm.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            {
                CloseVoice(m_bgm);
                m_bgmTrack = BgmTrack::None;
            }
        }

        void CloseVoice(Voice& voice)
        {
            if (voice.device != nullptr)
            {
                waveOutReset(voice.device);
                if ((voice.header.dwFlags & WHDR_PREPARED) != 0)
                    waveOutUnprepareHeader(voice.device, &voice.header, sizeof(WAVEHDR));
                waveOutClose(voice.device);
            }
            voice = Voice{};
        }

        bool m_initialized = false;
        BgmTrack m_bgmTrack = BgmTrack::None;
        Voice m_bgm;
        std::array<Voice, 4> m_sfxVoices{};
        std::vector<int16_t> m_gameBgmSamples;
        std::vector<int16_t> m_swordSwingSamples;
        std::vector<int16_t> m_enemyRoarSamples;
        std::vector<int16_t> m_dodgeSamples;
        std::vector<int16_t> m_dragonAttackSamples;
        float m_bgmVolume = BGM_MASTER_VOLUME;
        float m_sfxVolume = SFX_MASTER_VOLUME;
    };

    RuntimeAudio g_audio;
}

namespace SoundManager
{
    void Init() { g_audio.Init(); }
    void Update() { g_audio.Update(); }
    void Shutdown() { g_audio.Shutdown(); }
    void PlayTitleBgm() { g_audio.PlayBgm(BgmTrack::Title); }
    void PlayGameBgm() { g_audio.PlayBgm(BgmTrack::Game); }
    void StopBgm() { g_audio.StopBgm(); }
    void PlaySwordSwing() { g_audio.PlaySwordSwing(); }
    void PlayEnemyRoar() { g_audio.PlayEnemyRoar(); }
    void PlayDodge() { g_audio.PlayDodge(); }
    void PlayDragonAttack() { g_audio.PlayDragonAttack(); }
    float GetBgmVolume() { return g_audio.GetBgmVolume(); }
    float GetSfxVolume() { return g_audio.GetSfxVolume(); }
    void SetBgmVolume(float volume) { g_audio.SetBgmVolume(volume); }
    void SetSfxVolume(float volume) { g_audio.SetSfxVolume(volume); }
    void ResetVolumes() { g_audio.ResetVolumes(); }
}
