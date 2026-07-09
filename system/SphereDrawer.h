#pragma once

#include	"CommonTypes.h"
#include	"transform.h"

void SphereDrawerInit();
void SphereDrawerDraw(float radius, Color col, float ex, float ey, float ez);
void SphereDrawerDraw(SRT rts, Color col);
void SphereDrawerDraw(Matrix4x4 mtx, Color col);
void SphereDrawerDrawShadow(
	float radius,       // 球の半径に合わせた影サイズ
	float groundY,      // 地面の Y 高さ
	float ex, float ez, // 影の XZ 位置（球の位置に合わせる）
	Color shadowColor   // 減算する影色（例: Color(0.3f,0.3f,0.3f,1)）
);

