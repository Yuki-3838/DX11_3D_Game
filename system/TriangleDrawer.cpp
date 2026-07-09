#include	<iostream>
#include	<array>

#include	"CommonTypes.h"
#include	"CIndexBuffer.h"
#include	"CVertexBuffer.h"
#include	"TriangleDrawer.h"
#include	"CMaterial.h"
#include    "CShader.h"

// 描画の為の情報（メッシュに関わる情報）
static CIndexBuffer				g_IndexBuffer;				// インデックスバッファ
static CVertexBuffer<VERTEX_3D>	g_VertexBuffer;				// 頂点バッファ

static std::vector<VERTEX_3D>		g_v;

static CMaterial g_material;
static CShader g_shader;

void TriangleDrawerInit()
{
	g_v.resize(3);

	g_v[0].Position = Vector3(-1, 0, 0);
	g_v[1].Position = Vector3(0, 1, 0);
	g_v[2].Position = Vector3(1, 0, 0);

	g_v[0].Diffuse = Color(1, 1, 1, 1);
	g_v[1].Diffuse = Color(1, 1, 1, 1);
	g_v[2].Diffuse = Color(1, 1, 1, 1);

	g_v[0].Normal = Vector3(0, 0, 1);
	g_v[1].Normal = Vector3(0, 0, 1);
	g_v[2].Normal = Vector3(0, 0, 1);

	g_v[0].TexCoord = Vector2(0, 0);
	g_v[1].TexCoord = Vector2(0, 0);
	g_v[2].TexCoord = Vector2(0, 0);

	// 頂点バッファ生成
	g_VertexBuffer.Create(g_v);

	// インデックスバッファ生成
	std::vector<uint32_t> indices = {0,1,2};
	g_IndexBuffer.Create(indices);

	MATERIAL mtrl;
	// マテリアル生成
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = FALSE;

	g_material.Create(mtrl);

	// シェーダーの初期化
	g_shader.Create(
		"shader/unlitTextureVS.hlsl",			// 頂点シェーダー
		"shader/unlitTexturePS.hlsl",			// ピクセルシェーダー
		"shader/GeometryShader.hlsl"			// ジオメトリシェーダ
	);

}

void TriangleDrawerDraw(
	std::array<Vector3, 3> verices,
	Color col) {

	g_v[0].Position = verices[0];
	g_v[1].Position = verices[1];
	g_v[2].Position = verices[2];

	// 頂点バッファを更新
	g_VertexBuffer.Modify(g_v);

	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();
	g_material.SetGPU();

	g_VertexBuffer.SetGPU();
	g_IndexBuffer.SetGPU();

	Matrix4x4 mtx = Matrix4x4::Identity;
	Renderer::SetWorldMatrix(&mtx);

	// 描画の処理
	ID3D11DeviceContext* devicecontext;
	devicecontext = Renderer::GetDeviceContext();

	// トポロジーをセット（旧プリミティブタイプ）
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

//	devicecontext->DrawIndexed(
//		3,							// 描画するインデックス数
//		0,							// 最初のインデックスバッファの位置
//		0);

	devicecontext->Draw(
		3,							// 描画するインデックス数
		0);							// 最初のインデックスバッファの位置

}
