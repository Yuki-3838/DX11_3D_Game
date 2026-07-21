#include    <memory>
#include	<string>
#include	<array>
#include	<vector>
#include	<cstdint>
#include	<fstream>
#include	<iomanip>
#include	<algorithm>
#include	<limits>
#include	"CarScene.h"
#include	"../system/CShader.h"
#include	"../system/imgui/imgui.h"
#include	"../system/MeshManager.h"
#include	"../system/DebugUI.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/renderer.h"
#include	"../system/meshmanager.h"
#include	<filesystem>
#include	<string_view>

namespace {
	using GM31::GE::Collision::BoundingBoxAABB;
	using GM31::GE::Collision::BoundingBoxOBB;

	struct Load3DInfo {
		std::string filename;
		std::string texdirectoryname;
		Load3DInfo(std::string p1, std::string p2) {
			filename = p1;
			texdirectoryname = p2;
		}
	};

	std::string getfilename(std::string_view filestring) {
		auto u8name = std::filesystem::path(filestring).filename().u8string();
		return { reinterpret_cast<const char*>(u8name.data()), u8name.size() };
	}

	std::array<Load3DInfo, 14> g_loadmodel =
	{
			Load3DInfo(
				"assets/model/car000.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/car001.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/car002.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/f1.x",				// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/akai/akai.fbx",		// モデル名
				"assets/model/akai/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/Blue/Blue.pmx",		// モデル名
				"assets/model/Blue/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/glinco/glinco.pmx",	// モデル名
				"assets/model/glinco/"),			// テクスチャのパス

			Load3DInfo(
				"assets/model/hal/hal.pmx",			// モデル名
				"assets/model/hal/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/man/man.fbx",			// モデル名
				"assets/model/man/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/obj/goal.obj",		// モデル名
				"assets/model/obj/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/obj/cylinder.obj",	// モデル名
				"assets/model/obj/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/starwars/TIE_Fighter.x",				// モデル名
				"assets/model/starwars/texture/TIE-Fighter"),		// テクスチャのパス

			Load3DInfo(
				"assets/model/woman/woman.fbx",		// モデル名
				"assets/model/woman/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/jack1/JACK式ポプ子1.1.pmx",	// モデル名
				"assets/model/jack1/")						// テクスチャのパス
	};

	// for debug
	void debugMeshinfo(std::string meshID) {

		CStaticMesh* smesh = MeshManager::getMesh<CStaticMesh>(meshID);
		const std::vector<VERTEX_3D>& vertices = smesh->GetVertices();
		const std::vector<uint32_t>& indices = smesh->GetIndices();
		const std::vector<SUBSET>& subsets = smesh->GetSubsets();

		{
			// テキストファイルとして開く
			std::ofstream ofs("vertices.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "Index X Y Z\n";

			for (size_t i = 0; i < vertices.size(); ++i)
			{
				const Vector3& pos = vertices[i].Position;

				ofs << i << " "
					<< pos.x << " "
					<< pos.y << " "
					<< pos.z << "\n";
			}
		}

		{
			// テキストファイルとして開く
			std::ofstream ofs("indices.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "Index \n";

			for (size_t i = 0; i < indices.size(); ++i)
			{
				const uint32_t& index = indices[i];

				ofs << i << " "
					<< index << "\n";
			}
		}

		// subsetの情報を出力
		{
			// テキストファイルとして開く
			std::ofstream ofs("subsets.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "subset \n";

			for (size_t i = 0; i < subsets.size(); ++i)
			{
				const SUBSET& subset = subsets[i];

				ofs << i << " "
					<< subset.MaterialIdx << " "
					<< subset.IndexBase << "/"
					<< subset.IndexNum << ":"
					<< subset.VertexBase << "/"
					<< subset.VertexNum << ":"
					<< "\n";
			}
		}
	}

	Vector3 getAABBSize(const BoundingBoxAABB& aabb)
	{
		return aabb.max - aabb.min;
	}

	Vector3 getAABBCenter(const BoundingBoxAABB& aabb)
	{
		return (aabb.min + aabb.max) * 0.5f;
	}

	Vector3 safeNormalize(Vector3 axis, const Vector3& fallback, float& length)
	{
		length = axis.Length();
		if (length <= 1.0e-6f) {
			length = 1.0f;
			return fallback;
		}

		return axis / length;
	}

	BoundingBoxOBB makeOBB(
		const BoundingBoxAABB& localAABB,
		Matrix4x4 worldmtx)
	{
		BoundingBoxOBB obb{};
		const Vector3 localCenter = getAABBCenter(localAABB);
		const Vector3 localSize = getAABBSize(localAABB);

		// ローカルAABBの中心をモデル行列で移動し、OBBのワールド中心にする。
		obb.center = localCenter;
		obb.worldcenter = Vector3::Transform(localCenter, worldmtx);
		obb.min = localAABB.min;
		obb.max = localAABB.max;

		float scaleX = 1.0f;
		float scaleY = 1.0f;
		float scaleZ = 1.0f;

		// worldmtxの各ローカル軸を取り出す。長さは拡大縮小率、向きはOBBの軸になる。
		obb.axisX = safeNormalize(Vector3(worldmtx._11, worldmtx._12, worldmtx._13), Vector3(1, 0, 0), scaleX);
		obb.axisY = safeNormalize(Vector3(worldmtx._21, worldmtx._22, worldmtx._23), Vector3(0, 1, 0), scaleY);
		obb.axisZ = safeNormalize(Vector3(worldmtx._31, worldmtx._32, worldmtx._33), Vector3(0, 0, 1), scaleZ);

		obb.lengthx = localSize.x * scaleX;
		obb.lengthy = localSize.y * scaleY;
		obb.lengthz = localSize.z * scaleZ;

		return obb;
	}

	void drawBoxEdges(const std::array<Vector3, 8>& corners, Color col)
	{
		constexpr std::array<std::array<int, 2>, 12> edges = { {
			{{0, 1}}, {{1, 3}}, {{3, 2}}, {{2, 0}},
			{{4, 5}}, {{5, 7}}, {{7, 6}}, {{6, 4}},
			{{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
		} };

		for (const auto& edge : edges) {
			Segment segment(corners[edge[0]], corners[edge[1]]);
			segment.SetWidth(3.0f);
			segment.Draw(Matrix4x4::Identity, col);
		}
	}
}

// モデル選択
void CarScene::debugModelSelect()
{
	static int selected_model = 0;
	static int selected_target_model = 1;

	ImGui::SetNextWindowPos(ImVec2(20, 70), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
	ImGui::Begin("Model Selector");
	ImGui::PushItemWidth(-1.0f);

	// 現在選択されているモデルの名前をプレビュー用に取得（範囲外アクセスも防止）
	std::string preview_name = "None";
	if (selected_model >= 0 && selected_model < g_loadmodel.size())
	{
		preview_name = getfilename(g_loadmodel[selected_model].filename);
	}

	// BeginComboを使ってドロップダウンを作成
	if (ImGui::BeginCombo("Model", preview_name.c_str()))
	{
		for (int i = 0; i < g_loadmodel.size(); ++i)
		{
			const bool is_selected = (selected_model == i);
			std::string item_name = getfilename(g_loadmodel[i].filename);

			// リストの各アイテムを描画し、クリックされたか判定
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_model = i;

				m_meshid = getfilename(g_loadmodel[selected_model].filename);

				if (MeshManager::ContainsRenderer(item_name)==false) {
					
					// メッシュを生成
					std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
					mesh->Load(g_loadmodel[selected_model].filename, g_loadmodel[selected_model].texdirectoryname);

					// メッシュレンダラを生成
					std::unique_ptr<CStaticMeshRenderer> meshrenderer = std::make_unique<CStaticMeshRenderer>();
					meshrenderer->Init(*mesh.get());

					MeshManager::RegisterMesh<CStaticMesh>(m_meshid, std::move(mesh));
					MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(m_meshid, std::move(meshrenderer));

				}

				updateModelLocalAABB();
			}

			// ドロップダウンを開いた時、現在選択されているアイテムにフォーカスを合わせる
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Text("Selected Model: %d", selected_model);

	std::string target_preview_name = "None";
	if (selected_target_model >= 0 && selected_target_model < g_loadmodel.size())
	{
		target_preview_name = getfilename(g_loadmodel[selected_target_model].filename);
	}

	// 当たり判定の相手側もモデルとして選択する。
	if (ImGui::BeginCombo("Target Model", target_preview_name.c_str()))
	{
		for (int i = 0; i < g_loadmodel.size(); ++i)
		{
			const bool is_selected = (selected_target_model == i);
			std::string item_name = getfilename(g_loadmodel[i].filename);

			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_target_model = i;
				loadModel(selected_target_model, m_targetMeshid);
				updateLocalAABB(m_targetMeshid, m_targetLocalAABB, m_hasTargetLocalAABB);
			}

			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Text("Target Model: %d", selected_target_model);

	ImGui::PopItemWidth();
	ImGui::End();
}

void CarScene::debugBoundingBox()
{
	ImGui::SetNextWindowPos(ImVec2(880, 70), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_FirstUseEver);
	ImGui::Begin("Bounding Box");
	ImGui::PushItemWidth(-1.0f);

	ImGui::Text("AABB: local AABB -> world transform");
	ImGui::Text("OBB: local AABB center/axis/length");
	ImGui::Text("Collision: 15 separating axes");

	ImGui::Separator();
	ImGui::Text("Collision Target Model");
	ImGui::SliderFloat3("Target Position", &m_targetPosition.x, -300.0f, 300.0f);
	ImGui::SliderFloat3("Target Rotation", &m_targetRotation.x, 0.0f, PI);
	ImGui::SliderFloat3("Target Scale", &m_targetScale.x, 0.1f, 20.0f);

	if (m_hasModelLocalAABB && m_hasTargetLocalAABB) {
		const BoundingBoxOBB modelOBB = calcModelOBB(m_modelLocalAABB, m_ScaleMtx * m_RotationMtx);
		const BoundingBoxOBB targetOBB = calcModelOBB(m_targetLocalAABB, calcTargetWorldMatrix());
		const bool isHit = GM31::GE::Collision::CollisionOBB(modelOBB, targetOBB);

		ImGui::Text("Model OBB vs Target OBB: %s", isHit ? "Hit" : "No Hit");
	}

	ImGui::PopItemWidth();
	ImGui::End();
}

// 角度から姿勢行列をつくる
void CarScene::debugRubikCubeRotation()
{

	ImGui::Begin("DebugRubikCube Rotation");

	ImGui::SliderFloat("X Rotation", &m_Rotation.x, 0.0f, PI);
	ImGui::SliderFloat("Y Rotation", &m_Rotation.y, 0.0f, PI);
	ImGui::SliderFloat("Z Rotation", &m_Rotation.z, 0.0f, PI);

	// 回転角度から回転行列を作成
	Matrix4x4 rotmtxX = Matrix4x4::CreateRotationX(m_Rotation.x);
	Matrix4x4 rotmtxY = Matrix4x4::CreateRotationY(m_Rotation.y);
	Matrix4x4 rotmtxZ = Matrix4x4::CreateRotationZ(m_Rotation.z);

	// 合成
	m_RotationMtx = rotmtxX * rotmtxY * rotmtxZ;

	// カメラの位置を極座標からデカルト座標に変換
	ImGui::End();
}

// ローカル軸回転
void CarScene::debugRubikCubeLocalRotation()
{
	Vector3 inputangle = { 0.0f,0.0f,0.0f };

	ImGui::Begin("DebugRubikCube Local Rotation");

	ImGui::SliderFloat("X Rotation", &inputangle.x, 0.0f, PI);
	ImGui::SliderFloat("Y Rotation", &inputangle.y, 0.0f, PI);
	ImGui::SliderFloat("Z Rotation", &inputangle.z, 0.0f, PI);

	// ローカル軸を取得
	Vector3 up = m_RotationMtx.Up();
	Vector3 right = m_RotationMtx.Right();
	Vector3 forward = m_RotationMtx.Forward();

	Quaternion qy = Quaternion::CreateFromAxisAngle(up, inputangle.y);
	Quaternion qx = Quaternion::CreateFromAxisAngle(right, inputangle.x);
	Quaternion qz = Quaternion::CreateFromAxisAngle(forward, inputangle.z);

	m_RotationQ = m_RotationQ * qx * qy * qz;

	m_RotationMtx = Matrix4x4::CreateFromQuaternion(m_RotationQ);

	static Vector3 scale = { 1.0f,1.0f,1.0f };

	ImGui::SliderFloat("X scale", &scale.x, 0.5f, 20.0f);
	ImGui::SliderFloat("Y scale", &scale.y, 0.5f, 20.0f);
	ImGui::SliderFloat("Z scale", &scale.z, 0.5f, 20.0f);

	m_ScaleMtx = Matrix4x4::CreateScale(scale);

	ImGui::End();
}

CarScene::CarScene()
{

}

void CarScene::update(uint64_t deltatime)
{

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
		m_segments[cnt]->SetWidth(3);
		m_segments[cnt]->Draw(m_RotationMtx, axiscol[cnt]);
	}

	Matrix4x4 mtx = m_ScaleMtx* m_RotationMtx;
	Renderer::SetWorldMatrix(&mtx);

	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid)->Draw();

	Matrix4x4 targetMtx = calcTargetWorldMatrix();
	if (!m_targetMeshid.empty()) {
		Renderer::SetWorldMatrix(&targetMtx);
		ShaderManager::Get<CShader>("Shader3D")->SetGPU();
		MeshManager::getRenderer<CStaticMeshRenderer>(m_targetMeshid)->Draw();
	}

	if (m_hasModelLocalAABB && m_hasTargetLocalAABB) {
		const BoundingBoxAABB worldAABB = calcWorldAABB(m_modelLocalAABB, mtx);
		const BoundingBoxAABB targetWorldAABB = calcWorldAABB(m_targetLocalAABB, targetMtx);
		const BoundingBoxOBB modelOBB = calcModelOBB(m_modelLocalAABB, mtx);
		const BoundingBoxOBB targetOBB = calcModelOBB(m_targetLocalAABB, targetMtx);
		const bool isHit = GM31::GE::Collision::CollisionOBB(modelOBB, targetOBB);

		// AABB: モデルの8頂点をワールド変換後、XYZ軸に平行な箱として表示する。
		drawAABB(worldAABB, Color(1, 1, 0, 1));

		// AABB: 当たり判定相手モデルも同じ方法で表示する。
		drawAABB(targetWorldAABB, Color(0, 1, 0, 1));

		// OBB: ローカルAABBの中心・軸・長さを使い、モデルの回転に追従する箱として表示する。
		drawOBB(modelOBB, isHit ? Color(1, 0, 0, 1) : Color(0, 1, 1, 1));

		// OBB: 相手モデルのOBB。15本の分離軸で当たっていたら赤く表示する。
		drawOBB(targetOBB, isHit ? Color(1, 0, 0, 1) : Color(1, 1, 1, 1));
	}

}

void CarScene::init()
{
	// カメラ(3D)の初期化
	m_camera.Init();
	m_camera.SetPosition(Vector3(0, 0, -300));
	m_camera.SetLookat(Vector3(0, 0, 0));
	m_camera.SetUP(Vector3(0, 1, 0));


	// ローカル軸表示用線分の初期化
	m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
	m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
	m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));

	// クオータニオンに単位クオータニオンをセット
	m_RotationQ = Quaternion::Identity;
	m_RotationMtx = Matrix4x4::Identity;
	m_ScaleMtx = Matrix4x4::Identity;

	// シェーダを生成
	std::unique_ptr<CShader> shader{};
	shader = std::make_unique<CShader>();
	shader->Create(
		"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
		"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー
	);
	ShaderManager::Register<CShader>("Shader3D", std::move(shader));

	// メッシュを生成
	std::unique_ptr<CStaticMesh> mesh{};
	mesh = std::make_unique<CStaticMesh>();
	mesh->Load(g_loadmodel[0].filename, g_loadmodel[0].texdirectoryname);

	// メッシュレンダラを生成
	std::unique_ptr<CStaticMeshRenderer> meshrenderer{};
	meshrenderer = std::make_unique<CStaticMeshRenderer>();
	meshrenderer->Init(*mesh.get());

	m_meshid = getfilename(g_loadmodel[0].filename);

	MeshManager::RegisterMesh<CStaticMesh>(m_meshid, std::move(mesh));
	MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(m_meshid, std::move(meshrenderer));
	updateModelLocalAABB();

	loadModel(1, m_targetMeshid);
	updateLocalAABB(m_targetMeshid, m_targetLocalAABB, m_hasTargetLocalAABB);


	// クオータニオンから回転行列
	DebugUI::RedistDebugFunction([this]() {
		debugRubikCubeLocalRotation();
		});

	// モデル選択
	DebugUI::RedistDebugFunction([this]() {
		debugModelSelect();
		});

	// AABB/BBOX/OBB当たり判定の確認
	DebugUI::RedistDebugFunction([this]() {
		debugBoundingBox();
		});

	m_shapecube = std::make_unique<Box>(1.0f, 1.0f, 1.0f);

}


void CarScene::dispose()
{

}

void CarScene::loadModel(int modelIndex, std::string& meshid)
{
	if (modelIndex < 0 || modelIndex >= static_cast<int>(g_loadmodel.size())) {
		return;
	}

	meshid = getfilename(g_loadmodel[modelIndex].filename);

	if (MeshManager::ContainsRenderer(meshid)) {
		return;
	}

	// 同じモデルリストから相手モデルもロードする。
	// MeshManagerで共有するので、既にロード済みなら再生成しない。
	std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
	mesh->Load(g_loadmodel[modelIndex].filename, g_loadmodel[modelIndex].texdirectoryname);

	std::unique_ptr<CStaticMeshRenderer> meshrenderer = std::make_unique<CStaticMeshRenderer>();
	meshrenderer->Init(*mesh.get());

	MeshManager::RegisterMesh<CStaticMesh>(meshid, std::move(mesh));
	MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(meshid, std::move(meshrenderer));
}

void CarScene::updateModelLocalAABB()
{
	updateLocalAABB(m_meshid, m_modelLocalAABB, m_hasModelLocalAABB);
}

void CarScene::updateLocalAABB(
	const std::string& meshid,
	BoundingBoxAABB& localAABB,
	bool& hasLocalAABB)
{
	CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(meshid);
	if (mesh == nullptr || mesh->GetVertices().empty()) {
		hasLocalAABB = false;
		return;
	}

	Vector3 minVec(
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max());
	Vector3 maxVec(
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest());

	// 選択モデルの全頂点から、モデルローカル空間での最小/最大を求める。
	// ここを基準にすると、モデルが変わっても実寸に合ったAABBを表示できる。
	for (const VERTEX_3D& vertex : mesh->GetVertices()) {
		const Vector3& pos = vertex.Position;
		minVec.x = std::min(minVec.x, pos.x);
		minVec.y = std::min(minVec.y, pos.y);
		minVec.z = std::min(minVec.z, pos.z);
		maxVec.x = std::max(maxVec.x, pos.x);
		maxVec.y = std::max(maxVec.y, pos.y);
		maxVec.z = std::max(maxVec.z, pos.z);
	}

	localAABB.min = minVec;
	localAABB.max = maxVec;
	hasLocalAABB = true;
}

Matrix4x4 CarScene::calcTargetWorldMatrix() const
{
	const Matrix4x4 scaleMtx = Matrix4x4::CreateScale(m_targetScale);
	const Matrix4x4 rotMtx = Matrix4x4::CreateFromYawPitchRoll(
		m_targetRotation.y,
		m_targetRotation.x,
		m_targetRotation.z);
	const Matrix4x4 posMtx = Matrix4x4::CreateTranslation(m_targetPosition);

	return scaleMtx * rotMtx * posMtx;
}

BoundingBoxAABB CarScene::calcWorldAABB(
	const BoundingBoxAABB& localAABB,
	Matrix4x4 worldmtx) const
{
	BoundingBoxAABB worldAABB{};
	worldAABB.min = Vector3(
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max());
	worldAABB.max = Vector3(
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest());

	// ローカルAABBの8頂点を現在のモデル行列で変換し直す。
	// 回転後のAABBは軸平行なので、変換後の8点から再度min/maxを取る。
	for (const Vector3& corner : GM31::GE::Collision::GetCorners(localAABB)) {
		const Vector3 pos = Vector3::Transform(corner, worldmtx);
		worldAABB.min.x = std::min(worldAABB.min.x, pos.x);
		worldAABB.min.y = std::min(worldAABB.min.y, pos.y);
		worldAABB.min.z = std::min(worldAABB.min.z, pos.z);
		worldAABB.max.x = std::max(worldAABB.max.x, pos.x);
		worldAABB.max.y = std::max(worldAABB.max.y, pos.y);
		worldAABB.max.z = std::max(worldAABB.max.z, pos.z);
	}

	return worldAABB;
}

BoundingBoxOBB CarScene::calcModelOBB(
	const BoundingBoxAABB& localAABB,
	Matrix4x4 worldmtx) const
{
	return makeOBB(localAABB, worldmtx);
}

void CarScene::drawAABB(const BoundingBoxAABB& aabb, Color col)
{
	const std::vector<Vector3> src = GM31::GE::Collision::GetCorners(aabb);
	std::array<Vector3, 8> corners{};
	std::copy(src.begin(), src.end(), corners.begin());
	drawBoxEdges(corners, col);
}

void CarScene::drawOBB(const BoundingBoxOBB& obb, Color col)
{
	const Vector3 x = obb.axisX * (obb.lengthx * 0.5f);
	const Vector3 y = obb.axisY * (obb.lengthy * 0.5f);
	const Vector3 z = obb.axisZ * (obb.lengthz * 0.5f);

	std::array<Vector3, 8> corners = {
		obb.worldcenter - x - y - z,
		obb.worldcenter + x - y - z,
		obb.worldcenter - x + y - z,
		obb.worldcenter + x + y - z,
		obb.worldcenter - x - y + z,
		obb.worldcenter + x - y + z,
		obb.worldcenter - x + y + z,
		obb.worldcenter + x + y + z,
	};

	drawBoxEdges(corners, col);
}
