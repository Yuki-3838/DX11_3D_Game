#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/commontypes.h"	
#include	"../system/IScene.h"
#include	"../system/CPlane.h"

class wall : public gameobject {
public:
	// ISceneのポインタを受け取るコンストラクタを追加
	wall(IScene* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setwidth(float w) {
		m_width = w;
	}
	void setheight(float h) {
		m_height = h;
	}

	void setangleY(float angle) {
		m_rotangley = angle;
		m_srt.rot.y = angle;
	}

	float getwidth() const{
		return m_width;
	}

	float getheight() const {
		return m_height;
	}

	CPlane getEquation() const{
		return m_planeEquation;
	}

	void calcEqation();

	void drawred(uint64_t dt);

private:
	float	m_height{100};				// 高さ
	float	m_width{500};				// 幅
	float   m_rotangley{ 0 };			// Y角度

	CPlane  m_planeEquation{};			// 平面の方程式
};