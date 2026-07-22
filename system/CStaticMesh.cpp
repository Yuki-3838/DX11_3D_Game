#include	<memory>
#include	<utility>
#include    "commontypes.h"
#include	"CStaticMesh.h"
#include	"AssimpPerse.h"

void CStaticMesh::Load(std::string filename, std::string texturedirectory, bool pretransformVertices)
{
	std::vector<GM31::GE::myAssimp::SUBSET> subsets{};					// サブセット情報
	std::vector<std::vector<GM31::GE::myAssimp::VERTEX>> vertices{};	// 頂点データ（メッシュ単位）
	std::vector<std::vector<unsigned int>> indices{};					// インデックスデータ（メッシュ単位）
	std::vector<GM31::GE::myAssimp::MATERIAL> materials{};				// マテリアル
	std::vector<std::unique_ptr<CTexture>> embededtextures{};			// 内蔵テクスチャ群

	// 再利用時に前回のモデルデータが残らないようにする。
	m_vertices.clear();
	m_indices.clear();
	m_subsets.clear();
	m_materials.clear();
	m_diffusetexturenames.clear();
	m_diffusetextures.clear();

	// assimpを使用してモデルデータを取得
	GM31::GE::myAssimp::GetModelData(filename, texturedirectory, pretransformVertices);

	subsets = GM31::GE::myAssimp::GetSubsets();							// サブセット情報取得
	vertices = GM31::GE::myAssimp::GetVertices();						// 頂点データ（メッシュ単位）
	indices = GM31::GE::myAssimp::GetIndices();							// インデックスデータ（メッシュ単位）
	materials = GM31::GE::myAssimp::GetMaterials();						// マテリアル情報取得

	m_diffusetextures = GM31::GE::myAssimp::GetDiffuseTextures();		// ｄｉｆｆｕｓｅテクスチャ情報取得	

	// 先に容量を確保し、巨大モデルでの再確保・コピーを抑える。
	size_t totalVertexCount = 0;
	for (const auto& mv : vertices) {
		totalVertexCount += mv.size();
	}
	m_vertices.reserve(totalVertexCount);

	// 頂点データ作成
	for (auto& mv : vertices)
	{
		for (const auto& v : mv)
		{
			VERTEX_3D vertex{};
			vertex.Position = Vector3(v.pos.x, v.pos.y, v.pos.z);
			vertex.Normal = Vector3(v.normal.x, v.normal.y, v.normal.z);
			vertex.TexCoord = Vector2(v.texcoord.x, v.texcoord.y);
			vertex.Diffuse = Color(v.color.r, v.color.g, v.color.b, v.color.a);

			for (int i = 0; i < 4; i++)
			{
				vertex.BoneIndex[i] = v.BoneIndex[i];
				vertex.BoneWeight[i] = v.BoneWeight[i];
			}

			m_vertices.emplace_back(vertex);
		}

		// 変換済みのメッシュからAssimp側の巨大な文字列データを解放する。
		mv.clear();
		mv.shrink_to_fit();
	}

	// インデックスデータ作成
	size_t totalIndexCount = 0;
	for (const auto& mi : indices) {
		totalIndexCount += mi.size();
	}
	m_indices.reserve(totalIndexCount);

	for (auto& mi : indices)
	{
		for (const auto index : mi)
		{
			m_indices.emplace_back(index);
		}

		mi.clear();
		mi.shrink_to_fit();
	}

	// サブセットデータ作成
	for (const auto& sub : subsets)
	{
		SUBSET subset{};
		subset.VertexBase = sub.VertexBase;
		subset.VertexNum = sub.VertexNum;
		subset.IndexBase = sub.IndexBase;
		subset.IndexNum = sub.IndexNum;
		subset.MtrlName = sub.mtrlname;
		subset.MaterialIdx = sub.materialindex;					//	マテリアル配列のインデックス
		m_subsets.emplace_back(subset);
	}

	// マテリアルデータ作成(表示のための)
	for (const auto& m : materials)
	{
		MATERIAL material{};
		material.Ambient = Color(m.Ambient.r, m.Ambient.g, m.Ambient.b, m.Ambient.a);
		material.Diffuse = Color(m.Diffuse.r, m.Diffuse.g, m.Diffuse.b, m.Diffuse.a);
		material.Specular = Color(m.Specular.r, m.Specular.g, m.Specular.b, m.Specular.a);
		material.Emission = Color(m.Emission.r, m.Emission.g, m.Emission.b, m.Emission.a);
		material.Shiness = m.Shiness;
		if (m.diffusetexturename.empty())
		{
			material.TextureEnable = FALSE;
			m_diffusetexturenames.emplace_back("");
		}
		else
		{
			material.TextureEnable = TRUE;
			m_diffusetexturenames.emplace_back(m.diffusetexturename);
		}

		m_materials.emplace_back(material);
	}
}
