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

// 尾回転: 体の回転そのものは敵AIが物理の向き(rot.y)を回して作るので、
// ここで足すのは「回る前に反対側へ捻って溜める」分と「腰を落とす」分だけにする。
// 見た目だけを回すと、当たり判定の基準の向き(物理の向き)と食い違って予兆が嘘になる。
inline constexpr float SPIN_BRACE_PITCH = 0.14f; ///< 尾回転: 腰を落として踏ん張る量
inline constexpr float SPIN_WIND_YAW = 0.40f;    ///< 尾回転: 回る向きと逆へ捻って溜める量
inline constexpr float SPIN_OVERRUN_YAW = 0.18f; ///< 尾回転: 回り切って体が流れる量(隙の表現)

/// 隙(Recovery)で姿勢が戻りきるまでの時間。長いほど「まだ動けない」ことが伝わる。
inline constexpr float RECOVERY_SETTLE_SECONDS = 0.45f;
} // namespace PoseTuning

/**
 * @brief 尾回転の状態。構えを求めるときに外から渡す。ほかの攻撃では使わない。
 *
 * 回る向きで構えが左右反転し、半回転の回数で判定の長さと「一拍」の位置が変わるため、
 * 敵AIが持っている値をそのまま渡してもらう。
 */
struct EnemySpinState
{
    float sign = 1.0f;          ///< 回る向き(+1 / -1)
    int halfTurns = 1;          ///< 半回転の回数
    float activeSeconds = 0.0f; ///< 判定の長さ(0なら攻撃データの値を使う)
};

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
 * @param spin      尾回転の状態(回る向き・半回転の回数・判定の長さ)。ほかの攻撃では使わない。
 */
inline EnemyPoseOffset EnemyAttackPose(
    EnemyAttackKind kind, EnemyAttackPhase phase, float phaseTime,
    const EnemySpinState& spin = {})
{
    EnemyPoseOffset offset{};
    if (phase == EnemyAttackPhase::None)
        return offset;

    const AttackData& attack = EnemyAttackOf(kind);
    const float windupSeconds = std::max(0.01f, attack.frames.anticipationSeconds);
    const float activeSeconds = std::max(
        0.01f,
        spin.activeSeconds > 0.0f ? spin.activeSeconds : attack.frames.activeSeconds);

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

    case EnemyAttackKind::TailSpin:
        if (phase == EnemyAttackPhase::Windup)
        {
            // 腰を落として踏ん張り、回る向きと逆へ体を捻る。
            // 「その場で回る準備をしている」と分かる形にする。捻る向きが回る向きの手がかりになる。
            offset.pitch = PoseTuning::SPIN_BRACE_PITCH * windupT;
            offset.yaw = -spin.sign * PoseTuning::SPIN_WIND_YAW * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            const EnemySpinPhase spinPhase = EnemySpinPhaseAt(phaseTime, spin.halfTurns);
            offset.pitch = PoseTuning::SPIN_BRACE_PITCH * 0.7f;
            if (spinPhase.pausing)
            {
                // 一拍。止まって腰を落とし、次の半回転へ向けてもう一度捻り直す。
                // ここが2回目の呼び動作になる。尾の溜め直しと同じ進み方にして、
                // 「体を捻って構え直している」と一目で分かる形にする。
                // ここで「まだ終わっていない」と見せることが、この攻撃の読み合いそのものになる。
                const float wind = Detail::SmoothStep01(
                    std::clamp((spinPhase.segmentT - 0.20f) / 0.55f, 0.0f, 1.0f));
                offset.pitch = PoseTuning::SPIN_BRACE_PITCH * (0.7f + 0.5f * wind);
                offset.yaw = -spin.sign * PoseTuning::SPIN_WIND_YAW * wind;
            }
            else
            {
                // 溜めた捻りを一気に戻す。回転そのものは物理の向きが担うので、
                // ここは出だしの勢いを足すだけにする(足しすぎると判定の向きとずれる)。
                const float release = Detail::FastOut(std::min(1.0f, spinPhase.segmentT * 2.5f));
                offset.yaw = -spin.sign * PoseTuning::SPIN_WIND_YAW * (1.0f - release);
            }
        }
        else // Recovery
        {
            // 回り切って体が流れたまま止まる。隙が長いので、ここが反撃の本命。
            const float settle =
                1.0f - Detail::SmoothStep01(phaseTime / PoseTuning::RECOVERY_SETTLE_SECONDS);
            offset.pitch = PoseTuning::SPIN_BRACE_PITCH * 0.7f * settle;
            offset.yaw = spin.sign * PoseTuning::SPIN_OVERRUN_YAW * settle;
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

/**
 * @brief 攻撃ごとに体の部位を動かす量(ラジアン)。全身の傾き(EnemyAttackPose)へ重ねる。
 *
 * 全身を傾けるだけでは、3種類の攻撃のシルエットが「前のめり/後ろ反り」の差にしかならない。
 * 首・あご・尾・前脚を攻撃ごとに動かして、遠目にも「何が来るか」が分かる形を作る。
 *
 *   叩き付け : 首を高く持ち上げ、前脚を浮かせて溜める → 首を一気に振り下ろす
 *   噛みつき : 首を引いて低く狙い、あごを開く → 首を前へ突き出す
 *   薙ぎ払い : 首と尾を逆向きに寄せて捻る → 反対側へ振り抜く
 *
 * 動かすのは描画の姿勢だけで、当たり判定は攻撃ごとの距離と左右の広さ(EnemyAttackOf / EnemyHitHalfAngleOf)で決まる。
 * 見た目と判定が食い違うと予兆が嘘になるので、振り抜く向きは判定の広さと揃えること。
 */
struct EnemyTellPose
{
    float neckPitch = 0.0f;    ///< 首を上下へ(正で持ち上げる)
    float neckYaw = 0.0f;      ///< 首を左右へ
    float jawOpen = 0.0f;      ///< あごを開く(正で開く)
    float tailYaw = 0.0f;      ///< 尾を左右へ(根元から先まで同じ割合で曲げる)
    /// 尾の**先だけ**を余分に振る量。根元は少し、先は大きく曲がる。
    /// これが無いと尾が1本の棒のまま体と一緒に回り、振り回している感じが出ない
    /// (実機で「尻尾と体がいっしょ。もっと尻尾を動かしてほしい」と指摘された)。
    float tailWhip = 0.0f;
    float frontLegLift = 0.0f; ///< 前脚を持ち上げる
};

namespace TellTuning
{
// 叩き付け: 首を高く上げて溜め、振り下ろす。前脚も浮かせて「大きく来る」ことを見せる。
inline constexpr float SLAM_NECK_REAR = 0.95f;
inline constexpr float SLAM_NECK_STRIKE = -1.10f;
inline constexpr float SLAM_LEG_LIFT = 0.55f;
inline constexpr float SLAM_JAW = 0.30f;
// 噛みつき: 首を引いて狙い、あごを開く。予兆が短いので形の変化は小さめ。
inline constexpr float BITE_NECK_PULL = 0.40f;
inline constexpr float BITE_NECK_THRUST = -0.50f;
inline constexpr float BITE_JAW = 0.75f;
// 薙ぎ払い: 首と尾を逆向きに寄せて捻り、反対側へ振り抜く。
inline constexpr float SWEEP_NECK_COIL = 1.15f;
inline constexpr float SWEEP_NECK_SWING = -1.50f;
inline constexpr float SWEEP_TAIL_COIL = 0.75f;
// 尾回転: 尾は体と一緒に回るのではなく、遅れて引きずられ、振り終わりに追い越す(むちの動き)。
//   溜め     : 回る向きの逆へ大きく引き寄せる(根元COIL + 先だけWHIP_COIL)
//   回転の前半: 体に遅れて引きずられる(LAG)
//   回転の後半: 一気に追い越して振り抜く(OVERSHOOT)
//   一拍     : 振り抜いた形から戻し、もう一度引き寄せて溜め直す(=2回目の呼び動作)
// 先ほど大きく振るため、WHIPの値は根元(COIL/LAG/OVERSHOOT)と同じくらい大きくしている。
inline constexpr float SPIN_TAIL_COIL = 1.25f;
inline constexpr float SPIN_TAIL_WHIP_COIL = 0.85f;
inline constexpr float SPIN_TAIL_LAG = 0.80f;
inline constexpr float SPIN_TAIL_WHIP_LAG = 0.70f;
inline constexpr float SPIN_TAIL_OVERSHOOT = 0.55f;
inline constexpr float SPIN_TAIL_WHIP_OVERSHOOT = 0.75f;
/// 振り抜き(追い越し)が始まる位置。半回転の何割を過ぎてから尾が追い越すか。
inline constexpr float SPIN_TAIL_WHIP_START = 0.55f;
inline constexpr float SPIN_NECK_TUCK = -0.35f;
} // namespace TellTuning

/** 攻撃の種類と段階から、部位ごとの動かし方を求める。 */
inline EnemyTellPose EnemyAttackTellPose(
    EnemyAttackKind kind, EnemyAttackPhase phase, float phaseTime,
    const EnemySpinState& spin = {})
{
    EnemyTellPose tell{};
    if (phase == EnemyAttackPhase::None)
        return tell;

    const AttackData& attack = EnemyAttackOf(kind);
    const float windupSeconds = std::max(0.01f, attack.frames.anticipationSeconds);
    const float activeSeconds = std::max(
        0.01f,
        spin.activeSeconds > 0.0f ? spin.activeSeconds : attack.frames.activeSeconds);
    const float windupT = Detail::SmoothStep01(phaseTime / windupSeconds);
    const float activeT = std::clamp(phaseTime / activeSeconds, 0.0f, 1.0f);
    // 隙では、振り抜いた形からゆっくり戻る。戻りきる前が反撃の機会になる。
    const float settle =
        1.0f - Detail::SmoothStep01(phaseTime / PoseTuning::RECOVERY_SETTLE_SECONDS);

    switch (kind)
    {
    case EnemyAttackKind::Slam:
        if (phase == EnemyAttackPhase::Windup)
        {
            tell.neckPitch = TellTuning::SLAM_NECK_REAR * windupT;
            tell.frontLegLift = TellTuning::SLAM_LEG_LIFT * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            // 出だしを速くして、振り下ろしの打点を分かりやすくする。
            const float strike = Detail::FastOut(activeT / 0.35f);
            tell.neckPitch =
                TellTuning::SLAM_NECK_REAR * (1.0f - strike) +
                TellTuning::SLAM_NECK_STRIKE * strike;
            tell.frontLegLift = TellTuning::SLAM_LEG_LIFT * (1.0f - strike);
            tell.jawOpen = TellTuning::SLAM_JAW * strike;
        }
        else
        {
            tell.neckPitch = TellTuning::SLAM_NECK_STRIKE * settle;
        }
        break;

    case EnemyAttackKind::Bite:
        if (phase == EnemyAttackPhase::Windup)
        {
            const float aim = Detail::FastOut(windupT);
            tell.neckPitch = TellTuning::BITE_NECK_PULL * aim;
            tell.jawOpen = TellTuning::BITE_JAW * aim;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            const float thrust = Detail::Pulse01(activeT);
            tell.neckPitch =
                TellTuning::BITE_NECK_PULL + TellTuning::BITE_NECK_THRUST * thrust;
            tell.jawOpen = TellTuning::BITE_JAW * (1.0f - thrust * 0.6f);
        }
        else
        {
            tell.neckPitch = TellTuning::BITE_NECK_PULL * settle;
            tell.jawOpen = TellTuning::BITE_JAW * settle * 0.5f;
        }
        break;

    case EnemyAttackKind::TailSpin:
        if (phase == EnemyAttackPhase::Windup)
        {
            // 尾を回る向きの逆へ大きく寄せて溜める。ここが「尾で来る」手がかりになるので、
            // 4種類の中で最も大きく動かす。首は低くたたんで巻き込む。
            tell.tailYaw = -spin.sign * TellTuning::SPIN_TAIL_COIL * windupT;
            tell.tailWhip = -spin.sign * TellTuning::SPIN_TAIL_WHIP_COIL * windupT;
            tell.neckPitch = TellTuning::SPIN_NECK_TUCK * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            const EnemySpinPhase spinPhase = EnemySpinPhaseAt(phaseTime, spin.halfTurns);
            tell.neckPitch = TellTuning::SPIN_NECK_TUCK;
            if (spinPhase.pausing)
            {
                // 一拍。ここが2回目の呼び動作になる。
                //   前半: 振り抜いて追い越した形から戻す
                //   後半: もう一度大きく引き寄せて溜め直し、溜めたまま待つ
                // 「止まった=終わった」ではないことを尾の形で見せる。
                // ここを省くと、止まった次の瞬間に尾が来て避けられない(実機で指摘された)。
                const float settleOut =
                    1.0f - Detail::SmoothStep01(std::min(1.0f, spinPhase.segmentT / 0.25f));
                const float wind = Detail::SmoothStep01(
                    std::clamp((spinPhase.segmentT - 0.20f) / 0.55f, 0.0f, 1.0f));
                tell.tailYaw =
                    spin.sign * TellTuning::SPIN_TAIL_OVERSHOOT * settleOut -
                    spin.sign * TellTuning::SPIN_TAIL_COIL * wind;
                tell.tailWhip =
                    spin.sign * TellTuning::SPIN_TAIL_WHIP_OVERSHOOT * settleOut -
                    spin.sign * TellTuning::SPIN_TAIL_WHIP_COIL * wind;
            }
            else
            {
                // 回っている間、尾は体と一緒には回らない。
                // 前半は体に引きずられて遅れ、後半で一気に追い越して振り抜く(むちの動き)。
                // 先(tailWhip)ほど大きく遅れ、大きく追い越す。
                const float whip = Detail::SmoothStep01(std::clamp(
                    (spinPhase.segmentT - TellTuning::SPIN_TAIL_WHIP_START) /
                        (1.0f - TellTuning::SPIN_TAIL_WHIP_START),
                    0.0f, 1.0f));
                tell.tailYaw =
                    -spin.sign * TellTuning::SPIN_TAIL_LAG * (1.0f - whip) +
                    spin.sign * TellTuning::SPIN_TAIL_OVERSHOOT * whip;
                tell.tailWhip =
                    -spin.sign * TellTuning::SPIN_TAIL_WHIP_LAG * (1.0f - whip) +
                    spin.sign * TellTuning::SPIN_TAIL_WHIP_OVERSHOOT * whip;
            }
        }
        else
        {
            // 隙。振り抜いた形からゆっくり戻る。
            tell.tailYaw = spin.sign * TellTuning::SPIN_TAIL_OVERSHOOT * settle;
            tell.tailWhip = spin.sign * TellTuning::SPIN_TAIL_WHIP_OVERSHOOT * settle;
            tell.neckPitch = TellTuning::SPIN_NECK_TUCK * settle;
        }
        break;

    case EnemyAttackKind::Sweep:
        if (phase == EnemyAttackPhase::Windup)
        {
            tell.neckYaw = TellTuning::SWEEP_NECK_COIL * windupT;
            tell.tailYaw = -TellTuning::SWEEP_TAIL_COIL * windupT;
        }
        else if (phase == EnemyAttackPhase::Active)
        {
            // 溜めた側から反対側へ一気に振り抜く。尾は首と逆向きに振れて体の回転を見せる。
            const float swing = Detail::FastOut(activeT);
            tell.neckYaw =
                TellTuning::SWEEP_NECK_COIL * (1.0f - swing) +
                TellTuning::SWEEP_NECK_SWING * swing;
            tell.tailYaw =
                -TellTuning::SWEEP_TAIL_COIL * (1.0f - swing) +
                TellTuning::SWEEP_TAIL_COIL * swing;
            tell.jawOpen = TellTuning::SLAM_JAW * swing;
        }
        else
        {
            tell.neckYaw = TellTuning::SWEEP_NECK_SWING * settle;
            tell.tailYaw = TellTuning::SWEEP_TAIL_COIL * settle;
        }
        break;
    }
    return tell;
}

} // namespace Combat
