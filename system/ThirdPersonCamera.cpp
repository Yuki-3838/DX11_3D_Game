#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>

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
	constexpr float AUTO_FOLLOW_DELAY = 0.8f;
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
	// --- 視点操作 ---
	// マウスの右 = 画面が右へ回る。カメラの向きは (-sin, cos) を見ているので、右へ回すにはYawを減らす。
	// マウスの下 = 見下ろす(カメラが上がる)。
	const bool manualLook = look.active &&
		(std::abs(look.deltaX) > 0.0f || std::abs(look.deltaY) > 0.0f);
	if (manualLook)
	{
		m_cameraYaw = WrapAngle(m_cameraYaw - look.deltaX * m_mouseSensitivity);
		m_pitch = std::clamp(m_pitch + look.deltaY * m_mouseSensitivity, MIN_PITCH, MAX_PITCH);
		m_timeSinceManualLook = 0.0f;
		m_recenterActive = false;
	}
	else
	{
		m_timeSinceManualLook += std::max(deltaSeconds, 0.0f);
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
				const float step = AUTO_FOLLOW_MAX_RATE * lateral * speedRatio *
					std::max(deltaSeconds, 0.0f);
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
	m_timeSinceManualLook = 0.0f;

	UpdateAnchor(playerPosition, deltaSeconds);

	Vector3 flatDelta = targetPosition - playerPosition;
	flatDelta.y = 0.0f;
	const float targetDistance = flatDelta.Length();
	const Vector3 toTarget = targetDistance > 0.001f
		? flatDelta / targetDistance
		: Vector3(-std::sin(m_cameraYaw), 0.0f, std::cos(m_cameraYaw));

	// --- 向き ---
	// カメラをプレイヤーの「敵と反対側」に置く。重なるほど近いときは向きを変えない。
	if (targetDistance > 2.0f)
	{
		const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
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

	// --- 距離 ---
	// 離れているほど引いて、自分と敵を同時に収める。
	const float framingDistance = std::clamp(
		targetDistance * 0.66f, 76.0f, LOCK_ON_MAX_DISTANCE);
	// 攻撃中の寄り(GameSceneがm_lookDistanceを少し縮める)をロックオンにも反映する。
	const float distanceScale = std::clamp(m_lookDistance / 70.0f, 0.85f, 1.0f);
	// 近すぎるとプレイヤーのモデルが画面いっぱいになるので下限を設ける。
	const float desiredDistance = std::clamp(
		framingDistance * distanceScale, LOCK_ON_MIN_DISTANCE, LOCK_ON_MAX_DISTANCE);
	UpdateDistance(desiredDistance, deltaSeconds);

	// --- 注視点 ---
	// プレイヤーと敵の間へ寄せる。切り替えの瞬間に画面が跳ねないよう、追従点からの「ずれ量」を補間する。
	// 注視点の位置そのものを補間すると、走っている間は注視点が遅れてついて来て画面が揺れる。
	const Vector3 desiredOffset = (targetFocus - m_anchor) * LOCK_ON_LOOK_TOWARD_TARGET;
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
	m_lookDistance = 70.0f;
	m_desiredDistance = m_lookDistance;
	m_currentDistance = m_lookDistance;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f;
	m_timeSinceManualLook = 10.0f;
	m_recenterActive = false;
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
	m_lookDistance = 70.0f;
	m_desiredDistance = m_lookDistance;
	m_currentDistance = m_lookDistance;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f;
	m_timeSinceManualLook = 10.0f;
	m_recenterActive = false;
	m_anchorInitialized = false;
	UpdateAnchor(playerPosition, 0.0f);
	m_lookOffset = Vector3(0.0f, 0.0f, 0.0f);
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

	m_camera.SetPosition(basePosition + shakeOffset);
	// 注視点も少しだけ揺らす。位置だけ動かすと視線が固定されたままで、
	// 揺れというより平行移動に見えてしまう。
	m_camera.SetLookat(baseLookat + shakeOffset * 0.35f);
}
