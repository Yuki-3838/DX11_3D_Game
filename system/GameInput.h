#pragma once

#include <Windows.h>
#include <Xinput.h>
#include <dinput.h>
#include <algorithm>
#include <array>
#include <cmath>

#include "CommonTypes.h"
#include "Inputmanager.h"
#include "DebugUI.h"
#include "imgui/imgui.h"
#include "../Application.h"

#pragma comment(lib, "xinput.lib")

/**
 * @brief ゲームの「行動」。キーやボタンではなく、何をしたいかで入力を問い合わせる。
 *
 * ゲーム側は「左クリックが押されたか」ではなく「攻撃が押されたか」を聞く。
 * キーボード・マウスとゲームパッドのどちらで押したかは、GameInputの中だけで扱う
 * (仮想コントローラー)。割り当てを変えるときも、ここだけを直せばよい。
 */
enum class GameAction
{
	Attack,      // 弱攻撃
	HeavyAttack, // 強攻撃
	Dodge,       // 回避
	Sprint,      // ダッシュ(押している間)
	LockOn,      // ロックオンの切り替え
	Confirm,     // 決定(タイトル・リザルト)
	Back,        // 戻る(リザルト→タイトル)
	Count
};

/** 画面に出す操作説明をどちらの機器の表記にするか。 */
enum class InputDevice
{
	KeyboardMouse,
	Gamepad
};

/**
 * @class GameInput
 * @brief キーボード・マウスとゲームパッド(XInput)をまとめた仮想コントローラー。
 *
 * 毎フレーム、CInputManager::Update()の後にUpdate()を呼ぶ。
 *
 * ゲームパッドの割り当て(モンスターハンター・エルデンリングでよく使われる配置に寄せた):
 *   左スティック: 移動 / 右スティック: 視点
 *   RB: 弱攻撃 / RT: 強攻撃 / A: 回避 / LB(押し続ける): ダッシュ / 右スティック押し込み: ロックオン
 *   ダッシュをLBにしたのは、右手の親指を視点操作(右スティック)に空けておくため。
 */
class GameInput
{
public:
	static GameInput& Get()
	{
		static GameInput instance;
		return instance;
	}

	void Update()
	{
		UpdateGamepad();
		UpdateKeyboardMouse();

		for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
		{
			m_wasPressed[i] = m_isPressed[i];
			m_isPressed[i] = m_keyboardPressed[i] || m_padPressed[i];
		}
	}

	/** 押している間ずっとtrue。 */
	bool IsPressed(GameAction action) const { return m_isPressed[Index(action)]; }

	/** 押した瞬間だけtrue。 */
	bool IsTriggered(GameAction action) const
	{
		const int i = Index(action);
		return m_isPressed[i] && !m_wasPressed[i];
	}

	/**
	 * @brief 移動の入力。x=右、y=前(画面奥)。長さは0〜1。
	 * キーボードは8方向、スティックはアナログ(デッドゾーンを除いて0〜1へ詰め直す)。
	 */
	Vector2 GetMove() const
	{
		Vector2 move = m_keyboardMove + m_padMove;
		const float length = move.Length();
		if (length > 1.0f)
			move /= length;
		return move;
	}

	/** 右スティックの倒し具合。x=右、y=上。長さは0〜1。 */
	Vector2 GetLookStick() const { return m_padLook; }

	bool IsGamepadConnected() const { return m_padIndex >= 0; }

	/**
	 * @brief 操作説明をどちらの表記で出すか。ゲームパッドがつながっていればゲームパッド。
	 *
	 * 表記は1つに決めて出す(ユーザー判断 2026-10-25)。
	 * 以前は「最後に触った方」に合わせていたが、両方つながっていると
	 * スティックを倒したままマウスに触れるだけで表記がフレームごとに入れ替わっていた。
	 * パッドをつないでいる人はパッドで遊ぶつもりなので、パッドを優先する。
	 * 入力そのものは、つながっている間もキーボード・マウスで受け付ける(表記だけを決める)。
	 */
	InputDevice GetDisplayDevice() const
	{
		return IsGamepadConnected() ? InputDevice::Gamepad : InputDevice::KeyboardMouse;
	}

	/** 画面に出す、その行動のボタン名。GetDisplayDevice()に合わせる。 */
	const char* GetButtonLabel(GameAction action) const
	{
		const bool pad = GetDisplayDevice() == InputDevice::Gamepad;
		switch (action)
		{
		case GameAction::Attack:      return pad ? "RB" : "左クリック";
		case GameAction::HeavyAttack: return pad ? "RT" : "右クリック";
		case GameAction::Dodge:       return pad ? "A" : "Space";
		case GameAction::Sprint:      return pad ? "LB(押し続ける)" : "Shift(押し続ける)";
		case GameAction::LockOn:      return pad ? "右スティック押し込み" : "Tab";
		case GameAction::Confirm:     return pad ? "A" : "Enter";
		case GameAction::Back:        return pad ? "B" : "Esc";
		default:                      return "";
		}
	}
	const char* GetMoveLabel() const { return GetDisplayDevice() == InputDevice::Gamepad ? "左スティック" : "WASD"; }
	const char* GetLookLabel() const { return GetDisplayDevice() == InputDevice::Gamepad ? "右スティック" : "マウス"; }

private:
	GameInput() = default;

	static int Index(GameAction action) { return static_cast<int>(action); }

	// スティックのデッドゾーン。XInputの推奨値(左7849・右8689 / 32767)とほぼ同じ。
	static constexpr float STICK_DEADZONE = 0.25f;
	// トリガーを「押した」とみなす深さ(0〜1)。
	static constexpr float TRIGGER_THRESHOLD = 0.35f;

	// デッドゾーンを除いて、倒し具合を0〜1へ詰め直す(倒し始めで急に動かないように)。
	static Vector2 ReadStick(SHORT rawX, SHORT rawY)
	{
		Vector2 stick(
			std::clamp(static_cast<float>(rawX) / 32767.0f, -1.0f, 1.0f),
			std::clamp(static_cast<float>(rawY) / 32767.0f, -1.0f, 1.0f));
		const float length = stick.Length();
		if (length <= STICK_DEADZONE)
			return Vector2(0.0f, 0.0f);
		const float scaled = std::min(1.0f, (length - STICK_DEADZONE) / (1.0f - STICK_DEADZONE));
		return stick * (scaled / length);
	}

	void UpdateGamepad()
	{
		m_padPressed.fill(false);
		m_padMove = Vector2(0.0f, 0.0f);
		m_padLook = Vector2(0.0f, 0.0f);

		// つながっていないスロットへの問い合わせは遅いので、未接続の間は1秒ごとに探す。
		if (m_padIndex < 0)
		{
			if (m_searchCooldown > 0)
			{
				--m_searchCooldown;
				return;
			}
			m_searchCooldown = 60;
			for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
			{
				XINPUT_STATE probe{};
				if (XInputGetState(i, &probe) == ERROR_SUCCESS)
				{
					m_padIndex = static_cast<int>(i);
					break;
				}
			}
			if (m_padIndex < 0)
				return;
		}

		XINPUT_STATE state{};
		if (XInputGetState(static_cast<DWORD>(m_padIndex), &state) != ERROR_SUCCESS)
		{
			// 抜かれた。次のフレームから探し直す。
			m_padIndex = -1;
			m_searchCooldown = 0;
			return;
		}

		// ウィンドウが前面でないときは無視する(別のアプリを触っているときに動かないように)。
		if (GetForegroundWindow() != Application::GetWindow())
			return;

		const XINPUT_GAMEPAD& pad = state.Gamepad;
		const auto button = [&pad](WORD mask) { return (pad.wButtons & mask) != 0; };
		const float rightTrigger = static_cast<float>(pad.bRightTrigger) / 255.0f;

		m_padPressed[Index(GameAction::Attack)] = button(XINPUT_GAMEPAD_RIGHT_SHOULDER);
		m_padPressed[Index(GameAction::HeavyAttack)] = rightTrigger > TRIGGER_THRESHOLD;
		m_padPressed[Index(GameAction::Dodge)] = button(XINPUT_GAMEPAD_A);
		m_padPressed[Index(GameAction::Sprint)] = button(XINPUT_GAMEPAD_LEFT_SHOULDER);
		m_padPressed[Index(GameAction::LockOn)] = button(XINPUT_GAMEPAD_RIGHT_THUMB);
		m_padPressed[Index(GameAction::Confirm)] = button(XINPUT_GAMEPAD_A) || button(XINPUT_GAMEPAD_START);
		m_padPressed[Index(GameAction::Back)] = button(XINPUT_GAMEPAD_B);

		m_padMove = ReadStick(pad.sThumbLX, pad.sThumbLY);
		m_padLook = ReadStick(pad.sThumbRX, pad.sThumbRY);
	}

	void UpdateKeyboardMouse()
	{
		auto& input = CInputManager::GetInstance();
		m_keyboardPressed.fill(false);

		// GetAsyncKeyStateはPC全体のキー状態を返すので、ゲームのウィンドウが前面にあるときだけ使う。
		// DirectInputで取りこぼすことがあるため、移動とダッシュはWin32のキー状態でも読む(以前からの対策)。
		const bool foreground = GetForegroundWindow() == Application::GetWindow();
		const auto keyDown = [&input, foreground](int directInputKey, int virtualKey) {
			return input.IsKeyPressed(directInputKey) ||
				(foreground && virtualKey != 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0);
		};
		// キーボードの入力がゲームへ届くか(以前はGameSceneのKeyboardGoesToGame())。
		// 視点をマウスで操作している間(カーソルをゲーム画面へ固定している間)は、必ずゲームへ渡す。
		// デバッグ表示へ文字を打ち込んでいるとき(WantTextInput)だけ、ゲームを止める。
		// ImGuiの`WantCaptureKeyboard`で判断してはいけない。これはデバッグ表示のウィンドウへ
		// キーボードの焦点が移っただけでも真になり、ロックオンのTabがImGuiの焦点移動と重なるため、
		// 一度Tabを押すと移動・ダッシュ・回避がすべて効かなくなっていた(2026-09-25に実機のログで判明)。
		const bool keysToGame = DebugUI::IsCursorLocked() || !ImGui::GetIO().WantTextInput;
		// デバッグ表示の上でのクリックは、ゲームへ渡さない。
		const bool mouseToGame = !ImGui::GetIO().WantCaptureMouse;

		if (mouseToGame)
		{
			m_keyboardPressed[Index(GameAction::Attack)] = input.IsMousePressed(CInputManager::MOUSE_LEFT);
			m_keyboardPressed[Index(GameAction::HeavyAttack)] = input.IsMousePressed(CInputManager::MOUSE_RIGHT);
		}
		if (keysToGame)
		{
			m_keyboardPressed[Index(GameAction::Dodge)] = input.IsKeyPressed(DIK_SPACE);
			m_keyboardPressed[Index(GameAction::Sprint)] = keyDown(DIK_LSHIFT, VK_LSHIFT);
			m_keyboardPressed[Index(GameAction::LockOn)] = input.IsKeyPressed(DIK_TAB);
		}
		m_keyboardPressed[Index(GameAction::Confirm)] =
			input.IsKeyPressed(DIK_RETURN) || input.IsKeyPressed(DIK_SPACE);
		m_keyboardPressed[Index(GameAction::Back)] = input.IsKeyPressed(DIK_ESCAPE);

		const float forward = static_cast<float>(keyDown(DIK_W, 'W')) - static_cast<float>(keyDown(DIK_S, 'S'));
		const float right = static_cast<float>(keyDown(DIK_D, 'D')) - static_cast<float>(keyDown(DIK_A, 'A'));
		m_keyboardMove = Vector2(right, forward);
		if (m_keyboardMove.LengthSquared() > 1.0f)
			m_keyboardMove.Normalize();
	}

	static constexpr int ACTION_COUNT = static_cast<int>(GameAction::Count);
	std::array<bool, ACTION_COUNT> m_keyboardPressed{};
	std::array<bool, ACTION_COUNT> m_padPressed{};
	std::array<bool, ACTION_COUNT> m_isPressed{};
	std::array<bool, ACTION_COUNT> m_wasPressed{};
	Vector2 m_keyboardMove{ 0.0f, 0.0f };
	Vector2 m_padMove{ 0.0f, 0.0f };
	Vector2 m_padLook{ 0.0f, 0.0f };
	int m_padIndex = -1;
	int m_searchCooldown = 0;
};
