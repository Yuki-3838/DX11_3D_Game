#pragma once

#include "camera.h"

/**
 * @brief 視点操作の入力。GameSceneがマウスなどから作って渡す。
 *
 * カメラクラスは入力デバイスを直接読まない(マウスの閉じ込めや、デバッグ表示との
 * 取り合いはシーン側の都合なので)。
 */
struct CameraLookInput
{
	float deltaX = 0.0f;   ///< 横方向の移動量(マウスの移動量。右が正)
	float deltaY = 0.0f;   ///< 縦方向の移動量(下が正)
	bool active = false;   ///< 視点操作を受け付けているか(カーソルを出している間はfalse)
};

/**
 * @brief 三人称の戦闘カメラ。
 *
 * フロム・ソフトウェア作品(Dark Souls / Elden Ring)の戦闘カメラを参考にしている。
 * - マウスを動かすだけで視点が回る。
 * - 視点を触らずに横へ走ると、カメラがゆっくり背後へ回り込む(自動追従)。
 * - カメラは「向き(Yaw)・高さ(Pitch)・距離」を補間して動かす。
 *   位置を直線で補間すると、キャラクターの周りを回るときに内側へ近道して一瞬寄って見えるため。
 * - ロックオン中は向きの変化に速さの上限を設け、敵が真横を横切ってもカメラが急に振られない。
 *   敵の頭の高さに合わせて見上げる。
 */
class ThirdPersonCamera
{
public:
	void Init();
	/**
	 * @brief 通常時(ロックオンしていないとき)の更新。
	 * @param playerPosition プレイヤーの足元の位置。
	 * @param playerVelocity プレイヤーの移動速度(ワールド単位/秒)。自動追従に使う。
	 * @param look 視点操作の入力。
	 */
	void Update(
		const Vector3& playerPosition,
		const Vector3& playerVelocity,
		const CameraLookInput& look,
		float deltaSeconds);
	/**
	 * @brief ロックオン中の更新。
	 * @param targetPosition 敵の足元の位置。
	 * @param targetHeight 敵の高さ(ワールド単位)。注視する高さと見上げる角度に使う。
	 */
	void UpdateLockOn(
		const Vector3& playerPosition,
		const Vector3& targetPosition,
		float targetHeight,
		float deltaSeconds);
	void Draw();
	void Reset(const Vector3& playerPosition, float playerYaw = 0.0f);
	// 登場演出の現在カメラを保持したまま、戦闘用の背後カメラへ短時間で遷移する。
	void BeginBattleTransition(const Vector3& playerPosition, float playerYaw);
	/**
	 * @brief カメラをプレイヤーの背後へ回す(ロックオンできないときのTab)。
	 * @details フロム作品でロックオン対象がいないときにロックオンボタンを押すと、
	 *          カメラが正面を向き直すのと同じ。視点を操作すると中断する。
	 */
	void RequestRecenter(float playerYaw);

	/**
	 * @brief 命中の手応えとしてカメラを短く揺らす。
	 * @param strength 揺れ幅(ワールド単位)。
	 * @param duration 揺れが収まるまでの時間(秒)。
	 * @details 既に揺れている場合は強い方を採用する。連続ヒットで
	 *          揺れが累積して画面が破綻するのを防ぐ。
	 */
	void TriggerShake(float strength, float duration);

	// カメラの水平な向き。プレイヤーの移動(WASD)はこの向きを基準にする。
	float GetYaw() const { return m_cameraYaw; }
	float GetPitch() const { return m_pitch; }
	float GetLookDistance() const { return m_lookDistance; }
	// 今のモード(通常・ロックオン)でカメラが取りたい距離。壁の衝突判定に使う。
	float GetDesiredDistance() const { return m_desiredDistance; }
	float GetTargetHeight() const { return m_targetHeight; }
	float GetMouseSensitivity() const { return m_mouseSensitivity; }
	bool IsAutoFollowEnabled() const { return m_autoFollowEnabled; }
	Matrix4x4 GetViewMatrix() const { return m_camera.GetViewMatrix(); }
	Matrix4x4 GetProjMatrix() const { return m_camera.GetProjMatrix(); }
	// 遮蔽判定のため、実際に使われているカメラ位置と注視点を参照する。
	Vector3 GetPosition() const { return m_camera.GetPosition(); }
	Vector3 GetLookat() const { return m_camera.GetLookat(); }

	void SetTargetHeight(float height) { m_targetHeight = height; }
	void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
	void SetAutoFollowEnabled(bool enabled) { m_autoFollowEnabled = enabled; }
	void SetLookDistance(float distance);
	// 闘技場の壁や敵の体がプレイヤーとカメラの間に入る場合はカメラを引き寄せる。
	// 負の値を指定すると通常の距離に戻す。
	void SetCollisionDistance(float distance);
	// 操作を受け付けない短い登場演出で、カメラ位置と注視点を直接指定する。
	void SetCinematicView(const Vector3& cameraPosition, const Vector3& lookAt)
	{
		m_camera.SetPosition(cameraPosition);
		m_camera.SetLookat(lookAt);
		m_battleTransitionActive = false;
		// 演出の後は、追従点を現在のプレイヤー位置から取り直す。
		m_anchorInitialized = false;
	}

private:
	// 追従点(プレイヤーの注視する高さ)を滑らかに動かす。水平は速く、上下はゆっくり追う。
	void UpdateAnchor(const Vector3& playerPosition, float deltaSeconds);
	// 衝突による距離の制限を反映した、実際に使う距離を滑らかに動かす。
	void UpdateDistance(float desiredDistance, float deltaSeconds);
	Vector3 BoomOffset(float distance) const;
	void ApplyView(
		const Vector3& position,
		const Vector3& lookat,
		float deltaSeconds);

	Camera m_camera;
	// カメラの水平な向き。オフセットは (sin, -cos) 方向に伸びる。
	float m_cameraYaw = PI;
	// 少し高い位置から引いた、戦闘全体を見渡しやすい構図にする。
	float m_pitch = CAMERA_DEFAULT_PITCH;
	// マウスの移動量1カウントあたりの回転(ラジアン)。約0.06度。
	// 実機でマウスを動かすと1フレームに数百〜数千カウント入るので、これより大きいと少し振るだけで1回転する。
	float m_mouseSensitivity = 0.0010f;
	float m_lookDistance = 70.0f;
	float m_desiredDistance = 70.0f;
	float m_currentDistance = 70.0f;
	float m_collisionDistance = -1.0f;
	float m_targetHeight = 25.0f;

	// 追従点(プレイヤーの注視する高さ)と、注視点の追従点からのずれ量(ロックオン中に敵の方へ寄せる)。
	Vector3 m_anchor{ 0.0f, 0.0f, 0.0f };
	Vector3 m_lookOffset{ 0.0f, 0.0f, 0.0f };
	bool m_anchorInitialized = false;

	// 自動追従。視点を最後に操作してからの時間が短い間は回さない。
	bool m_autoFollowEnabled = true;
	float m_timeSinceManualLook = 10.0f;

	// 背後へ回す(Tab)。
	bool m_recenterActive = false;
	float m_recenterYaw = 0.0f;

	// DEFAULT_PITCH という名前はWindowsのヘッダー(wingdi.h)がマクロで使っているので避ける。
	static constexpr float CAMERA_DEFAULT_PITCH = 0.30f;
	static constexpr float MIN_PITCH = -0.20f;
	static constexpr float MAX_PITCH = 0.95f;

	// カットシーンから戦闘へ切り替わるときの短いカメラ遷移。
	bool m_battleTransitionActive = false;
	float m_battleTransitionTime = 0.0f;
	static constexpr float BATTLE_TRANSITION_SECONDS = 0.36f;
	Vector3 m_battleTransitionStartPosition{ 0.0f, 50.0f, -100.0f };
	Vector3 m_battleTransitionStartLookat{ 0.0f, 0.0f, 0.0f };
	// 命中時のカメラ揺れ。揺れを含まない位置へ毎フレーム足すだけにする
	// (揺れた位置を次の補間元にすると揺れが減衰せず残るため)。
	float m_shakeStrength = 0.0f;
	float m_shakeTime = 0.0f;
	float m_shakeDuration = 0.0f;
};
