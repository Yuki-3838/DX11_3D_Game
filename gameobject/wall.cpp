#include	<cstdint>
#include	<vector>
#include	<cassert>
#include	"wall.h"	
#include	"../system/commontypes.h"
#include	"../system/PlaneDrawer.h"

void wall::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
	
	// 壁の高さと幅
	m_height = 100;
	m_width = 300.0f;

	calcEqation();
}

void wall::update(uint64_t dt) {

}

void wall::draw(uint64_t dt) 
{
	PlaneDrawerDraw(
		m_srt.rot,
		m_width,
		m_height,
		Color(1, 1, 1, 0.4f),
		m_srt.pos.x,
		m_srt.pos.y,
		m_srt.pos.z);
}

void wall::drawred(uint64_t dt)
{
	PlaneDrawerDraw(
		m_srt.rot,
		m_width,
		m_height,
		Color(1, 0, 0, 0.4f),
		m_srt.pos.x,
		m_srt.pos.y,
		m_srt.pos.z);
}

void wall::dispose() {

}

void wall::calcEqation() {
	// 壁の４頂点データ作成
	std::vector<Vector3>	wallvertices;
	wallvertices.reserve(4);
	Matrix4x4 rotmtxY = Matrix4x4::CreateRotationY(m_srt.rot.y);

	for (unsigned int y = 0; y <= 1; y++) {
		Vector3	vtx{};
		for (unsigned int x = 0; x <= 1; x++) {

			// 頂点座標セット
			vtx.x = -m_width / 2.0f + x * m_width;
			vtx.y = -m_height / 2.0f + y * m_height;
			vtx.z = 0.0f;

			// 壁の回転情報を反映させる
			vtx = Vector3::Transform(vtx, rotmtxY);
			vtx += m_srt.pos;

			wallvertices.push_back(vtx);
		}
	}

	// 平面の方程式作成
	m_planeEquation.MakeEquatation(wallvertices[0], wallvertices[1], wallvertices[2]);

}
