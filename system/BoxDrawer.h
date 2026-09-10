#pragma once
#include "CommonTypes.h"

void BoxDrawerInit();
void BoxDrawerDraw(
	float width, float height, float depth,
	Color col, float posx, float posy, float posz);
void BoxDrawerDraw(SRT rts, Color col);
void BoxDrawerDraw(Matrix4x4 mtx, Color col);
// 照明による面ごとの明暗を付けず、指定色をそのまま描画する。
void BoxDrawerDrawUnlit(SRT rts, Color col);

