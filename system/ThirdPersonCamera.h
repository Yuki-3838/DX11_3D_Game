#pragma once

#include "camera.h"

class ThirdPersonCamera
{
public:
	void Init();
	void Update(const Vector3& playerPosition, float playerYaw, bool viewportHovered);
	void Draw();
	void Reset(const Vector3& playerPosition, float playerYaw = 0.0f);

	// プレイヤーの背後を基準にしたワールドYawを返す。
	float GetYaw() const { return m_playerYaw + PI + m_yaw; }
	float GetPitch() const { return m_pitch; }
	float GetLookDistance() const { return m_lookDistance; }
	float GetTargetHeight() const { return m_targetHeight; }
	float GetMouseSensitivity() const { return m_mouseSensitivity; }
	bool IsMouseLookEnabled() const { return m_mouseLookEnabled; }
	bool IsOrbiting() const { return m_orbiting; }

	void SetMouseLookEnabled(bool enabled) { m_mouseLookEnabled = enabled; }
	void SetTargetHeight(float height) { m_targetHeight = height; }
	void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
	void SetLookDistance(float distance);

private:
	void ApplyTransform(const Vector3& playerPosition);

	Camera m_camera;
	float m_yaw = 0.0f;
	float m_playerYaw = 0.0f;
	float m_pitch = 0.2f;
	float m_mouseSensitivity = 0.004f;
	float m_lookDistance = 50.0f;
	float m_targetHeight = 12.0f;
	// 通常はプレイヤーの真後ろ。右ドラッグ時だけ手動で周回する。
	bool m_mouseLookEnabled = false;
	bool m_orbiting = false;
	int m_lastMouseX = 0;
	int m_lastMouseY = 0;
	bool m_mouseInitialized = false;
};
