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
	m_MaterialData.clear();
	m_CharacterMaterialProfile.clear();

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
        MATERIAL renderMaterial = material;
        // 既存形式のモデルにはRoughnessが無いため、旧Shininessを
        // 物理ベースの粗さへ変換する。極端な鏡面反射を避けるため、
        // 値が無い場合も完全な鏡面にはしない。
        if (renderMaterial.PBRParams.y <= 0.001f)
        {
            const float importedShininess = std::clamp(renderMaterial.Shiness, 0.0f, 96.0f);
            const float fallbackRoughness = importedShininess > 0.001f
                ? 1.0f - importedShininess / 96.0f
                : 0.68f;
            renderMaterial.PBRParams.y = std::clamp(fallbackRoughness, 0.12f, 0.92f);
        }

        m_MaterialData.push_back(renderMaterial);
        auto rendererMaterial = std::make_unique<CMaterial>();
        rendererMaterial->Create(renderMaterial);
        m_Materiales.push_back(std::move(rendererMaterial));
    }
}

// マテリアル名から装備部位を推定し、キャラクター専用の材質値を設定する。
// テクスチャを単色に塗り替えず、元のアルベドを残したまま反射の性質だけを分ける。
void CStaticMeshRenderer::ApplyCharacterMaterialProfile(std::string_view profile)
{
    const auto toLowerAscii = [](std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    };

    const std::string profileName = toLowerAscii(std::string(profile));
	m_CharacterMaterialProfile = profileName;
    for (std::size_t materialIndex = 0; materialIndex < m_MaterialData.size(); ++materialIndex)
    {
        std::string materialKey;
        for (const SUBSET& subset : m_Subsets)
        {
            if (subset.MaterialIdx != materialIndex)
                continue;
            materialKey += toLowerAscii(subset.MtrlName);
            materialKey += ' ';
            materialKey += toLowerAscii(subset.MeshName);
            materialKey += ' ';
        }

        float metallic = 0.0f;
        float roughness = 0.70f;
        if (profileName == "dragon")
        {
            // 鱗は金属ではなく、細かな凹凸を残す半粗面。白い鏡面を抑え、
            // テクスチャの茶/赤/黒の階調を優先する。
            metallic = 0.0f;
            roughness = 0.84f;
        }
        else if (profileName == "player")
        {
            const bool isMetalPart =
                materialKey.find("armor") != std::string::npos ||
                materialKey.find("plate") != std::string::npos ||
                materialKey.find("helmet") != std::string::npos ||
                materialKey.find("shield") != std::string::npos ||
                materialKey.find("sword") != std::string::npos ||
                materialKey.find("steel") != std::string::npos ||
                materialKey.find("iron") != std::string::npos ||
                materialKey.find("metal") != std::string::npos;
            const bool isSoftPart =
                materialKey.find("cloth") != std::string::npos ||
                materialKey.find("leather") != std::string::npos ||
                materialKey.find("strap") != std::string::npos ||
                materialKey.find("skin") != std::string::npos ||
                materialKey.find("hair") != std::string::npos ||
                materialKey.find("boot") != std::string::npos;

            if (isMetalPart)
            {
                metallic = 0.72f;
                roughness = 0.42f;
            }
            else if (isSoftPart)
            {
                metallic = 0.0f;
                roughness = 0.76f;
            }
            else
            {
                // 部位名が無いマテリアルは金属に決め打ちせず、
                // 防具と布の中間の控えめな反射にする。
                metallic = 0.12f;
                roughness = 0.62f;
            }
        }

        m_MaterialData[materialIndex].PBRParams.x = metallic;
        m_MaterialData[materialIndex].PBRParams.y = roughness;
        if (materialIndex < m_Materiales.size() && m_Materiales[materialIndex])
            m_Materiales[materialIndex]->Create(m_MaterialData[materialIndex]);
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

    const auto toLowerAscii = [](std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    };

    bool drewSubset = false;
    for (std::size_t index = 0; index < m_Subsets.size(); ++index)
    {
        // 非表示に設定された装備パーツは描画しない。
        if (index < m_SubsetVisible.size() && !m_SubsetVisible[index])
            continue;

        const SUBSET& subset = m_Subsets[index];
        if (m_Materiales.empty() || m_MaterialData.empty())
            continue;

        // FBX側のマテリアル番号が欠落/不整合でも、既定マテリアルで描画を継続する。
        const unsigned int materialIndex =
            subset.MaterialIdx < m_Materiales.size() ? subset.MaterialIdx : 0;
        if (!m_Materiales[materialIndex])
            continue;

        MATERIAL drawMaterial = m_MaterialData[materialIndex];
        if (m_CharacterMaterialProfile == "player")
        {
            std::string subsetKey = toLowerAscii(subset.MtrlName);
            subsetKey += ' ';
            subsetKey += toLowerAscii(subset.MeshName);
            const bool isSwordPart =
                subsetKey.find("sword") != std::string::npos ||
                subsetKey.find("weapon") != std::string::npos;
            const bool isMetalPart =
                subsetKey.find("armor") != std::string::npos ||
                subsetKey.find("plate") != std::string::npos ||
                subsetKey.find("helmet") != std::string::npos ||
                subsetKey.find("shield") != std::string::npos ||
                subsetKey.find("sword") != std::string::npos ||
                subsetKey.find("steel") != std::string::npos ||
                subsetKey.find("iron") != std::string::npos ||
                subsetKey.find("metal") != std::string::npos;
            const bool isSoftPart =
                subsetKey.find("cloth") != std::string::npos ||
                subsetKey.find("leather") != std::string::npos ||
                subsetKey.find("strap") != std::string::npos ||
                subsetKey.find("skin") != std::string::npos ||
                subsetKey.find("hair") != std::string::npos ||
                subsetKey.find("boot") != std::string::npos;

            // 1マテリアルのテクスチャアトラスでも、サブセット名が分かれば
            // 鎧だけを金属、布や身体を非金属として分けられる。
            if (isMetalPart)
            {
                if (isSwordPart)
                {
                    // 剣だけは鎧より滑らかで、冷たい金属色のハイライトを出す。
                    // 発光はごく弱く、暗い戦闘画面でも輪郭が読める補助に留める。
                    drawMaterial.PBRParams.x = 0.90f;
                    drawMaterial.PBRParams.y = 0.24f;
                    drawMaterial.Diffuse = Color(1.15f, 1.17f, 1.22f, 1.0f);
                    drawMaterial.Emission = Color(0.008f, 0.012f, 0.018f, 1.0f);
                }
                else
                {
                    // 太陽光の下では鏡面反射より拡散反射を主役にし、
                    // 塗装された鍛鉄のような粗い鎧として見せる。
                    drawMaterial.PBRParams.x = 0.48f;
                    drawMaterial.PBRParams.y = 0.68f;
                }
            }
            else if (isSoftPart || m_MaterialData.size() == 1)
            {
                drawMaterial.PBRParams.x = 0.0f;
                drawMaterial.PBRParams.y = isSoftPart ? 0.76f : 0.62f;
            }
        }

        m_Materiales[materialIndex]->SetGPU();
        // CMaterialは旧フィールドを個別コピーする実装が残っているため、
        // PBRParamsを含む最新の値を共通マテリアルバッファへ明示的に反映する。
        Renderer::SetMaterial(drawMaterial);
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
    if (!drewSubset && m_IndexNum > 0 && !m_Materiales.empty() &&
        !m_MaterialData.empty() && m_Materiales[0])
    {
        m_Materiales[0]->SetGPU();
        Renderer::SetMaterial(m_MaterialData[0]);
        DrawSubset(static_cast<unsigned int>(m_IndexNum), 0, 0);
    }
}
