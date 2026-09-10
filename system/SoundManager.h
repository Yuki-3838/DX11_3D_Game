#pragma once

// 軽量なゲーム内音声管理。
// BGMは実行時生成、戦闘SEは assets/audio のPCM WAVを再生する。
namespace SoundManager
{
    void Init();
    void Update();
    void Shutdown();

    void PlayTitleBgm();
    void PlayGameBgm();
    void StopBgm();

    void PlaySwordSwing();
    void PlayEnemyRoar();
    void PlayDodge();
    void PlayDragonAttack();

    float GetBgmVolume();
    float GetSfxVolume();
    void SetBgmVolume(float volume);
    void SetSfxVolume(float volume);
    void ResetVolumes();
}
