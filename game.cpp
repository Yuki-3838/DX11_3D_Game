#include	<cstdint>
#include    <string>
#include	<fstream>
#include	"system/renderer.h"
#include    "system/DebugUI.h"
#include    "system/CDirectInput.h"
#include	"system/scenemanager.h"
#include	"fpscontrol.h"
#include	"system/Inputmanager.h"
#include    "system/GameFlow.h"
#include    "system/SoundManager.h"

namespace
{
// dev_settings.ini(未コミット・ローカル専用)に devmode=1 と書くと、
// 起動時にタイトルを飛ばしてGameSceneへ直行し、ImGuiデバッグ表示も最初から出す。
// ファイルが無い/devmode=0なら、提出用と同じ挙動(タイトルから開始・デバッグ非表示)になる。
// 提出前にこのファイルを削除・devmode=0にする必要はない(存在しなければ何も変わらない)。
// dev_settings.iniから1つの設定を読む。無ければ空文字。
std::string LoadSetting(const std::string& key)
{
	std::ifstream input("dev_settings.ini");
	std::string line;
	const std::string prefix = key + "=";
	while (std::getline(input, line))
	{
		if (line.rfind(prefix, 0) != 0)
			continue;
		std::string value = line.substr(prefix.size());
		while (!value.empty() &&
			(value.back() == '\r' || value.back() == '\n' ||
			 value.back() == ' ' || value.back() == '\t'))
		{
			value.pop_back();
		}
		return value;
	}
	return {};
}

bool LoadDevModeSetting()
{
	return LoadSetting("devmode") == "1";
}

// タイトルとリザルトを使うか(ゲームループ)。
//   gameloop=1 : タイトル → ゲーム → リザルト → タイトル
//   gameloop=0 : ゲーム本編だけ。決着がついたら同じ戦いをやり直す。
// 書いていない場合は、これまでどおり devmode で決める
// (devmode=1 ならゲーム本編だけ、そうでなければゲームループ)。
bool LoadGameLoopSetting(bool devMode)
{
	const std::string value = LoadSetting("gameloop");
	if (value.empty())
		return !devMode;
	return value != "0";
}
}

void gameinit()
{
	// レンダラの初期化
	Renderer::Init();

	// DirectInputの初期化
	CDirectInput::GetInstance().Init(Application::GetHInstance(), 
		Application::GetWindow(),
		Application::GetWidth(),
		Application::GetHeight());

	// デバッグUIの初期化
	DebugUI::Init(Renderer::GetDevice(), Renderer::GetDeviceContext());
	SoundManager::Init();

	// シーンマネージャの初期化
	SceneManager::Init();

	//　シーン選択
	// 通常の起動はタイトルシーンから開始する。
	// F1はデバッグ用にゲームシーンへ直接移るショートカットとして残す。
	const bool devMode = LoadDevModeSetting();
	GameFlow::useGameLoop = LoadGameLoopSetting(devMode);
	// ゲームループを使うならタイトルから。本編だけなら直接ゲームへ。
	SceneManager::SetCurrentScene(GameFlow::useGameLoop ? "TitleScene" : "GameScene");
	if (devMode)
	{
		DebugUI::SetVisible(true);
	}

}

void gameupdate(uint64_t deltatime)
{
    auto& input = CInputManager::GetInstance();
    input.Update();
    SoundManager::Update();

    // F1：ゲームシーン、F2：車モデルシーン、F4：デバッグ表示切り替え
    // F3のゲーム内モーションエディター(MotionEditorScene)は2026-09-15にユーザー判断で外し、
    // 2026-09-24にシーンごと削除した。外部ツール(tools/MotionEditor.Wpf)と役割が重複しているため。
    // 戻したい場合はgitの履歴から取り出すこと。
    if (input.IsKeyTriggered(DIK_F1))
    {
        SceneManager::SetCurrentScene("GameScene");
    }
    else if (input.IsKeyTriggered(DIK_F2))
    {
        SceneManager::SetCurrentScene("CarScene");
    }
    else if (input.IsKeyTriggered(DIK_F4))
    {
        DebugUI::ToggleVisible();
    }

    SceneManager::Update(deltatime);

    const std::string requestedScene = GameFlow::ConsumeRequestedScene();
    if (!requestedScene.empty())
    {
        SceneManager::SetCurrentScene(requestedScene);
    }
    // 本編だけのとき、決着後に同じ戦いをやり直す。
    // シーンを作り直すので、体力も位置もAIの状態も初期化される。
    else if (GameFlow::ConsumeRestart())
    {
        SceneManager::SetCurrentScene(SceneManager::GetCurrentSceneName(), true);
    }
}

void gamedraw(uint64_t deltatime) 
{
	// レンダリング前処理
	Renderer::Begin();
	DebugUI::BeginFrame();

	// シーンマネージャの描画
	SceneManager::Draw(deltatime);

	// デバッグUIの描画
	DebugUI::Render();

	// レンダリング後処理
	Renderer::End();
}

void gamedispose() 
{
	// デバッグUIの終了処理
	DebugUI::DisposeUI();

	// シーンマネージャの終了処理
	SceneManager::Dispose();
	SoundManager::Shutdown();

	// レンダラの終了処理
	Renderer::Dispose();

}

void gameloop()
{
	uint64_t delta_time = 0;

	// フレームの待ち時間を計算する
	static FPS fpsrate(65);

	// 前回実行されてからの経過時間を計算する
	delta_time = fpsrate.BeginFrame();

	// 更新処理、描画処理を呼び出す
	gameupdate(delta_time);
	gamedraw(delta_time);

	// 規定時間までWAIT
	fpsrate.EndFrame();

}
