#pragma once
#include "commontypes.h"

// 平面の方程式を表す構造体
struct Plane {
	float a, b, c, d;
};

// 地形データの面の方程式情報
struct PLANEINFO {
	Plane		plane{};			//平面の方程式
	Vector3		p0{}, p1{}, p2{};	//頂点座標
};

// 平面クラス
class CPlane {
	PLANEINFO	m_planeinfo;
public:
	void MakeEquatation(
			Vector3 p0,
			Vector3 p1,
			Vector3 p2
	);
	PLANEINFO GetPlaneInfo() const{
		return m_planeinfo;
	}
};

