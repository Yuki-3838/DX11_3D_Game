#pragma once

#include	"CStaticMesh.h"
#include	"CMeshRenderer.h"
#include	"CTexture.h"
#include    "CMaterial.h"

class CStaticMeshRenderer : public CMeshRenderer 
{
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<CTexture>> m_DiffuseTextures;
	std::vector<std::unique_ptr<CMaterial>> m_Materiales;

public:	
	void Init(CStaticMesh& mesh);
	void Draw();
};
