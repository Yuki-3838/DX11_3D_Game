#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/IScene.h"
#include	"../system/commontypes.h"	

class player : public gameobject {
	//	
	/* 譛ｬ譚･縺ｮ繧ｳ繝ｼ繝・
	static constexpr float VALUE_MOVE_MODEL = 2.0f;				// 遘ｻ蜍暮㍼
	static constexpr float VALUE_ROTATE_MODEL = PI * 0.02f;		// 蝗櫁ｻ｢驥・
	static constexpr float RATE_ROTATE_MODEL = 0.4f;			// 蝗櫁ｻ｢蜑ｲ蜷・
	static constexpr float RATE_MOVE_MODEL = 0.2f;					// 遘ｻ蜍墓ｸ幄｡ｰ蜑ｲ蜷・
	*/
	// debug逕ｨ
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
		Jump,
	};
	
	// IScene縺ｮ繝昴う繝ｳ繧ｿ繧貞女縺大叙繧九さ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ繧定ｿｽ蜉
	player(IScene* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void update(uint64_t delta, float cameraYaw);
	void update(uint64_t delta, float cameraYaw, bool movementLocked);
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	Vector3 getVel() const {
		return m_move;
	}

	Vector3 getPos() const {
		return m_srt.pos;
	}

	SRT getRenderSRT() const;
	void setVisualGroundOffsetY(float offsetY) { m_visualGroundOffsetY = offsetY; }
	MotionState getMotionState() const { return m_motionState; }
	const char* getMotionStateName() const;
	float getMotionTime() const { return m_motionTime; }
	void resetMotion();

	void setVel(const Vector3& vel) {
		m_move = vel;
	}

private:
	Vector3 m_move{0,0,0};				// 遘ｻ蜍暮㍼
	Vector3 m_destrot{0,0,0};			// 逶ｮ讓吝ｧｿ蜍｢
	MotionState m_motionState = MotionState::Idle;
	float m_motionTime = 0.0f;
	float m_jumpVelocity = 0.0f;
	bool m_jumpWasPressed = false;
	bool m_isJumping = false;
	// Render-only correction for imported models whose local origin is not at
	// the feet. Gameplay movement and jump physics continue to use m_srt.
	float m_visualGroundOffsetY = 0.0f;

};
