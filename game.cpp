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
bool LoadDevModeSetting()
{
	std::ifstream input("dev_settings.ini");
	std::string line;
	while (std::getline(input, line))
	{
		if (line.rfind("devmode=", 0) == 0)
		{
			return line.substr(8) == "1";
		}
	}
	return false;
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
	SceneManager::SetCurrentScene(devMode ? "GameScene" : "TitleScene");
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

    // F1：ゲームシーン、F2：車モデルシーン、F3：モーションエディター、F4：デバッグ表示切り替え
    if (input.IsKeyTriggered(DIK_F1))
    {
        SceneManager::SetCurrentScene("GameScene");
    }
    else if (input.IsKeyTriggered(DIK_F2))
    {
        SceneManager::SetCurrentScene("CarScene");
    }
    else if (input.IsKeyTriggered(DIK_F3))
    {
        SceneManager::SetCurrentScene("MotionEditorScene");
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
