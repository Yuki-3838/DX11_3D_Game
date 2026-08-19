#pragma once

#include <cstdint>
#include <string_view>

#include <SimpleMath.h>

#include "collision.h"

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

    void Update(
        uint64_t deltaMicroseconds,
        const DirectX::SimpleMath::Vector3& playerPosition,
        const DirectX::SimpleMath::Vector3& enemyPosition,
        bool playerAttackTriggered,
		bool enemyAttackTriggered,
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
	bool IsPlayerAttackActive() const { return m_playerAttack.phase == Phase::Active; }
	const CollisionDebugState& GetCollisionDebugState() const { return m_collisionDebug; }
    bool IsEnemyAttacking() const { return m_enemyAttack.phase == Phase::Windup || m_enemyAttack.phase == Phase::Active || m_enemyAttack.phase == Phase::Recovery; }
    std::string_view GetStateName() const;

private:
    struct AttackState
    {
        Phase phase = Phase::Ready;
        float elapsed = 0.0f;
        bool hit = false;
		int hitCount = 0;
    };

    static constexpr float MAX_HP = 100.0f;
    static constexpr float PLAYER_DAMAGE = 25.0f;
    static constexpr float ENEMY_DAMAGE = 15.0f;
	static constexpr float ENEMY_ATTACK_RANGE = 64.0f;
	// Match the 1.15 second reference-retargeted attack motion: preparation
	// 0.00-0.30,
	// damaging swing 0.18-0.62, recovery 0.62-0.90.
	static constexpr float PLAYER_WINDUP = 0.18f;
	static constexpr float PLAYER_ACTIVE = 0.44f;
	static constexpr float PLAYER_RECOVERY = 0.28f;
    // Match enemy movement: the windup is readable and the post-attack
    // recovery is long enough for the player to punish the whiff or hit.
    static constexpr float ENEMY_WINDUP = 0.72f;
    static constexpr float ENEMY_ACTIVE = 0.90f;
    static constexpr float ENEMY_RECOVERY = 1.35f;
    static constexpr float ENEMY_COOLDOWN = 1.15f;
	static constexpr int ENEMY_MAX_HITS = 1;

    float m_playerHp = MAX_HP;
    float m_enemyHp = MAX_HP;
    float m_enemyCooldown = 0.7f;
    AttackState m_playerAttack{};
    AttackState m_enemyAttack{};
	CollisionDebugState m_collisionDebug{};
	float m_playerHitGrace = 0.0f;
};
