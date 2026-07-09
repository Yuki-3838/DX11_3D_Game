#pragma once

#include	"CommonTypes.h"

void CylinderDrawerInit();
void CylinderDrawerDraw(float radius, float hieght, Color col, float posx, float posy, float posz);
void CylinderDrawerDraw(SRT rts, Color col);
void CylinderDrawerDraw(Matrix4x4 mtx, Color col);
