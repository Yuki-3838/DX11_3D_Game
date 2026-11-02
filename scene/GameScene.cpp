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
#include	"../system/GameInput.h"
#include	"../system/AttackTimelineFile.h"
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
	// 口先の位置。首の先(neck3)から上あごの骨への向きに、その長さの何倍延ばすか。
	// 上あごの骨はあごの付け根にあるので、延ばさないと火球が頭の中から出てくる。
	constexpr float ENEMY_MOUTH_EXTEND = 0.6f;
	// 床の上面の高さ(m_arenaFloor.Initialize に渡している値)。火球はここへ落ちたら爆発する。
	constexpr float ARENA_FLOOR_TOP_Y = -0.29f;
	// プレイヤーの体の中心の高さと太さ(身長約18)。火球との当たりを見るのに使う。
	constexpr float PLAYER_BODY_CENTER_HEIGHT = 9.0f;
	constexpr float PLAYER_BODY_RADIUS = 5.0f;

	// --- 敵の動きの演出(EnemyMotionEffects・攻撃の通り道) ---
	// 足の骨。踏んだ瞬間に土煙を出す。前脚・後脚の左右。
	constexpr const char* ENEMY_FOOT_BONES[4] = {
		"Armature_leg_front_foot_L", "Armature_leg_front_foot_R",
		"Armature_leg_back_foot_L", "Armature_leg_back_foot_R" };
	// 踏んだとみなす高さ。足の骨は足首にあるので、接地していても床から約2の高さにある
	// (撮影のログで、立っている・構えている間の4本とも1.75〜2.1)。これより下へ下りてきたら踏んだ。
	// (以前は歩きの後脚が畳んだまま床から9以上にあって床に着かなかった。CAnimationMesh で、クリップに無い骨を
	// 休止姿勢へ戻すようにしてからは、歩きでも後脚が床から1.6〜5の高さで地面を踏み、土煙が出る。)
	constexpr float ENEMY_FOOT_CONTACT_HEIGHT = 3.0f;
	// 踏んだ後、次に踏んだとみなすまでに足を上げる高さ。小さな揺れで何度も土煙が出ないようにする。
	// 突進の構えの足踏みは前脚が約3.5までしか上がらない(ログで確認)ので、それでも1回ずつ出る値にする。
	constexpr float ENEMY_FOOT_LIFT_MIN = 0.4f;
	// 尾の先端。最後の骨(tail3)は尾の途中にあるので、tail2→tail3 の向きにその長さの何倍か延ばす。
	constexpr float ENEMY_TAIL_TIP_EXTEND = 1.0f;
	// 頭の通り道の先端。口先から、首の先→口先の向きへその長さの何倍か延ばす。
	// 口先までだと帯がほとんど頭の中に入ってしまい、頭に隠れて見えなかった(撮影で確認)。
	constexpr float ENEMY_HEAD_TRAIL_EXTEND = 0.35f;
	// 突進で走っている間・滑っている間に土煙を出す間隔。
	constexpr float ENEMY_CHARGE_DUST_INTERVAL = 0.05f;
	constexpr float ENEMY_SKID_DUST_INTERVAL = 0.03f;
	// 登場演出で敵が空から落ちてきて着地するまでの時間。
	constexpr float ENEMY_INTRO_FALL_SECONDS = 1.35f;
	// 咆哮の衝撃波の間隔と広がり。
	constexpr float ENEMY_ROAR_RING_INTERVAL = 0.22f;
	constexpr float ENEMY_ROAR_RING_RADIUS = 75.0f;

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

	// 壁に沿って滑らせる移動量を求める(モンスターハンターやフロム作品と同じ、壁に当たっても止まらない動き)。
	// いったん全部動かしてから、めり込んだ壁ごとに法線方向へ押し戻す。
	// 押し戻した後の位置で別の壁にめり込むことがある(多角形の闘技場の角)ので、数回くり返す。
	// calcWallAdjustedMove()は「動いた先が壁に重なるなら止める」作りで、
	// 角に来るたびに止まり、壁へ斜めに歩くと進めなくなっていた。
	Vector3 calcWallSlideMove(
		std::vector<WallData>& walldatas,
		float radius,
		Vector3 currentCenter,
		Vector3 velocity)
	{
		constexpr int MAX_ITERATIONS = 4;
		// 押し戻しをぴったり半径にすると、次のフレームでまた接触扱いになる。わずかに離す。
		constexpr float SKIN = 0.01f;
		Vector3 move = velocity;
		for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration)
		{
			const std::vector<WallCollision> hits = checkWallCollision(
				walldatas, radius, currentCenter, move);
			if (hits.empty())
				break;
			for (const auto& hit : hits)
			{
				const PLANEINFO pi = hit.walldata.plane.GetPlaneInfo();
				Vector3 normal(pi.plane.a, pi.plane.b, pi.plane.c);
				const float normalLength = normal.Length();
				if (normalLength <= 0.0001f)
					continue;
				normal /= normalLength;
				const float planeD = pi.plane.d / normalLength;
				// 移動前にいた側を「内側」とする。壁の向き(法線の正負)に頼らない。
				float side = normal.Dot(currentCenter) + planeD >= 0.0f ? 1.0f : -1.0f;
				const Vector3 next = currentCenter + move;
				const float distance = (normal.Dot(next) + planeD) * side;
				const float penetration = radius + SKIN - distance;
				if (penetration > 0.0f)
					move += normal * (side * penetration);
			}
		}
		// 押し戻しても抜けられないとき(壁と壁の間に挟まれたなど)は、元の位置に留める。
		if (!checkWallCollision(walldatas, radius, currentCenter, move).empty() &&
			move.LengthSquared() > 0.0f &&
			checkWallCollision(walldatas, radius, currentCenter, Vector3(0, 0, 0)).empty())
		{
			return Vector3(0, 0, 0);
		}
		return move;
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

	constexpr float fallDuration = ENEMY_INTRO_FALL_SECONDS;
	constexpr float roarDelay = 0.12f;
	// 読み込んだ咆哮を最後まで聞かせてからプレイヤーへ操作を返す。
	constexpr float battleStartTime = 4.55f;
	const float previousIntroTime = m_enemyIntroTime;
	m_enemyIntroTime += deltaSeconds;
	// 着地の瞬間。巨体が落ちてきた重さを、土煙と地面を走る衝撃の輪で見せる。
	if (previousIntroTime < fallDuration && m_enemyIntroTime >= fallDuration)
	{
		Vector3 ground = m_enemyIntroLandingPosition;
		ground.y = ARENA_FLOOR_TOP_Y;
		m_enemyMotionFx.SpawnGroundRing(ground, 110.0f, 0.60f, 0.55f);
		// 濃くしすぎると体の下半分が隠れる(撮影で確認)ので、輪で重さを見せ、土煙は薄めにする。
		m_enemyMotionFx.SpawnDust(ground, Vector3(0.0f, 0.0f, 0.0f), 26, 90.0f, 14.0f, 1.4f, 0.30f);
		// 足の高さの記録は着地してから取り直す(落ちている間の高さを接地とみなさないように)。
		for (EnemyFootTracker& foot : m_enemyFeet)
			foot = EnemyFootTracker{};
	}

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
		// 衝撃波は、声が大きい間(音の0.15〜2.3秒)だけ出す。
		StartEnemyRoarEffect(Combat::PoseTuning::INTRO_ROAR_HOLD_END - 0.1f);
		m_enemyRoarPlayed = true;
	}
	// 咆哮の動き(首を上げ、口を開けたまま吠え、閉じる)。音を鳴らしてからの秒を渡す。
	// 以前は待機のクリップのまま声だけが鳴り、口を閉じたまま吠えていた。
	m_enemies.front()->setIntroRoarTime(
		m_enemyRoarPlayed ? m_enemyIntroTime - (fallDuration + roarDelay) : -1.0f);

	UpdateEnemyAnimation(deltaSeconds);
	UpdateEnemyMotionEffects(deltaSeconds);
	if (m_enemyIntroTime >= battleStartTime)
	{
		m_enemies.front()->setIntroRoarTime(-1.0f);
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
		// 調整・撮影用: player_start_z で戦闘開始時のプレイヤーの位置を奥(+Z、壁の方)へずらす。
		// 突進が壁にぶつかる様子を撮るには、プレイヤーの後ろに壁が来ている必要があるため。
		const std::string playerStartZ = GetDevSetting("player_start_z", "");
		if (!playerStartZ.empty() && m_player)
		{
			SRT start = m_player->getSRT();
			start.pos = Vector3(0.0f, start.pos.y, std::stof(playerStartZ));
			m_player->setSRT(start);
		}
		// 調整用: enemy_passive=1 で敵を攻撃させない(プレイヤーの攻撃モーションを落ち着いて見るため)。
		m_enemies.front()->setPassive(GetDevSetting("enemy_passive", "0") == "1");
		// 調整・撮影用: rage_at_start=1 で戦闘開始と同時に怒らせる。
		// 実際に怒らせるには260ダメージ分殴る必要があり、咆哮や怒り中の動きを見るのに時間がかかるため。
		if (GetDevSetting("rage_at_start", "0") == "1")
			m_enemies.front()->forceRage();
		// 撮影・比較用: enemy_attack_pose=0 で構えの演出を切り、構えを入れる前の見た目を再現する。
		enemy::setAttackPoseEnabled(GetDevSetting("enemy_attack_pose", "1") != "0");
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
	if (!m_enemyIntroActive)
		m_battleSeconds += deltaSeconds;
	// 調整用: dev_settings.ini の auto_walk で自動的に歩かせる(1=前へ、2=右へ、3=左へ、4=後ろへ)。
	// 撮影で動きを確認するとき、外からキー入力を送らずに済ませるため。
	if (m_player)
	{
		static const std::string autoWalk = GetDevSetting("auto_walk", "0");
		m_player->setDebugForcedMove(
			autoWalk == "1" ? 1.0f : (autoWalk == "4" ? -1.0f : 0.0f),
			autoWalk == "2" ? 1.0f : (autoWalk == "3" ? -1.0f : 0.0f));
	}
	// F6: 攻撃のタイムラインエディターを開く・閉じる。開いている間はゲームを止める。
	if (input.IsKeyTriggered(DIK_F6) && !ImGui::GetIO().WantTextInput && m_player)
	{
		if (m_timelineEditor.IsOpen())
			CloseTimelineEditor();
		else
			OpenTimelineEditor();
	}
	// 起動引数 --timeline(VSの構成「攻撃タイムライン」)なら、最初からエディターを開いておく。
	if (GameFlow::openAttackTimelineOnStart && m_player && !m_timelineEditor.IsOpen())
	{
		GameFlow::openAttackTimelineOnStart = false;
		OpenTimelineEditor();
	}
	// エディターをウィンドウの×で閉じた場合も、閉じたときの後片付けをする。
	if (m_timelineEditorWasOpen && !m_timelineEditor.IsOpen())
		CloseTimelineEditor();
	// エディターでは、タイムラインを触るためにカーソルを出しておく。
	const bool lockCursor = m_mouseLookCaptured && !m_resultRequested && !m_timelineEditor.IsOpen();
	DebugUI::SetCursorLocked(lockCursor);
	DebugUI::SetCursorVisible(!lockCursor);
	if (m_timelineEditor.IsOpen())
	{
		UpdateTimelineEditor(deltaSeconds);
		return;
	}
	// ゲームの操作はGameInput(仮想コントローラー)から「行動」として受け取る。
	// キーボード・マウスとゲームパッドのどちらで押したかは、ここでは区別しない。
	const GameInput& gameInput = GameInput::Get();
	if (!m_enemyIntroActive && gameInput.IsTriggered(GameAction::LockOn))
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
		m_enemyFireball.Clear();
		ClearEnemyMotionEffects();
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
	const bool rawAttackTriggered = gameInput.IsTriggered(GameAction::Attack);
	const bool rawHeavyAttackTriggered = gameInput.IsTriggered(GameAction::HeavyAttack);
	const bool rawDodgeTriggered = gameInput.IsTriggered(GameAction::Dodge);
	const bool playerDodgeActive = m_player->isDodging();

	// 先行入力。押した瞬間に出せなかった攻撃・回避を覚えておく。
	// 以前は回避中のクリックや、攻撃の振り始め(回避で切れない区間)のスペースを捨てていたため、
	// 「押したのに出ない」と感じる場面があった。
	if (m_bufferedAction != BufferedAction::None)
	{
		m_bufferedActionTime -= deltaSeconds;
		if (m_bufferedActionTime <= 0.0f)
			m_bufferedAction = BufferedAction::None;
	}
	// 吹き飛ばされている間の入力は覚えない(飛ばされ終わった瞬間に勝手に動くと、被弾の代償が薄れる)。
	if (m_player->isKnockedBack())
		m_bufferedAction = BufferedAction::None;
	else
	{
		// 攻撃中の攻撃は戦闘側がコンボの予約として受け付けるので、ここで覚えるのは回避中だけ。
		const auto remember = [this](BufferedAction action) {
			m_bufferedAction = action;
			m_bufferedActionTime = INPUT_BUFFER_SECONDS;
		};
		if (playerDodgeActive && rawHeavyAttackTriggered)
			remember(BufferedAction::HeavyAttack);
		else if (playerDodgeActive && rawAttackTriggered)
			remember(BufferedAction::Attack);
		// 回避は、今すぐ出せないときだけ覚える(出せるときはそのまま出る)。
		const bool dodgeBlockedNow = playerDodgeActive ||
			(m_combat.IsPlayerAttacking() && !m_combat.CanCancelPlayerAttack(m_attackCancelEndFrame));
		if (rawDodgeTriggered && dodgeBlockedNow)
			remember(BufferedAction::Dodge);
		// 回避を覚えた後に攻撃を押し直したら、後から押した方(攻撃=コンボの予約)を優先する。
		else if (!playerDodgeActive && (rawAttackTriggered || rawHeavyAttackTriggered) &&
			m_bufferedAction == BufferedAction::Dodge)
			m_bufferedAction = BufferedAction::None;
	}
	// 覚えていた入力を、出せる状態になったら今押したものとして扱う。
	const bool bufferedAttack = !playerDodgeActive && m_bufferedAction == BufferedAction::Attack;
	const bool bufferedHeavyAttack = !playerDodgeActive && m_bufferedAction == BufferedAction::HeavyAttack;
	const bool attackTriggered = rawAttackTriggered || bufferedAttack;
	const bool heavyAttackTriggered = rawHeavyAttackTriggered || bufferedHeavyAttack;
	if (bufferedAttack || bufferedHeavyAttack)
		m_bufferedAction = BufferedAction::None;
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
		AimPlayerAttack();
		if (heavyAttackInput)
			m_playerAnimator.PlayHeavyAttackMotion(1);
		else if (playerWasRunning)
			m_playerAnimator.PlayDashAttackMotion();
		else
			m_playerAnimator.PlayAttackMotion(1);
		BeginAttackTimeline(Combat::PlayerAttackSlotOf(
			heavyAttackInput, 1, !heavyAttackInput && playerWasRunning));
		m_lastPlayerComboStep = 1;
		m_lastPlayerHeavyAttack = heavyAttackInput;
	}
	const bool dodgeTriggered = rawDodgeTriggered ||
		(m_bufferedAction == BufferedAction::Dodge && !attackStarted);
	// 調整・撮影用: auto_sprint=1 で走り続ける(auto_walk と組み合わせて「逃げる」動きを撮る)。
	static const bool autoSprint = GetDevSetting("auto_sprint", "0") == "1";
	const bool sprinting = (gameInput.IsPressed(GameAction::Sprint) || autoSprint) &&
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
		if (m_bufferedAction == BufferedAction::Dodge)
			m_bufferedAction = BufferedAction::None;
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
	// 壁に当たったら止めずに、壁に沿って滑らせる。
	// 歩き・回避・吹き飛ばし・攻撃の踏み込みのどれも、ここを通る。
	Vector3 adjustedPlayerMove = calcWallSlideMove(
		walldatas,
		prevPlayerSphere.radius,
		prevPlayerSphere.center,
		playerMove);

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


		// 接地している足のつま先をワールドへ固定し、脚をIKで合わせる(足の滑りを消す)。
		UpdateFootLock(deltaSeconds);

		// 接地。変形後の体の一番低い点を毎フレーム測り、それを地面の高さに合わせる。
		//
		// **足のIKより後で行うこと**。先に行うと、IKが脚を動かした分だけ高さがずれる
		// (実測で待機中に平均1.37単位ずれた)。IKが使う高さは1フレーム前の値になるが、
		// 高さはIKの目標に入れていない(水平だけを固定している)ので問題にならない。
		//
		// 以前は骨の初期姿勢(棒立ち)の高さで固定し、前転のときだけ測っていた。
		// しかしクリップの脚は初期姿勢と同じ高さにあるとは限らない。実測すると、
		//   歩き中: 体が0.3〜2.0単位浮く(歩きクリップは腰が沈む前提で作られている)
		//   攻撃中: 最大4.6単位浮く
		// さらに悪いことに、歩きをやめた瞬間に足の固定(FootLock)がその浮いた高さで掛かり、
		// **宙に立ったまま固まっていた**(止まった後ずっと1.96単位浮いたまま。実測)。
		// 「待機で浮く」「歩きがかくつく」「攻撃の足元が滑らかでない」はすべてこれが原因。
		//
		// 足元の高さは**足の骨から**求める。
		// 体のすべての頂点の最下点を使うと、剣を振り下ろしたときに**剣の先**が最下点になり、
		// 剣を地面に着けるために体が持ち上がって、足のほうが浮いてしまう
		// (このモデルは剣がメッシュに入っている)。
		// 最初のフレームで「頂点の最下点」と「足の骨」の差(靴底までの距離)を測り、以後はそれを足す。
		// 頂点をCPUで変形しなくて済むので軽くもなる。
		//
		// 前転だけは体を丸めて背中から接地するので、従来どおり頂点の最下点を使う。
		float lowestFootY = 0.0f;
		bool haveFootBones = false;
		{
			const std::string* footBones[] = {
				&m_leftLegChain.foot, &m_leftLegChain.toe,
				&m_rightLegChain.foot, &m_rightLegChain.toe };
			for (const std::string* bone : footBones)
			{
				Vector3 position{};
				if (bone->empty() ||
					!m_playerAnimationMesh->GetBoneModelPosition(*bone, position))
					continue;
				if (!haveFootBones || position.y < lowestFootY)
					lowestFootY = position.y;
				haveFootBones = true;
			}
		}
		if (haveFootBones && !m_playerSoleOffsetReady)
		{
			m_playerSoleOffsetY = m_playerAnimationMesh->GetAnimatedLocalMinY() - lowestFootY;
			m_playerSoleOffsetReady = true;
		}
		const bool useFootBones =
			haveFootBones && m_playerSoleOffsetReady && !m_player->isDodging();
		const float lowestLocalY = useFootBones
			? lowestFootY + m_playerSoleOffsetY
			: m_playerAnimationMesh->GetAnimatedLocalMinY();
		const float groundedOffset =
			m_playerGroundY - lowestLocalY * m_player->getSRT().scale.y;
		if (!m_playerGroundOffsetReady)
		{
			m_playerGroundOffsetY = groundedOffset;
			m_playerGroundOffsetReady = true;
		}
		// 技の間は、**クリップが本当に体を持ち上げた分だけ**浮かせる。
		//
		// 経緯:
		//  1. 「一番低い足を地面へ」をそのまま技にも当てたら、ダッシュ攻撃で脚を抱えた分だけ
		//     体が引き下ろされ、膝が地面についたまま飛んでいた(ユーザー指摘)。
		//  2. そこで技の間は「体を下げない」ことにしたら、今度は弱1・弱3・強攻撃が浮いた
		//     (これらのクリップは跳んでおらず、足が初期姿勢より高いだけだった)。
		//  3. 腰が上がったかで自動判定しようとしたが、**膝を伸ばしただけでも腰は上がる**ので
		//     区別できなかった(弱2・弱3が0.8〜0.9単位浮いた)。
		//  4. いまは**技ごとに「地面を離れる技か」を決めている**(いまはダッシュ攻撃だけ)。
		//     離れる技の間だけ体を下げない。ほかの技は足を地面に着ける。
		const bool clipDecidesHeight =
			m_combat.IsPlayerAttacking() || m_playerAnimator.IsMotionPlaying() ||
			m_player->isKnockedBack();
		const bool leavingGround =
			clipDecidesHeight && m_playerAnimator.AttackLeavesGround();
		const float targetGroundOffset = leavingGround
			? std::max(groundedOffset, m_playerStandingGroundOffsetY)
			: groundedOffset;
		// **体の高さは「踏んでいる足」が決める。一番低い足ではない。**
		//
		// 毎フレーム「一番低い足」を地面へ合わせていたが、それは走りで破綻する。
		// 走りには両足とも地面を離れる瞬間(跳んでいる間)があり、そこで一番低い足を
		// 地面へ下ろそうとして体が沈む。実測すると走りで体が**2.34単位**(身長の13%)上下しており、
		// これが「走りががくがく」の正体だった(歩きは0.87単位で、人の歩きとほぼ同じ)。
		//
		// 足が地面へめり込むのはすぐ直し(持ち上げは即座)、
		// 足が地面から離れているぶんはゆっくり追う(跳んでいる間は体を下げない)。
		// こうすると体の高さは踏んでいる足の高さに落ち着き、跳ぶ動きはそのまま残る。
		const float difference = targetGroundOffset - m_playerGroundOffsetY;
		if (difference > 0.0f)
		{
			// 持ち上げ: めり込みを直す。飛ばないように速さだけ制限する。
			m_playerGroundOffsetY += std::min(difference, 240.0f * deltaSeconds);
		}
		else
		{
			// 下げる速さは場面で変える。
			//
			// **ゆっくり下げるのは走っているときだけ**(4/秒)。
			// 走りには両足とも地面を離れる瞬間があり、そこで素早く下げると体が沈んで見える。
			//
			// それ以外は素早く(60/秒)。
			// ・技の間: クリップが足を体へ引き上げるので、ゆっくりだと追いつけず
			//   技の間ずっと浮いたままになる(実測: 弱1で平均1.52単位浮いていた)。
			// ・技が終わった直後: 脚は前の技の姿勢から戻っている途中で、足がまだ高い。
			//   ここでゆっくり下げると**技のあとしばらく浮いたまま**になる
			//   (ユーザー指摘「強攻撃三段目終わった後、浮いてしまう」)。
			// 立ち止まっているときは両足が地面を離れることはないので、素早く下げてよい。
			const bool movingNow =
				m_player->getMotionState() == player::MotionState::Run ||
				m_player->getMotionState() == player::MotionState::Walk;
			const float lowerPerSecond = (movingNow && !clipDecidesHeight) ? 4.0f : 60.0f;
			m_playerGroundOffsetY +=
				difference * (1.0f - std::exp(-deltaSeconds * lowerPerSecond));
		}
		m_player->setVisualGroundOffsetY(m_playerGroundOffsetY);

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

	// 攻撃の固定(調整用)を敵へ渡す。以前はデバッグ表示の「戦闘」タブの中で渡していたため、
	// タブを開いていない・デバッグ表示を出していないときは dev_settings.ini の
	// force_enemy_attack が効かなかった。
	if (!m_enemies.empty())
	{
		const Combat::EnemyAttackKind forced =
			m_forcedEnemyAttackIndex == 1 ? Combat::EnemyAttackKind::Bite :
			m_forcedEnemyAttackIndex == 2 ? Combat::EnemyAttackKind::Sweep :
			m_forcedEnemyAttackIndex == 3 ? Combat::EnemyAttackKind::TailSpin :
			m_forcedEnemyAttackIndex == 4 ? Combat::EnemyAttackKind::Fireball :
			m_forcedEnemyAttackIndex == 5 ? Combat::EnemyAttackKind::Charge :
			Combat::EnemyAttackKind::Slam;
		m_enemies.front()->setForcedAttackKind(m_forceEnemyAttack ? &forced : nullptr);
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
			if (e->getAttackKind() == Combat::EnemyAttackKind::Fireball)
				LaunchEnemyFireball(*e);
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

		// 突進が壁で止められたか。壁に浅く当たって沿って滑るぶんは止まったとみなさず、
		// 走る向きへ進めた量が6割未満になった(壁へ約37度より正面寄りにぶつかった)ときだけよろめかせる。
		// 壁際へ誘って突進を空振りさせると大きな隙ができる、という立ち回りを作るため。
		// 動けた量(長さ)では判定できない。壁の処理は、ぶつかった分を壁に沿う向きへ
		// 同じ速さで振り替えるので、正面からぶつかっても長さは減らない(撮影で確認)。
		const float intendedMove = enemyMove.Length();
		const float chargeYaw = prevEnemySrt.rot.y;
		const Vector3 chargeForward(-std::sinf(chargeYaw), 0.0f, -std::cosf(chargeYaw));
		if (e->isChargeRunning() && intendedMove > 0.01f &&
			adjustedEnemyMove.Dot(chargeForward) < intendedMove * 0.6f)
		{
			OnEnemyChargeHitWall(*e, prevEnemySrt.pos + chargeForward * 26.0f, chargeForward);
		}

		enemySrt.pos = prevEnemySrt.pos + adjustedEnemyMove;
		e->setSRT(enemySrt);
		e->setVel(adjustedEnemyMove);
	}
	UpdateEnemyAnimation(deltaSeconds);
	// 体(頭・首・胴・尾・脚)が壁にめり込んでいたら、体ごと内側へ押し戻す。
	// 上の壁の処理は敵の位置の球1つだけで見ていて、長い体の頭や尾は壁を突き抜けていた。
	PushEnemyBodyOutOfWalls();
	// 敵の動きの演出は、骨の姿勢が決まった後で骨の位置から出す。
	UpdateEnemyMotionEffects(deltaSeconds);

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

		// 剣の軌跡とタイムラインのイベント(効果音・カメラの揺れ・火花)。
		// どちらも、再生している技のクリップの時刻で決める(タイムラインエディターで調整した区間・瞬間)。
		// 軌跡は既定では攻撃判定と同じ区間に出る。「刃が危険な時間」がそのまま光って見えるので、
		// 予兆と判定を読ませたいこのゲームの方針とも噛み合う。
		UpdateAttackTimelineEvents();
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
		// 敵AIが選んだ攻撃を戦闘システムへ渡す。
		// 予兆として見えているモーションと、実際のダメージ・射程・タイミングを一致させるため。
		m_combat.SetEnemyAttackKind(m_enemies.front()->getAttackKind());
		// 攻撃判定を正面からの角度で絞るため、敵の向きも渡す。
		// 描画用SRTには演出のひねりが入っているので、必ず物理SRTの向きを使う。
		m_combat.SetEnemyFacingYaw(m_enemies.front()->getSRT().rot.y);
		// 尾回転は半回転の回数で判定の長さと「一拍で止まっている時間」が変わる。
		// 敵AIが決めた回数をそのまま渡し、判定側も同じ計算をする。
		m_combat.SetEnemySpinHalfTurns(m_enemies.front()->getSpinHalfTurns());
		m_combat.SetPlayerSprinting(playerWasRunning);
		// 吹き飛ばされている間は無敵にする。飛ばされた先で次の攻撃に拾われ続けると、
		// 1回の読み違えで何もできないまま体力を削り切られてしまう。
		// 火球の爆発も同じ無敵を見る(前転で爆発をくぐり抜けられる)。
		const bool playerInvincible =
			m_player->isKnockedBack() ||
			(m_player->isDodging() &&
			m_player->getDodgeFrame() >= m_dodgeInvincibleStartFrame &&
			m_player->getDodgeFrame() <= m_dodgeInvincibleEndFrame);
		m_combat.Update(
			deltatime,
			m_player->getSRT().pos,
			enemyPosition,
			attackInput,
			heavyAttackInput,
			m_enemies.front()->getMotionState() == enemy::MotionState::Windup,
			playerInvincible,
			swordBase,
			swordTip,
			previousSwordTip,
			swordTransformValid,
			playerObb,
			enemyObb);
		// 命中の演出は「当たったか」で出す。以前は体力が減ったかで判定していたが、
		// それだと撮影・調整用の無敵で体力を減らさないときに、火花や手応えまで消えてしまう。
		if (m_combat.DidPlayerHitLand())
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
			// 与えたダメージは怒りにも溜まる。しきい値を超えると敵が咆哮して怒り状態へ入る。
			// 怯みより優先する(吠えている最中にのけぞらせると、どちらの動きか分からなくなる)。
			if (m_enemies.front()->addRageDamage(static_cast<float>(playerAttack.damage)))
			{
				// 咆哮の間は攻撃が来ない。戦闘側の攻撃も取り消して、構えかけを捨てさせる。
				m_combat.CancelEnemyAttack();
				SoundManager::PlayEnemyRoar();
				StartEnemyRoarEffect(Combat::Tuning::ENEMY_RAGE_ROAR_SECONDS * 0.8f);
				// 怯みより強く長く揺らす。「相手の圧が一段上がった」ことを体で伝える。
				m_camera.TriggerShake(3.0f, 0.55f);
				m_playerAnimator.TriggerHitStop(0.10f);
			}
			else if (m_enemies.front()->addPostureDamage(postureDamage, m_player->getSRT().pos))
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
			{
				SoundManager::PlayEnemyRoar();
				// 苦しげな短い咆哮。怒りの咆哮より短くする。
				StartEnemyRoarEffect(0.7f);
			}
		}

		if (m_combat.DidEnemyHitLand())
		{
			m_camera.TriggerShake(2.6f, 0.35f);
			m_playerDamageFlashTime = 0.45f;

			// 食らった攻撃の重さに応じて吹き飛ばす。向きは敵から離れる方向。
			// 攻撃中なら攻撃は取り消す(飛ばされながら剣を振り続けないように)。
			const Combat::KnockbackData knockback =
				Combat::EnemyKnockbackOf(m_enemies.front()->getAttackKind());
			Vector3 awayFromEnemy = m_player->getSRT().pos - enemyPosition;
			// 突進は走る向きの横へはね飛ばす。敵から離れる向き(=走る向き)へ飛ばすと、
			// 走り続けている敵がすぐ追いつき、飛ばされた後も体で押して運んでしまう(撮影で確認)。
			// 横へ飛ばせば敵はそのまま通り過ぎる(モンスターハンターの突進と同じ)。
			if (m_enemies.front()->getAttackKind() == Combat::EnemyAttackKind::Charge)
			{
				const float yaw = m_enemies.front()->getSRT().rot.y;
				const Vector3 forward(-std::sinf(yaw), 0.0f, -std::cosf(yaw));
				const Vector3 right(-forward.z, 0.0f, forward.x);
				const float side = awayFromEnemy.x * right.x + awayFromEnemy.z * right.z;
				awayFromEnemy = right * (side >= 0.0f ? 1.0f : -1.0f) + forward * 0.5f;
			}
			if (m_combat.IsPlayerAttacking())
				m_combat.CancelPlayerAttack();
			m_player->applyKnockback(awayFromEnemy, knockback.speed, knockback.seconds);
			m_playerAnimator.PlayImpactMotion();
		}

		// 火球。吐いた後も敵の攻撃の段階とは関係なく飛び続けるので、ここで別に進める。
		UpdateEnemyFireballs(deltaSeconds, playerInvincible);
		WriteCaptureLog();

		// どちらかのHPが0になったら結果を保存してリザルト画面へ遷移する。
		if (!m_resultRequested &&
			(m_combat.IsEnemyDefeated() || m_combat.IsPlayerDefeated()))
		{
			GameFlow::SetResult(
				m_combat.IsEnemyDefeated()
				? GameFlow::Result::Victory
				: GameFlow::Result::Defeat);
			m_resultRequested = true;
			// ゲームループを使うならリザルトへ。本編だけなら同じ戦いをやり直す。
			if (GameFlow::useGameLoop)
				GameFlow::RequestScene("ResultScene");
			else
				GameFlow::RequestRestart();
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
		// コンボの2段目以降も、段の出だしで入力方向へ向き直れる(モンスターハンターと同じ)。
		AimPlayerAttack();
		if (m_combat.IsPlayerHeavyAttack())
			m_playerAnimator.PlayHeavyAttackMotion(comboStep);
		else
			m_playerAnimator.PlayAttackMotion(comboStep);
		BeginAttackTimeline(Combat::PlayerAttackSlotOf(m_combat.IsPlayerHeavyAttack(), comboStep));
		m_lastPlayerComboStep = comboStep;
		m_lastPlayerHeavyAttack = m_combat.IsPlayerHeavyAttack();
	}
	if (!m_combat.IsPlayerAttacking())
	{
		m_lastPlayerComboStep = 0;
		m_lastPlayerHeavyAttack = false;
	}
}

void GameScene::BeginAttackTimeline(Combat::PlayerAttackSlot slot)
{
	m_playingAttackSlot = slot;
	m_attackTimelineActive = true;
	// 技の使い始めより前から数え始める(使い始めちょうどに置いたイベントも起こすため)。
	m_lastAttackEventClipTime = Combat::EditablePlayerAttack(slot).clipStart - 0.0001f;
}

void GameScene::UpdateAttackTimelineEvents()
{
	if (!m_playerAnimationMesh)
		return;
	const float clipTime = m_playerAnimator.GetPlayingAttackClipSeconds();
	// 技が終わった・回避で取り消された、などで技のクリップの再生が止まったら追うのをやめる。
	// 戦闘側の「攻撃中か」では判断しない。技を始めたフレームでは戦闘側の更新がまだ後にあり、
	// そこで追うのをやめると、その技の間ずっと軌跡もイベントも出なくなる(実際にそうなっていた)。
	if (clipTime < 0.0f)
	{
		m_attackTimelineActive = false;
		return;
	}
	if (!m_attackTimelineActive)
		return;
	const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(m_playingAttackSlot);

	// 前のフレームから今のフレームまでに通り過ぎたイベントを起こす。
	// ヒットストップ中はクリップが止まるので、同じイベントが2回起きることはない。
	if (clipTime > m_lastAttackEventClipTime)
	{
		for (const Combat::TimelineEvent& event : step.events)
			if (event.time > m_lastAttackEventClipTime && event.time <= clipTime)
				FireTimelineEvent(event);
		m_lastAttackEventClipTime = clipTime;
	}

	// 剣の軌跡。渡すのは武器ボーンから求めた刃のワールド座標だけなので、
	// 剣やプレイヤーのモデルを差し替えても、この処理は変更しなくてよい。
	Vector3 trailBase{};
	Vector3 trailTip{};
	Vector3 trailPreviousTip{};
	if (clipTime >= step.TrailStart() && clipTime <= step.TrailEnd() &&
		m_playerAnimationMesh->GetSwordWorldSweep(trailBase, trailTip, trailPreviousTip))
	{
		m_playerWeaponTrail.Push(trailBase, trailTip);
	}
}

void GameScene::FireTimelineEvent(const Combat::TimelineEvent& event)
{
	switch (event.type)
	{
	case Combat::TimelineEventType::Sound:
		switch (event.sound)
		{
		case Combat::TimelineSound::Whoosh:       SoundManager::PlayDodge(); break;
		case Combat::TimelineSound::SwordHit:     SoundManager::PlaySwordSwing(); break;
		case Combat::TimelineSound::DragonAttack: SoundManager::PlayDragonAttack(); break;
		case Combat::TimelineSound::Roar:         SoundManager::PlayEnemyRoar(); break;
		default: break;
		}
		break;
	case Combat::TimelineEventType::CameraShake:
		if (m_timelineEditor.IsOpen())
		{
			// エディターではカメラを直接置いているので、揺れはエディターのカメラで作る。
			m_timelineShakeStrength = event.strength;
			m_timelineShakeDuration = event.duration;
			m_timelineShakeTime = event.duration;
		}
		else
			m_camera.TriggerShake(event.strength, event.duration);
		break;
	case Combat::TimelineEventType::Sparks:
	{
		Vector3 base{};
		Vector3 tip{};
		Vector3 previousTip{};
		if (m_playerAnimationMesh && m_playerAnimationMesh->GetSwordWorldSweep(base, tip, previousTip))
		{
			// 刃の進む向きへ飛ばす。止まっていれば刃の向き。
			Vector3 direction = tip - previousTip;
			if (direction.LengthSquared() < 0.0001f)
				direction = tip - base;
			m_hitEffect.Spawn(tip, direction, event.count, event.strength);
		}
		break;
	}
	default:
		break;
	}
}

bool GameScene::ReloadAttackClip(Combat::PlayerAttackSlot slot)
{
	const std::string& file = Combat::EditablePlayerAttack(slot).clipFile;
	std::error_code error;
	if (!std::filesystem::exists(AttackTimelineFile::PathFromUtf8(file), error))
		return false;
	// 名前は読み込むたびに変える。同じ名前で読み直すと、再生中の技が古いクリップを指したまま
	// 名前の対応だけが切り替わる(古いクリップ自体はCAnimationDataが持ち続けるので壊れはしない)。
	static int reloadCount = 0;
	const std::string name = std::string("timeline_") + Combat::PlayerAttackSlotKey(slot) +
		"_" + std::to_string(++reloadCount);
	if (m_playerAnimationData.LoadAnimation(file, name) == nullptr)
		return false;
	aiAnimation* animation = m_playerAnimationData.GetAnimation(name.c_str(), 0);
	if (animation == nullptr)
		return false;
	m_playerAnimator.SetAttackAnimation(slot, animation);
	m_timelineAnalysisRevision = -1;
	return true;
}

void GameScene::OpenTimelineEditor()
{
	// 敵の登場演出の途中なら、演出を最後まで飛ばす。
	// エディターの間は演出が進まないので、飛ばさないと敵が空中で止まったままになり、
	// 「敵の前に立つ」や「敵に当たる」の計算も空中の敵で行われてしまう。
	if (m_enemyIntroActive && !m_enemies.empty())
	{
		m_enemyRoarPlayed = true;          // 飛ばすときは咆哮を鳴らさない
		m_enemyIntroTime = 1000.0f;        // 演出の終わり(戦闘開始)を過ぎた時刻にして、終わりの処理だけを通す
		UpdateEnemyIntro(0.0f);
	}
	// 攻撃の途中で開いても、閉じたときに続きから再生されないよう、技は取り消しておく。
	m_combat.CancelPlayerAttack();
	m_attackTimelineActive = false;
	m_playerWeaponTrail.Clear();
	// 飛んでいる火球はエディター中は止まって空中に残り、技の姿勢を隠すので消しておく。
	m_enemyFireball.Clear();
	ClearEnemyMotionEffects();
	m_timelineSavedPlayerSrt = m_player->getSRT();
	m_timelineSavedGroundOffsetY = m_playerGroundOffsetY;
	// カメラはプレイヤーの斜め前から見る(剣の振りと体の両方が見える向き)。
	// カメラの向きcのとき、カメラはプレイヤーから(sin c, -cos c)の方向に置かれる。
	// 正面は(-sin y, -cos y)なので、c = -y がちょうど正面。そこから横へ約60度回す。
	m_timelineCameraYaw = -TimelinePreviewBaseSrt().rot.y + PI * 0.35f;
	m_timelineCameraPitch = 0.22f;
	m_timelineCameraDistance = 75.0f;
	m_timelineShakeTime = 0.0f;
	m_timelineAnalysisRevision = -1;
	m_timelineEditor.Open();
	m_timelineEditorWasOpen = true;
}

void GameScene::CloseTimelineEditor()
{
	m_timelineEditor.Close();
	m_timelineEditorWasOpen = false;
	// プレビューで踏み込ませた位置から、開いたときの位置へ戻す。
	m_player->setSRT(m_timelineSavedPlayerSrt);
	m_playerGroundOffsetY = m_timelineSavedGroundOffsetY;
	m_player->setVisualGroundOffsetY(m_playerGroundOffsetY);
	m_playerWeaponTrail.Clear();
	// 戦闘カメラは、閉じた後の最初の更新で追従点を取り直す。
	m_camera.Reset(m_player->getSRT().pos, m_player->getSRT().rot.y);
	// 変更はゲームの攻撃にすぐ効くが、保存しないと次に起動したときには消える。忘れないよう知らせる。
	m_timelineUnsavedNoticeTime = m_timelineEditor.IsDirty() ? 6.0f : 0.0f;
}

void GameScene::PoseTimelinePreview(Combat::PlayerAttackSlot slot, float clipSeconds)
{
	const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(slot);
	aiAnimation* clip = m_playerAnimator.GetAttackAnimation(slot);
	if (clip == nullptr || !m_playerAnimationMesh)
		return;
	m_playerAnimator.PoseAttackClip(*m_playerAnimationMesh, m_playerBoneComb, clip, clipSeconds, step.clipStart);

	// 踏み込み: 技の使い始めから今までに、クリップの腰が進んだ分だけ前へ出す(ゲームと同じ計算)。
	SRT srt = TimelinePreviewBaseSrt();
	float right = 0.0f;
	float forward = 0.0f;
	if (m_timelineEditor.ShowRootMotion() &&
		m_playerAnimator.SampleRootMotion(*m_playerAnimationMesh, clip,
			step.clipStart, std::max(clipSeconds, step.clipStart), step.rootMotionScale, right, forward))
	{
		const float yaw = srt.rot.y;
		const float forwardX = -std::sinf(yaw);
		const float forwardZ = -std::cosf(yaw);
		const float rightX = forwardZ;
		const float rightZ = -forwardX;
		srt.pos.x += forwardX * forward + rightX * right;
		srt.pos.z += forwardZ * forward + rightZ * right;
	}
	m_player->setSRT(srt);

	// 接地: ゲームと同じく、足の骨の一番低い点を地面へ合わせる。
	// 地面を離れる技(ダッシュ攻撃)は、立ち姿より下へは下げない(跳んでいる高さが見える)。
	float lowestFootY = 0.0f;
	bool haveFootBones = false;
	const std::string* footBones[] = {
		&m_leftLegChain.foot, &m_leftLegChain.toe,
		&m_rightLegChain.foot, &m_rightLegChain.toe };
	for (const std::string* bone : footBones)
	{
		Vector3 position{};
		if (bone->empty() || !m_playerAnimationMesh->GetBoneModelPosition(*bone, position))
			continue;
		if (!haveFootBones || position.y < lowestFootY)
			lowestFootY = position.y;
		haveFootBones = true;
	}
	if (haveFootBones && m_playerSoleOffsetReady)
	{
		const float grounded = m_playerGroundY - (lowestFootY + m_playerSoleOffsetY) * srt.scale.y;
		const float offset = slot == Combat::PlayerAttackSlot::Dash
			? std::max(grounded, m_playerStandingGroundOffsetY)
			: grounded;
		m_player->setVisualGroundOffsetY(offset);
	}
	m_playerAnimationMesh->UpdateSwordWorldTransform(m_player->getRenderSRT().GetMatrix());
}

SRT GameScene::TimelinePreviewBaseSrt() const
{
	SRT srt = TimelinePreviewStartSrt();
	// コンボを通して再生しているときは、前の段で踏み込んだ分だけ前から始める
	// (ゲームでも、次の段は前の段が止まった位置から始まる)。
	if (!m_timelineEditor.ShowRootMotion() || !m_playerAnimationMesh)
		return srt;
	for (const Combat::PlayerAttackSlot slot : m_timelineEditor.GetChainHistory())
	{
		const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(slot);
		aiAnimation* clip = m_playerAnimator.GetAttackAnimation(slot);
		float right = 0.0f;
		float forward = 0.0f;
		const float linkTime = std::min(step.ComboLinkClipTime(), step.clipEnd);
		if (!m_playerAnimator.SampleRootMotion(*m_playerAnimationMesh, clip,
			step.clipStart, linkTime, step.rootMotionScale, right, forward))
			continue;
		const float forwardX = -std::sinf(srt.rot.y);
		const float forwardZ = -std::cosf(srt.rot.y);
		srt.pos.x += forwardX * forward + forwardZ * right;
		srt.pos.z += forwardZ * forward - forwardX * right;
	}
	return srt;
}

SRT GameScene::TimelinePreviewStartSrt() const
{
	SRT srt = m_timelineSavedPlayerSrt;
	if (!m_timelineEditor.StandInFrontOfEnemy() || m_enemies.empty() || m_combat.IsEnemyDefeated())
		return srt;

	// 敵の体(当たり判定の箱)の表面から、間合いの距離だけ離れた位置に立つ。
	// 立つ方向は、エディターを開いたときにプレイヤーがいた方向(敵から見て)。
	const auto enemyObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
		m_localEnemyMeshBounds, m_enemies.front()->getRenderSRT());
	const Vector3 enemyCenter = enemyObb.worldcenter;
	Vector3 direction = srt.pos - enemyCenter;
	direction.y = 0.0f;
	if (direction.LengthSquared() < 0.0001f)
		direction = Vector3(0.0f, 0.0f, 1.0f);
	direction.Normalize();
	// 箱の表面までの水平な距離(箱の中なら0)。
	const auto distanceToBody = [&enemyObb](const Vector3& point) {
		const Vector3 offset(point.x - enemyObb.worldcenter.x, 0.0f, point.z - enemyObb.worldcenter.z);
		const Vector3 axes[3] = { enemyObb.axisX, enemyObb.axisY, enemyObb.axisZ };
		const float halves[3] = { enemyObb.lengthx * 0.5f, enemyObb.lengthy * 0.5f, enemyObb.lengthz * 0.5f };
		Vector3 closest = enemyObb.worldcenter;
		for (int i = 0; i < 3; ++i)
		{
			const float projected = std::clamp(offset.Dot(axes[i]), -halves[i], halves[i]);
			closest += axes[i] * projected;
		}
		const Vector3 gap(point.x - closest.x, 0.0f, point.z - closest.z);
		return gap.Length();
	};
	// 表面までの距離が間合いになる位置を、中心からの距離で二分探索する。
	float nearDistance = 0.0f;
	float farDistance = 600.0f;
	for (int i = 0; i < 40; ++i)
	{
		const float middle = (nearDistance + farDistance) * 0.5f;
		if (distanceToBody(enemyCenter + direction * middle) < m_timelineEditor.EnemyGap())
			nearDistance = middle;
		else
			farDistance = middle;
	}
	srt.pos.x = enemyCenter.x + direction.x * farDistance;
	srt.pos.z = enemyCenter.z + direction.z * farDistance;
	// 敵の方を向く(正面は(-sin, -cos))。
	srt.rot.y = std::atan2(direction.x, direction.z);
	return srt;
}

void GameScene::RecomputeTimelineAnalysis()
{
	const Combat::PlayerAttackSlot slot = m_timelineEditor.GetSlot();
	const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(slot);
	aiAnimation* clip = m_playerAnimator.GetAttackAnimation(slot);
	m_timelineAnalysis = {};
	m_timelineAnalysis.slot = slot;
	m_timelineAnalysis.clipFile = step.clipFile;
	m_timelinePathTips.clear();
	m_timelinePathBases.clear();
	m_timelinePathTimes.clear();
	m_timelineAnalysisSlot = slot;
	m_timelineAnalysisRevision = m_timelineEditor.GetAnalysisRevision();
	const float length = CCharacterAnimator::ClipSeconds(clip);
	if (clip == nullptr || length <= 0.0f)
		return;
	m_timelineAnalysis.clipLoaded = true;
	m_timelineAnalysis.clipLength = length;
	m_timelineAnalysis.hasEnemy = !m_enemies.empty() && !m_combat.IsEnemyDefeated();
	const auto enemyObb = m_timelineAnalysis.hasEnemy
		? GM31::GE::Collision::BuildWorldOBBFromLocalAABB(m_localEnemyMeshBounds, m_enemies.front()->getRenderSRT())
		: GM31::GE::Collision::BoundingBoxOBB{};

	// クリップ全体を、ゲームの2フレーム(再生速度込み)ごとに区切って姿勢を作る。
	// ゲームで実際に剣が通る位置と同じになるよう、プレビューと同じ処理(PoseTimelinePreview)を使う。
	// 1フレームごとにすると、Debugビルドで弱1(1.37秒)に64msかかり、技を切り替えるたびに画面が止まった。
	// 敵に当たるかは前の区切りの剣先からの掃過でも見ているので、2フレームごとでも当たりを見落とさない。
	const float sampleStep = std::max(step.playbackRate / 30.0f, 0.008f);
	const int sampleCount = std::min(600, static_cast<int>(std::ceil(length / sampleStep)) + 1);
	bool previousValid = false;
	Vector3 previousTip{};
	bool inHit = false;
	float hitFrom = 0.0f;
	for (int i = 0; i < sampleCount; ++i)
	{
		const float t = std::min(static_cast<float>(i) * sampleStep, length);
		PoseTimelinePreview(slot, t);
		Vector3 base{};
		Vector3 tip{};
		Vector3 unusedPrevious{};
		if (!m_playerAnimationMesh->GetSwordWorldSweep(base, tip, unusedPrevious))
			continue;
		m_timelinePathTimes.push_back(t);
		m_timelinePathTips.push_back(tip);
		m_timelinePathBases.push_back(base);
		// 剣先の速さ(ゲーム内の単位/ゲームの秒)。クリップの秒を再生速度でゲームの秒へ直す。
		const float speed = previousValid
			? (tip - previousTip).Length() / (sampleStep / step.playbackRate)
			: 0.0f;
		m_timelineAnalysis.sampleTimes.push_back(t);
		m_timelineAnalysis.tipSpeeds.push_back(speed);
		// 敵に当たるか(ゲームと同じ判定。前のフレームの剣先から今の剣先までの掃過も含む)。
		if (m_timelineAnalysis.hasEnemy)
		{
			const auto playerObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
				m_localPlayerMeshBounds, m_player->getRenderSRT());
			const bool hit = OneVsOneCombat::EvaluateSwordCollisionPreview(
				base, tip, previousValid ? previousTip : tip, playerObb, enemyObb).narrowPhaseHit;
			if (hit && !inHit)
			{
				inHit = true;
				hitFrom = t;
			}
			else if (!hit && inHit)
			{
				inHit = false;
				m_timelineAnalysis.enemyHitRanges.emplace_back(hitFrom, t);
			}
		}
		previousTip = tip;
		previousValid = true;
	}
	if (inHit)
		m_timelineAnalysis.enemyHitRanges.emplace_back(hitFrom, length);
}

void GameScene::UpdateTimelineEditor(float deltaSeconds)
{
	m_timelineEditor.Update(deltaSeconds);

	// 技・クリップ・踏み込みなど、剣の通り道が変わる値を触ったら解析をやり直す。
	// 帯をドラッグしている途中は重いので待つ(離したときにやり直す)。
	if ((m_timelineAnalysisRevision != m_timelineEditor.GetAnalysisRevision() ||
		 m_timelineAnalysisSlot != m_timelineEditor.GetSlot()) &&
		!m_timelineEditor.IsDragging())
	{
		RecomputeTimelineAnalysis();
	}

	// 再生位置の姿勢にする。
	PoseTimelinePreview(m_timelineEditor.GetSlot(), m_timelineEditor.GetTime());

	// いまの剣が敵に当たっているか(ゲームと同じ判定)。前のフレームの剣先からの掃過も含む。
	m_timelineBladeHitsEnemy = false;
	Vector3 base{};
	Vector3 tip{};
	Vector3 previousTip{};
	if (!m_enemies.empty() && !m_combat.IsEnemyDefeated() &&
		m_playerAnimationMesh->GetSwordWorldSweep(base, tip, previousTip))
	{
		const auto playerObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localPlayerMeshBounds, m_player->getRenderSRT());
		const auto enemyObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localEnemyMeshBounds, m_enemies.front()->getRenderSRT());
		m_timelineBladeHitsEnemy = OneVsOneCombat::EvaluateSwordCollisionPreview(
			base, tip, previousTip, playerObb, enemyObb).narrowPhaseHit;
	}

	// 再生中は、ゲームと同じく軌跡の区間で剣の軌跡を出す。
	const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(m_timelineEditor.GetSlot());
	const float t = m_timelineEditor.GetTime();
	if (m_timelineEditor.IsPlaying() && t >= step.TrailStart() && t <= step.TrailEnd() &&
		m_playerAnimationMesh->GetSwordWorldSweep(base, tip, previousTip))
		m_playerWeaponTrail.Push(base, tip);
	else if (!m_timelineEditor.IsPlaying())
		m_playerWeaponTrail.Clear();
	// 軌跡と火花の消え方も、プレビューの速さ(スロー)に合わせる。
	const float effectDelta = m_timelineEditor.IsPlaying()
		? deltaSeconds * m_timelineEditor.GetPlaybackSpeed()
		: deltaSeconds;
	m_playerWeaponTrail.Update(effectDelta);
	m_hitEffect.Update(effectDelta);

	// --- カメラ: 右ドラッグで回り込む、ホイールで寄る・引く ---
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			m_timelineCameraYaw -= io.MouseDelta.x * 0.008f;
			m_timelineCameraPitch = std::clamp(m_timelineCameraPitch + io.MouseDelta.y * 0.006f, -0.35f, 1.30f);
		}
		if (io.MouseWheel != 0.0f)
			m_timelineCameraDistance = std::clamp(
				m_timelineCameraDistance * (io.MouseWheel > 0.0f ? 0.88f : 1.14f), 25.0f, 260.0f);
	}
	// 注視点は足元より下にする。画面の下側はタイムラインのウィンドウで隠れるので、
	// キャラクターが画面の上半分に収まるようにするため。
	// 注視点は技を始める位置に固定し、踏み込みで動くキャラクターを追わない
	// (床のマス目に対してどれだけ前へ出たかが見えるように)。
	// コンボを通して再生しているときも、最初の段を始めた位置に固定する(段ごとにカメラが飛ばないように)。
	const Vector3 anchor = TimelinePreviewStartSrt().pos;
	Vector3 lookAt = anchor + Vector3(0.0f, -8.0f, 0.0f);
	const float horizontal = std::cos(m_timelineCameraPitch) * m_timelineCameraDistance;
	Vector3 cameraPosition = lookAt + Vector3(
		std::sinf(m_timelineCameraYaw) * horizontal,
		std::sin(m_timelineCameraPitch) * m_timelineCameraDistance,
		-std::cosf(m_timelineCameraYaw) * horizontal);
	if (m_timelineShakeTime > 0.0f)
	{
		m_timelineShakeTime = std::max(0.0f, m_timelineShakeTime - effectDelta);
		const float fade = m_timelineShakeTime / std::max(m_timelineShakeDuration, 0.001f);
		const float phase = m_timelineShakeTime * 70.0f;
		const Vector3 shake(std::sin(phase * 1.3f), std::sin(phase * 1.7f + 1.0f), std::sin(phase * 1.1f + 2.0f));
		cameraPosition += shake * (m_timelineShakeStrength * fade);
		lookAt += shake * (m_timelineShakeStrength * fade * 0.5f);
	}
	m_camera.SetCinematicView(cameraPosition, lookAt);
}

void GameScene::DrawTimelineEditor3D()
{
	if (!m_timelineEditor.IsOpen())
		return;
	const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(m_timelineEditor.GetSlot());
	const float t = m_timelineEditor.GetTime();

	// 体や敵に隠れても見えるよう、深度を見ずに上から描く。
	Renderer::SetDepthEnable(false);
	// 剣先の通り道。予兆(青)・判定(赤)・硬直(灰)で色を分け、使わない範囲は薄くする。
	// エディターのタイムラインの帯と同じ色にそろえている。
	if (m_timelineEditor.ShowSwordPath() && m_timelinePathTips.size() >= 2 &&
		m_timelineAnalysisSlot == m_timelineEditor.GetSlot())
	{
		const auto colorAt = [&step](float time) {
			if (time < step.clipStart || time > step.clipEnd) return Color(0.6f, 0.6f, 0.65f, 0.25f);
			if (time < step.hitStart) return Color(0.30f, 0.55f, 1.0f, 0.9f);
			if (time <= step.hitEnd) return Color(1.0f, 0.25f, 0.18f, 1.0f);
			return Color(0.55f, 0.57f, 0.62f, 0.8f);
		};
		SetLineWidth(0.0035f);
		for (std::size_t i = 1; i < m_timelinePathTips.size(); ++i)
		{
			const Vector3 direction = m_timelinePathTips[i] - m_timelinePathTips[i - 1];
			const float length = direction.Length();
			if (length > 0.0001f)
				LineDrawerDraw(length, m_timelinePathTips[i - 1], direction, colorAt(m_timelinePathTimes[i]));
		}
		// 軌跡を判定と別の区間にしているときは、軌跡が出る所を金色の扇で見せる
		// (軌跡は刃の35%の位置から剣先まで張られる。WeaponTrail と同じ)。
		SetLineWidth(0.0018f);
		if (step.trailStart >= 0.0f || step.trailEnd >= 0.0f)
		{
			for (std::size_t i = 0; i < m_timelinePathTips.size(); i += 2)
			{
				const float time = m_timelinePathTimes[i];
				if (time < step.TrailStart() || time > step.TrailEnd())
					continue;
				const Vector3 ribbonBase = m_timelinePathBases[i] + (m_timelinePathTips[i] - m_timelinePathBases[i]) * 0.35f;
				const Vector3 direction = m_timelinePathTips[i] - ribbonBase;
				const float length = direction.Length();
				if (length > 0.0001f)
					LineDrawerDraw(length, ribbonBase, direction, Color(1.0f, 0.80f, 0.30f, 0.45f));
			}
		}
		// 判定の区間は、刃が通る面(根元から剣先)を扇のように描き、当たる範囲の広さを見せる。
		for (std::size_t i = 0; i < m_timelinePathTips.size(); i += 2)
		{
			const float time = m_timelinePathTimes[i];
			if (time < step.hitStart || time > step.hitEnd)
				continue;
			const Vector3 direction = m_timelinePathTips[i] - m_timelinePathBases[i];
			const float length = direction.Length();
			if (length > 0.0001f)
				LineDrawerDraw(length, m_timelinePathBases[i], direction, Color(1.0f, 0.35f, 0.25f, 0.45f));
		}

		// 時刻にいちばん近い区切りの姿勢(剣の根元・剣先)を引く。
		const auto nearestSample = [this](float time) {
			std::size_t best = 0;
			for (std::size_t i = 1; i < m_timelinePathTimes.size(); ++i)
				if (std::fabs(m_timelinePathTimes[i] - time) < std::fabs(m_timelinePathTimes[best] - time))
					best = i;
			return best;
		};
		// 判定の始まりと終わりの刃を白く太く描く。帯の端をドラッグすると、刃がどこで当たり始めて
		// どこで当たり終わるかが3Dで分かる。
		SetLineWidth(0.0045f);
		for (const float boundary : { step.hitStart, step.hitEnd })
		{
			const std::size_t i = nearestSample(boundary);
			const Vector3 direction = m_timelinePathTips[i] - m_timelinePathBases[i];
			const float length = direction.Length();
			if (length > 0.0001f)
				LineDrawerDraw(length, m_timelinePathBases[i], direction, Color(1.0f, 1.0f, 1.0f, 0.95f));
		}
		// イベントを置いた瞬間の剣先に、イベントの色で小さな十字を出す(火花がどこから出るかなど)。
		SetLineWidth(0.004f);
		for (const Combat::TimelineEvent& event : step.events)
		{
			const Vector3 point = m_timelinePathTips[nearestSample(event.time)];
			const Color color = event.type == Combat::TimelineEventType::Sound ? Color(0.75f, 0.5f, 1.0f, 1.0f)
				: event.type == Combat::TimelineEventType::CameraShake ? Color(1.0f, 0.6f, 0.25f, 1.0f)
				: Color(1.0f, 0.9f, 0.3f, 1.0f);
			const float size = 1.6f;
			LineDrawerDraw(size * 2.0f, point - Vector3(size, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), color);
			LineDrawerDraw(size * 2.0f, point - Vector3(0.0f, size, 0.0f), Vector3(0.0f, 1.0f, 0.0f), color);
			LineDrawerDraw(size * 2.0f, point - Vector3(0.0f, 0.0f, size), Vector3(0.0f, 0.0f, 1.0f), color);
		}
	}

	// いまの剣の箱。判定の区間の中だけ出す(ゲームで判定が出ているときと同じ)。
	// 敵に当たっていれば緑、当たっていなければ赤。区間の外は、選んだときだけ灰色で出す。
	Vector3 base{};
	Vector3 tip{};
	Vector3 previousTip{};
	const bool inHitWindow = t >= step.hitStart && t <= step.hitEnd;
	if ((inHitWindow || m_timelineEditor.ShowHitboxOutsideWindow()) &&
		m_playerAnimationMesh->GetSwordWorldSweep(base, tip, previousTip))
	{
		const auto playerObb = GM31::GE::Collision::BuildWorldOBBFromLocalAABB(
			m_localPlayerMeshBounds, m_player->getRenderSRT());
		const auto preview = OneVsOneCombat::EvaluateSwordCollisionPreview(
			base, tip, previousTip, playerObb, playerObb);
		const Color color = !inHitWindow ? Color(0.6f, 0.6f, 0.65f, 0.8f)
			: m_timelineBladeHitsEnemy ? Color(0.25f, 1.0f, 0.35f, 1.0f)
			: Color(1.0f, 0.25f, 0.15f, 1.0f);
		DrawBoxEdges(GetObbCorners(preview.bladeObb), color);
	}
	Renderer::SetDepthEnable(true);
}

void GameScene::AimPlayerAttack()
{
	if (!m_player)
		return;
	const bool lockedOn = m_lockOnTarget && !m_enemies.empty() && !m_combat.IsEnemyDefeated();
	const Vector3 targetPosition = lockedOn ? m_enemies.front()->getSRT().pos : Vector3(0.0f, 0.0f, 0.0f);
	m_player->aimAttack(m_camera.GetYaw(), lockedOn, targetPosition);
}

void GameScene::UpdateFootLock(float deltaSeconds)
{
	if (!m_playerAnimationMesh || !m_player)
		return;
	if (!m_legChainsResolved)
	{
		// 足の固定は**既定で切ってある**(dev_settings.ini の foot_lock=1 で入る)。
		//
		// 理由(実測): 固定が掛かると、IKがつま先を固定位置へ寄せようとして
		// **足首が1フレームで0.17モデル単位動く**。前後のフレームは0.003〜0.02しか動いていないので、
		// 毎歩そこで足が飛んだように見えていた(ユーザー指摘「足がめっちゃとびとびに見える」)。
		// 目標が脚の届く範囲の外にあるため、IKが解ききれずに足の高さまで変わってしまう。
		// 切ると、つま先はモーションのとおりに動く(実測で補正量0)。
		// 足が多少滑るのと引き換えだが、FootLock.hに引いた記事のとおり
		// 「少し滑るほうが、モーションを壊すよりまし」と判断した。
		// 元のクリップの歩幅に合う移動速度にできたら、入れ直して比べること。
		m_footLockEnabled = GetDevSetting("foot_lock", "0") != "0";
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
	// 尾回転は体ごと回る攻撃で、頭を振り下ろす攻撃クリップとは形が合わない。
	// 溜めと隙は立ち姿勢のまま(尾と首だけを構えで動かす)、回っている間は
	// 歩きのクリップで脚を動かして「その場で回っている」ように見せる。
	const bool inAttack =
		state == enemy::MotionState::Windup ||
		state == enemy::MotionState::Active ||
		state == enemy::MotionState::Recovery;
	const Combat::EnemyAttackKind attackKind = m_enemies.front()->getAttackKind();
	const bool tailSpinAttack = inAttack && attackKind == Combat::EnemyAttackKind::TailSpin;
	// 攻撃クリップを使わない攻撃(尾回転・火球・突進)。立ち姿勢か歩きに、首や尾の構えを重ねて見せる。
	// 火球は上を向いて吐き、突進は走るので、頭を地面へ振り下ろす攻撃クリップとは形が合わない。
	const bool nonClipAttack = inAttack && !Combat::EnemyAttackUsesAttackClip(attackKind);
	const bool clipDrivenAttack = m_enemyAttackAnimation != nullptr && !nonClipAttack &&
		(state == enemy::MotionState::Windup ||
		 state == enemy::MotionState::Active ||
		 state == enemy::MotionState::Recovery);
	constexpr float recoveryPoseHoldSeconds = 0.4f;
	// 尾回転はここに入れない。攻撃クリップ(頭の振り下ろし)の最後の姿勢で止まってしまい、
	// 回り終わった直後だけ別の攻撃の形になる。
	const bool holdRecoveryPose = !clipDrivenAttack && !nonClipAttack &&
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
				m_previousEnemyMotionState == enemy::MotionState::Retreat ||
				m_previousEnemyMotionState == enemy::MotionState::Turn
					? m_enemyWalkAnimation
					// 怯み・練習台などの後は立ち姿勢(待機の最初のコマ)から入る。
					: (m_enemyIdleAnimation != nullptr ? m_enemyIdleAnimation : m_enemyWalkAnimation);
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
	int framesPerKey = std::clamp(
		static_cast<int>(std::lround(10.0f / std::max(0.1f, m_enemies.front()->getMoveSpeedScale()))),
		10, 24);
	// 回転中は足を踏み替えて回るので、歩きより速くキーを送る。
	// 歩きの速さのままだと、脚が止まったまま体だけが回って滑って見える。
	if (m_enemies.front()->isTailSpinning())
		framesPerKey = 4;
	// 突進は走るので、歩きのクリップを歩きの3倍以上の速さで回す(走る速さ185は歩き84の約2.2倍。
	// 歩幅も広がって見えるよう、脚の回転はそれより速くしている)。滑って止まる間は半分の速さへ落とす。
	if (state == enemy::MotionState::Turn)
		framesPerKey = std::max(framesPerKey, 13);
	if (m_enemies.front()->isChargeRunning())
		framesPerKey = 3;
	else if (m_enemies.front()->isChargeSkidding())
		framesPerKey = 6;
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

	// 立ち姿勢。待機のモーション(dragon_idle.dae)の**最初のコマで止めて**使う(カットシーン・練習台・怯みなど)。
	// 待機のモーションは途中で上体を大きく起こし、後ろ足が浮いて、斜めの光で影が足元から離れて長く伸びていた
	// (カットシーンの影がおかしく見えていた原因)。最初のコマは4本の足がすべて地面に着いた構えで、
	// 攻撃・倒れるモーションの最初のコマとも同じ姿勢(調査プログラムprobe13で足の高さを比較)。
	// 歩きのモーションで止めると、どのコマでもどれか1本の足が上がっている(実機で後ろ足が浮いた)。
	aiAnimation* animation = m_enemyIdleAnimation != nullptr ? m_enemyIdleAnimation : m_enemyWalkAnimation;
	bool holdStandingPose = true;
	if (!m_enemyIntroActive)
	{
		switch (state)
		{
		case enemy::MotionState::Approach:
		case enemy::MotionState::Circle:
		case enemy::MotionState::Retreat:
		// その場で向き直る間も脚を踏み替える(遅めに回す)。止めたまま体だけ回ると滑って見える。
		case enemy::MotionState::Turn:
			// 練習台(動かない敵)は歩かないので立ち姿勢のまま。歩きを再生すると足踏みして見える。
			holdStandingPose = m_enemies.front()->isPassive();
			if (!holdStandingPose)
				animation = m_enemyWalkAnimation;
			break;
		case enemy::MotionState::Windup:
			if (nonClipAttack)
				break; // 溜めは立ち姿勢のまま。尾を寄せる・首を上げる・地面を掻く動きは構え(EnemyTellPose)で見せる。
			holdStandingPose = false;
			animation = m_enemyAttackAnimation;
			break;
		case enemy::MotionState::Active:
			if (tailSpinAttack || m_enemies.front()->isChargeRunning())
			{
				// 回っている間・走っている間だけ脚を動かす。専用のモーションが無いので歩きを使う。
				holdStandingPose = false;
				animation = m_enemyWalkAnimation;
				break;
			}
			if (nonClipAttack)
				break; // 火球は立ったまま首を突き出して吐く。
			holdStandingPose = false;
			animation = m_enemyAttackAnimation;
			break;
		case enemy::MotionState::Recovery:
			// 突進の後、滑って止まる間は脚を動かし続ける(歩きを遅く回す)。
			if (m_enemies.front()->isChargeSkidding())
			{
				holdStandingPose = false;
				animation = m_enemyWalkAnimation;
			}
			// 隙の間も攻撃クリップの続き(頭を戻す動き)をそのまま見せる。
			else if (clipDrivenAttack)
			{
				holdStandingPose = false;
				animation = m_enemyAttackAnimation;
			}
			else if (holdRecoveryPose && m_enemyAttackAnimation != nullptr)
			{
				holdStandingPose = false;
				animation = m_enemyAttackAnimation;
				m_enemyAnimationFrame = recoveryPoseFrame;
			}
			break;
		default:
			// 怯みなど。立ち姿勢に体の傾き(EnemyFlinchPose)を重ねて見せる。
			break;
		}
	}
	if (holdStandingPose)
	{
		m_enemyAnimationFrame = 0;
		m_enemyAnimationTick = 0.0f;
	}

	bool blendedIntroThisUpdate = false;
	if (animation != nullptr)
	{
		m_enemyAnimationMesh->SetCurentAnimation(animation);
		// キー間の小数部。経過時間で進めた値をそのまま使う。
		const float fraction = holdRecoveryPose ? 0.0f :
			m_enemyAnimationTick;
		// カットシーンの立ち姿勢(待機の最初のコマ)から、戦闘の歩きの最初のコマへ短くつなぐ。
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
	// 攻撃ごとのシルエット。クリップの姿勢の上から首・あご・尾・前脚を回し、
	// 3種類の攻撃を遠目にも見分けられるようにする(接地の計算より前に済ませる)。
	ApplyEnemyTellPose(m_enemies.front()->getAttackTellPose());
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
	UpdateEnemyMouthPosition();
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
		// 立ち姿勢も最初の姿勢で止めておく。
		if (clipDrivenAttack || holdStandingPose)
			return;
		// 尾回転の一拍(半回転と半回転の間)では脚も止める。
		// 体が回っていないのに歩きのキーだけ進むと、その場で足踏みして見える。
		if (!m_enemies.empty() && m_enemies.front()->isTailSpinPausing())
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
	// 床はマス目付き(ArenaFloor)。大きさと高さは初期化時に決めている。
	m_arenaFloor.Draw();

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
	// 色は金色寄りにする。以前の白に近い色(1.0, 0.94, 0.72)は、床を明るい灰色のマス目にしてから
	// 床に溶けてほとんど見えなくなった(2026-10-26に画面で確認)。
	m_playerWeaponTrail.Draw(Color(1.0f, 0.78f, 0.30f, 1.0f));
	// 命中の火花と閃光。カメラへ正対させるためビュー行列を渡す。
	// 色は火花側が寿命に応じて白熱→橙→赤と変えるので、ここでは指定しない。
	m_hitEffect.Draw(m_camera.GetViewMatrix());
	// 敵の攻撃の通り道。剣の軌跡(金色)と見分けられ、明るい灰色の床にも溶けない赤寄りの色にする
	// (白っぽい色は、剣の軌跡で床に溶けて見えなかったことがある)。
	const Color enemyTrailColor(0.95f, 0.30f, 0.18f, 0.65f);
	m_enemyTailTrail.Draw(enemyTrailColor);
	m_enemyHeadTrail.Draw(enemyTrailColor);
	// 土煙・地面の衝撃の輪・咆哮の衝撃波。半透明なので、加算合成の炎より先に描く。
	m_enemyMotionFx.Draw(m_camera.GetViewMatrix());
	// 敵の火球と、吐く前に口に溜まる火。火花と同じく加算合成で、キャラクターの後に重ねる。
	m_enemyFireball.Draw(m_camera.GetViewMatrix());
	// タイムラインエディターを開いている間は、エディターの3D表示(剣先の通り道・剣の箱)だけを出す。
	DrawTimelineEditor3D();
	if (m_drawAttackCollisionDebug && !m_enemyIntroActive && !m_timelineEditor.IsOpen())
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

		// 剣の箱と攻撃の粗い判定は、攻撃判定が出ている間だけ描く。
		// 以前は剣の位置が取れていれば常に描いていたので、歩いているだけでも判定が出ているように見えた
		// (ユーザー指摘 2026-10-26「剣の当たり判定とか常に出てる」)。実際の判定は前から判定中だけだった。
		if (collision.swordTransformValid && m_combat.IsPlayerAttackActive())
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
	if (m_timelineEditor.IsOpen())
	{
		// エディターの間は戦闘のHUD(体力・操作説明)を出さない。タイムラインのウィンドウだけにする。
		m_timelineEditor.Draw(m_timelineAnalysis, m_timelineBladeHitsEnemy);
		return;
	}
	DrawGameplayHud();
	DrawEnemyIntroOverlay();
	CaptureFrameIfRequested();
}

void GameScene::WriteCaptureLog()
{
	if (m_captureDir.empty() || m_enemies.empty() || !m_player ||
		m_capturedFrames >= m_captureCount)
		return;
	if (!m_captureLog)
	{
		_wfopen_s(&m_captureLog, (m_captureDir + L"\\enemy_log.csv").c_str(), L"w");
		if (!m_captureLog)
			return;
		fprintf(m_captureLog, "time,state,attack,enemy_x,enemy_z,enemy_yaw_deg,facing_error_deg,distance,player_x,player_z,player_hp,foot_fl,foot_fr,foot_bl,foot_br,fx_puffs,fx_rings,wall_push\n");
	}
	const enemy& e = *m_enemies.front();
	const Vector3 enemyPos = e.getSRT().pos;
	const Vector3 playerPos = m_player->getSRT().pos;
	const float dx = playerPos.x - enemyPos.x;
	const float dz = playerPos.z - enemyPos.z;
	float facingError = std::atan2(-dx, -dz) - e.getSRT().rot.y;
	while (facingError > PI) facingError -= PI * 2.0f;
	while (facingError < -PI) facingError += PI * 2.0f;
	constexpr float TO_DEG = 180.0f / PI;
	// 状態名は日本語なので、表計算で読めるよう番号で書く(enemy::MotionState の並び)。
	// 足の骨の床からの高さ(踏んだ瞬間の土煙を合わせるのに使う)。
	float footHeight[4] = {};
	for (int i = 0; i < 4; ++i)
	{
		Vector3 foot{};
		if (GetEnemyBoneWorld(ENEMY_FOOT_BONES[i], foot))
			footHeight[i] = foot.y - ARENA_FLOOR_TOP_Y;
	}
	fprintf(m_captureLog, "%.2f,%d,%d,%.1f,%.1f,%.0f,%.0f,%.1f,%.1f,%.1f,%.0f,%.2f,%.2f,%.2f,%.2f,%zu,%zu,%.2f\n",
		m_battleSeconds,
		static_cast<int>(e.getMotionState()),
		static_cast<int>(e.getAttackKind()),
		enemyPos.x, enemyPos.z,
		e.getSRT().rot.y * TO_DEG,
		std::abs(facingError) * TO_DEG,
		std::sqrt(dx * dx + dz * dz),
		playerPos.x, playerPos.z,
		m_combat.GetPlayerHp(),
		footHeight[0], footHeight[1], footHeight[2], footHeight[3],
		m_enemyMotionFx.GetPuffCount(), m_enemyMotionFx.GetRingCount(),
		m_enemyWallPush.Length());
	fflush(m_captureLog);
}

void GameScene::CaptureFrameIfRequested()
{
	// 撮るのは、登場演出の間(capture_intro=1)か、戦闘の間のどちらか一方だけ。
	if (m_captureDir.empty() || m_capturedFrames >= m_captureCount ||
		m_enemyIntroActive != m_captureIntro)
		return;
	const float clock = m_captureIntro ? m_enemyIntroTime : m_battleSeconds;
	const float nextTime =
		m_captureStartSeconds + m_captureIntervalSeconds * static_cast<float>(m_capturedFrames);
	if (clock < nextTime)
		return;
	// ファイル名に戦闘開始(登場演出を撮るときは演出の始まり)からの秒を入れて、どの瞬間の絵か分かるようにする。
	wchar_t name[64];
	swprintf_s(name, L"\\f%03d_%06.2f.png", m_capturedFrames, clock);
	Renderer::SaveBackBufferPng((m_captureDir + name).c_str());
	++m_capturedFrames;
	if (m_captureExit && m_capturedFrames >= m_captureCount)
		PostQuitMessage(0);
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

	if (m_showControlGuide)
		DrawControlGuide();

	// タイムラインエディターを保存せずに閉じたときの注意(画面の上の中央、数秒で消える)。
	if (m_timelineUnsavedNoticeTime > 0.0f)
	{
		m_timelineUnsavedNoticeTime = std::max(0.0f, m_timelineUnsavedNoticeTime - ImGui::GetIO().DeltaTime);
		const float alpha = std::clamp(m_timelineUnsavedNoticeTime, 0.0f, 1.0f);
		const char* text = "攻撃のタイムラインに保存していない変更があります(F6で開いて Ctrl+S)";
		const ImVec2 size = ImGui::CalcTextSize(text);
		const ImVec2 position(viewport->Pos.x + (viewport->WorkSize.x - size.x) * 0.5f, viewport->Pos.y + 70.0f);
		drawList->AddRectFilled(ImVec2(position.x - 12.0f, position.y - 7.0f),
			ImVec2(position.x + size.x + 12.0f, position.y + size.y + 7.0f),
			IM_COL32(60, 20, 15, static_cast<int>(210.0f * alpha)), 6.0f);
		drawList->AddText(position, IM_COL32(255, 220, 200, static_cast<int>(255.0f * alpha)), text);
	}

	if (!m_lockOnTarget || m_enemyIntroActive || m_enemies.empty())
		return;

	// ロックオンの表示は、敵に出る照準だけにする。
	// 以前は枠と照準の色が敵の状態(予兆=赤・隙=緑)で変わり、攻撃名や「CHANCE」も文字で出していた。
	// それでは敵の体ではなくUIを見て戦うことになり、「予兆を体で読む」というこの作品の核が崩れる
	// (ユーザー判断 2026-09-20)。色は一定にし、状態を伝える文字も出さない。
	// さらに画面右上の「TARGET LOCK / ANCIENT DRAGON / TAB」の枠も外した
	// (ユーザー判断 2026-09-25「見えにくい」)。ロックしていることは敵に出る照準で分かる。
	const ImU32 lockColor = IM_COL32(244, 194, 72, 255);

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

void GameScene::DrawControlGuide()
{
	// 操作説明。遊ぶ人(作品を見る人)が操作を探して時間を使わないよう、戦闘中は常に画面の右下に出す。
	// ゲームパッドがつながっていればパッドの表記、なければキーボード・マウスの表記にする(GameInput::GetDisplayDevice)。
	const GameInput& gameInput = GameInput::Get();
	struct Row { const char* action; const char* button; };
	const Row rows[] = {
		{ "移動", gameInput.GetMoveLabel() },
		{ "視点", gameInput.GetLookLabel() },
		{ "弱攻撃", gameInput.GetButtonLabel(GameAction::Attack) },
		{ "強攻撃", gameInput.GetButtonLabel(GameAction::HeavyAttack) },
		{ "回避", gameInput.GetButtonLabel(GameAction::Dodge) },
		{ "ダッシュ", gameInput.GetButtonLabel(GameAction::Sprint) },
		{ "ロックオン", gameInput.GetButtonLabel(GameAction::LockOn) },
	};

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	const float lineHeight = ImGui::GetTextLineHeight() + 4.0f;
	const float padding = 10.0f;
	float actionWidth = 0.0f;
	float buttonWidth = 0.0f;
	for (const Row& row : rows)
	{
		actionWidth = std::max(actionWidth, ImGui::CalcTextSize(row.action).x);
		buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(row.button).x);
	}
	const float columnGap = 16.0f;
	const float boxWidth = padding * 2.0f + actionWidth + columnGap + buttonWidth;
	const float boxHeight = padding * 2.0f + lineHeight * static_cast<float>(std::size(rows));
	const ImVec2 boxMax(
		viewport->Pos.x + viewport->WorkSize.x - 24.0f,
		viewport->Pos.y + viewport->WorkSize.y - 24.0f);
	const ImVec2 boxMin(boxMax.x - boxWidth, boxMax.y - boxHeight);

	// 背景は半透明にして、敵の予兆(体の動き)を隠さない。明るい床の上でも文字が読める濃さにする。
	drawList->AddRectFilled(boxMin, boxMax, IM_COL32(8, 10, 14, 190), 6.0f);
	float y = boxMin.y + padding;
	for (const Row& row : rows)
	{
		drawList->AddText(ImVec2(boxMin.x + padding, y), IM_COL32(230, 233, 240, 255), row.action);
		drawList->AddText(
			ImVec2(boxMin.x + padding + actionWidth + columnGap, y),
			IM_COL32(255, 226, 150, 255), row.button);
		y += lineHeight;
	}
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
	// 撮影・調整用: dev_settings.ini の enemy_invincible=1 / player_invincible=1 で最初から無敵にする。
	// 当たって演出は出るが、体力だけ減らない。デバッグ表示の「戦闘」タブでも切り替えられる。
	m_combat.SetEnemyDebugInvincible(GetDevSetting("enemy_invincible", "0") == "1");
	m_combat.SetPlayerDebugInvincible(GetDevSetting("player_invincible", "0") == "1");
	// カメラ(3D)の初期匁E
	// 剣の軌跡。刃のワールド座標だけを渡す作りなので、
	// 武器やキャラクターのモデルが変わっても初期化はこのままでよい。
	m_playerWeaponTrail.Initialize();
	m_hitEffect.Initialize();
	m_enemyFireball.Initialize();
	m_enemyMotionFx.Initialize();
	// 尾は回転で大きく弧を描くので、剣より長く残して通り道を読ませる。
	m_enemyTailTrail.Initialize(32, 0.24f);
	m_enemyHeadTrail.Initialize(24, 0.22f);
	// 床。以前の床(厚さ0.6の箱を-0.59に置いたもの)と上面の高さをそろえる(-0.29)。
	// 色は試遊での指摘(距離感が分かりにくい)を受けて、まずはトレーニングステージ風にする。
	// デバッグ表示の「撮影・調整用」から石の色合いへ切り替えられる。
	m_arenaFloor.Initialize(ARENA_RADIUS * 2.0f, -0.29f, ArenaFloor::TrainingPalette());

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
	// 撮影・調整用の連続保存(dev_settings.ini の capture_dir / capture_start / capture_interval /
	// capture_count / capture_exit)。詳しくは GameScene.h の CaptureFrameIfRequested を参照。
	{
		const std::string dir = GetDevSetting("capture_dir", "");
		if (!dir.empty())
		{
			m_captureDir.assign(dir.begin(), dir.end()); // 英数字のパスだけを想定している
			m_captureStartSeconds = std::stof(GetDevSetting("capture_start", "0"));
			m_captureIntervalSeconds = std::stof(GetDevSetting("capture_interval", "0.1"));
			m_captureCount = std::stoi(GetDevSetting("capture_count", "30"));
			m_captureExit = GetDevSetting("capture_exit", "0") == "1";
			m_captureIntro = GetDevSetting("capture_intro", "0") == "1";
		}
	}
	// 敵の攻撃を1種類に固定する(dev_settings.iniの force_enemy_attack=slam|bite|sweep|spin|fireball|charge)。
	// 3種類の構えを見比べるとき、通常の選択では狙った攻撃が出るまで待つことになるため。
	// 未指定なら通常どおり敵AIが選ぶ。デバッグ表示のチェックボックスからも切り替えられる。
	{
		const std::string forced = GetDevSetting("force_enemy_attack", "");
		if (forced == "slam" || forced == "bite" || forced == "sweep" || forced == "spin" ||
			forced == "fireball" || forced == "charge")
		{
			m_forceEnemyAttack = true;
			m_forcedEnemyAttackIndex =
				forced == "bite" ? 1 : forced == "sweep" ? 2 : forced == "spin" ? 3 :
				forced == "fireball" ? 4 : forced == "charge" ? 5 : 0;
		}
	}
	// 技(コンボの各段・ダッシュ攻撃)のクリップと時間は、攻撃のタイムラインのファイルから読む
	// (assets/data/attack_timeline.txt。ゲーム内のタイムラインエディター(F6)で編集・保存する)。
	// ファイルが無ければ Combat::DefaultPlayerAttack() の既定値を使う。
	// 以前は dev_settings.ini の weak1〜3・heavy1〜3・dash_attack でクリップを差し替えていたが、
	// クリップを替えると判定の位置も見直す必要があり、iniでは時間を直せなかった。
	// クリップの選択はエディターへ移した(クリップを替えて、その場で判定を合わせ直せる)。
	// 読むのは起動して最初の1回だけ。リトライでこのシーンを作り直すたびに読み直すと、
	// エディターで調整して保存していない値が、ファイルの値で上書きされて消えてしまう。
	static bool attackTimelineLoaded = false;
	if (!attackTimelineLoaded)
	{
		AttackTimelineFile::Load();
		attackTimelineLoaded = true;
	}
	m_playerWalkAnimation = m_playerAnimationData.GetAnimation("walk", 0);
	aiAnimation* playerRunAnimation = m_playerAnimationData.GetAnimation("run", 0);
	std::array<aiAnimation*, 3> weakAttackAnimations{};
	std::array<aiAnimation*, 3> heavyAttackAnimations{};
	for (int index = 0; index < 3; ++index)
	{
		const std::string weakName = "weak_attack_" + std::to_string(index + 1);
		const std::string heavyName = "heavy_attack_" + std::to_string(index + 1);
		m_playerAnimationData.LoadAnimation(Combat::PlayerComboStepOf(false, index + 1).clipFile, weakName);
		m_playerAnimationData.LoadAnimation(Combat::PlayerComboStepOf(true, index + 1).clipFile, heavyName);
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
	m_playerAnimationData.LoadAnimation(Combat::PlayerDashAttackStep().clipFile, "dash_attack");
	m_playerAnimator.SetDashAttackAnimation(m_playerAnimationData.GetAnimation("dash_attack", 0));

	// 攻撃のタイムラインエディター(F6)。クリップの読み込みとイベントの再生はこのシーンが受け持つ。
	m_timelineEditor.Initialize({
		[this](Combat::PlayerAttackSlot slot) { return ReloadAttackClip(slot); },
		[this](const Combat::TimelineEvent& event) { FireTimelineEvent(event); },
	});
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
	// 最初は立ち姿勢(待機の最初のコマ。4本の足がすべて地面に着いた構え)。
	if (m_enemyWalkAnimation != nullptr || m_enemyIdleAnimation != nullptr)
	{
		m_enemyAnimationMesh->SetCurentAnimation(
			m_enemyIdleAnimation != nullptr ? m_enemyIdleAnimation : m_enemyWalkAnimation);
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
	if (m_captureLog)
	{
		fclose(m_captureLog);
		m_captureLog = nullptr;
	}
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

void GameScene::ApplyEnemyTellPose(const Combat::EnemyTellPose& tell)
{
	if (!m_enemyAnimationMesh)
		return;
	// 構えが無いときも必ず呼ぶ。前のフレームで重ねた回転を取り消すのは、この呼び出しの中だからである。
	// 早く抜けてしまうと、再生中のクリップにキーが無い骨(待機クリップには首の2番目のキーが無い)に
	// 回転が残り続け、次に重ねたときに積み上がってしまう。
	if (m_enemyAnimationMesh->AddBoneLocalRotations(BuildEnemyTellRotations(tell)))
		m_enemyAnimationMesh->RefreshBoneMatrices(m_enemyBoneComb);
}

void GameScene::UpdateEnemyMouthPosition()
{
	m_enemyMouthValid = false;
	if (!m_enemyAnimationMesh || m_enemies.empty() || !m_enemies.front())
		return;
	// 上あごの骨は口の付け根(あごの蝶番)にあり、鼻先はそれより前にある。
	// 首の先(neck3)から上あごへの向きを頭の向きとみなし、その先へ延ばした点を口先とする。
	// 骨から求めるので、首を持ち上げる構え(EnemyTellPose)を重ねた後の位置になる。
	Vector3 jaw{};
	if (!m_enemyAnimationMesh->GetBoneModelPosition("Armature_jaw_upper", jaw))
		return;
	Vector3 mouthModel = jaw;
	Vector3 neck{};
	if (m_enemyAnimationMesh->GetBoneModelPosition("Armature_neck3", neck))
		mouthModel = jaw + (jaw - neck) * ENEMY_MOUTH_EXTEND;
	const Matrix4x4 world = m_enemies.front()->getRenderSRT().GetMatrix();
	m_enemyMouthPosition = Vector3::Transform(mouthModel, world);
	m_enemyMouthValid = true;
}

void GameScene::LaunchEnemyFireball(const enemy& source)
{
	if (!m_player)
		return;
	const SRT srt = source.getSRT();
	const float yaw = srt.rot.y;
	const Vector3 forward(-std::sinf(yaw), 0.0f, -std::cosf(yaw));
	// 口の位置が取れないとき(モデルが読めていない)は、体の前の高いところから吐く。
	const Vector3 mouth = m_enemyMouthValid
		? m_enemyMouthPosition
		: srt.pos + forward * 28.0f + Vector3(0.0f, 30.0f, 0.0f);

	// 狙いは吐いた瞬間のプレイヤーの足元。飛んでいる間は追いかけない。
	// ただし敵の正面から曲げられる角度には限りがある(首より外へ曲がると口と関係ない向きへ飛んで見える)。
	// 予兆の終わり際で向き直りを止めているので、そこから横へ動いた分だけ狙いが逸れる。
	Vector3 flat = m_player->getSRT().pos - mouth;
	flat.y = 0.0f;
	const float distance = std::max(flat.Length(), 10.0f);
	float aimYaw = std::atan2(-flat.x, -flat.z); // enemy::angleToTarget と同じ向きの取り方
	float diff = aimYaw - yaw;
	while (diff > PI) diff -= PI * 2.0f;
	while (diff < -PI) diff += PI * 2.0f;
	diff = std::clamp(
		diff,
		-Combat::Tuning::ENEMY_FIREBALL_MAX_AIM_ANGLE,
		Combat::Tuning::ENEMY_FIREBALL_MAX_AIM_ANGLE);
	aimYaw = yaw + diff;
	const Vector3 target(
		mouth.x - std::sinf(aimYaw) * distance,
		ARENA_FLOOR_TOP_Y,
		mouth.z - std::cosf(aimYaw) * distance);
	m_enemyFireball.Launch(mouth, target);
}

void GameScene::UpdateEnemyFireballs(float deltaSeconds, bool playerInvincible)
{
	if (!m_player)
		return;
	// 口に溜まる火。予兆の進み具合で大きくなり、吐いたら消える。
	float charge = 0.0f;
	if (!m_enemies.empty() && m_enemies.front() && m_enemyMouthValid)
	{
		const enemy& source = *m_enemies.front();
		if (source.getMotionState() == enemy::MotionState::Windup &&
			source.getAttackKind() == Combat::EnemyAttackKind::Fireball)
		{
			charge = source.getStateTime() / Combat::Tuning::ENEMY_FIREBALL_ANTICIPATION;
		}
	}
	m_enemyFireball.SetMouthCharge(m_enemyMouthPosition, charge);

	std::vector<EnemyFireball::Impact> impacts;
	const Vector3 playerFeet = m_player->getSRT().pos;
	m_enemyFireball.Update(
		deltaSeconds,
		playerFeet + Vector3(0.0f, PLAYER_BODY_CENTER_HEIGHT, 0.0f),
		PLAYER_BODY_RADIUS,
		playerInvincible,
		ARENA_FLOOR_TOP_Y,
		impacts);

	for (const EnemyFireball::Impact& impact : impacts)
	{
		// 爆発の火花。上へ向けて大きく散らす。
		m_hitEffect.Spawn(impact.position + Vector3(0.0f, 2.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), 24, 1.7f);
		// 地面で爆発したときは、爆風で土煙が輪になって広がる。
		if (impact.position.y < ARENA_FLOOR_TOP_Y + 6.0f)
		{
			const Vector3 ground(impact.position.x, ARENA_FLOOR_TOP_Y, impact.position.z);
			m_enemyMotionFx.SpawnGroundRing(ground, 26.0f, 0.40f, 0.40f);
			m_enemyMotionFx.SpawnDust(ground, Vector3(0.0f, 0.0f, 0.0f), 10, 38.0f, 9.0f, 1.0f, 0.35f);
		}
		// 近くで爆発したほど強く揺らす。外れても「すぐそばに落ちた」ことが伝わるように。
		const float closeness =
			std::clamp(1.0f - (impact.position - playerFeet).Length() / 120.0f, 0.0f, 1.0f);
		if (closeness > 0.0f)
			m_camera.TriggerShake(0.6f + 1.4f * closeness, 0.25f);

		if (!impact.hitPlayer ||
			!m_combat.ApplyEnemyProjectileHit(static_cast<float>(Combat::Tuning::ENEMY_FIREBALL_DAMAGE)))
		{
			continue;
		}
		// 被弾の手応えは近接攻撃と同じものを出す。飛ばす向きは爆発の中心から離れる向き。
		m_camera.TriggerShake(2.6f, 0.35f);
		m_playerDamageFlashTime = 0.45f;
		Vector3 away = playerFeet - impact.position;
		away.y = 0.0f;
		if (away.LengthSquared() < 1.0f)
			away = impact.direction;
		if (m_combat.IsPlayerAttacking())
			m_combat.CancelPlayerAttack();
		const Combat::KnockbackData knockback =
			Combat::EnemyKnockbackOf(Combat::EnemyAttackKind::Fireball);
		m_player->applyKnockback(away, knockback.speed, knockback.seconds);
		m_playerAnimator.PlayImpactMotion();
	}
}

void GameScene::OnEnemyChargeHitWall(enemy& e, const Vector3& headPosition, const Vector3& forward)
{
	if (!e.notifyChargeBlocked())
		return;
	// 走りを打ち切ったので、戦闘側の攻撃も取り消す(よろめいている敵から当たらないように)。
	m_combat.CancelEnemyAttack();
	m_camera.TriggerShake(2.4f, 0.40f);
	// 頭が壁にぶつかった場所から破片を飛ばす。
	m_hitEffect.Spawn(
		headPosition + Vector3(0.0f, 14.0f, 0.0f),
		-forward + Vector3(0.0f, 0.6f, 0.0f),
		30,
		1.8f);
	// 壁の根元から土煙が噴き上がる。
	Vector3 wallFoot = headPosition;
	wallFoot.y = ARENA_FLOOR_TOP_Y;
	m_enemyMotionFx.SpawnDust(wallFoot, -forward * 30.0f + Vector3(0.0f, 20.0f, 0.0f),
		22, 40.0f, 13.0f, 1.4f, 0.45f);
}

void GameScene::PushEnemyBodyOutOfWalls()
{
	m_enemyWallPush = Vector3(0.0f, 0.0f, 0.0f);
	if (!m_enemyAnimationMesh || m_enemies.empty() || !m_enemies.front() || m_walls.empty())
		return;
	enemy& e = *m_enemies.front();

	// 体の形を、骨の位置を中心にした球の集まりで表す。半径は体の太さ(胴は横幅約23の半分)。
	// 骨から取るので、首を伸ばす・尾を振る・体を回すといった姿勢の変化にもそのまま付いてくる。
	struct BodyPart { const char* bone; float radius; };
	static constexpr BodyPart PARTS[] = {
		{ "Armature_jaw_upper", 6.0f },
		{ "Armature_neck3", 7.0f },
		{ "Armature_neck2", 7.0f },
		{ "Armature_neck1", 8.0f },
		{ "Armature_back1", 11.0f },
		{ "Armature_back2", 11.0f },
		{ "Armature_tail1", 7.0f },
		{ "Armature_tail2", 5.0f },
		{ "Armature_tail3", 4.0f },
		{ "Armature_leg_front_foot_L", 4.0f },
		{ "Armature_leg_front_foot_R", 4.0f },
		{ "Armature_leg_back_foot_L", 4.0f },
		{ "Armature_leg_back_foot_R", 4.0f },
	};
	std::vector<std::pair<Vector3, float>> spheres;
	spheres.reserve(std::size(PARTS) + 2);
	for (const BodyPart& part : PARTS)
	{
		Vector3 position{};
		if (GetEnemyBoneWorld(part.bone, position))
			spheres.emplace_back(position, part.radius);
	}
	// 骨の無い先端(口先・尾の先)も足す。尾の先は最後の骨よりさらに先にある。
	if (m_enemyMouthValid)
		spheres.emplace_back(m_enemyMouthPosition, 4.0f);
	Vector3 tail2{}, tail3{};
	if (GetEnemyBoneWorld("Armature_tail2", tail2) && GetEnemyBoneWorld("Armature_tail3", tail3))
		spheres.emplace_back(tail3 + (tail3 - tail2) * ENEMY_TAIL_TIP_EXTEND, 3.0f);
	if (spheres.empty())
		return;

	// 闘技場は凸の多角形なので、壁ごとに「一番深くめり込んだ部分」の分だけ内側へ押せば外に出せる。
	// 角では2枚の壁に同時にめり込むので、押した後の位置でもう一度見る。
	// 壁の当たり面は見た目の厚み(ARENA_WALL_DEPTH)の内側の面に合わせる。
	Vector3 push(0.0f, 0.0f, 0.0f);
	for (int pass = 0; pass < 2; ++pass)
	{
		for (const auto& wallObject : m_walls)
		{
			const PLANEINFO pi = wallObject->getEquation().GetPlaneInfo();
			Vector3 normal(pi.plane.a, 0.0f, pi.plane.c);
			const float length = normal.Length();
			if (length <= 0.0001f)
				continue;
			normal /= length;
			const float planeD = pi.plane.d / length;
			// 闘技場の中心(原点)がある側を内側とする。法線の向きに頼らない。
			const float inside = planeD >= 0.0f ? 1.0f : -1.0f;
			float deepest = 0.0f;
			for (const auto& [center, radius] : spheres)
			{
				const float distance = (normal.Dot(center + push) + planeD) * inside -
					ARENA_WALL_DEPTH * 0.5f;
				deepest = std::max(deepest, radius - distance);
			}
			if (deepest > 0.0f)
				push += normal * (inside * deepest);
		}
	}
	push.y = 0.0f;
	if (push.LengthSquared() < 0.0001f)
		return;

	SRT srt = e.getSRT();
	srt.pos += push;
	e.setSRT(srt);
	if (m_enemyMouthValid)
		m_enemyMouthPosition += push;
	m_enemyWallPush = push;

	// 突進の頭が壁に当たって押し戻されたら、壁にぶつかったよろめきに入る。
	// 体で押し戻すようになると、位置の球はもう壁に届かず、球だけの判定ではよろめかなくなるため。
	// 走る向きへ進んだ量の4割以上を押し返されたときだけにして、壁沿いにかすっただけでは止めない。
	if (e.isChargeRunning())
	{
		const float yaw = srt.rot.y;
		const Vector3 forward(-std::sinf(yaw), 0.0f, -std::cosf(yaw));
		const Vector3 velocity = e.getVel();
		const float advanced = std::max(0.0f, velocity.Dot(forward));
		if (advanced > 0.01f && -push.Dot(forward) > advanced * 0.4f)
		{
			const Vector3 head = m_enemyMouthValid ? m_enemyMouthPosition : srt.pos + forward * 26.0f;
			OnEnemyChargeHitWall(e, head, forward);
		}
	}
}

bool GameScene::GetEnemyBoneWorld(const char* bone, Vector3& out) const
{
	if (!m_enemyAnimationMesh || m_enemies.empty() || !m_enemies.front())
		return false;
	Vector3 model{};
	if (!m_enemyAnimationMesh->GetBoneModelPosition(bone, model))
		return false;
	out = Vector3::Transform(model, m_enemies.front()->getRenderSRT().GetMatrix());
	return true;
}

void GameScene::ClearEnemyMotionEffects()
{
	m_enemyMotionFx.Clear();
	m_enemyTailTrail.Clear();
	m_enemyHeadTrail.Clear();
	m_enemyRoarFxSeconds = 0.0f;
}

void GameScene::StartEnemyRoarEffect(float seconds)
{
	m_enemyRoarFxSeconds = std::max(m_enemyRoarFxSeconds, seconds);
	m_enemyRoarRingTimer = 0.0f;
	// 吠えた瞬間の声の圧で、足元の土が輪になって吹き飛ぶ。
	if (!m_enemies.empty() && m_enemies.front())
	{
		Vector3 ground = m_enemies.front()->getSRT().pos;
		ground.y = ARENA_FLOOR_TOP_Y;
		m_enemyMotionFx.SpawnGroundRing(ground, 95.0f, 0.75f, 0.35f);
		m_enemyMotionFx.SpawnDust(ground, Vector3(0.0f, 0.0f, 0.0f), 10, 70.0f, 11.0f, 1.1f, 0.20f);
	}
}

void GameScene::UpdateEnemyMotionEffects(float deltaSeconds)
{
	if (!m_enemies.empty() && m_enemies.front())
	{
		UpdateEnemyFootDust(deltaSeconds);
		UpdateEnemyAttackEffects();

		const enemy& e = *m_enemies.front();
		const float yaw = e.getSRT().rot.y;
		const Vector3 forward(-std::sinf(yaw), 0.0f, -std::cosf(yaw));

		// 突進。走っている間は後脚から土煙を後ろへ蹴り上げ、通った跡に煙の帯を残す。
		// 滑って止まる間は前脚が地面を削り、前と横へ土を押し出す。
		const bool running = e.isChargeRunning();
		const bool skidding = e.isChargeSkidding();
		if (running || skidding)
		{
			m_enemyChargeDustTimer -= deltaSeconds;
			if (m_enemyChargeDustTimer <= 0.0f)
			{
				m_enemyChargeDustTimer += running ? ENEMY_CHARGE_DUST_INTERVAL : ENEMY_SKID_DUST_INTERVAL;
				const int first = running ? 2 : 0;   // 走る間は後脚、滑る間は前脚
				for (int i = first; i < first + 2; ++i)
				{
					Vector3 foot{};
					if (!GetEnemyBoneWorld(ENEMY_FOOT_BONES[i], foot))
						continue;
					foot.y = ARENA_FLOOR_TOP_Y;
					if (running)
						m_enemyMotionFx.SpawnDust(foot, -forward * 35.0f, 1, 14.0f, 9.0f, 0.9f, 0.30f);
					else
						m_enemyMotionFx.SpawnDust(foot, forward * 30.0f, 2, 22.0f, 8.0f, 0.8f, 0.34f);
				}
			}
		}
		else
		{
			m_enemyChargeDustTimer = 0.0f;
		}

		// 咆哮の衝撃波。口元から一定の間隔で輪を出し続ける。
		if (m_enemyRoarFxSeconds > 0.0f)
		{
			m_enemyRoarFxSeconds -= deltaSeconds;
			m_enemyRoarRingTimer -= deltaSeconds;
			if (m_enemyRoarRingTimer <= 0.0f && m_enemyMouthValid)
			{
				m_enemyRoarRingTimer += ENEMY_ROAR_RING_INTERVAL;
				m_enemyMotionFx.SpawnRoarRing(m_enemyMouthPosition, ENEMY_ROAR_RING_RADIUS, 0.55f, 0.30f);
			}
		}
	}

	m_enemyMotionFx.Update(deltaSeconds);
	m_enemyTailTrail.Update(deltaSeconds);
	m_enemyHeadTrail.Update(deltaSeconds);
}

void GameScene::UpdateEnemyFootDust(float deltaSeconds)
{
	// 落ちてくる間は足が地面から遠く、高さの記録が意味を持たないので見ない。
	// 着地した後で記録し直す(UpdateEnemyIntro で着地の瞬間に記録を消している)。
	if (m_enemyIntroActive && m_enemyIntroTime < ENEMY_INTRO_FALL_SECONDS)
		return;
	if (deltaSeconds <= 0.0f)
		return;
	const enemy& e = *m_enemies.front();
	const float yaw = e.getSRT().rot.y;
	const Vector3 forward(-std::sinf(yaw), 0.0f, -std::cosf(yaw));
	for (int i = 0; i < 4; ++i)
	{
		EnemyFootTracker& foot = m_enemyFeet[i];
		foot.bone = ENEMY_FOOT_BONES[i];
		Vector3 position{};
		if (!GetEnemyBoneWorld(foot.bone, position))
			continue;
		const float height = position.y - ARENA_FLOOR_TOP_Y;
		if (!foot.valid)
		{
			foot.previousHeight = height;
			foot.liftedHeight = height;
			foot.valid = true;
			continue;
		}
		foot.liftedHeight = std::max(foot.liftedHeight, height);
		const float descent = (foot.previousHeight - height) / deltaSeconds;
		if (descent > 0.0f)
			foot.descentSpeed = std::max(foot.descentSpeed * 0.8f, descent);

		if (foot.liftedHeight >= ENEMY_FOOT_CONTACT_HEIGHT + ENEMY_FOOT_LIFT_MIN &&
			foot.previousHeight > ENEMY_FOOT_CONTACT_HEIGHT && height <= ENEMY_FOOT_CONTACT_HEIGHT)
		{
			// 強く踏み下ろしたほど、多く・速く散らす。歩きは控えめ、突進や叩き付けは大きく。
			const float strength = std::clamp(foot.descentSpeed / 60.0f, 0.0f, 1.0f);
			Vector3 ground = position;
			ground.y = ARENA_FLOOR_TOP_Y;
			// 前脚の足踏み(突進の構え)は土を後ろへ蹴る。
			const bool pawing = i < 2 && e.getMotionState() == enemy::MotionState::Windup &&
				e.getAttackKind() == Combat::EnemyAttackKind::Charge;
			const Vector3 push = pawing ? -forward * 25.0f : Vector3(0.0f, 0.0f, 0.0f);
			m_enemyMotionFx.SpawnDust(ground, push,
				3 + static_cast<int>(5.0f * strength),
				8.0f + 18.0f * strength,
				6.0f + 4.0f * strength,
				0.8f + 0.4f * strength,
				0.22f + 0.14f * strength);
			foot.liftedHeight = height;
			foot.descentSpeed = 0.0f;
		}
		foot.previousHeight = height;
	}
}

void GameScene::UpdateEnemyAttackEffects()
{
	const enemy& e = *m_enemies.front();
	const enemy::MotionState state = e.getMotionState();
	const Combat::EnemyAttackKind kind = e.getAttackKind();
	const float stateTime = e.getStateTime();
	const bool active = state == enemy::MotionState::Active;

	// 攻撃の通り道。判定が出ている間だけ描く(剣の軌跡と同じく「危ない時間」がそのまま見える)。
	if (active && kind == Combat::EnemyAttackKind::TailSpin && e.isTailSpinning())
	{
		Vector3 tail1{}, tail2{}, tail3{};
		if (GetEnemyBoneWorld("Armature_tail1", tail1) &&
			GetEnemyBoneWorld("Armature_tail2", tail2) &&
			GetEnemyBoneWorld("Armature_tail3", tail3))
		{
			m_enemyTailTrail.Push(tail1, tail3 + (tail3 - tail2) * ENEMY_TAIL_TIP_EXTEND);
		}
	}
	// 叩き付け・噛みつき・薙ぎ払いは、どれも同じクリップ(頭を振り下ろす動き)を速さと構えを変えて使っている。
	// 当てているのは頭なので、首の先から口先までを通り道として描く(前脚は振り下ろしの間むしろ上がっている。ログで確認)。
	if (active && Combat::EnemyAttackUsesAttackClip(kind) && m_enemyMouthValid)
	{
		Vector3 neck{};
		if (GetEnemyBoneWorld("Armature_neck3", neck))
			m_enemyHeadTrail.Push(neck,
				m_enemyMouthPosition + (m_enemyMouthPosition - neck) * ENEMY_HEAD_TRAIL_EXTEND);
	}

	// 叩き付けが地面を打った瞬間(判定が出る瞬間)。頭が打ちつけた地面から土煙と衝撃の輪を出す。
	const float slamHitTime = Combat::EnemyFirstHitTimeOf(Combat::EnemyAttackKind::Slam);
	const bool sameActive = m_enemyFxPreviousState == enemy::MotionState::Active;
	const float previousTime = sameActive ? m_enemyFxPreviousStateTime : -1.0f;
	if (active && kind == Combat::EnemyAttackKind::Slam &&
		previousTime < slamHitTime && stateTime >= slamHitTime)
	{
		Vector3 center = m_enemyMouthValid ? m_enemyMouthPosition : e.getSRT().pos;
		center.y = ARENA_FLOOR_TOP_Y;
		m_enemyMotionFx.SpawnGroundRing(center, 48.0f, 0.45f, 0.50f);
		m_enemyMotionFx.SpawnDust(center, Vector3(0.0f, 0.0f, 0.0f), 18, 48.0f, 11.0f, 1.2f, 0.42f);
	}

	m_enemyFxPreviousState = state;
	m_enemyFxPreviousStateTime = stateTime;
}

std::unordered_map<std::string, Matrix4x4> GameScene::BuildEnemyTellRotations(
	const Combat::EnemyTellPose& tell) const
{
	if (!m_enemyAnimationMesh)
		return {};
	constexpr float EPSILON = 0.0005f;
	if (std::abs(tell.neckPitch) < EPSILON && std::abs(tell.neckYaw) < EPSILON &&
		std::abs(tell.jawOpen) < EPSILON && std::abs(tell.tailYaw) < EPSILON &&
		std::abs(tell.tailWhip) < EPSILON && std::abs(tell.frontLegLift) < EPSILON)
	{
		return {};
	}

	// 回す軸は骨のローカル軸を決め打ちせず、今の姿勢から求める。
	// (最初はBlenderの慣習どおり「長さ方向が+Y、上下の曲げはX軸」と決め打ちしたが、
	//  このドラゴンでは首が上下ではなく前後へ動いた。実測で分かったので計算で求める方式にした)
	//
	// ドラゴンは描画時にX軸へ+90度起こすので、モデル空間の-Zがワールドの上になる。
	//   持ち上げる軸 = (骨の向き × 上)   … この軸で正の回転をすると骨の先が上を向く
	//   左右へ振る軸 = 上
	// 求めた軸は骨自身のローカル空間へ直してから渡す。
	// モデル空間の軸(実測): 上 = -Z(足のZは+0.31、頭のZは-0.18)、前 = -Y、左右 = X。
	// 上下の曲げは「体の左右の軸(X)」まわりで行う。
	// 最初は「骨の向き × 上」で軸を作ったが、首はほぼ真上を向いているためこの外積がほぼ0になり、
	// 軸が定まらず首が前後へ流れていた(実測で判明)。
	const Vector3 upModel(0.0f, 0.0f, -1.0f);
	const Vector3 lateralModel(1.0f, 0.0f, 0.0f);
	const auto& boneGlobals = m_enemyAnimationMesh->GetDebugBoneMatrices();
	// モデル空間の軸を、その骨のローカル空間へ直す。
	const auto toBoneLocal = [&](const char* boneName, const Vector3& modelAxis, Vector3& out)
	{
		const auto found = boneGlobals.find(boneName);
		if (found == boneGlobals.end())
			return false;
		Matrix4x4 rotation = found->second;
		rotation._41 = rotation._42 = rotation._43 = 0.0f;
		out = Vector3::TransformNormal(modelAxis, rotation.Invert());
		if (out.LengthSquared() < 0.000001f)
			return false;
		out.Normalize();
		return true;
	};

	std::unordered_map<std::string, Matrix4x4> offsets;
	// 首は立っている(頭は根元より0.32高い)ので、左右へ振るには「上まわり」ではなく
	// 「前後の軸まわり」に倒す。水平に伸びる尾は「上まわり」で横へ振れる。
	const Vector3 forwardModel(0.0f, -1.0f, 0.0f);
	// boneName に、bend(前後へ倒す。正で後ろへ振りかぶる)とswing(左右へ振る)を足す。
	const auto add = [&](const char* boneName, float bend, float swing, const Vector3& swingAxis)
	{
		if (std::abs(bend) < EPSILON && std::abs(swing) < EPSILON)
			return;
		Matrix4x4 rotation = Matrix4x4::Identity;
		Vector3 axisLocal{};
		if (std::abs(bend) >= EPSILON && toBoneLocal(boneName, lateralModel, axisLocal))
			rotation = rotation * Matrix4x4::CreateFromAxisAngle(axisLocal, bend);
		if (std::abs(swing) >= EPSILON && toBoneLocal(boneName, swingAxis, axisLocal))
			rotation = rotation * Matrix4x4::CreateFromAxisAngle(axisLocal, swing);
		offsets[boneName] = rotation;
	};
	// 首は3本へ分けて曲げる。1本だけ曲げると、そこだけ折れたように見える。
	add("Armature_neck1", tell.neckPitch * 0.40f, tell.neckYaw * 0.40f, forwardModel);
	add("Armature_neck2", tell.neckPitch * 0.35f, tell.neckYaw * 0.35f, forwardModel);
	add("Armature_neck3", tell.neckPitch * 0.25f, tell.neckYaw * 0.25f, forwardModel);
	// あごは下へ開く。
	add("Armature_jaw_lower", -tell.jawOpen, 0.0f, upModel);
	// 尾は水平に伸びているので、上まわりで横へ振る。
	// tailYawは根元から先まで同じ割合で曲げ(尾全体が弓なりになる)、
	// tailWhipは先へ行くほど強く掛ける(根元1割・中3割・先6割)。
	// 1本の棒のように曲げると体と一緒に回っているようにしか見えないので、
	// 先ほど大きく遅れて・大きく追い越す形にして「振り回している」ことを見せる。
	add("Armature_tail1", 0.0f, tell.tailYaw * 0.40f + tell.tailWhip * 0.10f, upModel);
	add("Armature_tail2", 0.0f, tell.tailYaw * 0.35f + tell.tailWhip * 0.30f, upModel);
	add("Armature_tail3", 0.0f, tell.tailYaw * 0.25f + tell.tailWhip * 0.60f, upModel);
	// 前脚は前へ振り上げる(叩き付けの溜めで浮かせる)。
	add("Armature_leg_front_upper_L", tell.frontLegLift, 0.0f, upModel);
	add("Armature_leg_front_upper_R", tell.frontLegLift, 0.0f, upModel);
	return offsets;
}

void GameScene::PlaceTrainingDummy()
{
	if (!m_player)
		return;
	// プレイヤーの正面、少し離れた位置に置き、プレイヤーの方を向かせる。
	// 正面は(-sin, -cos)。敵の向きをプレイヤーの向き+PIにすると、敵の正面がプレイヤーを向く。
	constexpr float DUMMY_DISTANCE = 70.0f;
	const SRT playerSrt = m_player->getSRT();
	const Vector3 forward(-std::sinf(playerSrt.rot.y), 0.0f, -std::cosf(playerSrt.rot.y));
	const Vector3 dummyPosition = playerSrt.pos + forward * DUMMY_DISTANCE;
	if (m_enemies.empty())
	{
		m_enemies.push_back(createEnemyObject(
			this, m_player.get(), dummyPosition, playerSrt.rot.y + PI, ENEMY_MODEL_SCALE));
	}
	else
	{
		// 戦闘の相手は先頭の敵なので、先頭の敵を練習台にして目の前へ移す。
		SRT enemySrt = m_enemies.front()->getSRT();
		enemySrt.pos = dummyPosition;
		enemySrt.rot = Vector3(0.0f, playerSrt.rot.y + PI, 0.0f);
		m_enemies.front()->setSRT(enemySrt);
		m_enemies.front()->setVel(Vector3(0.0f, 0.0f, 0.0f));
	}
	m_enemies.front()->resetEncounter();
	m_enemies.front()->setPassive(true);
	m_combat.CancelEnemyAttack();
	m_combat.SetEnemyDebugInvincible(true);
	m_combat.SetEnemyHpForDebug(m_combat.GetEnemyMaxHp());
	// 敵がいない状態から置いた場合、弱り具合も体力に合わせて戻す。
	m_enemies.front()->setHealthRatio(1.0f);
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
		// ゲームパッドの右スティック。カメラはマウスの移動量(ピクセル)で回る作りなので、
		// 「1秒に回したい角度」をマウスの移動量に直して足す。
		// スティックは倒している間ずっと回るので、カーソルを出していても効かせる。
		const Vector2 lookStick = GameInput::Get().GetLookStick();
		if (lookStick.LengthSquared() > 0.0f && m_camera.GetMouseSensitivity() > 0.0f)
		{
			constexpr float STICK_YAW_RADIANS_PER_SECOND = 2.6f;
			constexpr float STICK_PITCH_RADIANS_PER_SECOND = 1.6f;
			const float toMouse = deltaSeconds / m_camera.GetMouseSensitivity();
			look.active = true;
			look.deltaX += lookStick.x * STICK_YAW_RADIANS_PER_SECOND * toMouse;
			// スティックは上が正、マウスは下が正(下へ動かすと見下ろす)。上へ倒すと見上げる。
			look.deltaY -= lookStick.y * STICK_PITCH_RADIANS_PER_SECOND * toMouse;
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
		if (m_enemies.front()->getAttackKind() == Combat::EnemyAttackKind::TailSpin)
		{
			// 尾回転は回転数(半回転1回か2回)と回る向きで危険な範囲が変わるため、そこまで出す。
			ImGui::Text("敵の攻撃: %s (半回転%d回・%s回り、進み %.0f%%)",
				Combat::EnemyAttackDebugName(m_enemies.front()->getAttackKind()),
				m_enemies.front()->getSpinHalfTurns(),
				m_enemies.front()->getSpinSign() >= 0.0f ? "左" : "右",
				100.0f * m_enemies.front()->getSpinProgress());
		}
		else if (m_enemies.front()->isChargeStunned())
		{
			ImGui::Text("敵の攻撃: %s (壁にぶつかってよろめき中)",
				Combat::EnemyAttackDebugName(m_enemies.front()->getAttackKind()));
		}
		else
		{
			ImGui::Text("敵の攻撃: %s",
				Combat::EnemyAttackDebugName(m_enemies.front()->getAttackKind()));
		}
		ImGui::Text("敵の弱り具合: %s (体力 %.0f%%)",
			m_enemies.front()->getConditionName(),
			100.0f * m_combat.GetEnemyHp() / m_combat.GetEnemyMaxHp());
		// 怯み値の溜まり具合。バーが満ちると怯む。怯むたびにしきい値が上がる。
		// 怒り。溜まり具合と、怒っている残り時間。
		if (m_enemies.front()->isEnraged())
		{
			ImGui::Text("敵の怒り: 怒り中 (残り %.1f秒、%d回目)",
				m_enemies.front()->getRageSecondsLeft(),
				m_enemies.front()->getRageCount());
		}
		else
		{
			ImGui::Text("敵の怒り: 通常 (%.0f / %.0f、怒った回数 %d)",
				m_enemies.front()->getRageDamage(),
				m_enemies.front()->getRageThreshold(),
				m_enemies.front()->getRageCount());
		}
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
			ImGui::RadioButton("尾回転", &m_forcedEnemyAttackIndex, 3);
			ImGui::RadioButton("火球", &m_forcedEnemyAttackIndex, 4);
			ImGui::RadioButton("突進", &m_forcedEnemyAttackIndex, 5);
			ImGui::Unindent();
		}
		// 敵への反映は update() で毎フレーム行う(ここはタブを開いているときしか通らない)。
	}

	// 撮影・調整用。動画を撮るときに、倒れない的を置いたり、自分が倒れないようにしたりする。
	ImGui::SeparatorText("撮影・調整用");
	{
		// 床の色合い。マス目の大きさは同じ(1マス10単位=約1m、太線は4マスごと)。
		ImGui::TextUnformatted("床の色");
		ImGui::SameLine();
		if (ImGui::Button("トレーニングステージ風"))
			m_arenaFloor.SetPalette(ArenaFloor::TrainingPalette());
		ImGui::SameLine();
		if (ImGui::Button("闘技場の石"))
			m_arenaFloor.SetPalette(ArenaFloor::StonePalette());
		ImGui::Checkbox("操作説明を画面の右下に出す", &m_showControlGuide);
		const GameInput& gameInput = GameInput::Get();
		ImGui::Text("ゲームパッド: %s / 操作説明: %s",
			gameInput.IsGamepadConnected() ? "接続中" : "未接続",
			gameInput.GetDisplayDevice() == InputDevice::Gamepad ? "ゲームパッド用" : "キーボード・マウス用");
	}
	{
		bool useLoop = GameFlow::useGameLoop;
		if (ImGui::Checkbox("ゲームループを使う(タイトル→ゲーム→リザルト)", &useLoop))
			GameFlow::useGameLoop = useLoop;
		ImGui::SameLine();
		ImGui::TextDisabled(useLoop ? "(決着でリザルトへ)" : "(決着で戦いをやり直す)");
	}
	if (ImGui::Button("練習台の敵を置く(動かない・攻撃しない・倒れない)"))
		PlaceTrainingDummy();
	// 怒り状態の見た目と動きを、殴らずに確かめるためのボタン。
	if (!m_enemies.empty() && ImGui::Button("敵を怒らせる(咆哮して怒り状態へ)"))
		m_enemies.front()->forceRage();
	if (!m_enemies.empty())
	{
		bool passive = m_enemies.front()->isPassive();
		if (ImGui::Checkbox("敵を動かさない(攻撃もしない)", &passive))
		{
			m_enemies.front()->setPassive(passive);
			if (passive)
				m_combat.CancelEnemyAttack();
		}
	}
	bool enemyInvincible = m_combat.IsEnemyDebugInvincible();
	if (ImGui::Checkbox("敵を無敵にする(当たるが体力は減らない)", &enemyInvincible))
		m_combat.SetEnemyDebugInvincible(enemyInvincible);
	bool playerInvincible = m_combat.IsPlayerDebugInvincible();
	if (ImGui::Checkbox("プレイヤーを無敵にする(当たるが体力は減らない)", &playerInvincible))
		m_combat.SetPlayerDebugInvincible(playerInvincible);
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
