#include "CStaticMeshRenderer.h"

void CStaticMeshRenderer::Init(CStaticMesh& mesh)
{
	// 再初期化されるケースでも、前のメッシュの描画情報を残さない。
	m_Subsets.clear();
	m_DiffuseTextures.clear();
	m_Materiales.clear();

	// 頂点バッファとインデックスバッファを生成
	CMeshRenderer::Init(mesh);

	// サブセット情報取得
	m_Subsets = mesh.GetSubsets();

	// 一部のFBXはマテリアル/サブセット情報を持たず、頂点とインデックスだけ
	// 読み込まれることがある。その場合もメッシュ全体を1回で描画できるようにする。
	if (m_Subsets.empty() && !mesh.GetIndices().empty())
	{
		SUBSET fallbackSubset{};
		fallbackSubset.IndexNum = static_cast<unsigned int>(mesh.GetIndices().size());
		fallbackSubset.VertexNum = static_cast<unsigned int>(mesh.GetVertices().size());
		fallbackSubset.IndexBase = 0;
		fallbackSubset.VertexBase = 0;
		fallbackSubset.MaterialIdx = 0;
		fallbackSubset.MtrlName = "FallbackMaterial";
		m_Subsets.push_back(fallbackSubset);
	}

	// diffuseテクスチャ情報取得
	m_DiffuseTextures = mesh.GetDiffuseTextures();

	// マテリアル情報取得
	std::vector<MATERIAL> materials;
	materials = mesh.GetMaterials();

	// マテリアルがないメッシュ用の不透明な既定マテリアル。
	if (materials.empty() && !mesh.GetIndices().empty())
	{
		MATERIAL fallbackMaterial{};
		fallbackMaterial.Ambient = Color(0.65f, 0.65f, 0.70f, 1.0f);
		fallbackMaterial.Diffuse = Color(0.85f, 0.85f, 0.90f, 1.0f);
		fallbackMaterial.Specular = Color(0.25f, 0.25f, 0.25f, 1.0f);
		fallbackMaterial.Emission = Color(0.0f, 0.0f, 0.0f, 1.0f);
		fallbackMaterial.Shiness = 16.0f;
		fallbackMaterial.TextureEnable = FALSE;
		materials.push_back(fallbackMaterial);
	}

	// マテリアル数分ループしてマテリアルデータを生成
	for (int i = 0; i < materials.size(); i++)
	{
		// マテリアルオブジェクト生成
		std::unique_ptr<CMaterial> m = std::make_unique<CMaterial>();

		// マテリアル情報をセット
		m->Create(materials[i]);

		// マテリアルオブジェクトを配列に追加
		m_Materiales.push_back(std::move(m));
	}
}

void CStaticMeshRenderer::Draw()
{
	// インデックスバッファ・頂点バッファをセット
	BeforeDraw();

	// マテリアル数分ループ
	bool drewSubset = false;
	for (int i = 0; i < m_Subsets.size(); i++)
	{
		// マテリアルをセット(サブセット情報の中にあるマテリアルインデックを使用する)
		if (m_Materiales.empty())
			continue;
		// FBX側のマテリアル番号が欠落/不整合でも、既定マテリアルで描画を継続する。
		const unsigned int materialIndex =
			m_Subsets[i].MaterialIdx < m_Materiales.size()
			? m_Subsets[i].MaterialIdx
			: 0;
		if (!m_Materiales[materialIndex])
			continue;
		m_Materiales[materialIndex]->SetGPU();

		if (m_Materiales[materialIndex]->isDiffuseTextureEnable() &&
			materialIndex < m_DiffuseTextures.size() &&
			m_DiffuseTextures[materialIndex])
		{
			m_DiffuseTextures[materialIndex]->SetGPU();
		}
		if (m_Subsets[i].IndexNum == 0)
			continue;

		// サブセットの描画
		DrawSubset(
			m_Subsets[i].IndexNum,							// 描画するインデックス数
			m_Subsets[i].IndexBase,							// 最初のインデックスバッファの位置
		m_Subsets[i].VertexBase);						// 頂点バッファの最初から使用
		drewSubset = true;
	}
	if (!drewSubset && m_IndexNum > 0 && !m_Materiales.empty() && m_Materiales[0])
	{
		m_Materiales[0]->SetGPU();
		DrawSubset(static_cast<unsigned int>(m_IndexNum), 0, 0);
	}
}
