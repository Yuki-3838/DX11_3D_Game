#include    <memory>
#include	<string>
#include	<array>
#include	<cstdint>
#include	<filesystem>
#include	<string_view>
#include	<utility>
#include	<vector>
#include	<unordered_map>
#include	<initializer_list>
#include	<cmath>
#include	"GameScene.h"
#include	"../system/CShader.h"
#include	"../system/MeshManager.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/renderer.h"
#include	"../system/transform.h"
#include	"../system/C3DShape.h"
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"
#include	<dinput.h>
#include	"../system/PlaneDrawer.h"
#include	"../system/imgui/imgui.h"
#include	"../system/DebugUI.h"
#include	"../system/CPlane.h"
#include	"../system/collision.h"
#include	"../gameobject/player.h"
#include	"../gameobject/field.h"
#include	"../gameobject/wall.h"
#include	"../gameobject/enemy.h"
#include	"../system/SphereDrawer.h"
#include <algorithm>

// 無名名前空間
namespace {

	struct Load3DInfo {
		std::string meshid{};
		std::string filename;
		std::string texdirectoryname;
		Load3DInfo(std::string id,std::string p1, std::string p2) {
			meshid = id;
			filename = p1;
			texdirectoryname = p2;
		}
	};

	std::string getfilename(std::string_view filestring) {
		const auto separator = filestring.find_last_of("/\\");
		const auto filename = separator == std::string_view::npos
			? filestring
			: filestring.substr(separator + 1);
		return std::string(filename);
	}

	std::array<Load3DInfo, 3> g_loadmodel =
	{
			Load3DInfo(
				"furina_player",
				"assets/model/Furina/Furina.pmx",			// ASCII名に変換したプレイヤーモデル
				"assets/model/Furina/"),				// テクスチャのパス

			Load3DInfo(
				"car001.x",
				"assets/model/car001.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"car002.x",
				"assets/model/car002.x",			// モデル名
				"assets/model/"),					// テクスチャのパス
	};

	constexpr float ENEMY_MODEL_SCALE = 0.7f;

	// 壁データ
	struct WallData {
		Vector3 pos{0,0,0};				// 位置	
		Vector3 rot{0,0,0};				// 姿勢
		float height{0};				// 高さ
		float width{0};					// 幅	
		CPlane plane{};					// 平面方程式
		wall* pwallobj{nullptr};		// WALL obj
		bool hitflag{ false };
	};

	// 衝突した壁データ
	struct WallCollision {
		WallData walldata;			// 壁データ	
		Vector3 penetration;		// 侵入ベクトル
		Vector3 sliding;			// 壁摺りベクトル
		Vector3 intersectionPoint;	// 交点（最近接点）
	};

	// 壁群と当たり判定を行う（壁と球のあたり判定を行う）
	std::vector<WallCollision> checkWallCollision(
		std::vector<WallData>& walldatas,		// 当たり判定の対象壁情報
		float radius,				// 球の半径
		Vector3 pos,				// 現在位置 	
		Vector3 velocity)			// 速度ベクトル
	{
		// 衝突している壁
		std::vector<WallCollision> hitwalls{};

		// 次の場所を求める
		Vector3 nextpos = pos + velocity;

		// 平面と球の距離を求める
		for (auto& wall : walldatas)
		{
			wall.hitflag = false;

			PLANEINFO pi = wall.plane.GetPlaneInfo();				// 壁の平面方程式を取得
			// 壁と中心座標の距離を求める（法線ベクトルを正規化しているので可能）
			float lng = pi.plane.a * nextpos.x + pi.plane.b * nextpos.y + pi.plane.c * nextpos.z + pi.plane.d;

			if (fabs(lng) < radius)
				// 半径以内なら衝突している可能性があるので　精密に判定する
			{
				// OOBと球の当たり判定を行う(奥行を持たせて考えるという事（今は Z=2.0 固定）)
				GM31::GE::Collision::BoundingBoxOBB obb;
				obb = GM31::GE::Collision::SetOBB(wall.rot, wall.pos, wall.width, wall.height, 2.0f);

				// 球の定義
				GM31::GE::Collision::BoundingSphere sphere(nextpos, radius);

				// 球とOBBの当たり判定
				bool sts = GM31::GE::Collision::CollisionSphereOBB(
					sphere,
					obb);

				// 衝突したので壁衝突したデータを作成
				if (sts) {
					wall.hitflag = true;

					WallCollision wallcollision;					// 衝突した壁の詳細情報

					wallcollision.walldata = wall;					// 壁データ
					wallcollision.penetration = Vector3(0, 0, 0);	// 侵入ベクトル
					wallcollision.sliding = Vector3(0, 0, 0);		// 壁擦りベクトル

					ClosestPtPointOBB(sphere.center, obb, wallcollision.intersectionPoint);		// 最近接点を求める
					hitwalls.push_back(wallcollision);				// ヒットした壁を追加
				}
			}
		}

		return hitwalls;
	}

	// ローカルBSをワールドBSにする
	GM31::GE::Collision::BoundingSphere transformBSphere(
		const GM31::GE::Collision::BoundingSphere& localSphere, const SRT& transform)
	{
		GM31::GE::Collision::BoundingSphere worldSphere;

		// 中心座標を変換
		worldSphere.center = Vector3::Transform(localSphere.center, transform.GetMatrix());

		// 半径をスケール（XYZのうち最も大きいスケール値を掛ける）
		float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
		worldSphere.radius = localSphere.radius * maxScale;

		return worldSphere;
	}

	std::vector<WallData> createWallData(
		const std::vector<std::unique_ptr<wall>>& walls)
	{
		std::vector<WallData> walldatas{};
		walldatas.reserve(walls.size());

		for (const auto& wallobj : walls) {
			WallData wd;
			wd.height = wallobj->getheight();
			wd.width = wallobj->getwidth();
			wd.plane = wallobj->getEquation();
			wd.pos = wallobj->getSRT().pos;
			wd.rot = wallobj->getSRT().rot;
			wd.pwallobj = wallobj.get();
			walldatas.push_back(wd);
		}

		return walldatas;
	}

	Vector3 calcWallAdjustedMove(
		std::vector<WallData>& walldatas,
		float radius,
		Vector3 currentCenter,
		Vector3 velocity,
		bool stopOnHit)
	{
		std::vector<WallCollision> hits = checkWallCollision(
			walldatas,
			radius,
			currentCenter,
			velocity);

		if (hits.empty()) {
			return velocity;
		}

		if (stopOnHit) {
			return Vector3(0, 0, 0);
		}

		Vector3 slideVelocity = velocity;
		for (const auto& hit : hits) {
			PLANEINFO pi = hit.walldata.plane.GetPlaneInfo();
			Vector3 normal(pi.plane.a, pi.plane.b, pi.plane.c);
			if (normal.Length() > 0.0001f) {
				normal.Normalize();
				slideVelocity -= normal * slideVelocity.Dot(normal);
			}
		}

		if (slideVelocity.Length() < 0.0001f) {
			return Vector3(0, 0, 0);
		}

		std::vector<WallCollision> slideHits = checkWallCollision(
			walldatas,
			radius,
			currentCenter,
			slideVelocity);

		if (!slideHits.empty()) {
			return Vector3(0, 0, 0);
		}

		return slideVelocity;
	}

	Vector3 calcWallAvoidMove(
		std::vector<WallData>& walldatas,
		float radius,
		Vector3 currentCenter,
		Vector3 velocity,
		Vector3 targetCenter)
	{
		std::vector<WallCollision> hits = checkWallCollision(
			walldatas,
			radius,
			currentCenter,
			velocity);

		if (hits.empty()) {
			return velocity;
		}

		for (const auto& hit : hits) {
			PLANEINFO pi = hit.walldata.plane.GetPlaneInfo();
			Vector3 normal(pi.plane.a, pi.plane.b, pi.plane.c);
			normal.y = 0.0f;
			if (normal.Length() <= 0.0001f) {
				continue;
			}
			normal.Normalize();

			Vector3 tangent(normal.z, 0.0f, -normal.x);
			Vector3 toTarget = targetCenter - currentCenter;
			toTarget.y = 0.0f;
			if (tangent.Dot(toTarget) < 0.0f) {
				tangent = -tangent;
			}

			float speed = velocity.Length();
			if (speed < 0.0001f) {
				speed = 0.5f;
			}

			float signedDistance =
				pi.plane.a * currentCenter.x +
				pi.plane.b * currentCenter.y +
				pi.plane.c * currentCenter.z +
				pi.plane.d;

			Vector3 awayNormal = normal;
			if (signedDistance < 0.0f) {
				awayNormal = -awayNormal;
			}
			else if (std::fabs(signedDistance) <= 0.0001f && velocity.Dot(normal) > 0.0f) {
				awayNormal = -awayNormal;
			}

			float pushOutLength = std::max(0.0f, radius - std::fabs(signedDistance) + 0.5f);
			Vector3 pushOut = awayNormal * pushOutLength;

			Vector3 candidates[2] = {
				tangent * speed + pushOut,
				-tangent * speed + pushOut
			};

			for (const auto& candidate : candidates) {
				std::vector<WallCollision> avoidHits = checkWallCollision(
					walldatas,
					radius,
					currentCenter,
					candidate);
				if (avoidHits.empty()) {
					return candidate;
				}
			}
		}

		return calcWallAdjustedMove(walldatas, radius, currentCenter, velocity, false);
	}

	void collectHitWalls(
		std::vector<wall*>& hitWallObjects,
		const std::vector<std::unique_ptr<wall>>& walls,
		float radius,
		Vector3 center)
	{
		auto walldatas = createWallData(walls);
		auto hits = checkWallCollision(walldatas, radius, center, Vector3(0, 0, 0));
		for (const auto& hit : hits) {
			hitWallObjects.push_back(hit.walldata.pwallobj);
		}
	}

	bool containsWall(const std::vector<wall*>& walls, const wall* target)
	{
		return std::find(walls.begin(), walls.end(), target) != walls.end();
	}

	void appendHitWalls(
		std::vector<wall*>& hitWallObjects,
		std::vector<WallCollision>& collisions)
	{
		for (const auto& hit : collisions) {
			if (!containsWall(hitWallObjects, hit.walldata.pwallobj)) {
				hitWallObjects.push_back(hit.walldata.pwallobj);
			}
		}
	}

	std::unique_ptr<enemy> createEnemyObject(
		IScene* scene,
		player* target,
		const Vector3& pos,
		float rotY,
		float scale)
	{
		std::unique_ptr<enemy> newEnemy = std::make_unique<enemy>(scene);
		newEnemy->init();
		newEnemy->setTarget(target);

		SRT srt{};
		srt.pos = pos;
		srt.rot.y = rotY;
		srt.scale = Vector3(scale, scale, scale);
		newEnemy->setSRT(srt);

		return newEnemy;
	}

	void resolveEnemyCollisions(
		std::vector<std::unique_ptr<enemy>>& enemies,
		const GM31::GE::Collision::BoundingSphere& localSphere)
	{
		for (size_t i = 0; i < enemies.size(); i++) {
			for (size_t j = i + 1; j < enemies.size(); j++) {
				SRT srtA = enemies[i]->getSRT();
				SRT srtB = enemies[j]->getSRT();
				GM31::GE::Collision::BoundingSphere sphereA = transformBSphere(localSphere, srtA);
				GM31::GE::Collision::BoundingSphere sphereB = transformBSphere(localSphere, srtB);

				Vector3 diff = sphereB.center - sphereA.center;
				diff.y = 0.0f;

				float distance = diff.Length();
				float hitDistance = sphereA.radius + sphereB.radius;
				if (distance >= hitDistance) {
					continue;
				}

				Vector3 pushDir(1, 0, 0);
				if (distance > 0.0001f) {
					pushDir = diff / distance;
				}

				float pushLength = (hitDistance - distance) * 0.5f;
				srtA.pos -= pushDir * pushLength;
				srtB.pos += pushDir * pushLength;

				enemies[i]->setSRT(srtA);
				enemies[j]->setSRT(srtB);
				enemies[i]->setVel(Vector3(0, 0, 0));
				enemies[j]->setVel(Vector3(0, 0, 0));
			}
		}
	}

	void resolvePlayerEnemyCollisions(
		player* playerObj,
		std::vector<std::unique_ptr<enemy>>& enemies,
		const GM31::GE::Collision::BoundingSphere& localPlayerSphere,
		const GM31::GE::Collision::BoundingSphere& localEnemySphere)
	{
		if (playerObj == nullptr) {
			return;
		}

		SRT playerSrt = playerObj->getSRT();
		GM31::GE::Collision::BoundingSphere playerSphere = transformBSphere(localPlayerSphere, playerSrt);

		for (auto& enemyObj : enemies) {
			SRT enemySrt = enemyObj->getSRT();
			GM31::GE::Collision::BoundingSphere enemySphere = transformBSphere(localEnemySphere, enemySrt);

			Vector3 diff = enemySphere.center - playerSphere.center;
			diff.y = 0.0f;

			float distance = diff.Length();
			float hitDistance = playerSphere.radius + enemySphere.radius;
			if (distance >= hitDistance) {
				continue;
			}

			Vector3 pushDir(1, 0, 0);
			if (distance > 0.0001f) {
				pushDir = diff / distance;
			}

			float pushLength = (hitDistance - distance) * 0.5f;
			playerSrt.pos -= pushDir * pushLength;
			enemySrt.pos += pushDir * pushLength;

			playerObj->setSRT(playerSrt);
			playerObj->setVel(Vector3(0, 0, 0));
			enemyObj->setSRT(enemySrt);
			enemyObj->setVel(Vector3(0, 0, 0));

			playerSphere = transformBSphere(localPlayerSphere, playerSrt);
		}
	}
}

GameScene::GameScene()
{
	PlaneDrawerInit();
	SphereDrawerInit();
}

void GameScene::update(uint64_t deltatime)
{
    auto& input = CInputManager::GetInstance();

    if (input.IsKeyTriggered(DIK_R))
    {
        m_combat.Reset();
        SRT playerReset = m_player->getSRT();
        playerReset.pos = Vector3(0, 0, 0);
        playerReset.rot = Vector3(0, 0, 0);
        m_player->setSRT(playerReset);
		m_player->resetMotion();
        if (!m_enemies.empty())
        {
            SRT enemyReset = m_enemies.front()->getSRT();
            enemyReset.pos = Vector3(0, 0, -120);
            enemyReset.rot = Vector3(0, 0, 0);
            m_enemies.front()->setSRT(enemyReset);
            m_enemies.front()->setVel(Vector3(0, 0, 0));
        }
    }
	auto walldatas = createWallData(m_walls);
	m_hitWallObjects.clear();

	SRT prevPlayerSrt = m_player->getSRT();
	GM31::GE::Collision::BoundingSphere prevPlayerSphere = transformBSphere(m_localbsplayer, prevPlayerSrt);

	m_player->update(deltatime, m_camera.GetYaw());

	SRT playerSrt = m_player->getSRT();
	GM31::GE::Collision::BoundingSphere nextPlayerSphere = transformBSphere(m_localbsplayer, playerSrt);
	Vector3 playerMove = nextPlayerSphere.center - prevPlayerSphere.center;
	Vector3 adjustedPlayerMove = calcWallAdjustedMove(
		walldatas,
		prevPlayerSphere.radius,
		prevPlayerSphere.center,
		playerMove,
		true);

	if (adjustedPlayerMove.Length() < playerMove.Length()) {
		auto playerHitWalls = checkWallCollision(
			walldatas,
			prevPlayerSphere.radius,
			prevPlayerSphere.center,
			playerMove);
		appendHitWalls(m_hitWallObjects, playerHitWalls);
	}

	playerSrt.pos = prevPlayerSrt.pos + adjustedPlayerMove;
	m_player->setSRT(playerSrt);
	if (adjustedPlayerMove.Length() < playerMove.Length()) {
		m_player->setVel(Vector3(0, 0, 0));
	}
	if (m_playerAnimationMesh && m_player)
	{
		m_playerAnimator.Update(
			*m_playerAnimationMesh,
			m_playerBoneComb,
			{
				m_player->getMotionState() == player::MotionState::Walk || m_player->getVel().Length() > 0.01f,
				m_player->getMotionState() == player::MotionState::Jump,
				m_player->getMotionTime()
			});
	}

	for (auto& e : m_enemies) {
		SRT prevEnemySrt = e->getSRT();
		GM31::GE::Collision::BoundingSphere prevEnemySphere = transformBSphere(m_localbsenemy, prevEnemySrt);

		e->update(deltatime);

		SRT enemySrt = e->getSRT();
		GM31::GE::Collision::BoundingSphere nextEnemySphere = transformBSphere(m_localbsenemy, enemySrt);
		Vector3 enemyMove = nextEnemySphere.center - prevEnemySphere.center;
		Vector3 adjustedEnemyMove = calcWallAvoidMove(
			walldatas,
			prevEnemySphere.radius,
			prevEnemySphere.center,
			enemyMove,
			playerSrt.pos);

		if (adjustedEnemyMove.Length() < enemyMove.Length()) {
			auto enemyHitWalls = checkWallCollision(
				walldatas,
				prevEnemySphere.radius,
				prevEnemySphere.center,
				enemyMove);
			appendHitWalls(m_hitWallObjects, enemyHitWalls);
		}

		enemySrt.pos = prevEnemySrt.pos + adjustedEnemyMove;
		if (adjustedEnemyMove.Length() > 0.0001f) {
			enemySrt.rot.y = std::atan2(-adjustedEnemyMove.x, -adjustedEnemyMove.z);
		}
		e->setSRT(enemySrt);
		e->setVel(adjustedEnemyMove);
	}

	resolveEnemyCollisions(m_enemies, m_localbsenemy);
	resolvePlayerEnemyCollisions(m_player.get(), m_enemies, m_localbsplayer, m_localbsenemy);
    if (!m_enemies.empty())
    {
        const Vector3 enemyPosition = m_enemies.front()->getSRT().pos;
        const bool attackTriggered = input.IsKeyTriggered(DIK_SPACE) || input.IsKeyTriggered(DIK_J);
        const bool guardPressed = input.IsKeyPressed(DIK_K) ||
            (input.IsMousePressed(CInputManager::MOUSE_RIGHT) && !m_camera.IsOrbiting());
        m_combat.Update(deltatime, m_player->getSRT().pos, enemyPosition, attackTriggered, guardPressed);
    }
}

void GameScene::draw(uint64_t deltatime)
{
	m_camera.Draw();

	// 3軸カラー
	Color axiscol[3] = {
		Color(1, 0, 0, 1), 
		Color(0, 1, 0, 1),
		Color(0, 1, 1, 1)
	};

	// 3本のworld軸を描画
	for (int cnt = 0; cnt < m_segments.size(); cnt++)
	{
		Matrix4x4 worldmtx = Matrix4x4::Identity;
		m_segments[cnt]->SetWidth(3);
		m_segments[cnt]->Draw(worldmtx, Color(0,0,0,1));
	}

	// 3本のローカル軸を描画
	for (int cnt=0;cnt<m_segments.size();cnt++)
	{
		SRT srt = m_player->getSRT();
		Matrix4x4 localmtx{};
		srt.scale = Vector3(1, 1, 1);
		localmtx = srt.GetMatrix();
		m_segments[cnt]->SetWidth(3);
		m_segments[cnt]->Draw(localmtx, axiscol[cnt]);
	}

	m_field->draw(deltatime);

	// モデルを描画
	{
	// プレイヤモデルの姿勢情報を取得
		SRT srt = m_player->getRenderSRT();
		Matrix4x4 worldmtx{};
		worldmtx = srt.GetMatrix();
		Renderer::SetWorldMatrix(&worldmtx);

		ShaderManager::Get<CShader>("Shader3DSkin")->SetGPU();
		m_playerBoneComb.Update();
		m_playerBoneComb.SetGPU();
		m_playerAnimationMesh->Draw();
	}

	// 敵を描画
	for (auto& e : m_enemies) {
		SRT srt = e->getSRT();
		Matrix4x4 worldmtx{};
		worldmtx = srt.GetMatrix();
		Renderer::SetWorldMatrix(&worldmtx);

		ShaderManager::Get<CShader>("Shader3D")->SetGPU();
		if (auto* enemyRenderer = MeshManager::getRenderer<CStaticMeshRenderer>(g_loadmodel[1].meshid))
		{
			enemyRenderer->Draw();
		}
	}

	std::vector<wall*> hitWallObjects = m_hitWallObjects;

	SRT playersrt = m_player->getSRT();
	m_worldbsplayer = transformBSphere(m_localbsplayer, playersrt);
	SphereDrawerDraw(m_worldbsplayer.radius, Color(1, 1, 1, 0.5f),
		m_worldbsplayer.center.x,
		m_worldbsplayer.center.y,
		m_worldbsplayer.center.z);
	collectHitWalls(hitWallObjects, m_walls, m_worldbsplayer.radius, m_worldbsplayer.center);

	for (auto& e : m_enemies) {
		GM31::GE::Collision::BoundingSphere enemySphere = transformBSphere(m_localbsenemy, e->getSRT());
		SphereDrawerDraw(enemySphere.radius, Color(1, 0, 0, 0.25f),
			enemySphere.center.x,
			enemySphere.center.y,
			enemySphere.center.z);
		collectHitWalls(hitWallObjects, m_walls, enemySphere.radius, enemySphere.center);
	}

	Renderer::DisableCulling(false);
	for (const auto& wallobj : m_walls) {
		if (containsWall(hitWallObjects, wallobj.get())) {
			wallobj->drawred(deltatime);
		}
		else {
			wallobj->draw(deltatime);
		}
	}
	Renderer::DisableCulling(true);

}
void GameScene::init()
{
	m_combat.Reset();
	// カメラ(3D)の初期化
	m_camera.Init();

	// ローカル軸表示用線分の初期化
	m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
	m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
	m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));

	// シェーダを生成
	std::unique_ptr<CShader> shader{};
	shader = std::make_unique<CShader>();
	shader->Create(
		"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
		"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー
	);
	ShaderManager::Register<CShader>("Shader3D", std::move(shader));

	std::unique_ptr<CShader> skinShader = std::make_unique<CShader>();
	skinShader->Create(
		"shader/vertexLightingOneSkinVS.hlsl",
		"shader/vertexLightingPS.hlsl"
	);
	ShaderManager::Register<CShader>("Shader3DSkin", std::move(skinShader));

	// メッシュを生成
	for (int cnt = 0; cnt < g_loadmodel.size(); cnt++) {
		std::unique_ptr<CStaticMesh> mesh{};
		mesh = std::make_unique<CStaticMesh>();
		mesh->Load(g_loadmodel[cnt].filename, g_loadmodel[cnt].texdirectoryname);

		// メッシュレンダラを生成
		std::unique_ptr<CStaticMeshRenderer> meshrenderer{};
		meshrenderer = std::make_unique<CStaticMeshRenderer>();
		meshrenderer->Init(*mesh.get());

		if (g_loadmodel[cnt].meshid.empty()) {
			g_loadmodel[cnt].meshid = getfilename(g_loadmodel[cnt].filename);
		}

		MeshManager::RegisterMesh<CStaticMesh>(g_loadmodel[cnt].meshid, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(g_loadmodel[cnt].meshid, std::move(meshrenderer));
	}

	m_playerAnimationMesh = std::make_unique<CAnimationMesh>();
	m_playerAnimationMesh->Load(g_loadmodel[0].filename, g_loadmodel[0].texdirectoryname);
	m_playerAnimator.Initialize(*m_playerAnimationMesh);
	m_playerBoneComb.Create();
	m_playerAnimationMesh->UpdateManualPose(m_playerBoneComb, {});

	m_player = std::make_unique<player>(this);
	m_player->init();
	SRT playerModelSrt = m_player->getSRT();
	playerModelSrt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_player->setSRT(playerModelSrt);

	m_field = std::make_unique<field>(this);
	m_field->init();

	m_enemies.reserve(INITIAL_ENEMYNUM);
	for (int ecnt = 0; ecnt < INITIAL_ENEMYNUM; ecnt++) { 		Vector3 enemyPos(0, 0, -120); 		float enemyRotY = 0.0f; 		m_enemies.push_back(createEnemyObject(this, m_player.get(), enemyPos, enemyRotY, ENEMY_MODEL_SCALE)); 	}
	// PLAYER BS作成
	{
	CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(g_loadmodel[0].meshid);
		const std::vector<VERTEX_3D>& vertices = mesh->GetVertices();

		std::vector<Vector3> vs;
		for (auto& v : vertices) {
			vs.push_back(v.Position);
		}

		SRT srt{};
		m_localbsplayer = GM31::GE::Collision::calcBSphere(vs, srt);
	}

	// ENEMY BS作成
	{
		CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(g_loadmodel[1].meshid);
		if (mesh == nullptr || mesh->GetVertices().empty())
		{
			m_localbsenemy = { Vector3(0, 0, 0), 0.0f };
		}
		else
		{
			const std::vector<VERTEX_3D>& vertices = mesh->GetVertices();

			std::vector<Vector3> vs;
			vs.reserve(vertices.size());
			for (const auto& v : vertices) {
				vs.push_back(v.Position);
			}

			SRT srt{};
			m_localbsenemy = GM31::GE::Collision::calcBSphere(vs, srt);
		}
	}
	// 敵のパラメータを設定
	DebugUI::RedistDebugFunction([this]() {
		DebugEnemies();
		});

	// プレイヤのパラメータを設定
	DebugUI::RedistDebugFunction([this]() {
		DebugPlayerSRT();
		});

	DebugUI::RedistDebugFunction([this]() {
		DebugCamera();
		});

	DebugUI::RedistDebugFunction([this]() {
		DebugCombat();
		});

}
	
void GameScene::dispose()
{
	m_playerAnimationMesh.reset();
}


// 壁パラメータ調整
void GameScene::DebugWalls()
{
	static int selected_model = 0;

	ImGui::Begin("debug Walls");

	if (ImGui::Button("Add Wall"))
	{
		std::unique_ptr<wall> newWall = std::make_unique<wall>(this);
		newWall->init();
		SRT srt{};
		srt.pos = m_player->getSRT().pos + Vector3(0, 0, 120.0f);
		srt.rot.y = 0.0f;
		newWall->setSRT(srt);
		newWall->setheight(100.0f);
		newWall->setwidth(200.0f);
		newWall->calcEqation();
		m_walls.push_back(std::move(newWall));
		selected_model = static_cast<int>(m_walls.size()) - 1;
	}

	if (m_walls.empty())
	{
		ImGui::Text("No walls");
		ImGui::End();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_walls.size()) - 1);

	// 1. ドロップダウンのプレビュー名を現在の selected_model から作成
	std::string preview_str = std::to_string(selected_model);
	if (preview_str.length() < 3) {
		preview_str.insert(0, 3 - preview_str.length(), '0');
	}
	std::string preview_name = "Wall_" + preview_str;

	// BeginComboを使ってドロップダウンを作成
	if (ImGui::BeginCombo("Wall", preview_name.c_str()))
	{
		for (int i = 0; i < static_cast<int>(m_walls.size()); ++i)
		{
			const bool is_selected = (selected_model == i);

			// 【修正】リストの項目名は selected_model ではなく i を使う
			std::string str = std::to_string(i);
			if (str.length() < 3) {
				// 3桁に足りない分だけ、先頭に '0' を挿入する
				str.insert(0, 3 - str.length(), '0');
			}

			std::string item_name = std::string("Wall_") + str;

			// リストの各アイテムを描画し、クリックされたか判定
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_model = i;
			}

			// ドロップダウンを開いた時、現在選択されているアイテムにフォーカスを合わせる
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Delete Wall") && !m_walls.empty())
	{
		m_walls.erase(m_walls.begin() + selected_model);
		if (m_walls.empty()) {
			selected_model = 0;
		}
		else if (selected_model >= static_cast<int>(m_walls.size())) {
			selected_model = static_cast<int>(m_walls.size()) - 1;
		}
	}

	if (m_walls.empty())
	{
		ImGui::Text("No walls");
		ImGui::End();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_walls.size()) - 1);

	// 2. 選択されている壁のインスタンスを取得
	// ※ m_walls は壁を管理している配列や std::vector を想定しています。実際の変数名に合わせてください。
	auto& currentWall = m_walls[selected_model];
	if (!currentWall)
	{
		ImGui::End();
		return;
	}

	// 3. 選択中の壁から【現在の値】を取得し、スライダー用の変数にセット
	SRT srt = currentWall->getSRT();
	float wallheight = currentWall->getheight();	// ※getterが存在すると仮定
	float wallwidth = currentWall->getwidth();		// ※getterが存在すると仮定
	float wallrotationy = srt.rot.y;
	Vector3 wallposition = srt.pos;

	bool isChanged = false;

	// スライダーが操作されて値が変わった場合、isChanged が true になる
	isChanged |= ImGui::SliderFloat("height", &wallheight, 1.0f, 500.0f);
	isChanged |= ImGui::SliderFloat("width", &wallwidth, 1.0f, 1000.0f);
	isChanged |= ImGui::SliderFloat("rotation Y", &wallrotationy, -PI, PI);
	isChanged |= ImGui::SliderFloat3("position", &wallposition.x, -1000.0f, 1000.0f);

	// 4. パラメータに変更があった場合のみ、選択中の壁に変更を反映
	if (isChanged)
	{
		// 【修正】g_wall... ではなく、ImGuiで操作したローカル変数を使用する
		srt.pos = wallposition;
		srt.rot.y = wallrotationy;

		currentWall->setSRT(srt);
		currentWall->setwidth(wallwidth);
		currentWall->setheight(wallheight);

		currentWall->calcEqation();	// 変更があったので平面の方程式を再計算する
	}

	ImGui::End();
}

// 敵パラメータ調整
void GameScene::DebugEnemies()
{
	static int selected_model = 0;

	ImGui::Begin("debug Enemies");

	if (ImGui::Button("Add Enemy"))
	{
		Vector3 enemyPos = m_player->getSRT().pos + Vector3(120.0f, 0, 0);
		m_enemies.push_back(createEnemyObject(this, m_player.get(), enemyPos, 0.0f, ENEMY_MODEL_SCALE));
		selected_model = static_cast<int>(m_enemies.size()) - 1;
	}

	if (m_enemies.empty())
	{
		ImGui::Text("No enemies");
		ImGui::End();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_enemies.size()) - 1);

	std::string preview_str = std::to_string(selected_model);
	if (preview_str.length() < 3) {
		preview_str.insert(0, 3 - preview_str.length(), '0');
	}
	std::string preview_name = "Enemy_" + preview_str;

	if (ImGui::BeginCombo("Enemy", preview_name.c_str()))
	{
		for (int i = 0; i < static_cast<int>(m_enemies.size()); ++i)
		{
			const bool is_selected = (selected_model == i);

			std::string str = std::to_string(i);
			if (str.length() < 3) {
				str.insert(0, 3 - str.length(), '0');
			}

			std::string item_name = std::string("Enemy_") + str;
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_model = i;
			}
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Delete Enemy") && !m_enemies.empty())
	{
		m_enemies.erase(m_enemies.begin() + selected_model);
		if (m_enemies.empty()) {
			selected_model = 0;
		}
		else if (selected_model >= static_cast<int>(m_enemies.size())) {
			selected_model = static_cast<int>(m_enemies.size()) - 1;
		}
	}

	if (m_enemies.empty())
	{
		ImGui::Text("No enemies");
		ImGui::End();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_enemies.size()) - 1);

	auto& currentEnemy = m_enemies[selected_model];
	SRT srt = currentEnemy->getSRT();
	Vector3 enemyposition = srt.pos;
	float enemyrotationy = srt.rot.y;
	float enemyscale = srt.scale.x;

	bool isChanged = false;
	isChanged |= ImGui::SliderFloat3("position", &enemyposition.x, -1000.0f, 1000.0f);
	isChanged |= ImGui::SliderFloat("rotation Y", &enemyrotationy, -PI, PI);
	isChanged |= ImGui::SliderFloat("scale", &enemyscale, 0.2f, 3.0f);

	if (isChanged)
	{
		srt.pos = enemyposition;
		srt.rot.y = enemyrotationy;
		srt.scale = Vector3(enemyscale, enemyscale, enemyscale);
		currentEnemy->setSRT(srt);
	}

	ImGui::End();
}

void GameScene::DebugPlayerSRT()
{
	ImGui::Begin("debug Player SRT");

	SRT playercurrentsrt = m_player->getSRT();
	Vector3 scale = playercurrentsrt.scale;
	Vector3 rot = playercurrentsrt.rot;
	Vector3 position = playercurrentsrt.pos;

	bool isChanged = false;

	// スライダーが操作されて値が変わった場合、isChanged が true になる
	isChanged |= ImGui::SliderFloat3("rotation", &rot.x, -PI, PI);
	isChanged |= ImGui::SliderFloat3("scale", &scale.x, 0.01f, 10.0f);
	isChanged |= ImGui::SliderFloat3("position", &position.x, -1000.0f, 1000.0f);
	if (ImGui::Button("Reset Player Transform"))
	{
		position = Vector3(0.0f, 0.0f, 0.0f);
		rot = Vector3(0.0f, 0.0f, 0.0f);
		scale = Vector3(1.0f, 1.0f, 1.0f);
		isChanged = true;
	}

	if (isChanged)
	{
		playercurrentsrt.pos = position;
		playercurrentsrt.rot = rot;
		playercurrentsrt.scale = scale;

		m_player->setSRT(playercurrentsrt);
	}

	ImGui::End();
}

void GameScene::DebugCamera()
{
	ImGui::Begin("Third Person Camera");
	bool mouseLookEnabled = m_camera.IsMouseLookEnabled();
	if (ImGui::Checkbox("Mouse movement controls camera", &mouseLookEnabled))
	{
		m_camera.SetMouseLookEnabled(mouseLookEnabled);
	}
	ImGui::Text("Third-person camera: always active");
	ImGui::Text("Right drag: orbit / Mouse wheel: zoom");
	ImGui::Text("Mouse wheel: zoom / WASD: move player");

	const ImVec2 viewportSize(360.0f, 200.0f);
	ImGui::InvisibleButton("CameraOrbitViewport", viewportSize);
	const bool viewportHovered = ImGui::IsItemHovered();
		m_camera.Update(m_player->getSRT().pos, m_player->getSRT().rot.y, viewportHovered);
	const ImVec2 viewportMin = ImGui::GetItemRectMin();
	const ImVec2 viewportMax = ImGui::GetItemRectMax();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(viewportMin, viewportMax, IM_COL32(28, 34, 42, 255));
	drawList->AddRect(viewportMin, viewportMax, IM_COL32(90, 150, 220, 255));
	drawList->AddText(ImVec2(viewportMin.x + 12.0f, viewportMin.y + 12.0f),
		IM_COL32(230, 235, 245, 255), "CAMERA VIEWPORT");
	drawList->AddText(ImVec2(viewportMin.x + 12.0f, viewportMin.y + 38.0f),
		IM_COL32(190, 200, 215, 255), "Drag here to orbit the player");

	if (ImGui::Button("Reset Camera"))
	{
		m_camera.Reset(m_player->getSRT().pos, m_player->getSRT().rot.y);
	}
	ImGui::End();
}

void GameScene::DebugCombat()
{
    ImGui::Begin("1v1 Combat");
    ImGui::Text("State: %s", m_combat.GetStateName().data());
    ImGui::Text("Player Motion: %s", m_player->getMotionStateName());
    ImGui::Text("Left Shift: Jump");
    ImGui::Text("Space / J: Attack");
    ImGui::Text("K / Right Mouse: Guard");
    ImGui::Text("R: Reset Match");
    ImGui::Separator();
    ImGui::Text("Player HP");
    ImGui::ProgressBar(m_combat.GetPlayerHp() / 100.0f, ImVec2(-1.0f, 0.0f));
    ImGui::Text("Enemy HP");
    ImGui::ProgressBar(m_combat.GetEnemyHp() / 100.0f, ImVec2(-1.0f, 0.0f));
    ImGui::End();
}
