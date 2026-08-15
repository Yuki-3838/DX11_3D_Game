#include	<iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <array>
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

void CAnimationMesh::LoadSwordAttachmentPreset()
{
	if (m_swordPresetPath.empty())
		return;

	std::ifstream input(m_swordPresetPath);
	if (!input)
	{
		m_swordPresetStatus = "No saved preset (using defaults)";
		return;
	}

	std::string key;
	float value = 0.0f;
	while (input >> key >> value)
	{
		if (key == "offset_x") m_swordHandOffset.x = value;
		else if (key == "offset_y") m_swordHandOffset.y = value;
		else if (key == "offset_z") m_swordHandOffset.z = value;
		else if (key == "rotation_x") m_swordRotationDegrees.x = value;
		else if (key == "rotation_y") m_swordRotationDegrees.y = value;
		else if (key == "rotation_z") m_swordRotationDegrees.z = value;
		else if (key == "scale") m_swordScale = value;
	}
	m_swordPresetStatus = "Loaded: " + m_swordPresetPath;
}

void CAnimationMesh::SaveSwordAttachmentPreset()
{
	if (m_swordPresetPath.empty())
	{
		m_swordPresetStatus = "Cannot save: no sword asset path";
		return;
	}

	std::ofstream output(m_swordPresetPath, std::ios::trunc);
	if (!output)
	{
		m_swordPresetStatus = "Cannot save: file is not writable";
		return;
	}

	output << "# Fallen Paladin sword attachment preset\n"
		<< "offset_x " << m_swordHandOffset.x << "\n"
		<< "offset_y " << m_swordHandOffset.y << "\n"
		<< "offset_z " << m_swordHandOffset.z << "\n"
		<< "rotation_x " << m_swordRotationDegrees.x << "\n"
		<< "rotation_y " << m_swordRotationDegrees.y << "\n"
		<< "rotation_z " << m_swordRotationDegrees.z << "\n"
		<< "scale " << m_swordScale << "\n";
	m_swordPresetStatus = "Saved: " + m_swordPresetPath;
}

bool CAnimationMesh::BuildEmbeddedSwordWorldSegment(
	const Matrix4x4& parentWorld,
	Vector3& base,
	Vector3& tip) const
{
	if (m_embeddedSwordVertexIndices.size() < 2)
		return false;
	const auto swordBoneIt = m_BoneDictionary.find("mixamorig:Sword_joint");

	// The embedded subset has mixed/incorrect weights, so its CPU-deformed
	// vertices are not safe collision endpoints.  Use the same right-hand bone
	// that visually holds the weapon and a fitted local hand direction instead.
	const auto rightHandIt = m_DebugBoneMatrices.find("mixamorig:RightHand");
	if (rightHandIt != m_DebugBoneMatrices.end())
	{
		Vector3 localMin = m_vertices[m_embeddedSwordVertexIndices.front()].Position;
		Vector3 localMax = localMin;
		for (const uint32_t vertexIndex : m_embeddedSwordVertexIndices)
		{
			if (vertexIndex >= m_vertices.size())
				continue;
			localMin = Vector3::Min(localMin, m_vertices[vertexIndex].Position);
			localMax = Vector3::Max(localMax, m_vertices[vertexIndex].Position);
		}
		const Vector3 localExtent = localMax - localMin;
		const float localLength = std::max({ localExtent.x, localExtent.y, localExtent.z, 0.001f });
		const Matrix4x4 handWorld = rightHandIt->second * parentWorld;
		base = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handWorld);
		float worldLength = localLength * Vector3::TransformNormal(
			Vector3(0.0f, 0.0f, 1.0f), handWorld).Length();
		if (swordBoneIt != m_BoneDictionary.end())
		{
			Vector3 localAxis(0.0f, 0.0f, 0.0f);
			if (localExtent.x >= localExtent.y && localExtent.x >= localExtent.z)
				localAxis.x = localLength;
			else if (localExtent.y >= localExtent.z)
				localAxis.y = localLength;
			else
				localAxis.z = localLength;
			worldLength = Vector3::TransformNormal(
				localAxis, swordBoneIt->second.Matrix * parentWorld).Length();
		}
		// The visible sword is held sideways from the hand in the reference pose.
		// The former world-down correction produced the vertical OBB seen in the
		// debug view. Use the hand's local horizontal axis so the span rotates with
		// the character instead of staying vertical in world space.
		// Flip the horizontal axis to match the visible blade direction, then add
		// a small local-down component for the sword's natural diagonal pose.
		Vector3 bladeDirection = Vector3::TransformNormal(
			Vector3(1.0f, 0.18f, 0.0f), handWorld);
		if (bladeDirection.LengthSquared() <= 0.0001f)
			bladeDirection = Vector3(1.0f, 0.0f, 0.0f);
		else
			bladeDirection.Normalize();
		// The imported bounds include the guard/pommel and are longer than the
		// visible blade span from the gripping palm.  The previous 28% stopped
		// before the rendered tip; 45% reaches the actual sword endpoint while
		// keeping the right-hand grip as the segment origin.
		worldLength *= 0.45f;
		base = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handWorld);
		tip = base + bladeDirection * worldLength;
		// The hand joint is slightly above and toward the guard on this asset.
		// Shift both endpoints together so the OBB center sits on the visible
		// blade center; this is a center correction, not a length correction.
		const Vector3 centerOffset =
			-bladeDirection * (worldLength * 0.08f) +
			Vector3(0.0f, -worldLength * 0.12f, 0.0f);
		base += centerOffset;
		tip += centerOffset;
		return true;
	}

	if (swordBoneIt == m_BoneDictionary.end())
		return false;

	// Reproduce the skinning shader on the CPU, but select the hilt/tip from the
	// sword's *source* longitudinal axis. The asset's sword mesh has stray
	// character-bone weights; a PCA over the already-deformed cloud can therefore
	// choose a body/origin outlier as the blade tip. Selecting endpoint bands in
	// local mesh space keeps the collision segment on the same visible vertices
	// that the shader renders.
	std::array<const BONE*, MAX_BONE> bonesByIndex{};
	for (const auto& [name, bone] : m_BoneDictionary)
	{
		if (bone.idx >= 0 && bone.idx < MAX_BONE)
			bonesByIndex[bone.idx] = &bone;
	}
	Vector3 localMin{};
	Vector3 localMax{};
	bool hasVertex = false;
	for (const uint32_t vertexIndex : m_embeddedSwordVertexIndices)
	{
		if (vertexIndex >= m_vertices.size())
			continue;
		const Vector3 point = m_vertices[vertexIndex].Position;
		if (!hasVertex)
		{
			localMin = point;
			localMax = point;
			hasVertex = true;
		}
		else
		{
			localMin = Vector3::Min(localMin, point);
			localMax = Vector3::Max(localMax, point);
		}
	}
	if (!hasVertex)
		return false;
	const Vector3 localExtent = localMax - localMin;
	int axisIndex = 0;
	if (localExtent.y > localExtent.x && localExtent.y >= localExtent.z)
		axisIndex = 1;
	else if (localExtent.z > localExtent.x && localExtent.z > localExtent.y)
		axisIndex = 2;
	const float axisMin = axisIndex == 0 ? localMin.x : axisIndex == 1 ? localMin.y : localMin.z;
	const float axisMax = axisIndex == 0 ? localMax.x : axisIndex == 1 ? localMax.y : localMax.z;
	const float endpointBand = std::max((axisMax - axisMin) * 0.08f, 0.001f);

	const auto skinToWorld = [&](const VERTEX_3D& vertex)
	{
		Vector3 skinned{};
		float weightSum = 0.0f;
		for (int slot = 0; slot < 4; ++slot)
		{
			const int boneIndex = vertex.BoneIndex[slot];
			const float weight = vertex.BoneWeight[slot];
			if (boneIndex < 0 || boneIndex >= MAX_BONE || weight <= 0.0f)
				continue;
			const BONE* bone = bonesByIndex[boneIndex];
			if (!bone)
				continue;
			skinned += Vector3::Transform(vertex.Position, bone->Matrix) * weight;
			weightSum += weight;
		}
		if (weightSum > 0.0001f)
			skinned += vertex.Position * std::max(0.0f, 1.0f - weightSum);
		else
			skinned = vertex.Position;
		return Vector3::Transform(skinned, parentWorld);
	};
	Vector3 endpointMin{};
	Vector3 endpointMax{};
	int endpointMinCount = 0;
	int endpointMaxCount = 0;
	for (const uint32_t vertexIndex : m_embeddedSwordVertexIndices)
	{
		if (vertexIndex >= m_vertices.size())
			continue;
		const VERTEX_3D& vertex = m_vertices[vertexIndex];
		const float value = axisIndex == 0 ? vertex.Position.x : axisIndex == 1 ? vertex.Position.y : vertex.Position.z;
		const Vector3 worldPoint = skinToWorld(vertex);
		if (value <= axisMin + endpointBand)
		{
			endpointMin += worldPoint;
			++endpointMinCount;
		}
		if (value >= axisMax - endpointBand)
		{
			endpointMax += worldPoint;
			++endpointMaxCount;
		}
	}
	if (endpointMinCount == 0 || endpointMaxCount == 0)
		return false;
	endpointMin /= static_cast<float>(endpointMinCount);
	endpointMax /= static_cast<float>(endpointMaxCount);

	const auto handIt = m_DebugBoneMatrices.find("mixamorig:RightHand");
	const Vector3 hand = handIt != m_DebugBoneMatrices.end()
		? Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handIt->second * parentWorld)
		: endpointMin;
	base = (endpointMin - hand).LengthSquared() <= (endpointMax - hand).LengthSquared()
		? endpointMin
		: endpointMax;
	tip = (endpointMin - hand).LengthSquared() > (endpointMax - hand).LengthSquared()
		? endpointMin
		: endpointMax;
	return true;
}

void CAnimationMesh::UpdateSwordWorldTransform(const Matrix4x4& parentWorld)
{
	const bool drawProxy = m_swordUseGuaranteedProxy && m_swordProxyMesh;
	const bool drawFbx = !m_swordUseGuaranteedProxy && m_swordMesh;
	const bool collisionOnlyEmbeddedSword = m_swordEmbeddedInPlayerAsset;
	if ((!m_swordEnabled && !collisionOnlyEmbeddedSword) ||
		(!drawProxy && !drawFbx && !collisionOnlyEmbeddedSword))
	{
		m_swordWorldSegmentValid = false;
		return;
	}
	if (m_swordEmbeddedInPlayerAsset)
	{
		Vector3 nextBase{};
		Vector3 nextTip{};
		if (!BuildEmbeddedSwordWorldSegment(parentWorld, nextBase, nextTip))
		{
			m_swordWorldSegmentValid = false;
			return;
		}
		m_swordPreviousWorldTip = m_swordWorldSegmentValid ? m_swordWorldTip : nextTip;
		m_swordWorldBase = nextBase;
		m_swordWorldTip = nextTip;
		m_swordWorldSegmentValid = true;
		if (!m_swordTransformLogged)
		{
			const auto handIt = m_DebugBoneMatrices.find("mixamorig:RightHand");
			const Vector3 rightHand = handIt != m_DebugBoneMatrices.end()
				? Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handIt->second * parentWorld)
				: Vector3(0.0f, 0.0f, 0.0f);
			std::cout << "[Sword] embedded CPU segment base="
				<< nextBase.x << "," << nextBase.y << "," << nextBase.z
				<< " tip=" << nextTip.x << "," << nextTip.y << "," << nextTip.z
				<< " rightHand=" << rightHand.x << "," << rightHand.y << "," << rightHand.z
				<< std::endl;
			m_swordTransformLogged = true;
		}
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
	const Matrix4x4 swordLocal = centerCorrection * sizeMatrix *
		Matrix4x4::CreateFromYawPitchRoll(
			rotationRadians.y, rotationRadians.x, rotationRadians.z) *
		Matrix4x4::CreateTranslation(
			placement.x, placement.y, placement.z);

	m_swordWorldMatrix = swordLocal * boneMatrix * parentWorld;
	const float halfLength = std::max(m_swordLocalHalfLength, 0.001f);
	const Vector3 localBase = m_swordUsesPlayerAsset
		// swordLocal first subtracts m_swordModelCenter from every vertex;
		// add it back here so the collision endpoints use the same centered
		// coordinate system as the visible sword mesh.
		? m_swordModelCenter + m_swordCollisionLocalAxis * halfLength
		: Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 localTip = m_swordUsesPlayerAsset
		? m_swordModelCenter - m_swordCollisionLocalAxis * halfLength
		: (drawProxy ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.5f, 0.0f));
	const Vector3 nextBase = Vector3::Transform(
		localBase, m_swordWorldMatrix);
	const Vector3 nextTip = Vector3::Transform(
		localTip,
		m_swordWorldMatrix);
	if (!m_swordTransformLogged)
	{
		const Matrix4x4 handWorld = boneMatrix * parentWorld;
		const Vector3 handOrigin = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handWorld);
		const Vector3 handAxisX = Vector3::TransformNormal(Vector3(1.0f, 0.0f, 0.0f), handWorld);
		const Vector3 handAxisY = Vector3::TransformNormal(Vector3(0.0f, 1.0f, 0.0f), handWorld);
		const Vector3 handAxisZ = Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), handWorld);
		std::cout << "[Sword] handOrigin=" << handOrigin.x << "," << handOrigin.y << "," << handOrigin.z
			<< " axesX=" << handAxisX.x << "," << handAxisX.y << "," << handAxisX.z
			<< " axesY=" << handAxisY.x << "," << handAxisY.y << "," << handAxisY.z
			<< " axesZ=" << handAxisZ.x << "," << handAxisZ.y << "," << handAxisZ.z
			<< " base=" << nextBase.x << "," << nextBase.y << "," << nextBase.z
			<< " tip=" << nextTip.x << "," << nextTip.y << "," << nextTip.z
			<< " offset=" << m_swordHandOffset.x << "," << m_swordHandOffset.y << "," << m_swordHandOffset.z
			<< std::endl;
		m_swordTransformLogged = true;
	}
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
	m_RestGlobalMatrices.clear();

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

	// glTF inverse-bind matrices use a different import convention than the
	// legacy PMX/FBX path. Rebuild them from the rest hierarchy so the bind pose
	// is stable: finalBone = inverse(restGlobal) * currentGlobal.
	const std::string extension = std::filesystem::path(filename).extension().string();
	if (extension == ".gltf" || extension == ".glb")
	{
		const auto buildRestGlobals = [&](auto&& self,
			CTreeNode<std::string>* node,
			const Matrix4x4& parent) -> void
		{
			const auto local = m_RestLocalMatrices.find(node->m_nodedata);
			const Matrix4x4 localMatrix = local != m_RestLocalMatrices.end()
				? local->second
				: Matrix4x4::Identity;
			const Matrix4x4 global = localMatrix * parent;
			m_RestGlobalMatrices[node->m_nodedata] = global;
			for (auto& child : node->m_children)
				self(self, child.get(), global);
		};
		buildRestGlobals(buildRestGlobals, &m_AssimpNodeNameTree, Matrix4x4::Identity);

		for (auto& [name, bone] : m_BoneDictionary)
		{
			if (bone.weights.empty())
				continue;
			const auto restGlobal = m_RestGlobalMatrices.find(name);
			if (restGlobal != m_RestGlobalMatrices.end())
				bone.OffsetMatrix = restGlobal->second.Invert();
		}
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

	// Optional player weapon attachment. Fallen Paladin's original FBX contains
	// a separate loose sword component, so use the extracted, textured version
	// for this model instead of the generic debug sword/proxy.
	const bool usesEmbeddedSword = filename.find("SwordShieldPack_Player") != std::string::npos;
	m_swordUsesPlayerAsset = filename.find("FallenPaladin_Player") != std::string::npos;
	if (usesEmbeddedSword)
	{
		// The pack model already contains a skinned sword under Sword_joint.
		// Do not load/draw a second loose sword; the embedded sword is rendered
		// together with the character and its joint is used for collision.
		m_swordEmbeddedInPlayerAsset = true;
		m_swordEnabled = false;
		m_swordUsesPlayerAsset = false;
		m_swordAssetPath = "embedded:mixamorig:Sword_joint";
		m_swordBoneName = "mixamorig:Sword_joint";
		m_embeddedSwordVertexIndices.clear();
		for (const SUBSET& subset : GetSubsets())
		{
			std::string meshName = subset.MeshName;
			std::transform(meshName.begin(), meshName.end(), meshName.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (meshName.find("sword") == std::string::npos &&
				meshName.find("weapon") == std::string::npos)
				continue;
			for (unsigned int i = 0; i < subset.VertexNum; ++i)
				m_embeddedSwordVertexIndices.push_back(subset.VertexBase + i);
			std::cout << "[Sword] embedded mesh=" << subset.MeshName
				<< " vertices=" << subset.VertexNum << std::endl;
		}
		m_swordWorldSegmentValid = false;
		if (!m_swordDebugRegistered)
		{
			DebugUI::RedistDebugFunction([this]() { RenderSwordDebug(); });
			m_swordDebugRegistered = true;
		}
		return;
	}
	std::filesystem::path swordPath = m_swordUsesPlayerAsset
		? std::filesystem::path("assets/model/FallenPaladin/runtime/FallenPaladin_Sword.glb")
		: std::filesystem::path("assets/model/Sword.fbx");
	if (!std::filesystem::exists(swordPath))
	{
		swordPath = m_swordUsesPlayerAsset
			? std::filesystem::path("../assets/model/FallenPaladin/runtime/FallenPaladin_Sword.glb")
			: std::filesystem::path("../assets/model/Sword.fbx");
	}
	if (std::filesystem::exists(swordPath))
	{
		m_swordAssetPath = swordPath.string();
		// Always use an absolute path. Visual Studio and a double-clicked EXE can
		// have different current directories, which previously made Save appear
		// to work while the next launch loaded a different relative file.
		m_swordPresetPath = std::filesystem::absolute(swordPath).string() + ".attach.txt";
		m_swordMesh = std::make_unique<CStaticMesh>();
		// Apply importer node transforms so the extracted sword is not left at
		// an importer-local offset.
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
			if (extent.x >= extent.y && extent.x >= extent.z)
				m_swordCollisionLocalAxis = Vector3(1.0f, 0.0f, 0.0f);
			else if (extent.y >= extent.z)
				m_swordCollisionLocalAxis = Vector3(0.0f, 1.0f, 0.0f);
			else
				m_swordCollisionLocalAxis = Vector3(0.0f, 0.0f, 1.0f);
			m_swordScale = playerExtent * 0.75f / swordLength;
			if (m_swordUsesPlayerAsset)
			{
				// The extracted asset already uses the player's 2.9-unit scale.
				// Center it on the hand, then move its grip (the upper end of the
				// source sword) to the hand by roughly half of its length.
				m_swordScale = 1.0f;
				m_swordLocalHalfLength = swordLength * 0.5f;
				// The source sword's local Z axis follows the forearm diagonally in
				// the idle pose.  Rotate around local Y so the held weapon points
				// downward instead of lying across the body.
				m_swordRotationDegrees = Vector3(81.50f, -46.50f, 76.75f);
				// The runtime rig's hand.r is fitted to the visible palm.  The sword
				// is centered on its bounds, and the grip needs a small +local-Z
				// placement in this renderer's row-vector convention to sit inside
				// that palm.  Keep this model-specific value proportional to the
				// imported sword length.  Rendering and collision use the same matrix.
				// Measured from FallenPaladin_Sword.glb after subtracting its bounds
				// center.  It is the centroid of the handle/pommel region (source
				// local Z > 1.45), so the automatic fit targets the grip itself.
				m_swordGripLocalPoint = Vector3(-0.0484f, 0.0094f, 0.7171f);
				// Keep the empirically fitted hand placement.  The grip-point
				// auto-correction is intentionally not applied here because the
				// imported DirectX row-vector transform already includes the model's
				// node basis; applying that correction a second time moves the sword
				// away from the hand.
				// The current hand pose intersects the blade center.  Move the
				// centered mesh toward its hilt along the attachment's local Z so
				// the grip/guard, rather than the blade midpoint, is in the palm.
				m_swordHandOffset = Vector3(0.650f, -0.070f, 0.646f);
				m_swordUseGuaranteedProxy = false;
			}
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
	LoadSwordAttachmentPreset();

}

void CAnimationMesh::RenderSwordDebug()
{
	ImGui::SetNextWindowPos(ImVec2(900.0f, 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(430.0f, 470.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Sword Attachment Debug");
	ImGui::Checkbox("Draw sword", &m_swordEnabled);
	ImGui::Checkbox("Use guaranteed sword", &m_swordUseGuaranteedProxy);
	ImGui::Checkbox("Force test placement", &m_swordForceTestPlacement);
	ImGui::TextColored(
		m_swordUsesPlayerAsset && m_swordMesh
			? ImVec4(0.25f, 1.0f, 0.35f, 1.0f)
			: (m_swordProxyMesh ? ImVec4(0.25f, 1.0f, 0.35f, 1.0f) : ImVec4(1.0f, 0.25f, 0.25f, 1.0f)),
		m_swordUsesPlayerAsset && m_swordMesh
			? "MODEL SWORD: EQUIPPED"
			: (m_swordProxyMesh ? "GUARANTEED SWORD: READY" : "GUARANTEED SWORD: FAILED"));
	ImGui::Text("Asset: %s", m_swordAssetPath.empty() ? "NOT FOUND" : m_swordAssetPath.c_str());
	ImGui::SeparatorText("LIVE FIT - changes apply immediately");
	ImGui::TextWrapped("Drag the values while watching the character. Save attachment keeps the fit for the next launch.");
	ImGui::DragFloat3("Rotation XYZ (deg)", &m_swordRotationDegrees.x, 0.25f, -180.0f, 180.0f, "%.2f");
	if (m_swordForceTestPlacement)
		ImGui::DragFloat3("Test position", &m_swordTestPosition.x, 0.05f, -1000.0f, 1000.0f, "%.3f");
	else
		ImGui::DragFloat3("Hand offset (local)", &m_swordHandOffset.x, 0.01f, -10.0f, 10.0f, "%.3f");
	ImGui::DragFloat("Model scale", &m_swordScale, 0.005f, 0.001f, 1000.0f, "%.3f");
	if (ImGui::Button("Save attachment"))
		SaveSwordAttachmentPreset();
	ImGui::SameLine();
	if (ImGui::Button("Load saved"))
		LoadSwordAttachmentPreset();
	ImGui::SameLine();
	if (ImGui::Button("Reset default"))
	{
		m_swordRotationDegrees = Vector3(81.50f, -46.50f, 76.75f);
		m_swordHandOffset = Vector3(0.650f, -0.070f, 0.646f);
		m_swordScale = 1.0f;
		m_swordPresetStatus = "Reset in memory (press Save attachment to keep it)";
	}
	ImGui::TextWrapped("Preset: %s", m_swordPresetStatus.c_str());
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
		ImGui::Text("Asset: %s", m_swordAssetPath.empty() ? "NOT FOUND" : m_swordAssetPath.c_str());
	}
	else
	{
		ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.35f, 1.0f), "LOADED");
		ImGui::Text("Vertices: %zu", m_swordMesh->GetVertices().size());
		ImGui::Text("Subsets: %zu", m_swordMesh->GetSubsets().size());
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
