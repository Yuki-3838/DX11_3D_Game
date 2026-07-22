#include	<cstdint>
#include	<cmath>
#include	<Windows.h>
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
	update(dt, m_srt.rot.y);
}

void player::update(uint64_t dt, float cameraYaw) {

	auto& input = CInputManager::GetInstance();
	const auto isMoveKeyPressed = [&input](int directInputKey, int virtualKey) {
		return input.IsKeyPressed(directInputKey) ||
			((GetAsyncKeyState(virtualKey) & 0x8000) != 0);
	};

	const bool moveForward = isMoveKeyPressed(DIK_W, 'W');
	const bool moveBackward = isMoveKeyPressed(DIK_S, 'S');
	const bool moveLeft = isMoveKeyPressed(DIK_A, 'A');
	const bool moveRight = isMoveKeyPressed(DIK_D, 'D');

	// WASDはカメラの水平な向きを基準に移動する。
	// Wは画面奥、A/Dは画面左右、Sは画面手前へ移動する。
	const float forwardInput = static_cast<float>(moveForward) - static_cast<float>(moveBackward);
	const float rightInput = static_cast<float>(moveRight) - static_cast<float>(moveLeft);
	const float inputLength = std::sqrt(forwardInput * forwardInput + rightInput * rightInput);
	if (inputLength > 0.0f)
	{
		const float normalizedForward = forwardInput / inputLength;
		const float normalizedRight = rightInput / inputLength;

		// カメラの水平な前方向と右方向を移動軸にする。
		const float forwardX = -std::sinf(cameraYaw);
		const float forwardZ = std::cosf(cameraYaw);
		const float rightX = std::cosf(cameraYaw);
		const float rightZ = std::sinf(cameraYaw);
		const float moveX = forwardX * normalizedForward + rightX * normalizedRight;
		const float moveZ = forwardZ * normalizedForward + rightZ * normalizedRight;

		m_move.x += moveX * VALUE_MOVE_MODEL;
		m_move.z += moveZ * VALUE_MOVE_MODEL;

		// 移動方向へプレイヤーを滑らかに振り向かせる。
		m_destrot.y = std::atan2(-moveX, -moveZ);
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
