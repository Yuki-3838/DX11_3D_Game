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
	std::unordered_map<std::string, Matrix4x4> m_DebugBoneMatrices{};

	// レンダラ
	CStaticMeshRenderer m_StaticMeshRenderer{};
	std::unique_ptr<CStaticMesh> m_swordMesh{};
	CStaticMeshRenderer m_swordRenderer{};
	std::unique_ptr<CMesh> m_swordProxyMesh{};
	CMeshRenderer m_swordProxyRenderer{};
	CMaterial m_swordProxyMaterial{};
	std::string m_swordBoneName{};
	float m_swordScale = 1.0f;
	float m_swordProxyLength = 75.0f;
	Vector3 m_swordModelCenter{};
	Vector3 m_swordTestPosition{ 30.0f, 55.0f, 0.0f };
	Vector3 m_swordRotationDegrees{ 0.0f, 0.0f, 0.0f };
	Vector3 m_swordHandOffset{ 0.0f, 0.0f, 0.0f };
	Vector3 m_swordWorldBase{};
	Vector3 m_swordWorldTip{};
	Vector3 m_swordPreviousWorldTip{};
	Matrix4x4 m_swordWorldMatrix = Matrix4x4::Identity;
	bool m_swordWorldSegmentValid = false;
	bool m_swordDebugRegistered = false;
	bool m_swordEnabled = true;
	bool m_swordUseGuaranteedProxy = true;
	bool m_swordForceTestPlacement = false;

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
	bool IsSwordLoaded() const { return m_swordMesh != nullptr; }
	bool GetSwordWorldSweep(Vector3& base, Vector3& tip, Vector3& previousTip) const
	{
		if (!m_swordWorldSegmentValid) return false;
		base = m_swordWorldBase;
		tip = m_swordWorldTip;
		previousTip = m_swordPreviousWorldTip;
		return true;
	}
};
