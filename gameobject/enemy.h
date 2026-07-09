#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/IScene.h"
#include	"../system/commontypes.h"	

class player;

class enemy : public gameobject {
	static constexpr float VALUE_MOVE_MODEL = 2.0f;				// 移動量
	static constexpr float VALUE_ROTATE_MODEL = PI * 0.02f;		// 回転量
	static constexpr float RATE_ROTATE_MODEL = 0.4f;			// 回転割合	
	static constexpr float RATE_MOVE_MODEL = 0.2f;					// 移動減衰割合
public:
	
	// ISceneのポインタを受け取るコンストラクタを追加
	enemy(IScene* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setTarget(player* target) {
		m_target = target;
	}

	Vector3 getVel() const {
		return m_move;
	}

	void setVel(const Vector3& vel) {
		m_move = vel;
	}

private:
	Vector3 m_move{0,0,0};				// 移動量
	Vector3 m_destrot{0,0,0};			// 目標姿勢
	player* m_target{ nullptr };			// 追尾対象
};