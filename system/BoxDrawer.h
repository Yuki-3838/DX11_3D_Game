#pragma once
#include "CommonTypes.h"

void BoxDrawerInit();
void BoxDrawerDraw(
	float width, float height, float depth,
	Color col, float posx, float posy, float posz);
void BoxDrawerDraw(SRT rts, Color col);
void BoxDrawerDraw(Matrix4x4 mtx, Color col);

