#include	<iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include	"CAnimationMesh.h"
#include	"utility.h"
#include	"meshmanager.h"
#include	"DebugUI.h"
#include	"imgui/imgui.h"

namespace
{
	class CGuaranteedSwordMesh final : public CMesh
	{
		void AddFace(
			const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d,
			const Vector3& normal, const Color& color)
		{
			const unsigned int base = static_cast<unsigned int>(m_vertices.size());
			const Vector3 positions[4] = { a, b, c, d };
			const Vector2 texcoords[4] = {
				Vector2(0.0f, 1.0f), Vector2(0.0f, 0.0f),
				Vector2(1.0f, 0.0f), Vector2(1.0f, 1.0f)
			};
			for (int i = 0; i < 4; ++i)
			{
				VERTEX_3D vertex{};
				vertex.Position = positions[i];
				vertex.Normal = normal;
				vertex.Diffuse = color;
				vertex.TexCoord = texcoords[i];
				m_vertices.push_back(vertex);
			}
			m_indices.insert(m_indices.end(), {
				base + 0, base + 1, base + 2,
				base + 0, base + 2, base + 3
			});
		}

		void AddBox(const Vector3& center, const Vector3& size, const Color& color)
		{
			const Vector3 h = size * 0.5f;
			const float l = center.x - h.x;
			const float r = center.x + h.x;
			const float b = center.y - h.y;
			const float t = center.y + h.y;
			const float n = center.z - h.z;
			const float f = center.z + h.z;

			AddFace(Vector3(l,b,f), Vector3(l,t,f), Vector3(r,t,f), Vector3(r,b,f), Vector3(0,0,1), color);
			AddFace(Vector3(r,b,n), Vector3(r,t,n), Vector3(l,t,n), Vector3(l,b,n), Vector3(0,0,-1), color);
			AddFace(Vector3(l,b,n), Vector3(l,t,n), Vector3(l,t,f), Vector3(l,b,f), Vector3(-1,0,0), color);
			AddFace(Vector3(r,b,f), Vector3(r,t,f), Vector3(r,t,n), Vector3(r,b,n), Vector3(1,0,0), color);
			AddFace(Vector3(l,t,f), Vector3(l,t,n), Vector3(r,t,n), Vector3(r,t,f), Vector3(0,1,0), color);
			AddFace(Vector3(l,b,n), Vector3(l,b,f), Vector3(r,b,f), Vector3(r,b,n), Vector3(0,-1,0), color);
		}

	public:
		CGuaranteedSwordMesh()
		{
			// 全長1.0、柄の底を原点にした簡易剣。FBXやテクスチャに依存しない。
			AddBox(Vector3(0.0f, 0.09f, 0.0f), Vector3(0.055f, 0.18f, 0.045f), Color(0.22f, 0.10f, 0.04f, 1.0f));
			AddBox(Vector3(0.0f, 0.195f, 0.0f), Vector3(0.25f, 0.035f, 0.055f), Color(0.95f, 0.68f, 0.12f, 1.0f));
			AddBox(Vector3(0.0f, 0.60f, 0.0f), Vector3(0.075f, 0.78f, 0.025f), Color(0.88f, 0.94f, 1.0f, 1.0f));
		}
	};

	std::string NormalizeBoneName(std::string value)
	{
		value.erase(std::remove_if(value.begin(), value.end(), [](char c) {
			return c == '_' || c == '-' || c == ' ';
		}), value.end());
		for (char& c : value)
			if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		return value;
	}

	int SwordBonePriority(const std::string& name)
	{
		const std::string normalized = NormalizeBoneName(name);
		int score = 0;

		// このキャラクターでは実際に変形へ使われる右手首ボーンが
		// 「右手首D」。腕・腕捩より必ずこちらを優先する。
		if (name == "右手首D") score = 10000;
		else if (name.find("右手首") != std::string::npos) score = 9500;
		else if (normalized == "rightwrist" || normalized == "righthand") score = 9000;
		else if (normalized.find("rightwrist") != std::string::npos) score = 8500;
		else if (normalized.find("righthand") != std::string::npos) score = 8000;
		else if (name.find("右手") != std::string::npos) score = 7500;
		else if (normalized.find("hand") != std::string::npos) score = 6000;
		else if (name == "右腕D") score = 2500;
		else if (name.find("右腕") != std::string::npos) score = 1500;
		else if (normalized.find("rightarm") != std::string::npos) score = 1000;

		if (name.find("捩") != std::string::npos ||
			normalized.find("twist") != std::string::npos)
			score -= 5000;
		return score;
	}
}

void CAnimationMesh::SetCurentAnimation(aiAnimation * currentanimation) {
	m_CurrentAnimation = currentanimation;
}

// ノードツリー表示(debug用)
static void DispNodeTree(CTreeNode<std::string>* ptree) 
{
	std::cout << ptree->m_nodedata << std::endl;

	for (unsigned int n = 0; n < ptree->m_children.size(); n++)
	{
		DispNodeTree(ptree->m_children[n].get());
	}
}

void CAnimationMesh::UpdateSwordWorldTransform(const Matrix4x4& parentWorld)
{
	const bool drawProxy = m_swordUseGuaranteedProxy && m_swordProxyMesh;
	const bool drawFbx = !m_swordUseGuaranteedProxy && m_swordMesh;
	if (!m_swordEnabled || (!drawProxy && !drawFbx))
	{
		m_swordWorldSegmentValid = false;
		return;
	}

	const auto boneIt = m_DebugBoneMatrices.find(m_swordBoneName);
	const bool useBoneAttachment = !m_swordForceTestPlacement &&
		boneIt != m_DebugBoneMatrices.end();
	const Matrix4x4 boneMatrix = useBoneAttachment
		? boneIt->second
		: Matrix4x4::Identity;
	const Vector3 rotationRadians(
		m_swordRotationDegrees.x * PI / 180.0f,
		m_swordRotationDegrees.y * PI / 180.0f,
		m_swordRotationDegrees.z * PI / 180.0f);
	const Matrix4x4 centerCorrection = drawProxy
		? Matrix4x4::Identity
		: Matrix4x4::CreateTranslation(-m_swordModelCenter);
	const Matrix4x4 sizeMatrix = drawProxy
		? Matrix4x4::CreateScale(m_swordProxyLength)
		: Matrix4x4::CreateScale(m_swordScale);
	const Vector3 placement = useBoneAttachment
		? m_swordHandOffset
		: m_swordTestPosition;
	const Matrix4x4 swordLocal = centerCorrection *
		sizeMatrix *
		Matrix4x4::CreateFromYawPitchRoll(
			rotationRadians.y, rotationRadians.x, rotationRadians.z) *
		Matrix4x4::CreateTranslation(placement.x, placement.y, placement.z);

	m_swordWorldMatrix = swordLocal * boneMatrix * parentWorld;
	const Vector3 nextBase = Vector3::Transform(
		Vector3(0.0f, 0.0f, 0.0f), m_swordWorldMatrix);
	const Vector3 nextTip = Vector3::Transform(
		drawProxy ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.5f, 0.0f),
		m_swordWorldMatrix);
	m_swordPreviousWorldTip = m_swordWorldSegmentValid ? m_swordWorldTip : nextTip;
	m_swordWorldBase = nextBase;
	m_swordWorldTip = nextTip;
	m_swordWorldSegmentValid = true;
}

void CAnimationMesh::Draw()
{
	// メッシュ描画
	m_StaticMeshRenderer.Draw();
	const bool drawProxy = m_swordUseGuaranteedProxy && m_swordProxyMesh;
	const bool drawFbx = !m_swordUseGuaranteedProxy && m_swordMesh;
	if (m_swordEnabled && m_swordWorldSegmentValid && (drawProxy || drawFbx))
	{
		Matrix4x4 parentWorld = Renderer::GetWorldMatrix();
		Renderer::SetWorldMatrix(&m_swordWorldMatrix);
		if (auto* swordShader = ShaderManager::Get<CShader>("Shader3D"))
		{
			swordShader->SetGPU();
			if (drawProxy)
			{
				m_swordProxyMaterial.SetGPU();
				m_swordProxyRenderer.Draw();
			}
			else
			{
				m_swordRenderer.Draw();
			}
		}
		Renderer::SetWorldMatrix(&parentWorld);
	}
}


void CAnimationMesh::Load(std::string filename, std::string texturedirectory) 
{
	// メッシュ読み込み
	// アニメーション用メッシュはボーン変換を後段で適用するため、
	// Assimp の静的頂点変換は行わない。
	CStaticMesh::Load(filename, texturedirectory, false);
	m_BoneDictionary.clear();
	m_RestLocalMatrices.clear();

	// アニメーションデータ(ASSIMP用）
	std::unordered_map<std::string, GM31::GE::myAssimp::BONE> assimp_BoneDictionary{};	// 20240714 DX化

	// ボーン辞書取得（ボーン名をキーにしてボーン情報が取れる）
	assimp_BoneDictionary = GM31::GE::myAssimp::GetBoneDictionary();					// 20240714 DX化

	for (auto& asimpbone : assimp_BoneDictionary) {										// 20240714 DX化
		BONE dxbone;																	// 20240714 DX化	

		dxbone.meshname = asimpbone.second.meshname;									// 20240714 DX化
		dxbone.armaturename = asimpbone.second.armaturename;							// 20240714 DX化
		dxbone.bonename = asimpbone.second.bonename;									// 20240714 DX化
		dxbone.idx = asimpbone.second.idx;												// 20240714 DX化

		dxbone.OffsetMatrix = utility::aiMtxToDxMtx(asimpbone.second.OffsetMatrix);
		dxbone.AnimationMatrix = Matrix4x4::Identity;										// 20240714 DX化
		dxbone.Matrix = Matrix4x4::Identity;												// 20240714 DX化

		dxbone.weights.clear();															// 20240714 DX化
		for (auto& asimpweight : asimpbone.second.weights)								// 20240714 DX化	
		{
			WEIGHT dxweight;															// 20240714 DX化			
			dxweight.bonename = asimpweight.bonename;									// 20240714 DX化
			dxweight.meshname = asimpweight.meshname;									// 20240714 DX化
			dxweight.vertexindex = asimpweight.vertexindex;								// 20240714 DX化
			dxweight.weight = asimpweight.weight;										// 20240714 DX化
			dxbone.weights.push_back(dxweight);											// 20240714 DX化		
		}																				// 20240714 DX化

		m_BoneDictionary[asimpbone.first] = dxbone;										// 20240714 DX化
	}																	

	// ボーン名ツリー取得
	m_AssimpNodeNameTree = GM31::GE::myAssimp::GetBoneNameTree();
	for (const auto& [name, matrix] : GM31::GE::myAssimp::GetNodeLocalMatrices())
	{
		m_RestLocalMatrices[name] = utility::aiMtxToDxMtx(matrix);
	}

	// レンダラ初期化
	m_StaticMeshRenderer.Init(*this);

	float playerExtent = 100.0f;
	if (!m_vertices.empty())
	{
		Vector3 minPosition = m_vertices.front().Position;
		Vector3 maxPosition = minPosition;
		for (const auto& vertex : m_vertices)
		{
			minPosition = Vector3::Min(minPosition, vertex.Position);
			maxPosition = Vector3::Max(maxPosition, vertex.Position);
		}
		playerExtent = std::max(maxPosition.y - minPosition.y, 1.0f);
	}

	// FBXの状態に関係なく、剣を振る機能を確認できる描画用メッシュ。
	m_swordProxyLength = playerExtent * 0.75f;
	m_swordProxyMesh = std::make_unique<CGuaranteedSwordMesh>();
	m_swordProxyRenderer.Init(*m_swordProxyMesh);
	MATERIAL proxyMaterial{};
	proxyMaterial.Ambient = Color(0.55f, 0.55f, 0.60f, 1.0f);
	proxyMaterial.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	proxyMaterial.Specular = Color(0.8f, 0.8f, 0.9f, 1.0f);
	proxyMaterial.Emission = Color(0.12f, 0.12f, 0.16f, 1.0f);
	proxyMaterial.Shiness = 32.0f;
	proxyMaterial.TextureEnable = FALSE;
	m_swordProxyMaterial.Create(proxyMaterial);

	// Optional player weapon attachment. The asset is intentionally discovered
	// here so the existing GameScene does not need another scene-specific draw call.
	std::filesystem::path swordPath("assets/model/Sword.fbx");
	if (!std::filesystem::exists(swordPath))
		swordPath = "../assets/model/Sword.fbx";
	if (std::filesystem::exists(swordPath))
	{
		m_swordMesh = std::make_unique<CStaticMesh>();
		// Sword.fbx is a static attachment. Apply its FBX node transforms so
		// the mesh is not left at an importer-local offset.
		m_swordMesh->Load(swordPath.string(), "assets/model/", true);
		m_swordRenderer.Init(*m_swordMesh);
		if (!m_swordMesh->GetVertices().empty())
		{
			Vector3 minPosition = m_swordMesh->GetVertices().front().Position;
			Vector3 maxPosition = minPosition;
			for (const auto& vertex : m_swordMesh->GetVertices())
			{
				minPosition = Vector3::Min(minPosition, vertex.Position);
				maxPosition = Vector3::Max(maxPosition, vertex.Position);
			}
			const Vector3 extent = maxPosition - minPosition;
			m_swordModelCenter = (minPosition + maxPosition) * 0.5f;
			const float swordLength = std::max({ extent.x, extent.y, extent.z, 0.001f });
			m_swordScale = playerExtent * 0.75f / swordLength;
		}
		int bestBoneScore = 0;
		for (const auto& [name, bone] : m_BoneDictionary)
		{
			const int score = SwordBonePriority(name);
			if (score > bestBoneScore)
			{
				bestBoneScore = score;
				m_swordBoneName = name;
			}
		}
		std::cout << "[Sword] vertices=" << m_swordMesh->GetVertices().size()
			<< " subsets=" << m_swordMesh->GetSubsets().size()
			<< " attachBone=" << m_swordBoneName
			<< " scale=" << m_swordScale << std::endl;
	}
	if (!m_swordDebugRegistered)
	{
		DebugUI::RedistDebugFunction([this]() { RenderSwordDebug(); });
		m_swordDebugRegistered = true;
	}

}

void CAnimationMesh::RenderSwordDebug()
{
	ImGui::SetNextWindowPos(ImVec2(900.0f, 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(370.0f, 330.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Sword Attachment Debug");
	ImGui::Checkbox("Draw sword", &m_swordEnabled);
	ImGui::Checkbox("Use guaranteed sword", &m_swordUseGuaranteedProxy);
	ImGui::Checkbox("Force test placement", &m_swordForceTestPlacement);
	ImGui::TextColored(
		m_swordProxyMesh ? ImVec4(0.25f, 1.0f, 0.35f, 1.0f) : ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
		m_swordProxyMesh ? "GUARANTEED SWORD: READY" : "GUARANTEED SWORD: FAILED");
	ImGui::DragFloat("Sword length", &m_swordProxyLength, 0.5f, 1.0f, 500.0f, "%.1f");
	ImGui::DragFloat3("Rotation XYZ", &m_swordRotationDegrees.x, 1.0f, -180.0f, 180.0f, "%.1f");
	if (m_swordForceTestPlacement)
		ImGui::DragFloat3("Test position", &m_swordTestPosition.x, 1.0f, -1000.0f, 1000.0f, "%.1f");
	else
		ImGui::DragFloat3("Hand offset", &m_swordHandOffset.x, 0.5f, -100.0f, 100.0f, "%.1f");
	ImGui::Text("Attach bone: %s", m_swordBoneName.empty() ? "NOT FOUND" : m_swordBoneName.c_str());
	if (ImGui::BeginCombo("Right hand bone", m_swordBoneName.empty() ? "NOT FOUND" : m_swordBoneName.c_str()))
	{
		std::vector<std::string> candidates;
		for (const auto& [name, bone] : m_BoneDictionary)
			if (SwordBonePriority(name) > 0) candidates.push_back(name);
		std::sort(candidates.begin(), candidates.end(), [](const std::string& a, const std::string& b) {
			return SwordBonePriority(a) > SwordBonePriority(b);
		});
		for (const std::string& name : candidates)
		{
			const bool selected = name == m_swordBoneName;
			if (ImGui::Selectable(name.c_str(), selected)) m_swordBoneName = name;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::Button("Show beside player"))
	{
		m_swordUseGuaranteedProxy = true;
		m_swordForceTestPlacement = true;
		m_swordTestPosition = Vector3(30.0f, 55.0f, 0.0f);
		m_swordRotationDegrees = Vector3(0.0f, 0.0f, -90.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Attach to right hand"))
	{
		m_swordUseGuaranteedProxy = true;
		m_swordForceTestPlacement = false;
	}
	if (!m_swordMesh)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "NOT LOADED");
		ImGui::Text("Expected: assets/model/Sword.fbx");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.35f, 1.0f), "LOADED");
		ImGui::Text("Vertices: %zu", m_swordMesh->GetVertices().size());
		ImGui::Text("Subsets: %zu", m_swordMesh->GetSubsets().size());
		ImGui::DragFloat("FBX scale", &m_swordScale, 0.01f, 0.001f, 1000.0f, "%.3f");
		ImGui::Text("Model center: %.1f, %.1f, %.1f",
			m_swordModelCenter.x, m_swordModelCenter.y, m_swordModelCenter.z);
	}
	ImGui::End();
}

// 階層構造を考慮したボーンコンビネーション行列を更新
void CAnimationMesh::UpdateBoneMatrix(
	CTreeNode<std::string>* ptree, 
	Matrix4x4 matrix)														// 20240714 DX化	
{
	// ノード名からボーン辞書を使ってボーン情報を取得
	BONE* bone = &m_BoneDictionary[ptree->m_nodedata];						// 20240714 DX化		

	Matrix4x4 bonecombination;												// 20240714 DX化；

	// ボーンオフセット行列×ボーンアニメメーション行列×逆ボーンオフセット行列
	bonecombination = bone->OffsetMatrix * bone->AnimationMatrix * matrix;	// 20240714 DX化
	bone->Matrix = bonecombination;											// 20240714 DX化

	// 自分の姿勢を表す行列を作成
	Matrix4x4 mybonemtx;													// 20240714 DX化
	mybonemtx = bone->AnimationMatrix * matrix;								// 20240714 DX化
	m_DebugBoneMatrices[ptree->m_nodedata] = mybonemtx;
	// 子ノードに対して再帰的に処理											// 20240714 DX化
	for (unsigned int n = 0; n < ptree->m_children.size(); n++)				// 20240714 DX化
	{																		// 20240714 DX化
		UpdateBoneMatrix(ptree->m_children[n].get(), mybonemtx);			// 20240714 DX化
	}																		// 20240714 DX化
}

// ローカルポーズ生成
void CAnimationMesh::BuildLocalPoseMap(
	const aiAnimation* animationdata,
	int& CurrentFrame,
	std::unordered_map<std::string, SRTQ>& localposemap)
{
	// アニメーションデータ取得
	const aiAnimation* animation = animationdata;

	// ボーン数分ループしてボーン行列を作成
	for (unsigned int c = 0; c < animation->mNumChannels; c++)
	{
		aiNodeAnim* nodeAnim = animation->mChannels[c];

		int f;

		f = CurrentFrame % nodeAnim->mNumRotationKeys;				//簡易実装
		aiQuaternion rot = nodeAnim->mRotationKeys[f].mValue;

		f = CurrentFrame % nodeAnim->mNumPositionKeys;				//簡易実装
		aiVector3D pos = nodeAnim->mPositionKeys[f].mValue;

		// assimp SRT=>DX版　SRT
		Vector3 s = { 1.0f,1.0f,1.0f };		// 20240714 DX化
		Vector3 t = { pos.x,pos.y,pos.z };	// 20240714 DX化
		Quaternion r{};						// 20240714 DX化

		r.x = rot.x;						// 20240714 DX化
		r.y = rot.y;						// 20240714 DX化
		r.z = rot.z;						// 20240714 DX化
		r.w = rot.w;						// 20240714 DX化

		SRTQ srtq;
		srtq.scale = s;
		srtq.pos = t;
		srtq.rot = Vector3(0, 0, 0);
		srtq.quat = r;

		localposemap[nodeAnim->mNodeName.C_Str()] = srtq;
	}
}

// アニメーションの更新
void CAnimationMesh::Update(BoneCombMatrix& bonecombarray,int& CurrentFrame)
{
	m_DebugBoneMatrices.clear();
	// アニメーションデータ取得
	aiAnimation* animation = m_CurrentAnimation;

	// ローカルポーズを生成
	std::unordered_map<std::string, SRTQ> localpose;
	BuildLocalPoseMap(
		m_CurrentAnimation,
		CurrentFrame,
		localpose);

	// localpose の中身を 1 件ずつ取り出す
	for (auto& pair : localpose) {
		// map の「キー（ボーン名）」と「値（SRTQデータ）」を明示的に取り出す
		const std::string& bonename = pair.first;   // ボーンの名前
		SRTQ& srtq = pair.second;                   // 位置・回転・スケールの情報

		// ノード名からボーン辞書を使ってassimpのボーン情報を取得
		BONE* bone = &m_BoneDictionary[bonename];

		Matrix4x4 scalemtx = Matrix4x4::CreateScale(srtq.scale);
		Matrix4x4 rotmtx = Matrix4x4::CreateFromQuaternion(srtq.quat);
		Matrix4x4 transmtx = Matrix4x4::CreateTranslation(srtq.pos);

		// ローカル座標からボーンのアニメーション行列を作成
		bone->AnimationMatrix = scalemtx * rotmtx * transmtx;
	}


	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);		// 20240714 DX化	

	// ボーンコンビネーション行列の配列をセット
	for (const auto& bone : m_BoneDictionary)
	{
		bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] = bone.second.Matrix.Transpose();	// 20240714 DX化
	}
}

void CAnimationMesh::UpdateManualPose(
	BoneCombMatrix& bonecombarray,
	const std::unordered_map<std::string, Matrix4x4>& localRotations)
{
	m_DebugBoneMatrices.clear();
	for (auto& [name, bone] : m_BoneDictionary)
	{
		auto rest = m_RestLocalMatrices.find(name);
		bone.AnimationMatrix = (rest != m_RestLocalMatrices.end())
			? rest->second
			: Matrix4x4::Identity;
	}

	for (const auto& [name, rotation] : localRotations)
	{
		auto bone = m_BoneDictionary.find(name);
		auto rest = m_RestLocalMatrices.find(name);
		if (bone != m_BoneDictionary.end() && rest != m_RestLocalMatrices.end())
		{
			// 頂点をボーンのローカル回転で動かしてから、Rest姿勢と親階層へ戻す。
			bone->second.AnimationMatrix = rotation * rest->second;
		}
	}

	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);
	for (const auto& [name, bone] : m_BoneDictionary)
	{
		if (bone.idx >= 0 && bone.idx < MAX_BONE)
		{
			bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.idx] = bone.Matrix.Transpose();
		}
	}
}

std::vector<std::string> CAnimationMesh::GetBoneNames() const
{
	std::vector<std::string> names;
	names.reserve(m_BoneDictionary.size());
	for (const auto& [name, bone] : m_BoneDictionary)
	{
		if (!name.empty() && bone.idx >= 0 && bone.idx < MAX_BONE)
		{
			names.push_back(name);
		}
	}
	return names;
}
