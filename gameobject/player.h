#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/IScene.h"
#include	"../system/commontypes.h"	

class player : public gameobject {
	// 旧来の移動調整値（現在は未使用）。
public:
	// 歩く速さ(単位/秒)。以前は70で、身長約18単位のキャラクターに対して速すぎた
	// (人間に換算して秒速約7m。ユーザーから「ゴキブリかと思った」)。
	// 歩きクリップを1倍速で再生したときの速さ(約14)の約1.4倍にしている。
	inline static float VALUE_MOVE_MODEL = 20.0f;
	// ダッシュの速さ = 歩く速さ x この倍率(=45)。走りクリップの1倍速(約39)の約1.15倍。
	inline static float RUN_SPEED_MULTIPLIER = 2.25f;
	inline static float VALUE_ROTATE_MODEL = PI * 0.002f;		// 蝗櫁ｻ｢驥・
	inline static float RATE_ROTATE_MODEL = 0.4f;				// 蝗櫁ｻ｢蜑ｲ蜷・
	inline static float RATE_MOVE_MODEL = 0.2f;					// 遘ｻ蜍墓ｸ幄｡ｰ蜑ｲ蜷・

public:
	enum class MotionState
	{
		Idle,
		Walk,
		Run,
		Dodge,
		Jump,
		Knockback, // 敵の攻撃を食らって吹き飛ばされている
	};
	
	// 所属シーンを受け取るゲームオブジェクトのコンストラクタ。
	explicit player(IScene* scene);

	void update(uint64_t delta) override;
	void update(uint64_t delta, float cameraYaw);
	void update(uint64_t delta, float cameraYaw, bool movementLocked);
	void update(uint64_t delta, float cameraYaw, bool movementLocked, bool sprinting, bool dodgeTriggered);
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	Vector3 getVel() const;
	Vector3 getPos() const;

	SRT getRenderSRT() const;
	void setVisualGroundOffsetY(float offsetY);
	MotionState getMotionState() const;
	const char* getMotionStateName() const;
	float getMotionTime() const;
	bool isDodging() const;
	bool isInvincible() const;
	int getDodgeFrame() const;
	void resetMotion();

	// 敵の攻撃を食らったときに吹き飛ばす。directionは飛ばされる向き(水平)。
	// 飛ばされている間は操作を受け付けない。回避中でも上書きする。
	void applyKnockback(const Vector3& direction, float speed, float seconds);
	bool isKnockedBack() const;

	// ロックオン中は敵を向いたまま移動する(フロム作品のロックオン移動)。
	// 向きを移動方向へ回さないので、横や後ろへ動くと横歩き・後ろ歩きのクリップが混ざる。
	// ダッシュ中は向きの固定を外し、走る方向を向く(エルデンリングと同じ)。
	void setLockOnTarget(bool enabled, const Vector3& targetPosition);

	// 攻撃の踏み込み(ルートモーション)。次のupdateでキャラクターの位置へ足す。
	// updateの中で足すのは、壁との衝突補正(更新前後の差分で行う)を通すため。
	void applyRootMotion(const Vector3& worldDelta);

	void setVel(const Vector3& vel);

private:
	Vector3 m_move{0,0,0};				// 遘ｻ蜍暮㍼
	Vector3 m_destrot{0,0,0};			// 逶ｮ讓吝ｧｿ蜍｢
	MotionState m_motionState = MotionState::Idle;
	float m_motionTime = 0.0f;
	float m_jumpVelocity = 0.0f;
	bool m_jumpWasPressed = false;
	bool m_isJumping = false;
	bool m_isDodging = false;
	float m_dodgeTime = 0.0f;
	Vector3 m_dodgeDirection{0, 0, 0};
	Vector3 m_pendingRootMotion{0, 0, 0};
	bool m_lockOnEnabled = false;
	Vector3 m_lockOnTargetPosition{0, 0, 0};
	bool m_isKnockedBack = false;
	float m_knockbackTime = 0.0f;
	float m_knockbackSeconds = 0.0f;
	float m_knockbackSpeed = 0.0f;
	Vector3 m_knockbackDirection{0, 0, 0};
	// 読み込んだモデルの原点が足元にない場合に描画だけへ加える補正値。
	// ゲーム内の移動とジャンプ物理は従来どおりm_srtを使用する。
	float m_visualGroundOffsetY = 0.0f;

};
