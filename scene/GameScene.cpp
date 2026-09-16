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
#include	<iostream>
#include	"GameScene.h"
#include	"../system/LineDrawer.h"
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
#include	"../system/GameFlow.h"
#include	"../system/SoundManager.h"
#include	"../system/CPlane.h"
#include	"../system/CharacterModelProfile.h"
#include	"../system/collision.h"
#include	"../gameobject/player.h"
#include	"../gameobject/field.h"
#include	"../gameobject/wall.h"
#include	"../gameobject/enemy.h"
#include	"../system/SphereDrawer.h"
#include	"../application.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <cstdio>
#include <fstream>
#include <string>

player* GameScene::getplayer()
{
	return m_player.get();
}

// 無名名前空間
namespace {

	// dev_settings.ini から設定値を読む(ローカル専用・未コミットのファイル)。
	// モーションの選定は実際に動かして見比べないと判断できないため、
	// クリップの差し替えをビルドなしで試せるようにしている。
	// 提出時はこのファイルが無ければ既定値で動くので、消し忘れの心配がない。
	std::string GetDevSetting(const std::string& key, const std::string& fallback)
	{
		std::ifstream input("dev_settings.ini");
		std::string line;
		const std::string prefix = key + "=";
		while (std::getline(input, line))
		{
			if (line.rfind(prefix, 0) != 0)
				continue;
			std::string value = line.substr(prefix.size());
			// 行末の空白と復帰文字を落とす(手で編集したときに混ざりやすい)。
			while (!value.empty() &&
				(value.back() == '\r' || value.back() == '\n' ||
				 value.back() == ' ' || value.back() == '\t'))
			{
				value.pop_back();
			}
			if (!value.empty())
				return value;
		}
		return fallback;
	}

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
				"warrior_player",
				"assets/model/FallenPaladin/runtime/FallenPaladin_Player_clean.glb", // CC BY 4.0 / Pigcraft: 軽量化・リギング済み
				"assets/model/FallenPaladin/runtime/"),

			Load3DInfo(
				"car001.x",
				"assets/model/car001.x",			// モチEΝ吁E
				"assets/model/"),					// チEけスチャのパス

			Load3DInfo(
				"car002.x",
				"assets/model/car002.x",			// モチEΝ吁E
				"assets/model/"),					// チEけスチャのパス
	};

	// Cethiel製ドラゴンはほぼ1単位の大きさで作られているため、
	// プレイヤーと並べたときに大型の敵として見える倍率へ拡大する。
	constexpr float ENEMY_MODEL_SCALE = 60.0f;

	// コンパクトな多角形闘技場にして戦闘を見やすくしつつ、
	// ドラゴンの大きな当たり判定と回避一回分の空間を確保する。
	constexpr int ARENA_WALL_SEGMENTS = 16;
	constexpr float ARENA_RADIUS = 300.0f;
	constexpr float ARENA_WALL_HEIGHT = 120.0f;
	const float ARENA_WALL_WIDTH =
		2.0f * ARENA_RADIUS * std::sinf(PI / static_cast<float>(ARENA_WALL_SEGMENTS)) + 8.0f;
	constexpr float ARENA_WALL_DEPTH = 8.0f;
	const Color ARENA_WALL_COLOR(0.38f, 0.27f, 0.19f, 1.0f);

	std::array<Vector3, 8> GetAabbCorners(
		const GM31::GE::Collision::BoundingBoxAABB& box)
	{
		return {
			Vector3(box.min.x, box.min.y, box.min.z),
			Vector3(box.max.x, box.min.y, box.min.z),
			Vector3(box.max.x, box.max.y, box.min.z),
			Vector3(box.min.x, box.max.y, box.min.z),
			Vector3(box.min.x, box.min.y, box.max.z),
			Vector3(box.max.x, box.min.y, box.max.z),
			Vector3(box.max.x, box.max.y, box.max.z),
			Vector3(box.min.x, box.max.y, box.max.z)
		};
	}

	std::array<Vector3, 8> GetObbCorners(
		const GM31::GE::Collision::BoundingBoxOBB& box)
	{
		const Vector3 x = box.axisX * (box.lengthx * 0.5f);
		const Vector3 y = box.axisY * (box.lengthy * 0.5f);
		const Vector3 z = box.axisZ * (box.lengthz * 0.5f);
		return {
			box.worldcenter - x - y - z,
			box.worldcenter + x - y - z,
			box.worldcenter + x + y - z,
			box.worldcenter - x + y - z,
			box.worldcenter - x - y + z,
			box.worldcenter + x - y + z,
			box.worldcenter + x + y + z,
			box.worldcenter - x + y + z
		};
	}

	void DrawBoxEdges(const std::array<Vector3, 8>& corners, const Color& color)
	{
		static constexpr int edges[12][2] = {
			{0,1}, {1,2}, {2,3}, {3,0},
			{4,5}, {5,6}, {6,7}, {7,4},
			{0,4}, {1,5}, {2,6}, {3,7}
		};
		// GeometryShaderの透視補正後、720p画面で約3ピクセルになる太さ。
		SetLineWidth(0.0045f);
		for (const auto& edge : edges)
		{
			const Vector3 direction = corners[edge[1]] - corners[edge[0]];
			const float length = direction.Length();
			if (length > 0.0001f)
				LineDrawerDraw(length, corners[edge[0]], direction, color);
		}
	}

	Vector3 GetAabbCenter(const GM31::GE::Collision::BoundingBoxAABB& box)
	{
		return (box.min + box.max) * 0.5f;
	}

	bool ProjectWorldToScreen(
		const Matrix4x4& view,
		const Matrix4x4& projection,
		const Vector3& worldPosition,
		ImVec2& screenPosition)
	{
		const float width = static_cast<float>(Application::GetWidth());
		const float height = static_cast<float>(Application::GetHeight());
		const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
			DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f),
			0.0f, 0.0f, width, height, 0.0f, 1.0f,
			projection, view, Matrix4x4::Identity);
		const float depth = DirectX::XMVectorGetZ(projected);
		if (depth < 0.0f || depth > 1.0f)
			return false;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		screenPosition = ImVec2(
			viewport->Pos.x + DirectX::XMVectorGetX(projected),
			viewport->Pos.y + DirectX::XMVectorGetY(projected));
		return true;
	}

	void DrawWorldCollisionLabel(
		const Matrix4x4& view,
		const Matrix4x4& projection,
		const Vector3& worldPosition,
		const ImVec2& screenOffset,
		const char* text,
		ImU32 color)
	{
		const float width = static_cast<float>(Application::GetWidth());
		const float height = static_cast<float>(Application::GetHeight());
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 viewportOrigin = viewport->Pos;
		const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
			DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f),
			0.0f, 0.0f, width, height, 0.0f, 1.0f,
			projection, view, Matrix4x4::Identity);
		const float depth = DirectX::XMVectorGetZ(projected);
		if (depth < 0.0f || depth > 1.0f)
			return;

		const ImVec2 anchor(
			viewportOrigin.x + DirectX::XMVectorGetX(projected),
			viewportOrigin.y + DirectX::XMVectorGetY(projected));
		const ImVec2 textSize = ImGui::CalcTextSize(text);
		ImVec2 labelPosition(anchor.x + screenOffset.x, anchor.y + screenOffset.y);
		labelPosition.x = std::clamp(
			labelPosition.x,
			viewportOrigin.x + 8.0f,
			viewportOrigin.x + width - textSize.x - 20.0f);
		labelPosition.y = std::clamp(
			labelPosition.y,
			viewportOrigin.y + 8.0f,
			viewportOrigin.y + height - textSize.y - 16.0f);
		const ImVec2 padding(7.0f, 4.0f);
		const ImVec2 boxMin(labelPosition.x - padding.x, labelPosition.y - padding.y);
		const ImVec2 boxMax(labelPosition.x + textSize.x + padding.x, labelPosition.y + textSize.y + padding.y);

		ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
		const ImVec2 connector(
			std::clamp(anchor.x, boxMin.x, boxMax.x),
			std::clamp(anchor.y, boxMin.y, boxMax.y));
		drawList->AddLine(anchor, connector, IM_COL32(0, 0, 0, 230), 5.0f);
		drawList->AddLine(anchor, connector, color, 2.0f);
		drawList->AddCircleFilled(anchor, 5.0f, IM_COL32(0, 0, 0, 255));
		drawList->AddCircleFilled(anchor, 3.0f, color);
		drawList->AddRectFilled(boxMin, boxMax, IM_COL32(5, 8, 12, 225), 4.0f);
		drawList->AddRect(boxMin, boxMax, color, 4.0f, 0, 2.0f);
		drawList->AddText(labelPosition, IM_COL32(255, 255, 255, 255), text);
	}

	// 壁データ
	struct WallData {
		Vector3 pos{0,0,0};				// 位置	
		Vector3 rot{0,0,0};				// 姿勢
		float height{0};				// 高さ
		float width{0};					// 幁E
		CPlane plane{};					// 平面方程弁E
		wall* pwallobj{nullptr};		// WALL obj
		bool hitflag{ false };
	};

	// 衝突した壁データ
	struct WallCollision {
		WallData walldata;			// 壁データ	
		Vector3 penetration;		// 侵入ベクトル
		Vector3 sliding;			// 壁摺り縺Eクトル
		Vector3 intersectionPoint;	// 交点・域怙近接点・・
	};

	// 壁群と当たり判定を行う・亥｣√→琁E・あたり判定を行う・・
	std::vector<WallCollision> checkWallCollision(
		std::vector<WallData>& walldatas,		// 当たり判定縺E対象壁情報
		float radius,				// 琁E・半征E
		Vector3 pos,				// 現在位置 	
		Vector3 velocity)			// 速度ベクトル
	{
		// 衝突してぁEｋ壁E
		std::vector<WallCollision> hitwalls{};

		// 次の場所を求めめE
		Vector3 nextpos = pos + velocity;

		// 平面と琁E・距離を求めめE
		for (auto& wall : walldatas)
		{
			wall.hitflag = false;

			PLANEINFO pi = wall.plane.GetPlaneInfo();				// 壁縺E平面方程式を取征E
			// 壁と中忁Eｺｧ標縺E距離を求める（法線縺Eクトルを正規化してぁEｋので可能・・
			float lng = pi.plane.a * nextpos.x + pi.plane.b * nextpos.y + pi.plane.c * nextpos.z + pi.plane.d;

			if (fabs(lng) < radius)
				// 半征Eｻ･冁E↑ら衝突してぁEｋ可能性がある縺Eで　精寁E↓判定すめE
			{
				// OOBと琁E・当たり判定を行う(奥行を持たせて老E∴るとぁE≧事（今縺E Z=2.0 固定！E
				GM31::GE::Collision::BoundingBoxOBB obb;
				obb = GM31::GE::Collision::SetOBB(wall.rot, wall.pos, wall.width, wall.height, 2.0f);

				// 琁E・定義
				GM31::GE::Collision::BoundingSphere sphere(nextpos, radius);

				// 琁E→OBBの当たり判宁E
				bool sts = GM31::GE::Collision::CollisionSphereOBB(
					sphere,
					obb);

				// 衝突した縺Eで壁衝突したデータを作諱E
				if (sts) {
					wall.hitflag = true;

					WallCollision wallcollision;					// 衝突した壁縺E詳細惁Eｱ

					wallcollision.walldata = wall;					// 壁データ
					wallcollision.penetration = Vector3(0, 0, 0);	// 侵入ベクトル
					wallcollision.sliding = Vector3(0, 0, 0);		// 壁擦り縺Eクトル

					ClosestPtPointOBB(sphere.center, obb, wallcollision.intersectionPoint);		// 最近接点を求めめE
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

		// 中忁Eｺｧ標を変換
		worldSphere.center = Vector3::Transform(localSphere.center, transform.GetMatrix());

		// 半征Eｒスケール・・YZのぁE■最も大きいスケール値を掛ける・・
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
				// 描画モデルは意図的に大きくしているが、物理判定の半径は見やすい
				// 胴体サイズに抑え、敵が数メートル手前で止まらないようにする。
				srtA.scale = Vector3::Min(srtA.scale, Vector3(8.0f, 8.0f, 8.0f));
				srtB.scale = Vector3::Min(srtB.scale, Vector3(8.0f, 8.0f, 8.0f));
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
		const GM31::GE::Collision::BoundingSphere& localEnemySphere,
		bool lockPlayerPosition)
	{
		if (playerObj == nullptr) {
			return;
		}

		SRT playerSrt = playerObj->getSRT();
		GM31::GE::Collision::BoundingSphere playerSphere = transformBSphere(localPlayerSphere, playerSrt);

		for (auto& enemyObj : enemies) {
			SRT enemySrt = enemyObj->getSRT();
			SRT enemyCollisionSrt = enemySrt;
			enemyCollisionSrt.scale = Vector3::Min(enemyCollisionSrt.scale, Vector3(8.0f, 8.0f, 8.0f));
			GM31::GE::Collision::BoundingSphere enemySphere = transformBSphere(localEnemySphere, enemyCollisionSrt);

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

			const float overlap = hitDistance - distance;
			if (lockPlayerPosition)
			{
				// 攻撃中はプレイヤーをアニメーションの固定根にする。
				// めり込み補正を敵側へ集め、プレイヤーが滑って見えないようにする。
				enemySrt.pos += pushDir * overlap;
			}
			else
			{
				const float pushLength = overlap * 0.5f;
				playerSrt.pos -= pushDir * pushLength;
				enemySrt.pos += pushDir * pushLength;
			}

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

void GameScene::StartEnemyIntro()
{
	// タイトル曲を登場演出へ持ち越さない。
	// Rキーで再戦するときも、戦闘開始までは無音になるようにする。
	SoundManager::StopBgm();
	m_enemyIntroActive = true;
	m_enemyRoarPlayed = false;
	m_enemyIntroTime = 0.0f;
	m_enemyIntroBattleBlendActive = false;
	m_enemyIntroBattleBlendTime = 0.0f;
	// 再戦時も前回の攻撃ズームをカットシーン後へ持ち越さない。
	m_cameraDistanceCurrent = m_cameraBaseDistance;
	m_enemyAnimationFrame = 0;
	m_enemyAnimationTick = 0.0f;
	m_previousEnemyMotionState = enemy::MotionState::Approach;

	if (m_enemies.empty())
		return;

	SRT enemySrt = m_enemies.front()->getSRT();
	enemySrt.pos = m_enemyIntroLandingPosition + Vector3(0.0f, 150.0f, 0.0f);
	// ドラゴンは-Z側へ降り、プレイヤーは原点（ドラゴンから見て+Z側）にいる。
	// 降下中もドラゴンの正面がプレイヤーを向くようにする。
	enemySrt.rot = Vector3(0.0f, PI, 0.0f);
	m_enemies.front()->setSRT(enemySrt);
	m_enemies.front()->setVel(Vector3(0.0f, 0.0f, 0.0f));
	m_enemies.front()->resetEncounter();
}

void GameScene::UpdateEnemyIntro(float deltaSeconds)
{
	if (m_enemies.empty())
	{
		// 敵がいない(素振り)ときは登場演出を飛ばし、すぐ操作を返す。
		// 演出の途中で敵を消した場合もカットシーンの構図のまま止まらないよう、戦闘カメラへ戻す。
		m_enemyIntroActive = false;
		SoundManager::PlayGameBgm();
		m_camera.BeginBattleTransition(
			m_player->getSRT().pos,
			m_player->getSRT().rot.y);
		return;
	}

	constexpr float fallDuration = 1.35f;
	constexpr float roarDelay = 0.12f;
	// 読み込んだ咆哮を最後まで聞かせてからプレイヤーへ操作を返す。
	constexpr float battleStartTime = 4.55f;
	m_enemyIntroTime += deltaSeconds;

	SRT enemySrt = m_enemies.front()->getSRT();
	if (m_enemyIntroTime < fallDuration)
	{
		const float progress = std::clamp(m_enemyIntroTime / fallDuration, 0.0f, 1.0f);
		// 短いイーズアウトで着地を見せ、瞬間移動に見えないようにする。
		// 初期高度を大きくして落下演出として分かりやすくする。
		const float remaining = 1.0f - progress;
		enemySrt.pos = m_enemyIntroLandingPosition +
			Vector3(0.0f, 150.0f * remaining * remaining * remaining, 0.0f);
	}
	else
	{
		enemySrt.pos = m_enemyIntroLandingPosition;
	}
	enemySrt.rot.y = PI;
	m_enemies.front()->setSRT(enemySrt);
	m_enemies.front()->setVel(Vector3(0.0f, 0.0f, 0.0f));

	// 降下中は通常のプレイヤー追従カメラから切り替え、
	// ドラゴンを少し近い斜め前方の構図に収める。
	// ゲーム原点ではなくモデルの高さを基準にして、空中から着地まで
	// カメラが安定してドラゴンを映すようにする。
	const SRT dragonRenderSrt = m_enemies.front()->getRenderSRT();
	const float dragonHeight = std::max(
		(m_localEnemyMeshBounds.max.y - m_localEnemyMeshBounds.min.y) *
		dragonRenderSrt.scale.y,
		40.0f);
	// ゲーム上の原点ではなく、描画後のモデル境界の中心を注視する。
	// ドラゴンのモデルは回転補正と原点のずれがあるため、SRT位置だけを見ると
	// 画面内で上下にずれてしまう。
	const Vector3 localDragonCenter =
		(m_localEnemyMeshBounds.min + m_localEnemyMeshBounds.max) * 0.5f;
	const Vector3 dragonFocus = Vector3::Transform(
		localDragonCenter, dragonRenderSrt.GetMatrix());
	const Vector3 cinematicCameraPosition = dragonFocus +
		Vector3(0.0f, std::max(30.0f, dragonHeight * 0.42f),
			// 画面いっぱいにせず、ドラゴン全体が読める距離を保つ。
			std::clamp(dragonHeight * 1.45f, 120.0f, 180.0f));
	m_camera.SetCinematicView(cinematicCameraPosition, dragonFocus);

	if (!m_enemyRoarPlayed && m_enemyIntroTime >= fallDuration + roarDelay)
	{
		SoundManager::PlayEnemyRoar();
		m_enemyRoarPlayed = true;
	}

	UpdateEnemyAnimation(deltaSeconds);
	if (m_enemyIntroTime >= battleStartTime)
	{
		SoundManager::PlayGameBgm();
		// 戦闘開始時のWalk先頭へ、直前に表示したIdle姿勢からつなぐ。
		// 位置は補間せず、回転だけを短時間ブレンドする。
		m_enemyIntroBattleBlendActive = true;
		m_enemyIntroBattleBlendTime = 0.0f;
		m_enemyIntroBlendFromFrame = m_enemyLastSampledFrame;
		m_enemyIntroBlendFromFraction = m_enemyLastSampledFraction;
		// カットシーン中のIdle姿勢を戦闘開始後のWalkへ持ち越さない。
		// ここで先頭へ戻すことで、遷移直後に後ろ足だけ前フレームの姿勢へ
		// 残るスナップを防ぐ。
		m_enemyAnimationFrame = 0;
		m_enemyAnimationTick = 0.0f;
		m_enemyIntroActive = false;
		m_enemyIntroTime = battleStartTime;
		m_enemies.front()->setSRT(enemySrt);
		m_enemies.front()->resetEncounter();
		m_camera.BeginBattleTransition(
			m_player->getSRT().pos,
			m_player->getSRT().rot.y);
		// 調整用: dev_settings.ini の auto_lockon=1 で戦闘開始と同時にロックオンする。
		// ロックオン中の移動(横歩き・後ろ歩き)やカメラを、毎回Tabを押さずに確かめるため。
		if (GetDevSetting("auto_lockon", "0") == "1")
			m_lockOnTarget = true;
		// 調整用: enemy_passive=1 で敵を攻撃させない(プレイヤーの攻撃モーションを落ち着いて見るため)。
		m_enemies.front()->setPassive(GetDevSetting("enemy_passive", "0") == "1");
	}
}

void GameScene::DebugAudio()
{
	if (!ImGui::BeginTabItem("音量"))
		return;

	float bgmVolume = SoundManager::GetBgmVolume();
	if (ImGui::SliderFloat("戦闘BGM", &bgmVolume, 0.0f, 1.0f, "%.2f"))
		SoundManager::SetBgmVolume(bgmVolume);

	float sfxVolume = SoundManager::GetSfxVolume();
	if (ImGui::SliderFloat("効果音", &sfxVolume, 0.0f, 1.0f, "%.2f"))
		SoundManager::SetSfxVolume(sfxVolume);

	ImGui::Text("次回の起動へ自動で保存されます");
	ImGui::Text("保存先: audio_settings.ini");
	if (ImGui::Button("音量を初期値へ戻す"))
		SoundManager::ResetVolumes();
	ImGui::EndTabItem();
}

void GameScene::update(uint64_t deltatime)
{
    auto& input = CInputManager::GetInstance();
	const float deltaSeconds = std::clamp(static_cast<float>(deltatime) * 0.000001f, 0.0f, 0.1f);
	// 視点操作。フロム作品(PC版)と同じく、マウスを動かすだけでカメラが回る。
	// そのためカーソルをゲーム画面に閉じ込めて隠す。デバッグ表示を触るときは左Altでカーソルを出す。
	if (input.IsKeyTriggered(DIK_LMENU))
		m_mouseLookCaptured = !m_mouseLookCaptured;
	// 調整用: dev_settings.ini の auto_walk で自動的に歩かせる(1=前へ、2=右へ、3=左へ)。
	// 撮影で動きを確認するとき、外からキー入力を送らずに済ませるため。
	if (m_player)
	{
		static const std::string autoWalk = GetDevSetting("auto_walk", "0");
		m_player->setDebugForcedMove(
			autoWalk == "1" ? 1.0f : 0.0f,
			autoWalk == "2" ? 1.0f : (autoWalk == "3" ? -1.0f : 0.0f));
	}
	const bool lockCursor = m_mouseLookCaptured && !m_resultRequested;
	DebugUI::SetCursorLocked(lockCursor);
	DebugUI::SetCursorVisible(!lockCursor);
	if (!m_enemyIntroActive && !ImGui::GetIO().WantCaptureKeyboard &&
		input.IsKeyTriggered(DIK_TAB))
	{
		if (m_lockOnTarget)
		{
			m_lockOnTarget = false;
		}
		else if (!m_enemies.empty() && !m_combat.IsEnemyDefeated())
		{
			m_lockOnTarget = true;
		}
		else if (m_player)
		{
			// ロックオンできる相手がいないときは、カメラをプレイヤーの背後へ回す
			// (フロム作品でロックオン対象がいないときの挙動)。
			m_camera.RequestRecenter(m_player->getSRT().rot.y);
		}
	}

    if (input.IsKeyTriggered(DIK_R))
    {
		m_combat.Reset();
		ApplyDebugEnemyStartHp();
		m_playerWeaponTrail.Clear();
		m_hitEffect.Clear();
		m_lockOnTarget = false;
		m_lastPlayerComboStep = 0;
		m_lastPlayerHeavyAttack = false;
		m_playerStamina = PLAYER_MAX_STAMINA;
		StartEnemyIntro();
        SRT playerReset = m_player->getSRT();
        playerReset.pos = Vector3(0, 0, 0);
        playerReset.rot = Vector3(0, 0, 0);
		m_player->setSRT(playerReset);
		m_player->resetMotion();
	}
	if (m_enemyIntroActive)
	{
		UpdateEnemyIntro(deltaSeconds);
		return;
	}
	if (m_enemies.empty() || m_combat.IsEnemyDefeated())
		m_lockOnTarget = false;
	const bool attackTriggered = !ImGui::GetIO().WantCaptureMouse &&
		input.IsMouseTriggered(CInputManager::MOUSE_LEFT);
	const bool heavyAttackTriggered = !ImGui::GetIO().WantCaptureMouse &&
		input.IsMouseTriggered(CInputManager::MOUSE_RIGHT);
	const bool playerDodgeActive = m_player->isDodging();
	// コンボ入力はボタンを押した瞬間だけ受け付ける。
	// ボタンを押し続けても次の攻撃を予約せず、追加段数には再クリックを必要とする。
	// 回避中のクリックは戦闘システムへ渡さない。回避中に攻撃を開始すると、
	// player::update()が回避移動を優先してreturnするため、回避の移動量を
	// 持ったまま攻撃モーションだけが始まる状態になる。
	// 吹き飛ばされている間も攻撃を受け付けない。飛ばされながら剣を振れると、
	// 被弾が「読み違えた代償」にならない。
	const bool playerKnockedBack = m_player->isKnockedBack();
	// 走っている最中に弱攻撃を押すとダッシュ攻撃(突進斬り)になる。
	// 判定は前フレームの移動状態で行い、戦闘側とアニメーション側で同じ値を使う。
	const bool playerWasRunning = m_player->getMotionState() == player::MotionState::Run;
	const bool attackInput = attackTriggered && !playerDodgeActive && !playerKnockedBack;
	const bool heavyAttackInput = heavyAttackTriggered && !playerDodgeActive && !playerKnockedBack;
	const bool attackStarted = (attackInput || heavyAttackInput) &&
		m_combat.CanStartPlayerAttack();
	if (attackStarted)
	{
		if (heavyAttackInput)
			m_playerAnimator.PlayHeavyAttackMotion(1);
		else if (playerWasRunning)
			m_playerAnimator.PlayDashAttackMotion();
		else
			m_playerAnimator.PlayAttackMotion(1);
		m_lastPlayerComboStep = 1;
		m_lastPlayerHeavyAttack = heavyAttackInput;
	}
	const bool dodgeTriggered = !ImGui::GetIO().WantCaptureKeyboard && input.IsKeyTriggered(DIK_SPACE);
	const bool sprinting = !ImGui::GetIO().WantCaptureKeyboard &&
		// 移動キー(WASD)と同じく、DirectInputで取れない場合はWin32のキー状態でも読む(ゲームが前面のときだけ)。
		(input.IsKeyPressed(DIK_LSHIFT) ||
		 (GetForegroundWindow() == Application::GetWindow() &&
		  (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0)) &&
		!m_combat.IsPlayerAttacking() && !m_player->isDodging();
	if (sprinting)
		m_playerStamina = std::max(0.0f, m_playerStamina - 28.0f * deltaSeconds);
	else if (!m_player->isDodging())
		m_playerStamina = std::min(PLAYER_MAX_STAMINA, m_playerStamina + 20.0f * deltaSeconds);
	const bool canCancelAttack =
		m_combat.CanCancelPlayerAttack(m_attackCancelEndFrame);
	const bool canDodge = dodgeTriggered && m_playerStamina >= 25.0f &&
		!attackStarted && !m_player->isDodging() && !playerKnockedBack &&
		(!m_combat.IsPlayerAttacking() || canCancelAttack);
	if (canDodge)
	{
		m_playerStamina -= 25.0f;
		SoundManager::PlayDodge();
		// 攻撃中は足を固定するが、設定したキャンセル時間内の回避だけは
		// 攻撃を中断して逃げられるようにする。
		if (m_combat.IsPlayerAttacking())
			m_combat.CancelPlayerAttack();
		m_playerAnimator.PlayDodgeMotion();
	}
	// 攻撃中は移動キーを押していても移動させない。攻撃中の下半身へ
	// ロコモーションを重ねる実装は残すが、プレイヤーのワールド移動は許可しない。
	// これにより「歩行アニメーションを攻撃へ重ねる」ことと「歩きながら攻撃する」
	// ことを分離し、今回の仕様ではその場で攻撃する。
	const bool attackAnimationPlaying = m_playerAnimator.IsMotionPlaying() &&
		!m_player->isDodging() && !canDodge;
	const bool lockPlayerMovement =
		m_combat.IsPlayerAttacking() ||
		(attackStarted && !canDodge) ||
		attackAnimationPlaying;

	auto walldatas = createWallData(m_walls);
	m_hitWallObjects.clear();

	SRT prevPlayerSrt = m_player->getSRT();
	GM31::GE::Collision::BoundingSphere prevPlayerSphere = transformBSphere(m_localbsplayer, prevPlayerSrt);

	// ロックオンしても体は敵へ向けない(ユーザー判断 2026-09-15)。
	// モンスターハンターのロックオン(ターゲットカメラ)はカメラだけが敵を追い、
	// キャラクターは入力した方向を向いて動くため。
	// フロム作品のように体まで敵を向かせる場合は、この定数をtrueにする。
	// その場合は移動のブレンドツリーの横歩き・後ろ歩きが使われる。
	constexpr bool LOCK_ON_FACES_TARGET = false;
	if (LOCK_ON_FACES_TARGET && m_lockOnTarget && !m_enemies.empty() && !m_enemyIntroActive)
		m_player->setLockOnTarget(true, m_enemies.front()->getSRT().pos);
	else
		m_player->setLockOnTarget(false, Vector3(0.0f, 0.0f, 0.0f));
	m_player->update(deltatime, m_camera.GetYaw(), lockPlayerMovement, sprinting, canDodge);

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
	// カメラの自動追従に使う実際の移動速度。吹き飛ばされている間は自分で動いていないので0にする
	// (飛ばされた向きへカメラが回ると、何に飛ばされたのかが画面から外れる)。
	m_playerWorldVelocity = (deltaSeconds > 0.0001f && !m_player->isKnockedBack())
		? adjustedPlayerMove / deltaSeconds
		: Vector3(0.0f, 0.0f, 0.0f);
	if (adjustedPlayerMove.Length() < playerMove.Length()) {
		m_player->setVel(Vector3(0, 0, 0));
	}
	if (m_playerAnimationMesh && m_player)
	{
		// 移動のブレンドツリーへ渡す、キャラクターから見た速度(単位/秒)。
		// 壁に押し戻された分を含めるため、入力ではなく実際に動いた量から求める
		// (壁に向かって歩き続けても、その場で足踏みしないように)。
		// 吹き飛ばし・回避の移動はブレンドツリーへ入れない(それぞれ専用の見た目がある)。
		float velocityRight = 0.0f;
		float velocityForward = 0.0f;
		if (deltaSeconds > 0.0001f &&
			!m_player->isKnockedBack() && !m_player->isDodging())
		{
			const SRT currentPlayerSrt = m_player->getSRT();
			const float moveX = (currentPlayerSrt.pos.x - prevPlayerSrt.pos.x) / deltaSeconds;
			const float moveZ = (currentPlayerSrt.pos.z - prevPlayerSrt.pos.z) / deltaSeconds;
			// プレイヤーの正面は(-sin, -cos)。右は(正面.z, -正面.x)。
			// カメラの右方向(cos, sin)と同じ規約になるように決めている
			// (カメラと同じ向きを向いたとき、キャラクターの右 = 画面の右)。
			const float yaw = currentPlayerSrt.rot.y;
			const float forwardX = -std::sinf(yaw);
			const float forwardZ = -std::cosf(yaw);
			const float rightX = forwardZ;
			const float rightZ = -forwardX;
			velocityForward = moveX * forwardX + moveZ * forwardZ;
			velocityRight = moveX * rightX + moveZ * rightZ;
		}
		m_playerAnimator.Update(
			*m_playerAnimationMesh,
			m_playerBoneComb,
			{
				// 吹き飛ばし中は歩行扱いにしない。飛ばされる速さで脚が歩行を刻むと、
				// 自分で後ろへ走っているように見えるため。
				m_player->getMotionState() != player::MotionState::Knockback &&
				(m_player->getMotionState() == player::MotionState::Walk ||
				m_player->getMotionState() == player::MotionState::Run ||
				m_player->getMotionState() == player::MotionState::Dodge ||
				m_player->getVel().Length() > 0.01f),
				m_player->getMotionState() == player::MotionState::Run,
				m_player->getMotionState() == player::MotionState::Jump,
				m_player->getMotionTime(),
				deltaSeconds,
				velocityRight,
				velocityForward
			});

		// 前転中の接地。前転のモーションは腰を中心に体を一回転させるので、立ったときの高さのままだと
		// 腰の高さで宙返りしているように見え、影からも離れる(実機キャプチャで確認)。
		// 変形後の体の一番低い点を地面に合わせ、転がる体が地面に着くようにする。
		// 前転の最初と最後は立ち姿勢なので、段差が出ないよう途中だけ効かせる(sinで0→1→0)。
		// 体のすべての頂点をCPUで変形するので、回避中だけ計算する。
		if (m_player->isDodging())
		{
			const float progress = m_player->getDodgeProgress();
			const float weight = std::sin(progress * PI);
			const float groundedOffset = m_playerGroundY -
				m_playerAnimationMesh->GetAnimatedLocalMinY() * m_player->getSRT().scale.y;
			m_player->setVisualGroundOffsetY(
				m_playerStandingGroundOffsetY +
				(groundedOffset - m_playerStandingGroundOffsetY) * weight);		}
		else
		{
			m_player->setVisualGroundOffsetY(m_playerStandingGroundOffsetY);
		}

		// 接地している足のつま先をワールドへ固定し、脚をIKで合わせる(足の滑りを消す)。
		UpdateFootLock(deltaSeconds);

		// 攻撃の踏み込み。アニメーターが溜めた「キャラクターから見た移動量」を
		// ワールドの向きへ直し、次の更新でプレイヤーの位置へ足す(壁の衝突補正を通る)。
		float rootRight = 0.0f;
		float rootForward = 0.0f;
		if (m_playerAnimator.ConsumeRootMotion(rootRight, rootForward) &&
			!m_player->isKnockedBack())
		{
			const float yaw = m_player->getSRT().rot.y;
			const float forwardX = -std::sinf(yaw);
			const float forwardZ = -std::cosf(yaw);
			const float rightX = forwardZ;
			const float rightZ = -forwardX;
			m_player->applyRootMotion(Vector3(
				forwardX * rootForward + rightX * rootRight,
				0.0f,
				forwardZ * rootForward + rightZ * rootRight));
		}
	}

	for (auto& e : m_enemies) {
		SRT prevEnemySrt = e->getSRT();
		const enemy::MotionState previousEnemyMotionState = e->getMotionState();
		SRT prevEnemyCollisionSrt = prevEnemySrt;
		prevEnemyCollisionSrt.scale = Vector3::Min(
			prevEnemyCollisionSrt.scale, Vector3(8.0f, 8.0f, 8.0f));
		GM31::GE::Collision::BoundingSphere prevEnemySphere =
			transformBSphere(m_localbsenemy, prevEnemyCollisionSrt);

		e->update(deltatime);

		SRT enemySrt = e->getSRT();
		if (previousEnemyMotionState != enemy::MotionState::Active &&
			e->getMotionState() == enemy::MotionState::Active)
		{
			SoundManager::PlayDragonAttack();
		}
		SRT nextEnemyCollisionSrt = enemySrt;
		nextEnemyCollisionSrt.scale = Vector3::Min(
			nextEnemyCollisionSrt.scale, Vector3(8.0f, 8.0f, 8.0f));
		GM31::GE::Collision::BoundingSphere nextEnemySphere =
			transformBSphere(m_localbsenemy, nextEnemyCollisionSrt);
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
		e->setSRT(enemySrt);
		e->setVel(adjustedEnemyMove);
	}
	UpdateEnemyAnimation(deltaSeconds);

	resolveEnemyCollisions(m_enemies, m_localbsenemy);
	resolvePlayerEnemyCollisions(
		m_player.get(),
		m_enemies,
		m_localbsplayer,
		m_localbsenemy,
		lockPlayerMovement);
	// 命中演出の残り時間を減らす。
	m_enemyHitFlashTime = std::max(0.0f, m_enemyHitFlashTime - deltaSeconds);
	m_playerDamageFlashTime = std::max(0.0f, m_playerDamageFlashTime - deltaSeconds);

	// 壁への寄せ(衝突)より先に攻撃中の寄せを決める。
	// SetCollisionDistanceは基準距離を上限としてクランプするため、
	// 先に基準距離を確定させておかないと壁際で挙動がちぐはぐになる。
	UpdateCombatCameraDistance(deltaSeconds);
	UpdateArenaCameraCollision();
	UpdateGameplayCamera(deltaSeconds);
	if (m_playerAnimationMesh && m_player)
	{
		// 移動と衝突補正が終わった後に剣の位置を一度だけ確定する。
		// 描画と攻撃判定が同じワールド行列を使うようにする。
		const Matrix4x4 playerWorld = m_player->getRenderSRT().GetMatrix();
		m_playerAnimationMesh->UpdateSwordWorldTransform(playerWorld);

		// 剣の軌跡。攻撃判定が出ている間だけ点を足す。
		// 渡すのは武器ボーンから求めた刃のワールド座標だけなので、
		// 剣やプレイヤーのモデルを差し替えても、この処理は変更しなくてよい。
		Vector3 trailBase{};
		Vector3 trailTip{};
		Vector3 trailPreviousTip{};
		// 攻撃判定が出ている間だけ軌跡を残す。
		// 「刃が危険な時間」がそのまま光って見えるので、
		// 予兆と判定を読ませたいこのゲームの方針とも噛み合う。
		if (m_combat.IsPlayerAttackActive() &&
			m_playerAnimationMesh->GetSwordWorldSweep(
				trailBase, trailTip, trailPreviousTip))
		{
			m_playerWeaponTrail.Push(trailBase, trailTip);
		}
	}
	m_playerWeaponTrail.Update(deltaSeconds);
	m_hitEffect.Update(deltaSeconds);
    if (!m_enemies.empty())
    {
        const Vector3 enemyPosition = m_enemies.front()->getSRT().pos;
		Vector3 swordBase{};
		Vector3 swordTip{};
		Vector3 previousSwordTip{};
		const bool swordTransformValid = m_playerAnimationMesh &&
			m_playerAnimationMesh->GetSwordWorldSweep(swordBase, swordTip, previousSwordTip);
		// 敵とプレイヤーの受け判定はモデル描画と同じSRTを使う。
		// 読み込んだ頂点から寸法を作るため、モデルの倍率や回転を変えても
		// 表示モデルとOBBの位置がずれない。
		const auto playerObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localPlayerMeshBounds,
			m_player->getRenderSRT());
		const auto enemyObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localEnemyMeshBounds,
			m_enemies.front()->getRenderSRT());
		const float enemyHpBeforeAttack = m_combat.GetEnemyHp();
		const float playerHpBeforeAttack = m_combat.GetPlayerHp();
		// 敵AIが選んだ攻撃を戦闘システムへ渡す。
		// 予兆として見えているモーションと、実際のダメージ・射程・タイミングを一致させるため。
		m_combat.SetEnemyAttackKind(m_enemies.front()->getAttackKind());
		// 攻撃判定を正面からの角度で絞るため、敵の向きも渡す。
		// 描画用SRTには演出のひねりが入っているので、必ず物理SRTの向きを使う。
		m_combat.SetEnemyFacingYaw(m_enemies.front()->getSRT().rot.y);
		m_combat.SetPlayerSprinting(playerWasRunning);
		m_combat.Update(
			deltatime,
			m_player->getSRT().pos,
			enemyPosition,
			attackInput,
			heavyAttackInput,
			m_enemies.front()->getMotionState() == enemy::MotionState::Windup,
			// 吹き飛ばされている間は無敵にする。飛ばされた先で次の攻撃に拾われ続けると、
			// 1回の読み違えで何もできないまま体力を削り切られてしまう。
			m_player->isKnockedBack() ||
			(m_player->isDodging() &&
			m_player->getDodgeFrame() >= m_dodgeInvincibleStartFrame &&
			m_player->getDodgeFrame() <= m_dodgeInvincibleEndFrame),
			swordBase,
			swordTip,
			previousSwordTip,
			swordTransformValid,
			playerObb,
			enemyObb);
		if (m_combat.GetEnemyHp() < enemyHpBeforeAttack)
		{
			// この音は剣が接触した音として聞こえるため、攻撃入力時ではなく
			// 戦闘システムがダメージを確定したフレームだけ再生する。
			SoundManager::PlaySwordSwing();

			// 命中の手応え。ヒットストップ・カメラの揺れ・敵の発光を同じ瞬間に重ねる。
			// 強攻撃と、敵の硬直(隙)へ入れた一撃は手応えを強くして、
			// 「読みが当たった」ことが体で分かるようにする。
			const bool heavyHit = m_combat.IsPlayerHeavyAttack();
			const bool punishHit = m_enemies.front()->isInRecovery();
			const float hitScale = (heavyHit ? 1.5f : 1.0f) * (punishHit ? 1.4f : 1.0f);

			m_playerAnimator.TriggerHitStop(std::min(0.05f * hitScale, 0.12f));
			m_camera.TriggerShake(1.1f * hitScale, 0.22f);
			m_enemyHitFlashTime = 0.12f;

			// 怯み値を溜める。隙へ入れた攻撃は多く溜まる。
			// しきい値を超えたら敵が怯み、構えていた攻撃は取り消される。
			const Combat::AttackData& playerAttack = heavyHit
				? Combat::PlayerHeavyAttack()
				: Combat::PlayerWeakAttack();
			const float postureDamage = static_cast<float>(playerAttack.postureDamage) *
				(punishHit ? Combat::Tuning::PUNISH_POSTURE_MULTIPLIER : 1.0f);
			if (m_enemies.front()->addPostureDamage(postureDamage, m_player->getSRT().pos))
			{
				// 敵AIだけ怯ませて戦闘側の攻撃を残すと、のけぞっている敵から
				// ダメージが飛んでくるので、戦闘側の攻撃も取り消す。
				m_combat.CancelEnemyAttack();
				// 怯ませた瞬間は通常の命中より一段強く揺らし、止める。
				m_playerAnimator.TriggerHitStop(0.12f);
				m_camera.TriggerShake(2.2f, 0.30f);
			}

			// 火花とダメージ数値。命中点は剣先のワールド座標を使う。
			// 剣先は武器ボーンから求めているので、武器やモデルを差し替えても成立する。
			const Vector3 hitPoint = swordTransformValid ? swordTip : enemyPosition;
			if (swordTransformValid)
			{
				// 刃の進行方向へ弾く。前フレームの剣先との差から求める。
				Vector3 bladeDirection = swordTip - previousSwordTip;
				if (bladeDirection.LengthSquared() < 0.0001f)
					bladeDirection = swordTip - swordBase;
				m_hitEffect.Spawn(
					hitPoint,
					bladeDirection,
					heavyHit ? 20 : 14,
					hitScale);
			}
		}

		// 被弾側の手応え。与ダメージより強く揺らし、画面端を赤くする。
		// 「今食らった」ことが分からないと、読み合いに失敗した実感が出ない。
		// 敵の弱り具合を残り体力から更新する。体力ゲージは出さず、体の変化で伝える。
		// 段階が悪くなった瞬間は敵がよろめくので、構えていた攻撃は取り消す。
		if (m_enemies.front()->setHealthRatio(
			m_combat.GetEnemyHp() / m_combat.GetEnemyMaxHp()))
		{
			m_combat.CancelEnemyAttack();
			m_camera.TriggerShake(1.8f, 0.30f);
			// 瀕死に入った瞬間だけ咆哮させる。モンスターハンターで弱ったモンスターが
			// 苦しげに吠えるのと同じく、「もう少しで倒せる」ことを音でも伝える。
			if (m_enemies.front()->getCondition() == Combat::EnemyCondition::Dying)
				SoundManager::PlayEnemyRoar();
		}

		if (m_combat.GetPlayerHp() < playerHpBeforeAttack)
		{
			m_camera.TriggerShake(2.6f, 0.35f);
			m_playerDamageFlashTime = 0.45f;

			// 食らった攻撃の重さに応じて吹き飛ばす。向きは敵から離れる方向。
			// 攻撃中なら攻撃は取り消す(飛ばされながら剣を振り続けないように)。
			const Combat::KnockbackData knockback =
				Combat::EnemyKnockbackOf(m_enemies.front()->getAttackKind());
			const Vector3 awayFromEnemy = m_player->getSRT().pos - enemyPosition;
			if (m_combat.IsPlayerAttacking())
				m_combat.CancelPlayerAttack();
			m_player->applyKnockback(awayFromEnemy, knockback.speed, knockback.seconds);
			m_playerAnimator.PlayImpactMotion();
		}

		// どちらかのHPが0になったら結果を保存してリザルト画面へ遷移する。
		if (!m_resultRequested &&
			(m_combat.IsEnemyDefeated() || m_combat.IsPlayerDefeated()))
		{
			GameFlow::SetResult(
				m_combat.IsEnemyDefeated()
				? GameFlow::Result::Victory
				: GameFlow::Result::Defeat);
			m_resultRequested = true;
			GameFlow::RequestScene("ResultScene");
		}
    }
	else
	{
		// 敵がいなくても剣を振れるようにする(素振り)。攻撃の段・判定・硬直は
		// 敵がいるときと同じ表で進めるので、モーションの見た目とつなぎをそのまま確かめられる。
		m_combat.SetPlayerSprinting(playerWasRunning);
		m_combat.UpdateWithoutEnemy(deltatime, attackInput, heavyAttackInput);
		// 攻撃対象がいなくても、調整用にプレイヤーと攻撃判定は
		// 描画中の剣へ追従し続ける。
		Vector3 swordBase{};
		Vector3 swordTip{};
		Vector3 previousSwordTip{};
		const bool swordTransformValid = m_playerAnimationMesh &&
			m_playerAnimationMesh->GetSwordWorldSweep(
				swordBase, swordTip, previousSwordTip);
		const auto playerObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localPlayerMeshBounds,
			m_player->getRenderSRT());
		m_combat.UpdatePlayerCollisionDebug(
			swordBase,
			swordTip,
			previousSwordTip,
			swordTransformValid,
			playerObb);
	}

	// 戦闘側でコンボが次の段へ進んだら、その段のモーションを再生する。
	// 敵の有無に関係なく通す(敵がいないときもコンボを出せるように)。
	const int comboStep = m_combat.GetPlayerComboStep();
	if (m_combat.IsPlayerAttacking() &&
		(comboStep != m_lastPlayerComboStep ||
		 m_combat.IsPlayerHeavyAttack() != m_lastPlayerHeavyAttack))
	{
		if (m_combat.IsPlayerHeavyAttack())
			m_playerAnimator.PlayHeavyAttackMotion(comboStep);
		else
			m_playerAnimator.PlayAttackMotion(comboStep);
		m_lastPlayerComboStep = comboStep;
		m_lastPlayerHeavyAttack = m_combat.IsPlayerHeavyAttack();
	}
	if (!m_combat.IsPlayerAttacking())
	{
		m_lastPlayerComboStep = 0;
		m_lastPlayerHeavyAttack = false;
	}
}

void GameScene::UpdateFootLock(float deltaSeconds)
{
	if (!m_playerAnimationMesh || !m_player)
		return;
	if (!m_legChainsResolved)
	{
		// 調整用: dev_settings.ini の foot_lock=0 で切れる(効果の比較に使う)。
		m_footLockEnabled = GetDevSetting("foot_lock", "1") != "0";
		// 骨の名前から左右の脚を1度だけ探す。モデルを差し替えても名前から拾えるようにしている。
		FootLock::LegBoneNames left{};
		FootLock::LegBoneNames right{};
		FootLock::FindLegBones(m_playerAnimationMesh->GetBoneNames(), left, right);
		if (left.IsValid())
			m_leftLegChain = { left.upperLeg, left.lowerLeg, left.foot, left.toe };
		if (right.IsValid())
			m_rightLegChain = { right.upperLeg, right.lowerLeg, right.foot, right.toe };
		m_legChainsResolved = true;
	}
	// 移動と待機のときだけ効かせる。攻撃・回避・吹き飛ばしは脚が地面から離れる動きで、
	// 固定すると元のモーションを壊してしまう(記事の「少し滑るほうが、モーションを壊すよりまし」)。
	const bool locomotion =
		!m_combat.IsPlayerAttacking() &&
		!m_playerAnimator.IsMotionPlaying() &&
		!m_player->isDodging() &&
		!m_player->isKnockedBack();
	if (!m_footLockEnabled || !locomotion ||
		m_leftLegChain.toe.empty() || m_rightLegChain.toe.empty())
	{
		m_footLockLeft.Reset();
		m_footLockRight.Reset();
		return;
	}

	const Matrix4x4 playerWorld = m_player->getRenderSRT().GetMatrix();
	const Matrix4x4 worldToModel = playerWorld.Invert();
	bool solvedAny = false;
	const auto solveFoot = [&](const CAnimationMesh::LegIKChain& chain, FootLock::FootState& state)
	{
		Vector3 toeModel{};
		if (!m_playerAnimationMesh->GetBoneModelPosition(chain.toe, toeModel))
			return;
		const Vector3 toeWorld = Vector3::Transform(toeModel, playerWorld);
		const Vector3 targetWorld =
			FootLock::Update(state, toeWorld, deltaSeconds, m_footLockSettings);
		if ((targetWorld - toeWorld).LengthSquared() < 0.000001f)
			return;
		const Vector3 targetModel = Vector3::Transform(targetWorld, worldToModel);
		if (m_playerAnimationMesh->SolveLegIK(chain, targetModel))
			solvedAny = true;
	};
	solveFoot(m_leftLegChain, m_footLockLeft);
	solveFoot(m_rightLegChain, m_footLockRight);
	if (solvedAny)
		m_playerAnimationMesh->RefreshBoneMatrices(m_playerBoneComb);
}

void GameScene::UpdateEnemyAnimation(float deltaSeconds)
{
	if (!m_enemyAnimationMesh || m_enemies.empty())
		return;

	const enemy::MotionState state = m_enemies.front()->getMotionState();
	// 攻撃は予兆・判定・隙をまたいで1本のクリップとして再生する(下のclipDrivenAttack)。
	// 以前は判定までで再生を打ち切り、隙の間は最後の姿勢を0.4秒だけ保持してから待機へ戻していた。
	const bool clipDrivenAttack = m_enemyAttackAnimation != nullptr &&
		(state == enemy::MotionState::Windup ||
		 state == enemy::MotionState::Active ||
		 state == enemy::MotionState::Recovery);
	constexpr float recoveryPoseHoldSeconds = 0.4f;
	const bool holdRecoveryPose = !clipDrivenAttack &&
		state == enemy::MotionState::Recovery &&
		m_enemies.front()->getStateTime() < recoveryPoseHoldSeconds;
	int recoveryPoseFrame = 0;
	if (m_enemyAttackAnimation != nullptr)
	{
		unsigned int attackKeyCount = 0;
		for (unsigned int channelIndex = 0;
			channelIndex < m_enemyAttackAnimation->mNumChannels;
			++channelIndex)
		{
			const aiNodeAnim* channel = m_enemyAttackAnimation->mChannels[channelIndex];
			if (channel == nullptr)
				continue;
			attackKeyCount = std::max(attackKeyCount, channel->mNumRotationKeys);
			attackKeyCount = std::max(attackKeyCount, channel->mNumPositionKeys);
		}
		if (attackKeyCount > 0)
			recoveryPoseFrame = static_cast<int>(attackKeyCount - 1);
	}

	if (state != m_previousEnemyMotionState)
	{
		const bool wasAttackMotion =
			m_previousEnemyMotionState == enemy::MotionState::Windup ||
			m_previousEnemyMotionState == enemy::MotionState::Active;
		const bool isAttackMotion =
			state == enemy::MotionState::Windup ||
			state == enemy::MotionState::Active;
		if (!(wasAttackMotion && isAttackMotion))
		{
			m_enemyAnimationFrame = 0;
			m_enemyAnimationTick = 0.0f;
		}
		// 攻撃へ入る瞬間の姿勢を控えておき、攻撃クリップの先頭へ短くつなぐ。
		// 噛みつきはクリップの途中(頭を上げかけた姿勢)から再生するため、つながないと姿勢が飛ぶ。
		if (state == enemy::MotionState::Windup && m_enemyAttackAnimation != nullptr)
		{
			m_enemyAttackBlendFromAnimation =
				m_previousEnemyMotionState == enemy::MotionState::Approach ||
				m_previousEnemyMotionState == enemy::MotionState::Circle ||
				m_previousEnemyMotionState == enemy::MotionState::Retreat
					? m_enemyWalkAnimation
					: m_enemyIdleAnimation;
			m_enemyAttackBlendFromFrame = m_enemyLastSampledFrame;
			m_enemyAttackBlendFromFraction = m_enemyLastSampledFraction;
			m_enemyAttackBlendTime = 0.0f;
			m_enemyAttackBlendActive = m_enemyAttackBlendFromAnimation != nullptr;
		}
		m_previousEnemyMotionState = state;
	}
	// ドラゴンの歩きモーションは約1.67秒に9キーある。
	// 約10回の固定更新ごとに1キー進め、足が地面を滑らないようにする。
	// 弱って移動が遅くなったら、歩きのアニメーションも同じ割合で遅くする。
	// 足の運びと移動量が合わないと、足が地面を滑って見える。
	const int framesPerKey = std::clamp(
		static_cast<int>(std::lround(10.0f / std::max(0.1f, m_enemies.front()->getMoveSpeedScale()))),
		10, 24);
	if (clipDrivenAttack)
	{
		// 攻撃クリップの再生位置を「攻撃を始めてからの時間」から直接求める。
		// 敵の攻撃モーションは1本しか無いので、種類ごとに再生速度と使う区間を変えて
		// 「速い噛みつき」「遅い薙ぎ払い」を作る(Combat::EnemyAttackClipOf)。
		// クリップを再生し終えた後の隙は、最後の姿勢のまま待つ。
		const Combat::Tuning::EnemyAttackClip& clip =
			Combat::EnemyAttackClipOf(m_enemies.front()->getAttackKind());
		const double ticksPerSecond = m_enemyAttackAnimation->mTicksPerSecond > 0.0
			? m_enemyAttackAnimation->mTicksPerSecond
			: 30.0;
		const float clipSeconds =
			static_cast<float>(m_enemyAttackAnimation->mDuration / ticksPerSecond);
		const int keyCount = recoveryPoseFrame + 1;
		if (clipSeconds > 0.0f && keyCount > 1)
		{
			const float keysPerSecond = static_cast<float>(keyCount - 1) / clipSeconds;
			const float clipTime = std::min(
				clip.clipStart +
					m_enemies.front()->getAttackElapsedSeconds() * clip.playbackRate,
				clip.clipEnd);
			const float keyPosition = std::clamp(
				clipTime * keysPerSecond, 0.0f, static_cast<float>(keyCount - 1));
			m_enemyAnimationFrame = static_cast<int>(keyPosition);
			m_enemyAnimationTick = keyPosition - std::floor(keyPosition);
		}
	}
	if (state == enemy::MotionState::Recovery && !holdRecoveryPose && !clipDrivenAttack &&
		m_enemyAnimationFrame == recoveryPoseFrame + 1)
	{
		m_enemyAnimationFrame = 0;
		m_enemyAnimationTick = 0.0f;
	}

	aiAnimation* animation = m_enemyIdleAnimation;
	if (!m_enemyIntroActive)
	{
		switch (state)
		{
		case enemy::MotionState::Approach:
		case enemy::MotionState::Circle:
		case enemy::MotionState::Retreat:
			animation = m_enemyWalkAnimation;
			break;
		case enemy::MotionState::Windup:
		case enemy::MotionState::Active:
			animation = m_enemyAttackAnimation;
			break;
		case enemy::MotionState::Recovery:
			// 隙の間も攻撃クリップの続き(頭を戻す動き)をそのまま見せる。
			if (clipDrivenAttack)
			{
				animation = m_enemyAttackAnimation;
			}
			else if (holdRecoveryPose && m_enemyAttackAnimation != nullptr)
			{
				animation = m_enemyAttackAnimation;
				m_enemyAnimationFrame = recoveryPoseFrame;
			}
			break;
		}
	}

	bool blendedIntroThisUpdate = false;
	if (animation != nullptr)
	{
		m_enemyAnimationMesh->SetCurentAnimation(animation);
		// キー間の小数部。経過時間で進めた値をそのまま使う。
		const float fraction = holdRecoveryPose ? 0.0f :
			m_enemyAnimationTick;
		const bool blendIntroToBattle =
			m_enemyIntroBattleBlendActive &&
			!m_enemyIntroActive &&
			m_enemyIdleAnimation != nullptr &&
			m_enemyWalkAnimation != nullptr;
		if (blendIntroToBattle)
		{
			blendedIntroThisUpdate = true;
			const float blendRate = std::clamp(
				m_enemyIntroBattleBlendTime / ENEMY_INTRO_BATTLE_BLEND_SECONDS,
				0.0f,
				1.0f);
			m_enemyAnimationMesh->UpdateBlendedRotationAnimation(
				m_enemyBoneComb,
				m_enemyIdleAnimation,
				m_enemyIntroBlendFromFrame,
				m_enemyIntroBlendFromFraction,
				true,
				m_enemyWalkAnimation,
				0,
				0.0f,
				true,
				blendRate);
			m_enemyIntroBattleBlendTime += std::max(deltaSeconds, 0.0f);
			if (m_enemyIntroBattleBlendTime >= ENEMY_INTRO_BATTLE_BLEND_SECONDS)
				m_enemyIntroBattleBlendActive = false;
		}
		else if (m_enemyAttackBlendActive && clipDrivenAttack)
		{
			// 攻撃の入り。直前の姿勢から攻撃クリップの再生位置へ短くつなぐ。
			const float blendRate = std::clamp(
				m_enemyAttackBlendTime / ENEMY_ATTACK_BLEND_SECONDS, 0.0f, 1.0f);
			m_enemyLastSampledFrame = m_enemyAnimationFrame;
			m_enemyLastSampledFraction = fraction;
			m_enemyAnimationMesh->UpdateBlendedRotationAnimation(
				m_enemyBoneComb,
				m_enemyAttackBlendFromAnimation,
				m_enemyAttackBlendFromFrame,
				m_enemyAttackBlendFromFraction,
				true,
				animation,
				m_enemyAnimationFrame,
				fraction,
				false,
				blendRate);
			m_enemyAttackBlendTime += std::max(deltaSeconds, 0.0f);
			if (m_enemyAttackBlendTime >= ENEMY_ATTACK_BLEND_SECONDS)
				m_enemyAttackBlendActive = false;
		}
		else
		{
			m_enemyLastSampledFrame = m_enemyAnimationFrame;
			m_enemyLastSampledFraction = fraction;
			m_enemyAnimationMesh->Update(m_enemyBoneComb, m_enemyAnimationFrame,
				fraction, !holdRecoveryPose);
		}
	}
	else
	{
		m_enemyAnimationMesh->UpdateManualPose(m_enemyBoneComb, {});
	}
	// 敵の静的AABBだけでは、スキニングで脚や胴が沈んだ姿勢のときに
	// 実頂点の最下点を拾えない。描画専用の接地オフセットだけを現在姿勢に
	// 合わせ、物理で使う敵のワールドSRTは変更しない。
	if (!m_enemies.empty() &&
		(m_localEnemyMeshBounds.min.z != 0.0f ||
			m_localEnemyMeshBounds.max.z != 0.0f))
	{
		constexpr float enemyGroundY = -0.3f;
		constexpr float groundingSafetyMargin = 0.01f;
		// 攻撃の演出で本体を前後へ傾けているため、最下点は「ローカルZの最大値」では
		// なくなる。傾きを織り込まずに接地させると、傾けた分だけ敵が地面へ埋まる。
		const Combat::EnemyPoseOffset enemyPose =
			m_enemies.front()->getAttackPoseOffset();
		const float lowestHeight =
			m_enemyAnimationMesh->GetAnimatedLowestLocalHeight(enemyPose.pitch) -
			groundingSafetyMargin;
		const float visualGroundOffsetY =
			enemyGroundY - lowestHeight * ENEMY_MODEL_SCALE;
		for (const auto& enemyObject : m_enemies)
		{
			if (enemyObject)
				enemyObject->setVisualGroundOffsetY(visualGroundOffsetY);
		}
	}
	m_enemyBoneComb.Update();
	if (blendedIntroThisUpdate)
	{
		// ブレンド中はWalkの先頭姿勢を維持し、完了後に通常のキー送りへ戻す。
		m_enemyAnimationFrame = 0;
		m_enemyAnimationTick = 0.0f;
	}
	else if (holdRecoveryPose)
	{
		m_enemyAnimationFrame = recoveryPoseFrame + 1;
		m_enemyAnimationTick = 0.0f;
	}
	else
	{
		// 攻撃中は再生位置をクリップの時間から直接決めているので、ここでは進めない。
		if (clipDrivenAttack)
			return;
		// 経過時間でキーを進める。
		// 以前は「更新1回ごとに1カウント、framesPerKey回で1キー」だったため、
		// fpsが違う機種(30fpsのSwitchなど)では敵のアニメーションの速さが変わっていた。
		// framesPerKeyは60Hzで調整してきた値なので、「60Hzでの更新回数」として秒へ直して使う。
		m_enemyAnimationTick += deltaSeconds * 60.0f / static_cast<float>(framesPerKey);
		while (m_enemyAnimationTick >= 1.0f)
		{
			m_enemyAnimationTick -= 1.0f;
			++m_enemyAnimationFrame;
		}
	}
}

void GameScene::draw(uint64_t deltatime)
{
	// 影の生成は本描画より先に行う。
	// ライトから見た深度を作ってから、通常のカメラで描き直す2パス構成。
	// 影パス用のシェーダーはライトのビュー射影行列を専用の定数バッファ(b7)から
	// 直接読むため、カメラのビュー・射影行列は触らずに済んでいる。
	RenderShadowMap();

	m_camera.Draw();

	// 3軸カラー
	Color axiscol[3] = {
		Color(1, 0, 0, 1), 
		Color(0, 1, 0, 1),
		Color(0, 1, 1, 1)
	};

	if (m_drawLegacyPhysicsDebug)
	{
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
	}

	m_field->draw(deltatime);

	// 闘技場に石の床と外周ブロックを描画する。
	// 実際の衝突平面は後で描画し、ここでは見た目だけの外殻として
	// m_wallsと同じ変換を使う。
	m_arenaFloorVisual.SetSize(ARENA_RADIUS * 2.0f, 0.6f, ARENA_RADIUS * 2.0f);
	SRT floorSrt{};
	floorSrt.pos = Vector3(0.0f, -0.59f, 0.0f);
	m_arenaFloorVisual.Draw(floorSrt, Color(0.24f, 0.20f, 0.16f, 1.0f));

	Renderer::DisableCulling(false);
	m_arenaWallVisual.SetSize(ARENA_WALL_WIDTH, ARENA_WALL_HEIGHT, ARENA_WALL_DEPTH);
	m_arenaWallCapVisual.SetSize(ARENA_WALL_WIDTH + 6.0f, 8.0f, ARENA_WALL_DEPTH + 8.0f);
	for (const auto& arenaWall : m_walls)
	{
		SRT wallSrt = arenaWall->getSRT();
		// 壁は全周を同じ色で見せるため、照明による面ごとの明暗を付けない。
		m_arenaWallVisual.DrawUnlit(wallSrt, ARENA_WALL_COLOR);
		wallSrt.pos.y = ARENA_WALL_HEIGHT + 4.0f;
		m_arenaWallCapVisual.DrawUnlit(wallSrt, ARENA_WALL_COLOR);
	}
	Renderer::DisableCulling(true);

	// モチEΝを描画
	{
	// プレイヤモチEΝの姿勢惁Eｱを取征E
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
		SRT srt = e->getRenderSRT();
		Matrix4x4 worldmtx{};
		worldmtx = srt.GetMatrix();
		Renderer::SetWorldMatrix(&worldmtx);

		ShaderManager::Get<CShader>("Shader3DSkin")->SetGPU();
		m_enemyBoneComb.SetGPU();

		// 被弾フラッシュ。短時間だけ発光させ、命中した瞬間を分かりやすくする。
		if (m_enemyHitFlashTime > 0.0f)
		{
			const float intensity = std::clamp(m_enemyHitFlashTime / 0.12f, 0.0f, 1.0f);
			// 強くしすぎると敵全体が金色に塗り潰され、姿勢も予兆も読めなくなる。
			// 「一瞬光った」と分かる程度に留める。
			Renderer::SetCharacterTint(
				Vector4(1.0f, 0.85f, 0.55f, intensity * 0.16f));
		}
		m_enemyAnimationMesh->Draw();
		// 他の描画へ影響しないよう必ず戻す。
		Renderer::SetCharacterTint(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	// 剣の軌跡は半透明なので、キャラクターを描いた後に重ねる。
	m_playerWeaponTrail.Draw(Color(1.0f, 0.94f, 0.72f, 0.9f));
	// 命中の火花と閃光。カメラへ正対させるためビュー行列を渡す。
	// 色は火花側が寿命に応じて白熱→橙→赤と変えるので、ここでは指定しない。
	m_hitEffect.Draw(m_camera.GetViewMatrix());
	if (m_drawAttackCollisionDebug && !m_enemyIntroActive)
	{
		const auto& collision = m_combat.GetCollisionDebugState();
		const bool showSwordSweep = collision.swordTransformValid &&
			m_combat.IsPlayerAttackActive();
		if (m_drawAttackCollisionXray)
			Renderer::SetDepthEnable(false);

		if (!m_enemies.empty() && m_drawEnemyAabb)
		{
			DrawBoxEdges(GetAabbCorners(collision.enemyBroadPhase),
				collision.broadPhaseOverlap
					? Color(1.0f, 0.85f, 0.05f, 1.0f)
					: Color(0.10f, 0.55f, 1.0f, 1.0f));
		}
		if (m_drawPlayerObb || m_drawEnemyObb)
		{
			if (m_drawPlayerObb)
			{
				DrawBoxEdges(GetObbCorners(collision.playerObb),
					Color(0.15f, 0.75f, 1.0f, 1.0f));
			}
			if (!m_enemies.empty() && m_drawEnemyObb)
			{
				DrawBoxEdges(GetObbCorners(collision.enemyObb),
					collision.narrowPhaseHit
						? Color(1.0f, 0.05f, 0.02f, 1.0f)
						: Color(0.15f, 1.0f, 0.30f, 1.0f));
			}
		}

		if (collision.swordTransformValid)
		{
			if (m_drawAttackAabb)
			{
				DrawBoxEdges(GetAabbCorners(collision.attackBroadPhase),
					collision.broadPhaseOverlap
						? Color(1.0f, 0.85f, 0.05f, 1.0f)
						: Color(0.10f, 0.90f, 1.0f, 1.0f));
			}
			if (m_drawSwordObb)
			{
				DrawBoxEdges(GetObbCorners(collision.bladeObb),
					collision.narrowPhaseHit
						? Color(1.0f, 0.05f, 0.02f, 1.0f)
						: Color(1.0f, 0.35f, 0.02f, 1.0f));
				if (showSwordSweep)
				{
					DrawBoxEdges(GetObbCorners(collision.tipSweepObb),
						collision.narrowPhaseHit
							? Color(1.0f, 0.05f, 0.02f, 1.0f)
							: Color(0.85f, 0.15f, 1.0f, 1.0f));
				}
			}
		}

		if (m_drawAttackCollisionXray)
			Renderer::SetDepthEnable(true);
    }
    std::vector<wall*> hitWallObjects = m_hitWallObjects;

	SRT playersrt = m_player->getSRT();
	m_worldbsplayer = transformBSphere(m_localbsplayer, playersrt);
	if (m_drawLegacyPhysicsDebug)
		SphereDrawerDraw(m_worldbsplayer.radius, Color(1, 1, 1, 0.5f),
			m_worldbsplayer.center.x,
			m_worldbsplayer.center.y,
			m_worldbsplayer.center.z);
	collectHitWalls(hitWallObjects, m_walls, m_worldbsplayer.radius, m_worldbsplayer.center);

	for (auto& e : m_enemies) {
		SRT enemyCollisionSrt = e->getSRT();
		enemyCollisionSrt.scale = Vector3::Min(
			enemyCollisionSrt.scale, Vector3(8.0f, 8.0f, 8.0f));
		GM31::GE::Collision::BoundingSphere enemySphere =
			transformBSphere(m_localbsenemy, enemyCollisionSrt);
		if (m_drawLegacyPhysicsDebug)
			SphereDrawerDraw(enemySphere.radius, Color(1, 0, 0, 0.25f),
				enemySphere.center.x,
				enemySphere.center.y,
				enemySphere.center.z);
		collectHitWalls(hitWallObjects, m_walls, enemySphere.radius, enemySphere.center);
	}

	// 物理壁の半透明表示はデバッグ時だけ描画し、通常画面の色に混ぜない。
	if (m_drawLegacyPhysicsDebug)
	{
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
	DrawGameplayHud();
	DrawEnemyIntroOverlay();
}

void GameScene::DrawGameplayHud()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	const float left = viewport->Pos.x + 28.0f;
	const float top = viewport->Pos.y + 28.0f;
	const float width = 300.0f;
	const float height = 18.0f;
	const auto drawBar = [drawList, left, width, height](float y, float ratio, ImU32 fill)
	{
		drawList->AddRectFilled(ImVec2(left, y), ImVec2(left + width, y + height), IM_COL32(10, 12, 18, 230), 3.0f);
		drawList->AddRectFilled(ImVec2(left + 2.0f, y + 2.0f), ImVec2(left + 2.0f + (width - 4.0f) * std::clamp(ratio, 0.0f, 1.0f), y + height - 2.0f), fill, 3.0f);
		drawList->AddRect(ImVec2(left, y), ImVec2(left + width, y + height), IM_COL32(235, 240, 250, 230), 3.0f, 0, 1.5f);
	};
	// 被弾時に画面の縁を赤くする。中央を覆わないので敵の予兆は隠れない。
	if (m_playerDamageFlashTime > 0.0f)
	{
		const float intensity = std::clamp(m_playerDamageFlashTime / 0.45f, 0.0f, 1.0f);
		const float bandWidth = viewport->WorkSize.x * 0.16f;
		const float bandHeight = viewport->WorkSize.y * 0.16f;
		const ImU32 edge = IM_COL32(190, 30, 30, static_cast<int>(150.0f * intensity));
		const ImU32 clear = IM_COL32(190, 30, 30, 0);
		const ImVec2 min = viewport->Pos;
		const ImVec2 max(min.x + viewport->WorkSize.x, min.y + viewport->WorkSize.y);
		drawList->AddRectFilledMultiColor(
			min, ImVec2(min.x + bandWidth, max.y), edge, clear, clear, edge);
		drawList->AddRectFilledMultiColor(
			ImVec2(max.x - bandWidth, min.y), max, clear, edge, edge, clear);
		drawList->AddRectFilledMultiColor(
			min, ImVec2(max.x, min.y + bandHeight), edge, edge, clear, clear);
		drawList->AddRectFilledMultiColor(
			ImVec2(min.x, max.y - bandHeight), max, clear, clear, edge, edge);
	}

	drawBar(top, m_combat.GetPlayerHp() / m_combat.GetPlayerMaxHp(), IM_COL32(75, 205, 75, 255));
	drawBar(top + 24.0f, m_playerStamina / PLAYER_MAX_STAMINA, IM_COL32(235, 195, 55, 255));

	if (!m_lockOnTarget || m_enemyIntroActive || m_enemies.empty())
		return;

	const enemy::MotionState targetState = m_enemies.front()->getMotionState();
	const bool targetIsWindup = targetState == enemy::MotionState::Windup;
	const bool targetIsRecovery = targetState == enemy::MotionState::Recovery;
	const bool targetHasCombatCue = targetIsWindup || targetIsRecovery;
	const float panelWidth = 236.0f;
	const float panelHeight = targetHasCombatCue ? 84.0f : 62.0f;
	const ImVec2 panelMin(
		viewport->Pos.x + viewport->WorkSize.x - panelWidth - 28.0f,
		viewport->Pos.y + 28.0f);
	const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);
	const ImU32 lockColor = targetIsRecovery
		? IM_COL32(65, 235, 115, 255)
		: targetIsWindup
		? IM_COL32(255, 105, 65, 255)
		: IM_COL32(244, 194, 72, 255);
	drawList->AddRectFilled(panelMin, panelMax, IM_COL32(9, 12, 18, 225), 4.0f);
	drawList->AddRect(panelMin, panelMax, lockColor, 4.0f, 0, 2.0f);
	drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 8.0f),
		lockColor, "TARGET LOCK");
	drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 31.0f),
		IM_COL32(245, 245, 245, 255), "ANCIENT DRAGON");
	if (targetHasCombatCue)
	{
		// 予兆中は「何が来るか」を文字でも示す。攻撃の種類ごとに予兆の長さ・射程・隙が違うため、
		// プレイヤーが回避するか踏み込むかを判断する手がかりになる。
		const char* cueText = targetIsRecovery
			? "CHANCE"
			: Combat::EnemyAttackDisplayName(m_enemies.front()->getAttackKind());
		drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 53.0f),
			lockColor, cueText);
	}
	drawList->AddText(ImVec2(panelMax.x - 62.0f, panelMin.y + 9.0f),
		IM_COL32(190, 198, 210, 255), "TAB");

	// 照準は敵の胸の高さに出す。以前は境界ボックスのY寸法(ドラゴンでは体の長さ)を高さとして使っていて、
	// 照準がドラゴンの頭上の空中に出ていた。
	float targetBottomY = 0.0f;
	float targetHeight = 40.0f;
	GetEnemyVisualVerticalExtent(targetBottomY, targetHeight);
	Vector3 markerWorldPosition = m_enemies.front()->getSRT().pos;
	markerWorldPosition.y = targetBottomY + targetHeight * 0.6f;
	ImVec2 marker{};
	if (!ProjectWorldToScreen(
		m_camera.GetViewMatrix(), m_camera.GetProjMatrix(),
		markerWorldPosition, marker))
		return;

	const float markerRadius = 28.0f;
	const float corner = 9.0f;
	const ImU32 shadow = IM_COL32(7, 8, 10, 230);
	drawList->AddCircle(marker, markerRadius, shadow, 32, 6.0f);
	drawList->AddCircle(marker, markerRadius, lockColor, 32, 2.0f);
	drawList->AddLine(ImVec2(marker.x - markerRadius - corner, marker.y - markerRadius),
		ImVec2(marker.x - markerRadius, marker.y - markerRadius), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x - markerRadius - corner, marker.y - markerRadius),
		ImVec2(marker.x - markerRadius - corner, marker.y - markerRadius + corner), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x + markerRadius + corner, marker.y - markerRadius),
		ImVec2(marker.x + markerRadius, marker.y - markerRadius), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x + markerRadius + corner, marker.y - markerRadius),
		ImVec2(marker.x + markerRadius + corner, marker.y - markerRadius + corner), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x - markerRadius - corner, marker.y + markerRadius),
		ImVec2(marker.x - markerRadius, marker.y + markerRadius), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x - markerRadius - corner, marker.y + markerRadius),
		ImVec2(marker.x - markerRadius - corner, marker.y + markerRadius - corner), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x + markerRadius + corner, marker.y + markerRadius),
		ImVec2(marker.x + markerRadius, marker.y + markerRadius), shadow, 6.0f);
	drawList->AddLine(ImVec2(marker.x + markerRadius + corner, marker.y + markerRadius),
		ImVec2(marker.x + markerRadius + corner, marker.y + markerRadius - corner), shadow, 6.0f);
	drawList->AddCircleFilled(marker, 4.0f, lockColor);
}

void GameScene::DrawEnemyIntroOverlay()
{
	if (!m_enemyIntroActive)
		return;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	const ImVec2 min = viewport->WorkPos;
	const ImVec2 max(min.x + viewport->WorkSize.x, min.y + viewport->WorkSize.y);
	const float height = viewport->WorkSize.y;

	constexpr float battleStartTime = 4.55f;
	const float fadeOut = std::clamp((battleStartTime - m_enemyIntroTime) / 0.55f, 0.0f, 1.0f);
	const int barAlpha = static_cast<int>(std::clamp(170.0f * fadeOut, 0.0f, 170.0f));
	drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, min.y + height * 0.12f),
		IM_COL32(4, 6, 10, barAlpha));
	drawList->AddRectFilled(ImVec2(min.x, max.y - height * 0.12f), ImVec2(max.x, max.y),
		IM_COL32(4, 6, 10, barAlpha));
}

void GameScene::init()
{
	// ゲームプレイ開始時は、提出用映像にデバッグ表示が映らないようにする。
	DebugUI::SetVisible(false);
	DebugUI::SetCursorVisible(false);
	m_resultRequested = false;
	SoundManager::StopBgm();
	// Sword and Shield Pack のモデルとモーションは同じ Mixamo リグを
	// 使用するため、別キャラクターへのリターゲットを行わない。
	const CharacterModelProfile& playerProfile = GetPlayerModelProfile();
	g_loadmodel[0].filename = playerProfile.meshPath;
	g_loadmodel[0].texdirectoryname = playerProfile.textureDirectory;
	g_loadmodel[1].meshid = "cethiel_dragon";
	g_loadmodel[1].filename = "assets/model/CethielDragon/dragon.dae";
	g_loadmodel[1].texdirectoryname = "assets/model/CethielDragon/";
	m_combat.Reset();
	m_playerStamina = PLAYER_MAX_STAMINA;
	ApplyDebugEnemyStartHp();
	// カメラ(3D)の初期匁E
	// 剣の軌跡。刃のワールド座標だけを渡す作りなので、
	// 武器やキャラクターのモデルが変わっても初期化はこのままでよい。
	m_playerWeaponTrail.Initialize();
	m_hitEffect.Initialize();

	m_camera.Init();
	// 攻撃中の寄せは、この基準距離からの相対で決める。
	// カメラ側の既定値を正としておくことで、片方だけ書き換えたときのズレを防ぐ。
	m_cameraBaseDistance = m_camera.GetLookDistance();
	m_cameraDistanceCurrent = m_cameraBaseDistance;
	m_cameraBaseTargetHeight = m_camera.GetTargetHeight();

	// ローカル軸表示用線蛛Eの初期匁E
	m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
	m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
	m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));

	// シェーダを生戁E
	std::unique_ptr<CShader> shader{};
	shader = std::make_unique<CShader>();
	shader->Create(
		"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
		"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー
	);
	ShaderManager::Register<CShader>("Shader3D", std::move(shader));

	std::unique_ptr<CShader> skinShader = std::make_unique<CShader>();
	// LBS/DQSハイブリッドスキニング。純粋なLBSは捻りで体積が潰れるため、
	// 攻撃モーションで腕・胴を大きく回すと破綻していた。
	// 純LBS版と比較したい場合は vertexLightingOneSkinVSSafe.hlsl に戻せばよい。
	// 技術的な背景は docs/スキニング技術解説.md を参照。
	skinShader->Create(
		"shader/vertexLightingOneSkinVSHybrid.hlsl",
		"shader/vertexLightingSkinPS.hlsl"
	);
	ShaderManager::Register<CShader>("Shader3DSkin", std::move(skinShader));

	// 影生成パス用の深度専用シェーダー。
	// 本描画とは別に、ライトから見た深度だけを書き込むために使う。
	m_shadowDepthShader.Create(
		"shader/shadowDepthVS.hlsl",
		"shader/shadowDepthPS.hlsl");
	m_shadowDepthSkinShader.Create(
		"shader/shadowDepthSkinVS.hlsl",
		"shader/shadowDepthPS.hlsl");

	// メチEすュを生戁E
	for (int cnt = 0; cnt < g_loadmodel.size(); cnt++) {
		std::unique_ptr<CStaticMesh> mesh{};
		mesh = std::make_unique<CStaticMesh>();
		mesh->Load(g_loadmodel[cnt].filename, g_loadmodel[cnt].texdirectoryname);

		// メチEすュレンダラを生戁E
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
	m_playerAnimationMesh->ApplyModelProfile(playerProfile);
	m_playerAnimator.Initialize(*m_playerAnimationMesh, playerProfile);
	m_playerAnimationData.LoadAnimation(
		"assets/motion/sword and shield walk.fbx", "walk");
	m_playerAnimationData.LoadAnimation(
		"assets/motion/sword and shield run.fbx", "run");
	// 待機モーション。idleクリップは4種類あるので、これもiniで差し替えられるようにする。
	m_playerAnimationData.LoadAnimation(
		GetDevSetting("idle", "assets/motion/sword and shield idle.fbx"), "idle");
	// 敵の攻撃を1種類に固定する(dev_settings.iniの force_enemy_attack=slam|bite|sweep)。
	// 3種類の構えを見比べるとき、通常の選択では狙った攻撃が出るまで待つことになるため。
	// 未指定なら通常どおり敵AIが選ぶ。デバッグ表示のチェックボックスからも切り替えられる。
	{
		const std::string forced = GetDevSetting("force_enemy_attack", "");
		if (forced == "slam" || forced == "bite" || forced == "sweep")
		{
			m_forceEnemyAttack = true;
			m_forcedEnemyAttackIndex = forced == "bite" ? 1 : forced == "sweep" ? 2 : 0;
		}
	}
	// コンボの各段には別々のクリップを割り当てる。
	// 以前は3段とも同じファイルを指定していたため、連続入力しても
	// 同じ振りが3回繰り返されるだけで「繋がっている」感が出なかった。
	// 1段目は従来から使っていて動作が確認できているクリップを維持し、
	// 2段目以降に別の振りを割り当てている。
	// 差し替えたい場合はモーションエディタ(F3)の
	// 「Preview Combo (Weak/Heavy)」で3段続けて再生して確認できる。
	// どのクリップを使うかは dev_settings.ini から差し替えられる。
	// assets/motion には攻撃系だけで9本あり、どれが良いかは実際に見ないと判断できない。
	// ビルドし直さずに比較できるようにしておくことで、選定の反復を速くする。
	// 例:
	//   weak1=assets/motion/sword and shield slash.fbx
	//   heavy2=assets/motion/sword and shield attack.fbx
	// 指定が無ければ既定のクリップを使う。
	const std::array<std::string, 3> weakAttackFiles = {
		// 既定のクリップと振りの位置は Combat::PlayerComboStepOf() が持つ。
		// iniで差し替えると振りの位置の値と合わなくなるので、差し替えたら表の値も見直すこと。
		GetDevSetting("weak1", Combat::PlayerComboStepOf(false, 1).clipFile),
		GetDevSetting("weak2", Combat::PlayerComboStepOf(false, 2).clipFile),
		GetDevSetting("weak3", Combat::PlayerComboStepOf(false, 3).clipFile),
	};
	const std::array<std::string, 3> heavyAttackFiles = {
		GetDevSetting("heavy1", Combat::PlayerComboStepOf(true, 1).clipFile),
		GetDevSetting("heavy2", Combat::PlayerComboStepOf(true, 2).clipFile),
		GetDevSetting("heavy3", Combat::PlayerComboStepOf(true, 3).clipFile),
	};
	for (size_t index = 0; index < weakAttackFiles.size(); ++index)
	{
		m_playerAnimationData.LoadAnimation(
			weakAttackFiles[index], "weak_attack_" + std::to_string(index + 1));
		m_playerAnimationData.LoadAnimation(
			heavyAttackFiles[index], "heavy_attack_" + std::to_string(index + 1));
	}
	m_playerWalkAnimation = m_playerAnimationData.GetAnimation("walk", 0);
	aiAnimation* playerRunAnimation = m_playerAnimationData.GetAnimation("run", 0);
	std::array<aiAnimation*, 3> weakAttackAnimations{};
	std::array<aiAnimation*, 3> heavyAttackAnimations{};
	for (size_t index = 0; index < weakAttackAnimations.size(); ++index)
	{
		const std::string weakName = "weak_attack_" + std::to_string(index + 1);
		const std::string heavyName = "heavy_attack_" + std::to_string(index + 1);
		weakAttackAnimations[index] = m_playerAnimationData.GetAnimation(weakName.c_str(), 0);
		heavyAttackAnimations[index] = m_playerAnimationData.GetAnimation(heavyName.c_str(), 0);
	}
	aiAnimation* playerIdleAnimation = m_playerAnimationData.GetAnimation("idle", 0);
	m_playerAnimator.SetLocomotionAnimations(m_playerWalkAnimation, playerRunAnimation);

	// 移動のブレンドツリー。前後左右 × 歩き/走り の8本を速度で混ぜる。
	// 手持ちのクリップの腰の移動量を調べて方向を割り当てた(2026-09-15):
	//   walk=前 / walk (2)=後ろ / run=前 / run (2)=後ろ
	//   strafe (2)=歩き+X / strafe (3)=走り+X / strafe=歩き-X / strafe (4)=走り-X
	// Mixamoのキャラクターは+Zを向いており、+Xがキャラクターの左になる。
	// 左右が逆に見える場合は dev_settings.ini で差し替えられる(ビルド不要)。
	{
		struct BlendClipFile { const char* key; const char* defaultFile; };
		const BlendClipFile blendClipFiles[8] = {
			{ "walk_forward",  "assets/motion/sword and shield walk.fbx" },
			{ "run_forward",   "assets/motion/sword and shield run.fbx" },
			{ "walk_right",    "assets/motion/sword and shield strafe.fbx" },
			{ "run_right",     "assets/motion/sword and shield strafe (4).fbx" },
			{ "walk_backward", "assets/motion/sword and shield walk (2).fbx" },
			{ "run_backward",  "assets/motion/sword and shield run (2).fbx" },
			{ "walk_left",     "assets/motion/sword and shield strafe (2).fbx" },
			{ "run_left",      "assets/motion/sword and shield strafe (3).fbx" },
		};
		// 並び順は CCharacterAnimator の [方向(前,右,後,左) x 2 + 歩調(歩き,走り)] に合わせる。
		std::array<aiAnimation*, 8> blendClips{};
		for (size_t index = 0; index < blendClips.size(); ++index)
		{
			const std::string name = std::string("blend_") + blendClipFiles[index].key;
			m_playerAnimationData.LoadAnimation(
				GetDevSetting(blendClipFiles[index].key, blendClipFiles[index].defaultFile), name);
			blendClips[index] = m_playerAnimationData.GetAnimation(name.c_str(), 0);
		}
		m_playerAnimator.SetLocomotionBlendSpace(
			*m_playerAnimationMesh,
			playerProfile.modelScale.y,
			blendClips,
			player::VALUE_MOVE_MODEL,
			player::VALUE_MOVE_MODEL * player::RUN_SPEED_MULTIPLIER);
	}
	m_playerAnimator.SetIdleAnimation(playerIdleAnimation);
	m_playerAnimator.SetAttackAnimations(weakAttackAnimations, heavyAttackAnimations);
	// 被弾ののけぞり。手持ちのクリップにある「impact」を使う(新規アセットは追加しない)。
	m_playerAnimationData.LoadAnimation(
		GetDevSetting("impact", "assets/motion/sword and shield impact.fbx"), "impact");
	m_playerAnimator.SetImpactAnimation(m_playerAnimationData.GetAnimation("impact", 0));
	// ダッシュ攻撃(走りながら弱攻撃)。モンハンの片手剣の突進斬りの役。
	m_playerAnimationData.LoadAnimation(
		GetDevSetting("dash_attack", Combat::PlayerDashAttackStep().clipFile), "dash_attack");
	m_playerAnimator.SetDashAttackAnimation(m_playerAnimationData.GetAnimation("dash_attack", 0));
	// 攻撃中の腰の回転の取り込み方(調整用)。0=取り込まない / 1=バインド姿勢からの変化 / 2=クリップのまま
	m_playerAnimator.SetAttackHipsRotationMode(std::stoi(GetDevSetting("attack_hips_rotation", "1")));
	if (playerIdleAnimation == nullptr)
		std::cout << "[Player] idle animation not loaded" << std::endl;
	if (m_playerWalkAnimation == nullptr)
		std::cout << "[Player] walk animation not loaded" << std::endl;
	if (playerRunAnimation == nullptr)
		std::cout << "[Player] run animation not loaded" << std::endl;
	m_playerBoneComb.Create();
	// 最初の描画前に立ち姿を適用する。
	// 空の姿勢のままだと、更新が止まったフレームでモデルがTポーズになるためである。
	m_playerAnimator.Update(
		*m_playerAnimationMesh,
		m_playerBoneComb,
		CharacterAnimationState{ false, false, false, 0.0f });

	m_enemyAnimationMesh = std::make_unique<CAnimationMesh>();
	m_enemyAnimationMesh->Load(
		"assets/model/CethielDragon/dragon.dae",
		"assets/model/CethielDragon/");
	m_enemyBoneComb.Create();
	m_enemyAnimationData.LoadAnimation(
		"assets/model/CethielDragon/dragon_idle.dae", "idle");
	m_enemyAnimationData.LoadAnimation(
		"assets/model/CethielDragon/dragon_walk.dae", "walk");
	m_enemyAnimationData.LoadAnimation(
		"assets/model/CethielDragon/dragon_attack.dae", "attack");
	m_enemyAnimationData.LoadAnimation(
		"assets/model/CethielDragon/dragon_die.dae", "die");
	m_enemyIdleAnimation = m_enemyAnimationData.GetAnimation("idle", 0);
	m_enemyWalkAnimation = m_enemyAnimationData.GetAnimation("walk", 0);
	m_enemyAttackAnimation = m_enemyAnimationData.GetAnimation("attack", 0);
	m_enemyDieAnimation = m_enemyAnimationData.GetAnimation("die", 0);
	if (m_enemyIdleAnimation != nullptr)
	{
		m_enemyAnimationMesh->SetCurentAnimation(m_enemyIdleAnimation);
		m_enemyAnimationMesh->Update(m_enemyBoneComb, m_enemyAnimationFrame);
	}
	else
	{
		m_enemyAnimationMesh->UpdateManualPose(m_enemyBoneComb, {});
	}
	m_enemyBoneComb.Update();

	m_player = std::make_unique<player>(this);
	m_player->init();
	SRT playerModelSrt = m_player->getSRT();
	// Fallen Paladinは小さい単位で作られているため、ゲーム内表示の基準倍率を10にする。
	// 下で同じ頂点とSRTからOBBを作り直し、表示倍率と衝突寸法を一致させる。
	playerModelSrt.scale = playerProfile.modelScale;
	m_player->setSRT(playerModelSrt);

	m_field = std::make_unique<field>(this);
	m_field->init();

	// 壁パネルを重ねて闘技場の外周を作る。
	// 通常のm_wallsへ登録することで、プレイヤー移動、敵AI、回避距離、
	// 衝突デバッグが同じ境界を共有する。
	m_walls.clear();
	m_walls.reserve(ARENA_WALL_SEGMENTS);
	for (int i = 0; i < ARENA_WALL_SEGMENTS; ++i)
	{
		const float angle = (2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(ARENA_WALL_SEGMENTS);
		std::unique_ptr<wall> arenaWall = std::make_unique<wall>(this);
		arenaWall->init();
		SRT wallSrt{};
		wallSrt.pos = Vector3(
			std::sinf(angle) * ARENA_RADIUS,
			ARENA_WALL_HEIGHT * 0.5f,
			std::cosf(angle) * ARENA_RADIUS);
		wallSrt.rot.y = angle;
		arenaWall->setSRT(wallSrt);
		arenaWall->setheight(ARENA_WALL_HEIGHT);
		arenaWall->setwidth(ARENA_WALL_WIDTH);
		arenaWall->calcEqation();
		m_walls.push_back(std::move(arenaWall));
	}

	m_enemies.reserve(INITIAL_ENEMYNUM);
	for (int ecnt = 0; ecnt < INITIAL_ENEMYNUM; ecnt++) { 		Vector3 enemyPos(0, 0, -120); 		float enemyRotY = 0.0f; 		m_enemies.push_back(createEnemyObject(this, m_player.get(), enemyPos, enemyRotY, ENEMY_MODEL_SCALE)); 	}
	// 調整用: dev_settings.ini の no_enemy=1 で敵を置かずに始める(素振りでモーションを確かめるため)。
	// 敵の境界の計算にはモデルが要るので、敵は一度作ってから外す。
	const bool startWithoutEnemy = GetDevSetting("no_enemy", "0") == "1";
	StartEnemyIntro();
	// プレイヤーのローカル境界球とモデル境界を作成する。
	{
	CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(g_loadmodel[0].meshid);
		const std::vector<VERTEX_3D>& vertices = mesh->GetVertices();

		std::vector<Vector3> vs;
		for (auto& v : vertices) {
			vs.push_back(v.Position);
		}

		SRT srt{};
		m_localbsplayer = GM31::GE::Collision::calcBSphere(vs, srt);
		m_localPlayerMeshBounds =
			GM31::GE::Collision::BuildLocalAABBFromVertices(vs);

		// フィールドの高さはy=-0.3である。読み込んだモデルの原点が足元とは
		// 限らないため、実頂点の最小Yから描画専用の持ち上げ量を求める。
		// 物理のSRTはy=0のままにし、描画、剣の接続、見た目のOBBだけが
		// getRenderSRT()を使うことで、足をフィールドへ正確に接地させる。
		const float playerGroundY = playerProfile.groundY;
		const float playerScaleY = m_player->getSRT().scale.y;
		m_playerGroundY = playerGroundY;
		m_playerStandingGroundOffsetY =
			playerGroundY - m_localPlayerMeshBounds.min.y * playerScaleY;
		m_player->setVisualGroundOffsetY(m_playerStandingGroundOffsetY);
	}

	// 敵のローカル境界球とモデル境界を作成する。
	{
		CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(g_loadmodel[1].meshid);
		if (mesh == nullptr || mesh->GetVertices().empty())
		{
			m_localbsenemy = { Vector3(0, 0, 0), 0.0f };
			m_localEnemyMeshBounds = {
				Vector3(0.0f, 0.0f, 0.0f),
				Vector3(0.0f, 0.0f, 0.0f) };
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
		m_localEnemyMeshBounds =
				GM31::GE::Collision::BuildLocalAABBFromVertices(vs);
			const float enemyGroundY = -0.3f;
			// 敵はenemy::getRenderSRT()で描画時にX軸へ+90度回転させている
			// (ドラゴンのモデルが傾いた姿勢で作られているため)。
			// この回転ではローカルの+Zがワールドの-Yへ向くので、
			// 「一番低い点」はローカルAABBのmin.yではなくmax.zで決まる。
			// プレイヤーと同じようにmin.yで計算すると別の軸の寸法を使うことになり、
			// 敵が地面から浮いて見える。
			m_enemies.front()->setVisualGroundOffsetY(
				enemyGroundY + m_localEnemyMeshBounds.max.z * ENEMY_MODEL_SCALE);
		}
	}
	if (startWithoutEnemy)
		m_enemies.clear();
	// デバッグ表示はまとめて1つのウィンドウへ登録する。
	// 個別に登録すると、それぞれが独立したウィンドウとして開いてしまう。
	DebugUI::RedistDebugFunction([this]() {
		DrawDebugWindow();
		});

}

void GameScene::ApplyDebugEnemyStartHp()
{
	const std::string value = GetDevSetting("enemy_hp", "");
	if (value.empty())
		return;
	try
	{
		m_combat.SetEnemyHpForDebug(std::stof(value));
	}
	catch (const std::exception&)
	{
		// 数値でなければ無視して通常の体力で始める。
	}
}

void GameScene::DrawDebugWindow()
{
	// マルチビューポートが有効なので、ImGuiのウィンドウ位置は「画面全体の座標」である。
	// (10,120)のように小さな値を渡すと、ゲームウィンドウの外側に別のOSウィンドウとして
	// 開いてしまう。ゲーム画面の左上を基準に置くこと。
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->WorkPos.x + 10.0f, mainViewport->WorkPos.y + 120.0f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(440.0f, 520.0f), ImGuiCond_FirstUseEver);
	// 位置をimgui.iniへ保存しない。位置は画面全体の座標で保存されるため、
	// 次に起動したときゲームウィンドウが別の場所に開くと、
	// デバッグウィンドウだけが前回の場所(ゲーム画面の外)に取り残される。
	// 起動のたびにゲーム画面の左上を基準に置き直す。起動中に動かすのは自由。
	if (ImGui::Begin("デバッグ", nullptr, ImGuiWindowFlags_NoSavedSettings))
	{
		if (ImGui::BeginTabBar("デバッグタブ"))
		{
			DebugCombat();
			DebugCollision();
			DebugEnemies();
			DebugPlayerSRT();
			DebugCamera();
			DebugAudio();
			DebugWalls();
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	// 3D空間へ重ねるラベルは、当たり判定タブを開いていなくても描く。
	// 見たいのは画面の中のキャラクターであって、タブの中身ではないため。
	DrawCollisionWorldLabels();
}
	
void GameScene::dispose()
{
	m_playerAnimationMesh.reset();
}


// 壁パラメータ調整
void GameScene::DebugWalls()
{
	static int selected_model = 0;

	if (!ImGui::BeginTabItem("壁"))
		return;

	if (ImGui::Button("壁を追加"))
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
		ImGui::Text("壁がありません");
		ImGui::EndTabItem();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_walls.size()) - 1);

	// 1. ドロチE・ダウンのプレビュー名を現在の selected_model から作諱E
	std::string preview_str = std::to_string(selected_model);
	if (preview_str.length() < 3) {
		preview_str.insert(0, 3 - preview_str.length(), '0');
	}
	std::string preview_name = "Wall_" + preview_str;

	// BeginComboを使ってドロチE・ダウンを作諱E
	if (ImGui::BeginCombo("対象の壁", preview_name.c_str()))
	{
		for (int i = 0; i < static_cast<int>(m_walls.size()); ++i)
		{
			const bool is_selected = (selected_model == i);

			// 【修正】リスト縺E頁E岼名縺E selected_model ではなぁEi を使ぁE
			std::string str = std::to_string(i);
			if (str.length() < 3) {
				// 3桁に足りなぁE・だけ、蛛E頭に '0' を挿入する
				str.insert(0, 3 - str.length(), '0');
			}

			std::string item_name = std::string("Wall_") + str;

			// リスト縺E吁EいイチEΒを描画し、クリチEけされたか判宁E
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_model = i;
			}

			// ドロチE・ダウンを開ぁE◆時、現在選択されてぁEｋアイチEΒにフォーカスを合わせめE
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("この壁を削除") && !m_walls.empty())
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
		ImGui::Text("壁がありません");
		ImGui::EndTabItem();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_walls.size()) - 1);

	// 2. 選択されてぁEｋ壁縺Eインスタンスを取征E
	// ※ m_walls は壁を管琁E＠てぁEｋ配蛛EめEstd::vector を想定してぁE∪す。実際の変数名に合わせてください、E
	auto& currentWall = m_walls[selected_model];
	if (!currentWall)
	{
		ImGui::EndTabItem();
		return;
	}

	// 3. 選択中の壁から【現在の値】を取得し、スライダー用の変数にセチEヨ
	SRT srt = currentWall->getSRT();
	float wallheight = currentWall->getheight();	// ※getterが存在すると仮宁E
	float wallwidth = currentWall->getwidth();		// ※getterが存在すると仮宁E
	float wallrotationy = srt.rot.y;
	Vector3 wallposition = srt.pos;

	bool isChanged = false;

	// スライダーが操作されて値が変わった場合、isChanged ぁEtrue になめE
	isChanged |= ImGui::SliderFloat("高さ", &wallheight, 1.0f, 500.0f);
	isChanged |= ImGui::SliderFloat("幅", &wallwidth, 1.0f, 1000.0f);
	isChanged |= ImGui::SliderFloat("Y軸の回転", &wallrotationy, -PI, PI);
	isChanged |= ImGui::SliderFloat3("位置", &wallposition.x, -1000.0f, 1000.0f);

	// 4. パラメータに変更があった場合縺Eみ、E∈択中の壁に変更を反映
	if (isChanged)
	{
		// 【修正】g_wall... ではなく、ImGuiで操作したローカル変数を使用する
		srt.pos = wallposition;
		srt.rot.y = wallrotationy;

		currentWall->setSRT(srt);
		currentWall->setwidth(wallwidth);
		currentWall->setheight(wallheight);

		currentWall->calcEqation();	// 変更があった縺Eで平面の方程式を再計算すめE
	}

	ImGui::EndTabItem();
}

// 敵パラメータ調整
void GameScene::DebugEnemies()
{
	static int selected_model = 0;

	if (!ImGui::BeginTabItem("敵"))
		return;


	if (ImGui::Button("敵を追加"))
	{
		Vector3 enemyPos = m_player->getSRT().pos + Vector3(120.0f, 0, 0);
		m_enemies.push_back(createEnemyObject(this, m_player.get(), enemyPos, 0.0f, ENEMY_MODEL_SCALE));
		selected_model = static_cast<int>(m_enemies.size()) - 1;
	}
	ImGui::SameLine();
	// 素振りでモーションを確かめるための近道。敵がいなくても攻撃・コンボ・ダッシュ攻撃は出せる。
	// 起動時から敵なしにしたい場合は dev_settings.ini に no_enemy=1 を書く。
	if (ImGui::Button("敵を全員消す(素振り)") && !m_enemies.empty())
	{
		m_enemies.clear();
		m_combat.ClearEnemyCollisionDebug();
		m_combat.CancelEnemyAttack();
		m_lockOnTarget = false;
		selected_model = 0;
	}

	if (m_enemies.empty())
	{
		ImGui::Text("敵がいません");
		ImGui::EndTabItem();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_enemies.size()) - 1);

	std::string preview_str = std::to_string(selected_model);
	if (preview_str.length() < 3) {
		preview_str.insert(0, 3 - preview_str.length(), '0');
	}
	std::string preview_name = "Enemy_" + preview_str;

	if (ImGui::BeginCombo("対象の敵", preview_name.c_str()))
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

	if (ImGui::Button("この敵を削除") && !m_enemies.empty())
	{
		m_enemies.erase(m_enemies.begin() + selected_model);
		m_combat.ClearEnemyCollisionDebug();
		m_combat.CancelEnemyAttack();
		if (m_enemies.empty()) {
			selected_model = 0;
		}
		else if (selected_model >= static_cast<int>(m_enemies.size())) {
			selected_model = static_cast<int>(m_enemies.size()) - 1;
		}
	}

	if (m_enemies.empty())
	{
		ImGui::Text("敵がいません");
		ImGui::EndTabItem();
		return;
	}

	selected_model = std::clamp(selected_model, 0, static_cast<int>(m_enemies.size()) - 1);

	auto& currentEnemy = m_enemies[selected_model];
	SRT srt = currentEnemy->getSRT();
	Vector3 enemyposition = srt.pos;
	float enemyrotationy = srt.rot.y;
	float enemyscale = srt.scale.x;

	bool isChanged = false;
	isChanged |= ImGui::SliderFloat3("位置", &enemyposition.x, -1000.0f, 1000.0f);
	isChanged |= ImGui::SliderFloat("Y軸の回転", &enemyrotationy, -PI, PI);
	isChanged |= ImGui::SliderFloat("大きさ", &enemyscale, 0.2f, 3.0f);

	if (isChanged)
	{
		srt.pos = enemyposition;
		srt.rot.y = enemyrotationy;
		srt.scale = Vector3(enemyscale, enemyscale, enemyscale);
		currentEnemy->setSRT(srt);
	}

	ImGui::EndTabItem();
}

void GameScene::DebugPlayerSRT()
{
	if (!ImGui::BeginTabItem("プレイヤー"))
		return;

	ImGui::SeparatorText("足の接地(フットロック)");
	ImGui::Checkbox("接地した足をその場へ固定する", &m_footLockEnabled);
	ImGui::Text("左足: %s (速さ %.0f)  /  右足: %s (速さ %.0f)",
		m_footLockLeft.locked ? "接地" : "離れている", m_footLockLeft.speed,
		m_footLockRight.locked ? "接地" : "離れている", m_footLockRight.speed);
	ImGui::SliderFloat("固定する速さの上限", &m_footLockSettings.lockSpeed, 1.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("解除する速さ", &m_footLockSettings.unlockSpeed, 4.0f, 60.0f, "%.0f");
	ImGui::SliderFloat("解除する距離", &m_footLockSettings.unlockDistance, 4.0f, 60.0f, "%.0f");
	ImGui::SliderFloat("接地とみなす高さ", &m_footLockSettings.groundContactHeight, 0.5f, 12.0f, "%.1f");
	ImGui::Separator();

	SRT playercurrentsrt = m_player->getSRT();
	Vector3 scale = playercurrentsrt.scale;
	Vector3 rot = playercurrentsrt.rot;
	Vector3 position = playercurrentsrt.pos;

	bool isChanged = false;

	// スライダーが操作されて値が変わった場合、isChanged ぁEtrue になめE
	isChanged |= ImGui::SliderFloat3("回転", &rot.x, -PI, PI);
	isChanged |= ImGui::SliderFloat3("大きさ", &scale.x, 0.01f, 10.0f);
	isChanged |= ImGui::SliderFloat3("位置", &position.x, -1000.0f, 1000.0f);
	if (ImGui::Button("位置と姿勢を初期値へ戻す"))
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

	ImGui::EndTabItem();
}

void GameScene::UpdateArenaCameraCollision()
{
	if (!m_player)
		return;

	// 闘技場は正多角形なので、少し内側へ縮めた円で16枚の壁を近似する。
	// プレイヤーは壁まで近づけるが、カメラには少し内側の余白を残す。
	const Vector3 playerPosition = m_player->getSRT().pos;
	const float cameraSafeRadius = ARENA_RADIUS - 20.0f;
	const float yaw = m_camera.GetYaw();
	const float cosPitch = std::cos(m_camera.GetPitch());
	const Vector3 boomDirection(
		std::sinf(yaw),
		0.0f,
		-std::cosf(yaw));
	// 通常時とロックオン中で取りたい距離が違う(ロックオンは最大112)ので、カメラが今取りたい距離を使う。
	const float desiredDistance = m_camera.GetDesiredDistance();
	const float desiredHorizontalDistance = desiredDistance * cosPitch;

	// |playerXZ + boomDirectionXZ * distance| <= safeRadiusを解く。
	// これにより別の衝突システムを追加せず、カメラの軌道と安全円の
	// 交点からカメラの許容距離を求められる。
	const float dot = playerPosition.x * boomDirection.x +
		playerPosition.z * boomDirection.z;
	const float constant = playerPosition.x * playerPosition.x +
		playerPosition.z * playerPosition.z -
		cameraSafeRadius * cameraSafeRadius;
	const float discriminant = dot * dot - constant;
	float allowedHorizontalDistance = desiredHorizontalDistance;
	if (discriminant >= 0.0f)
	{
		const float root = std::sqrt(discriminant);
		const float nearIntersection = -dot - root;
		const float farIntersection = -dot + root;
		if (constant <= 0.0f)
		{
			// プレイヤーが安全円の内側にいる場合は、遠い側の交点だけで
			// プレイヤー後方のカメラ距離を制限する。
			if (farIntersection > 0.0f)
				allowedHorizontalDistance = std::min(allowedHorizontalDistance, farIntersection);
		}
		else if (farIntersection > 0.0f)
		{
			// プレイヤーのカプセルが安全余白へ少し重なる場合は、壁の外へ出さず
			// 近い交点と遠い交点の間にカメラを収める。
			allowedHorizontalDistance = std::clamp(
				allowedHorizontalDistance,
				std::max(nearIntersection, 0.0f),
				farIntersection);
		}
		else
		{
			allowedHorizontalDistance = 0.0f;
		}
	}

	float safeDistance = allowedHorizontalDistance / std::max(cosPitch, 0.001f);

	// 敵の体がカメラとプレイヤーの間へ入る場合も引き寄せる。
	// 大型の敵に密着すると、以前はプレイヤーがドラゴンの体でほぼ隠れ、
	// 予兆を読むどころか自分の位置すら分からなくなっていた。
	safeDistance = std::min(safeDistance, CalcEnemyOccludedCameraDistance(desiredDistance));

	if (safeDistance < desiredDistance - 0.01f)
		m_camera.SetCollisionDistance(std::max(3.0f, safeDistance));
	else
		m_camera.SetCollisionDistance(-1.0f);
}

void GameScene::GetEnemyVisualVerticalExtent(float& bottomY, float& height) const
{
	bottomY = 0.0f;
	height = 40.0f;
	if (m_enemies.empty() || !m_enemies.front())
		return;
	const Matrix4x4 world = m_enemies.front()->getRenderSRT().GetMatrix();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	for (int corner = 0; corner < 8; ++corner)
	{
		const Vector3 local(
			(corner & 1) ? m_localEnemyMeshBounds.max.x : m_localEnemyMeshBounds.min.x,
			(corner & 2) ? m_localEnemyMeshBounds.max.y : m_localEnemyMeshBounds.min.y,
			(corner & 4) ? m_localEnemyMeshBounds.max.z : m_localEnemyMeshBounds.min.z);
		const float y = Vector3::Transform(local, world).y;
		minY = std::min(minY, y);
		maxY = std::max(maxY, y);
	}
	bottomY = minY;
	height = std::max(maxY - minY, 1.0f);
}

float GameScene::CalcEnemyOccludedCameraDistance(float desiredDistance) const
{
	if (m_enemies.empty() || !m_enemies.front())
		return desiredDistance;

	// 実際に使われているカメラ位置と注視点から、今のブームの向きを求める。
	// ロックオン時は注視点がプレイヤーと敵の中間寄りになるため、
	// 理想的なブームを再計算すると実際の構図とずれてしまう。
	const Vector3 lookat = m_camera.GetLookat();
	Vector3 boom = m_camera.GetPosition() - lookat;
	const float boomLength = boom.Length();
	if (boomLength < 0.001f)
		return desiredDistance;
	boom /= boomLength;

	// 敵を球で近似する。実頂点から作った境界球を使うので、
	// モデルを差し替えても大きさが自動で追従する。
	const SRT enemySrt = m_enemies.front()->getRenderSRT();
	float enemyBottomY = 0.0f;
	float enemyHeight = 40.0f;
	GetEnemyVisualVerticalExtent(enemyBottomY, enemyHeight);
	Vector3 enemyCenter = m_enemies.front()->getSRT().pos;
	enemyCenter.y = enemyBottomY + enemyHeight * 0.45f;
	// 境界球そのものだと尻尾や翼まで含んで大きすぎ、少し離れただけで
	// カメラが寄ってしまう。胴体の太さに近い範囲へ絞る。
	const float enemyRadius = std::max(
		(m_localEnemyMeshBounds.max.x - m_localEnemyMeshBounds.min.x) *
		std::abs(enemySrt.scale.x) * 0.40f,
		1.0f);

	// 注視点から後方(カメラ側)へ伸ばした線分と、敵の球との交差を解く。
	const Vector3 toCenter = enemyCenter - lookat;
	const float projection = toCenter.Dot(boom);
	const float centerDistanceSq = toCenter.LengthSquared();
	const float perpendicularSq =
		centerDistanceSq - projection * projection;
	const float radiusSq = enemyRadius * enemyRadius;
	if (perpendicularSq > radiusSq)
		return desiredDistance;	// ブームの軸から離れており、視界を塞がない。

	const float halfChord = std::sqrt(std::max(radiusSq - perpendicularSq, 0.0f));
	const float entry = projection - halfChord;
	if (entry <= 0.0f || entry >= desiredDistance)
	{
		// 注視点が既に敵の内側にある場合や、敵がカメラより後ろにいる場合は
		// 引き寄せても改善しないため、通常の距離のままにする。
		return desiredDistance;
	}

	// 敵の手前へ少し余白を取って収める。
	constexpr float MARGIN = 6.0f;
	return std::max(entry - MARGIN, 12.0f);
}

Matrix4x4 GameScene::BuildLightViewProjection() const
{
	// 平行光源なので、ライトの位置そのものには意味が無く「向き」と
	// 「影を掛ける範囲」だけが効く。闘技場全体を覆う正射影を作る。
	LIGHT light = Renderer::GetLight();
	Vector3 direction(light.Direction.x, light.Direction.y, light.Direction.z);
	if (direction.LengthSquared() < 0.0001f)
		direction = Vector3(0.5f, -1.0f, 0.8f);
	direction.Normalize();

	// 影を掛けたい範囲の中心。戦闘はプレイヤー周辺で起きるので、
	// 闘技場全体ではなくプレイヤーを中心にすることで、
	// 限られた解像度を必要な場所へ集中させ、影の輪郭を細かく保つ。
	Vector3 center(0.0f, 0.0f, 0.0f);
	// 影を掛ける範囲。広げるほど影が粗くなるので、
	// プレイヤーと敵が収まる程度に絞る。
	float orthoSize = 260.0f;
	// 登場カットシーンの間はカメラがドラゴンを映しており、プレイヤーは画面外にいる。
	// プレイヤー中心のままだと、着地地点(z=-120付近)のドラゴンが範囲の縁へ来て
	// 影が切れる。さらに落下中は高い位置にいるため、平行光源の向きの分だけ
	// 影の落ちる位置が本体から大きく離れ、範囲外へ出てしまう。
	// カットシーン中はドラゴンを中心にし、範囲も広げて影を収める。
	if (m_enemyIntroActive && !m_enemies.empty() && m_enemies.front())
	{
		const Vector3 enemyPos = m_enemies.front()->getSRT().pos;
		center = Vector3(enemyPos.x, 0.0f, enemyPos.z);
		orthoSize = 460.0f;
	}
	else if (m_player)
	{
		const Vector3 playerPos = m_player->getSRT().pos;
		center = Vector3(playerPos.x, 0.0f, playerPos.z);
	}

	// 光の来る方向へ十分離れた位置から中心を見る。
	constexpr float LIGHT_DISTANCE = 400.0f;
	const Vector3 eye = center - direction * LIGHT_DISTANCE;

	// 見上げ・見下ろしが真上に近いと上方向ベクトルが縮退するため、
	// 光の向きとほぼ平行でない軸を上方向に選ぶ。
	Vector3 up(0.0f, 1.0f, 0.0f);
	if (std::abs(direction.y) > 0.95f)
		up = Vector3(0.0f, 0.0f, 1.0f);

	const Matrix4x4 view = Matrix4x4::CreateLookAt(eye, center, up);

	const Matrix4x4 projection = Matrix4x4::CreateOrthographic(
		orthoSize, orthoSize, 1.0f, LIGHT_DISTANCE * 2.0f);

	return view * projection;
}

void GameScene::RenderShadowMap()
{
	if (!m_shadowEnabled)
	{
		Renderer::SetLightViewProjection(Matrix4x4::Identity, false);
		return;
	}

	const Matrix4x4 lightViewProjection = BuildLightViewProjection();
	Renderer::SetLightViewProjection(lightViewProjection, true);

	Renderer::BeginShadowPass();

	// 影を落とす対象だけを描く。地面や壁は影の受け手であって落とし手ではないので
	// ここでは描かない(自分自身の影で暗くなるのを避ける)。
	if (m_playerAnimationMesh && m_player)
	{
		const SRT srt = m_player->getRenderSRT();
		Matrix4x4 worldmtx = srt.GetMatrix();
		Renderer::SetWorldMatrix(&worldmtx);
		m_shadowDepthSkinShader.SetGPU();
		m_playerBoneComb.Update();
		m_playerBoneComb.SetGPU();
		m_playerAnimationMesh->Draw();
	}

	if (m_enemyAnimationMesh)
	{
		for (const auto& e : m_enemies)
		{
			if (!e) continue;
			const SRT srt = e->getRenderSRT();
			Matrix4x4 worldmtx = srt.GetMatrix();
			Renderer::SetWorldMatrix(&worldmtx);
			m_shadowDepthSkinShader.SetGPU();
			m_enemyBoneComb.Update();
			m_enemyBoneComb.SetGPU();
			m_enemyAnimationMesh->Draw();
		}
	}

	Renderer::EndShadowPass();
}

void GameScene::UpdateCombatCameraDistance(float deltaSeconds)
{
	// 攻撃中はカメラをわずかに寄せる。
	// 以前は0.62倍まで寄せていたが、攻撃のたびにカメラが前後し、足元や敵の予兆が画面から切れていた
	// (実機キャプチャで足が画面外になることを確認)。フロム作品は攻撃でカメラを寄せないので、
	// 迫力の手掛かりとしてごく少しだけ残す(ユーザー判断 2026-09-15)。
	constexpr float ATTACK_DISTANCE_RATE = 0.90f;   // 通常時に対する攻撃中の距離比
	constexpr float APPROACH_SPEED = 3.0f;          // 寄る速さ
	constexpr float RETURN_SPEED = 2.0f;            // 戻る速さ(戻りは緩やかにする)
	// 注視点(m_targetHeight)は動かさない。
	// 一度は「キャラクターが画面下寄りになるので注視点も下げる」を試したが、
	// 実機で確認すると地面寄りを見る構図になり、近づいた敵に視界を塞がれて
	// かえって見づらくなったため取りやめた。距離だけを変える。
	const bool attacking = m_combat.IsPlayerAttacking();
	const float targetDistance = attacking
		? m_cameraBaseDistance * ATTACK_DISTANCE_RATE
		: m_cameraBaseDistance;
	const float speed = attacking ? APPROACH_SPEED : RETURN_SPEED;
	const float blend = std::clamp(speed * deltaSeconds, 0.0f, 1.0f);
	m_cameraDistanceCurrent += (targetDistance - m_cameraDistanceCurrent) * blend;
	m_camera.SetLookDistance(m_cameraDistanceCurrent);
}

void GameScene::UpdateGameplayCamera(float deltaSeconds)
{
	if (m_player == nullptr || m_enemyIntroActive)
		return;

	if (m_lockOnTarget && !m_enemies.empty())
	{
		float targetBottomY = 0.0f;
		float targetHeight = 40.0f;
		GetEnemyVisualVerticalExtent(targetBottomY, targetHeight);
		Vector3 targetFeet = m_enemies.front()->getSRT().pos;
		targetFeet.y = targetBottomY;
		m_camera.UpdateLockOn(
			m_player->getSRT().pos,
			targetFeet,
			targetHeight,
			deltaSeconds);
	}
	else
	{
		// マウスの移動量(DirectInputの相対値)。カーソルを閉じ込めている間だけ視点に使う。
		// カーソルを出しているときにマウスで視点が回ると、デバッグ表示を触れない。
		CameraLookInput look{};
		look.active = DebugUI::IsCursorLocked();
		if (look.active)
		{
			look.deltaX = static_cast<float>(CDirectInput::GetInstance().GetMouseDeltaX());
			look.deltaY = static_cast<float>(CDirectInput::GetInstance().GetMouseDeltaY());
		}
		m_camera.Update(
			m_player->getSRT().pos,
			m_playerWorldVelocity,
			look,
			deltaSeconds);
	}
}

void GameScene::DebugCamera()
{
	if (!ImGui::BeginTabItem("カメラ"))
		return;

	ImGui::Text("マウス: 視点  /  左Alt: カーソルの表示切り替え");
	ImGui::Text("Tab: ロックオン(相手がいないときはカメラを背後へ回す)");
	ImGui::Text("視点を %s", DebugUI::IsCursorLocked() ? "マウスで操作中" : "操作していない(カーソル表示中)");

	float sensitivity = m_camera.GetMouseSensitivity() * 1000.0f;
	if (ImGui::SliderFloat("マウス感度", &sensitivity, 0.2f, 4.0f, "%.1f"))
		m_camera.SetMouseSensitivity(sensitivity / 1000.0f);
	bool autoFollow = m_camera.IsAutoFollowEnabled();
	if (ImGui::Checkbox("横へ走るとカメラが背後へ回り込む(自動追従)", &autoFollow))
		m_camera.SetAutoFollowEnabled(autoFollow);
	ImGui::Text("向き %.0f度 / 高さ %.0f度 / 距離 %.0f",
		m_camera.GetYaw() * 180.0f / PI,
		m_camera.GetPitch() * 180.0f / PI,
		m_camera.GetDesiredDistance());

	if (ImGui::Button("カメラを初期位置へ戻す"))
	{
		m_camera.Reset(m_player->getSRT().pos, m_player->getSRT().rot.y);
	}
	ImGui::EndTabItem();
}

void GameScene::DebugCombat()
{
	if (!ImGui::BeginTabItem("戦闘"))
		return;

    ImGui::Text("戦闘の状態: %s", m_combat.GetStateName().data());
    ImGui::Text("プレイヤーの動作: %s", m_player->getMotionStateName());
	if (!m_enemies.empty())
	{
		ImGui::Text("敵AIの状態: %s (%.2f秒)",
			m_enemies.front()->getMotionStateName(),
			m_enemies.front()->getStateTime());
		ImGui::Text("敵の攻撃: %s",
			Combat::EnemyAttackDebugName(m_enemies.front()->getAttackKind()));
		ImGui::Text("敵の弱り具合: %s (体力 %.0f%%)",
			m_enemies.front()->getConditionName(),
			100.0f * m_combat.GetEnemyHp() / m_combat.GetEnemyMaxHp());
		// 怯み値の溜まり具合。バーが満ちると怯む。怯むたびにしきい値が上がる。
		ImGui::Text("敵の怯み値 (%.0f / %.0f、怯んだ回数 %d)",
			m_enemies.front()->getPosture(),
			m_enemies.front()->getFlinchThreshold(),
			m_enemies.front()->getFlinchCount());
		ImGui::ProgressBar(
			m_enemies.front()->getPosture() / m_enemies.front()->getFlinchThreshold(),
			ImVec2(-1.0f, 0.0f));
		// 3種類の構えを見比べるための調整用。通常は距離と直前の攻撃で選ばれるため、
		// 狙った攻撃が出るまで待つことになり、予兆の形を詰めにくい。
		ImGui::Checkbox("敵の攻撃を固定する", &m_forceEnemyAttack);
		if (m_forceEnemyAttack)
		{
			ImGui::Indent();
			ImGui::RadioButton("叩き付け", &m_forcedEnemyAttackIndex, 0);
			ImGui::RadioButton("噛みつき", &m_forcedEnemyAttackIndex, 1);
			ImGui::RadioButton("薙ぎ払い", &m_forcedEnemyAttackIndex, 2);
			ImGui::Unindent();
		}
		const Combat::EnemyAttackKind forced =
			m_forcedEnemyAttackIndex == 1 ? Combat::EnemyAttackKind::Bite :
			m_forcedEnemyAttackIndex == 2 ? Combat::EnemyAttackKind::Sweep :
			Combat::EnemyAttackKind::Slam;
		m_enemies.front()->setForcedAttackKind(m_forceEnemyAttack ? &forced : nullptr);
	}
	ImGui::SeparatorText("操作");
	ImGui::Text("WASD: 移動  /  左Shift: ダッシュ  /  Space: 回避");
	ImGui::Text("左クリック: 通常攻撃  /  右クリック: 強攻撃");
	ImGui::Text("マウス: 視点  /  左Alt: カーソルを出す(デバッグ表示を触るとき)");
	ImGui::Text("Tab: %s", m_lockOnTarget ? "ロックオン中" : "ロックオン解除中");
	ImGui::Text("R: 仕切り直し");
	ImGui::SeparatorText("回避の無敵時間 (60フレーム/秒)");
	ImGui::SliderInt("無敵の開始", &m_dodgeInvincibleStartFrame, 0, 24);
	ImGui::SliderInt("無敵の終了", &m_dodgeInvincibleEndFrame, 0, 24);
	if (m_dodgeInvincibleEndFrame < m_dodgeInvincibleStartFrame)
		m_dodgeInvincibleEndFrame = m_dodgeInvincibleStartFrame;
	ImGui::Text("無敵の区間: %d 〜 %d フレーム目",
		m_dodgeInvincibleStartFrame, m_dodgeInvincibleEndFrame);
	ImGui::SeparatorText("攻撃中の移動・回避キャンセル (60フレーム/秒)");
	ImGui::SliderInt("回避キャンセルの終了", &m_attackCancelEndFrame, 0, 60);
	ImGui::Text("攻撃中の移動: 不可  /  回避キャンセル: 0 〜 %d フレーム目",
		m_attackCancelEndFrame);
    ImGui::Separator();
    ImGui::Text("プレイヤーの体力");
    ImGui::ProgressBar(m_combat.GetPlayerHp() / m_combat.GetPlayerMaxHp(), ImVec2(-1.0f, 0.0f));
    // 敵の体力はゲーム画面には出さない(弱り具合は体の動きで伝える)。ここは開発者向け。
    ImGui::Text("敵の体力 (%.0f / %.0f)", m_combat.GetEnemyHp(), m_combat.GetEnemyMaxHp());
    ImGui::ProgressBar(m_combat.GetEnemyHp() / m_combat.GetEnemyMaxHp(), ImVec2(-1.0f, 0.0f));
	if (!m_enemies.empty())
	{
		ImGui::Text("敵の弱り具合: %s", m_enemies.front()->getConditionName());
		// 弱り具合の見た目を確かめるための調整用。体力を直接書き換える。
		// 一度悪くなった弱り具合は戻らないので、戻したいときはRで仕切り直す。
		float enemyHp = m_combat.GetEnemyHp();
		if (ImGui::SliderFloat("敵の体力を設定", &enemyHp, 1.0f, m_combat.GetEnemyMaxHp(), "%.0f"))
			m_combat.SetEnemyHpForDebug(enemyHp);
	}
    ImGui::EndTabItem();
}

void GameScene::DebugCollision()
{
	if (!ImGui::BeginTabItem("当たり判定"))
		return;

    ImGui::Checkbox("当たり判定の形を描く", &m_drawAttackCollisionDebug);
	if (m_drawAttackCollisionDebug)
	{
		ImGui::Indent();
		ImGui::Checkbox("物陰でも透けて見せる", &m_drawAttackCollisionXray);
		ImGui::SeparatorText("表示する判定");
		ImGui::Checkbox("プレイヤーのOBB", &m_drawPlayerObb);
		ImGui::Checkbox("敵のOBB", &m_drawEnemyObb);
		ImGui::Checkbox("敵のAABB(粗い判定)", &m_drawEnemyAabb);
		ImGui::Checkbox("剣のOBB", &m_drawSwordObb);
		ImGui::Checkbox("攻撃のAABB(粗い判定)", &m_drawAttackAabb);
		ImGui::Checkbox("3D上の名前表示", &m_drawAttackCollisionLabels);
		ImGui::Unindent();
	}
	ImGui::Checkbox("旧デバッグ表示(球・座標軸)", &m_drawLegacyPhysicsDebug);
    ImGui::Separator();
	const auto& collision = m_combat.GetCollisionDebugState();
	ImGui::Text("判定の流れ: AABBで粗く絞る → OBBで正確に判定");
    ImGui::Text("攻撃判定: %s", m_combat.IsPlayerAttackActive() ? "出ている" : "出ていない");
	ImGui::Text("剣の姿勢: %s", collision.swordTransformValid ? "取得できている" : "取得できていない");
	ImGui::Text("プレイヤーのOBB: %.1f x %.1f x %.1f",
		collision.playerObb.lengthx,
		collision.playerObb.lengthy,
		collision.playerObb.lengthz);
	ImGui::Text("敵のOBB: %.1f x %.1f x %.1f",
		collision.enemyObb.lengthx,
		collision.enemyObb.lengthy,
		collision.enemyObb.lengthz);
	ImGui::Separator();
	ImGui::TextColored(
		collision.broadPhaseOverlap
			? ImVec4(1.0f, 0.85f, 0.10f, 1.0f)
			: ImVec4(0.25f, 0.80f, 1.0f, 1.0f),
		"1. 粗い判定(AABB): %s",
		collision.broadPhaseOverlap ? "通過" : "除外");
	ImGui::TextColored(
		collision.narrowPhaseHit
			? ImVec4(1.0f, 0.15f, 0.05f, 1.0f)
			: ImVec4(0.75f, 0.45f, 1.0f, 1.0f),
		"2. 正確な判定(OBB): %s",
		collision.narrowPhaseHit
			? "命中"
			: (collision.narrowPhaseTested ? "外れ" : "判定せず"));
	ImGui::Text("与ダメージ: %.1f", 25.0f);
    ImGui::Separator();
	ImGui::SeparatorText("画面に描く色の意味");
	ImGui::TextColored(ImVec4(0.15f, 0.85f, 1.0f, 1.0f), "水色・青: AABB(粗い判定)");
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.10f, 1.0f), "黄: AABBが重なっている");
	ImGui::TextColored(ImVec4(0.20f, 0.75f, 1.0f, 1.0f), "明るい青: プレイヤーのOBB");
	ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.35f, 1.0f), "緑: 敵のOBB");
	ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.05f, 1.0f), "橙・紫: 剣のOBB");
	ImGui::TextColored(ImVec4(1.0f, 0.08f, 0.04f, 1.0f), "赤: 正確な判定で命中");
    ImGui::EndTabItem();
}

void GameScene::DrawCollisionWorldLabels()
{
	if (!m_drawAttackCollisionDebug || !m_drawAttackCollisionLabels)
		return;

	const auto& collision = m_combat.GetCollisionDebugState();
	const Matrix4x4 view = m_camera.GetViewMatrix();
	const Matrix4x4 projection = m_camera.GetProjMatrix();
	const bool showSwordSweep = collision.swordTransformValid &&
		m_combat.IsPlayerAttackActive();
	if (!m_enemies.empty() && m_drawEnemyAabb)
	{
		DrawWorldCollisionLabel(
			view, projection, GetAabbCenter(collision.enemyBroadPhase),
			ImVec2(-135.0f, -72.0f), "ENEMY AABB [BROAD]", IM_COL32(30, 145, 255, 255));
	}
	if (collision.swordTransformValid && m_drawAttackAabb)
	{
		DrawWorldCollisionLabel(
			view, projection, GetAabbCenter(collision.attackBroadPhase),
			ImVec2(-150.0f, 45.0f), "ATTACK AABB [BROAD]", IM_COL32(30, 230, 255, 255));
	}
	if (m_drawPlayerObb || m_drawEnemyObb || m_drawSwordObb)
	{
		if (m_drawPlayerObb)
		{
			DrawWorldCollisionLabel(
				view, projection, collision.playerObb.worldcenter,
				ImVec2(-180.0f, 4.0f), "PLAYER OBB [AUTO]", IM_COL32(40, 190, 255, 255));
		}
		if (!m_enemies.empty() && m_drawEnemyObb)
		{
			DrawWorldCollisionLabel(
				view, projection, collision.enemyObb.worldcenter,
				ImVec2(-180.0f, -34.0f), "ENEMY OBB [NARROW]", IM_COL32(45, 255, 75, 255));
		}
		if (collision.swordTransformValid && m_drawSwordObb)
		{
			DrawWorldCollisionLabel(
				view, projection, collision.bladeObb.worldcenter,
				ImVec2(45.0f, -10.0f), "BLADE OBB", IM_COL32(255, 95, 10, 255));
			if (showSwordSweep)
			{
				DrawWorldCollisionLabel(
					view, projection, collision.tipSweepObb.worldcenter,
					ImVec2(45.0f, 28.0f), "SWEEP OBB", IM_COL32(220, 45, 255, 255));
			}
		}
	}
}
