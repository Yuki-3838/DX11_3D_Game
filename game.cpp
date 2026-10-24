#include	<cstdint>
#include    <string>
#include	<fstream>
#include	<cwchar>
#include	<windows.h>
#include	<shellapi.h>
#pragma comment(lib, "shell32.lib")
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
// dev_settings.ini(未コミット・ローカル専用)から1つの設定を読む。無ければ空文字。
// ファイルが無ければ提出用と同じ挙動になるので、提出前に消す必要はない。
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

// コマンドライン引数に指定の文字列があるか。
// VSではツールバーのコマンドライン引数欄(またはプロジェクトのプロパティ→デバッグ→コマンド引数)で渡す。
// __argv はこの環境では null のまま(読むとアクセス違反)なので、Windowsから直接受け取る。
bool HasArg(const wchar_t* name)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr)
		return false;
	bool found = false;
	for (int i = 1; i < argc && !found; ++i)
	{
		found = (std::wcscmp(argv[i], name) == 0);
	}
	LocalFree(argv);
	return found;
}

// いまの進行の形をウィンドウのタイトルに出す。
// 画面へ文字を描く仕組みを増やさずに、どちらで動いているか一目で分かるようにする。
void UpdateWindowTitle()
{
	SetWindowText(
		Application::GetWindow(),
		GameFlow::useGameLoop
			? TEXT("就職作品")
			: TEXT("就職作品 ［本編だけ］ F5でゲームループへ"));
}

// 「進行の形」と「デバッグ表示」は別々に決める。
// どちらも 引数 → dev_settings.ini → 既定(提出用) の順に見る。
// 引数も設定ファイルも無ければ提出用と同じ挙動(ゲームループ・デバッグ非表示)。

// タイトルとリザルトを使うか(ゲームループ)。
//   --loop / gameloop=1 : タイトル → ゲーム → リザルト → タイトル
//   --game / gameloop=0 : ゲーム本編だけ。決着がついたら同じ戦いをやり直す。
bool ResolveUseGameLoop()
{
	if (HasArg(L"--game"))
		return false;
	if (HasArg(L"--loop"))
		return true;
	return LoadSetting("gameloop") != "0";
}

// 起動時からデバッグ表示を出すか。
//   --debug   / devmode=1 : 出す
//   --nodebug / devmode=0 : 出さない
bool ResolveDebugVisible()
{
	if (HasArg(L"--nodebug"))
		return false;
	if (HasArg(L"--debug"))
		return true;
	return LoadSetting("devmode") == "1";
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
	GameFlow::useGameLoop = ResolveUseGameLoop();
	// ゲームループを使うならタイトルから。本編だけなら直接ゲームへ。
	SceneManager::SetCurrentScene(GameFlow::useGameLoop ? "TitleScene" : "GameScene");
	UpdateWindowTitle();
	if (ResolveDebugVisible())
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
    // F5：ゲームループ(タイトル→ゲーム→リザルト)と本編だけの切り替え
    //     デバッグ表示とは独立している(デバッグ表示は F4)
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
    else if (input.IsKeyTriggered(DIK_F5))
    {
        // 進行の形だけを切り替える(デバッグ表示はそのまま)。
        // 引数や設定ファイルを渡せないPC(お披露目先など)でも、その場で切り替えられるようにする。
        // 決着がついた瞬間にこの値を見るので、戦闘中に押しても次の決着から効く。
        GameFlow::useGameLoop = !GameFlow::useGameLoop;
        UpdateWindowTitle();
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
