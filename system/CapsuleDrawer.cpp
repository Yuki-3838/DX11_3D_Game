#include "CapsuleDrawer.h"

#include "SphereDrawer.h"
#include "CylinderDrawer.h"
#include "CommonTypes.h"
#include "transform.h"

// ------------------------------------------------------------
// 内部関数
// カプセルを「円柱 + 上球 + 下球」で描画する
// ------------------------------------------------------------
static void DrawCapsuleParts(
	float radius,
	float cylinderHeight,
	Color col,
	const Matrix4x4& world)
{
	if (radius <= 0.0f) {
		radius = 1.0f;
	}

	if (cylinderHeight < 0.0f) {
		cylinderHeight = 0.0f;
	}

	// ------------------------------------------------------------
	// 中央の円柱
	//
	// CCylinderMesh は Y=0 ～ Y=1 の形状として作られている前提。
	// そのため、
	//   Scale(radius, cylinderHeight, radius)
	//   Y方向に -cylinderHeight / 2 移動
	// して、カプセル中心を原点に合わせる。
	// ------------------------------------------------------------
	Matrix4x4 cylinderMtx =
		Matrix4x4::CreateScale(radius, cylinderHeight, radius);

	cylinderMtx._41 = 0.0f;
	cylinderMtx._42 = -cylinderHeight * 0.5f;
	cylinderMtx._43 = 0.0f;

	CylinderDrawerDraw(cylinderMtx * world, col);

	// ------------------------------------------------------------
	// 上の球
	// ------------------------------------------------------------
	Matrix4x4 topSphereMtx =
		Matrix4x4::CreateScale(radius);

	topSphereMtx._41 = 0.0f;
	topSphereMtx._42 = cylinderHeight * 0.5f;
	topSphereMtx._43 = 0.0f;

	SphereDrawerDraw(topSphereMtx * world, col);

	// ------------------------------------------------------------
	// 下の球
	// ------------------------------------------------------------
	Matrix4x4 bottomSphereMtx =
		Matrix4x4::CreateScale(radius);

	bottomSphereMtx._41 = 0.0f;
	bottomSphereMtx._42 = -cylinderHeight * 0.5f;
	bottomSphereMtx._43 = 0.0f;

	SphereDrawerDraw(bottomSphereMtx * world, col);
}

// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------
void CapsuleDrawerInit()
{
	// カプセル自身はメッシュを持たず、
	// 球描画と円柱描画を使って描画する
	SphereDrawerInit();
	CylinderDrawerInit();
}

// ------------------------------------------------------------
// 半径・円柱高さ・位置指定で描画
// posx, posy, posz はカプセルの中心位置
// ------------------------------------------------------------
void CapsuleDrawerDraw(
	float radius,
	float height,
	Color col,
	float posx,
	float posy,
	float posz)
{
	Matrix4x4 world = Matrix4x4::CreateTranslation(posx, posy, posz);

	DrawCapsuleParts(
		radius,
		height,
		col,
		world);
}

// ------------------------------------------------------------
// SRT 指定で描画
//
// この版では、
// 半径 1、中央円柱高さ 1 のカプセルを
// SRT の行列で変形して描画する。
// ------------------------------------------------------------
void CapsuleDrawerDraw(SRT srt, Color col)
{
	Matrix4x4 world = srt.GetMatrix();

	DrawCapsuleParts(
		1.0f,	// 半径
		1.0f,	// 中央円柱部分の高さ
		col,
		world);
}

// ------------------------------------------------------------
// 行列指定で描画
//
// この版では、
// 半径 1、中央円柱高さ 1 のカプセルを
// 指定された行列で変形して描画する。
// ------------------------------------------------------------
void CapsuleDrawerDraw(Matrix4x4 mtx, Color col)
{
	DrawCapsuleParts(
		1.0f,	// 半径
		1.0f,	// 中央円柱部分の高さ
		col,
		mtx);
}