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
        // 長さは柄から剣先までの実際の距離にする。
        // ここへ厚みを足すとOBBが両端からはみ出し、方向が正しくても
        // デバッグ表示と見た目の剣が一致しなくなる。
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
    m_playerHp = PLAYER_MAX_HP;
    m_enemyHp = ENEMY_MAX_HP;
    m_enemyCooldown = 0.7f;
    m_enemyAttackKind = Combat::EnemyAttackKind::Slam;
    m_playerAttack = {};
    m_enemyAttack = {};
    m_collisionDebug = {};
	m_playerHitGrace = 0.0f;
}

void OneVsOneCombat::ClearEnemyCollisionDebug()
{
	m_collisionDebug.broadPhaseOverlap = false;
	m_collisionDebug.narrowPhaseTested = false;
	m_collisionDebug.narrowPhaseHit = false;
	m_collisionDebug.enemyBroadPhase = {};
	m_collisionDebug.enemyObb = {};
}

void OneVsOneCombat::UpdatePlayerCollisionDebug(
	const Vector3& swordBase,
	const Vector3& swordTip,
	const Vector3& previousSwordTip,
	bool swordTransformValid,
	const BoundingBoxOBB& playerObb)
{
	ClearEnemyCollisionDebug();
	m_collisionDebug.swordTransformValid = swordTransformValid;
	m_collisionDebug.playerObb = playerObb;
	m_collisionDebug.playerBroadPhase =
		GM31::GE::Collision::BuildWorldAABBFromOBB(playerObb);
	m_collisionDebug.attackBroadPhase = {};
	m_collisionDebug.bladeObb = {};
	m_collisionDebug.tipSweepObb = {};
	if (!swordTransformValid)
		return;

	m_collisionDebug.bladeObb = MakeSegmentObb(swordBase, swordTip, 1.4f);
	m_collisionDebug.tipSweepObb = MakeSegmentObb(
		previousSwordTip, swordTip, 2.2f);
	m_collisionDebug.attackBroadPhase = MakePointCloudAabb(
		swordBase, swordTip, previousSwordTip, 1.1f);
}

bool OneVsOneCombat::CanCancelPlayerAttack(int cancelEndFrame) const
{
	if (!IsPlayerAttacking() || IsPlayerDefeated() || IsEnemyDefeated())
		return false;
	return GetPlayerAttackFrame() <= cancelEndFrame;
}

void OneVsOneCombat::CancelPlayerAttack()
{
	if (IsPlayerAttacking())
		m_playerAttack = {};
}

void OneVsOneCombat::CancelEnemyAttack()
{
	if (IsEnemyAttacking())
		m_enemyAttack = {};
}

int OneVsOneCombat::GetPlayerAttackFrame() const
{
	return static_cast<int>(m_playerAttack.totalElapsed * 60.0f);
}

void OneVsOneCombat::Update(
    uint64_t deltaMicroseconds,
    const Vector3& playerPosition,
    const Vector3& enemyPosition,
	bool playerAttackTriggered,
	bool playerHeavyAttackTriggered,
	bool enemyAttackTriggered,
	bool playerInvincible,
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
		// フェーズ境界で発生した短い接触を少しだけ保持する。
		// 60Hz更新では、振りかぶり中に剣が交差して次のダメージ開始フレームで
		// すでに離れていることがあるためである。
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
		if ((playerAttackTriggered || playerHeavyAttackTriggered) && m_playerAttack.phase == Phase::Ready)
		{
			m_playerAttack = {};
			m_playerAttack.phase = Phase::Windup;
			m_playerAttack.kind = playerHeavyAttackTriggered ? AttackKind::Heavy : AttackKind::Normal;
			m_playerAttack.comboStep = 1;
		}
		else if ((playerAttackTriggered || playerHeavyAttackTriggered) &&
			m_playerAttack.phase != Phase::Ready &&
			m_playerAttack.phase != Phase::Defeated)
		{
			const AttackKind requestedKind = playerHeavyAttackTriggered ? AttackKind::Heavy : AttackKind::Normal;
			const bool sameKind = requestedKind == m_playerAttack.kind;
			// 現在の攻撃中に次の入力を受け付ける。
			// 有効時間や硬直の境界直前に押してもコンボが途切れないようにする。
			const bool canBuffer = m_playerAttack.phase == Phase::Windup ||
				m_playerAttack.phase == Phase::Active ||
				m_playerAttack.phase == Phase::Recovery;
			if (sameKind && canBuffer && m_playerAttack.comboStep < 3)
				m_playerAttack.comboQueued = std::min(2, m_playerAttack.comboQueued + 1);
		}

		// 敵の攻撃の時間・威力・射程は、敵AIが選んだ攻撃データから引く。
		// これにより攻撃の種類ごとに「予兆の長さ」「隙の大きさ」が変わり、
		// プレイヤーは予兆を見て回避するか踏み込むかを選べる。
		const Combat::AttackData& enemyAttack = Combat::EnemyAttackOf(m_enemyAttackKind);
		const float enemyWindup = enemyAttack.frames.anticipationSeconds;
		const float enemyActive = enemyAttack.frames.activeSeconds;
		const float enemyRecovery = enemyAttack.frames.recoverySeconds;

		if (enemyAttackTriggered && m_enemyAttack.phase == Phase::Ready)
        {
            m_enemyAttack = { Phase::Windup, 0.0f, false };
            m_enemyCooldown = enemyAttack.frames.cooldownSeconds;
        }

		m_playerAttack.elapsed += deltaSeconds;
		m_playerAttack.totalElapsed += deltaSeconds;
		const float playerWindup = m_playerAttack.kind == AttackKind::Heavy ? HEAVY_WINDUP : PLAYER_WINDUP;
		const float playerActive = m_playerAttack.kind == AttackKind::Heavy ? HEAVY_ACTIVE : PLAYER_ACTIVE;
		const float playerRecovery = m_playerAttack.kind == AttackKind::Heavy ? HEAVY_RECOVERY : PLAYER_RECOVERY;
		if (m_playerAttack.phase == Phase::Windup &&
		    m_playerAttack.elapsed >= playerWindup)
        {
            m_playerAttack.phase = Phase::Active;
            m_playerAttack.elapsed = 0.0f;
        }
        else if (m_playerAttack.phase == Phase::Active)
        {
			if (!m_playerAttack.hit &&
				(m_collisionDebug.narrowPhaseHit || m_playerHitGrace > 0.0f))
			{
				const float damage = m_playerAttack.kind == AttackKind::Heavy ? HEAVY_DAMAGE : PLAYER_DAMAGE;
				m_enemyHp = std::max(0.0f, m_enemyHp - damage);
				m_playerAttack.hit = true;
				m_playerHitGrace = 0.0f;
            }
			if (m_playerAttack.elapsed >= playerActive)
            {
                m_playerAttack.phase = Phase::Recovery;
                m_playerAttack.elapsed = 0.0f;
            }
        }
		else if (m_playerAttack.phase == Phase::Recovery &&
		    m_playerAttack.elapsed >= playerRecovery)
        {
			if (m_playerAttack.comboQueued && m_playerAttack.comboStep < 3)
			{
				const AttackKind kind = m_playerAttack.kind;
				const int nextStep = m_playerAttack.comboStep + 1;
				const int queuedInputs = m_playerAttack.comboQueued;
				m_playerAttack = {};
				m_playerAttack.phase = Phase::Windup;
				m_playerAttack.kind = kind;
				m_playerAttack.comboStep = nextStep;
				m_playerAttack.comboQueued = std::max(0, queuedInputs - 1);
			}
			else
				m_playerAttack = {};
        }

        m_enemyAttack.elapsed += deltaSeconds;
        if (m_enemyAttack.phase == Phase::Windup &&
            m_enemyAttack.elapsed >= enemyWindup)
        {
            m_enemyAttack.phase = Phase::Active;
            m_enemyAttack.elapsed = 0.0f;
        }
        else if (m_enemyAttack.phase == Phase::Active)
        {
			// ダメージを与えるのは最初の一撃だけにする。
			// その後の踏み込みは移動・アニメーション専用で、二重にヒットさせない。
			const float firstHitTime = Combat::EnemyFirstHitTimeOf(m_enemyAttackKind);
			if (m_enemyAttack.hitCount < ENEMY_MAX_HITS &&
				m_enemyAttack.elapsed >= firstHitTime)
			{
				// 射程は攻撃の種類ごとに違う。噛みつきは近距離だけ、薙ぎ払いは広い。
				const float hitRange = enemyAttack.broadPhaseFilter.maxDistance;
				// 左右の広さも攻撃ごとに変える。以前は距離だけで判定していたため、
				// 真後ろにいても噛みつきが当たっていた。それでは
				// 「予兆の形を見て回り込む」という選択が成立しない。
				// 敵の正面(enemy.cppと同じ -sin/-cos の向き)との角度差で絞る。
				const Vector3 enemyForward(
					-std::sin(m_enemyFacingYaw), 0.0f, -std::cos(m_enemyFacingYaw));
				const float toPlayerX = playerPosition.x - enemyPosition.x;
				const float toPlayerZ = playerPosition.z - enemyPosition.z;
				const float toPlayerLength =
					std::sqrt(toPlayerX * toPlayerX + toPlayerZ * toPlayerZ);
				// 真上から重なっているときは向きを決められないので、当たり扱いにする。
				float angleToPlayer = 0.0f;
				if (toPlayerLength > 0.0001f)
				{
					const float cosAngle = std::clamp(
						(enemyForward.x * toPlayerX + enemyForward.z * toPlayerZ) /
							toPlayerLength,
						-1.0f, 1.0f);
					angleToPlayer = std::acos(cosAngle);
				}
				const bool inFrontArc =
					angleToPlayer <= Combat::EnemyHitHalfAngleOf(m_enemyAttackKind);
				if (distance <= hitRange && inFrontArc && !playerInvincible)
				{
					m_playerHp = std::max(
						0.0f,
						m_playerHp - static_cast<float>(enemyAttack.damage));
				}
				++m_enemyAttack.hitCount;
			}
            if (m_enemyAttack.elapsed >= enemyActive)
            {
                m_enemyAttack.phase = Phase::Recovery;
                m_enemyAttack.elapsed = 0.0f;
            }
        }
        else if (m_enemyAttack.phase == Phase::Recovery &&
            m_enemyAttack.elapsed >= enemyRecovery)
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
    if (IsEnemyDefeated()) return "敵を撃破";
    if (IsPlayerDefeated()) return "プレイヤー戦闘不能";
    if (m_playerAttack.phase == Phase::Active) return "プレイヤーの攻撃判定中";
    if (m_enemyAttack.phase == Phase::Active) return "敵の攻撃判定中";
    if (m_playerAttack.phase == Phase::Windup ||
        m_playerAttack.phase == Phase::Recovery) return "プレイヤーの予兆・硬直";
    if (m_enemyAttack.phase == Phase::Windup ||
        m_enemyAttack.phase == Phase::Recovery) return "敵の予兆・硬直";
    return "待機";
}
