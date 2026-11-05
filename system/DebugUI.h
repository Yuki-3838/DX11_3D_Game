#pragma once

#include <d3d11.h>
#include <vector>
#include <functional>
#include "../Application.h"

#include "imGui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

class DebugUI {
    static std::vector<std::function<void(void)>> m_debugfunction;
public:

    static void Init(ID3D11Device* device, ID3D11DeviceContext* context);
// デバッグウィンドウの登録
    static void RedistDebugFunction(std::function<void(void)> f);

    static void ClearDebugFunctions();

	static void SetVisible(bool visible);
	static void ToggleVisible();
	// 戦闘を始めるたびにデバッグ表示を出すか(起動時の devmode / --debug / --nodebug で決める)。
	// 戦闘のシーンは作り直されるたびに表示を決め直すので、起動時の1回だけ出すと
	// 倒されてやり直した2回目以降に消えてしまう。そのため値として覚えておく。
	static void SetGameplayDefaultVisible(bool visible);
	static bool IsGameplayDefaultVisible();
	static void SetCursorVisible(bool visible);
	// マウスで視点を回す間、カーソルをゲーム画面の中に閉じ込める。
	// 閉じ込めている間はデバッグ表示をマウスで操作できない(ImGuiにマウスを渡さない)。
	// カーソルを表示する要求(SetCursorVisible(true))が優先され、ゲームが前面にないときも外す。
	static void SetCursorLocked(bool locked);
	static bool IsCursorLocked();

    static void BeginFrame();

    static void Render();

    static void DisposeUI();
};

