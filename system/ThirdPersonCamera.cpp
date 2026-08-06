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

void ThirdPersonCamera::Update(const Vector3& playerPosition, float playerYaw, bool viewportHovered)
{
	(void)viewportHovered;
	m_playerYaw = playerYaw;
	// Keep the camera directly behind the player's facing direction.
	m_yaw = 0.0f;
	m_orbiting = false;
	ApplyTransform(playerPosition);
}

void ThirdPersonCamera::Draw()
{
	m_camera.Draw();
}

void ThirdPersonCamera::Reset(const Vector3& playerPosition, float playerYaw)
{
	m_yaw = 0.0f;
	m_playerYaw = playerYaw;
	// Keep the initial rear direction, but never rotate the camera when the
	// player turns or moves. The player position is still followed for framing.
	m_cameraYaw = playerYaw + PI;
	m_pitch = 0.30f;
	m_mouseSensitivity = 0.004f;
	m_lookDistance = 70.0f;
	m_targetHeight = 25.0f;
	m_orbiting = false;
	ApplyTransform(playerPosition);
}

void ThirdPersonCamera::SetLookDistance(float distance)
{
	m_lookDistance = std::clamp(distance, 3.0f, 100.0f);
}

void ThirdPersonCamera::ApplyTransform(const Vector3& playerPosition)
{
	const Vector3 target = playerPosition + Vector3(0.0f, m_targetHeight, 0.0f);
	// プレイヤーの正面とは反対側をカメラの初期位置にする。
	const float cameraYaw = m_cameraYaw + m_yaw;
	const float cosPitch = std::cos(m_pitch);
	const Vector3 offset(
		std::sin(cameraYaw) * cosPitch * m_lookDistance,
		std::sin(m_pitch) * m_lookDistance,
		-std::cos(cameraYaw) * cosPitch * m_lookDistance);

	m_camera.SetPosition(target + offset);
	m_camera.SetLookat(target);
}
