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
// --- 体力 ---
// 以前はプレイヤーと敵が同じ100を共有しており、強攻撃(40)3発で敵が倒れていた。
// それでは予兆を読んで立ち回る時間がほとんど無く、怯みも1戦で1回程度しか起きない。
// エルデンリングのボスやモンハンの大型モンスターのように、読み合いを何度も繰り返す
// 長さの戦闘にするため、敵の体力をプレイヤーと切り離して大きくした。
//
// 目安: 敵の攻撃1周期(予兆→攻撃→隙→様子見)は約4秒。隙へ3段コンボ(75)を
// 毎回入れられるわけではないので、平均の与ダメージを秒間12程度と見積もると、
// 予兆を読めるプレイヤーで約2分半〜3分の戦闘になる。
inline constexpr float PLAYER_MAX_HP = 100.0f;
inline constexpr float ENEMY_MAX_HP = 2000.0f;

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

// --- 当たり判定の左右の広さ(正面からの片側角度・ラジアン) ---
// 敵の攻撃判定は元々「距離だけ」で、どの攻撃も全方位に当たっていた。
// 見た目上は薙ぎ払いだけが横へ振り抜くのに、噛みつきでも真横で当たるのでは
// 「横へ回り込んで避ける」という判断が成立しない。
// 攻撃ごとに広さを変えることで、予兆から読み取れる形と実際の危険範囲を一致させる。
//   噛みつき : 狭い。正面を外せば当たらない。
//   叩き付け : 標準。正面寄りだけ。
//   薙ぎ払い : 広い。横へ回り込んでも当たるので、回避か距離で対処する。
inline constexpr float ENEMY_HIT_HALF_ANGLE = 0.96f;       // 約55度
inline constexpr float ENEMY_BITE_HIT_HALF_ANGLE = 0.61f;  // 約35度
inline constexpr float ENEMY_SWEEP_HIT_HALF_ANGLE = 1.92f; // 約110度

// --- 敵の怯み ---
// 読みが当たって隙へ攻撃を入れたとき、敵の体が反応しないと「反撃できた」手応えが無い。
// 攻撃ごとの怯み値(AttackData::postureDamage)を敵に溜め、しきい値を超えたら怯ませる。
// 1発ごとに必ず怯むと、敵が何もできずに一方的な戦いになるため、溜める方式にしている。
inline constexpr int PLAYER_WEAK_POSTURE_DAMAGE = 14;
inline constexpr int PLAYER_HEAVY_POSTURE_DAMAGE = 34;
// 敵の隙(Recovery)へ入れた攻撃は怯み値を増やす。「読んで踏み込んだ」ことへのご褒美。
inline constexpr float PUNISH_POSTURE_MULTIPLIER = 2.0f;
// 怯むたびにしきい値を上げる(モンスターハンターの怯み耐性と同じ考え方)。
// 体力を2000へ増やしたとき、固定のしきい値60のままだと1戦で40回以上怯み、
// 敵が何もできないまま殴られ続ける。上げていくことで、序盤は怯みやすく
// 終盤ほど「狙って溜めないと怯まない」ようにし、1戦で8回前後に収める。
//   150 → 188 → 234 → 293 → 366 → 375(上限)...
inline constexpr float ENEMY_FLINCH_BASE_THRESHOLD = 150.0f;
inline constexpr float ENEMY_FLINCH_THRESHOLD_GROWTH = 1.25f;
inline constexpr float ENEMY_FLINCH_THRESHOLD_MAX = 375.0f;
// 攻撃を当てない時間が続くと怯み値は抜けていく。散発的な攻撃でいつの間にか怯むのを防ぐ。
inline constexpr float ENEMY_POSTURE_RECOVERY_PER_SECOND = 6.0f;
// 怯んでいる時間。短すぎると気付けず、長すぎると反撃が一方的になる。
inline constexpr float ENEMY_FLINCH_SECONDS = 0.90f;

// --- 敵の弱り具合(体力ゲージの代わり) ---
// 敵の体力は画面に数値やゲージで出さない。モンスターハンターのように、
// 「足を引きずる」「息が荒い」「隙が長くなる」といった体の変化で弱ってきたことを伝える。
//   通常 : 体力50%より上
//   疲れ : 50%以下。息が荒く頭が下がり、動きが少し鈍る
//   瀕死 : 20%以下。足を引きずり、動きが遅く、攻撃後の隙が長い
// 段階が変わった瞬間はよろめかせる(怯みと同じ動き)。変化に気付かせるため。
inline constexpr float ENEMY_TIRED_HP_RATIO = 0.50f;
inline constexpr float ENEMY_DYING_HP_RATIO = 0.20f;
inline constexpr float ENEMY_TIRED_MOVE_SCALE = 0.85f;
inline constexpr float ENEMY_DYING_MOVE_SCALE = 0.55f;
// 隙(Recovery)を伸ばす倍率。敵AIの側だけを伸ばす。戦闘判定側の攻撃は先に終わって
// 待機へ戻るので、次の攻撃の受け付けとは食い違わない。
inline constexpr float ENEMY_TIRED_RECOVERY_SCALE = 1.20f;
inline constexpr float ENEMY_DYING_RECOVERY_SCALE = 1.50f;

// --- プレイヤーの吹き飛ばし ---
// 食らった攻撃の重さを、体が飛ばされる距離で伝える。
// 同じダメージ表現(画面の揺れ・赤み)だけだと、どの攻撃を食らったか体で分からない。
//   噛みつき : 軽く押し戻されるだけ。すぐ動ける。
//   叩き付け : 大きく吹き飛ぶ。最も重い。
//   薙ぎ払い : 横薙ぎで大きく飛ばされる。
// 初速(単位/秒)と時間(秒)。速度は時間とともに二乗で落ちるので、
// 飛ばされる距離は 初速 x 時間 / 3 になる(叩き付けで約77、薙ぎ払いで約51、噛みつきで約11)。
// 初版(叩き付け260=約48)は実機で「押し戻された」程度にしか見えなかったため強めた。
inline constexpr float ENEMY_KNOCKBACK_SPEED = 420.0f;
inline constexpr float ENEMY_KNOCKBACK_SECONDS = 0.55f;
inline constexpr float ENEMY_BITE_KNOCKBACK_SPEED = 150.0f;
inline constexpr float ENEMY_BITE_KNOCKBACK_SECONDS = 0.22f;
inline constexpr float ENEMY_SWEEP_KNOCKBACK_SPEED = 340.0f;
inline constexpr float ENEMY_SWEEP_KNOCKBACK_SECONDS = 0.45f;
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
        attack.postureDamage = Tuning::PLAYER_WEAK_POSTURE_DAMAGE;
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
        attack.postureDamage = Tuning::PLAYER_HEAVY_POSTURE_DAMAGE;
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

/**
 * @brief 当たり判定の左右の広さ(正面からの片側角度)を種類から引く。
 *
 * 敵の正面方向とプレイヤーへの方向の角度差がこの値以内なら当たる。
 * 距離だけの判定にすると、横へ回り込んでも当たってしまい、
 * 「予兆の形を見て回り込む/回避する」という選択が意味を持たなくなる。
 */
inline float EnemyHitHalfAngleOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_HIT_HALF_ANGLE;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_HIT_HALF_ANGLE;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_HIT_HALF_ANGLE;
    }
}

/** プレイヤーが吹き飛ばされる強さ。 */
struct KnockbackData
{
    float speed = 0.0f;   ///< 初速(単位/秒)
    float seconds = 0.0f; ///< 飛ばされている時間(秒)
};

/** 敵の攻撃を食らったときの吹き飛ばしを種類から引く。 */
inline KnockbackData EnemyKnockbackOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:
        return { Tuning::ENEMY_BITE_KNOCKBACK_SPEED, Tuning::ENEMY_BITE_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Sweep:
        return { Tuning::ENEMY_SWEEP_KNOCKBACK_SPEED, Tuning::ENEMY_SWEEP_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Slam:
    default:
        return { Tuning::ENEMY_KNOCKBACK_SPEED, Tuning::ENEMY_KNOCKBACK_SECONDS };
    }
}

/**
 * @brief デバッグ表示用の日本語名。
 *
 * HUDは英字で統一しているため、`EnemyAttackDisplayName()`とは別に持つ。
 * デバッグ表示は開発者が読むものなので日本語にする。
 */
inline const char* EnemyAttackDebugName(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return "噛みつき";
    case EnemyAttackKind::Sweep: return "薙ぎ払い";
    case EnemyAttackKind::Slam:
    default:                     return "叩き付け";
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
