#pragma once
#include <memory>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/Camera.h"
#include "../gameobject/player.h"
#include "../gameobject/field.h"
#include "../gameobject/enemy.h"


class CarScene : public IScene 
{
public:
	/// @brief 敵の数
	static constexpr int ENEMYMAX = 100;

	explicit CarScene();
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;

	player* getplayer() {
		return m_player.get();
	}

private:
	Camera m_camera;									// 固定カメラ
	std::array<std::unique_ptr<Segment>,3> m_segments;	// ローカル軸表示用線分

	//	プレイヤ
	std::unique_ptr<player>	m_player;
	std::unique_ptr<field>	m_field;
	std::vector<std::unique_ptr<enemy>>	m_enemies;

};

REGISTER_CLASS(CarScene)