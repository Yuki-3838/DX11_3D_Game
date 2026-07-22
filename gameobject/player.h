#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/IScene.h"
#include	"../system/commontypes.h"	

class player : public gameobject {
	//	
	/* 本来のコード
	static constexpr float VALUE_MOVE_MODEL = 2.0f;				// 移動量
	static constexpr float VALUE_ROTATE_MODEL = PI * 0.02f;		// 回転量
	static constexpr float RATE_ROTATE_MODEL = 0.4f;			// 回転割合	
	static constexpr float RATE_MOVE_MODEL = 0.2f;					// 移動減衰割合
	*/
	// debug用
public:
	inline static float VALUE_MOVE_MODEL = 2.0f;				// 移動量
	inline static float VALUE_ROTATE_MODEL = PI * 0.002f;		// 回転量
	inline static float RATE_ROTATE_MODEL = 0.4f;				// 回転割合	
	inline static float RATE_MOVE_MODEL = 0.2f;					// 移動減衰割合

public:
	enum class MotionState
	{
		Idle,
		Walk,
		Jump,
	};
	
	// ISceneのポインタを受け取るコンストラクタを追加
	player(IScene* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void update(uint64_t delta, float cameraYaw);
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
	MotionState getMotionState() const { return m_motionState; }
	const char* getMotionStateName() const;
	float getMotionTime() const { return m_motionTime; }
	void resetMotion();

	void setVel(const Vector3& vel) {
		m_move = vel;
	}

private:
	Vector3 m_move{0,0,0};				// 移動量
	Vector3 m_destrot{0,0,0};			// 目標姿勢
	MotionState m_motionState = MotionState::Idle;
	float m_motionTime = 0.0f;
	float m_jumpVelocity = 0.0f;
	bool m_jumpWasPressed = false;
	bool m_isJumping = false;

};
