#pragma once

#include "transform.h"
#include "CStaticMesh.h"
#include "AssimpPerse.h"
#include "CAnimationData.h"
#include "CTreeNode.h"	
#include "renderer.h"
#include "BoneCombMatrix.h"
#include "CStaticMeshRenderer.h"

class CAnimationMesh : public CStaticMesh
{
protected:
	// ボーン辞書
	std::unordered_map<std::string, BONE> m_BoneDictionary{};	// 20240714 DX化

	// カレントのアニメーションデータ
	aiAnimation* m_CurrentAnimation{};

	// assimp ノード名ツリー（親子関係がわかる）
	CTreeNode<std::string>	m_AssimpNodeNameTree{};

	// レンダラ
	CStaticMeshRenderer m_StaticMeshRenderer{};

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

	// 描画
	void Draw();
};
