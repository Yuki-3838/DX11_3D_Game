#include	<iostream>
#include   "CommonTypes.h"

#include	"CMeshRenderer.h"
#include	"CMaterial.h"
#include	"CSphereMesh.h"
#include    "CShader.h"
#include	"transform.h"


static CSphereMesh g_mesh;
static CMeshRenderer g_renderer;
static CMaterial g_material;
static CShader g_shader;

void SphereDrawerInit() 
{
	g_mesh.Init(1, Color(1, 1, 1, 1), 50, 50);
	g_renderer.Init(g_mesh);

	MATERIAL mtrl;
	// マテリアル生成
	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 0, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = FALSE;

	g_material.Create(mtrl);

	// シェーダーの初期化
//	g_shader.Create(
//		"shader/unlitTextureVS.hlsl",			// 頂点シェーダー
//		"shader/unlitTexturePS.hlsl");			// ピクセルシェーダー

	g_shader.Create(
		"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
		"shader/vertexLightingPS.hlsl");			// ピクセルシェーダー
	
}

void SphereDrawerDraw(float radius,Color col,float ex, float ey, float ez)
{
	Matrix4x4 mtx = Matrix4x4::CreateScale(radius);

	mtx._41 = ex;
	mtx._42 = ey;
	mtx._43 = ez;

	Renderer::SetWorldMatrix(&mtx);
	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();

	g_material.SetGPU();
	g_renderer.Draw();
}

void SphereDrawerDraw(SRT srt ,Color col)
{
	Matrix4x4 mtx = srt.GetMatrix();

	Renderer::SetWorldMatrix(&mtx);
	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();

	g_material.SetGPU();
	g_renderer.Draw();
}

void SphereDrawerDraw(Matrix4x4 mtx, Color col)
{
	Renderer::SetWorldMatrix(&mtx);
	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();

	g_material.SetGPU();
	g_renderer.Draw();
}

// -------------------------------------
// 丸影（ブロブシャドウ）を描画する処理
//   - 減算ブレンドを使用
//   - Y 方向をつぶした球を地面に置く
// -------------------------------------
void SphereDrawerDrawShadow(
	float radius,       // 球の半径に合わせた影サイズ
	float groundY,      // 地面の Y 高さ
	float ex, float ez, // 影の XZ 位置（球の位置に合わせる）
	Color shadowColor   // 減算する影色（例: Color(0.3f,0.3f,0.3f,1)）
)
{
	// 影用に「つぶした球」を作る
	// XZ は球の半径、Y は極薄にスケールして地面上に「丸」に見えるようにする
	Matrix4x4 mtx = Matrix4x4::CreateScale(radius, 0.01f, radius);

	// 地面の少し上に置いて Z ファイティングを避ける
	mtx._41 = ex;
	mtx._42 = groundY + 0.01f;
	mtx._43 = ez;

	Renderer::SetWorldMatrix(&mtx);

	// 影は黒〜グレー系の色を使う
	g_material.SetDiffuse(shadowColor);
	g_material.Update();

	g_shader.SetGPU();
	g_material.SetGPU();
	g_renderer.Draw();
}