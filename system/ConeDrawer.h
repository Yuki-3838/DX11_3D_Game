#pragma once

#include	"CommonTypes.h"
#include	"transform.h"

void ConeDrawerInit(bool);

void ConeDrawerDraw(float radius,
	float height,
	Color col,
	float posx, float posy, float posz);

void ConeDrawerDraw(
	float radius,
	float height,
	Color col,
	float posx, float posy, float posz,
	Matrix4x4 rmtx);

void ConeDrawerDraw(
	SRT	rts,
	Color col);

void ConeDrawerDraw(
	Matrix4x4 mtx,
	Color col);


