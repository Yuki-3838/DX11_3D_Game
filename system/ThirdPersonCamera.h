#pragma once

#include "camera.h"

class ThirdPersonCamera
{
public:
	void Init();
	void Update(
		const Vector3& playerPosition,
		float playerYaw,
		bool viewportHovered,
		float deltaSeconds = 0.0f);
	void UpdateLockOn(
		const Vector3& playerPosition,
		const Vector3& targetPosition,
		bool viewportHovered,
		float deltaSeconds = 0.0f);
	void Draw();
	void Reset(const Vector3& playerPosition, float playerYaw = 0.0f);
	// 登場演出の現在カメラを保持したまま、戦闘用の背後カメラへ短時間で遷移する。
	void BeginBattleTransition(const Vector3& playerPosition, float playerYaw);

	/**
	 * @brief 命中の手応えとしてカメラを短く揺らす。
	 * @param strength 揺れ幅(ワールド単位)。
	 * @param duration 揺れが収まるまでの時間(秒)。
	 * @details 既に揺れている場合は強い方を採用する。連続ヒットで
	 *          揺れが累積して画面が破綻するのを防ぐ。
	 */
	void TriggerShake(float strength, float duration);

	// プレイヤーの背後を基準にしたワールドYawを返す。
	// カメラの向きはプレイヤーの向きとは独立して管理する。
	float GetYaw() const { return m_cameraYaw + m_yaw; }
	float GetPitch() const { return m_pitch; }
	float GetLookDistance() const { return m_lookDistance; }
	float GetTargetHeight() const { return m_targetHeight; }
	float GetMouseSensitivity() const { return m_mouseSensitivity; }
	Matrix4x4 GetViewMatrix() const { return m_camera.GetViewMatrix(); }
	Matrix4x4 GetProjMatrix() const { return m_camera.GetProjMatrix(); }
	// 遮蔽判定のため、実際に使われているカメラ位置と注視点を参照する。
	// ロックオン時は注視点をプレイヤーと敵の中間へ寄せているため、
	// 呼び出し側で理想的なブーム位置を再計算すると実際の構図とずれる。
	Vector3 GetPosition() const { return m_camera.GetPosition(); }
	Vector3 GetLookat() const { return m_camera.GetLookat(); }
	bool IsMouseLookEnabled() const { return m_mouseLookEnabled; }
	bool IsOrbiting() const { return m_orbiting; }

	void SetMouseLookEnabled(bool enabled) { m_mouseLookEnabled = enabled; }
	void SetTargetHeight(float height) { m_targetHeight = height; }
	void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
	void SetLookDistance(float distance);
	// 闘技場の壁がプレイヤーとカメラの間に入る場合はカメラを引き寄せる。
	// 負の値を指定すると通常の距離に戻す。
	void SetCollisionDistance(float distance);
	// 操作を受け付けない短い登場演出で、カメラ位置と注視点を直接指定する。
	void SetCinematicView(const Vector3& cameraPosition, const Vector3& lookAt)
	{
		m_camera.SetPosition(cameraPosition);
		m_camera.SetLookat(lookAt);
		m_battleTransitionActive = false;
	}

private:
	void ApplyTransform(const Vector3& playerPosition, float deltaSeconds = 0.0f);
	void ApplyView(
		const Vector3& desiredPosition,
		const Vector3& desiredLookat,
		float deltaSeconds);

	Camera m_camera;
	float m_yaw = 0.0f;
	float m_playerYaw = 0.0f;
	float m_cameraYaw = PI;
	// 少し高い位置から引いた、戦闘全体を見渡しやすい構図にする。
	float m_pitch = 0.30f;
	float m_mouseSensitivity = 0.004f;
	float m_lookDistance = 70.0f;
	float m_collisionDistance = -1.0f;
	float m_targetHeight = 25.0f;
	// カットシーンから戦闘へ切り替わるときの短いカメラ遷移。
	bool m_battleTransitionActive = false;
	float m_battleTransitionTime = 0.0f;
	static constexpr float BATTLE_TRANSITION_SECONDS = 0.36f;
	Vector3 m_battleTransitionStartPosition{ 0.0f, 50.0f, -100.0f };
	Vector3 m_battleTransitionStartLookat{ 0.0f, 0.0f, 0.0f };
	// 通常はプレイヤーの真後ろ。中央ドラッグ時だけ手動で周回する。
	// 命中時のカメラ揺れ。
	// 揺れを含まない「本来のカメラ位置」を別に持つ。揺れた位置を次フレームの
	// 補間元にすると、揺れが減衰せず残り続けてしまうため。
	Vector3 m_basePosition{ 0.0f, 0.0f, 0.0f };
	Vector3 m_baseLookat{ 0.0f, 0.0f, 0.0f };
	bool m_baseInitialized = false;
	float m_shakeStrength = 0.0f;
	float m_shakeTime = 0.0f;
	float m_shakeDuration = 0.0f;
	bool m_mouseLookEnabled = true;
	bool m_orbiting = false;
	int m_lastMouseX = 0;
	int m_lastMouseY = 0;
	bool m_mouseInitialized = false;
};
