#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/IScene.h"
#include	"../system/commontypes.h"	

class player : public gameobject {
	// 旧来の移動調整値（現在は未使用）。
public:
	inline static float VALUE_MOVE_MODEL = 70.0f;				// 遘ｻ蜍暮㍼
	inline static float VALUE_ROTATE_MODEL = PI * 0.002f;		// 蝗櫁ｻ｢驥・
	inline static float RATE_ROTATE_MODEL = 0.4f;				// 蝗櫁ｻ｢蜑ｲ蜷・
	inline static float RATE_MOVE_MODEL = 0.2f;					// 遘ｻ蜍墓ｸ幄｡ｰ蜑ｲ蜷・

public:
	enum class MotionState
	{
		Idle,
		Walk,
		Run,
		Dodge,
		Jump,
	};
	
	// 所属シーンを受け取るゲームオブジェクトのコンストラクタ。
	explicit player(IScene* scene);

	void update(uint64_t delta) override;
	void update(uint64_t delta, float cameraYaw);
	void update(uint64_t delta, float cameraYaw, bool movementLocked);
	void update(uint64_t delta, float cameraYaw, bool movementLocked, bool sprinting, bool dodgeTriggered);
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	Vector3 getVel() const;
	Vector3 getPos() const;

	SRT getRenderSRT() const;
	void setVisualGroundOffsetY(float offsetY);
	MotionState getMotionState() const;
	const char* getMotionStateName() const;
	float getMotionTime() const;
	bool isDodging() const;
	bool isInvincible() const;
	int getDodgeFrame() const;
	void resetMotion();

	void setVel(const Vector3& vel);

private:
	Vector3 m_move{0,0,0};				// 遘ｻ蜍暮㍼
	Vector3 m_destrot{0,0,0};			// 逶ｮ讓吝ｧｿ蜍｢
	MotionState m_motionState = MotionState::Idle;
	float m_motionTime = 0.0f;
	float m_jumpVelocity = 0.0f;
	bool m_jumpWasPressed = false;
	bool m_isJumping = false;
	bool m_isDodging = false;
	float m_dodgeTime = 0.0f;
	Vector3 m_dodgeDirection{0, 0, 0};
	// 読み込んだモデルの原点が足元にない場合に描画だけへ加える補正値。
	// ゲーム内の移動とジャンプ物理は従来どおりm_srtを使用する。
	float m_visualGroundOffsetY = 0.0f;

};
