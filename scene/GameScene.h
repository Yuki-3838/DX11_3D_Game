#pragma once
#include <memory>
#include <array>
#include <cstdint>
#include <vector>
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/Camera.h"
#include "../system/collision.h"
#include "../system/OneVsOneCombat.h"
#include "../gameobject/player.h"
#include "../gameobject/field.h"
#include "../gameobject/wall.h"
#include "../gameobject/enemy.h"

class GameScene : public IScene 
{
public:
	static constexpr uint8_t INITIAL_WALLNUM = 0;
	static constexpr uint8_t INITIAL_ENEMYNUM = 1;

	explicit GameScene();
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;

	player* getplayer() {
		return m_player.get();
	}

	void DebugWalls();
	void DebugEnemies();
	void DebugPlayerSRT();
	void DebugCamera();
	void DebugCombat();

private:
	void UpdateThirdPersonCamera(uint64_t deltatime);

	Camera m_camera;										// デバッグ用フリーカメラ
	float m_cameraYaw = 0.0f;
	float m_cameraPitch = -0.05f;
	float m_cameraMouseSensitivity = 0.004f;
	float m_cameraLookDistance = 80.0f;
	float m_cameraTargetHeight = 8.0f;
	bool m_cameraFreeControl = true;
	bool m_cameraViewportHovered = false;
	bool m_cameraOrbiting = false;
	int m_lastCameraMouseX = 0;
	int m_lastCameraMouseY = 0;
	bool m_cameraMouseInitialized = false;
	std::array<std::unique_ptr<Segment>,3> m_segments;		// ローカル軸表示用線分

	std::unique_ptr<player>	m_player;						//	プレイヤ
	std::unique_ptr<field>	m_field;						//	フィールド
	std::vector<std::unique_ptr<wall>>	m_walls;			//	ｗａｌｌ
	std::vector<std::unique_ptr<enemy>>	m_enemies;			//	敵
	OneVsOneCombat m_combat;

	GM31::GE::Collision::BoundingSphere m_localbsplayer;	//  プレイヤBS（）ローカル座標系
	GM31::GE::Collision::BoundingSphere m_worldbsplayer;	//  プレイヤBS（）ワールド座標系
	GM31::GE::Collision::BoundingSphere m_localbsenemy;		//  敵BS（）ローカル座標系
	std::vector<wall*> m_hitWallObjects;						//  今フレーム衝突した壁
};

REGISTER_CLASS(GameScene)
