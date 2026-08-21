#pragma once

#include "transform.h"
#include "CStaticMesh.h"
#include "AssimpPerse.h"
#include "CAnimationData.h"
#include "CTreeNode.h"	
#include "renderer.h"
#include "BoneCombMatrix.h"
#include "CMeshRenderer.h"
#include "CMaterial.h"
#include "CStaticMeshRenderer.h"
#include <vector>

class CAnimationMesh : public CStaticMesh
{
protected:
	// ボーン辞書
	std::unordered_map<std::string, BONE> m_BoneDictionary{};	// 20240714 DX化

	// カレントのアニメーションデータ
	aiAnimation* m_CurrentAnimation{};

	// assimp ノード名ツリー（親子関係がわかる）
	CTreeNode<std::string>	m_AssimpNodeNameTree{};
	std::unordered_map<std::string, Matrix4x4> m_RestLocalMatrices{};
	std::unordered_map<std::string, Matrix4x4> m_RestGlobalMatrices{};
	std::unordered_map<std::string, Matrix4x4> m_DebugBoneMatrices{};

	// レンダラ
	CStaticMeshRenderer m_StaticMeshRenderer{};
	std::unique_ptr<CStaticMesh> m_swordMesh{};
	CStaticMeshRenderer m_swordRenderer{};
	std::string m_swordAssetPath{};
	std::string m_swordPresetPath{};
	std::string m_swordPresetStatus{"Preset not loaded"};
	bool m_swordUsesPlayerAsset = false;
	bool m_swordEmbeddedInPlayerAsset = false;
	std::unique_ptr<CMesh> m_swordProxyMesh{};
	CMeshRenderer m_swordProxyRenderer{};
	CMaterial m_swordProxyMaterial{};
	std::string m_swordBoneName{};
	float m_swordScale = 1.0f;
	float m_swordLocalHalfLength = 0.5f;
	float m_swordProxyLength = 75.0f;
	Vector3 m_swordModelCenter{};
	// Longest local axis of the imported sword vertices.  Collision endpoints
	// are generated from this axis instead of assuming the asset is +Z.
	Vector3 m_swordCollisionLocalAxis{ 0.0f, 0.0f, 1.0f };
	// Centered local coordinate of the Fallen Paladin sword grip.  This is
	// measured from the extracted mesh's handle region, not guessed from its
	// overall bounding-box center.
	Vector3 m_swordGripLocalPoint{};
	Vector3 m_swordTestPosition{ 30.0f, 55.0f, 0.0f };
	// Fitted interactively in GameScene and kept as the safe fallback when no
	// preset exists yet.  These are the values from the designer's good fit.
	Vector3 m_swordRotationDegrees{ 81.50f, -46.50f, 76.75f };
	Vector3 m_swordHandOffset{ 0.650f, -0.070f, 0.646f };
	Vector3 m_swordWorldBase{};
	Vector3 m_swordWorldTip{};
	Vector3 m_swordPreviousWorldTip{};
	Matrix4x4 m_swordWorldMatrix = Matrix4x4::Identity;
	bool m_swordWorldSegmentValid = false;
	bool m_swordDebugRegistered = false;
	bool m_swordTransformLogged = false;
	bool m_swordEnabled = true;
	bool m_swordUseGuaranteedProxy = true;
	bool m_swordForceTestPlacement = false;
	std::vector<uint32_t> m_embeddedSwordVertexIndices{};

	void LoadSwordAttachmentPreset();
	void SaveSwordAttachmentPreset();
	bool BuildEmbeddedSwordWorldSegment(
		const Matrix4x4& parentWorld,
		Vector3& base,
		Vector3& tip) const;

	// ローカルポーズ生成
	void BuildLocalPoseMap(
		const aiAnimation* animationdata,
		int& CurrentFrame,
		std::unordered_map<std::string, SRTQ>& localposemap);

public:
	void SetCurentAnimation(aiAnimation* currentanimation);

	void Load(std::string filename, std::string texturedirectory = "");

	// 階層構造を考慮したボーンコンビネーション行列を更新
	void UpdateBoneMatrix(CTreeNode<std::string>* ptree, Matrix4x4 matrix);		// 20240714 DX化	

	// アニメーションの更新
	void Update(BoneCombMatrix& bonecombarray, int& CurrentFrame);

	// Imported locomotion can drive selected bones while the title pose keeps
	// control of the weapon and contract hand.
	void UpdateAnimationWithManualPose(
		BoneCombMatrix& bonecombarray,
		aiAnimation* animationdata,
		int& CurrentFrame,
		const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations,
		const std::vector<std::string>& animatedBoneNames);

	// レスト姿勢に対して指定ボーンのローカル回転を加えたポーズを更新
	void UpdateManualPose(
		BoneCombMatrix& bonecombarray,
		const std::unordered_map<std::string, Matrix4x4>& localRotations);

	std::vector<std::string> GetBoneNames() const;
	const std::unordered_map<std::string, Matrix4x4>& GetDebugBoneMatrices() const
	{
		return m_DebugBoneMatrices;
	}

	// 描画
	void UpdateSwordWorldTransform(const Matrix4x4& parentWorld);
	void Draw();
	void RenderSwordDebug();
	bool IsSwordLoaded() const { return m_swordMesh != nullptr || m_swordEmbeddedInPlayerAsset; }
	bool GetSwordWorldSweep(Vector3& base, Vector3& tip, Vector3& previousTip) const
	{
		if (!m_swordWorldSegmentValid) return false;
		base = m_swordWorldBase;
		tip = m_swordWorldTip;
		previousTip = m_swordPreviousWorldTip;
		return true;
	}
};
