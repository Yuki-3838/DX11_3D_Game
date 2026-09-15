#include	<cstdint>
#include	<cmath>
#include	<algorithm>
#include	<Windows.h>
#include	<dinput.h>
#include	"player.h"	
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"	

player::player(IScene* scene)
	: gameobject(scene)
{
}

Vector3 player::getVel() const
{
	return m_move;
}

Vector3 player::getPos() const
{
	return m_srt.pos;
}

void player::setVisualGroundOffsetY(float offsetY)
{
	m_visualGroundOffsetY = offsetY;
}

player::MotionState player::getMotionState() const
{
	return m_motionState;
}

float player::getMotionTime() const
{
	return m_motionTime;
}

bool player::isDodging() const
{
	return m_isDodging;
}

bool player::isInvincible() const
{
	return m_isDodging;
}

int player::getDodgeFrame() const
{
	return static_cast<int>(m_dodgeTime * 60.0f);
}

void player::setVel(const Vector3& vel)
{
	m_move = vel;
}

void player::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
	resetMotion();
}
void player::update(uint64_t dt) {
	update(dt, m_srt.rot.y);
}

void player::update(uint64_t dt, float cameraYaw) {
	update(dt, cameraYaw, false);
}

void player::update(uint64_t dt, float cameraYaw, bool movementLocked) {
	update(dt, cameraYaw, movementLocked, false, false);
}

void player::update(uint64_t dt, float cameraYaw, bool movementLocked, bool sprinting, bool dodgeTriggered) {

	auto& input = CInputManager::GetInstance();
	const auto isMoveKeyPressed = [&input](int directInputKey, int virtualKey) {
		return input.IsKeyPressed(directInputKey) ||
			((GetAsyncKeyState(virtualKey) & 0x8000) != 0);
	};

	const bool moveForward = !movementLocked && isMoveKeyPressed(DIK_W, 'W');
	const bool moveBackward = !movementLocked && isMoveKeyPressed(DIK_S, 'S');
	const bool moveLeft = !movementLocked && isMoveKeyPressed(DIK_A, 'A');
	const bool moveRight = !movementLocked && isMoveKeyPressed(DIK_D, 'D');
	const bool jumpTriggered = false;
	if (movementLocked)
	{
		// 攻撃中は攻撃アニメーションがルート姿勢を管理する。
		// 攻撃開始フレームで移動の慣性を消し、剣を振りながら滑らないようにする。
		m_move.x = 0.0f;
		m_move.z = 0.0f;
	}

	const float deltaSec = std::clamp(static_cast<float>(dt) * 0.000001f, 0.0f, 0.1f);
	m_move.x = 0.0f;
	m_move.z = 0.0f;
	// 吹き飛ばしは回避や移動入力より優先する。食らった瞬間に操作を奪い、
	// 「読み違えた」ことを体で分からせる。
	// 位置の変更は回避と同じくm_srt.posへ直接足す。壁との衝突補正は
	// GameScene側が更新前後の差分で行うので、闘技場の外へは飛ばされない。
	if (m_isKnockedBack)
	{
		m_knockbackTime += deltaSec;
		m_motionState = MotionState::Knockback;
		const float progress = std::clamp(
			m_knockbackTime / std::max(0.01f, m_knockbackSeconds), 0.0f, 1.0f);
		// 出だしが最も速く、二乗で止まる。等速だと滑っているように見える。
		const float remaining = 1.0f - progress;
		m_move = m_knockbackDirection * (m_knockbackSpeed * remaining * remaining * deltaSec);
		if (m_knockbackTime >= m_knockbackSeconds)
		{
			m_isKnockedBack = false;
			m_knockbackTime = 0.0f;
			m_motionState = MotionState::Idle;
		}
		m_srt.pos += m_move;
		return;
	}
	if (!m_isDodging && dodgeTriggered && !movementLocked && !m_isJumping)
	{
		m_isDodging = true;
		m_dodgeTime = 0.0f;
		m_dodgeDirection = Vector3(-std::sinf(m_srt.rot.y), 0.0f, -std::cosf(m_srt.rot.y));
	}
	if (m_isDodging)
	{
		m_dodgeTime += deltaSec;
		m_motionState = MotionState::Dodge;
		m_move = m_dodgeDirection * (180.0f * deltaSec);
		if (m_dodgeTime >= 0.40f)
		{
			m_isDodging = false;
			m_dodgeTime = 0.0f;
		}
		m_srt.pos += m_move;
		return;
	}
	if (jumpTriggered && !m_isJumping)
	{
		m_jumpVelocity = 8.5f;
		m_isJumping = true;
	}
	if (m_isJumping)
	{
		m_srt.pos.y += m_jumpVelocity * deltaSec;
		m_jumpVelocity -= 24.0f * deltaSec;
		if (m_srt.pos.y <= 0.0f)
		{
			m_srt.pos.y = 0.0f;
			m_jumpVelocity = 0.0f;
			m_isJumping = false;
		}
	}

	// WASDはカメラの水平な向きを基準に移動する。
	// Wは画面奥、A/Dは画面左右、Sは画面手前へ移動する。
	const float forwardInput = static_cast<float>(moveForward) - static_cast<float>(moveBackward);
	const float rightInput = static_cast<float>(moveRight) - static_cast<float>(moveLeft);
	const float inputLength = std::sqrt(forwardInput * forwardInput + rightInput * rightInput);
	const bool isMoving = inputLength > 0.0f;
	if (m_isJumping)
	{
		m_motionState = MotionState::Jump;
	}
	else if (isMoving && sprinting)
	{
		m_motionState = MotionState::Run;
	}
	else if (isMoving)
	{
		m_motionState = MotionState::Walk;
	}
	else
	{
		m_motionState = MotionState::Idle;
	}
	if (m_motionState == MotionState::Walk || m_motionState == MotionState::Run)
	{
		m_motionTime += deltaSec;
	}
	else if (m_motionState == MotionState::Idle)
	{
		m_motionTime = 0.0f;
	}

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

		const float moveSpeed = sprinting ? VALUE_MOVE_MODEL * 1.65f : VALUE_MOVE_MODEL;
		m_move.x = moveX * moveSpeed * deltaSec;
		m_move.z = moveZ * moveSpeed * deltaSec;

		// 移動方向へプレイヤーを滑らかに振り向かせる。
		m_destrot.y = std::atan2(-moveX, -moveZ);
	}

	if (!movementLocked && CInputManager::GetInstance().IsKeyPressed(DIK_RIGHT))
	{// 左回転
		m_destrot.y = m_srt.rot.y - VALUE_ROTATE_MODEL;
		if (m_destrot.y < -PI)
		{
			m_destrot.y += PI * 2.0f;
		}
	}

	if (!movementLocked && CInputManager::GetInstance().IsKeyPressed(DIK_LEFT))
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

SRT player::getRenderSRT() const
{
	SRT renderSrt = m_srt;
	renderSrt.pos.y += m_visualGroundOffsetY;
	if (m_motionState == MotionState::Walk || m_motionState == MotionState::Run)
	{
		const float step = std::sinf(m_motionTime * (m_motionState == MotionState::Run ? 10.0f : 7.0f));
		renderSrt.pos.y -= std::fabs(step) * 0.12f;
	}
	if (m_isKnockedBack)
	{
		// 飛ばされている間は上体を後ろへ反らせる。移動だけだと押されて滑っているように見え、
		// 「食らった」衝撃が伝わらない。
		// プレイヤーの正面は-Z方向。行ベクトル規約でX軸へ正の回転を加えると
		// 頭が+Z(後ろ)へ倒れる。モデルの原点は足元なので、足は地面から離れない。
		const float progress = std::clamp(
			m_knockbackTime / std::max(0.01f, m_knockbackSeconds), 0.0f, 1.0f);
		// 食らった瞬間に最大まで反り、飛ばされ終わるまでに戻す。
		const float lean = progress < 0.15f
			? progress / 0.15f
			: 1.0f - (progress - 0.15f) / 0.85f;
		constexpr float KNOCKBACK_LEAN_RADIANS = 0.30f;
		renderSrt.rot.x += KNOCKBACK_LEAN_RADIANS * std::clamp(lean, 0.0f, 1.0f);
	}
	return renderSrt;
}

void player::applyKnockback(const Vector3& direction, float speed, float seconds)
{
	Vector3 horizontal(direction.x, 0.0f, direction.z);
	if (horizontal.LengthSquared() < 0.0001f)
	{
		// 敵と完全に重なっていて向きが決まらないときは、今向いている方の反対へ飛ばす。
		horizontal = Vector3(std::sinf(m_srt.rot.y), 0.0f, std::cosf(m_srt.rot.y));
	}
	horizontal.Normalize();

	m_isKnockedBack = true;
	m_knockbackTime = 0.0f;
	m_knockbackSeconds = seconds;
	m_knockbackSpeed = speed;
	m_knockbackDirection = horizontal;
	// 飛ばしてきた相手の方を向かせる。背中から飛ばされる向きにすると、
	// 何に飛ばされたのかが画面から読めない。
	// 正面は(-sin, -cos)なので、飛ばされる向きの逆を正面にするyawを求める。
	m_srt.rot.y = std::atan2(horizontal.x, horizontal.z);
	// 目標の向きも揃えておく。揃えないと、飛ばされ終わった直後に
	// 食らう前の向きへ振り向き直してしまう。
	m_destrot.y = m_srt.rot.y;
	// 回避中に食らった場合(無敵時間の外)は、回避を打ち切る。
	m_isDodging = false;
	m_dodgeTime = 0.0f;
	m_motionState = MotionState::Knockback;
}

bool player::isKnockedBack() const
{
	return m_isKnockedBack;
}

const char* player::getMotionStateName() const
{
	switch (m_motionState)
	{
	case MotionState::Walk:
		return "歩行";
	case MotionState::Run:
		return "ダッシュ";
	case MotionState::Dodge:
		return "回避";
	case MotionState::Jump:
		return "ジャンプ";
	case MotionState::Knockback:
		return "吹き飛ばし";
	default:
		return "待機";
	}
}

void player::resetMotion()
{
	m_move = Vector3(0, 0, 0);
	m_motionState = MotionState::Idle;
	m_motionTime = 0.0f;
	m_jumpVelocity = 0.0f;
	m_jumpWasPressed = false;
	m_isJumping = false;
	m_isDodging = false;
	m_dodgeTime = 0.0f;
	m_dodgeDirection = Vector3(0, 0, 0);
	m_isKnockedBack = false;
	m_knockbackTime = 0.0f;
}

void player::dispose() {

}
