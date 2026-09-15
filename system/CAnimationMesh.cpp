#include	<iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <array>
#include <cmath>
#include <limits>
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

	// ローカルボーン行列をSRTとして補間する。行ベクトル規約では
	// 回転行列の各行の長さが各軸のスケールになる。
	// 回転だけを取り出して再構成すると、バインド姿勢に含まれる
	// ボーン固有スケールが消え、攻撃開始時にモデルの一部が拡大・縮小する。
	Matrix4x4 BlendLocalSrt(
		const Matrix4x4& from,
		const Matrix4x4& to,
		float amount)
	{
		const float rate = std::clamp(amount, 0.0f, 1.0f);
		if (rate <= 0.0f)
			return from;
		if (rate >= 1.0f)
			return to;

		const auto extractScale = [](const Matrix4x4& matrix)
		{
			return Vector3(
				Vector3(matrix._11, matrix._12, matrix._13).Length(),
				Vector3(matrix._21, matrix._22, matrix._23).Length(),
				Vector3(matrix._31, matrix._32, matrix._33).Length());
		};
		const auto removeScale = [&extractScale](const Matrix4x4& matrix)
		{
			Matrix4x4 rotation = matrix;
			const Vector3 scale = extractScale(matrix);
			const float sx = std::max(scale.x, 0.0001f);
			const float sy = std::max(scale.y, 0.0001f);
			const float sz = std::max(scale.z, 0.0001f);
			rotation._11 /= sx; rotation._12 /= sx; rotation._13 /= sx;
			rotation._21 /= sy; rotation._22 /= sy; rotation._23 /= sy;
			rotation._31 /= sz; rotation._32 /= sz; rotation._33 /= sz;
			rotation._41 = 0.0f;
			rotation._42 = 0.0f;
			rotation._43 = 0.0f;
			rotation._44 = 1.0f;
			return rotation;
		};

		const Vector3 fromScale = extractScale(from);
		const Vector3 toScale = extractScale(to);
		const Quaternion fromRotation =
			Quaternion::CreateFromRotationMatrix(removeScale(from));
		const Quaternion toRotation =
			Quaternion::CreateFromRotationMatrix(removeScale(to));
		const Quaternion rotation = Quaternion::Slerp(fromRotation, toRotation, rate);
		const Vector3 scale = Vector3::Lerp(fromScale, toScale, rate);
		const Vector3 position = Vector3::Lerp(
			Vector3(from._41, from._42, from._43),
			Vector3(to._41, to._42, to._43),
			rate);
		return Matrix4x4::CreateScale(scale) *
			Matrix4x4::CreateFromQuaternion(rotation) *
			Matrix4x4::CreateTranslation(position);
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

	// 内蔵メッシュの一部はウェイトが混在・不正なため、CPU変形後の頂点を
	// 攻撃判定の端点に使うと表示とずれる。見た目で武器を持つ右手ボーンと、
	// 調整済みの手元ローカル方向から端点を求める。
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
		// 基準姿勢では見た目の剣が手から横向きに伸びている。
		// ワールド下方向の補正ではデバッグOBBが縦になったため、手のローカル横軸を使い、
		// キャラクターと一緒に回転するようにする。
		// 見た目の剣先方向へ軸を反転し、自然な斜め姿勢のため少しだけローカル下方向を加える。
		Vector3 bladeDirection = Vector3::TransformNormal(
			Vector3(1.0f, 0.18f, 0.0f), handWorld);
		if (bladeDirection.LengthSquared() <= 0.0001f)
			bladeDirection = Vector3(1.0f, 0.0f, 0.0f);
		else
			bladeDirection.Normalize();
		// 読み込み境界には鍔や柄尻が含まれ、手から見える刃の範囲より長い。
		// 以前の28%では剣先まで届かなかったため45%まで伸ばし、
		// 右手の握りを線分の始点として実際の剣先へ届かせる。
		worldLength *= 0.45f;
		base = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), handWorld);
		tip = base + bladeDirection * worldLength;
		// このモデルでは手の関節が鍔より少し上にある。
		// OBBの中心が見た目の刃の中心に来るよう両端を同じ量だけ移動する。
		// これは長さではなく中心位置の補正である。
		const Vector3 centerOffset =
			-bladeDirection * (worldLength * 0.08f) +
			Vector3(0.0f, -worldLength * 0.12f, 0.0f);
		base += centerOffset;
		tip += centerOffset;
		return true;
	}

	if (swordBoneIt == m_BoneDictionary.end())
		return false;

	// スキニングシェーダーの変形をCPUで再現するが、柄と剣先は剣の
	// 「元メッシュの長手方向」から選ぶ。剣メッシュにはキャラクター側の余計な
	// ボーンウェイトがあるため、変形後の頂点群へPCAを行うと胴体の外れ値を
	// 剣先と誤認する可能性がある。ローカルメッシュの端部帯から選ぶことで、
	// シェーダーが描画する頂点と同じ部分に攻撃線分を置く。
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
		// swordLocalでは各頂点からm_swordModelCenterを先に引いているため、
		// 見た目の剣メッシュと同じ中心座標系になるよう、ここで中心を戻す。
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

	// glTFの逆バインド行列は従来のPMX/FBX経路と読み込み規約が異なる。
	// バインド姿勢を安定させるため休止階層から再構築する。
	// 計算式はfinalBone = inverse(restGlobal) * currentGlobalである。
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
	if (filename.find("SwordShieldPack_Player") != std::string::npos)
	{
		// 出力された装備名が入れ替わっており、Helmetというサブセットが大きな盾形、
		// Shieldが兜の外殻になっている。兜用関節のスケールを0にせず、
		// 描画時に盾サブセットだけを隠して攻撃中に復活しないようにする。
		if (!m_StaticMeshRenderer.SetSubsetVisibleByKeyword(
			"Paladin_J_Nordstrom_Helmet", false))
			std::cout << "[Player] shield subset not found" << std::endl;
	}

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

	// プレイヤー武器の追加接続処理。Fallen Paladinの元FBXには分離した剣が含まれるため、
	// 汎用デバッグ剣ではなく、抽出済みでテクスチャ付きのモデルを使う。
	const bool usesEmbeddedSword = filename.find("SwordShieldPack_Player") != std::string::npos;
	m_swordUsesPlayerAsset = filename.find("FallenPaladin_Player") != std::string::npos;
	if (usesEmbeddedSword)
	{
		// パックモデルにはSword_joint配下へスキニング済みの剣が含まれる。
		// 分離した剣を二重に読み込まず、キャラクターと一緒に描画される内蔵剣の
		// 関節を攻撃判定にも使う。
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
		// 常に絶対パスを使う。Visual Studio実行とEXEのダブルクリックでは
		// 作業フォルダが異なる場合があり、相対パスだと保存できたように見えて
		// 次回起動時に別ファイルを読み込むことがある。
		m_swordPresetPath = std::filesystem::absolute(swordPath).string() + ".attach.txt";
		m_swordMesh = std::make_unique<CStaticMesh>();
		// 読み込みノードの変換を適用し、抽出した剣が読み込み元のローカル位置に残らないようにする。
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
				// 抽出した剣はプレイヤー用の2.9単位で作られている。
				// 剣を手の中心に置いた後、柄側の端が手に来るよう長さの半分ほど移動する。
				m_swordScale = 1.0f;
				m_swordLocalHalfLength = swordLength * 0.5f;
				// 元剣のローカルZ軸はアイドル姿勢で前腕に対して斜めになっている。
				// ローカルY回転を加え、胴体を横切らず下向きに持たせる。
				m_swordRotationDegrees = Vector3(81.50f, -46.50f, 76.75f);
				// 実行時リグのhand.rは見た目の手のひらへ合わせている。
				// 剣は境界中心を基準にしているため、この描画器の行ベクトル規約では
				// 握りを手のひらへ入れるため少し+ローカルZへ置く。
				// モデル固有値だが、読み込んだ剣の長さに比例させ、描画と攻撃判定で同じ行列を使う。
				// 剣の境界中心を引いた後の柄・柄尻領域の重心から測定した値である。
				m_swordGripLocalPoint = Vector3(-0.0484f, 0.0094f, 0.7171f);
				// 実際に合わせた手の位置を維持する。
				// DirectXの行ベクトル変換にはモデルのノード基底が含まれるため、自動補正を
				// もう一度適用すると剣が手から離れてしまう。
				// 現在の手姿勢では手のひらが刃の中心と重なるため、接続ローカルZ方向へ
				// 柄側を寄せ、刃の中央ではなく握り・鍔が手のひらに来るようにする。
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
	std::unordered_map<std::string, SRTQ>& localposemap,
	float frameFraction, bool loopAnimation)
{
	// アニメーションデータ取得
	const aiAnimation* animation = animationdata;

	// ボーン数分ループしてボーン行列を作成
	for (unsigned int c = 0; c < animation->mNumChannels; c++)
	{
		aiNodeAnim* nodeAnim = animation->mChannels[c];

		if (!nodeAnim || nodeAnim->mNumRotationKeys == 0 || nodeAnim->mNumPositionKeys == 0)
			continue;
		const auto keyIndex = [&](unsigned int count, unsigned int offset) {
			const unsigned int frame = static_cast<unsigned int>(std::max(CurrentFrame, 0));
			return loopAnimation ? (frame + offset) % count : std::min(frame + offset, count - 1);
		};
		const float blend = std::clamp(frameFraction, 0.0f, 1.0f);
		aiQuaternion rot;
		aiQuaternion::Interpolate(rot,
			nodeAnim->mRotationKeys[keyIndex(nodeAnim->mNumRotationKeys, 0)].mValue,
			nodeAnim->mRotationKeys[keyIndex(nodeAnim->mNumRotationKeys, 1)].mValue, blend);
		rot.Normalize();
		// 敵のワールド位置と接地はGameScene側のSRTで管理している。
		// 位置キーまで補間すると、ルートを含むアニメーションの上下移動が
		// 描画SRTへ重なり、敵が地面へ埋まることがある。位置は従来どおり
		// 現在のキーから読み、今回の補間対象は回転だけに限定する。
		const aiVector3D pos =
			nodeAnim->mPositionKeys[keyIndex(nodeAnim->mNumPositionKeys, 0)].mValue;

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
void CAnimationMesh::Update(BoneCombMatrix& bonecombarray,int& CurrentFrame,
	float frameFraction, bool loopAnimation)
{
	m_DebugBoneMatrices.clear();
	// アニメーションデータ取得
	aiAnimation* animation = m_CurrentAnimation;

	// ローカルポーズを生成
	std::unordered_map<std::string, SRTQ> localpose;
	BuildLocalPoseMap(
		m_CurrentAnimation,
		CurrentFrame,
		localpose, frameFraction, loopAnimation);

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

void CAnimationMesh::UpdateBlendedRotationAnimation(
	BoneCombMatrix& bonecombarray,
	const aiAnimation* fromAnimation,
	int fromFrame,
	float fromFrameFraction,
	bool loopFromAnimation,
	const aiAnimation* toAnimation,
	int toFrame,
	float toFrameFraction,
	bool loopToAnimation,
	float blendRate)
{
	m_DebugBoneMatrices.clear();

	std::unordered_map<std::string, SRTQ> fromPose;
	std::unordered_map<std::string, SRTQ> toPose;
	if (fromAnimation != nullptr)
	{
		BuildLocalPoseMap(
			fromAnimation,
			fromFrame,
			fromPose,
			fromFrameFraction,
			loopFromAnimation);
	}
	if (toAnimation != nullptr)
	{
		BuildLocalPoseMap(
			toAnimation,
			toFrame,
			toPose,
			toFrameFraction,
			loopToAnimation);
	}

	const float rate = std::clamp(blendRate, 0.0f, 1.0f);
	std::unordered_map<std::string, SRTQ> blendedPose;
	blendedPose.reserve(std::max(fromPose.size(), toPose.size()));

	// to側にあるボーンを基準にし、位置・スケールは補間せずto側を使う。
	// 敵のワールド移動や接地はSRT側が管理するため、ここで位置を混ぜない。
	for (const auto& [boneName, toSrtq] : toPose)
	{
		SRTQ blended = toSrtq;
		const auto fromIt = fromPose.find(boneName);
		if (fromIt != fromPose.end())
			blended.quat = Quaternion::Slerp(fromIt->second.quat, toSrtq.quat, rate);
		blendedPose.emplace(boneName, blended);
	}

	// to側にキーが無いボーンはfrom側を保持し、姿勢が欠けて原点へ戻らないようにする。
	for (const auto& [boneName, fromSrtq] : fromPose)
	{
		if (blendedPose.find(boneName) == blendedPose.end())
			blendedPose.emplace(boneName, fromSrtq);
	}

	for (const auto& [boneName, srtq] : blendedPose)
	{
		auto boneIt = m_BoneDictionary.find(boneName);
		if (boneIt == m_BoneDictionary.end())
			continue;

		Matrix4x4 scalemtx = Matrix4x4::CreateScale(srtq.scale);
		Matrix4x4 rotmtx = Matrix4x4::CreateFromQuaternion(srtq.quat);
		Matrix4x4 transmtx = Matrix4x4::CreateTranslation(srtq.pos);
		boneIt->second.AnimationMatrix = scalemtx * rotmtx * transmtx;
	}

	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);
	for (const auto& bone : m_BoneDictionary)
		bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] =
			bone.second.Matrix.Transpose();
}

float CAnimationMesh::GetAnimatedLocalMaxZ() const
{
	// 追加のピッチが無ければ、最下点はローカルZの最大値そのもの。
	return -GetAnimatedLowestLocalHeight(0.0f);
}

float CAnimationMesh::GetAnimatedLowestLocalHeight(float extraPitchRadians) const
{
	if (m_vertices.empty())
		return 0.0f;

	// 行ベクトル規約でX軸へθ回転すると、ワールドYは y*cosθ - z*sinθ になる。
	// 基準姿勢のθ=90度では -z となり、Z最大の頂点が最下点になる(従来の実装)。
	// θ=90度+pのときは cos=-sin(p)、sin=cos(p) なので、高さは -y*sin(p) - z*cos(p)。
	const float sinPitch = std::sin(extraPitchRadians);
	const float cosPitch = std::cos(extraPitchRadians);

	std::array<const BONE*, MAX_BONE> bonesByIndex{};
	for (const auto& [name, bone] : m_BoneDictionary)
	{
		(void)name;
		if (bone.idx >= 0 && bone.idx < MAX_BONE)
			bonesByIndex[bone.idx] = &bone;
	}

	float lowestHeight = (std::numeric_limits<float>::max)();
	bool hasVertex = false;
	for (const VERTEX_3D& vertex : m_vertices)
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
			if (bone == nullptr)
				continue;

			skinned += Vector3::Transform(vertex.Position, bone->Matrix) * weight;
			weightSum += weight;
		}

		if (weightSum > 0.0001f)
		{
			// GPU側と同じく、4ウェイトへ収まらなかった残りは恒等変換で補う。
			if (weightSum < 1.0f)
				skinned += vertex.Position * (1.0f - weightSum);
		}
		else
		{
			skinned = vertex.Position;
		}

		const float height = -skinned.y * sinPitch - skinned.z * cosPitch;
		lowestHeight = std::min(lowestHeight, height);
		hasVertex = true;
	}

	return hasVertex ? lowestHeight : 0.0f;
}

void CAnimationMesh::ApplyAnimationToBones(
	aiAnimation* animationdata,
	int currentFrame,
	float frameFraction,
	const std::vector<std::string>& animatedBoneNames,
	bool loopAnimation,
	const std::string& translationBone)
{
	const auto isAnimatedBone = [&animatedBoneNames](const std::string& name)
	{
		return std::find(animatedBoneNames.begin(), animatedBoneNames.end(), name) !=
			animatedBoneNames.end();
	};

	if (animationdata != nullptr)
	{
		// FBX出力ではボーンごとのキー数が一致しないことがある。
		// 剣モーションでは前腕のキーが胴体より多く、同じ生キー番号を使うと
		// 腕が動いている間に胴体だけ先にループするため、クリップ内の正規化時間で
		// 全チャンネルを同じ位置からサンプリングする。
		unsigned int maxRotationKeys = 0;
		for (unsigned int c = 0; c < animationdata->mNumChannels; ++c)
			maxRotationKeys = std::max(
				maxRotationKeys,
				animationdata->mChannels[c]->mNumRotationKeys);
		const unsigned int sampledFrame = maxRotationKeys > 0 && loopAnimation
			? static_cast<unsigned int>(std::max(currentFrame, 0)) % maxRotationKeys
			: static_cast<unsigned int>(std::max(currentFrame, 0));
		// キー番号の小数部を足してからクリップ内の正規化時間を求める。
		// これによりキーとキーの中間の時刻を表現でき、下でslerp補間できる。
		const float sampledPosition =
			static_cast<float>(sampledFrame) + std::clamp(frameFraction, 0.0f, 1.0f);
		const float normalizedFrame = maxRotationKeys > 1
			? std::clamp(
				sampledPosition / static_cast<float>(maxRotationKeys - 1),
				0.0f,
				1.0f)
			: 0.0f;
		for (unsigned int c = 0; c < animationdata->mNumChannels; ++c)
		{
			aiNodeAnim* nodeAnim = animationdata->mChannels[c];
			const std::string boneName = nodeAnim->mNodeName.C_Str();
			if (!isAnimatedBone(boneName) || nodeAnim->mNumRotationKeys == 0)
				continue;

			auto boneIt = m_BoneDictionary.find(boneName);
			if (boneIt == m_BoneDictionary.end())
				continue;

			// キー間をslerpで補間する。
			// 以前は最も近いキーへ丸めていたため、元データ(約30fps)のキーが
			// そのまま段階的に切り替わり、60Hz以上の描画では明確にカクついて見えていた。
			// 球面線形補間にすることで、キーの間の姿勢を連続的に作る。
			aiQuaternion rotation = nodeAnim->mRotationKeys[0].mValue;
			if (nodeAnim->mNumRotationKeys > 1)
			{
				const float keyPosition =
					normalizedFrame * static_cast<float>(nodeAnim->mNumRotationKeys - 1);
				const unsigned int keyIndex = std::min(
					nodeAnim->mNumRotationKeys - 2,
					static_cast<unsigned int>(std::max(0.0f, std::floor(keyPosition))));
				const float blend = std::clamp(
					keyPosition - static_cast<float>(keyIndex), 0.0f, 1.0f);
				// aiQuaternion::Interpolateは最短経路のslerpを行い、
				// q と -q の符号違い(反対称性)も内部で処理してくれる。
				aiQuaternion::Interpolate(
					rotation,
					nodeAnim->mRotationKeys[keyIndex].mValue,
					nodeAnim->mRotationKeys[keyIndex + 1].mValue,
					blend);
			}
			Quaternion quaternion{};
			quaternion.x = rotation.x;
			quaternion.y = rotation.y;
			quaternion.z = rotation.z;
			quaternion.w = rotation.w;
			const Matrix4x4 rotationMatrix = Matrix4x4::CreateFromQuaternion(quaternion);

			// モデルのバインド姿勢の平行移動を維持する。
			// ワールド移動はシーン側が管理するため、ルート移動を読み込むと二重移動になる。
			const auto rest = m_RestLocalMatrices.find(boneName);
			if (rest != m_RestLocalMatrices.end())
		{
				Vector3 restPosition(rest->second._41,
					rest->second._42, rest->second._43);

				// 腰など、指定されたボーンは「平行移動だけ」をクリップから取り込む。
				//
				// 回転まで取り込むと体全体が傾く。腰の回転はキャラクターの向きそのもので、
				// クリップ側の基準姿勢とモデルの休止姿勢が一致しないため、
				// そのまま適用すると寝転んだような姿勢になる(実機で確認済み)。
				// 欲しいのは沈み込みだけなので、回転は休止姿勢のまま維持する。
				//
				// 平行移動も絶対座標ではなく「クリップ先頭からの相対オフセット」にする。
				// ワールド移動はゲーム側(SRT)が管理しているため、
				// 絶対座標を入れると二重移動になる。
				// この沈み込みが無いと、腰が下がる前提で作られた脚の角度だけが適用され、
				// 脚が横へ開いて座り込んだ姿勢になる。
				const bool translationOnlyBone =
					!translationBone.empty() && boneName == translationBone;
				if (translationOnlyBone && nodeAnim->mNumPositionKeys > 1)
				{
					const unsigned int positionKeyCount = nodeAnim->mNumPositionKeys;
					const float positionKeyPosition =
						normalizedFrame * static_cast<float>(positionKeyCount - 1);
					const unsigned int positionIndex = std::min(
						positionKeyCount - 2,
						static_cast<unsigned int>(
							std::max(0.0f, std::floor(positionKeyPosition))));
					const float positionBlend = std::clamp(
						positionKeyPosition - static_cast<float>(positionIndex),
						0.0f, 1.0f);
					const aiVector3D a = nodeAnim->mPositionKeys[positionIndex].mValue;
					const aiVector3D b = nodeAnim->mPositionKeys[positionIndex + 1].mValue;
					const aiVector3D sampled = a + (b - a) * positionBlend;
					const aiVector3D origin = nodeAnim->mPositionKeys[0].mValue;

					// 上下方向だけを使う。
					// 前後左右の移動はゲーム側(SRT)が管理しているので、
					// 取り込むと足が滑ったり、その場でずれていったりする。
					//
					// さらに、クリップとモデルで長さの単位が違う(FBXはcm単位のことが多い)。
					// 差分をそのまま足すと過大になり、キャラクターが地面へ埋まっていく。
					// そこで「クリップ内での腰の高さに対する割合」へ直してから、
					// モデル側の腰の高さへ掛ける。これで単位に依存しなくなる。
					const float clipHipHeight = std::abs(origin.y);
					if (clipHipHeight > 0.0001f)
					{
						const float sinkRatio = (sampled.y - origin.y) / clipHipHeight;
						// 元データに極端な値が入っていても破綻しないよう、
						// 沈み込みは腰の高さの±12%までに制限する。
						const float clampedRatio = std::clamp(sinkRatio, -0.12f, 0.12f);
						restPosition.y += restPosition.y * clampedRatio;
					}
				}

				if (translationOnlyBone)
				{
					// 回転は休止姿勢のものを使い、位置だけ差し替える。
					Matrix4x4 pose = rest->second;
					pose._41 = restPosition.x;
					pose._42 = restPosition.y;
					pose._43 = restPosition.z;
					boneIt->second.AnimationMatrix = pose;
				}
				else
				{
					boneIt->second.AnimationMatrix = rotationMatrix *
						Matrix4x4::CreateTranslation(restPosition);
				}
			}
			else
			{
				boneIt->second.AnimationMatrix = rotationMatrix;
			}
		}
	}

}
void CAnimationMesh::UpdateAnimationWithManualPose(
	BoneCombMatrix& bonecombarray,
	aiAnimation* animationdata,
	int& CurrentFrame,
	const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations,
	const std::vector<std::string>& animatedBoneNames,
	bool loopAnimation,
	float frameFraction,
	const std::unordered_map<std::string, Matrix4x4>* blendFromPose,
	float blendRate)
{
	m_DebugBoneMatrices.clear();
	const auto isAnimatedBone = [&animatedBoneNames](const std::string& name)
	{
		return std::find(animatedBoneNames.begin(), animatedBoneNames.end(), name) !=
			animatedBoneNames.end();
	};

	// モデルの休止姿勢から始め、下で選択したボーンだけを読み込んだクリップへ置き換える。
	for (auto& [name, bone] : m_BoneDictionary)
	{
		const auto rest = m_RestLocalMatrices.find(name);
		bone.AnimationMatrix = rest != m_RestLocalMatrices.end()
			? rest->second
			: Matrix4x4::Identity;
	}

	ApplyAnimationToBones(
		animationdata, CurrentFrame, frameFraction, animatedBoneNames, loopAnimation);

	// タイトル姿勢は上半身だけへ適用する。
	// 歩きクリップの手のキーで剣の握りや契約書を持つ手が変わらないようにする。
	for (const auto& [boneName, localPose] : manualLocalRotations)
	{
		if (isAnimatedBone(boneName))
			continue;
		auto boneIt = m_BoneDictionary.find(boneName);
		auto rest = m_RestLocalMatrices.find(boneName);
		if (boneIt != m_BoneDictionary.end() && rest != m_RestLocalMatrices.end())
			boneIt->second.AnimationMatrix = localPose * rest->second;
	}

	// 指定ボーンだけを前姿勢から補間する。全身を一括で混ぜず、呼び出し側が
	// 渡したanimatedBoneNamesに限定することで、下半身の接地やルート移動を
	// 別レイヤーから維持できる。
	if (blendFromPose != nullptr && !blendFromPose->empty())
	{
		const float rate = std::clamp(blendRate, 0.0f, 1.0f);
		for (const std::string& boneName : animatedBoneNames)
		{
			const auto fromIt = blendFromPose->find(boneName);
			auto boneIt = m_BoneDictionary.find(boneName);
			if (fromIt == blendFromPose->end() || boneIt == m_BoneDictionary.end())
				continue;

			boneIt->second.AnimationMatrix = BlendLocalSrt(
				fromIt->second, boneIt->second.AnimationMatrix, rate);
		}
	}

	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);
	for (const auto& bone : m_BoneDictionary)
	{
		if (bone.second.idx >= 0 && bone.second.idx < MAX_BONE)
			bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] =
				bone.second.Matrix.Transpose();
	}
}

void CAnimationMesh::UpdateLayeredAnimation(
	BoneCombMatrix& bonecombarray,
	aiAnimation* baseAnimation,
	int baseFrame,
	float baseFrameFraction,
	const std::vector<std::string>& baseBones,
	bool loopBaseAnimation,
	aiAnimation* overlayAnimation,
	int overlayFrame,
	float overlayFrameFraction,
	const std::vector<std::string>& overlayBones,
	bool loopOverlayAnimation,
	const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations,
	const std::string& baseTranslationBone,
	const std::unordered_map<std::string, Matrix4x4>* overlayBlendFromPose,
	float overlayBlendRate)
{
	m_DebugBoneMatrices.clear();

	// 休止姿勢から始める。
	for (auto& [name, bone] : m_BoneDictionary)
	{
		const auto rest = m_RestLocalMatrices.find(name);
		bone.AnimationMatrix = rest != m_RestLocalMatrices.end()
			? rest->second
			: Matrix4x4::Identity;
	}

	// 下半身(ベース)を先に適用し、その上へ上半身(オーバーレイ)を重ねる。
	// 同じボーンが両方の一覧にある場合は、後から書くオーバーレイが勝つ。
	ApplyAnimationToBones(
		baseAnimation, baseFrame, baseFrameFraction, baseBones, loopBaseAnimation,
		baseTranslationBone);
	ApplyAnimationToBones(
		overlayAnimation, overlayFrame, overlayFrameFraction, overlayBones,
		loopOverlayAnimation);

	// どちらのクリップにも含まれないボーンだけ、指定された姿勢で上書きする。
	const auto isDriven = [&baseBones, &overlayBones](const std::string& name)
	{
		return std::find(baseBones.begin(), baseBones.end(), name) != baseBones.end() ||
			std::find(overlayBones.begin(), overlayBones.end(), name) != overlayBones.end();
	};
	for (const auto& [boneName, localPose] : manualLocalRotations)
	{
		if (isDriven(boneName))
			continue;
		auto boneIt = m_BoneDictionary.find(boneName);
		auto rest = m_RestLocalMatrices.find(boneName);
		if (boneIt != m_BoneDictionary.end() && rest != m_RestLocalMatrices.end())
			boneIt->second.AnimationMatrix = localPose * rest->second;
	}

	// 攻撃開始時は、下半身の移動を維持したまま上半身だけを直前の
	// ロコモーション姿勢から攻撃姿勢へ補間する。全身を混ぜると、歩行側の
	// 足まで攻撃クリップへ引かれて接地が崩れるため、overlayBonesに限定する。
	if (overlayBlendFromPose != nullptr && !overlayBlendFromPose->empty())
	{
		const float rate = std::clamp(overlayBlendRate, 0.0f, 1.0f);
		for (const std::string& boneName : overlayBones)
		{
			const auto fromIt = overlayBlendFromPose->find(boneName);
			auto boneIt = m_BoneDictionary.find(boneName);
			if (fromIt == overlayBlendFromPose->end() || boneIt == m_BoneDictionary.end())
				continue;

			// 攻撃クリップがスケールキーを持たなくても、from側のバインド姿勢に
			// 含まれるスケールを保ったまま補間する。ここで回転・移動だけを
			// 再構成すると、攻撃開始時にプレイヤーが拡大して見えることがある。
			boneIt->second.AnimationMatrix = BlendLocalSrt(
				fromIt->second, boneIt->second.AnimationMatrix, rate);
		}
	}

	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);
	for (const auto& bone : m_BoneDictionary)
	{
		if (bone.second.idx >= 0 && bone.second.idx < MAX_BONE)
			bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] =
				bone.second.Matrix.Transpose();
	}
}

std::unordered_map<std::string, Matrix4x4> CAnimationMesh::CaptureCurrentLocalPose() const
{
	std::unordered_map<std::string, Matrix4x4> pose;
	pose.reserve(m_BoneDictionary.size());
	for (const auto& [boneName, bone] : m_BoneDictionary)
		pose.emplace(boneName, bone.AnimationMatrix);
	return pose;
}

std::unordered_map<std::string, Matrix4x4> CAnimationMesh::SampleLocalPose(
	aiAnimation* animation,
	float normalizedTime,
	const std::vector<std::string>& boneNames)
{
	std::unordered_map<std::string, Matrix4x4> pose;
	if (animation == nullptr)
		return pose;

	// ApplyAnimationToBonesはボーン辞書へ直接書き込むので、対象ボーンの今の値を退避してから
	// 休止姿勢へ戻し、クリップを適用して読み取り、最後に元へ戻す。
	std::vector<std::pair<std::string, Matrix4x4>> saved;
	saved.reserve(boneNames.size());
	for (const std::string& name : boneNames)
	{
		auto boneIt = m_BoneDictionary.find(name);
		if (boneIt == m_BoneDictionary.end())
			continue;
		saved.emplace_back(name, boneIt->second.AnimationMatrix);
		boneIt->second.AnimationMatrix = GetRestLocalMatrix(name);
	}

	// 正規化時間をキー番号と小数部へ直す。キー数はチャンネルごとに違うことがあるので、
	// ApplyAnimationToBonesと同じく回転キーの最大数を基準にする。
	unsigned int maxRotationKeys = 0;
	for (unsigned int c = 0; c < animation->mNumChannels; ++c)
		maxRotationKeys = std::max(maxRotationKeys, animation->mChannels[c]->mNumRotationKeys);
	const float wrapped = normalizedTime - std::floor(normalizedTime);
	const float keyPosition = maxRotationKeys > 1
		? wrapped * static_cast<float>(maxRotationKeys - 1)
		: 0.0f;
	const int frame = static_cast<int>(std::floor(keyPosition));
	const float fraction = keyPosition - static_cast<float>(frame);
	// ループ指定にすると末尾でキー番号が0へ巻き戻るため、ここでは非ループで渡す
	// (正規化時間の巻き戻しは上で済ませている)。
	ApplyAnimationToBones(animation, frame, fraction, boneNames, false, std::string());

	for (auto& [name, previous] : saved)
	{
		auto boneIt = m_BoneDictionary.find(name);
		pose.emplace(name, boneIt->second.AnimationMatrix);
		boneIt->second.AnimationMatrix = previous;
	}
	return pose;
}

void CAnimationMesh::ApplyLocalPose(
	BoneCombMatrix& bonecombarray,
	const std::unordered_map<std::string, Matrix4x4>& localPose,
	const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations)
{
	m_DebugBoneMatrices.clear();
	for (auto& [name, bone] : m_BoneDictionary)
		bone.AnimationMatrix = GetRestLocalMatrix(name);

	for (const auto& [name, matrix] : localPose)
	{
		auto boneIt = m_BoneDictionary.find(name);
		if (boneIt != m_BoneDictionary.end())
			boneIt->second.AnimationMatrix = matrix;
	}

	for (const auto& [name, rotation] : manualLocalRotations)
	{
		if (localPose.find(name) != localPose.end())
			continue;
		auto boneIt = m_BoneDictionary.find(name);
		if (boneIt != m_BoneDictionary.end())
			boneIt->second.AnimationMatrix = rotation * GetRestLocalMatrix(name);
	}

	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);
	for (const auto& [name, bone] : m_BoneDictionary)
	{
		if (bone.idx >= 0 && bone.idx < MAX_BONE)
			bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.idx] = bone.Matrix.Transpose();
	}
}

Matrix4x4 CAnimationMesh::BlendLocalMatrix(const Matrix4x4& from, const Matrix4x4& to, float amount)
{
	return BlendLocalSrt(from, to, amount);
}

namespace
{
	const aiNodeAnim* FindChannel(const aiAnimation* animation, const std::string& boneName)
	{
		if (animation == nullptr)
			return nullptr;
		for (unsigned int c = 0; c < animation->mNumChannels; ++c)
		{
			const aiNodeAnim* channel = animation->mChannels[c];
			if (channel != nullptr && boneName == channel->mNodeName.C_Str())
				return channel;
		}
		return nullptr;
	}

	aiVector3D SamplePositionKeys(const aiNodeAnim* channel, float normalizedTime)
	{
		if (channel->mNumPositionKeys == 0)
			return aiVector3D(0.0f, 0.0f, 0.0f);
		if (channel->mNumPositionKeys == 1)
			return channel->mPositionKeys[0].mValue;
		const float position = std::clamp(normalizedTime, 0.0f, 1.0f) *
			static_cast<float>(channel->mNumPositionKeys - 1);
		const unsigned int index = std::min(
			channel->mNumPositionKeys - 2,
			static_cast<unsigned int>(std::floor(position)));
		const float blend = std::clamp(position - static_cast<float>(index), 0.0f, 1.0f);
		const aiVector3D& a = channel->mPositionKeys[index].mValue;
		const aiVector3D& b = channel->mPositionKeys[index + 1].mValue;
		return a + (b - a) * blend;
	}

	aiQuaternion SampleRotationKeys(const aiNodeAnim* channel, float normalizedTime)
	{
		if (channel->mNumRotationKeys == 0)
			return aiQuaternion();
		if (channel->mNumRotationKeys == 1)
			return channel->mRotationKeys[0].mValue;
		const float position = std::clamp(normalizedTime, 0.0f, 1.0f) *
			static_cast<float>(channel->mNumRotationKeys - 1);
		const unsigned int index = std::min(
			channel->mNumRotationKeys - 2,
			static_cast<unsigned int>(std::floor(position)));
		const float blend = std::clamp(position - static_cast<float>(index), 0.0f, 1.0f);
		aiQuaternion result;
		aiQuaternion::Interpolate(
			result, channel->mRotationKeys[index].mValue, channel->mRotationKeys[index + 1].mValue, blend);
		result.Normalize();
		return result;
	}

	Matrix4x4 QuaternionToMatrix(const aiQuaternion& q)
	{
		Quaternion dx{};
		dx.x = q.x;
		dx.y = q.y;
		dx.z = q.z;
		dx.w = q.w;
		return Matrix4x4::CreateFromQuaternion(dx);
	}
}

Vector3 CAnimationMesh::SampleBonePositionOffset(
	aiAnimation* animation, float normalizedTime, const std::string& boneName) const
{
	const aiNodeAnim* channel = FindChannel(animation, boneName);
	if (channel == nullptr || channel->mNumPositionKeys == 0)
		return Vector3(0.0f, 0.0f, 0.0f);
	const aiVector3D now = SamplePositionKeys(channel, normalizedTime);
	const aiVector3D& origin = channel->mPositionKeys[0].mValue;
	return Vector3(now.x - origin.x, now.y - origin.y, now.z - origin.z);
}

Matrix4x4 CAnimationMesh::SampleHipsInPlace(
	aiAnimation* animation, float normalizedTime, const std::string& boneName, int rotationMode) const
{
	Matrix4x4 rest = GetRestLocalMatrix(boneName);
	const aiNodeAnim* channel = FindChannel(animation, boneName);
	if (channel == nullptr)
		return rest;

	Vector3 restPosition(rest._41, rest._42, rest._43);
	Matrix4x4 restRotation = rest;
	restRotation._41 = 0.0f;
	restRotation._42 = 0.0f;
	restRotation._43 = 0.0f;

	// 上下: クリップの腰の高さを、そのままモデルの腰の高さにする。
	//
	// 以前は「クリップ先頭からの変化」を割合で足していた。ところが slash (5) のように
	// クリップの先頭ですでにしゃがんでいる(腰41cm、立つと約92cm)クリップでは、
	// 変化が0のまま腰だけが立った高さに残り、しゃがんだ脚の角度で足が宙に浮いた(実機で確認)。
	// 脚の角度はクリップの腰の高さを前提に作られているので、高さは絶対値で合わせる必要がある。
	//
	// 前提: クリップの位置キーとモデルの腰の休止位置が同じ単位(Mixamoはどちらもcm)。
	// 高さの軸はクリップはY、モデルの親空間は最も大きい成分の軸(このモデルではZ)。
	if (channel->mNumPositionKeys > 0)
	{
		const aiVector3D now = SamplePositionKeys(channel, normalizedTime);
		const float ax = std::abs(restPosition.x);
		const float ay = std::abs(restPosition.y);
		const float az = std::abs(restPosition.z);
		if (az >= ax && az >= ay)
			restPosition.z = std::copysign(now.y, restPosition.z);
		else if (ay >= ax)
			restPosition.y = std::copysign(now.y, restPosition.y);
		else
			restPosition.x = std::copysign(now.y, restPosition.x);
	}

	Matrix4x4 rotation = restRotation;
	if (rotationMode == 1 && channel->mNumRotationKeys > 0)
	{
		// クリップのバインド姿勢からの回転の変化を、腰自身のローカル空間で休止姿勢へ重ねる。
		// 行ベクトル規約: 変化 = 現在 * 基準の逆、結果 = 変化 * 休止姿勢。
		// 親空間で重ねる(休止姿勢 * 基準の逆 * 現在)と、親の上の軸がクリップと違うモデルでは
		// 回転の軸がずれ、腰のひねりが前後の倒れに化けてキャラクターが寝転ぶ。
		//
		// 基準: クリップのファイル自身のバインド姿勢(Mixamoのクリップでは腰の回転なし = 単位回転。調査で確認)。
		// モデルの休止姿勢も同じバインド姿勢なので、クリップの腰の向きがそのままモデルに再現される。
		//
		// 以前の基準と、それぞれの問題:
		// - 攻撃クリップ自身の先頭: 先頭で前かがみ・しゃがみのクリップ(slash (5)など)ではその傾きが消え、脚の角度と合わない。
		// - 待機クリップの先頭: 待機は片手剣の構えで**腰が55度ひねれている**。その分だけ攻撃中の体が常に斜めを向き、
		//   前へ跳ぶダッシュ攻撃で「前を向いているのに斜めを向いて攻撃する」と指摘された。
		const Matrix4x4 now = QuaternionToMatrix(SampleRotationKeys(channel, normalizedTime));
		rotation = now * restRotation;
	}
	else if (rotationMode == 2 && channel->mNumRotationKeys > 0)
	{
		rotation = QuaternionToMatrix(SampleRotationKeys(channel, normalizedTime));
	}
	return rotation * Matrix4x4::CreateTranslation(restPosition);
}

float CAnimationMesh::DominantAxisComponent(const Matrix4x4& localMatrix)
{
	const float x = localMatrix._41;
	const float y = localMatrix._42;
	const float z = localMatrix._43;
	if (std::abs(z) >= std::abs(x) && std::abs(z) >= std::abs(y))
		return z;
	return std::abs(y) >= std::abs(x) ? y : x;
}

Matrix4x4 CAnimationMesh::GetRestLocalMatrix(const std::string& boneName) const
{
	const auto rest = m_RestLocalMatrices.find(boneName);
	return rest != m_RestLocalMatrices.end() ? rest->second : Matrix4x4::Identity;
}

float CAnimationMesh::GetRestBoneModelHeight(const std::string& boneName) const
{
	const auto rest = m_RestGlobalMatrices.find(boneName);
	// 行ベクトル規約なので平行移動は4行目にある。
	return rest != m_RestGlobalMatrices.end() ? rest->second._42 : 0.0f;
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

void CAnimationMesh::ApplyModelProfile(const CharacterModelProfile& profile)
{
	if (!profile.weaponBone.empty())
		m_swordBoneName = profile.weaponBone;
	m_swordRotationDegrees = profile.weaponRotationDegrees;
	m_swordHandOffset = profile.weaponHandOffset;
	m_swordScale = profile.weaponScale;
}

static void CollectDebugBoneParents(
	const CTreeNode<std::string>* node,
	std::unordered_map<std::string, std::string>& parents)
{
	if (!node)
		return;

	for (const auto& child : node->m_children)
	{
		if (child && !node->m_nodedata.empty() && !child->m_nodedata.empty())
			parents[child->m_nodedata] = node->m_nodedata;
		CollectDebugBoneParents(child.get(), parents);
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

std::unordered_map<std::string, std::string> CAnimationMesh::GetDebugBoneParentNames() const
{
	std::unordered_map<std::string, std::string> parents;
	CollectDebugBoneParents(&m_AssimpNodeNameTree, parents);
	return parents;
}
