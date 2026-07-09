#pragma once
//==============================================================================
// CInputManager.h
//   CDirectInput をラップした初心者向け入力システム
//
//   できること：
//     ・キー/マウスボタンが「押されているか」を調べる
//     ・キー/マウスボタンが「押し続けられている時間（ミリ秒）」を取得
//     ・「押下トリガー」（押した瞬間）の検知
//     ・「離されたトリガー」（離した瞬間）の検知 + その時の押下時間
//
//   使い方：
//     1) ゲーム初期化時に CDirectInput::GetInstance().Init(...) を呼ぶ
//     2) 毎フレーム先頭で CInputManager::GetInstance().Update() を呼ぶ
//     3) あとは IsKeyPressed / IsKeyTriggered などを自由に呼ぶだけ！
//==============================================================================
#include "CDirectInput.h"
#include <windows.h>

class CInputManager {
public:
	//--------------------------------------------------------------------------
	// マウスボタン識別子（初心者が数字で迷わないように enum で用意）
	//--------------------------------------------------------------------------
	enum MouseButton {
		MOUSE_LEFT = 0,	// マウス左ボタン
		MOUSE_RIGHT = 1,	// マウス右ボタン
		MOUSE_CENTER = 2,	// マウス中央ボタン（ホイールクリック）
	};

private:
	//--------------------------------------------------------------------------
	// 1 つのキー or ボタンの状態をまとめた構造体
	//--------------------------------------------------------------------------
	struct KeyInfo {
		bool  isPressed = false;	// 現在押されているか
		bool  wasPressed = false;	// 1 フレーム前に押されていたか
		DWORD pressStartTime = 0;		// 押し始めた時刻 (ミリ秒)
		DWORD pressDuration = 0;		// 現在押され続けている時間 (ミリ秒)
		DWORD lastHoldTime = 0;		// 最後に離された時、何ミリ秒押されていたか
	};

	KeyInfo m_keys[256]{};			// キーボード 256 個分
	KeyInfo m_mouseButtons[8]{};	// マウスボタン 8 個分（使うのは左右中の 3 個）

	// シングルトンなので外からの生成は禁止
	CInputManager() = default;

	//--------------------------------------------------------------------------
	// 1 つのキー情報を更新する内部処理
	//--------------------------------------------------------------------------
	void UpdateKeyInfo(KeyInfo& info, bool nowPressed, DWORD currentTime) {
		// 前フレームの押下状態を保存
		info.wasPressed = info.isPressed;

		if (nowPressed) {
			if (!info.isPressed) {
				// ----- 押された瞬間（押下トリガー） -----
				info.pressStartTime = currentTime;
				info.pressDuration = 0;
			}
			else {
				// ----- 押され続けている -----
				info.pressDuration = currentTime - info.pressStartTime;
			}
			info.isPressed = true;
		}
		else {
			if (info.isPressed) {
				// ----- 離された瞬間（離しトリガー） -----
				info.lastHoldTime = currentTime - info.pressStartTime;
				info.pressDuration = 0;
			}
			info.isPressed = false;
		}
	}

public:
	// コピー・ムーブ禁止
	CInputManager(const CInputManager&) = delete;
	CInputManager& operator=(const CInputManager&) = delete;
	CInputManager(CInputManager&&) = delete;
	CInputManager& operator=(CInputManager&&) = delete;

	//--------------------------------------------------------------------------
	// インスタンス取得
	//--------------------------------------------------------------------------
	static CInputManager& GetInstance() {
		static CInputManager instance;
		return instance;
	}

	//==========================================================================
	// 毎フレーム先頭で必ず呼ぶ更新処理
	//==========================================================================
	void Update() {
		CDirectInput& di = CDirectInput::GetInstance();

		// DirectInput からキー/マウス状態を取得
		di.GetKeyBuffer();
		di.GetMouseState();

		const DWORD now = GetTickCount();

		// ----- キーボード 256 個の状態を一括更新 -----
		for (int i = 0; i < 256; ++i) {
			bool pressed = di.CheckKeyBuffer(i);
			UpdateKeyInfo(m_keys[i], pressed, now);
		}

		// ----- マウス左右中 3 ボタンを更新 -----
		UpdateKeyInfo(m_mouseButtons[MOUSE_LEFT], di.GetMouseLButtonCheck(), now);
		UpdateKeyInfo(m_mouseButtons[MOUSE_RIGHT], di.GetMouseRButtonCheck(), now);
		UpdateKeyInfo(m_mouseButtons[MOUSE_CENTER], di.GetMouseCButtonCheck(), now);
	}

	//==========================================================================
	// 【キーボード】判定関数
	//   keyCode には DIK_A, DIK_SPACE, DIK_RETURN などの DirectInput キー定数を渡す
	//==========================================================================

	// キーが押されているか（押しっぱなし状態で true を返し続ける）
	bool IsKeyPressed(int keyCode) const {
		if (keyCode < 0 || keyCode >= 256) return false;
		return m_keys[keyCode].isPressed;
	}

	// 押した瞬間だけ true（押下トリガー）
	bool IsKeyTriggered(int keyCode) const {
		if (keyCode < 0 || keyCode >= 256) return false;
		return m_keys[keyCode].isPressed && !m_keys[keyCode].wasPressed;
	}

	// 離した瞬間だけ true（離しトリガー）
	bool IsKeyReleased(int keyCode) const {
		if (keyCode < 0 || keyCode >= 256) return false;
		return !m_keys[keyCode].isPressed && m_keys[keyCode].wasPressed;
	}

	// 現在何ミリ秒押され続けているか
	//   押されていない時は 0
	DWORD GetKeyHoldTime(int keyCode) const {
		if (keyCode < 0 || keyCode >= 256) return 0;
		return m_keys[keyCode].pressDuration;
	}

	// 最後に離された時、何ミリ秒押されていたか
	//   （IsKeyReleased() が true のフレームで使うと「その押下時間」が取れる）
	DWORD GetKeyLastHoldTime(int keyCode) const {
		if (keyCode < 0 || keyCode >= 256) return 0;
		return m_keys[keyCode].lastHoldTime;
	}

	//==========================================================================
	// 【マウスボタン】判定関数
	//==========================================================================
	bool  IsMousePressed(MouseButton btn) const { return  m_mouseButtons[btn].isPressed && true; }
	bool  IsMouseTriggered(MouseButton btn) const { return  m_mouseButtons[btn].isPressed && !m_mouseButtons[btn].wasPressed; }
	bool  IsMouseReleased(MouseButton btn) const { return !m_mouseButtons[btn].isPressed && m_mouseButtons[btn].wasPressed; }
	DWORD GetMouseHoldTime(MouseButton btn) const { return m_mouseButtons[btn].pressDuration; }
	DWORD GetMouseLastHoldTime(MouseButton btn) const { return m_mouseButtons[btn].lastHoldTime; }

	//==========================================================================
	// 【マウス座標】（CDirectInput に丸投げするだけのラッパー）
	//==========================================================================
	int GetMouseX() const { return CDirectInput::GetInstance().GetMousePosX(); }
	int GetMouseY() const { return CDirectInput::GetInstance().GetMousePosY(); }
};