#include	"commontypes.h"
#include	"CPlane.h"

// 平面の方程式を作成する
void CPlane::MakeEquatation(
	Vector3 p0,
	Vector3 p1,
	Vector3 p2)
{
	Plane p;
	Vector3		p0p1;
	Vector3		p1p2;
	Vector3		normal;

	p0p1 = p1 - p0;
	p1p2 = p2 - p1;

	// 外積を求める
	normal = p0p1.Cross(p1p2);

	// 正規化
	normal.Normalize();

	// a,b,c,dを求める
	p.a = normal.x;
	p.b = normal.y;
	p.c = normal.z;
	p.d = -(p.a * p0.x + p.b * p0.y + p.c * p0.z);

	// メンバ変数へセット
	m_planeinfo.plane = p;

	m_planeinfo.p0 = p0;
	m_planeinfo.p1 = p1;
	m_planeinfo.p2 = p2;
}