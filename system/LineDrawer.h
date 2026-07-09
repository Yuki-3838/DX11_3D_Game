#pragma once

#include	"CommonTypes.h"

void LineDrawerInit();
void LineDrawerDraw(
	float length,
	Vector3 start,
	Vector3 direction,
	Color col);

void SetLineWidth(float);
