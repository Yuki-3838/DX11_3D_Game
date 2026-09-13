#include "DebugUI.h"
#include "renderer.h"
#include <fstream>

namespace
{
bool g_cursorHidden = false;
}

std::vector<std::function<void(void)>> DebugUI::m_debugfunction;
namespace { bool g_debugVisible = false; }
namespace { bool g_cursorVisibleRequested = false; }

void DebugUI::Init(ID3D11Device* device, ID3D11DeviceContext* context) 
{

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io = ImGui::GetIO();
    // キーボードナビは有効にしない。有効にするとデバッグUIを表示している間ずっと
    // io.WantCaptureKeyboard が true になり、回避(Space)やダッシュ(LShift)など
    // WantCaptureKeyboardで入力を止めているゲーム側の操作が効かなくなるためである。
    // テキスト入力中は従来どおりWantCaptureKeyboardがtrueになるので、
    // デバッグUIへの文字入力とゲーム操作の取り合いは起きない。
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Allow ImGui windows to detach outside the game window.
    // ドッキングを有効にする。デバッグウィンドウのタイトルバーを別のウィンドウへ
    // 重ねると1つにまとまり、タブで切り替えられるようになる。
    // 画面全体を覆うドックスペースは作らない。作るとゲーム画面の上に
    // 透明な受け皿が乗り、左クリック攻撃などの入力を吸ってしまうためである。
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Dear ImGuiの表示スタイルを初期化する。
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.MergeMode = false;
    // デバッグUIは日本語で表示するため、日本語の字形を持つフォントが要る。
    // 同梱フォントを先に試すのは、別のPCで確実に読めるようにするためである
    // (メイリオは日本語版Windowsにしか無い)。どれも読めなければ
    // ImGuiの既定フォントへ落とす。既定フォントには日本語が無いので
    // 文字は出ないが、少なくとも起動はできる。
    const char* fontCandidates[] = {
        "assets/font/NotoSansJP-Light.otf",
        "C:\\Windows\\Fonts\\meiryo.ttc",
        "C:\\Windows\\Fonts\\YuGothM.ttc",
        "C:\\Windows\\Fonts\\msgothic.ttc",
    };
    ImFont* debugFont = nullptr;
    for (const char* path : fontCandidates)
    {
        // 存在しないファイルを渡すとImGuiがアサートで止まるため、先に開いて確かめる。
        std::ifstream probe(path, std::ios::binary);
        if (!probe.is_open())
            continue;
        probe.close();
        debugFont = io.Fonts->AddFontFromFileTTF(
            path,
            18.0f,
            &cfg,
            io.Fonts->GetGlyphRangesJapanese()   // Japanese glyph range
        );
        if (debugFont != nullptr)
            break;
    }
    if (debugFont == nullptr)
        io.Fonts->AddFontDefault();
    // DX11バックエンドがテクスチャ設定を登録した後にフォントアトラスを構築する。

    // プラットフォーム用とレンダラー用のバックエンドを初期化する。
    ImGui_ImplWin32_Init(Application::GetWindow());
    ImGui_ImplDX11_Init(device, context);
}

void DebugUI::DisposeUI() {
    // デバッグUIを終了して使用したリソースを解放する。
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_cursorHidden)
    {
        ShowCursor(TRUE);
        g_cursorHidden = false;
    }
}
// デバッグウィンドウを登録する。
void DebugUI::RedistDebugFunction(std::function<void(void)> f) {
    m_debugfunction.push_back(std::move(f));
}

void DebugUI::ClearDebugFunctions() {
    m_debugfunction.clear();
}

void DebugUI::SetVisible(bool visible) {
    g_debugVisible = visible;
}

void DebugUI::ToggleVisible() {
    g_debugVisible = !g_debugVisible;
}

void DebugUI::SetCursorVisible(bool visible) {
    g_cursorVisibleRequested = visible;
}

void DebugUI::BeginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const bool gameIsForeground = GetForegroundWindow() == Application::GetWindow();
    const bool showCursor = g_cursorVisibleRequested ||
        !gameIsForeground || ImGui::GetIO().WantCaptureMouse;
    if (showCursor && g_cursorHidden)
    {
        ShowCursor(TRUE);
        g_cursorHidden = false;
    }
    else if (!showCursor && !g_cursorHidden)
    {
        ShowCursor(FALSE);
        g_cursorHidden = true;
    }
}

void DebugUI::Render() {
    if (g_debugVisible)
    {
        // 位置と大きさは初回だけ決める。Alwaysで固定すると、
        // 他のウィンドウへドッキングしてまとめることができなくなる。
        // マルチビューポートが有効なので、位置は画面全体の座標になる。
        // 小さな値をそのまま渡すとゲームウィンドウの外へ別ウィンドウとして開くため、
        // ゲーム画面の左上を基準にする。
        // 左上はゲームの体力・スタミナゲージが使っているため、右上へ置く。
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(mainViewport->WorkPos.x + mainViewport->WorkSize.x - 290.0f,
                   mainViewport->WorkPos.y + 10.0f),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280.0f, 96.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("動作状況");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("フレームレート: %.1f fps", io.Framerate);
        ImGui::Text("1フレームの時間: %.3f ミリ秒", 1000.0f / io.Framerate);
        ImGui::End();

        for (auto& f : m_debugfunction)
        {
            f();
        }
    }
    // ImGuiフレームを確定して画面へ描画する。
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    Renderer::RestoreMainRenderTarget();
}
