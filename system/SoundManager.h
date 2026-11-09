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
    // 今流れているBGMだけを一時的に小さくする(1で元の音量、0で無音)。設定には保存しない。
    // 画面の暗転に合わせて曲を消すのに使う。StopBgmで元に戻る。
    void SetBgmFade(float amount);

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
