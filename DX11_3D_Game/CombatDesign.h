#pragma once

#include <string>
#include <vector>

/**
 * @namespace Combat
 * @brief 一対一アクションの読み合い、攻撃フレーム、命中判定を表す設計層。
 */
namespace Combat
{
/**
 * @enum AttackPhase
 * @brief 攻撃を「読む時間」「当たる時間」「反撃できる時間」に分ける。
 */
enum class AttackPhase
{
    Idle,          ///< 攻撃していない。
    Anticipation,  ///< 前兆。プレイヤーが回避、ガード、間合い調整を判断する時間。
    Active,        ///< 攻撃判定が有効な時間。
    Recovery,      ///< 攻撃後の硬直。プレイヤーが反撃する主なすき。
    Cooldown,      ///< 次行動までの制限時間。
};

/**
 * @enum NarrowPhaseType
 * @brief 精密な当たり判定で使う形状。
 */
enum class NarrowPhaseType
{
    Sphere,  ///< 軽量な球判定。
    Capsule, ///< 武器や腕、尻尾に使いやすいカプセル判定。
    Obb,     ///< 角度付き箱判定。武器の刃や範囲攻撃向け。
};

/**
 * @enum GuardType
 * @brief 攻撃がガードにどう扱われるか。
 */
enum class GuardType
{
    Blockable,     ///< 通常ガード可能。
    GuardBreak,    ///< ガードはできるが姿勢値を大きく削る。
    Unblockable,   ///< ガード不可。前兆を大きくして公平性を担保する。
};

/**
 * @struct AttackFrameDefinition
 * @brief 1つの攻撃に含まれる時間区間。
 *
 * 設計プランのAnticipation/Active/Recovery/Cooldown/Commitをそのままデータ化します。
 * 秒指定にしておくことで、60FPS以外の検証ツールからも同じデータを扱えます。
 */
struct AttackFrameDefinition
{
    float anticipationSeconds = 0.35f; ///< 前兆時間。
    float activeSeconds = 0.12f;       ///< 攻撃判定が出る時間。
    float recoverySeconds = 0.45f;     ///< 反撃可能な硬直時間。
    float cooldownSeconds = 0.20f;     ///< 次行動までの待ち時間。
    float commitSeconds = 0.20f;       ///< 一度始めたら中断できない時間。
};

/**
 * @struct BroadPhaseFilter
 * @brief 精密判定の前に、距離、角度、カテゴリで候補を絞る条件。
 */
struct BroadPhaseFilter
{
    float maxDistance = 3.0f;      ///< 攻撃者から対象までの最大距離。
    float maxAngleDegrees = 70.0f; ///< 攻撃者の正面から許容する角度。
    bool acceptsGuardingTarget = true;
    bool acceptsDownedTarget = false;
};

/**
 * @struct HitboxDefinition
 * @brief 攻撃側の判定名と形状。
 */
struct HitboxDefinition
{
    std::string name;                              ///< 例: claw_left, sword_tip。
    NarrowPhaseType narrowPhase = NarrowPhaseType::Capsule;
    float radius = 0.25f;
    float length = 0.8f;
};

/**
 * @struct AttackData
 * @brief 敵やプレイヤーが使う1攻撃分のデータ。
 *
 * AnimationName、VFX、SFX、NextActionCandidatesを持たせ、後からDataAsset/JSON/BinaryAssetへ
 * 外部化できる形にしています。
 */
struct AttackData
{
    std::string attackId;
    std::string animationName;
    AttackFrameDefinition frames;
    BroadPhaseFilter broadPhaseFilter;
    std::vector<HitboxDefinition> hitboxes;
    GuardType guardType = GuardType::Blockable;
    int damage = 10;
    int staminaDamage = 8;
    int postureDamage = 5;
    float trackingAngleDegrees = 25.0f;
    float trackingSpeed = 3.0f;
    float moveDistance = 0.0f;
    float turnLimitDegrees = 35.0f;
    std::string vfx;
    std::string sfx;
    std::vector<std::string> nextActionCandidates;
    float fatigueCost = 0.0f;
};

/**
 * @struct CollisionPair
 * @brief ナローフェーズ後に確定した命中結果。
 */
struct CollisionPair
{
    std::string attackerId;
    std::string targetId;
    std::string attackId;
    std::string hitboxName;
};

/**
 * @struct CombatDebugState
 * @brief ImGuiとブラウザデバッグに表示する戦闘状態。
 */
struct CombatDebugState
{
    std::string currentAttackId = "none";
    AttackPhase currentPhase = AttackPhase::Idle;
    int broadPhaseCandidateCount = 0;
    int confirmedCollisionCount = 0;
    int playerHp = 100;
    int playerStamina = 100;
    int enemyHp = 160;
    float distanceMeters = 3.0f;
    bool playerGuarding = false;
    bool enemyInRecovery = false;
};

/**
 * @brief Phase 1の確認用に使う、読みやすいサンプル攻撃データを作る。
 */
AttackData CreatePrototypeHeavySlash();

/**
 * @brief 攻撃フェーズをデバッグ表示用の文字列へ変換する。
 */
const char* ToString(AttackPhase phase);
} // namespace Combat
