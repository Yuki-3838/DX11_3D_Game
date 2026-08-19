#include "OneVsOneCombat.h"

#include <algorithm>
#include <array>
#include <cmath>

using DirectX::SimpleMath::Vector3;

namespace
{
    using GM31::GE::Collision::BoundingBoxAABB;
    using GM31::GE::Collision::BoundingBoxOBB;

    float DistanceXZ(const Vector3& left, const Vector3& right)
    {
        Vector3 delta = right - left;
        delta.y = 0.0f;
        return delta.Length();
    }

    BoundingBoxAABB MakePointCloudAabb(
        const Vector3& swordBase,
        const Vector3& swordTip,
        const Vector3& previousSwordTip,
        float padding)
    {
        BoundingBoxAABB result{};
        result.min = Vector3::Min(swordBase, Vector3::Min(swordTip, previousSwordTip)) -
            Vector3(padding, padding, padding);
        result.max = Vector3::Max(swordBase, Vector3::Max(swordTip, previousSwordTip)) +
            Vector3(padding, padding, padding);
        return result;
    }

    BoundingBoxOBB MakeSegmentObb(
        const Vector3& start,
        const Vector3& end,
        float thickness)
    {
        Vector3 axisY = end - start;
        float length = axisY.Length();
        if (length <= 0.0001f)
        {
            axisY = Vector3(0.0f, 1.0f, 0.0f);
            length = 0.0001f;
        }
        else
        {
            axisY /= length;
        }

        const Vector3 helper = std::abs(axisY.y) < 0.95f
            ? Vector3(0.0f, 1.0f, 0.0f)
            : Vector3(1.0f, 0.0f, 0.0f);
        Vector3 axisX = helper.Cross(axisY);
        axisX.Normalize();
        Vector3 axisZ = axisY.Cross(axisX);
        axisZ.Normalize();

        BoundingBoxOBB result{};
        result.worldcenter = (start + end) * 0.5f;
        result.center = Vector3(0.0f, 0.0f, 0.0f);
        result.axisX = axisX;
        result.axisY = axisY;
        result.axisZ = axisZ;
        result.lengthx = thickness;
        // length is the exact hilt-to-tip span. Adding thickness here made the
        // OBB extend past both endpoints, so the debug box no longer matched
        // the visible sword even when the segment direction was correct.
        result.lengthy = std::max(length, 0.001f);
        result.lengthz = thickness;
        return result;
    }

    bool OverlapsAabb(const BoundingBoxAABB& a, const BoundingBoxAABB& b)
    {
        return a.min.x <= b.max.x && a.max.x >= b.min.x &&
            a.min.y <= b.max.y && a.max.y >= b.min.y &&
            a.min.z <= b.max.z && a.max.z >= b.min.z;
    }

    bool SeparatedOnAxis(
        const BoundingBoxOBB& a,
        const BoundingBoxOBB& b,
        Vector3 axis)
    {
        const float lengthSquared = axis.LengthSquared();
        if (lengthSquared <= 0.000001f)
            return false;
        axis /= std::sqrt(lengthSquared);

        const std::array<Vector3, 3> axesA = { a.axisX, a.axisY, a.axisZ };
        const std::array<Vector3, 3> axesB = { b.axisX, b.axisY, b.axisZ };
        const std::array<float, 3> halfA = {
            a.lengthx * 0.5f, a.lengthy * 0.5f, a.lengthz * 0.5f };
        const std::array<float, 3> halfB = {
            b.lengthx * 0.5f, b.lengthy * 0.5f, b.lengthz * 0.5f };

        float radiusA = 0.0f;
        float radiusB = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            radiusA += halfA[i] * std::abs(axesA[i].Dot(axis));
            radiusB += halfB[i] * std::abs(axesB[i].Dot(axis));
        }
        const float centerDistance = std::abs((b.worldcenter - a.worldcenter).Dot(axis));
        return centerDistance > radiusA + radiusB + 0.0001f;
    }

    bool OverlapsObb(const BoundingBoxOBB& a, const BoundingBoxOBB& b)
    {
        const std::array<Vector3, 3> axesA = { a.axisX, a.axisY, a.axisZ };
        const std::array<Vector3, 3> axesB = { b.axisX, b.axisY, b.axisZ };
        for (const Vector3& axis : axesA)
            if (SeparatedOnAxis(a, b, axis)) return false;
        for (const Vector3& axis : axesB)
            if (SeparatedOnAxis(a, b, axis)) return false;
        for (const Vector3& axisA : axesA)
            for (const Vector3& axisB : axesB)
                if (SeparatedOnAxis(a, b, axisA.Cross(axisB))) return false;
        return true;
    }

    OneVsOneCombat::CollisionDebugState EvaluateSwordCollision(
        const Vector3& swordBase,
        const Vector3& swordTip,
        const Vector3& previousSwordTip,
        bool swordTransformValid,
        const BoundingBoxOBB& playerObb,
        const BoundingBoxOBB& enemyObb)
    {
        OneVsOneCombat::CollisionDebugState result{};
        result.swordTransformValid = swordTransformValid;
        result.playerObb = playerObb;
        result.enemyObb = enemyObb;
        result.playerBroadPhase = GM31::GE::Collision::BuildWorldAABBFromOBB(playerObb);
        result.enemyBroadPhase = GM31::GE::Collision::BuildWorldAABBFromOBB(enemyObb);
        if (!swordTransformValid)
            return result;

        result.bladeObb = MakeSegmentObb(swordBase, swordTip, 1.4f);
        result.tipSweepObb = MakeSegmentObb(previousSwordTip, swordTip, 2.2f);
        result.attackBroadPhase = MakePointCloudAabb(
            swordBase, swordTip, previousSwordTip, 1.1f);
        result.broadPhaseOverlap = OverlapsAabb(
            result.attackBroadPhase, result.enemyBroadPhase);
        if (!result.broadPhaseOverlap)
            return result;

        result.narrowPhaseTested = true;
        result.narrowPhaseHit = OverlapsObb(result.bladeObb, result.enemyObb) ||
            OverlapsObb(result.tipSweepObb, result.enemyObb);
        return result;
    }
}

void OneVsOneCombat::Reset()
{
    m_playerHp = MAX_HP;
    m_enemyHp = MAX_HP;
    m_enemyCooldown = 0.7f;
    m_playerAttack = {};
    m_enemyAttack = {};
    m_collisionDebug = {};
	m_playerHitGrace = 0.0f;
}

void OneVsOneCombat::Update(
    uint64_t deltaMicroseconds,
    const Vector3& playerPosition,
    const Vector3& enemyPosition,
    bool playerAttackTriggered,
	bool enemyAttackTriggered,
    const Vector3& swordBase,
    const Vector3& swordTip,
    const Vector3& previousSwordTip,
    bool swordTransformValid,
    const BoundingBoxOBB& playerObb,
    const BoundingBoxOBB& enemyObb)
{
    const float deltaSeconds = std::min(
        static_cast<float>(deltaMicroseconds) / 1000000.0f,
        0.1f);
    const float distance = DistanceXZ(playerPosition, enemyPosition);

    m_collisionDebug = EvaluateSwordCollision(
        swordBase,
        swordTip,
        previousSwordTip,
        swordTransformValid,
        playerObb,
        enemyObb);
	m_playerHitGrace = std::max(0.0f, m_playerHitGrace - deltaSeconds);
	if ((m_playerAttack.phase == Phase::Windup ||
		m_playerAttack.phase == Phase::Active) &&
		m_collisionDebug.narrowPhaseHit)
	{
		// Preserve a very short contact that occurs at a phase boundary. Without
		// this, a 60 Hz frame can see the blade intersect during windup and miss
		// it one frame later when the damaging phase begins.
		m_playerHitGrace = 0.12f;
	}

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
        if (playerAttackTriggered && m_playerAttack.phase == Phase::Ready)
            m_playerAttack = { Phase::Windup, 0.0f, false };

		if (enemyAttackTriggered && m_enemyAttack.phase == Phase::Ready)
        {
            m_enemyAttack = { Phase::Windup, 0.0f, false };
            m_enemyCooldown = ENEMY_COOLDOWN;
        }

        m_playerAttack.elapsed += deltaSeconds;
        if (m_playerAttack.phase == Phase::Windup &&
            m_playerAttack.elapsed >= PLAYER_WINDUP)
        {
            m_playerAttack.phase = Phase::Active;
            m_playerAttack.elapsed = 0.0f;
        }
        else if (m_playerAttack.phase == Phase::Active)
        {
			if (!m_playerAttack.hit &&
				(m_collisionDebug.narrowPhaseHit || m_playerHitGrace > 0.0f))
			{
				m_enemyHp = std::max(0.0f, m_enemyHp - PLAYER_DAMAGE);
				m_playerAttack.hit = true;
				m_playerHitGrace = 0.0f;
            }
            if (m_playerAttack.elapsed >= PLAYER_ACTIVE)
            {
                m_playerAttack.phase = Phase::Recovery;
                m_playerAttack.elapsed = 0.0f;
            }
        }
        else if (m_playerAttack.phase == Phase::Recovery &&
            m_playerAttack.elapsed >= PLAYER_RECOVERY)
        {
            m_playerAttack = {};
        }

        m_enemyAttack.elapsed += deltaSeconds;
        if (m_enemyAttack.phase == Phase::Windup &&
            m_enemyAttack.elapsed >= ENEMY_WINDUP)
        {
            m_enemyAttack.phase = Phase::Active;
            m_enemyAttack.elapsed = 0.0f;
        }
        else if (m_enemyAttack.phase == Phase::Active)
        {
			// Only the initial head slam deals damage. The later forward lunge is
			// movement/animation only and must not create a second hit.
			static constexpr float ENEMY_HIT_TIMES[ENEMY_MAX_HITS] = { 0.18f };
			if (m_enemyAttack.hitCount < ENEMY_MAX_HITS &&
				m_enemyAttack.elapsed >= ENEMY_HIT_TIMES[m_enemyAttack.hitCount])
			{
				if (distance <= ENEMY_ATTACK_RANGE)
					m_playerHp = std::max(0.0f, m_playerHp - ENEMY_DAMAGE);
				++m_enemyAttack.hitCount;
			}
            if (m_enemyAttack.elapsed >= ENEMY_ACTIVE)
            {
                m_enemyAttack.phase = Phase::Recovery;
                m_enemyAttack.elapsed = 0.0f;
            }
        }
        else if (m_enemyAttack.phase == Phase::Recovery &&
            m_enemyAttack.elapsed >= ENEMY_RECOVERY)
        {
            m_enemyAttack = {};
        }

        if (IsEnemyDefeated())
            m_enemyAttack = { Phase::Defeated, 0.0f, false };
        if (IsPlayerDefeated())
            m_playerAttack = { Phase::Defeated, 0.0f, false };
    }
}

std::string_view OneVsOneCombat::GetStateName() const
{
    if (IsEnemyDefeated()) return "ENEMY DOWN - YOU WIN";
    if (IsPlayerDefeated()) return "PLAYER DOWN - TRY AGAIN";
    if (m_playerAttack.phase == Phase::Active) return "PLAYER ATTACK";
    if (m_enemyAttack.phase == Phase::Active) return "ENEMY ATTACK";
    if (m_playerAttack.phase == Phase::Windup ||
        m_playerAttack.phase == Phase::Recovery) return "PLAYER RECOVERY";
    if (m_enemyAttack.phase == Phase::Windup ||
        m_enemyAttack.phase == Phase::Recovery) return "ENEMY RECOVERY";
    return "READY";
}
