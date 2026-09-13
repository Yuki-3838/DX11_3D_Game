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

enemy::enemy(IScene* scene)
	: gameobject(scene)
{
}

void enemy::setTarget(player* target)
{
	m_target = target;
}

Vector3 enemy::getVel() const
{
	return m_move;
}

void enemy::setVel(const Vector3& vel)
{
	m_move = vel;
}

enemy::MotionState enemy::getMotionState() const
{
	return m_motionState;
}

void enemy::resetEncounter()
{
	m_motionState = MotionState::Approach;
	m_stateTime = 0.0f;
	m_move = Vector3(0.0f, 0.0f, 0.0f);
	m_circleDirection = 1.0f;
}

bool enemy::isInRecovery() const
{
	return m_motionState == MotionState::Recovery;
}

float enemy::getStateTime() const
{
	return m_stateTime;
}

Combat::EnemyAttackKind enemy::getAttackKind() const
{
	return m_attackKind;
}

const Combat::AttackData& enemy::getAttackData() const
{
	return Combat::EnemyAttackOf(m_attackKind);
}

float enemy::windupSeconds() const
{
	return getAttackData().frames.anticipationSeconds;
}

float enemy::activeSeconds() const
{
	return getAttackData().frames.activeSeconds;
}

float enemy::recoverySeconds() const
{
	return getAttackData().frames.recoverySeconds;
}

void enemy::setForcedAttackKind(const Combat::EnemyAttackKind* kind)
{
	m_forceAttackKind = kind != nullptr;
	if (kind != nullptr)
		m_forcedAttackKind = *kind;
}

void enemy::selectNextAttack(float distance)
{
	// 調整中は指定された攻撃だけを出す。射程の条件も無視する。
	if (m_forceAttackKind)
	{
		m_previousAttackKind = m_attackKind;
		m_attackKind = m_forcedAttackKind;
		return;
	}

	// 距離で候補を絞り、そのうえで直前と同じ攻撃が続かないようにする。
	// 完全なランダムだと同じ攻撃が連続して「読む意味」が薄れ、
	// 逆に完全な順番固定だと暗記ゲームになるため、その中間を取る。
	const bool inBiteRange = distance <= Combat::Tuning::ENEMY_BITE_HIT_RANGE;

	Combat::EnemyAttackKind candidates[3];
	int candidateCount = 0;
	candidates[candidateCount++] = Combat::EnemyAttackKind::Slam;
	candidates[candidateCount++] = Combat::EnemyAttackKind::Sweep;
	// 噛みつきは射程が短いので、近いときだけ選択肢に入れる。
	// 遠くから出しても当たらず、プレイヤーが予兆を読む意味が無くなるためである。
	if (inBiteRange)
		candidates[candidateCount++] = Combat::EnemyAttackKind::Bite;

	// 単純な巡回に位置ずらしを加えることで、外部の乱数生成器に依存せず
	// 「毎回同じ順番」にもならないようにする。
	m_attackSelectCounter = (m_attackSelectCounter + 1) % 7;
	int index = (m_attackSelectCounter + (m_attackSelectCounter / 3)) % candidateCount;
	if (candidates[index] == m_previousAttackKind && candidateCount > 1)
		index = (index + 1) % candidateCount;

	m_previousAttackKind = m_attackKind;
	m_attackKind = candidates[index];
}

SRT enemy::getRenderSRT() const
{
	SRT renderSrt = m_srt;
	// ゲーム上の向きは正しいが、モデルの正面が反転して見えるため、描画時だけ向きを補正する。
	// ドラゴンのモデルは傾いた姿勢で作られているので、描画姿勢だけピッチを加えて頭部と胴体を正しく見せる。
	renderSrt.rot.x += PI * 0.5f;
	renderSrt.pos.y += m_visualGroundOffsetY;

	// 攻撃の種類ごとに違う構えを、描画姿勢にだけ足す。
	// 敵の攻撃クリップは1本しか無いため、これをしないと3種類の攻撃が
	// 再生速度違いにしか見えず、プレイヤーはHUDの文字でしか区別できない。
	// 物理SRTは変更しない(壁との衝突・接地・敵AIの判断に影響させないため)。
	// なお m_visualGroundOffsetY はGameScene側でこの傾きを織り込んで計算されている。
	// 傾けると足元の最下点が変わるため、織り込まないと攻撃のたびに地面へ埋まる。
	const Combat::EnemyPoseOffset pose = getAttackPoseOffset();
	renderSrt.rot.x += pose.pitch;
	renderSrt.rot.y += pose.yaw;
	return renderSrt;
}

void enemy::setVisualGroundOffsetY(float offsetY)
{
	m_visualGroundOffsetY = offsetY;
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
	{
		// 大型の敵は衝突分離によって希望距離の少し外側で止まることがあるため、
		// 敵の中心が希望距離へ完全に入ることは遷移条件にしない。
		selectNextAttack(distance);
		changeState(MotionState::Windup);
	}
	else if (m_motionState == MotionState::Circle &&
		m_stateTime >= MIN_CIRCLE_SECONDS &&
		(distance <= ATTACK_DISTANCE ||
		 (m_stateTime >= MAX_CIRCLE_SECONDS && distance <= ATTACK_DISTANCE + 32.0f)))
	{
		selectNextAttack(distance);
		changeState(MotionState::Windup);
	}

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
		// 周回しながら、プレイヤーが離れた場合は距離方向にも近づける。
		// 接近量に小さすぎる上限を設けると、後退後に攻撃距離へ戻れず
		// 敵が延々と周回し続けるためである。
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
		if (m_stateTime >= windupSeconds()) changeState(MotionState::Active);
		break;
	case MotionState::Active:
		// 攻撃判定中は短く踏み込ませ、攻撃の有効時間を動きでも分かるようにする。
		// 薙ぎ払いは射程が広い分だけ踏み込みも大きくし、
		// 「距離を取るだけでは避けられない」ことが動きから読めるようにする。
		moveInFacingDirection(
			(m_attackKind == Combat::EnemyAttackKind::Sweep ? 68.0f : 48.0f) * deltaSec);
		if (m_stateTime >= activeSeconds()) changeState(MotionState::Recovery);
		break;
	case MotionState::Recovery:
		if (m_stateTime >= recoverySeconds()) changeState(MotionState::Retreat);
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
}

Combat::EnemyPoseOffset enemy::getAttackPoseOffset() const
{
	return Combat::EnemyAttackPose(m_attackKind, currentAttackPhase(), m_stateTime);
}

Combat::EnemyAttackPhase enemy::currentAttackPhase() const
{
	switch (m_motionState)
	{
	case MotionState::Windup:   return Combat::EnemyAttackPhase::Windup;
	case MotionState::Active:   return Combat::EnemyAttackPhase::Active;
	case MotionState::Recovery: return Combat::EnemyAttackPhase::Recovery;
	default:                    return Combat::EnemyAttackPhase::None;
	}
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
	case MotionState::Approach: return "接近";
	case MotionState::Circle: return "様子見(周回)";
	case MotionState::Windup: return "予兆";
	case MotionState::Active: return "攻撃判定中";
	case MotionState::Recovery: return "隙";
	case MotionState::Retreat: return "後退";
	default: return "不明";
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
