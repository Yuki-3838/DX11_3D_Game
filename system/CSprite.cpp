#include "transform.h"
#include "CSprite.h"
#include "renderer.h"

/// @brief スプライト用の共通シェーダ
CShader CSprite::m_Shader{};

/**
 * @brief マテリアルを初期化する
 *
 * デフォルトのライティングなし白マテリアルを設定し、テクスチャを有効にします。
 */
void CSprite::CreateMtrl() {
	MATERIAL mtrl;
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = TRUE;

	m_Material.Create(mtrl);
}

/**
 * @brief マテリアルを初期化する
 *
 * デフォルトのライティングなし白マテリアルを設定し、テクスチャを有効にします。
 */
void CSprite::ModifyMtrl(MATERIAL mtrl) {
	m_Material.SetMaterial(mtrl);
}
/**
 * @brief スプライト用のシェーダを読み込み・生成する
 *
 * 頂点シェーダとピクセルシェーダをロードしてバインド可能な状態にします。
 */
void CSprite::CreateShader() {
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");
}

/**
 * @brief テクスチャを読み込む
 *
 * @param texfilename 読み込むテクスチャファイルのパス
 *
 * @note 読み込みに失敗した場合は assert によりクラッシュします。
 */
void CSprite::CreateTexture(const std::string& texfilename) {
	bool sts = m_Texture.Load(texfilename);
	assert(sts == true);
}

/**
 * @brief スプライトのコンストラクタ
 *
 * 指定された幅・高さ・テクスチャ・UVでスプライトを初期化します。
 *
 * @param width スプライトの幅（ピクセル）
 * @param height スプライトの高さ（ピクセル）
 * @param texfilename 使用するテクスチャのファイル名
 * @param uv UV座標（左上、右上、左下、右下）の配列
 */

CSprite::CSprite(int width, int height, std::string texfilename, std::array<Vector2, 4> uv)
{
	m_Width = width;
	m_Height = height;

	CreateMtrl();
	CreateShader();
	CreateTexture(texfilename);

	m_Vertices.clear();
	m_Vertices.resize(4);

	m_Vertices[0].Position = Vector3(-width / 2.0f, -height / 2.0f, 0);
	m_Vertices[1].Position = Vector3(width / 2.0f, -height / 2.0f, 0);
	m_Vertices[2].Position = Vector3(-width / 2.0f, height / 2.0f, 0);
	m_Vertices[3].Position = Vector3(width / 2.0f, height / 2.0f, 0);

	for (int i = 0; i < 4; ++i) {
		m_Vertices[i].Diffuse = Color(1, 1, 1, 1);
		m_Vertices[i].TexCoord = uv[i];
	}

	m_VertexBuffer.Create(m_Vertices);

	std::vector<unsigned int> indices = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(indices);
}

/**
 * @brief UV座標を更新する
 *
 * @param uv 新しいUV座標（配列長4：左上・右上・左下・右下）
 *
 * @note 変更後は頂点バッファが再構築されます。
 */
void CSprite::ModifyUV(const Vector2* uv)
{
	m_Vertices[0].TexCoord = uv[0];
	m_Vertices[1].TexCoord = uv[1];
	m_Vertices[2].TexCoord = uv[2];
	m_Vertices[3].TexCoord = uv[3];

	m_VertexBuffer.Modify(m_Vertices);
}

/**
 * @brief スプライトを描画する
 *
 * 指定されたスケール・回転・位置でスプライトを描画します。
 *
 * @param scale スケーリング（X, Y, Z）
 * @param rotation 回転角（X, Y, Z：ラジアン）
 * @param pos ワールド空間上の位置
 */
void CSprite::Draw(Vector3 scale, Vector3 rotation, Vector3 pos)
{
	SRT srt;
	srt.scale = scale;
	srt.rot = rotation;
	srt.pos = pos;

	Matrix4x4 worldmtx = srt.GetMatrix();

	Renderer::SetWorldMatrix(&worldmtx);

	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->DrawIndexed(
		4, // インデックス数
		0, // 開始インデックス
		0  // 頂点バッファのオフセット
	);
}

/**
 * @brief スプライトを描画
 *
 * @param Matrix4x4 行列
 */
void CSprite::Draw(Matrix4x4 mtx) 
{
	Renderer::SetWorldViewProjection2D();
	Renderer::SetWorldMatrix(&mtx);

	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	m_Material.SetGPU();
	m_Texture.SetGPU();

	devicecontext->DrawIndexed(
		4, // インデックス数
		0, // 開始インデックス
		0  // 頂点バッファのオフセット
	);
}

/**
 * @brief 使用しているリソースを解放する
 *
 * @note 現在は未実装ですが、明示的な解放処理を追加する際に使用します。
 */
void CSprite::Dispose()
{
	// 必要に応じてリソース解放を記述
}
