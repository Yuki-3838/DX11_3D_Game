#pragma once
#include <memory>
#include <array>
#include <cstdint>
#include <vector>
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/CShader.h"
#include "../system/WeaponTrail.h"
#include "../system/HitEffect.h"
#include "../system/ThirdPersonCamera.h"
#include "../system/CAnimationMesh.h"
#include "../system/CAnimationData.h"
#include "../system/CCharacterAnimator.h"
#include "../system/BoneCombMatrix.h"
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

	player* getplayer();

	// デバッグ表示は1つのウィンドウにタブでまとめる。
	// 以前は7つの独立したウィンドウが画面中に散らばり、
	// ゲーム画面がほとんど見えなくなっていた。
	void DrawDebugWindow();
	void DebugWalls();
	void DebugEnemies();
	void DebugPlayerSRT();
	void DebugCamera();
	void DebugCombat();
	void DebugCollision();
	// 当たり判定の3D上のラベル。タブの選択状態に関わらず描くため、
	// タブの中身とは別の関数に分けている。
	void DrawCollisionWorldLabels();
	// 調整用。dev_settings.ini の enemy_hp を試合開始時の敵の体力にする。
	// 弱り具合(疲れ・瀕死)の見た目を、長い戦闘をしなくても確かめられるようにする。
	void ApplyDebugEnemyStartHp();
	void DebugAudio();
	void UpdateEnemyAnimation(float deltaSeconds = 0.0f);
	void StartEnemyIntro();
	void UpdateEnemyIntro(float deltaSeconds);
	void DrawEnemyIntroOverlay();
	void DrawGameplayHud();
	void UpdateArenaCameraCollision();
	// 敵の体がカメラとプレイヤーの間に入る場合の、許容できるカメラ距離を返す。
	// 遮蔽が無ければ desiredDistance をそのまま返す。
	float CalcEnemyOccludedCameraDistance(float desiredDistance) const;
	void UpdateGameplayCamera(float deltaSeconds = 0.0f);
	void UpdateCombatCameraDistance(float deltaSeconds);
	// 平行光源から見た深度を書き込み、キャラクターの形をした影を作る。
	void RenderShadowMap();
	Matrix4x4 BuildLightViewProjection() const;

private:
	ThirdPersonCamera m_camera;
	std::unique_ptr<CAnimationMesh> m_playerAnimationMesh;
	std::unique_ptr<CAnimationMesh> m_enemyAnimationMesh;
	CCharacterAnimator m_playerAnimator;
	BoneCombMatrix m_playerBoneComb;
	BoneCombMatrix m_enemyBoneComb;
	CAnimationData m_enemyAnimationData;
	CAnimationData m_playerAnimationData;
	aiAnimation* m_playerWalkAnimation = nullptr;
	aiAnimation* m_enemyIdleAnimation = nullptr;
	aiAnimation* m_enemyWalkAnimation = nullptr;
	aiAnimation* m_enemyAttackAnimation = nullptr;
	aiAnimation* m_enemyDieAnimation = nullptr;
	int m_enemyAnimationFrame = 0;
	int m_enemyAnimationTick = 0;
	// カットシーン終了時のIdle姿勢を、戦闘開始時のWalk先頭へ短時間でつなぐ。
	static constexpr float ENEMY_INTRO_BATTLE_BLEND_SECONDS = 0.18f;
	bool m_enemyIntroBattleBlendActive = false;
	float m_enemyIntroBattleBlendTime = 0.0f;
	int m_enemyIntroBlendFromFrame = 0;
	float m_enemyIntroBlendFromFraction = 0.0f;
	int m_enemyLastSampledFrame = 0;
	float m_enemyLastSampledFraction = 0.0f;
	enemy::MotionState m_previousEnemyMotionState = enemy::MotionState::Approach;
	std::array<std::unique_ptr<Segment>,3> m_segments;		// ローカル軸表示用線分

	std::unique_ptr<player>	m_player;						//	プレイヤ
	std::unique_ptr<field>	m_field;						//	フィールド
	std::vector<std::unique_ptr<wall>>	m_walls;			//	ｗａｌｌ
	std::vector<std::unique_ptr<enemy>>	m_enemies;			//	敵
	// 見た目だけの闘技場パーツ。衝突判定はm_wallsが担当し、
	// デバッグ編集画面とゲームプレイが同じ境界を共有する。
	Box m_arenaFloorVisual{1.0f, 1.0f, 1.0f};
	Box m_arenaWallVisual{1.0f, 1.0f, 1.0f};
	Box m_arenaWallCapVisual{1.0f, 1.0f, 1.0f};
	// 影生成パス用のシェーダー(深度だけを書き込む)。
	// スキンあり・なしで頂点の変換が違うため2種類持つ。
	CShader m_shadowDepthShader;
	CShader m_shadowDepthSkinShader;
	bool m_shadowEnabled = true;
	// 攻撃中だけカメラを寄せる。引いたままだと画面内のキャラクターが小さく、
	// 振りの迫力が出ないため、踏み込みに合わせて距離を詰める。
	// 剣の軌跡。武器ボーン由来のワールド座標だけを使うため、
	// 剣モデルやプレイヤーモデルを差し替えてもそのまま動く。
	WeaponTrail m_playerWeaponTrail;
	// 命中点の火花。当たったワールド座標と向きだけを渡すため、
	// 敵の攻撃や別の武器にも同じものを使える。
	HitEffect m_hitEffect;

	// 命中の手応え用。残り時間で減衰させる。
	// 敵は被弾時に発光させ、プレイヤー被弾時は画面端を赤くする。
	float m_enemyHitFlashTime = 0.0f;
	float m_playerDamageFlashTime = 0.0f;
	float m_cameraBaseDistance = 70.0f;
	float m_cameraDistanceCurrent = 70.0f;
	float m_cameraBaseTargetHeight = 25.0f;
	OneVsOneCombat m_combat;
	int m_lastPlayerComboStep = 0;
	bool m_lastPlayerHeavyAttack = false;
	bool m_resultRequested = false;
	bool m_enemyIntroActive = true;
	bool m_enemyRoarPlayed = false;
	bool m_lockOnTarget = false;
	float m_enemyIntroTime = 0.0f;
	Vector3 m_enemyIntroLandingPosition{ 0.0f, 0.0f, -120.0f };
	float m_playerStamina = 100.0f;
	static constexpr float PLAYER_MAX_STAMINA = 100.0f;
	int m_dodgeInvincibleStartFrame = 0;
	int m_dodgeInvincibleEndFrame = 12;
	int m_attackCancelEndFrame = 18;
	bool m_drawAttackCollisionDebug = false;
	bool m_drawAttackCollisionLabels = false;
	bool m_drawAttackCollisionXray = false;
	bool m_drawPlayerObb = false;
	bool m_drawEnemyObb = false;
	bool m_drawEnemyAabb = false;
	bool m_drawSwordObb = false;
	bool m_drawAttackAabb = false;
	bool m_drawLegacyPhysicsDebug = false;
	// 攻撃ごとの構えを見比べるための調整用(デバッグ表示からのみ操作する)。
	bool m_forceEnemyAttack = false;
	int m_forcedEnemyAttackIndex = 0;

	GM31::GE::Collision::BoundingSphere m_localbsplayer;	//  プレイヤBS（）ローカル座標系
	GM31::GE::Collision::BoundingSphere m_worldbsplayer;	//  プレイヤBS（）ワールド座標系
	GM31::GE::Collision::BoundingSphere m_localbsenemy;		//  敵BS（）ローカル座標系
	GM31::GE::Collision::BoundingBoxAABB m_localPlayerMeshBounds{};
	GM31::GE::Collision::BoundingBoxAABB m_localEnemyMeshBounds{};
	std::vector<wall*> m_hitWallObjects;						//  今フレーム衝突した壁
};

REGISTER_CLASS(GameScene)
