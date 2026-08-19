#include <cstdint>
#include <cmath>
#include <algorithm>
#include "enemy.h"
#include "player.h"
#include "../system/transform.h"

namespace
{
	float WrapAngle(float angle)
	{
		while (angle > PI) angle -= PI * 2.0f;
		while (angle < -PI) angle += PI * 2.0f;
		return angle;
	}
}

void enemy::init()
{
	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
	m_move = Vector3(0, 0, 0);
	m_motionState = MotionState::Approach;
	m_stateTime = 0.0f;
	m_circleDirection = 1.0f;
}

void enemy::update(uint64_t dt)
{
	if (m_target == nullptr) return;

	const float deltaSec = std::clamp(static_cast<float>(dt) * 0.000001f, 0.0f, MAX_DELTA_SECONDS);
	const Vector3 targetPosition = m_target->getSRT().pos;
	const float distance = distanceToTarget(targetPosition);
	m_stateTime += deltaSec;
	m_move = Vector3(0, 0, 0);

	if (m_motionState == MotionState::Approach && distance <= ATTACK_DISTANCE)
		// Do not require the body center to enter the preferred orbit radius.
		// Collision separation can stop a large monster just outside that radius.
		changeState(MotionState::Windup);
	else if (m_motionState == MotionState::Circle &&
		m_stateTime >= MIN_CIRCLE_SECONDS &&
		(distance <= ATTACK_DISTANCE ||
		 (m_stateTime >= MAX_CIRCLE_SECONDS && distance <= ATTACK_DISTANCE + 32.0f)))
		changeState(MotionState::Windup);

	switch (m_motionState)
	{
	case MotionState::Approach:
		faceTarget(targetPosition, deltaSec, 8.0f);
		moveInFacingDirection(APPROACH_SPEED * deltaSec);
		break;
	case MotionState::Circle:
	{
		faceTarget(targetPosition, deltaSec, 7.0f);
		const float distanceError = distance - PREFERRED_DISTANCE;
		// Keep orbiting, but actually close the gap when the player escapes.
		// The previous 0.65-unit cap made the radial approach negligible after
		// the retreat, so the enemy could circle forever outside attack range.
		const float forwardSpeed = std::clamp(
			distanceError * 3.0f,
			-CIRCLE_SPEED * 0.75f,
			CIRCLE_SPEED * 0.75f);
		const float side = m_circleDirection * CIRCLE_SPEED * deltaSec;
		const Vector3 forwardDir(-std::sinf(m_srt.rot.y), 0.0f, -std::cosf(m_srt.rot.y));
		const Vector3 sideDir(-forwardDir.z, 0.0f, forwardDir.x);
		m_move = forwardDir * (forwardSpeed * deltaSec) + sideDir * side;
		break;
	}
	case MotionState::Windup:
		faceTarget(targetPosition, deltaSec, 3.5f);
		if (m_stateTime >= WINDUP_SECONDS) changeState(MotionState::Active);
		break;
	case MotionState::Active:
		// A short committed lunge makes the active hit window readable.
		moveInFacingDirection(48.0f * deltaSec);
		if (m_stateTime >= ACTIVE_SECONDS) changeState(MotionState::Recovery);
		break;
	case MotionState::Recovery:
		if (m_stateTime >= RECOVERY_SECONDS) changeState(MotionState::Retreat);
		break;
	case MotionState::Retreat:
		faceTarget(targetPosition, deltaSec, 6.0f);
		moveInFacingDirection(-RETREAT_SPEED * deltaSec);
		if (m_stateTime >= RETREAT_SECONDS || distance >= RETREAT_DISTANCE)
		{
			m_circleDirection = -m_circleDirection;
			changeState(MotionState::Circle);
		}
		break;
	}

	// Scene側はupdate前後の座標差分を使って壁・他エネミーとの
	// 衝突補正を行うため、ここでAIが決めた移動を仮適用する。
	m_srt.pos += m_move;
	m_srt.pos += m_move;
}

void enemy::changeState(MotionState nextState)
{
	m_motionState = nextState;
	m_stateTime = 0.0f;
}

float enemy::distanceToTarget(const Vector3& targetPosition) const
{
	const float x = targetPosition.x - m_srt.pos.x;
	const float z = targetPosition.z - m_srt.pos.z;
	return std::sqrt(x * x + z * z);
}

float enemy::angleToTarget(const Vector3& targetPosition) const
{
	return std::atan2(-(targetPosition.x - m_srt.pos.x), -(targetPosition.z - m_srt.pos.z));
}

void enemy::faceTarget(const Vector3& targetPosition, float deltaSec, float turnRate)
{
	m_destrot.y = angleToTarget(targetPosition);
	const float diffrot = WrapAngle(m_destrot.y - m_srt.rot.y);
	m_srt.rot.y = WrapAngle(m_srt.rot.y + diffrot * std::min(1.0f, turnRate * deltaSec));
}

void enemy::moveInFacingDirection(float distance)
{
	m_move = Vector3(-std::sinf(m_srt.rot.y), 0.0f, -std::cosf(m_srt.rot.y)) * distance;
}

const char* enemy::getMotionStateName() const
{
	switch (m_motionState)
	{
	case MotionState::Approach: return "Approach";
	case MotionState::Circle: return "Circle";
	case MotionState::Windup: return "Windup";
	case MotionState::Active: return "Active";
	case MotionState::Recovery: return "Recovery";
	case MotionState::Retreat: return "Retreat";
	default: return "Unknown";
	}
}

void enemy::draw(uint64_t dt)
{
	(void)dt;
	Matrix4x4 tmtx = Matrix4x4::CreateTranslation(m_srt.pos.x, m_srt.pos.y, 0);
	Matrix4x4 rmtx = Matrix4x4::CreateRotationZ(m_srt.rot.z);
	Matrix4x4 pivotmtx1 = Matrix4x4::CreateTranslation(-m_srt.pivot.x, -m_srt.pivot.y, 0);
	Matrix4x4 pivotmtx2 = Matrix4x4::CreateTranslation(m_srt.pivot.x, m_srt.pivot.y, 0);
	Matrix4x4 smtx = Matrix4x4::CreateScale(m_srt.scale.x, m_srt.scale.y, m_srt.scale.z);
	Matrix4x4 mtx = smtx * pivotmtx1 * rmtx * pivotmtx2 * tmtx;
	(void)mtx;
}

void enemy::dispose()
{
}
