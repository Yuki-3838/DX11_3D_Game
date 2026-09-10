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

public:	
	void Init(CStaticMesh& mesh);
	bool SetSubsetVisibleByKeyword(std::string_view keyword, bool visible);
	void Draw();
};
