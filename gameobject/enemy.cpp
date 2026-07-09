#include	<cstdint>
#include	<cmath>
#include	"enemy.h"
#include	"player.h"
#include	"../system/transform.h"
#include	"../system/commontypes.h"

void enemy::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
}

void enemy::update(uint64_t dt) {

	if (m_target == nullptr) {
		return;
	}

	SRT psrt = m_target->getSRT();

	float diffz = psrt.pos.z - m_srt.pos.z;
	float diffx = psrt.pos.x - m_srt.pos.x;

	m_destrot.y = std::atan2(-diffx, -diffz);

	// –Ú•WŠp“x‚ÆŒ»ÝŠp“x‚Æ‚Ì·•ª‚ð‹‚ß‚é
	float diffrot = m_destrot.y - m_srt.rot.y;
	if (diffrot > PI)
	{
		diffrot -= PI * 2.0f;
	}
	if (diffrot < -PI)
	{
		diffrot += PI * 2.0f;
	}

	// ”ä—¦ŒvŽZ
	m_srt.rot.y += diffrot * RATE_ROTATE_MODEL;
	if (m_srt.rot.y > PI)
	{
		m_srt.rot.y -= PI * 2.0f;
	}
	if (m_srt.rot.y < -PI)
	{
		m_srt.rot.y += PI * 2.0f;
	}

	m_move = Vector3(-std::sinf(m_srt.rot.y), 0, -std::cosf(m_srt.rot.y))*0.5f;

	/// ˆÊ’uˆÚ“®
	m_srt.pos += m_move;

	// ˆÚ“®—Ê‚ÉŠµ«‚ð‚©‚¯‚é(Œ¸‘¬—¦)
	m_move += -m_move * RATE_MOVE_MODEL;

}

void enemy::draw(uint64_t dt) {

	// •½sˆÚ“®
	Matrix4x4 tmtx;
	tmtx = Matrix4x4::CreateTranslation(m_srt.pos.x, m_srt.pos.y, 0);

	// ZŽ²‰ñ“]
	Matrix4x4 rmtx;
	rmtx = Matrix4x4::CreateRotationZ(m_srt.rot.z);

	// ƒsƒ{ƒbƒgˆ—
	Matrix4x4 pivotmtx1;
	Matrix4x4 pivotmtx2;
	pivotmtx1 = Matrix4x4::CreateTranslation(-m_srt.pivot.x, -m_srt.pivot.y, 0);
	pivotmtx2 = Matrix4x4::CreateTranslation(m_srt.pivot.x, m_srt.pivot.y, 0);

	// Šg‘åk¬
	Matrix4x4 smtx;
	smtx = Matrix4x4::CreateScale(m_srt.scale.x, m_srt.scale.y, m_srt.scale.z);

	// s—ñ
	Matrix4x4 mtx = smtx * pivotmtx1 * rmtx * pivotmtx2 * tmtx;

}

void enemy::dispose() {

}
