#pragma once

#include <cstdint>
#include "gameobject.h"

class player;

class enemy : public gameobject
{
public:
	// 攻撃前に止まり、攻撃後に長く止まることで、プレイヤーが差し込める隙を作る。
	enum class MotionState { Approach, Circle, Windup, Active, Recovery, Retreat };

	enemy(IScene* scene) : gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setTarget(player* target) { m_target = target; }
	Vector3 getVel() const { return m_move; }
	void setVel(const Vector3& vel) { m_move = vel; }

	MotionState getMotionState() const { return m_motionState; }
	const char* getMotionStateName() const;
	bool isInRecovery() const { return m_motionState == MotionState::Recovery; }
	float getStateTime() const { return m_stateTime; }
	SRT getRenderSRT() const
	{
		SRT renderSrt = m_srt;
		// The gameplay heading is correct, but the asset presents its back
		// The imported dragon is authored in a steep pose; pitch the visual pose
		// so its head/body read correctly from the third-person camera.
		renderSrt.rot.x += PI * 0.5f;
		renderSrt.pos.y += m_visualGroundOffsetY;
		return renderSrt;
	}
	void setVisualGroundOffsetY(float offsetY) { m_visualGroundOffsetY = offsetY; }

private:
	void changeState(MotionState nextState);
	float distanceToTarget(const Vector3& targetPosition) const;
	float angleToTarget(const Vector3& targetPosition) const;
	void faceTarget(const Vector3& targetPosition, float deltaSec, float turnRate);
	void moveInFacingDirection(float distance);

	static constexpr float PREFERRED_DISTANCE = 26.0f;
	static constexpr float ATTACK_DISTANCE = 32.0f;
	static constexpr float RETREAT_DISTANCE = 38.0f;
	static constexpr float APPROACH_SPEED = 84.0f;
	static constexpr float CIRCLE_SPEED = 56.0f;
	static constexpr float RETREAT_SPEED = 98.0f;
	static constexpr float WINDUP_SECONDS = 0.80f;
	static constexpr float ACTIVE_SECONDS = 0.90f;
	static constexpr float RECOVERY_SECONDS = 1.35f;
	static constexpr float RETREAT_SECONDS = 0.22f;
	static constexpr float MIN_CIRCLE_SECONDS = 0.55f;
	static constexpr float MAX_CIRCLE_SECONDS = 1.40f;
	static constexpr float MAX_DELTA_SECONDS = 0.1f;

	Vector3 m_move{0, 0, 0};
	Vector3 m_destrot{0, 0, 0};
	player* m_target{nullptr};
	MotionState m_motionState = MotionState::Approach;
	float m_stateTime = 0.0f;
	float m_circleDirection = 1.0f;
	float m_visualGroundOffsetY = 0.0f;
};
