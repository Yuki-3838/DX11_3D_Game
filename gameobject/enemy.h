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
	// Flinchは攻撃を受けて怯んでいる状態。行動が止まり、攻撃の予兆も中断される。
	// Roarは怒りに入る合図の咆哮。攻撃はせず、当たり判定も出ない。
	// Turnは攻撃の前に、その場で足を踏み替えてプレイヤーの方へ向き直っている状態。
	// 大型の敵は一瞬では振り向けない。向き直っている間が側面・背後を殴る機会になる。
	enum class MotionState { Approach, Circle, Windup, Active, Recovery, Retreat, Flinch, Roar, Turn };

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
	// 攻撃(予兆)を始めてからの経過時間。攻撃モーションの再生位置をこの時間から決める。
	// 状態ごとの時間(getStateTime)は判定や隙へ移るたびに0へ戻るので、クリップの再生には使えない。
	float getAttackElapsedSeconds() const;
	SRT getRenderSRT() const;
	void setVisualGroundOffsetY(float offsetY);

	// 現在選択中の攻撃。戦闘判定(OneVsOneCombat)とHUDが同じものを参照することで、
	// 「見えている予兆」と「実際に来る攻撃」が必ず一致する。
	Combat::EnemyAttackKind getAttackKind() const;
	const Combat::AttackData& getAttackData() const;

	// 攻撃を受けたときに怯み値を溜める。しきい値を超えたら怯み、trueを返す。
	// hitFromPositionは攻撃してきた相手の位置で、顔を背ける向きを決めるのに使う。
	bool addPostureDamage(float amount, const Vector3& hitFromPosition);
	bool isFlinching() const;

	// --- 怒り ---
	// 与えたダメージが一定を超えると怒る。怒っている間は動きが速く、攻撃が続けて来る。
	// 怒りに入った瞬間だけtrueを返す(呼び出し側で咆哮の音と画面の揺れを出し、攻撃を取り消す)。
	bool addRageDamage(float damage);
	// 調整・撮影用。ダメージを与えずに怒らせる(dev_settings.ini の rage_at_start と
	// デバッグ表示のボタンから呼ぶ)。実際に殴って怒らせるには時間がかかるため。
	void forceRage();
	bool isEnraged() const { return m_rageSeconds > 0.0f; }
	bool isRoaring() const { return m_motionState == MotionState::Roar; }
	// 登場演出の咆哮。咆哮の音を鳴らしてからの秒を渡す(負なら咆哮していない)。
	// 登場の間は敵のAIを動かさないので、状態(Roar)ではなく時刻を外から渡して姿勢だけを付ける。
	void setIntroRoarTime(float seconds) { m_introRoarTime = seconds; }
	// デバッグ表示用。次に怒るまでの溜まり具合と、怒りの残り時間。
	float getRageDamage() const { return m_rageDamage; }
	float getRageThreshold() const;
	float getRageSecondsLeft() const { return m_rageSeconds; }
	int getRageCount() const { return m_rageCount; }
	float getPosture() const;
	// 残り体力の割合(0〜1)を受け取り、弱り具合を更新する。
	// 体力ゲージを出さない代わりに、弱り具合で動き・隙・姿勢を変える。
	// 弱り具合が一段悪くなった瞬間はよろめいてtrueを返す(呼び出し側で攻撃を取り消す)。
	bool setHealthRatio(float ratio);
	Combat::EnemyCondition getCondition() const;
	const char* getConditionName() const;
	// 歩きのアニメーションを移動の速さに合わせて遅くするため、シーン側が使う。
	float getMoveSpeedScale() const { return moveSpeedScale(); }

	// 次に怯むまでに必要な怯み値。怯むたびに上がる(怯み耐性)。
	float getFlinchThreshold() const;
	int getFlinchCount() const;

	// 演出で加えている体の傾き・ひねり・高さ。
	// 傾けると足元の最下点が変わるため、接地オフセットを計算する側が必要とする。
	Combat::EnemyPoseOffset getAttackPoseOffset() const;
	// 攻撃ごとに首・あご・尾・前脚を動かす量。攻撃のシルエットを種類ごとに変えるために使う。
	Combat::EnemyTellPose getAttackTellPose() const;

	// --- 尾回転(体ごと半回転して尾で薙ぐ攻撃) ---
	// 回る向きと回転数は、当たり判定・アニメーション・構えが同じものを見る必要があるため公開する。
	// 回転そのものは物理の向き(m_srt.rot.y)を回して作るので、
	// 当たり判定(敵の向きの反対=尾の向き)と見た目は自動で一致する。
	int getSpinHalfTurns() const { return m_spinHalfTurns; }
	float getSpinSign() const { return m_spinSign; }
	// 回っている間(Active)か。回転中は脚を動かすため、アニメーション側が見る。
	bool isTailSpinning() const;

	// --- 突進 ---
	// 走っている最中(Active)か。走っている間は歩きのクリップを速く回す。
	bool isChargeRunning() const;
	// 走り終えて滑りながら止まっている最中か。脚は走りより遅く動かす。
	bool isChargeSkidding() const;
	// 壁にぶつかってよろめいている最中か。
	bool isChargeStunned() const;
	// 走っている最中に壁で止められたことをシーンから知らせる(壁との衝突はシーン側が解いている)。
	// 走るのをやめ、その場で長くよろめく(大きな隙)。止めたときはtrueを返す。
	bool notifyChargeBlocked();
	// 半回転と半回転の間で止まっている(一拍おいている)最中か。
	// 止まっている間は脚も止める。動かすと、回っていないのに足踏みして見える。
	bool isTailSpinPausing() const;
	// 構えを求めるために渡す、尾回転の状態(回る向き・半回転の回数・判定の長さ)。
	Combat::EnemySpinState getSpinState() const;
	// 回転の進み具合(0→1)。回転中以外は0。
	float getSpinProgress() const;

	// 調整用。敵を攻撃させず、その場でプレイヤーの方を向かせるだけにする(練習用の的)。
	// プレイヤーの攻撃モーションを、敵の攻撃に邪魔されずに見比べるため。
	void setPassive(bool passive) { m_passive = passive; }
	bool isPassive() const { return m_passive; }

	// 調整用。攻撃の種類を固定する(デバッグ表示からのみ使う)。
	// 3種類の構えを見比べるには同じ攻撃を繰り返し出させる必要があるが、
	// 通常の選択は距離と直前の攻撃で変わるため、狙った攻撃が出るまで待つことになる。
	void setForcedAttackKind(const Combat::EnemyAttackKind* kind);

	// 撮影・比較用。構え(EnemyAttackPose)の加算を切る。
	// 構えを入れる前と後を、同じカメラ・同じ攻撃で撮り比べるために使う。
	// 全個体で共通の設定なので静的に持つ。
	static void setAttackPoseEnabled(bool enabled) { s_attackPoseEnabled = enabled; }
	static bool isAttackPoseEnabled() { return s_attackPoseEnabled; }

private:
	static inline bool s_attackPoseEnabled = true;

	void changeState(MotionState nextState);
	// 描画用の構え(EnemyAttackPose.h)へ渡すため、AIの状態を攻撃の段階へ変換する。
	Combat::EnemyAttackPhase currentAttackPhase() const;
	// playerBehind=true なら、近接の候補から尾回転を選ぶ(背後の相手へ向き直らずに応じる)。
	void selectNextAttack(float distance, bool playerBehind = false);
	// 攻撃すると決めたときに呼ぶ。正面がプレイヤーへ向いていれば攻撃を選んで予兆へ入り、
	// 向いていなければ、まずその場で向き直る(Turn)。背後の相手には尾回転で応じることもある。
	void requestAttack(float distance, const Vector3& targetPosition);
	// 離れた相手への攻撃を続けて出せる状態か(一度出したら少し間を空ける)。
	bool canUseRangedAttack() const;
	// 予兆の間、どれだけの速さでプレイヤーの方へ向き直るか。
	// 火球と突進は予兆の終わり際に向き直りを止める(狙いを決める)ので0を返す。
	float windupTurnRate() const;
	float distanceToTarget(const Vector3& targetPosition) const;
	float angleToTarget(const Vector3& targetPosition) const;
	// 目標の向きへ回る。turnRate は差の何割を1秒で詰めるか(近いほどゆっくり)、
	// maxTurnSpeed は回る速さの上限(ラジアン/秒)。大型の敵は一瞬では振り向けない。
	void turnTowardYaw(float targetYaw, float deltaSec, float turnRate, float maxTurnSpeed);
	void faceTarget(const Vector3& targetPosition, float deltaSec, float turnRate, float maxTurnSpeed);
	// 正面とプレイヤーへの向きの差(0〜π)。
	float facingErrorTo(const Vector3& targetPosition) const;
	void moveInFacingDirection(float distance);
	// 尾回転に入る瞬間に、回る向きと基準の向きを決める。
	void beginTailSpin(const Vector3& targetPosition);

	static constexpr float PREFERRED_DISTANCE = 26.0f;
	static constexpr float ATTACK_DISTANCE = 32.0f;
	static constexpr float RETREAT_DISTANCE = 38.0f;
	static constexpr float APPROACH_SPEED = 84.0f;
	// 周回は横歩き(プレイヤーを向いたまま真横へ滑る)をやめ、前へ歩きながら弧を描く。
	// 回る速さの上限(CIRCLE_TURN_SPEED)と合わせて、半径 = 速さ / 回る速さ ≒ 29 の弧になる。
	// 以前の56のままだと弧が大きく(約37)、攻撃を決める距離(32)へ入りにくい。
	static constexpr float CIRCLE_SPEED = 44.0f;
	static constexpr float RETREAT_SPEED = 98.0f;
	// 回る速さの上限(ラジアン/秒)。以前は差の割合で回すだけで上限が無く、
	// 真後ろへ回り込まれても1秒足らずで向き直っていた(毎秒1000度以上)。
	// モンスターハンターの大型モンスターのように、大きく回り込まれたら向き直るのに時間がかかるようにする。
	// どれも移動の速さの倍率(怒り・弱り具合)を掛けて使う。
	static constexpr float APPROACH_TURN_SPEED = 1.75f;      // 約100度/秒
	static constexpr float CIRCLE_TURN_SPEED = 1.50f;        // 約86度/秒
	static constexpr float TURN_IN_PLACE_SPEED = 2.00f;      // 約115度/秒。止まって向き直るときは歩きながらより速い
	static constexpr float WINDUP_TURN_SPEED = 1.40f;        // 約80度/秒。予兆の間の追いかけ
	// 攻撃へ入ってよい正面のずれ(約35度)。これより外れていたら、まずその場で向き直る。
	// 向き直らずに攻撃すると、横にいる相手へ噛みつきが空振りする(当たり判定は正面寄りだけ)。
	static constexpr float ATTACK_ALIGN_ANGLE = 0.61f;
	// 背後にいるとみなす角度(約100度)。背後の相手へは、向き直らずに尾回転で応じることがある。
	static constexpr float BEHIND_ANGLE = 1.75f;
	// 向き直りがこれだけかかったら諦めて周回へ戻る(プレイヤーが回り続けて追いつけないとき)。
	static constexpr float TURN_TIMEOUT_SECONDS = 2.2f;
	// 攻撃の時間は system/CombatAttackTable.h が定義元。
	// 攻撃の種類ごとに長さが変わるため、固定値ではなく
	// 選択中の攻撃データ(m_attackKind)から都度取得する。
	// 戦闘判定(OneVsOneCombat)も同じデータを参照するため、
	// 見た目の予備動作と実際の攻撃判定が必ず一致する。
	float windupSeconds() const;
	float activeSeconds() const;
	// 弱っているほど長くなる(敵AIの側だけ)。
	float recoverySeconds() const;
	// 移動の速さの倍率。弱っているほど遅い。攻撃の踏み込みには掛けない
	// (掛けると攻撃の届く距離と見た目が食い違う)。
	float moveSpeedScale() const;
	static constexpr float RETREAT_SECONDS = 0.22f;
	static constexpr float MIN_CIRCLE_SECONDS = 0.55f;
	static constexpr float MAX_CIRCLE_SECONDS = 1.40f;
	static constexpr float MAX_DELTA_SECONDS = 0.1f;
	// 離れた相手への攻撃を出してから、次に出せるまでの時間。
	// 間を空けないと、突進で離れる → また遠いので突進、を繰り返して近づいてこなくなる。
	static constexpr float RANGED_ATTACK_INTERVAL_SECONDS = 3.2f;

	Vector3 m_move{0, 0, 0};
	Vector3 m_destrot{0, 0, 0};
	player* m_target{nullptr};
	MotionState m_motionState = MotionState::Approach;
	float m_stateTime = 0.0f;
	// 予兆・判定・隙をまたいで数える、攻撃を始めてからの時間。
	float m_attackElapsed = 0.0f;
	float m_circleDirection = 1.0f;
	float m_visualGroundOffsetY = 0.0f;
	Combat::EnemyAttackKind m_attackKind = Combat::EnemyAttackKind::Slam;
	// 同じ攻撃が続けて出ると読み合いにならないため、直前に使った攻撃を覚えておく。
	Combat::EnemyAttackKind m_previousAttackKind = Combat::EnemyAttackKind::Slam;
	int m_attackSelectCounter = 0;
	// 溜まっている怯み値と、怯んだときに顔を背ける向き。
	// 怒り。与えられたダメージを溜め、しきい値を超えると一定時間だけ怒る。
	float m_rageDamage = 0.0f;
	float m_rageSeconds = 0.0f;
	float m_introRoarTime = -1.0f;
	int m_rageCount = 0;
	float m_posture = 0.0f;
	float m_flinchYawSign = 1.0f;
	int m_flinchCount = 0;
	Combat::EnemyCondition m_condition = Combat::EnemyCondition::Healthy;
	// 呼吸や足取りの周期に使う。状態が変わっても0へ戻さない(戻すと動きが途切れて見える)。
	float m_conditionTime = 0.0f;
	bool m_passive = false;
	bool m_forceAttackKind = false;
	Combat::EnemyAttackKind m_forcedAttackKind = Combat::EnemyAttackKind::Slam;
	// 尾回転。半回転の回数(1か2)、回る向き(+1/-1)、回り始めたときの向き。
	int m_spinHalfTurns = 1;
	float m_spinSign = 1.0f;
	float m_spinStartYaw = 0.0f;
	// 離れた相手への攻撃。次に出せるまでの残り時間と、直前に出した種類(交互に出すため)。
	float m_rangedCooldown = 0.0f;
	Combat::EnemyAttackKind m_previousRangedKind = Combat::EnemyAttackKind::Charge;
	int m_rangedSelectCounter = 0;
	// 背後の相手へ尾回転で応じるか、向き直るかを交互に決めるための数。
	int m_behindDecisionCounter = 0;
	// 突進が壁で止められ、よろめいている。隙を長くし、構えの代わりによろめきの姿勢を出す。
	bool m_chargeStunned = false;
};
