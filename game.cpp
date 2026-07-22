#include	<cstdint>
#include	"system/renderer.h"
#include    "system/DebugUI.h"
#include    "system/CDirectInput.h"
#include	"system/scenemanager.h"
#include	"fpscontrol.h"
#include	"system/Inputmanager.h"

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

	// シーンマネージャの初期化
	SceneManager::Init();

	//　シーン選択
	SceneManager::SetCurrentScene("GameScene");

}

void gameupdate(uint64_t deltatime)
{
    auto& input = CInputManager::GetInstance();
    input.Update();

    // F1: 壁・衝突デバッグシーン、F2: 車モデル確認シーン
    if (input.IsKeyTriggered(DIK_F1))
    {
        SceneManager::SetCurrentScene("GameScene");
    }
    else if (input.IsKeyTriggered(DIK_F2))
    {
        SceneManager::SetCurrentScene("CarScene");
    }

    SceneManager::Update(deltatime);
}

void gamedraw(uint64_t deltatime) 
{
	// レンダリング前処理
	Renderer::Begin();

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