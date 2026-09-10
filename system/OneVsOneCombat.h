#pragma once

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

    float GetPlayerHp() const { return m_playerHp; }
    float GetEnemyHp() const { return m_enemyHp; }
    bool IsPlayerDefeated() const { return m_playerHp <= 0.0f; }
    bool IsEnemyDefeated() const { return m_enemyHp <= 0.0f; }
	bool CanStartPlayerAttack() const { return m_playerAttack.phase == Phase::Ready && !IsPlayerDefeated() && !IsEnemyDefeated(); }
	bool IsPlayerAttacking() const { return m_playerAttack.phase == Phase::Windup || m_playerAttack.phase == Phase::Active || m_playerAttack.phase == Phase::Recovery; }
	bool CanCancelPlayerAttack(int cancelEndFrame) const;
	void CancelPlayerAttack();
	int GetPlayerAttackFrame() const;
	bool IsPlayerAttackActive() const { return m_playerAttack.phase == Phase::Active; }
	int GetPlayerComboStep() const { return m_playerAttack.comboStep; }
	bool IsPlayerHeavyAttack() const { return m_playerAttack.kind == AttackKind::Heavy; }
	bool IsPlayerComboQueued() const { return m_playerAttack.comboQueued > 0; }
	const CollisionDebugState& GetCollisionDebugState() const { return m_collisionDebug; }
    bool IsEnemyAttacking() const { return m_enemyAttack.phase == Phase::Windup || m_enemyAttack.phase == Phase::Active || m_enemyAttack.phase == Phase::Recovery; }
    std::string_view GetStateName() const;

private:
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
    };

    // 攻撃の時間・威力の定義元は system/CombatAttackTable.h に集約している。
    // 敵AI(gameobject/enemy)も同じテーブルを参照するため、
    // 「アニメーションの予備動作」と「実際の攻撃判定」が必ず一致する。
    static constexpr float MAX_HP = 100.0f;
    static constexpr float PLAYER_DAMAGE =
        static_cast<float>(Combat::Tuning::PLAYER_WEAK_DAMAGE);
    static constexpr float ENEMY_DAMAGE =
        static_cast<float>(Combat::Tuning::ENEMY_DAMAGE);
	static constexpr float ENEMY_ATTACK_RANGE = Combat::Tuning::ENEMY_HIT_RANGE;
	// プレイヤーアニメーターで使用する、短く地に足の着いた一閃モーションに合わせる。
	static constexpr float PLAYER_WINDUP = Combat::Tuning::PLAYER_WEAK_ANTICIPATION;
	static constexpr float PLAYER_ACTIVE = Combat::Tuning::PLAYER_WEAK_ACTIVE;
	static constexpr float PLAYER_RECOVERY = Combat::Tuning::PLAYER_WEAK_RECOVERY;
	static constexpr float HEAVY_WINDUP = Combat::Tuning::PLAYER_HEAVY_ANTICIPATION;
	static constexpr float HEAVY_ACTIVE = Combat::Tuning::PLAYER_HEAVY_ACTIVE;
	static constexpr float HEAVY_RECOVERY = Combat::Tuning::PLAYER_HEAVY_RECOVERY;
	static constexpr float HEAVY_DAMAGE =
		static_cast<float>(Combat::Tuning::PLAYER_HEAVY_DAMAGE);
    // 敵の移動も攻撃に合わせる。予備動作を見やすくし、攻撃後の硬直を十分に取ることで、
    // 空振りや命中後にプレイヤーが反撃できるようにする。
    static constexpr float ENEMY_WINDUP = Combat::Tuning::ENEMY_ANTICIPATION;
    static constexpr float ENEMY_ACTIVE = Combat::Tuning::ENEMY_ACTIVE;
    static constexpr float ENEMY_RECOVERY = Combat::Tuning::ENEMY_RECOVERY;
    static constexpr float ENEMY_COOLDOWN = Combat::Tuning::ENEMY_COOLDOWN;
	static constexpr int ENEMY_MAX_HITS = 1;

    float m_playerHp = MAX_HP;
    float m_enemyHp = MAX_HP;
    float m_enemyCooldown = 0.7f;
    // 敵が現在出している攻撃の種類。時間・威力・射程はここから引く。
    Combat::EnemyAttackKind m_enemyAttackKind = Combat::EnemyAttackKind::Slam;
    AttackState m_playerAttack{};
    AttackState m_enemyAttack{};
	CollisionDebugState m_collisionDebug{};
	float m_playerHitGrace = 0.0f;
};
