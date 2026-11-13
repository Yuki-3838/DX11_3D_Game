#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	// 角度の差を -PI〜PI へ収める。そのまま引くと、-179度と179度の差が358度になり逆回りする。
	float WrapAngle(float radians)
	{
		const float twoPi = PI * 2.0f;
		radians = std::fmod(radians + PI, twoPi);
		if (radians < 0.0f)
			radians += twoPi;
		return radians - PI;
	}

	// 経過時間に対する指数補間の係数。fpsが違っても同じ秒数で同じだけ近づく。
	float ExpBlend(float rate, float deltaSeconds)
	{
		return 1.0f - std::exp(-rate * std::max(deltaSeconds, 0.0f));
	}

	// --- 調整値(秒・ラジアン・ワールド単位) ---
	// 自動追従: 視点を最後に触ってから、この秒数が経つまでは回さない。
	// 自分で合わせた視点を勝手に戻されると、操作を奪われたように感じるため。
	// 0.8秒だと、マウスで向きを合わせて走り出した直後にカメラが勝手に回り始め、
	// 「滑る」と感じる原因になっていた。マウスは手を一瞬離すことが多いので長めにする。
	constexpr float AUTO_FOLLOW_DELAY = 1.5f;
	// 待ち時間が過ぎてから、効きが最大になるまでの時間(秒)。
	constexpr float AUTO_FOLLOW_RAMP_SECONDS = 1.0f;
	// 自動追従の最大の速さ(ラジアン/秒)。真横へ全力で走ったときにこの速さになる。
	// 最初は約57度/秒にしたが、実測するとダッシュで横へ走り続けたときに半径約46(身長の2.5倍)の円を
	// 約6秒で一周してしまい、カメラに振り回される。約29度/秒(半径約90、一周約12秒)へ下げた。
	constexpr float AUTO_FOLLOW_MAX_RATE = 0.5f;
	// これ以上カメラ側へ向かって走っているときは回さない(約130度)。
	// 手前へ走るたびにカメラが半回転すると、前後の移動が成り立たない。
	constexpr float AUTO_FOLLOW_MAX_ANGLE = 2.3f;
	constexpr float AUTO_FOLLOW_FULL_SPEED = 45.0f;

	// 追従点。水平は速く、上下はゆっくり追う(回避や吹き飛ばしで画面が上下に揺れないように)。
	constexpr float ANCHOR_HORIZONTAL_RATE = 14.0f;
	constexpr float ANCHOR_VERTICAL_RATE = 6.0f;

	// 衝突で寄るときは速く(壁にめり込む瞬間を見せない)、離れるときはゆっくり戻す。
	constexpr float DISTANCE_PULL_IN_RATE = 20.0f;
	constexpr float DISTANCE_RELEASE_RATE = 3.5f;

	// ロックオン: 向きの補間の速さと、向きが変わる速さの上限(ラジアン/秒)。
	// 上限が無いと、敵がプレイヤーの真横や頭上を横切ったときにカメラが一瞬で半回転する。
	constexpr float LOCK_ON_YAW_RATE = 8.0f;
	constexpr float LOCK_ON_MAX_YAW_SPEED = 3.5f;
	constexpr float LOCK_ON_PITCH_RATE = 5.0f;
	// 注視点をプレイヤーから敵の方へ寄せる割合。0.5で中間。
	// プレイヤーを画面の下寄りに、敵を上寄りに置き、両方の足元と敵の予兆を同時に読めるようにする。
	constexpr float LOCK_ON_LOOK_TOWARD_TARGET = 0.45f;
	constexpr float LOOK_OFFSET_RATE = 8.0f;
	constexpr float LOCK_ON_MIN_DISTANCE = 64.0f;
	constexpr float LOCK_ON_MAX_DISTANCE = 112.0f;
	constexpr float LOCK_ON_SHOULDER_OFFSET = 5.0f;

	constexpr float RECENTER_RATE = 12.0f;

	// ロックオン中、壁を背にしてカメラの後ろに余地が無いときは、壁に沿って横へ回り込む。
	// 以前は距離を縮めるだけだったため、壁際では追従点の約6後ろまで寄り、
	// プレイヤーが画面から消えてドラゴンの体で画面が埋まっていた(2026-11-10の自動プレイで、
	// 壁際に押し込まれた後半の画面の約7割)。
	// 上へ逃がす案は、密着した大型の敵では頭(予兆)が画面の上へ切れるので採らなかった。
	// 横へ回れば、プレイヤーと敵を横から並べて映せる。
	// 欲しい水平距離のうち、この割合が取れる向きを探す。
	constexpr float WALL_SLIDE_ROOM_RATIO = 0.75f;
	// 回り込む角度の上限(約100度)。壁際では真横(90度)まで回らないと余地が出ないことが多い。
	constexpr float WALL_SLIDE_MAX = 1.75f;
	constexpr float WALL_SLIDE_SEARCH_STEP = 0.1f;
	// 回り込むときは速く(ただし向きの速さの上限 LOCK_ON_MAX_YAW_SPEED は掛かる)、戻るときはゆっくり。
	constexpr float WALL_SLIDE_ENGAGE_RATE = 8.0f;
	constexpr float WALL_SLIDE_RELEASE_RATE = 1.5f;
	// 壁の判定に使う円は、プレイヤーよりこの分だけ外まで広げる(GameScene の壁の判定と同じ考え方)。
	constexpr float WALL_ROOM_BEYOND_ANCHOR = 6.0f;

	// スティックで回る最高の速さ(ラジアン/秒)。横は約150度/秒、縦は約92度/秒。
	constexpr float STICK_YAW_SPEED = 2.6f;
	constexpr float STICK_PITCH_SPEED = 1.6f;
	// 回る速さの追いつき方。倒したときは約60ミリ秒で立ち上がり、離したときは約25ミリ秒で止まる。
	constexpr float STICK_ACCEL_RATE = 16.0f;
	constexpr float STICK_DECEL_RATE = 40.0f;

	// スティックの倒し具合を回る速さへ。少し倒したときは細かく合わせやすいよう遅く、
	// 倒し切ると最高の速さになる(直線と3乗の中間)。
	float StickCurve(float value)
	{
		const float v = std::clamp(value, -1.0f, 1.0f);
		return 0.4f * v + 0.6f * v * v * v;
	}

	// 回る速さを目標へ近づける。目標が0に近づく(離す)ときは速く止める。
	float ApproachStickSpeed(float current, float target, float deltaSeconds)
	{
		const bool slowingDown = std::abs(target) < std::abs(current) || target * current < 0.0f;
		const float rate = slowingDown ? STICK_DECEL_RATE : STICK_ACCEL_RATE;
		const float next = current + (target - current) * ExpBlend(rate, deltaSeconds);
		// 指数で近づけると0にいつまでも届かず、ごくわずかに回り続けるので切り捨てる。
		return (target == 0.0f && std::abs(next) < 0.001f) ? 0.0f : next;
	}

	// プレイヤーの背後から、プレイヤーと同じ向きを見るカメラのYaw。
	// プレイヤーの正面は (-sin, -cos)、カメラの見る向きは (-sin, cos) なので、両者が一致するのは
	// 「PI - プレイヤーのYaw」。以前は「プレイヤーのYaw + PI」にしていて、Yaw=0(初期の向き)のときしか合わず、
	// 横を向いたプレイヤーでは正面側へ回っていた。
	float BehindPlayerYaw(float playerYaw)
	{
		return WrapAngle(PI - playerYaw);
	}
}

void ThirdPersonCamera::Init()
{
	m_camera.Init();
	m_camera.SetUP(Vector3(0.0f, 1.0f, 0.0f));
	Reset(Vector3(0.0f, 0.0f, 0.0f));
}

void ThirdPersonCamera::UpdateAnchor(const Vector3& playerPosition, float deltaSeconds)
{
	const Vector3 desired = playerPosition + Vector3(0.0f, m_targetHeight, 0.0f);
	if (!m_anchorInitialized || deltaSeconds <= 0.0f)
	{
		m_anchor = desired;
		m_anchorInitialized = true;
		return;
	}
	const float horizontal = ExpBlend(ANCHOR_HORIZONTAL_RATE, deltaSeconds);
	const float vertical = ExpBlend(ANCHOR_VERTICAL_RATE, deltaSeconds);
	m_anchor.x += (desired.x - m_anchor.x) * horizontal;
	m_anchor.z += (desired.z - m_anchor.z) * horizontal;
	m_anchor.y += (desired.y - m_anchor.y) * vertical;
}

void ThirdPersonCamera::UpdateDistance(float desiredDistance, float deltaSeconds)
{
	m_desiredDistance = desiredDistance;
	const float limited = m_collisionDistance >= 0.0f
		? std::min(desiredDistance, m_collisionDistance)
		: desiredDistance;
	if (deltaSeconds <= 0.0f)
	{
		m_currentDistance = limited;
		return;
	}
	const float rate = limited < m_currentDistance ? DISTANCE_PULL_IN_RATE : DISTANCE_RELEASE_RATE;
	m_currentDistance += (limited - m_currentDistance) * ExpBlend(rate, deltaSeconds);
}

Vector3 ThirdPersonCamera::BoomOffset(float distance) const
{
	const float cosPitch = std::cos(m_pitch);
	return Vector3(
		std::sin(m_cameraYaw) * cosPitch * distance,
		std::sin(m_pitch) * distance,
		-std::cos(m_cameraYaw) * cosPitch * distance);
}

void ThirdPersonCamera::Update(
	const Vector3& playerPosition,
	const Vector3& playerVelocity,
	const CameraLookInput& look,
	float deltaSeconds)
{
	// 壁に沿った回り込みはロックオン中だけ。次にロックオンしたとき前回の分から始まらないよう捨てる。
	m_wallSlideYaw = 0.0f;
	// --- 視点操作 ---
	// マウスの右 = 画面が右へ回る。カメラの向きは (-sin, cos) を見ているので、右へ回すにはYawを減らす。
	// マウスの下 = 見下ろす(カメラが上がる)。
	const float dt = std::max(deltaSeconds, 0.0f);
	const bool mouseMoved = look.active &&
		(std::abs(look.deltaX) > 0.0f || std::abs(look.deltaY) > 0.0f);
	const bool stickHeld = std::abs(look.stickX) > 0.0f || std::abs(look.stickY) > 0.0f;

	// 1) マウス: 動かした量をそのまま角度にする(時間に依存しないので、fpsが揺れても回る量は同じ)。
	float yawStep = 0.0f;
	float pitchStep = 0.0f;
	if (mouseMoved)
	{
		const float mouseYaw = -look.deltaX * m_mouseSensitivity;
		const float mousePitch = look.deltaY * m_mouseSensitivity;
		if (m_mouseSmoothSeconds > 0.0f)
		{
			m_pendingYaw += mouseYaw;
			m_pendingPitch += mousePitch;
		}
		else
		{
			yawStep += mouseYaw;
			pitchStep += mousePitch;
		}
	}
	else if (!look.active)
	{
		// カーソルを出したら、溜めていた分は捨てる(出した後に視点が動かないように)。
		m_pendingYaw = 0.0f;
		m_pendingPitch = 0.0f;
	}
	if (m_mouseSmoothSeconds > 0.0f &&
		(std::abs(m_pendingYaw) > 0.000001f || std::abs(m_pendingPitch) > 0.000001f))
	{
		// 慣らしを調整用に有効にした場合だけ通る。速さの上限は設けない
		// (上限があると使い切れない分が溜まり、手を止めた後も回り続けるため)。
		const float use = 1.0f - std::exp(-dt / m_mouseSmoothSeconds);
		yawStep += m_pendingYaw * use;
		pitchStep += m_pendingPitch * use;
		m_pendingYaw *= 1.0f - use;
		m_pendingPitch *= 1.0f - use;
		// 残りがわずかになったら捨てる。いつまでも微小に回り続けないように。
		if (std::abs(m_pendingYaw) < 0.0005f) m_pendingYaw = 0.0f;
		if (std::abs(m_pendingPitch) < 0.0005f) m_pendingPitch = 0.0f;
	}

	// 2) スティック: 倒した量を回る速さにする。マウスの右と同じく、右へ倒すと画面が右へ回る。
	//    上へ倒すと見上げる(カメラが下がる = Pitchが減る)。
	m_stickYawSpeed = ApproachStickSpeed(
		m_stickYawSpeed, -StickCurve(look.stickX) * STICK_YAW_SPEED, dt);
	m_stickPitchSpeed = ApproachStickSpeed(
		m_stickPitchSpeed, -StickCurve(look.stickY) * STICK_PITCH_SPEED, dt);
	yawStep += m_stickYawSpeed * dt;
	pitchStep += m_stickPitchSpeed * dt;

	if (mouseMoved || stickHeld)
	{
		m_timeSinceManualLook = 0.0f;
		m_autoFollowWeight = 0.0f;
		m_recenterActive = false;
	}
	else
	{
		m_timeSinceManualLook += dt;
	}

	if (yawStep != 0.0f || pitchStep != 0.0f)
	{
		m_cameraYaw = WrapAngle(m_cameraYaw + yawStep);
		const float newPitch = std::clamp(m_pitch + pitchStep, MIN_PITCH, MAX_PITCH);
		// 上下の端で止まったら、溜めた分とスティックの速さを捨てる
		// (残すと、手を離した後に逆向きへ動かしても端に張り付いたままになる)。
		if (newPitch != m_pitch + pitchStep)
		{
			m_pendingPitch = 0.0f;
			m_stickPitchSpeed = 0.0f;
		}
		m_pitch = newPitch;
	}

	if (m_recenterActive)
	{
		const float diff = WrapAngle(m_recenterYaw - m_cameraYaw);
		const float blend = ExpBlend(RECENTER_RATE, deltaSeconds);
		m_cameraYaw = WrapAngle(m_cameraYaw + diff * blend);
		m_pitch += (CAMERA_DEFAULT_PITCH - m_pitch) * blend;
		if (std::abs(diff) < 0.005f)
			m_recenterActive = false;
	}
	else if (m_autoFollowEnabled && m_timeSinceManualLook >= AUTO_FOLLOW_DELAY)
	{
		m_autoFollowWeight = std::min(1.0f, m_autoFollowWeight + dt / AUTO_FOLLOW_RAMP_SECONDS);
		// 効き始めは滑らかに(smoothstep)。
		const float weight = m_autoFollowWeight * m_autoFollowWeight * (3.0f - 2.0f * m_autoFollowWeight);
		// --- 自動追従 ---
		// 走っている向きの真後ろへ、横方向の速さに比例した速さで少しずつ回す。
		// 奥へまっすぐ走っているとき(横成分なし)はほとんど回らず、真横へ走るほど回る。
		const float speed = std::sqrt(
			playerVelocity.x * playerVelocity.x + playerVelocity.z * playerVelocity.z);
		if (speed > 5.0f)
		{
			// カメラが見ている向きを移動方向に合わせるYaw。
			const float desiredYaw = std::atan2(-playerVelocity.x, playerVelocity.z);
			const float diff = WrapAngle(desiredYaw - m_cameraYaw);
			if (std::abs(diff) < AUTO_FOLLOW_MAX_ANGLE)
			{
				const float lateral = std::abs(std::sin(diff));
				const float speedRatio = std::clamp(speed / AUTO_FOLLOW_FULL_SPEED, 0.0f, 1.0f);
				const float step = AUTO_FOLLOW_MAX_RATE * weight * lateral * speedRatio * dt;
				m_cameraYaw = WrapAngle(
					m_cameraYaw + std::copysign(std::min(std::abs(diff), step), diff));
			}
		}
	}

	UpdateAnchor(playerPosition, deltaSeconds);
	UpdateDistance(m_lookDistance, deltaSeconds);

	// ロックオンを外した直後は、注視点を敵寄りからプレイヤーへゆっくり戻す。
	if (deltaSeconds > 0.0f)
		m_lookOffset *= 1.0f - ExpBlend(LOOK_OFFSET_RATE, deltaSeconds);
	else
		m_lookOffset = Vector3(0.0f, 0.0f, 0.0f);

	ApplyView(m_anchor + BoomOffset(m_currentDistance), m_anchor + m_lookOffset, deltaSeconds);
}

void ThirdPersonCamera::UpdateLockOn(
	const Vector3& playerPosition,
	const Vector3& targetPosition,
	float targetHeight,
	float deltaSeconds)
{
	m_recenterActive = false;
	// ロックオン中はマウスで向きを変えないので、解除した直後に自動追従が走らないよう数え直す。
	// ロックオンする直前の操作の残り(溜めた回転・スティックの速さ)も、解除した瞬間に効かないよう捨てる。
	m_timeSinceManualLook = 0.0f;
	ClearLookState();

	UpdateAnchor(playerPosition, deltaSeconds);

	Vector3 flatDelta = targetPosition - playerPosition;
	flatDelta.y = 0.0f;
	const float targetDistance = flatDelta.Length();
	const Vector3 toTarget = targetDistance > 0.001f
		? flatDelta / targetDistance
		: Vector3(-std::sin(m_cameraYaw), 0.0f, std::cos(m_cameraYaw));

	// --- 距離 ---
	// 離れているほど引いて、自分と敵を同時に収める。
	const float framingDistance = std::clamp(
		targetDistance * 0.66f, 76.0f * m_framingScale, LOCK_ON_MAX_DISTANCE * m_framingScale);
	// 攻撃中の寄り(GameSceneがm_lookDistanceを少し縮める)をロックオンにも反映する。
	const float distanceScale = std::clamp(m_lookDistance / (70.0f * m_framingScale), 0.85f, 1.0f);
	// 近すぎるとプレイヤーのモデルが画面いっぱいになるので下限を設ける。
	const float desiredDistance = std::clamp(
		framingDistance * distanceScale, LOCK_ON_MIN_DISTANCE * m_framingScale, LOCK_ON_MAX_DISTANCE * m_framingScale);

	// --- 向き ---
	// カメラをプレイヤーの「敵と反対側」に置く。重なるほど近いときは向きを変えない。
	if (targetDistance > 2.0f)
	{
		const float behindYaw = std::atan2(-toTarget.x, toTarget.z);
		// 壁を背にしているときは、余地のある横の向きへずらす。
		const float slideTarget = FindWallSlideYaw(behindYaw, desiredDistance * std::cos(m_pitch));
		const float slideRate = std::abs(slideTarget) > std::abs(m_wallSlideYaw)
			? WALL_SLIDE_ENGAGE_RATE
			: WALL_SLIDE_RELEASE_RATE;
		m_wallSlideYaw += (slideTarget - m_wallSlideYaw) * ExpBlend(slideRate, deltaSeconds);
		const float desiredYaw = WrapAngle(behindYaw + m_wallSlideYaw);
		const float diff = WrapAngle(desiredYaw - m_cameraYaw);
		float step = diff * ExpBlend(LOCK_ON_YAW_RATE, deltaSeconds);
		const float maxStep = LOCK_ON_MAX_YAW_SPEED * std::max(deltaSeconds, 0.0f);
		step = std::clamp(step, -maxStep, maxStep);
		m_cameraYaw = WrapAngle(m_cameraYaw + step);
	}

	// --- 高さ(見上げる角度) ---
	// 敵の胴の高さがプレイヤーの注視点より高く、近いほど、カメラを下げて見上げる。
	// 大型の敵に密着したとき、見下ろしたままだと頭(噛みつき・叩き付けの予兆)が画面の上へ切れる。
	const Vector3 targetFocus = targetPosition + Vector3(0.0f, targetHeight * 0.5f, 0.0f);
	const float elevation = std::atan2(
		targetFocus.y - m_anchor.y,
		std::max(targetDistance, 1.0f));
	const float desiredPitch = std::clamp(CAMERA_DEFAULT_PITCH - elevation * 0.75f, 0.04f, 0.55f);
	m_pitch += (desiredPitch - m_pitch) * ExpBlend(LOCK_ON_PITCH_RATE, deltaSeconds);

	UpdateDistance(desiredDistance, deltaSeconds);

	// --- 注視点 ---
	// プレイヤーと敵の間へ寄せる。切り替えの瞬間に画面が跳ねないよう、追従点からの「ずれ量」を補間する。
	// 注視点の位置そのものを補間すると、走っている間は注視点が遅れてついて来て画面が揺れる。
	// 壁に沿って横へ回り込んでいる間は、寄せる量を減らす。横から見ているのに敵の方へ大きく寄せると、
	// プレイヤーが画面の端から外れる(敵が遠いと特に。自動プレイの画面で確認)。
	const float slideLookScale = 0.35f + 0.65f * std::max(std::cos(m_wallSlideYaw), 0.0f);
	// 壁や敵の体でカメラが引き寄せられている間も減らす。近い位置から敵の方を見ると、
	// プレイヤーが画面の下へ外れる(回避で壁へ飛び込み、回り込みが追いつくまでの間に起きていた)。
	const float pulledIn = std::clamp(m_currentDistance / std::max(desiredDistance, 1.0f), 0.0f, 1.0f);
	const float pullLookScale = 0.3f + 0.7f * pulledIn;
	const Vector3 desiredOffset = (targetFocus - m_anchor) *
		(LOCK_ON_LOOK_TOWARD_TARGET * slideLookScale * pullLookScale);
	if (deltaSeconds > 0.0f)
		m_lookOffset += (desiredOffset - m_lookOffset) * ExpBlend(LOOK_OFFSET_RATE, deltaSeconds);
	else
		m_lookOffset = desiredOffset;

	// カメラはプレイヤー(追従点)を基準に置く。注視点を基準にすると、敵と離れたときに
	// カメラがプレイヤーより前へ出てしまう。わずかに肩越しへずらす。
	const Vector3 shoulder(-toTarget.z, 0.0f, toTarget.x);
	const Vector3 cameraPosition = m_anchor + BoomOffset(m_currentDistance) +
		shoulder * LOCK_ON_SHOULDER_OFFSET;
	ApplyView(cameraPosition, m_anchor + m_lookOffset, deltaSeconds);
}

void ThirdPersonCamera::Draw()
{
	m_camera.Draw();
}

void ThirdPersonCamera::Reset(const Vector3& playerPosition, float playerYaw)
{
	m_battleTransitionActive = false;
	m_battleTransitionTime = 0.0f;
	// 初期方向はプレイヤーの後方。
	m_cameraYaw = BehindPlayerYaw(playerYaw);
	m_pitch = CAMERA_DEFAULT_PITCH;
	m_lookDistance = 70.0f * m_framingScale;
	m_desiredDistance = m_lookDistance;
	m_currentDistance = m_lookDistance;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f * m_framingTargetScale;
	m_timeSinceManualLook = 10.0f;
	m_recenterActive = false;
	ClearLookState();
	m_wallSlideYaw = 0.0f;
	// リセット時は揺れと補間の状態も初期化する。
	// 残っていると再戦の1フレーム目に前回の揺れが乗る。
	m_shakeStrength = 0.0f;
	m_shakeTime = 0.0f;
	m_shakeDuration = 0.0f;
	m_anchorInitialized = false;
	UpdateAnchor(playerPosition, 0.0f);
	m_lookOffset = Vector3(0.0f, 0.0f, 0.0f);
	ApplyView(m_anchor + BoomOffset(m_currentDistance), m_anchor, 0.0f);
}

void ThirdPersonCamera::BeginBattleTransition(
	const Vector3& playerPosition,
	float playerYaw)
{
	// 直前のカットシーン構図を開始点として保存する。Reset()で即時に背後へ
	// 飛ばすと、咆哮の終わりに画面が跳ねるため、戦闘カメラへ補間する。
	m_battleTransitionStartPosition = m_camera.GetPosition();
	m_battleTransitionStartLookat = m_camera.GetLookat();
	m_battleTransitionTime = 0.0f;
	m_battleTransitionActive = true;

	m_cameraYaw = BehindPlayerYaw(playerYaw);
	m_pitch = CAMERA_DEFAULT_PITCH;
	m_lookDistance = 70.0f * m_framingScale;
	m_desiredDistance = m_lookDistance;
	m_currentDistance = m_lookDistance;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f * m_framingTargetScale;
	m_timeSinceManualLook = 10.0f;
	m_recenterActive = false;
	ClearLookState();
	m_wallSlideYaw = 0.0f;
	m_anchorInitialized = false;
	UpdateAnchor(playerPosition, 0.0f);
	m_lookOffset = Vector3(0.0f, 0.0f, 0.0f);
}

float ThirdPersonCamera::HorizontalRoom(float yaw) const
{
	if (m_wallSafeRadius <= 0.0f)
		return std::numeric_limits<float>::max();
	// 追従点からカメラ側へ伸ばした水平の半直線と、壁の安全円の交点までの距離。
	// 追従点が円の外へ出ていても必ず内側になるよう、円を追従点より少し外まで広げる。
	const float anchorRadius = std::sqrt(m_anchor.x * m_anchor.x + m_anchor.z * m_anchor.z);
	const float safeRadius = std::max(m_wallSafeRadius, anchorRadius + WALL_ROOM_BEYOND_ANCHOR);
	const float dirX = std::sin(yaw);
	const float dirZ = -std::cos(yaw);
	const float dot = m_anchor.x * dirX + m_anchor.z * dirZ;
	const float constant = anchorRadius * anchorRadius - safeRadius * safeRadius;
	return -dot + std::sqrt(std::max(dot * dot - constant, 0.0f));
}

float ThirdPersonCamera::FindWallSlideYaw(float behindYaw, float wantedHorizontal) const
{
	if (m_wallSafeRadius <= 0.0f)
		return 0.0f;
	const float needed = wantedHorizontal * WALL_SLIDE_ROOM_RATIO;
	if (HorizontalRoom(behindYaw) >= needed)
		return 0.0f;
	// 今回り込んでいる側を優先する(左右を行き来すると、カメラが敵の向こうを半周してしまう)。
	// まだ回り込んでいなければ、少し回したときに余地が多い側から探す。
	float preferred = 1.0f;
	if (std::abs(m_wallSlideYaw) > 0.05f)
		preferred = m_wallSlideYaw > 0.0f ? 1.0f : -1.0f;
	else if (HorizontalRoom(behindYaw - 0.5f) > HorizontalRoom(behindYaw + 0.5f))
		preferred = -1.0f;

	// 優先する側を端まで探してから、反対側を探す。角度の小さい順に左右を交互に探すと、
	// 反対側に少しでも近い解があるたびに乗り換え、カメラが壁の方向を横切って振れていた(自動プレイのログで確認)。
	float bestOffset = 0.0f;
	float bestRoom = HorizontalRoom(behindYaw);
	for (float sign : { preferred, -preferred })
	{
		for (float offset = WALL_SLIDE_SEARCH_STEP; offset <= WALL_SLIDE_MAX + 0.001f; offset += WALL_SLIDE_SEARCH_STEP)
		{
			const float room = HorizontalRoom(behindYaw + sign * offset);
			if (room >= needed)
				return sign * offset;
			if (room > bestRoom + 0.5f)
			{
				bestRoom = room;
				bestOffset = sign * offset;
			}
		}
	}
	// どこにも十分な余地が無ければ、一番余地のある向き(残りは距離を縮めて収める)。
	return bestOffset;
}

void ThirdPersonCamera::ClearLookState()
{
	m_pendingYaw = 0.0f;
	m_pendingPitch = 0.0f;
	m_stickYawSpeed = 0.0f;
	m_stickPitchSpeed = 0.0f;
	m_autoFollowWeight = 0.0f;
}

void ThirdPersonCamera::RequestRecenter(float playerYaw)
{
	m_recenterYaw = BehindPlayerYaw(playerYaw);
	m_recenterActive = true;
}

void ThirdPersonCamera::SetLookDistance(float distance)
{
	m_lookDistance = std::clamp(distance, 3.0f, 100.0f);
}

void ThirdPersonCamera::SetCollisionDistance(float distance)
{
	// 以前は通常時の距離(m_lookDistance)で上限を切っていたため、ロックオン中(最大112)に
	// 衝突すると、ぶつかっていない分まで70へ縮んでいた。
	m_collisionDistance = distance < 0.0f ? -1.0f : std::max(distance, 3.0f);
}

void ThirdPersonCamera::TriggerShake(float strength, float duration)
{
	if (strength <= 0.0f || duration <= 0.0f)
		return;
	// 連続ヒットで揺れが累積して画面が破綻しないよう、強い方だけを採用する。
	if (strength >= m_shakeStrength)
	{
		m_shakeStrength = strength;
		m_shakeDuration = duration;
		m_shakeTime = 0.0f;
	}
}

void ThirdPersonCamera::ApplyView(
	const Vector3& position,
	const Vector3& lookat,
	float deltaSeconds)
{
	Vector3 basePosition = position;
	Vector3 baseLookat = lookat;
	if (m_battleTransitionActive)
	{
		m_battleTransitionTime += std::max(deltaSeconds, 0.0f);
		const float linearT = std::clamp(
			m_battleTransitionTime / BATTLE_TRANSITION_SECONDS,
			0.0f,
			1.0f);
		// smoothstepで開始・終了の速度を落とし、演出から操作カメラへの
		// 切り替えを映像的に見せる。
		const float t = linearT * linearT * (3.0f - 2.0f * linearT);
		basePosition = Vector3::Lerp(m_battleTransitionStartPosition, position, t);
		baseLookat = Vector3::Lerp(m_battleTransitionStartLookat, lookat, t);
		if (linearT >= 1.0f)
			m_battleTransitionActive = false;
	}

	// 命中時の揺れを最後に足す。
	Vector3 shakeOffset(0.0f, 0.0f, 0.0f);
	if (m_shakeStrength > 0.0f && m_shakeDuration > 0.0f)
	{
		m_shakeTime += std::max(deltaSeconds, 0.0f);
		const float progress = std::clamp(m_shakeTime / m_shakeDuration, 0.0f, 1.0f);
		if (progress >= 1.0f)
		{
			m_shakeStrength = 0.0f;
		}
		else
		{
			// 時間とともに二次で減衰させ、最初の一撃を強く、収まりを速くする。
			const float decay = (1.0f - progress) * (1.0f - progress);
			const float amplitude = m_shakeStrength * decay;
			// 軸ごとに周波数をずらし、単純な往復ではなく不規則な揺れに見せる。
			const float t = m_shakeTime;
			shakeOffset = Vector3(
				std::sinf(t * 62.0f) * amplitude,
				std::sinf(t * 48.0f + 1.7f) * amplitude * 0.8f,
				std::sinf(t * 55.0f + 3.1f) * amplitude * 0.6f);
		}
	}

	Vector3 finalPosition = basePosition + shakeOffset;
	if (m_boundaryRadius > 0.0f)
	{
		// 中心へ向かって押し戻す。入力に対して連続なので、円に触れても画面は跳ねない。
		const float radius = std::sqrt(finalPosition.x * finalPosition.x + finalPosition.z * finalPosition.z);
		if (radius > m_boundaryRadius)
		{
			const float scale = m_boundaryRadius / radius;
			finalPosition.x *= scale;
			finalPosition.z *= scale;
		}
	}
	m_camera.SetPosition(finalPosition);
	// 注視点も少しだけ揺らす。位置だけ動かすと視線が固定されたままで、
	// 揺れというより平行移動に見えてしまう。
	m_camera.SetLookat(baseLookat + shakeOffset * 0.35f);
}
