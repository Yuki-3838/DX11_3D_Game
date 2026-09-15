#pragma once

#include <algorithm>
#include <cmath>
#include "CombatAttackTable.h"

/**
 * @file EnemyAttackPose.h
 * @brief 敵の攻撃の「予兆」を、HUDの文字ではなく体の動きで見せるための姿勢オフセット。
 *
 * このゲームの核は「敵のしぐさを見て、踏み込むか回避するかを決める」ことにある。
 * ところが敵の攻撃モーションは`dragon_attack.dae`の1本しか無く、
 * 攻撃が3種類あっても再生速度が違うだけで、見た目はほとんど同じだった。
 * その結果、プレイヤーが「何が来るか」を知る手段が画面隅のHUDの文字しか無く、
 * 「モンスターを見る」ではなく「文字を読む」ゲームになっていた。
 *
 * ここでは新しいモーションを足さずに、攻撃の種類ごとに違うシルエットを作る。
 * クリップの再生に対して、体全体の向き(ヨー)と前後の傾き(ピッチ)を
 * 手続き的に足すことで、遠目にも区別できる構えにする。
 *
 *  - 叩き付け(Slam) : 上体を後ろへ反らして溜め、前へ叩き落とす。
 *  - 噛みつき(Bite) : 頭を下げて低く沈み、そのまま突っ込む。
 *  - 薙ぎ払い(Sweep): 体を大きく横へひねって溜め、反対側へ振り抜く。
 *
 * 適用先は描画用SRT(`enemy::getRenderSRT()`)だけである。
 * 物理SRT・壁との衝突・敵AIの判断には一切影響しない。
 * 「見た目の演出」と「ゲーム内の位置」を混ぜると、
 * 壁抜けや接地ずれといった別の不具合を呼び込むためである。
 *
 * 一方で、見た目だけを変えて当たり判定が伴わないと予兆が嘘になる。
 * 薙ぎ払いが横へ振り抜いて見えるのに正面しか当たらない、という状態を避けるため、
 * 当たり判定の左右の広さは`Combat::EnemyHitHalfAngleOf()`で攻撃ごとに変えている。
 */
namespace Combat
{

/** 攻撃のどの段階にいるか。`enemy::MotionState`から変換して渡す。 */
enum class EnemyAttackPhase
{
    None,     ///< 攻撃していない(移動・待機)
    Windup,   ///< 予兆。ここでプレイヤーが「何が来るか」を読む。
    Active,   ///< 攻撃判定が出ている。
    Recovery, ///< 隙。ここがプレイヤーの反撃機会。
};

/**
 * @brief 描画用SRTへ加算する姿勢のずらし量(ラジアン)。
 *
 * 高さは持たない。当初は「沈み込む/伸び上がる」を高さの加算で表していたが、
 * 敵の接地は毎更新でスキニング後の最下点から求め直しているため、
 * 高さを直接足すと接地補正と食い違って**敵が地面へ埋まった**。
 * 傾ければ最下点も動くので、接地補正側が傾きを織り込めば
 * 沈み込みも伸び上がりも自然に付いてくる
 * (`CAnimationMesh::GetAnimatedLowestLocalHeight()`)。
 */
struct EnemyPoseOffset
{
    float pitch = 0.0f; ///< 前後の傾き。正で前のめり、負で後ろへ反る。
    float yaw = 0.0f;   ///< 体の水平のひねり。高さには影響しない。
};

namespace PoseTuning
{
// 溜めの大きさ。実機で見て、遠目のシルエットで区別できる最小限に留めている。
// 大きくしすぎると、1本しか無いクリップとの食い違い(足が付いていかない)が目立つ。
inline constexpr float SLAM_REAR_PITCH = -0.34f;  ///< 叩き付け: 後ろへ反る量
inline constexpr float SLAM_STRIKE_PITCH = 0.26f; ///< 叩き付け: 振り下ろす量

// 噛みつきは叩き付けと構えが似ていると読み分けられない。実機で見比べたところ、
// 初期値(前傾0.20 / 沈み込み-3.4)では叩き付けとの差が小さかったため、
// 「低く沈んで鼻先を落とす」方向へはっきり振っている。
inline constexpr float BITE_CROUCH_PITCH = 0.34f; ///< 噛みつき: 頭を下げる量
inline constexpr float BITE_LUNGE_PITCH = 0.30f;  ///< 噛みつき: 突っ込むときの前傾

inline constexpr float SWEEP_WIND_YAW = 0.62f;   ///< 薙ぎ払い: 溜めでひねる角度
inline constexpr float SWEEP_SWING_YAW = -0.86f; ///< 薙ぎ払い: 振り抜く角度

/// 隙(Recovery)で姿勢が戻りきるまでの時間。長いほど「まだ動けない」ことが伝わる。
inline constexpr float RECOVERY_SETTLE_SECONDS = 0.45f;
} // namespace PoseTuning

namespace Detail
{
/// 0→1へ滑らかに立ち上がる。等速の線形補間だと溜めの終わりが唐突に見える。
inline float SmoothStep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// 0→1→0。振り抜いてから戻る動きに使う。
inline float Pulse01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return std::sin(t * 3.14159265f);
}

/// 手前で速く、後半でゆっくり。踏み込みのように出だしが鋭い動きに使う。
inline float FastOut(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}
} // namespace Detail

/**
 * @brief 攻撃の種類と段階から、描画姿勢のずらし量を求める。
 *
 * @param kind      いま敵が選んでいる攻撃。
 * @param phase     攻撃のどの段階か。
 * @param phaseTime その段階に入ってからの経過秒。
 */
inline EnemyPoseOffset EnemyAttackPose(
    EnemyAttackKind kind, EnemyAttackPhase phase, float phaseTime)
{
    EnemyPoseOffset offset{};
    if (phase == EnemyAttackPhase::None)
        return offset;

    const AttackData& attack = EnemyAttackOf(kind);
    const float windupSeconds = std::max(0.01f, attack.frames.anticipationSeconds);
    const float activeSeconds = std::max(0.01f, attack.frames.activeSeconds);

    // 予兆の進み具合(0→1)。1に近いほど「もう来る」。
    const float windupT = Detail::SmoothStep01(phaseTime / windupSeconds);
    // 攻撃判定中の進み具合(0→1)。
    const float activeT = std::clamp(phaseTime / activeSeconds, 0.0f, 1.0f);

    switch (kind)
    {
    case EnemyAttackKind::Slam:
        if (phase == EnemyAttackPhase::Windup)
        {
            // 後ろへ反りながら伸び上がる。予兆が長いので、じわじわ溜まって見える。
            offset.pitch = PoseTuning::SLAM_REAR_PITCH * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            // 溜めた姿勢から一気に振り下ろす。出だしを速くして打点を分かりやすくする。
            const float strike = Detail::FastOut(activeT / 0.35f);
            offset.pitch =
                PoseTuning::SLAM_REAR_PITCH * (1.0f - strike) +
                PoseTuning::SLAM_STRIKE_PITCH * strike;
        }
        else // Recovery
        {
            // 振り下ろした前のめりのまま、ゆっくり戻る。これが反撃の合図になる。
            const float settle =
                1.0f - Detail::SmoothStep01(phaseTime / PoseTuning::RECOVERY_SETTLE_SECONDS);
            offset.pitch = PoseTuning::SLAM_STRIKE_PITCH * settle;
        }
        break;

    case EnemyAttackKind::Bite:
        if (phase == EnemyAttackPhase::Windup)
        {
            // 予兆が短いので、溜めではなく「狙いを定めて沈む」動きにする。
            const float aim = Detail::FastOut(windupT);
            offset.pitch = PoseTuning::BITE_CROUCH_PITCH * aim;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            // 沈んだ姿勢から前へ突っ込む。前進そのものは敵AI側が行う。
            const float lunge = Detail::Pulse01(activeT);
            offset.pitch =
                PoseTuning::BITE_CROUCH_PITCH + PoseTuning::BITE_LUNGE_PITCH * lunge;
        }
        else // Recovery
        {
            // 隙が小さいので、素早く元へ戻る。欲張って反撃すると次が来る。
            const float settle =
                1.0f - Detail::SmoothStep01(phaseTime / (PoseTuning::RECOVERY_SETTLE_SECONDS * 0.6f));
            offset.pitch = PoseTuning::BITE_CROUCH_PITCH * settle;
        }
        break;

    case EnemyAttackKind::Sweep:
    default:
        if (phase == EnemyAttackPhase::Windup)
        {
            // 体を大きく横へひねる。3種類の中で最も分かりやすい溜めにしている。
            // 予兆が1.10秒と長いので、見てから回避が間に合う。
            offset.yaw = PoseTuning::SWEEP_WIND_YAW * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            // ひねった側から反対側へ振り抜く。横へ動いても当たる範囲が広い。
            const float swing = Detail::SmoothStep01(activeT);
            offset.yaw =
                PoseTuning::SWEEP_WIND_YAW * (1.0f - swing) +
                PoseTuning::SWEEP_SWING_YAW * swing;
        }
        else // Recovery
        {
            // 振り抜いた体勢のまま大きく泳ぐ。隙が最大なので反撃の本命。
            const float settle =
                1.0f - Detail::SmoothStep01(phaseTime / PoseTuning::RECOVERY_SETTLE_SECONDS);
            offset.yaw = PoseTuning::SWEEP_SWING_YAW * settle;
        }
        break;
    }

    return offset;
}

namespace PoseTuning
{
// 怯みの大きさ。攻撃の構えより大きく、しかし一瞬で戻り切らない程度にする。
inline constexpr float FLINCH_PITCH = -0.30f;  ///< のけぞる量(負で後ろへ反る)
inline constexpr float FLINCH_YAW = 0.38f;     ///< 打たれた側から顔を背ける量
inline constexpr float FLINCH_PEAK_SECONDS = 0.07f; ///< のけぞりが最大になるまで
} // namespace PoseTuning

/**
 * @brief 怯みの姿勢のずらし量。
 *
 * 打たれた瞬間に一気にのけぞり、残りの時間でゆっくり戻す。
 * 立ち上がりを遅くすると「攻撃が当たった結果」に見えず、別の動作に見えてしまう。
 *
 * @param flinchTime    怯み始めてからの経過秒。
 * @param flinchSeconds 怯みの長さ。
 * @param yawSign       顔を背ける向き(+1 / -1)。打たれた側と反対へ向ける。
 */
inline EnemyPoseOffset EnemyFlinchPose(float flinchTime, float flinchSeconds, float yawSign)
{
    EnemyPoseOffset offset{};
    const float peak = PoseTuning::FLINCH_PEAK_SECONDS;
    float envelope = 0.0f;
    if (flinchTime < peak)
    {
        envelope = Detail::FastOut(flinchTime / peak);
    }
    else
    {
        const float settleSeconds = std::max(0.01f, flinchSeconds - peak);
        envelope = 1.0f - Detail::SmoothStep01((flinchTime - peak) / settleSeconds);
    }
    offset.pitch = PoseTuning::FLINCH_PITCH * envelope;
    offset.yaw = PoseTuning::FLINCH_YAW * yawSign * envelope;
    return offset;
}

/** 敵の弱り具合。体力ゲージを出さない代わりに、体の動きで伝える。 */
enum class EnemyCondition
{
    Healthy, ///< 通常
    Tired,   ///< 疲れ(体力50%以下)
    Dying,   ///< 瀕死(体力20%以下)
};

namespace PoseTuning
{
inline constexpr float TIRED_HEAD_DROOP = 0.05f;    ///< 疲れ: 頭の下がり
inline constexpr float TIRED_BREATH_PITCH = 0.04f;  ///< 疲れ: 息で上下する量
inline constexpr float TIRED_BREATH_SPEED = 2.4f;   ///< 疲れ: 呼吸の速さ(ラジアン/秒)
inline constexpr float DYING_HEAD_DROOP = 0.12f;    ///< 瀕死: 頭の下がり
inline constexpr float DYING_BREATH_PITCH = 0.07f;  ///< 瀕死: 肩で息をする量
inline constexpr float DYING_BREATH_SPEED = 1.7f;   ///< 瀕死: 呼吸の速さ(遅く深い)
inline constexpr float DYING_LIMP_PITCH = 0.14f;    ///< 瀕死: 足を引きずって前へつんのめる量
inline constexpr float DYING_LIMP_YAW = 0.07f;      ///< 瀕死: 引きずる足の側へ体がぶれる量
inline constexpr float DYING_STEP_SPEED = 4.2f;     ///< 瀕死: 一歩の速さ(ラジアン/秒)
} // namespace PoseTuning

/**
 * @brief 弱り具合による姿勢のずらし量。
 *
 * 高さは足さない(接地計算がピッチを見て最下点を決めているため。高さを直接足すと埋まる)。
 * 横倒し(ロール)も使わない。接地計算がロールを扱っておらず、片側の足が地面へ沈むため。
 * 足を引きずる様子は、一歩ごとの前のめり(ピッチ)と体のぶれ(ヨー)で表す。
 *
 * @param condition  弱り具合。
 * @param moving     歩いているか。瀕死のときだけ足を引きずる動きを足す。
 * @param time       途切れずに進み続ける経過秒(状態が変わっても0へ戻さない)。
 */
inline EnemyPoseOffset EnemyConditionPose(EnemyCondition condition, bool moving, float time)
{
    EnemyPoseOffset offset{};
    switch (condition)
    {
    case EnemyCondition::Tired:
        offset.pitch = PoseTuning::TIRED_HEAD_DROOP +
            PoseTuning::TIRED_BREATH_PITCH * std::sin(time * PoseTuning::TIRED_BREATH_SPEED);
        break;
    case EnemyCondition::Dying:
        if (moving)
        {
            // 片足をかばうので、2歩に1回だけ大きくつんのめる。
            // sinの正の側だけを使い、つんのめりを鋭く、戻りをゆっくりにする。
            const float step = std::sin(time * PoseTuning::DYING_STEP_SPEED);
            const float lurch = std::max(0.0f, step);
            offset.pitch = PoseTuning::DYING_HEAD_DROOP +
                PoseTuning::DYING_LIMP_PITCH * lurch * lurch;
            offset.yaw = PoseTuning::DYING_LIMP_YAW * step;
        }
        else
        {
            offset.pitch = PoseTuning::DYING_HEAD_DROOP +
                PoseTuning::DYING_BREATH_PITCH * std::sin(time * PoseTuning::DYING_BREATH_SPEED);
        }
        break;
    case EnemyCondition::Healthy:
    default:
        break;
    }
    return offset;
}

} // namespace Combat
