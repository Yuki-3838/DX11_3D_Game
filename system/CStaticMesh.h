#pragma once

#include	<string>
#include	<vector>
#include	<memory>
#include	"CTexture.h"
#include	"CMesh.h"
#include	"ModelCache.h"
#include	"renderer.h"

class CStaticMesh : public CMesh {
public:
	// Uses the model cache (ModelCache.h) when it is valid; otherwise reads the file with Assimp and saves the cache.
	void Load(std::string filename, std::string texturedirectory="", bool pretransformVertices = false);

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

protected:
	// Reads the file with Assimp (no cache). CAnimationMesh calls this because it also needs the Assimp work area (bones).
	void LoadWithAssimp(const std::string& filename, const std::string& texturedirectory, bool pretransformVertices);
	// Model cache sections (defined in ModelCache.cpp).
	bool WriteMeshCache(ModelCache::Writer& writer) const;
	bool ReadMeshCache(ModelCache::Reader& reader);
	std::vector<std::pair<std::string, ModelCache::FileStamp>> TextureDependencies() const;
	void ReleaseTexturePixels();

private:

	std::vector<MATERIAL> m_materials;					// マテリアル情報
	std::vector<std::string> m_diffusetexturenames;		// ディフューズテクスチャ名
	std::vector<SUBSET> m_subsets;						// サブセット情報

	std::vector<std::unique_ptr<CTexture>>	m_diffusetextures;	// ディフューズテクスチャ群
};