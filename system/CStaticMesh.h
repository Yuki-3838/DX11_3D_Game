#pragma once

#include	<string>
#include	<vector>
#include	<memory>
#include	"CTexture.h"
#include	"CMesh.h"
#include	"renderer.h"

class CStaticMesh : public CMesh {
public:
	void Load(std::string filename, std::string texturedirectory="");

	const std::vector<MATERIAL>& GetMaterials() {
		return m_materials;
	}

	const std::vector<SUBSET>& GetSubsets() {
		return m_subsets;
	}

	const std::vector<std::string>& GetDiffuseTextureNames() {
		return m_diffusetexturenames;
	}

	std::vector<std::unique_ptr<CTexture>> GetDiffuseTextures() {
		return std::move(m_diffusetextures);
	}

private:

	std::vector<MATERIAL> m_materials;					// マテリアル情報
	std::vector<std::string> m_diffusetexturenames;		// ディフューズテクスチャ名
	std::vector<SUBSET> m_subsets;						// サブセット情報

	std::vector<std::unique_ptr<CTexture>>	m_diffusetextures;	// ディフューズテクスチャ群
};