#include "OneVsOneCombat.h"

#include <algorithm>

using DirectX::SimpleMath::Vector3;

namespace
{
    float DistanceXZ(const Vector3& left, const Vector3& right)
    {
        Vector3 delta = right - left;
        delta.y = 0.0f;
        return delta.Length();
    }
}

void OneVsOneCombat::Reset()
{
    m_playerHp = MAX_HP;
    m_enemyHp = MAX_HP;
    m_enemyCooldown = 0.7f;
    m_playerGuardPressed = false;
    m_playerAttack = {};
    m_enemyAttack = {};
}

void OneVsOneCombat::Update(
    uint64_t deltaMicroseconds,
    const Vector3& playerPosition,
    const Vector3& enemyPosition,
    bool playerAttackTriggered,
    bool playerGuardPressed)
{
    const float deltaSeconds = std::min(
        static_cast<float>(deltaMicroseconds) / 1000000.0f,
        0.1f);
    const float distance = DistanceXZ(playerPosition, enemyPosition);

    m_playerGuardPressed = playerGuardPressed;
    m_enemyCooldown = std::max(0.0f, m_enemyCooldown - deltaSeconds);

    if (IsPlayerDefeated())
    {
        m_playerAttack.phase = Phase::Defeated;
    }
    else if (IsEnemyDefeated())
    {
        m_enemyAttack.phase = Phase::Defeated;
    }
    else
    {
        if (playerAttackTriggered && m_playerAttack.phase == Phase::Ready && !m_playerGuardPressed)
        {
            m_playerAttack = { Phase::Windup, 0.0f, false };
        }

        if (m_enemyAttack.phase == Phase::Ready && m_enemyCooldown <= 0.0f && distance <= ATTACK_RANGE)
        {
            m_enemyAttack = { Phase::Windup, 0.0f, false };
            m_enemyCooldown = ENEMY_COOLDOWN;
        }

        m_playerAttack.elapsed += deltaSeconds;
        if (m_playerAttack.phase == Phase::Windup && m_playerAttack.elapsed >= PLAYER_WINDUP)
        {
            m_playerAttack.phase = Phase::Active;
            m_playerAttack.elapsed = 0.0f;
        }
        else if (m_playerAttack.phase == Phase::Active)
        {
            if (!m_playerAttack.hit && distance <= ATTACK_RANGE)
            {
                m_enemyHp = std::max(0.0f, m_enemyHp - PLAYER_DAMAGE);
                m_playerAttack.hit = true;
            }
            if (m_playerAttack.elapsed >= PLAYER_ACTIVE)
            {
                m_playerAttack.phase = Phase::Recovery;
                m_playerAttack.elapsed = 0.0f;
            }
        }
        else if (m_playerAttack.phase == Phase::Recovery && m_playerAttack.elapsed >= PLAYER_RECOVERY)
        {
            m_playerAttack = {};
        }

        m_enemyAttack.elapsed += deltaSeconds;
        if (m_enemyAttack.phase == Phase::Windup && m_enemyAttack.elapsed >= ENEMY_WINDUP)
        {
            m_enemyAttack.phase = Phase::Active;
            m_enemyAttack.elapsed = 0.0f;
        }
        else if (m_enemyAttack.phase == Phase::Active)
        {
            if (!m_enemyAttack.hit && distance <= ATTACK_RANGE)
            {
                const float damage = m_playerGuardPressed ? ENEMY_DAMAGE * 0.2f : ENEMY_DAMAGE;
                m_playerHp = std::max(0.0f, m_playerHp - damage);
                m_enemyAttack.hit = true;
            }
            if (m_enemyAttack.elapsed >= ENEMY_ACTIVE)
            {
                m_enemyAttack.phase = Phase::Recovery;
                m_enemyAttack.elapsed = 0.0f;
            }
        }
        else if (m_enemyAttack.phase == Phase::Recovery && m_enemyAttack.elapsed >= ENEMY_RECOVERY)
        {
            m_enemyAttack = {};
        }

        if (IsEnemyDefeated())
        {
            m_enemyAttack = { Phase::Defeated, 0.0f, false };
        }
        if (IsPlayerDefeated())
        {
            m_playerAttack = { Phase::Defeated, 0.0f, false };
        }
    }
}

std::string_view OneVsOneCombat::GetStateName() const
{
    if (IsEnemyDefeated())
    {
        return "ENEMY DOWN - YOU WIN";
    }
    if (IsPlayerDefeated())
    {
        return "PLAYER DOWN - TRY AGAIN";
    }
    if (m_playerAttack.phase == Phase::Active)
    {
        return "PLAYER ATTACK";
    }
    if (m_enemyAttack.phase == Phase::Active)
    {
        return "ENEMY ATTACK";
    }
    if (IsPlayerGuarding())
    {
        return "GUARDING";
    }
    if (m_playerAttack.phase == Phase::Windup || m_playerAttack.phase == Phase::Recovery)
    {
        return "PLAYER RECOVERY";
    }
    if (m_enemyAttack.phase == Phase::Windup || m_enemyAttack.phase == Phase::Recovery)
    {
        return "ENEMY RECOVERY";
    }
    return "READY";
}
