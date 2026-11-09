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
	m_posture = 0.0f;
	m_rageDamage = 0.0f;
	m_rageSeconds = 0.0f;
	m_rageCount = 0;
	m_flinchCount = 0;
	m_condition = Combat::EnemyCondition::Healthy;
	m_conditionTime = 0.0f;
	m_rangedCooldown = 0.0f;
	m_chargeStunned = false;
	m_playerFallen = false;
	m_victoryRoarDone = false;
}

void enemy::beginDeath()
{
	// 先に今の傾きを控える(changeStateの後では状態が変わって求められない)。
	m_deathPoseStart = getAttackPoseOffset();
	m_move = Vector3(0.0f, 0.0f, 0.0f);
	changeState(MotionState::Dead);
}

bool enemy::isInRecovery() const
{
	return m_motionState == MotionState::Recovery;
}

float enemy::getStateTime() const
{
	return m_stateTime;
}

float enemy::getAttackElapsedSeconds() const
{
	return m_attackElapsed;
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
	// 尾回転は「半回転 → 一拍 → 半回転」で、回数によって長さが変わる
	// (1回なら0.42秒、2回なら0.42+0.30+0.42=1.14秒)。
	// 戦闘判定(OneVsOneCombat)も同じ計算をするので、判定とアニメーションの時間はそろう。
	if (m_attackKind == Combat::EnemyAttackKind::TailSpin)
		return Combat::EnemySpinActiveSeconds(m_spinHalfTurns);
	return getAttackData().frames.activeSeconds;
}

bool enemy::isTailSpinning() const
{
	return m_motionState == MotionState::Active &&
		m_attackKind == Combat::EnemyAttackKind::TailSpin;
}

bool enemy::isTailSpinPausing() const
{
	if (!isTailSpinning())
		return false;
	return Combat::EnemySpinPhaseAt(m_stateTime, m_spinHalfTurns).pausing;
}

bool enemy::isChargeRunning() const
{
	return m_motionState == MotionState::Active &&
		m_attackKind == Combat::EnemyAttackKind::Charge;
}

bool enemy::isChargeSkidding() const
{
	return m_motionState == MotionState::Recovery &&
		m_attackKind == Combat::EnemyAttackKind::Charge &&
		!m_chargeStunned &&
		m_stateTime < Combat::Tuning::ENEMY_CHARGE_SKID_SECONDS;
}

bool enemy::isChargeStunned() const
{
	return m_motionState == MotionState::Recovery && m_chargeStunned;
}

bool enemy::notifyChargeBlocked()
{
	if (!isChargeRunning())
		return false;
	// 走りを打ち切って隙へ移る。よろめきの分だけ隙が長くなる(recoverySeconds)。
	changeState(MotionState::Recovery);
	m_chargeStunned = true;
	// よろめきは怯みと同じ動きを使う。顔を背ける向きは毎回入れ替える。
	m_flinchYawSign = -m_flinchYawSign;
	return true;
}

Combat::EnemySpinState enemy::getSpinState() const
{
	Combat::EnemySpinState state;
	state.sign = m_spinSign;
	state.halfTurns = m_spinHalfTurns;
	state.activeSeconds = activeSeconds();
	return state;
}

float enemy::getSpinProgress() const
{
	if (m_attackKind != Combat::EnemyAttackKind::TailSpin)
		return 0.0f;
	if (m_motionState == MotionState::Recovery)
		return 1.0f;
	if (m_motionState != MotionState::Active)
		return 0.0f;
	const int turns = std::max(1, m_spinHalfTurns);
	return Combat::EnemySpinPhaseAt(m_stateTime, turns).turnedHalfTurns /
		static_cast<float>(turns);
}

void enemy::beginTailSpin(const Vector3& targetPosition)
{
	// 尾は正面の反対を向いている。回す向きは「尾がプレイヤーへ早く届く側」を選ぶ。
	const float tailYawNow = WrapAngle(m_srt.rot.y + PI);
	const float delta = WrapAngle(angleToTarget(targetPosition) - tailYawNow);
	// 予兆の間は正面をプレイヤーへ向けているので、尾はほぼ真後ろ(差が±180度付近)になる。
	// その場合はどちらへ回しても尾が通るため、前回と逆向きにして
	// 「いつも同じ側から尾が来る」と覚えられないようにする。
	constexpr float AMBIGUOUS_ANGLE = 2.6f; // 約150度以上離れていたら「ほぼ真後ろ」とみなす
	if (std::abs(delta) >= AMBIGUOUS_ANGLE)
		m_spinSign = -m_spinSign;
	else
		m_spinSign = delta >= 0.0f ? 1.0f : -1.0f;
	m_spinStartYaw = m_srt.rot.y;
}

float enemy::recoverySeconds() const
{
	// 弱っているほど攻撃後の隙が長い。「効いている」ことが隙の長さからも伝わり、
	// 終盤ほど反撃しやすくなる(モンスターハンターの弱った大型モンスターと同じ)。
	const float scale =
		m_condition == Combat::EnemyCondition::Dying ? Combat::Tuning::ENEMY_DYING_RECOVERY_SCALE :
		m_condition == Combat::EnemyCondition::Tired ? Combat::Tuning::ENEMY_TIRED_RECOVERY_SCALE :
		1.0f;
	// 突進で壁にぶつかったときは、よろめいている分だけ長く動けない。
	const float stun = m_chargeStunned ? Combat::Tuning::ENEMY_CHARGE_WALL_STUN_SECONDS : 0.0f;
	return getAttackData().frames.recoverySeconds * scale + stun;
}

float enemy::windupTurnRate() const
{
	const float windupRatio = m_stateTime / std::max(0.01f, windupSeconds());
	switch (m_attackKind)
	{
	// 尾回転は「その場で踏ん張って回る準備をする」攻撃なので、向き直りを遅くする。
	case Combat::EnemyAttackKind::TailSpin:
		return 2.2f;
	// 火球と突進は、予兆の終わり際で向き直りを止める(狙いを決める)。
	// 最後まで追いかけると、吐いた/走り出した瞬間の向きが常にプレイヤーの方になり、
	// 「溜めを見て横へずれる」避け方が成立しない。止めた後に動いた分だけ外れる。
	case Combat::EnemyAttackKind::Fireball:
		return windupRatio >= Combat::Tuning::ENEMY_FIREBALL_AIM_LOCK_RATIO ? 0.0f : 3.5f;
	case Combat::EnemyAttackKind::Charge:
		return windupRatio >= Combat::Tuning::ENEMY_CHARGE_AIM_LOCK_RATIO ? 0.0f : 3.0f;
	default:
		return 3.5f;
	}
}

bool enemy::canUseRangedAttack() const
{
	return m_rangedCooldown <= 0.0f;
}

void enemy::requestAttack(float distance, const Vector3& targetPosition)
{
	const float facingError = facingErrorTo(targetPosition);
	// 背後の近くにいる相手には、2回に1回は向き直らずに尾回転で応じる。
	// 毎回向き直るだけだと「背後に回れば安全な時間がある」が確定してしまうため。
	if (!m_forceAttackKind &&
		facingError >= BEHIND_ANGLE &&
		distance <= Combat::Tuning::ENEMY_SPIN_HIT_RANGE)
	{
		m_behindDecisionCounter = (m_behindDecisionCounter + 1) % 2;
		if (m_behindDecisionCounter == 0)
		{
			selectNextAttack(distance, true);
			changeState(MotionState::Windup);
			return;
		}
	}
	// 正面が向いていなければ、まずその場で向き直る。攻撃はそろってから選ぶ(Turnの処理)。
	if (facingError > ATTACK_ALIGN_ANGLE)
	{
		changeState(MotionState::Turn);
		return;
	}
	selectNextAttack(distance);
	changeState(MotionState::Windup);
}

float enemy::moveSpeedScale() const
{
	const float condition =
		m_condition == Combat::EnemyCondition::Dying ? Combat::Tuning::ENEMY_DYING_MOVE_SCALE :
		m_condition == Combat::EnemyCondition::Tired ? Combat::Tuning::ENEMY_TIRED_MOVE_SCALE :
		1.0f;
	// 怒っている間は速い。弱っていても、怒れば一時的に持ち直して見える。
	return condition * (isEnraged() ? Combat::Tuning::ENEMY_RAGE_MOVE_SCALE : 1.0f);
}

float enemy::getRageThreshold() const
{
	// 怒るたびに必要なダメージが増える(怯み耐性と同じ考え方)。
	return Combat::Tuning::ENEMY_RAGE_DAMAGE_THRESHOLD *
		std::pow(Combat::Tuning::ENEMY_RAGE_THRESHOLD_GROWTH, static_cast<float>(m_rageCount));
}

void enemy::forceRage()
{
	m_rageDamage = 0.0f;
	m_rageSeconds = Combat::Tuning::ENEMY_RAGE_SECONDS;
	++m_rageCount;
	changeState(MotionState::Roar);
}

bool enemy::addRageDamage(float damage)
{
	m_rageDamage += std::max(0.0f, damage);
	// 怒っている最中は溜め直さない(怒りが途切れず続くのを防ぐ)。
	if (isEnraged() || m_rageDamage < getRageThreshold())
		return false;

	m_rageDamage = 0.0f;
	m_rageSeconds = Combat::Tuning::ENEMY_RAGE_SECONDS;
	++m_rageCount;
	// 咆哮で知らせる。ここは攻撃が来ない時間なので、プレイヤーが距離を取り直せる。
	changeState(MotionState::Roar);
	return true;
}

void enemy::setForcedAttackKind(const Combat::EnemyAttackKind* kind)
{
	m_forceAttackKind = kind != nullptr;
	if (kind != nullptr)
		m_forcedAttackKind = *kind;
}

void enemy::selectNextAttack(float distance, bool playerBehind)
{
	// 単純な巡回に位置ずらしを加えることで、外部の乱数生成器に依存せず
	// 「毎回同じ順番」にもならないようにする。
	m_attackSelectCounter = (m_attackSelectCounter + 1) % 7;

	// 調整中は指定された攻撃だけを出す。射程の条件も無視する。
	if (m_forceAttackKind)
	{
		m_previousAttackKind = m_attackKind;
		m_attackKind = m_forcedAttackKind;
	}
	else if (distance >= Combat::Tuning::ENEMY_RANGED_MIN_DISTANCE)
	{
		// 離れた相手には火球か突進。近接の4種類は届かないので候補に入れない。
		// 基本は交互に出し、3回に1回だけ同じものを続ける(交互だけだと次が読めてしまう)。
		m_rangedSelectCounter = (m_rangedSelectCounter + 1) % 3;
		const bool repeat = m_rangedSelectCounter == 0;
		const Combat::EnemyAttackKind other =
			m_previousRangedKind == Combat::EnemyAttackKind::Fireball
				? Combat::EnemyAttackKind::Charge
				: Combat::EnemyAttackKind::Fireball;
		m_previousAttackKind = m_attackKind;
		m_attackKind = repeat ? m_previousRangedKind : other;
		// 突進が届かない遠さなら火球にする。
		if (distance > Combat::Tuning::ENEMY_CHARGE_MAX_DISTANCE)
			m_attackKind = Combat::EnemyAttackKind::Fireball;
		m_previousRangedKind = m_attackKind;
	}
	else if (playerBehind)
	{
		// 背後の相手には向き直らずに尾回転。正面の攻撃は向き直らないと届かない。
		m_previousAttackKind = m_attackKind;
		m_attackKind = Combat::EnemyAttackKind::TailSpin;
	}
	else
	{
		// 距離で候補を絞り、そのうえで直前と同じ攻撃が続かないようにする。
		// 完全なランダムだと同じ攻撃が連続して「読む意味」が薄れ、
		// 逆に完全な順番固定だと暗記ゲームになるため、その中間を取る。
		const bool inBiteRange = distance <= Combat::Tuning::ENEMY_BITE_HIT_RANGE;

		Combat::EnemyAttackKind candidates[4];
		int candidateCount = 0;
		candidates[candidateCount++] = Combat::EnemyAttackKind::Slam;
		candidates[candidateCount++] = Combat::EnemyAttackKind::Sweep;
		// 尾回転は距離を問わず候補に入れる。危ないのは正面ではなく側面と背後なので、
		// 「正面の攻撃を避けたあと、横へ回り込んで待つ」立ち回りへの答えになる。
		// 懐(16以内)のプレイヤーには当たらないが、それは避け方として残す
		// (敵が「近いから出さない」と判断すると、踏み込みの安全が確定してしまう)。
		candidates[candidateCount++] = Combat::EnemyAttackKind::TailSpin;
		// 噛みつきは射程が短いので、近いときだけ選択肢に入れる。
		// 遠くから出しても当たらず、プレイヤーが予兆を読む意味が無くなるためである。
		if (inBiteRange)
			candidates[candidateCount++] = Combat::EnemyAttackKind::Bite;

		int index = (m_attackSelectCounter + (m_attackSelectCounter / 3)) % candidateCount;
		if (candidates[index] == m_previousAttackKind && candidateCount > 1)
			index = (index + 1) % candidateCount;

		m_previousAttackKind = m_attackKind;
		m_attackKind = candidates[index];
	}

	// 尾回転の回転数。半々で「半回転1回」と「半回転 → 一拍 → 半回転」を出し分ける。
	// 予兆は1回でも2回でも同じなので、「回り切って止まるのを見てから踏み込む」のが正解になる
	// (モンスターハンターの回転尾攻撃と同じ読み合い)。
	// 一拍の後にもう半回転来るかどうかは、止まった体勢を見ないと分からない。
	if (m_attackKind == Combat::EnemyAttackKind::TailSpin)
	{
		// 怒っている間は必ず2回続ける。回転が長くなり、踏み込む間が減る。
		m_spinHalfTurns = (isEnraged() || m_attackSelectCounter % 2 != 0)
			? Combat::Tuning::ENEMY_SPIN_MAX_HALF_TURNS
			: 1;
	}

	// 離れた相手への攻撃を出したら、しばらくは出さない(歩いて近づいてくる)。
	// 怒っている間は間隔が短い。
	if (m_attackKind == Combat::EnemyAttackKind::Fireball ||
		m_attackKind == Combat::EnemyAttackKind::Charge)
	{
		m_rangedCooldown = RANGED_ATTACK_INTERVAL_SECONDS *
			(isEnraged() ? Combat::Tuning::ENEMY_RAGE_CIRCLE_SCALE : 1.0f);
	}
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
	// 予兆から隙までを1本のクリップとして再生するため、攻撃の間はまとめて時間を数える。
	if (m_motionState == MotionState::Windup ||
		m_motionState == MotionState::Active ||
		m_motionState == MotionState::Recovery)
	{
		m_attackElapsed += deltaSec;
	}
	m_move = Vector3(0, 0, 0);
	m_conditionTime += deltaSec;
	// 決着の後。倒れた敵・勝ち残った敵は何もしない。
	if (m_motionState == MotionState::Dead)
		return;
	if (m_motionState == MotionState::Standby)
	{
		// 一拍おいて勝利の咆哮を上げる。咆哮の後はここへ戻り、立っているだけになる。
		if (!m_victoryRoarDone && m_stateTime >= VICTORY_ROAR_DELAY_SECONDS)
		{
			m_victoryRoarDone = true;
			changeState(MotionState::Roar);
		}
		return;
	}
	// プレイヤーを倒したら、出している攻撃(判定中・隙)だけは最後まで動かす。
	// 予兆は捨てる(誰もいないところへ攻撃を出さない)。
	if (m_playerFallen &&
		m_motionState != MotionState::Active &&
		m_motionState != MotionState::Recovery &&
		m_motionState != MotionState::Roar)
	{
		changeState(MotionState::Standby);
		return;
	}
	// 怒っている時間を減らす。切れたら普通の動きへ戻る。
	m_rageSeconds = std::max(0.0f, m_rageSeconds - deltaSec);
	m_rangedCooldown = std::max(0.0f, m_rangedCooldown - deltaSec);
	// 攻撃を当てない時間が続くと怯み値は抜けていく。
	m_posture = std::max(
		0.0f, m_posture - Combat::Tuning::ENEMY_POSTURE_RECOVERY_PER_SECOND * deltaSec);

	// 練習用の的: 怯み以外は、その場でプレイヤーの方を向くだけにする。
	// 咆哮(怒り)と怯みは練習台でも出す。どちらも攻撃ではなく、見た目の確認に使うため。
	if (m_passive && m_motionState != MotionState::Flinch && m_motionState != MotionState::Roar)
	{
		if (m_motionState != MotionState::Circle)
			changeState(MotionState::Circle);
		faceTarget(targetPosition, deltaSec, 4.0f, APPROACH_TURN_SPEED);
		return;
	}

	// 咆哮の間は何もしない(攻撃の判断もしない)。
	//
	// **終わりの判定はここで行うこと**。
	// 以前は下のswitchの`case MotionState::Roar`に書いていたが、
	// このreturnがswitchより前にあるため一度も評価されず、
	// 一度吠えた敵が二度と動かなくなっていた(怒りは260ダメージで入るので、戦闘中に突然止まる)。
	if (m_motionState == MotionState::Roar)
	{
		if (m_stateTime < Combat::Tuning::ENEMY_RAGE_ROAR_SECONDS)
		{
			m_srt.pos += m_move;
			return;
		}
		// 勝利の咆哮の後は立っているだけ。
		if (m_playerFallen)
		{
			changeState(MotionState::Standby);
			return;
		}
		// 吠え終わったら様子見へ戻す。この先は通常どおり動く。
		changeState(MotionState::Circle);
	}

	if (m_motionState == MotionState::Approach && distance <= ATTACK_DISTANCE)
	{
		// 大型の敵は衝突分離によって希望距離の少し外側で止まることがあるため、
		// 敵の中心が希望距離へ完全に入ることは遷移条件にしない。
		requestAttack(distance, targetPosition);
	}
	// 怒っている間は様子見が短く、次の攻撃がすぐ来る。
	const float circleScale =
		isEnraged() ? Combat::Tuning::ENEMY_RAGE_CIRCLE_SCALE : 1.0f;
	const bool rangedReady =
		canUseRangedAttack() && distance >= Combat::Tuning::ENEMY_RANGED_MIN_DISTANCE;
	// 離れた相手へ近づいている最中。しばらく近づけなければ火球か突進に切り替える。
	// (以前は追いかけ続けるしかなく、離れて待つプレイヤーは常に安全だった)
	if (m_motionState == MotionState::Approach && rangedReady &&
		m_stateTime >= Combat::Tuning::ENEMY_RANGED_APPROACH_SECONDS * circleScale)
	{
		requestAttack(distance, targetPosition);
	}
	if (m_motionState == MotionState::Circle &&
		m_stateTime >= MIN_CIRCLE_SECONDS * circleScale)
	{
		if (distance <= ATTACK_DISTANCE)
		{
			requestAttack(distance, targetPosition);
		}
		// 大きく離れた(逃げた)相手には、様子見を待たずに火球か突進を出す。
		else if (rangedReady && distance >= Combat::Tuning::ENEMY_RANGED_FAR_DISTANCE)
		{
			requestAttack(distance, targetPosition);
		}
		// 大きく離れているのに離れた相手への攻撃が出せない(出した直後)ときは、歩いて詰める。
		// 周回のままだと前へ出る速さが遅く、いつまでも近づけない。
		else if (!rangedReady && distance >= Combat::Tuning::ENEMY_RANGED_FAR_DISTANCE)
		{
			changeState(MotionState::Approach);
		}
		else if (m_stateTime >= MAX_CIRCLE_SECONDS * circleScale)
		{
			// 様子見を終える。近接が届かない距離なら火球か突進、そうでなければ近接。
			if (rangedReady || distance <= ATTACK_DISTANCE + 32.0f)
				requestAttack(distance, targetPosition);
		}
	}

	// 回る速さの上限にも、移動の速さの倍率(怒りで速く、弱ると遅く)を掛ける。
	const float turnScale = moveSpeedScale();
	switch (m_motionState)
	{
	case MotionState::Approach:
		// 向きは上限つきで回すので、真横や後ろの相手へは弧を描きながら近づく。
		faceTarget(targetPosition, deltaSec, 8.0f, APPROACH_TURN_SPEED * turnScale);
		moveInFacingDirection(APPROACH_SPEED * moveSpeedScale() * deltaSec);
		break;
	case MotionState::Circle:
	{
		// 周回は「プレイヤーを向いたまま真横へ滑る」のではなく、前へ歩きながら弧を描く。
		// 以前はプレイヤーを見続けたまま横へ動いていたので、前歩きのモーションのまま横へ滑り
		// (カニ歩き)、常にこちらを見ている不自然な敵になっていた。
		// 歩く向きは「プレイヤーの方向から横へずらした向き」で、ずらす量は距離で変える。
		//   遠い     : 15度ずらす(ほぼ近づく)
		//   好む距離 : 75度ずらす(横腹を見せながら回る)
		//   近すぎる : 115度ずらす(少し離れながら回る)
		const float t = std::clamp((distance - PREFERRED_DISTANCE) / 40.0f, -1.0f, 1.0f);
		const float offset = t >= 0.0f
			? 1.31f + (0.26f - 1.31f) * t
			: 1.31f + (2.00f - 1.31f) * -t;
		const float desiredYaw = angleToTarget(targetPosition) + m_circleDirection * offset;
		turnTowardYaw(desiredYaw, deltaSec, 4.0f, CIRCLE_TURN_SPEED * turnScale);
		moveInFacingDirection(CIRCLE_SPEED * moveSpeedScale() * deltaSec);
		break;
	}
	case MotionState::Turn:
	{
		// その場で足を踏み替えて向き直る。動かないので、この間が側面・背後を殴る機会になる。
		// そろったら、その時の距離で攻撃を選ぶ(向き直っている間に離れられたら火球・突進になる)。
		faceTarget(targetPosition, deltaSec, 6.0f, TURN_IN_PLACE_SPEED * turnScale);
		if (facingErrorTo(targetPosition) <= ATTACK_ALIGN_ANGLE)
		{
			selectNextAttack(distance);
			changeState(MotionState::Windup);
		}
		else if (m_stateTime >= TURN_TIMEOUT_SECONDS)
		{
			changeState(MotionState::Circle);
		}
		break;
	}
	case MotionState::Windup:
	{
		const float turnRate = windupTurnRate();
		if (turnRate > 0.0f)
			faceTarget(targetPosition, deltaSec, turnRate, WINDUP_TURN_SPEED);
		if (m_stateTime >= windupSeconds())
		{
			if (m_attackKind == Combat::EnemyAttackKind::TailSpin)
				beginTailSpin(targetPosition);
			changeState(MotionState::Active);
		}
		break;
	}
	case MotionState::Active:
		if (m_attackKind == Combat::EnemyAttackKind::TailSpin)
		{
			// 体ごと回す。当たり判定も描画もこの物理の向きを見ているので、
			// 「尾が通ったところに当たる」が見た目と一致する
			// (見た目だけ回すと判定の向きとずれて、予兆が嘘になる)。
			// 「半回転 → 一拍おいて構え直す → もう半回転」のリズムで回す
			// (リオレウス・リオレイアの回転尾攻撃。一続きに回すと、どこで止まるか読めない)。
			// 半回転ごとに出だしと止まり際をなめらかにして、止まった瞬間を見て取れるようにする。
			const Combat::EnemySpinPhase spin =
				Combat::EnemySpinPhaseAt(m_stateTime, m_spinHalfTurns);
			m_srt.rot.y = WrapAngle(m_spinStartYaw + m_spinSign * PI * spin.turnedHalfTurns);
			// 回りながら少し流れる。踏み込みではないので小さくする。
			// 止まっている間は動かさない(止まって見せることがこの攻撃の読み合いなので)。
			if (!spin.pausing)
				moveInFacingDirection(Combat::Tuning::ENEMY_SPIN_LUNGE_SPEED * deltaSec);
		}
		else if (m_attackKind == Combat::EnemyAttackKind::Charge)
		{
			// 予兆の終わりで決めた向きのまま、一直線に走る。向き直りはしない
			// (プレイヤーが横へ避けたら、そのまま通り過ぎる)。
			// 走り出しは少しずつ速くする。いきなり最高速だと溜めから瞬間移動したように見える。
			const float accel = std::clamp(
				m_stateTime / Combat::Tuning::ENEMY_CHARGE_ACCEL_SECONDS, 0.0f, 1.0f);
			moveInFacingDirection(
				Combat::Tuning::ENEMY_CHARGE_SPEED * accel * (2.0f - accel) * deltaSec);
		}
		else
		{
			// 攻撃判定中は踏み込ませ、攻撃の有効時間を動きでも分かるようにする。
			// 速さは攻撃ごとの表から引く。判定の時間を短くしたので、以前の固定値(48/68)のままだと
			// 踏み込む距離が1/4になり、「距離を取るだけでは避けられない」読み合いが消える。
			moveInFacingDirection(Combat::EnemyLungeSpeedOf(m_attackKind) * deltaSec);
		}
		if (m_stateTime >= activeSeconds()) changeState(MotionState::Recovery);
		break;
	case MotionState::Recovery:
		// 突進のあとは、走った勢いのまま滑って止まる(速さは二乗で落ちる。約28進む)。
		// 走りの最高速から始めるので、走りから隙へ移る瞬間に速さが途切れない。
		if (isChargeSkidding())
		{
			const float remain =
				1.0f - m_stateTime / Combat::Tuning::ENEMY_CHARGE_SKID_SECONDS;
			moveInFacingDirection(
				Combat::Tuning::ENEMY_CHARGE_SPEED * remain * remain * deltaSec);
		}
		if (m_stateTime >= recoverySeconds())
		{
			// 尾回転のあとは後退させない。半回転1回なら背中を向けたまま止まっているので、
			// そこから下がると「プレイヤーの方へ突っ込む」動きになってしまう
			// (後退は自分の正面の反対へ動く処理なので、向きが逆のときは前へ出る)。
			// 突進のあとも同じ(相手を通り過ぎて背中を向けている)。火球のあとはもともと離れている。
			// 怒っている間は下がらずに次の攻撃へ移る(押し続ける)。
			const bool keepDistance =
				m_attackKind == Combat::EnemyAttackKind::TailSpin ||
				m_attackKind == Combat::EnemyAttackKind::Charge ||
				m_attackKind == Combat::EnemyAttackKind::Fireball;
			changeState(
				(isEnraged() || keepDistance)
					? MotionState::Circle
					: MotionState::Retreat);
		}
		break;
	// 咆哮(Roar)はここへ来ない。上で早期に処理して終わりの判定も済ませている。
	case MotionState::Flinch:
		// 怯んでいる間は何もしない。向き直りもしないので、背後へ回り込む機会になる。
		// 明けたら様子見へ戻す。後退させると、反撃に踏み込んだプレイヤーから逃げてしまい、
		// 怯ませた手応えが薄れるためである。
		if (m_stateTime >= Combat::Tuning::ENEMY_FLINCH_SECONDS)
			changeState(MotionState::Circle);
		break;
	case MotionState::Retreat:
		faceTarget(targetPosition, deltaSec, 6.0f, APPROACH_TURN_SPEED * turnScale);
		moveInFacingDirection(-RETREAT_SPEED * moveSpeedScale() * deltaSec);
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
	// 撮影・比較用。構えを入れる前の見た目を再現する(dev_settings.ini の enemy_attack_pose=0)。
	// 怯み・弱り具合の姿勢もまとめて切る。どれも同じ「描画用SRTへ足す演出」で、
	// 一部だけ残すと比較にならないため。
	if (!s_attackPoseEnabled)
		return Combat::EnemyPoseOffset{};

	// 登場の咆哮。登場の間はAIが動かないので、外から渡された時刻で姿勢を付ける。
	if (m_introRoarTime >= 0.0f)
		return Combat::EnemyIntroRoarPose(m_introRoarTime);
	// 倒れた。倒れた瞬間の傾きを短く0へ戻し、あとは倒れるクリップに任せる。
	if (m_motionState == MotionState::Dead)
	{
		constexpr float DEATH_POSE_FADE_SECONDS = 0.30f;
		const float t = std::clamp(m_stateTime / DEATH_POSE_FADE_SECONDS, 0.0f, 1.0f);
		const float remain = 1.0f - t * t * (3.0f - 2.0f * t);
		Combat::EnemyPoseOffset pose = m_deathPoseStart;
		pose.pitch *= remain;
		pose.yaw *= remain;
		return pose;
	}
	// 咆哮は最優先。吠えている間は攻撃の構えも弱り具合の姿勢も出さない。
	if (m_motionState == MotionState::Roar)
		return Combat::EnemyRoarPose(m_stateTime, Combat::Tuning::ENEMY_RAGE_ROAR_SECONDS);
	// 怯みは攻撃の構えより優先する。構えの途中で怯んだら、構えを捨ててのけぞる。
	if (m_motionState == MotionState::Flinch)
	{
		return Combat::EnemyFlinchPose(
			m_stateTime, Combat::Tuning::ENEMY_FLINCH_SECONDS, m_flinchYawSign);
	}
	// 突進で壁にぶつかったよろめき。怯みと同じ動きを、隙の長さいっぱいに引き伸ばして使う。
	if (isChargeStunned())
		return Combat::EnemyFlinchPose(m_stateTime, recoverySeconds(), m_flinchYawSign);
	// 尾回転は回る向きで構えが左右反転し、一拍の位置も回転数で変わるので、状態ごと渡す。
	Combat::EnemyPoseOffset pose = Combat::EnemyAttackPose(
		m_attackKind, currentAttackPhase(), m_stateTime, getSpinState());

	// 弱り具合の姿勢は、攻撃の予兆と攻撃判定の間には足さない。
	// 息で頭が上下すると構えの形が崩れ、「何が来るか」が読みにくくなるため。
	// 予兆の読みやすさは、弱り具合の表現より優先する。
	const Combat::EnemyAttackPhase phase = currentAttackPhase();
	if (phase == Combat::EnemyAttackPhase::None || phase == Combat::EnemyAttackPhase::Recovery)
	{
		const bool moving =
			m_motionState == MotionState::Approach ||
			m_motionState == MotionState::Circle ||
			m_motionState == MotionState::Retreat ||
			m_motionState == MotionState::Turn;
		const Combat::EnemyPoseOffset condition =
			Combat::EnemyConditionPose(m_condition, moving, m_conditionTime);
		pose.pitch += condition.pitch;
		pose.yaw += condition.yaw;
	}
	return pose;
}

Combat::EnemyTellPose enemy::getAttackTellPose() const
{
	// 全身の傾きと同じ切り替え(dev_settings.ini の enemy_attack_pose=0)で、比較用にまとめて切れる。
	if (!s_attackPoseEnabled)
		return Combat::EnemyTellPose{};
	// 登場の咆哮。音に合わせて口を開け、吠えている間は開けたまま震わせる。
	if (m_introRoarTime >= 0.0f)
		return Combat::EnemyIntroRoarTellPose(m_introRoarTime);
	// 咆哮は首を持ち上げてあごを開く。
	if (m_motionState == MotionState::Roar)
		return Combat::EnemyRoarTellPose(m_stateTime, Combat::Tuning::ENEMY_RAGE_ROAR_SECONDS);
	// 怯んでいる間は攻撃の構えを捨てる(全身の傾きと同じ扱い)。
	if (m_motionState == MotionState::Flinch || m_motionState == MotionState::Dead || isChargeStunned())
		return Combat::EnemyTellPose{};
	return Combat::EnemyAttackTellPose(
		m_attackKind, currentAttackPhase(), m_stateTime, getSpinState());
}

bool enemy::setHealthRatio(float ratio)
{
	const Combat::EnemyCondition next =
		ratio <= Combat::Tuning::ENEMY_DYING_HP_RATIO ? Combat::EnemyCondition::Dying :
		ratio <= Combat::Tuning::ENEMY_TIRED_HP_RATIO ? Combat::EnemyCondition::Tired :
		Combat::EnemyCondition::Healthy;
	// 体力は減る一方なので、悪くなる向きだけを見る。
	if (static_cast<int>(next) <= static_cast<int>(m_condition))
		return false;

	m_condition = next;
	// 段階が変わった瞬間はよろめかせる。姿勢や速さの変化はじわじわしていて
	// 気付きにくいので、「今、効いた」と分かるきっかけを作る。
	// 怯みと同じ動きを使うが、怯み値や怯み耐性の回数には数えない。
	// 倒れる直前(体力0)は対象外。倒れたらすぐリザルトへ移るため。
	if (ratio > 0.0f && m_motionState != MotionState::Flinch)
	{
		m_flinchYawSign = -m_flinchYawSign;
		changeState(MotionState::Flinch);
	}
	return true;
}

Combat::EnemyCondition enemy::getCondition() const
{
	return m_condition;
}

const char* enemy::getConditionName() const
{
	switch (m_condition)
	{
	case Combat::EnemyCondition::Tired: return "疲れ";
	case Combat::EnemyCondition::Dying: return "瀕死";
	case Combat::EnemyCondition::Healthy:
	default: return "通常";
	}
}

bool enemy::addPostureDamage(float amount, const Vector3& hitFromPosition)
{
	m_posture += amount;
	// 怯んでいる最中に重ねて怯ませない。怯みが途切れず続き、一方的な戦いになるため。
	if (m_motionState == MotionState::Flinch ||
		m_posture < getFlinchThreshold())
	{
		return false;
	}

	m_posture = 0.0f;
	++m_flinchCount;
	// 打たれた側と反対へ顔を背ける。敵の右側から打たれたら左へ、という向きにする。
	// 敵の右方向は、正面(-sin, -cos)を上から見て時計回りに90度回したもの。
	const Vector3 forward(-std::sin(m_srt.rot.y), 0.0f, -std::cos(m_srt.rot.y));
	const Vector3 right(-forward.z, 0.0f, forward.x);
	const float side =
		(hitFromPosition.x - m_srt.pos.x) * right.x +
		(hitFromPosition.z - m_srt.pos.z) * right.z;
	m_flinchYawSign = side >= 0.0f ? 1.0f : -1.0f;
	changeState(MotionState::Flinch);
	return true;
}

bool enemy::isFlinching() const
{
	return m_motionState == MotionState::Flinch;
}

float enemy::getPosture() const
{
	return m_posture;
}

float enemy::getFlinchThreshold() const
{
	// 怯んだ回数だけしきい値を上げ、上限で止める。
	const float grown = Combat::Tuning::ENEMY_FLINCH_BASE_THRESHOLD *
		std::pow(Combat::Tuning::ENEMY_FLINCH_THRESHOLD_GROWTH, static_cast<float>(m_flinchCount));
	// 怒っている間は怯みにくい。殴って止める立ち回りが通りにくくなる。
	const float resist = isEnraged() ? Combat::Tuning::ENEMY_RAGE_FLINCH_RESIST : 1.0f;
	return std::min(grown, Combat::Tuning::ENEMY_FLINCH_THRESHOLD_MAX) * resist;
}

int enemy::getFlinchCount() const
{
	return m_flinchCount;
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
	// 攻撃の始まり(予兆)でだけ、攻撃の経過時間を数え直す。
	if (nextState == MotionState::Windup)
		m_attackElapsed = 0.0f;
	// よろめきは突進で壁にぶつかった隙の間だけ。ほかの状態へ移ったら解く。
	m_chargeStunned = false;
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

void enemy::turnTowardYaw(float targetYaw, float deltaSec, float turnRate, float maxTurnSpeed)
{
	m_destrot.y = WrapAngle(targetYaw);
	const float diffrot = WrapAngle(m_destrot.y - m_srt.rot.y);
	// 差が小さいときは割合で滑らかに寄せ、大きいときは上限の速さで回る。
	const float maxStep = maxTurnSpeed * deltaSec;
	const float step = std::clamp(diffrot * std::min(1.0f, turnRate * deltaSec), -maxStep, maxStep);
	m_srt.rot.y = WrapAngle(m_srt.rot.y + step);
}

void enemy::faceTarget(
	const Vector3& targetPosition, float deltaSec, float turnRate, float maxTurnSpeed)
{
	turnTowardYaw(angleToTarget(targetPosition), deltaSec, turnRate, maxTurnSpeed);
}

float enemy::facingErrorTo(const Vector3& targetPosition) const
{
	return std::abs(WrapAngle(angleToTarget(targetPosition) - m_srt.rot.y));
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
	case MotionState::Flinch: return "怯み";
	case MotionState::Roar: return "咆哮(怒り)";
	case MotionState::Turn: return "向き直り";
	case MotionState::Dead: return "倒れた";
	case MotionState::Standby: return "勝ち残り";
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
