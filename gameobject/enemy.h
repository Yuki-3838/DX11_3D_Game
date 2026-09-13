#pragma once

#include <cstdint>
#include "gameobject.h"
#include "../system/CombatAttackTable.h"
#include "../system/EnemyAttackPose.h"

class player;

class enemy : public gameobject
{
public:
	// 攻撃前に止まり、攻撃後に長く止まることで、プレイヤーが差し込める隙を作る。
	enum class MotionState { Approach, Circle, Windup, Active, Recovery, Retreat };

	explicit enemy(IScene* scene);

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setTarget(player* target);
	Vector3 getVel() const;
	void setVel(const Vector3& vel);

	MotionState getMotionState() const;
	void resetEncounter();
	const char* getMotionStateName() const;
	bool isInRecovery() const;
	float getStateTime() const;
	SRT getRenderSRT() const;
	void setVisualGroundOffsetY(float offsetY);

	// 現在選択中の攻撃。戦闘判定(OneVsOneCombat)とHUDが同じものを参照することで、
	// 「見えている予兆」と「実際に来る攻撃」が必ず一致する。
	Combat::EnemyAttackKind getAttackKind() const;
	const Combat::AttackData& getAttackData() const;

	// 演出で加えている体の傾き・ひねり・高さ。
	// 傾けると足元の最下点が変わるため、接地オフセットを計算する側が必要とする。
	Combat::EnemyPoseOffset getAttackPoseOffset() const;

	// 調整用。攻撃の種類を固定する(デバッグ表示からのみ使う)。
	// 3種類の構えを見比べるには同じ攻撃を繰り返し出させる必要があるが、
	// 通常の選択は距離と直前の攻撃で変わるため、狙った攻撃が出るまで待つことになる。
	void setForcedAttackKind(const Combat::EnemyAttackKind* kind);

private:
	void changeState(MotionState nextState);
	// 描画用の構え(EnemyAttackPose.h)へ渡すため、AIの状態を攻撃の段階へ変換する。
	Combat::EnemyAttackPhase currentAttackPhase() const;
	void selectNextAttack(float distance);
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
	// 攻撃の時間は system/CombatAttackTable.h が定義元。
	// 攻撃の種類ごとに長さが変わるため、固定値ではなく
	// 選択中の攻撃データ(m_attackKind)から都度取得する。
	// 戦闘判定(OneVsOneCombat)も同じデータを参照するため、
	// 見た目の予備動作と実際の攻撃判定が必ず一致する。
	float windupSeconds() const;
	float activeSeconds() const;
	float recoverySeconds() const;
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
	Combat::EnemyAttackKind m_attackKind = Combat::EnemyAttackKind::Slam;
	// 同じ攻撃が続けて出ると読み合いにならないため、直前に使った攻撃を覚えておく。
	Combat::EnemyAttackKind m_previousAttackKind = Combat::EnemyAttackKind::Slam;
	int m_attackSelectCounter = 0;
	bool m_forceAttackKind = false;
	Combat::EnemyAttackKind m_forcedAttackKind = Combat::EnemyAttackKind::Slam;
};
