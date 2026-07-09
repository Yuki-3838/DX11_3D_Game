#include    <memory>
#include	<string>
#include	<array>
#include	<cstdint>
#include	<filesystem>
#include	<string_view>
#include	<utility>
#include	"CarScene.h"
#include	"../system/CShader.h"
#include	"../system/imgui/imgui.h"
#include	"../system/MeshManager.h"
#include	"../system/DebugUI.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/renderer.h"
#include	"../system/transform.h"
#include	"../gameobject/player.h"
#include	"../system/C3DShape.h"
#include	"../system/commontypes.h"
#include    "../system/RandomEngine.h"

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
		auto u8name = std::filesystem::path(filestring).filename().u8string();
		return { reinterpret_cast<const char*>(u8name.data()), u8name.size() };
	}

	std::array<Load3DInfo, 3> g_loadmodel =
	{
			Load3DInfo(
				"car000.x",
				"assets/model/car000.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"car001.x",
				"assets/model/car001.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"car002.x",
				"assets/model/car002.x",			// モデル名
				"assets/model/"),					// テクスチャのパス
	};
}

CarScene::CarScene()
{

}

void CarScene::update(uint64_t deltatime)
{
	m_player->update(deltatime);

	// 敵を更新
	{
		for (auto& e : m_enemies) {
			e->update(deltatime);
		}
	}


}

void CarScene::draw(uint64_t deltatime)
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
		SRT srt = m_player->getSRT();
		Matrix4x4 worldmtx{};
		worldmtx = srt.GetMatrix();
		Renderer::SetWorldMatrix(&worldmtx);

		ShaderManager::Get<CShader>("Shader3D")->SetGPU();					// シェーダをセット
		MeshManager::getRenderer<CStaticMeshRenderer>("car000.x")->Draw();	// メッシュを描画
	}

	// 敵を描画
	{
		for (auto& e : m_enemies) {
			SRT srt = e->getSRT();
			Matrix4x4 worldmtx{};
			worldmtx = srt.GetMatrix();
			Renderer::SetWorldMatrix(&worldmtx);

			MeshManager::getRenderer<CStaticMeshRenderer>("car001.x")->Draw();	// メッシュを描画
		}
	}
}

void CarScene::init()
{
	// カメラ(3D)の初期化
	m_camera.Init();
	m_camera.SetPosition(Vector3(0, 50, -300));
	m_camera.SetLookat(Vector3(0, 0, 0));
	m_camera.SetUP(Vector3(0, 1, 0));


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

	// メッシュを生成
	for (int cnt = 0; cnt < g_loadmodel.size(); cnt++) {
		std::unique_ptr<CStaticMesh> mesh{};
		mesh = std::make_unique<CStaticMesh>();
		mesh->Load(g_loadmodel[cnt].filename, g_loadmodel[cnt].texdirectoryname);

		// メッシュレンダラを生成
		std::unique_ptr<CStaticMeshRenderer> meshrenderer{};
		meshrenderer = std::make_unique<CStaticMeshRenderer>();
		meshrenderer->Init(*mesh.get());

		g_loadmodel[cnt].meshid = getfilename(g_loadmodel[cnt].filename);

		MeshManager::RegisterMesh<CStaticMesh>(g_loadmodel[cnt].meshid, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(g_loadmodel[cnt].meshid, std::move(meshrenderer));
	}

	m_player= std::make_unique<player>(this);
	m_player->init();

	m_field = std::make_unique<field>(this);
	m_field->init();

	// 乱数エンジン
	RandomEngine rnd;

	// 敵初期化
	for (int cnt = 0; cnt < ENEMYMAX; cnt++) {

		// プレイヤ初期化
		std::unique_ptr<enemy> e = std::make_unique<enemy>(this);
		e->setTarget(m_player.get());

		float x =
			static_cast<float>(rnd.uniformReal(-500, 500));

		float z =
			static_cast<float>(rnd.uniformReal(-500, 500));

		float scale =
			static_cast<float>(rnd.uniformReal(0.5f, 0.8f));

		SRT srt{};
		srt.pos = Vector3(x, 0, z);
		srt.rot.y = static_cast<float>(rnd.uniformReal(-PI, PI));
		srt.scale = Vector3(scale, scale, scale);

		e->setSRT(srt);

		m_enemies.push_back(std::move(e));
	}
}

void CarScene::dispose()
{

}