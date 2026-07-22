#pragma once

#include <cstdint>
#include <string_view>

#include <SimpleMath.h>

class OneVsOneCombat
{
public:
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
        bool playerGuardPressed);

    float GetPlayerHp() const { return m_playerHp; }
    float GetEnemyHp() const { return m_enemyHp; }
    bool IsPlayerDefeated() const { return m_playerHp <= 0.0f; }
    bool IsEnemyDefeated() const { return m_enemyHp <= 0.0f; }
    bool IsPlayerGuarding() const { return m_playerGuardPressed && !IsPlayerDefeated(); }
    bool IsPlayerAttacking() const { return m_playerAttack.phase == Phase::Windup || m_playerAttack.phase == Phase::Active || m_playerAttack.phase == Phase::Recovery; }
    bool IsEnemyAttacking() const { return m_enemyAttack.phase == Phase::Windup || m_enemyAttack.phase == Phase::Active || m_enemyAttack.phase == Phase::Recovery; }
    std::string_view GetStateName() const;

private:
    struct AttackState
    {
        Phase phase = Phase::Ready;
        float elapsed = 0.0f;
        bool hit = false;
    };

    static constexpr float MAX_HP = 100.0f;
    static constexpr float PLAYER_DAMAGE = 25.0f;
    static constexpr float ENEMY_DAMAGE = 15.0f;
    static constexpr float ATTACK_RANGE = 125.0f;
    static constexpr float PLAYER_WINDUP = 0.14f;
    static constexpr float PLAYER_ACTIVE = 0.12f;
    static constexpr float PLAYER_RECOVERY = 0.28f;
    static constexpr float ENEMY_WINDUP = 0.38f;
    static constexpr float ENEMY_ACTIVE = 0.12f;
    static constexpr float ENEMY_RECOVERY = 0.35f;
    static constexpr float ENEMY_COOLDOWN = 1.15f;

    float m_playerHp = MAX_HP;
    float m_enemyHp = MAX_HP;
    float m_enemyCooldown = 0.7f;
    bool m_playerGuardPressed = false;
    AttackState m_playerAttack{};
    AttackState m_enemyAttack{};
};
