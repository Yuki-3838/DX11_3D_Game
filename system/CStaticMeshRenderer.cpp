#include "CStaticMeshRenderer.h"

#include <algorithm>
#include <cctype>
#include <string>

// 静的メッシュのサブセット、マテリアル、テクスチャを描画用に準備する。
void CStaticMeshRenderer::Init(CStaticMesh& mesh)
{
    // 再初期化されるケースでも、前のメッシュの描画情報を残さない。
    m_Subsets.clear();
    m_SubsetVisible.clear();
    m_DiffuseTextures.clear();
    m_Materiales.clear();

    // 頂点バッファとインデックスバッファを生成する。
    CMeshRenderer::Init(mesh);

    // サブセット情報を取得する。
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

    // 初期状態では全サブセットを表示する。
    m_SubsetVisible.assign(m_Subsets.size(), true);

    // 拡散テクスチャ情報を取得する。
    m_DiffuseTextures = mesh.GetDiffuseTextures();

    // マテリアル情報を取得する。
    std::vector<MATERIAL> materials = mesh.GetMaterials();

    // マテリアルがないメッシュ用に、不透明な既定マテリアルを作成する。
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

    // マテリアル数分ループして、描画用マテリアルを生成する。
    for (const MATERIAL& material : materials)
    {
        auto rendererMaterial = std::make_unique<CMaterial>();
        rendererMaterial->Create(material);
        m_Materiales.push_back(std::move(rendererMaterial));
    }
}

// マテリアル名またはメッシュ名にキーワードを含むサブセットの表示状態を変更する。
// 装備の一部だけを隠す場合に使用する。
bool CStaticMeshRenderer::SetSubsetVisibleByKeyword(std::string_view keyword, bool visible)
{
    if (keyword.empty())
        return false;

    const auto toLowerAscii = [](std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    };

    const std::string searchKeyword = toLowerAscii(std::string(keyword));
    bool changed = false;
    for (std::size_t index = 0; index < m_Subsets.size(); ++index)
    {
        const SUBSET& subset = m_Subsets[index];
        const std::string meshName = toLowerAscii(subset.MeshName);
        const std::string materialName = toLowerAscii(subset.MtrlName);
        if (meshName.find(searchKeyword) == std::string::npos &&
            materialName.find(searchKeyword) == std::string::npos)
        {
            continue;
        }

        if (index >= m_SubsetVisible.size())
            m_SubsetVisible.resize(m_Subsets.size(), true);
        m_SubsetVisible[index] = visible;
        changed = true;
    }
    return changed;
}

// 登録済みのサブセットをマテリアルとテクスチャ付きで描画する。
void CStaticMeshRenderer::Draw()
{
    // インデックスバッファと頂点バッファをセットする。
    BeforeDraw();

    bool drewSubset = false;
    for (std::size_t index = 0; index < m_Subsets.size(); ++index)
    {
        // 非表示に設定された装備パーツは描画しない。
        if (index < m_SubsetVisible.size() && !m_SubsetVisible[index])
            continue;

        const SUBSET& subset = m_Subsets[index];
        if (m_Materiales.empty())
            continue;

        // FBX側のマテリアル番号が欠落/不整合でも、既定マテリアルで描画を継続する。
        const unsigned int materialIndex =
            subset.MaterialIdx < m_Materiales.size() ? subset.MaterialIdx : 0;
        if (!m_Materiales[materialIndex])
            continue;

        m_Materiales[materialIndex]->SetGPU();
        if (m_Materiales[materialIndex]->isDiffuseTextureEnable() &&
            materialIndex < m_DiffuseTextures.size() &&
            m_DiffuseTextures[materialIndex])
        {
            m_DiffuseTextures[materialIndex]->SetGPU();
        }

        if (subset.IndexNum == 0)
            continue;

        // サブセットの開始位置とインデックス数を指定して描画する。
        DrawSubset(subset.IndexNum, subset.IndexBase, subset.VertexBase);
        drewSubset = true;
    }

    // サブセット情報がないメッシュでも、全体を描画できるようにする。
    if (!drewSubset && m_IndexNum > 0 && !m_Materiales.empty() && m_Materiales[0])
    {
        m_Materiales[0]->SetGPU();
        DrawSubset(static_cast<unsigned int>(m_IndexNum), 0, 0);
    }
}
