#include    "../system/commontypes.h"
#include	"PlaneDrawer.h"

#include	"CMeshRenderer.h"
#include	"CMaterial.h"
#include	"CPlaneMesh.h"
#include    "CShader.h"

static CPlaneMesh g_mesh;
static CMeshRenderer g_renderer;
static CMaterial g_material;
static CShader g_shader;


void PlaneDrawerInit()
{
	g_mesh.Init(1, 1,		// 分割数 
		1,					// 幅		
		1, 					// 高さ
		Color(1, 1, 1, 1),	// 色
		Vector3(0, 0, -1));	// 法線

	g_renderer.Init(g_mesh);

	// マテリアル生成
	MATERIAL mtrl;

	mtrl.Ambient = Color(0, 0, 0, 0);
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.Emission = Color(0, 0, 0, 0);
	mtrl.Specular = Color(0, 0, 0, 0);
	mtrl.Shiness = 0;
	mtrl.TextureEnable = FALSE;

	g_material.Create(mtrl);

	// シェーダーの初期化
	g_shader.Create(
		"shader/unlitTextureVS.hlsl",				// 頂点シェーダー
		"shader/unlitTexturePS.hlsl");			// ピクセルシェーダー

}

// 平面描画
void PlaneDrawerDraw(Vector3 rot,
	float width,float height, 
	Color col, 
	float posx, float posy, float posz)
{

	Matrix4x4 rmtx = Matrix4x4::CreateFromYawPitchRoll(rot.y, rot.x, rot.z);
	Matrix4x4 smtx = Matrix4x4::CreateScale(width, height, 1);
	Matrix4x4 mtx = smtx * rmtx;

	mtx._41 = posx;
	mtx._42 = posy;
	mtx._43 = posz;

	Renderer::SetWorldMatrix(&mtx);
	g_material.SetDiffuse(col);
	g_material.Update();

	g_shader.SetGPU();

	g_material.SetGPU();
	g_renderer.Draw();
}
