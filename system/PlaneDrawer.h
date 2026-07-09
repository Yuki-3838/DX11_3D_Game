#pragma once
#include <simplemath.h>

// •½–Ê•`‰æ‚Ì‰Šú‰»
void PlaneDrawerInit();

// •½–Ê•`‰æ
void PlaneDrawerDraw(DirectX::SimpleMath::Vector3 rot,
	float width, float height,
	DirectX::SimpleMath::Color col,
	float posx, float posy, float posz);

