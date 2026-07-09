#include	<cstdint>
#include	<vector>
#include	<cassert>
#include	"field.h"	
#include	"../system/commontypes.h"

void field::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);

	// 頂点データ
	std::vector<VERTEX_3D>	vertices;

	vertices.resize(4);

	vertices[0].Position = Vector3(-1000, -0.3f, 1000);
	vertices[1].Position = Vector3(1000, -0.3f, 1000);
	vertices[2].Position = Vector3(-1000, -0.3f, -1000);
	vertices[3].Position = Vector3(1000, -0.3f, -1000);

	vertices[0].Diffuse = Color(1, 1, 1, 1);
	vertices[1].Diffuse = Color(1, 1, 1, 1);
	vertices[2].Diffuse = Color(1, 1, 1, 1);
	vertices[3].Diffuse = Color(1, 1, 1, 1);

	vertices[0].TexCoord = Vector2(0, 0);
	vertices[1].TexCoord = Vector2(10, 0);
	vertices[2].TexCoord = Vector2(0, 10);
	vertices[3].TexCoord = Vector2(10, 10);

	// 頂点バッファ生成
	m_VertexBuffer.Create(vertices);

	// インデックスバッファ生成
	std::vector<unsigned int> indices;
	indices.resize(4);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	// インデックスバッファ生成
	m_IndexBuffer.Create(indices);

	// シェーダオブジェクト生成
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

	// マテリアル生成
	MATERIAL	mtrl;
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = TRUE;

	m_Material.Create(mtrl);

	// テクスチャロード
	bool sts = m_Texture.Load("assets/texture/Grass01.jpg");
	assert(sts == true);
}

void field::update(uint64_t dt) {

}

void field::draw(uint64_t dt) {

	Matrix4x4 mtx = m_srt.GetMatrix();
	Renderer::SetWorldMatrix(&mtx);

	// トポロジーをセット（旧プリミティブタイプ）
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	m_Material.SetGPU();
	m_Texture.SetGPU();

	Renderer::GetDeviceContext()->DrawIndexed(
		4,							// 描画するインデックス数（四角形なんで４）
		0,							// 最初のインデックスバッファの位置
		0);
}

void field::dispose() {

}
