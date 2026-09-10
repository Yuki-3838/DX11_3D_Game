#pragma once

#include "../DX11_3D_Game/CombatDesign.h"

/**
 * @file CombatAttackTable.h
 * @brief このゲームで実際に使う攻撃データを1箇所へまとめたテーブル。
 *
 * 攻撃の時間(予兆・判定・硬直)と威力は、以前は次の2箇所へ別々に書かれていた。
 *  - gameobject/enemy.h  : 敵AIの状態遷移(=プレイヤーが見るアニメーションの長さ)
 *  - system/OneVsOneCombat.h : 実際のダメージ判定タイミング
 * 値が食い違うと「予備動作を見てから回避する」という本作の核が壊れるため、
 * ここを唯一の定義元とし、両方から参照する。
 *
 * Tuning名前空間の定数は、constexprが必要な箇所(クラス内のstatic constexpr)から使う。
 * AttackDataを返す関数は、攻撃を種類ごとに扱いたい箇所(今後の攻撃パターン追加)から使う。
 */
namespace Combat
{
namespace Tuning
{
// --- プレイヤー通常攻撃 ---
inline constexpr float PLAYER_WEAK_ANTICIPATION = 0.16f;
inline constexpr float PLAYER_WEAK_ACTIVE = 0.38f;
inline constexpr float PLAYER_WEAK_RECOVERY = 0.41f;
inline constexpr int PLAYER_WEAK_DAMAGE = 25;

// --- プレイヤー強攻撃 ---
inline constexpr float PLAYER_HEAVY_ANTICIPATION = 0.20f;
inline constexpr float PLAYER_HEAVY_ACTIVE = 0.42f;
inline constexpr float PLAYER_HEAVY_RECOVERY = 0.48f;
inline constexpr int PLAYER_HEAVY_DAMAGE = 40;

// --- 敵の攻撃(叩き付け): 標準。予兆が長く、隙も大きい ---
// 予兆(ANTICIPATION)は敵AIのWindup状態の長さと必ず一致させること。
// 以前はAI・アニメーション側が0.80秒、戦闘判定側が0.72秒とズレており、
// 予備動作アニメーションが終わる前に攻撃判定が始まっていた。
// 「予兆を見てから回避するか踏み込むかを決める」という本作の核を守るため、
// 長い方(0.80秒)へ統一し、判定がアニメーションより先に出ないようにする。
inline constexpr float ENEMY_ANTICIPATION = 0.80f;
inline constexpr float ENEMY_ACTIVE = 0.90f;
inline constexpr float ENEMY_RECOVERY = 1.35f;
inline constexpr float ENEMY_COOLDOWN = 1.15f;
inline constexpr int ENEMY_DAMAGE = 15;
// 攻撃判定が届く距離。敵AIが攻撃を決断する距離(enemy.hのATTACK_DISTANCE)とは別物。
inline constexpr float ENEMY_HIT_RANGE = 64.0f;
// Active開始から実際にダメージが発生するまでの時間(叩き付けが当たる瞬間)。
inline constexpr float ENEMY_FIRST_HIT_TIME = 0.18f;

// --- 敵の攻撃(噛みつき): 速い。予兆が短く、隙も小さい ---
// 「様子見しすぎると刺される」役割。射程は短いので、距離を取っていれば安全。
// 隙が小さいので、これに反撃を欲張ると次の攻撃を食らう。
inline constexpr float ENEMY_BITE_ANTICIPATION = 0.42f;
inline constexpr float ENEMY_BITE_ACTIVE = 0.55f;
inline constexpr float ENEMY_BITE_RECOVERY = 0.70f;
inline constexpr float ENEMY_BITE_COOLDOWN = 0.95f;
inline constexpr int ENEMY_BITE_DAMAGE = 10;
inline constexpr float ENEMY_BITE_HIT_RANGE = 48.0f;
inline constexpr float ENEMY_BITE_FIRST_HIT_TIME = 0.10f;

// --- 敵の攻撃(薙ぎ払い): 遅い。予兆が非常に長く、隙が最大 ---
// 「見えたら必ず回避、そして大きく反撃できる」役割。
// 射程が広いので、回避せず距離を取るだけでは避けきれない。
inline constexpr float ENEMY_SWEEP_ANTICIPATION = 1.10f;
inline constexpr float ENEMY_SWEEP_ACTIVE = 0.85f;
inline constexpr float ENEMY_SWEEP_RECOVERY = 1.80f;
inline constexpr float ENEMY_SWEEP_COOLDOWN = 1.30f;
inline constexpr int ENEMY_SWEEP_DAMAGE = 22;
inline constexpr float ENEMY_SWEEP_HIT_RANGE = 78.0f;
inline constexpr float ENEMY_SWEEP_FIRST_HIT_TIME = 0.22f;
} // namespace Tuning

/** プレイヤーの通常攻撃1段分。 */
inline const AttackData& PlayerWeakAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "player_weak";
        attack.animationName = "sword_shield_slash";
        attack.frames.anticipationSeconds = Tuning::PLAYER_WEAK_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::PLAYER_WEAK_ACTIVE;
        attack.frames.recoverySeconds = Tuning::PLAYER_WEAK_RECOVERY;
        attack.damage = Tuning::PLAYER_WEAK_DAMAGE;
        return attack;
    }();
    return data;
}

/** プレイヤーの強攻撃1段分。 */
inline const AttackData& PlayerHeavyAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "player_heavy";
        attack.animationName = "sword_shield_attack";
        attack.frames.anticipationSeconds = Tuning::PLAYER_HEAVY_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::PLAYER_HEAVY_ACTIVE;
        attack.frames.recoverySeconds = Tuning::PLAYER_HEAVY_RECOVERY;
        attack.damage = Tuning::PLAYER_HEAVY_DAMAGE;
        return attack;
    }();
    return data;
}

/**
 * @brief 敵の攻撃の種類。
 *
 * 攻撃が1種類しかないと「待てば必ず同じことが起きる」ため読み合いにならない。
 * 予兆の長さ・射程・隙の大きさを変えた3種類を用意し、
 * プレイヤーが予兆を見て「回避する/踏み込む/距離を取る」を選べるようにする。
 */
enum class EnemyAttackKind
{
    Slam,  ///< 標準。予兆が長く、隙も大きい。
    Bite,  ///< 速い。予兆が短く隙も小さいが、射程が短い。
    Sweep, ///< 遅い。予兆が非常に長いが、射程が広く隙が最大。
};

/** 敵の攻撃(叩き付け)。 */
inline const AttackData& EnemyBasicAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_slam";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_HIT_RANGE;
        attack.damage = Tuning::ENEMY_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 敵の攻撃(噛みつき)。速いが射程が短く、隙も小さい。 */
inline const AttackData& EnemyBiteAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_bite";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_BITE_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_BITE_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_BITE_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_BITE_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_BITE_HIT_RANGE;
        attack.damage = Tuning::ENEMY_BITE_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 敵の攻撃(薙ぎ払い)。予兆が長く射程が広い。当てられると痛いが隙も最大。 */
inline const AttackData& EnemySweepAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_sweep";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_SWEEP_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_SWEEP_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_SWEEP_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_SWEEP_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_SWEEP_HIT_RANGE;
        attack.damage = Tuning::ENEMY_SWEEP_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 種類から攻撃データを引く。 */
inline const AttackData& EnemyAttackOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return EnemyBiteAttack();
    case EnemyAttackKind::Sweep: return EnemySweepAttack();
    case EnemyAttackKind::Slam:
    default:                     return EnemyBasicAttack();
    }
}

/** Active開始からダメージが発生するまでの時間を種類から引く。 */
inline float EnemyFirstHitTimeOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_FIRST_HIT_TIME;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_FIRST_HIT_TIME;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_FIRST_HIT_TIME;
    }
}

/** HUDへ予兆を表示するときの短い名前。プレイヤーが「何が来るか」を読む手がかり。 */
inline const char* EnemyAttackDisplayName(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return "QUICK BITE";
    case EnemyAttackKind::Sweep: return "WIDE SWEEP";
    case EnemyAttackKind::Slam:
    default:                     return "HEAVY SLAM";
    }
}
} // namespace Combat
