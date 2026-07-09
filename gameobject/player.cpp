#include	<cstdint>
#include	<cmath>
#include	<dinput.h>
#include	"player.h"	
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"	

void player::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
}
void player::update(uint64_t dt) {

	if (CInputManager::GetInstance().IsKeyPressed(DIK_A)) {
		if (CInputManager::GetInstance().IsKeyPressed(DIK_W))
		{// 左前移動

			float radian;
			radian = PI * 0.75f;

			m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;

		}
		else if (CInputManager::GetInstance().IsKeyPressed(DIK_S))
		{// 左後移動

			float radian;
			radian = PI * 0.25f;

			m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;
		}
		else
		{// 左移動

			float radian;
			radian = PI * 0.50f;

			m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;
		}
	}

	else if (CInputManager::GetInstance().IsKeyPressed(DIK_D))
	{
		if (CInputManager::GetInstance().IsKeyPressed(DIK_W)) {
			// 右前移動

			float radian;
			radian = -PI * 0.75f;

			m_move.x -= sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;

		}
		else if (CInputManager::GetInstance().IsKeyPressed(DIK_S))
		{// 右後移動
			float radian;
			radian = -PI * 0.25f;

			m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;
		}
		else
		{// 右移動

			float radian;
			radian = -PI * 0.50f;

			m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
			m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

			// 目標角度をセット
			m_destrot.y = radian;
		}
	}
	else if (CInputManager::GetInstance().IsKeyPressed(DIK_W))
	{// 前移動
		float radian;
		radian = PI;

		m_move.x -= std::sinf(radian) * VALUE_MOVE_MODEL;
		m_move.z -= std::cosf(radian) * VALUE_MOVE_MODEL;

		// 目標角度をセット
		m_destrot.y = PI;
	}
	else if (CInputManager::GetInstance().IsKeyPressed(DIK_S))
	{// 後移動
		float radian;
		radian = 0.0f;

		m_move.x -= sinf(radian) * VALUE_MOVE_MODEL;
		m_move.z -= cosf(radian) * VALUE_MOVE_MODEL;

		// 目標角度をセット
		m_destrot.y = 0.0f;
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_RIGHT))
	{// 左回転
		m_destrot.y = m_srt.rot.y - VALUE_ROTATE_MODEL;
		if (m_destrot.y < -PI)
		{
			m_destrot.y += PI * 2.0f;
		}
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_LEFT))
	{// 右回転
		m_destrot.y = m_srt.rot.y + VALUE_ROTATE_MODEL;
		if (m_destrot.y > PI)
		{
			m_destrot.y -= PI * 2.0f;
		}
	}

	// 目標角度と現在角度との差分を求める
	float diffrot = m_destrot.y - m_srt.rot.y;
	if (diffrot > PI)
	{
		diffrot -= PI * 2.0f;
	}
	if (diffrot < -PI)
	{
		diffrot += PI * 2.0f;
	}

	// 比率計算
	m_srt.rot.y += diffrot * RATE_ROTATE_MODEL;
	if (m_srt.rot.y > PI)
	{
		m_srt.rot.y -= PI * 2.0f;
	}
	if (m_srt.rot.y < -PI)
	{
		m_srt.rot.y += PI * 2.0f;
	}

	/// 位置移動
	m_srt.pos += m_move;

	// 移動量に慣性をかける(減速率)
	m_move += -m_move * RATE_MOVE_MODEL;

}

void player::draw(uint64_t dt) {

	// 平行移動
	Matrix4x4 tmtx;
	tmtx = Matrix4x4::CreateTranslation(m_srt.pos.x, m_srt.pos.y, 0);

	// Z軸回転
	Matrix4x4 rmtx;
	rmtx = Matrix4x4::CreateRotationZ(m_srt.rot.z);

	// ピボット処理
	Matrix4x4 pivotmtx1;
	Matrix4x4 pivotmtx2;
	pivotmtx1 = Matrix4x4::CreateTranslation(-m_srt.pivot.x, -m_srt.pivot.y, 0);
	pivotmtx2 = Matrix4x4::CreateTranslation(m_srt.pivot.x, m_srt.pivot.y, 0);

	// 拡大縮小
	Matrix4x4 smtx;
	smtx = Matrix4x4::CreateScale(m_srt.scale.x, m_srt.scale.y, m_srt.scale.z);

	// 行列
	Matrix4x4 mtx = smtx * pivotmtx1 * rmtx * pivotmtx2 * tmtx;

}

void player::dispose() {

}
