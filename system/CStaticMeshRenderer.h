#pragma once

#include	"CStaticMesh.h"
#include	"CMeshRenderer.h"
#include	"CTexture.h"
#include    "CMaterial.h"
#include <string_view>

class CStaticMeshRenderer : public CMeshRenderer 
{
	std::vector<SUBSET> m_Subsets;
	std::vector<bool> m_SubsetVisible;
	std::vector<std::unique_ptr<CTexture>> m_DiffuseTextures;
	std::vector<std::unique_ptr<CMaterial>> m_Materiales;
	// PBRParamsを含むCPU側マテリアル。描画直前にRendererの共通バッファへ渡す。
	std::vector<MATERIAL> m_MaterialData;
	std::string m_CharacterMaterialProfile;

public:	
	void Init(CStaticMesh& mesh);
	// キャラクター素材ごとのMetallic/Roughness既定値を適用する。
	// profileは"player"または"dragon"を想定する。
	void ApplyCharacterMaterialProfile(std::string_view profile);
	bool SetSubsetVisibleByKeyword(std::string_view keyword, bool visible);
	void Draw();
};
