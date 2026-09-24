#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include <SimpleMath.h>

#include "collision.h"
#include "CombatAttackTable.h"

class OneVsOneCombat
{
public:
	struct CollisionDebugState
	{
		bool swordTransformValid = false;
		bool broadPhaseOverlap = false;
		bool narrowPhaseTested = false;
		bool narrowPhaseHit = false;
		GM31::GE::Collision::BoundingBoxAABB attackBroadPhase{};
		GM31::GE::Collision::BoundingBoxAABB playerBroadPhase{};
		GM31::GE::Collision::BoundingBoxAABB enemyBroadPhase{};
		GM31::GE::Collision::BoundingBoxOBB bladeObb{};
		GM31::GE::Collision::BoundingBoxOBB tipSweepObb{};
		GM31::GE::Collision::BoundingBoxOBB playerObb{};
		GM31::GE::Collision::BoundingBoxOBB enemyObb{};
	};

    enum class Phase
    {
        Ready,
        Windup,
        Active,
        Recovery,
        Defeated,
    };

    void Reset();
	// 敵が今から出す攻撃を設定する。敵AI(gameobject/enemy)が選んだものをそのまま渡すことで、
	// 予兆として見えているモーションと、実際のダメージ・射程・タイミングが必ず一致する。
	// 未設定(nullptr)の場合は標準の叩き付けとして扱う。
	void SetEnemyAttackKind(Combat::EnemyAttackKind kind) { m_enemyAttackKind = kind; }
	Combat::EnemyAttackKind GetEnemyAttackKind() const { return m_enemyAttackKind; }
	// 敵の向き(ヨー)。攻撃判定を正面からの角度で絞るために使う。
	// Update()の引数がすでに多いため、攻撃の種類と同じくセッターで渡す。
	void SetEnemyFacingYaw(float yawRadians) { m_enemyFacingYaw = yawRadians; }
	// 尾回転の半回転の回数(1か2)。敵AIが決めたものをそのまま渡す。
	// 判定の長さ(半回転と一拍の合計)も、いつ止まっているかも、ここから同じ計算で求める。
	// 別々に持つと「見た目は止まっているのに当たる」といったずれが生まれる。
	void SetEnemySpinHalfTurns(int halfTurns) { m_enemySpinHalfTurns = halfTurns; }
	// プレイヤーが走っているか。走りながら弱攻撃を押すとダッシュ攻撃になる(Update前に設定する)。
	void SetPlayerSprinting(bool sprinting) { m_playerSprinting = sprinting; }
	bool IsPlayerDashAttack() const { return m_playerAttack.dash && m_playerAttack.comboStep == 1; }
	void ClearCollisionDebug()
	{
		m_collisionDebug = {};
		m_playerHitGrace = 0.0f;
	}
	void ClearEnemyCollisionDebug();
	void UpdatePlayerCollisionDebug(
		const DirectX::SimpleMath::Vector3& swordBase,
		const DirectX::SimpleMath::Vector3& swordTip,
		const DirectX::SimpleMath::Vector3& previousSwordTip,
		bool swordTransformValid,
		const GM31::GE::Collision::BoundingBoxOBB& playerObb);

	void Update(
        uint64_t deltaMicroseconds,
        const DirectX::SimpleMath::Vector3& playerPosition,
        const DirectX::SimpleMath::Vector3& enemyPosition,
		bool playerAttackTriggered,
		bool playerHeavyAttackTriggered,
		bool enemyAttackTriggered,
		bool playerInvincible,
		const DirectX::SimpleMath::Vector3& swordBase,
		const DirectX::SimpleMath::Vector3& swordTip,
		const DirectX::SimpleMath::Vector3& previousSwordTip,
		bool swordTransformValid,
		const GM31::GE::Collision::BoundingBoxOBB& playerObb,
		const GM31::GE::Collision::BoundingBoxOBB& enemyObb);
	// 敵がいないとき(素振り)の更新。プレイヤーの攻撃の段・判定・硬直だけを進め、ダメージは出さない。
	// 敵がいないとUpdate()を呼べず、攻撃が1段目のまま進まずコンボも出なかったため。
	void UpdateWithoutEnemy(
		uint64_t deltaMicroseconds,
		bool playerAttackTriggered,
		bool playerHeavyAttackTriggered);

    float GetPlayerHp() const { return m_playerHp; }
    float GetEnemyHp() const { return m_enemyHp; }
    float GetPlayerMaxHp() const { return PLAYER_MAX_HP; }
    float GetEnemyMaxHp() const { return ENEMY_MAX_HP; }
    // 調整用。敵の体力を直接設定する(弱り具合の見た目を確かめるため。デバッグ表示から使う)。
    void SetEnemyHpForDebug(float hp) { m_enemyHp = std::clamp(hp, 1.0f, ENEMY_MAX_HP); }
    // 撮影・調整用の無敵。攻撃は当たって演出(火花・怯み・吹き飛ばし)も出るが、体力だけ減らない。
    // 仕切り直し(Reset)をしても設定は残す。
    void SetEnemyDebugInvincible(bool invincible) { m_enemyDebugInvincible = invincible; }
    bool IsEnemyDebugInvincible() const { return m_enemyDebugInvincible; }
    void SetPlayerDebugInvincible(bool invincible) { m_playerDebugInvincible = invincible; }
    bool IsPlayerDebugInvincible() const { return m_playerDebugInvincible; }
    // この更新でプレイヤーの攻撃が敵に当たったか / 敵の攻撃がプレイヤーに当たったか。
    // 命中の演出は体力の増減ではなくこれで出す(無敵でも演出は出したいため)。
    bool DidPlayerHitLand() const { return m_playerHitLanded; }
    bool DidEnemyHitLand() const { return m_enemyHitLanded; }
    bool IsPlayerDefeated() const { return m_playerHp <= 0.0f; }
    bool IsEnemyDefeated() const { return m_enemyHp <= 0.0f; }
	bool CanStartPlayerAttack() const { return m_playerAttack.phase == Phase::Ready && !IsPlayerDefeated() && !IsEnemyDefeated(); }
	bool IsPlayerAttacking() const { return m_playerAttack.phase == Phase::Windup || m_playerAttack.phase == Phase::Active || m_playerAttack.phase == Phase::Recovery; }
	bool CanCancelPlayerAttack(int cancelEndFrame) const;
	void CancelPlayerAttack();
	// 敵が怯んだときに、進行中の攻撃を取り消す。
	// 敵AIだけ怯ませて戦闘側の攻撃を残すと、のけぞっている敵からダメージが飛んでくる。
	void CancelEnemyAttack();
	int GetPlayerAttackFrame() const;
	bool IsPlayerAttackActive() const { return m_playerAttack.phase == Phase::Active; }
	int GetPlayerComboStep() const { return m_playerAttack.comboStep; }
	bool IsPlayerHeavyAttack() const { return m_playerAttack.kind == AttackKind::Heavy; }
	bool IsPlayerComboQueued() const { return m_playerAttack.comboQueued > 0; }
	const CollisionDebugState& GetCollisionDebugState() const { return m_collisionDebug; }
    bool IsEnemyAttacking() const { return m_enemyAttack.phase == Phase::Windup || m_enemyAttack.phase == Phase::Active || m_enemyAttack.phase == Phase::Recovery; }
    std::string_view GetStateName() const;

private:
	// プレイヤーの攻撃の入力受付と段の進行。敵の有無に関係なく同じ処理を通す。
	// canHitEnemy=false のときは判定中でもダメージを与えない。
	void AdvancePlayerAttack(
		float deltaSeconds,
		bool playerAttackTriggered,
		bool playerHeavyAttackTriggered,
		bool canHitEnemy);

	enum class AttackKind { Normal, Heavy };
    struct AttackState
    {
        Phase phase = Phase::Ready;
        float elapsed = 0.0f;
        bool hit = false;
		int hitCount = 0;
		AttackKind kind = AttackKind::Normal;
		float totalElapsed = 0.0f;
		int comboStep = 1;
		int comboQueued = 0;
		// 走りから出したダッシュ攻撃か。1段目だけダッシュ攻撃のクリップと時間を使う。
		bool dash = false;
    };

    // 攻撃の時間・威力の定義元は system/CombatAttackTable.h に集約している。
    // 敵AI(gameobject/enemy)も同じテーブルを参照するため、
    // 「アニメーションの予備動作」と「実際の攻撃判定」が必ず一致する。
    static constexpr float PLAYER_MAX_HP = Combat::Tuning::PLAYER_MAX_HP;
    static constexpr float ENEMY_MAX_HP = Combat::Tuning::ENEMY_MAX_HP;
    static constexpr float PLAYER_DAMAGE =
        static_cast<float>(Combat::Tuning::PLAYER_WEAK_DAMAGE);
    static constexpr float ENEMY_DAMAGE =
        static_cast<float>(Combat::Tuning::ENEMY_DAMAGE);
	static constexpr float ENEMY_ATTACK_RANGE = Combat::Tuning::ENEMY_HIT_RANGE;
	// プレイヤーの攻撃の時間(予兆・判定・硬直)はコンボの段ごとに違うので、
	// Combat::PlayerComboStepOf() から都度引く。
	static constexpr float HEAVY_DAMAGE =
		static_cast<float>(Combat::Tuning::PLAYER_HEAVY_DAMAGE);
    // 敵の移動も攻撃に合わせる。予備動作を見やすくし、攻撃後の硬直を十分に取ることで、
    // 空振りや命中後にプレイヤーが反撃できるようにする。
    static constexpr float ENEMY_WINDUP = Combat::Tuning::ENEMY_ANTICIPATION;
    static constexpr float ENEMY_ACTIVE = Combat::Tuning::ENEMY_ACTIVE;
    static constexpr float ENEMY_RECOVERY = Combat::Tuning::ENEMY_RECOVERY;
    static constexpr float ENEMY_COOLDOWN = Combat::Tuning::ENEMY_COOLDOWN;
	// 1回の攻撃で当てられる回数は Combat::EnemyMaxHitsOf() が決める
	// (通常は1回。尾回転だけ半回転ごとに1回)。

    float m_playerHp = PLAYER_MAX_HP;
    float m_enemyHp = ENEMY_MAX_HP;
    float m_enemyCooldown = 0.7f;
    // 敵が現在出している攻撃の種類。時間・威力・射程はここから引く。
    Combat::EnemyAttackKind m_enemyAttackKind = Combat::EnemyAttackKind::Slam;
    float m_enemyFacingYaw = 0.0f;
    int m_enemySpinHalfTurns = 1;
    bool m_playerSprinting = false;
    AttackState m_playerAttack{};
    AttackState m_enemyAttack{};
	CollisionDebugState m_collisionDebug{};
	float m_playerHitGrace = 0.0f;
	bool m_enemyDebugInvincible = false;
	bool m_playerDebugInvincible = false;
	bool m_playerHitLanded = false;
	bool m_enemyHitLanded = false;
};
