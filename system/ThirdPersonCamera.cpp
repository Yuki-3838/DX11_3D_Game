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

void ThirdPersonCamera::Update(const Vector3& playerPosition, bool viewportHovered)
{
	auto& input = CInputManager::GetInstance();
	const bool rightMouse = input.IsMousePressed(CInputManager::MOUSE_RIGHT);
	const bool canUseMouse = viewportHovered || !ImGui::GetIO().WantCaptureMouse;
	const bool mouseLook = m_mouseLookEnabled && canUseMouse;
	const bool rotating = rightMouse && canUseMouse;
	m_orbiting = rotating;

	const int mouseX = input.GetMouseX();
	const int mouseY = input.GetMouseY();
	if (!m_mouseInitialized)
	{
		m_lastMouseX = mouseX;
		m_lastMouseY = mouseY;
		m_mouseInitialized = true;
	}

	const ImVec2 imguiMouseDelta = ImGui::GetIO().MouseDelta;
	float mouseDeltaX = imguiMouseDelta.x;
	float mouseDeltaY = imguiMouseDelta.y;
	if (std::abs(mouseDeltaX) < 0.001f && std::abs(mouseDeltaY) < 0.001f)
	{
		mouseDeltaX = static_cast<float>(mouseX - m_lastMouseX);
		mouseDeltaY = static_cast<float>(mouseY - m_lastMouseY);
	}
	m_lastMouseX = mouseX;
	m_lastMouseY = mouseY;

	if (mouseLook || rotating)
	{
		m_yaw += mouseDeltaX * m_mouseSensitivity;
		m_pitch -= mouseDeltaY * m_mouseSensitivity;
		m_pitch = std::clamp(m_pitch, -1.5f, 1.5f);
	}

	const float wheel = static_cast<float>(input.GetMouseWheelDelta());
	if (canUseMouse && wheel != 0.0f)
	{
		SetLookDistance(m_lookDistance - wheel * 0.02f);
	}

	ApplyTransform(playerPosition);
}

void ThirdPersonCamera::Draw()
{
	m_camera.Draw();
}

void ThirdPersonCamera::Reset(const Vector3& playerPosition)
{
	m_yaw = 0.0f;
	m_pitch = 0.1f;
	m_mouseSensitivity = 0.004f;
	m_lookDistance = 25.0f;
	m_targetHeight = 8.0f;
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
	const float cosPitch = std::cos(m_pitch);
	const Vector3 offset(
		std::sin(m_yaw) * cosPitch * m_lookDistance,
		std::sin(m_pitch) * m_lookDistance,
		-std::cos(m_yaw) * cosPitch * m_lookDistance);

	m_camera.SetPosition(target + offset);
	m_camera.SetLookat(target);
}
