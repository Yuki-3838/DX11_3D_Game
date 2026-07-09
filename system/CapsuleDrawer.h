#pragma once
#include	"CommonTypes.h"
#include    "renderer.h"
#include	"transform.h"

void CapsuleDrawerInit();

void CapsuleDrawerDraw(
	float radius,
	float height,
	Color col,
	float posx,
	float posy,
	float posz);

void CapsuleDrawerDraw(SRT srt, Color col);

void CapsuleDrawerDraw(Matrix4x4 mtx, Color col);
