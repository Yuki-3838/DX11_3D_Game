#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>

#include "Inputmanager.h"
#include "imgui/imgui.h"

void ThirdPersonCamera::Init()
{
	m_camera.Init();
	m_camera.SetUP(Vector3(0.0f, 1.0f, 0.0f));
	Reset(Vector3(0.0f, 0.0f, 0.0f));
}

void ThirdPersonCamera::Update(
	const Vector3& playerPosition,
	float playerYaw,
	bool viewportHovered,
	float deltaSeconds)
{
	(void)viewportHovered;
	m_playerYaw = playerYaw;
	if (m_mouseLookEnabled && viewportHovered)
	{
		auto& input = CInputManager::GetInstance();
		const bool orbiting = input.IsMousePressed(CInputManager::MOUSE_CENTER);
		if (orbiting)
		{
			const int mouseX = input.GetMouseX();
			const int mouseY = input.GetMouseY();
			if (!m_mouseInitialized)
			{
				m_lastMouseX = mouseX;
				m_lastMouseY = mouseY;
				m_mouseInitialized = true;
			}
			else
			{
				m_yaw += static_cast<float>(mouseX - m_lastMouseX) * m_mouseSensitivity;
				m_pitch = std::clamp(
					m_pitch - static_cast<float>(mouseY - m_lastMouseY) * m_mouseSensitivity,
					-0.15f,
					0.72f);
				m_lastMouseX = mouseX;
				m_lastMouseY = mouseY;
			}
			m_orbiting = true;
		}
		else
		{
			m_mouseInitialized = false;
			m_orbiting = false;
		}
	}
	else
	{
		m_mouseInitialized = false;
		m_orbiting = false;
	}
	ApplyTransform(playerPosition, deltaSeconds);
}

void ThirdPersonCamera::UpdateLockOn(
	const Vector3& playerPosition,
	const Vector3& targetPosition,
	bool viewportHovered,
	float deltaSeconds)
{
	(void)viewportHovered;
	Vector3 flatDelta = targetPosition - playerPosition;
	flatDelta.y = 0.0f;
	const float targetDistance = std::max(flatDelta.Length(), 0.001f);
	const Vector3 toTarget = flatDelta / targetDistance;
	const Vector3 behindTarget = -toTarget;

	// ロックオン対象だけに寄りすぎないようにカメラを少し引き、
	// プレイヤーの全身と敵を同時に確認できる距離を確保する。
	// Elden Ring寄りの「自キャラの操作感」と、Monster Hunter寄りの
	// 「大型の相手を画面から逃がさない」構図の中間を狙った値。
	const float framingDistance = std::clamp(targetDistance * 0.66f, 76.0f, 112.0f);
	// GameSceneが攻撃中だけm_lookDistanceを縮めるため、ロックオン構図にも
	// 同じ寄りを反映する。これを別管理にすると通常時と攻撃時で距離の責務が
	// 分かれ、片方だけズームしない不整合が起きる。
	const float distanceScale = std::clamp(m_lookDistance / 70.0f, 0.60f, 1.0f);
	// 敵に密着したときも攻撃ズーム率だけを掛けると、
	// 76 * 0.62 = 47.12単位までカメラが寄ってしまう。
	// プレイヤーのモデル幅に対して近すぎ、モデルが巨大化したように見えるため、
	// ロックオン戦闘では最低距離を設ける。遠距離時の攻撃ズームはこの制限に
	// 掛からない範囲で従来どおり残す。
	constexpr float MIN_LOCK_ON_CAMERA_DISTANCE = 64.0f;
	const float desiredDistance = std::clamp(
		std::max(framingDistance * distanceScale, MIN_LOCK_ON_CAMERA_DISTANCE),
		MIN_LOCK_ON_CAMERA_DISTANCE,
		112.0f);
	const float cameraDistance = m_collisionDistance >= 0.0f
		? std::min(desiredDistance, m_collisionDistance)
		: desiredDistance;
	// 注視点は両者のほぼ中間より少しプレイヤー側へ寄せる。
	// 敵だけを中央に置くより、回避方向と攻撃の届く距離を同時に読みやすい。
	const Vector3 midpoint = playerPosition +
		(targetPosition - playerPosition) * 0.46f + Vector3(0.0f, 9.0f, 0.0f);
	const float horizontalDistance = cameraDistance * std::cos(m_pitch);
	// カメラはプレイヤーの後方側に固定する。
	// 中間点に置くと敵との距離が大きいときにプレイヤーより前へ出て、
	// プレイヤーがニアクリップ面の後ろに隠れてしまうためである。
	Vector3 cameraPosition = playerPosition +
		Vector3(
			behindTarget.x * horizontalDistance,
			midpoint.y + cameraDistance * std::sin(m_pitch),
			behindTarget.z * horizontalDistance);
	// 完全な正面中央ではなく、わずかに肩越しへずらす。
	// プレイヤーの攻撃方向を残しながら敵の全身を読みやすくする。
	const Vector3 shoulder(-toTarget.z, 0.0f, toTarget.x);
	cameraPosition += shoulder * 6.0f;

	m_cameraYaw = std::atan2(behindTarget.x, -behindTarget.z);
	m_yaw = 0.0f;
	m_orbiting = false;
	m_mouseInitialized = false;
	ApplyView(cameraPosition, midpoint, deltaSeconds);
}

void ThirdPersonCamera::Draw()
{
	m_camera.Draw();
}

void ThirdPersonCamera::Reset(const Vector3& playerPosition, float playerYaw)
{
	m_battleTransitionActive = false;
	m_battleTransitionTime = 0.0f;
	m_yaw = 0.0f;
	m_playerYaw = playerYaw;
	// 初期方向はプレイヤーの後方に設定するが、プレイヤーの旋回や移動で
	// カメラの向きは勝手に回さない。位置だけを追従して画面構図を保つ。
	m_cameraYaw = playerYaw + PI;
	m_pitch = 0.30f;
	m_mouseSensitivity = 0.004f;
	m_lookDistance = 70.0f;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f;
	m_orbiting = false;
	m_mouseInitialized = false;
	// リセット時は揺れと補間の状態も初期化する。
	// 残っていると再戦の1フレーム目に前回の揺れが乗る。
	m_shakeStrength = 0.0f;
	m_shakeTime = 0.0f;
	m_shakeDuration = 0.0f;
	m_baseInitialized = false;
	ApplyTransform(playerPosition);
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

	m_yaw = 0.0f;
	m_playerYaw = playerYaw;
	m_cameraYaw = playerYaw + PI;
	m_pitch = 0.30f;
	m_mouseSensitivity = 0.004f;
	m_lookDistance = 70.0f;
	m_collisionDistance = -1.0f;
	m_targetHeight = 25.0f;
	m_orbiting = false;
	m_mouseInitialized = false;
	(void)playerPosition;
}

void ThirdPersonCamera::SetLookDistance(float distance)
{
	m_lookDistance = std::clamp(distance, 3.0f, 100.0f);
}

void ThirdPersonCamera::SetCollisionDistance(float distance)
{
	m_collisionDistance = distance < 0.0f
		? -1.0f
		: std::clamp(distance, 3.0f, m_lookDistance);
}

void ThirdPersonCamera::ApplyTransform(
	const Vector3& playerPosition,
	float deltaSeconds)
{
	const Vector3 target = playerPosition + Vector3(0.0f, m_targetHeight, 0.0f);
	// プレイヤーの正面とは反対側をカメラの初期位置にする。
	const float cameraYaw = m_cameraYaw + m_yaw;
	const float cosPitch = std::cos(m_pitch);
	const float lookDistance = m_collisionDistance >= 0.0f
		? m_collisionDistance
		: m_lookDistance;
	const Vector3 offset(
		std::sin(cameraYaw) * cosPitch * lookDistance,
		std::sin(m_pitch) * lookDistance,
		-std::cos(cameraYaw) * cosPitch * lookDistance);

	ApplyView(target + offset, target, deltaSeconds);
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
	const Vector3& desiredPosition,
	const Vector3& desiredLookat,
	float deltaSeconds)
{
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
		// 遷移中の位置をそのまま基準位置にしておく。
		// これをしないと遷移終了時に、遷移前の古い基準位置から補間し直して跳ねる。
		m_basePosition = Vector3::Lerp(
			m_battleTransitionStartPosition, desiredPosition, t);
		m_baseLookat = Vector3::Lerp(
			m_battleTransitionStartLookat, desiredLookat, t);
		m_baseInitialized = true;
		m_camera.SetPosition(m_basePosition);
		m_camera.SetLookat(m_baseLookat);
		if (linearT >= 1.0f)
			m_battleTransitionActive = false;
		return;
	}

	if (!m_baseInitialized)
	{
		m_basePosition = desiredPosition;
		m_baseLookat = desiredLookat;
		m_baseInitialized = true;
	}

	if (deltaSeconds <= 0.0f)
	{
		m_basePosition = desiredPosition;
		m_baseLookat = desiredLookat;
	}
	else
	{
		// 毎フレームの即時配置ではなく指数補間にする。敵の移動・ロックオン
		// 対象の位置変化を追いながら、カメラの首振りを抑える。
		// 補間は揺れを含まない位置で行う。揺れた位置を補間元にすると
		// 揺れが減衰せず残り続けてしまう。
		const float positionBlend = 1.0f - std::exp(-11.0f * deltaSeconds);
		const float lookatBlend = 1.0f - std::exp(-14.0f * deltaSeconds);
		m_basePosition = Vector3::Lerp(m_basePosition, desiredPosition, positionBlend);
		m_baseLookat = Vector3::Lerp(m_baseLookat, desiredLookat, lookatBlend);
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

	m_camera.SetPosition(m_basePosition + shakeOffset);
	// 注視点も少しだけ揺らす。位置だけ動かすと視線が固定されたままで、
	// 揺れというより平行移動に見えてしまう。
	m_camera.SetLookat(m_baseLookat + shakeOffset * 0.35f);
}
