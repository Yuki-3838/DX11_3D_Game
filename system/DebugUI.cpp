#include "DebugUI.h"
#include "renderer.h"

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

    // Dear ImGuiの表示スタイルを初期化する。
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.MergeMode = false;
    // WindowsのデバッグUI用にメイリオまたは游ゴシックを読み込む。
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\meiryo.ttc",
        18.0f,
        &cfg,
        io.Fonts->GetGlyphRangesJapanese()   // Japanese glyph range
    );
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
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(260.0f, 82.0f), ImGuiCond_Always);
        ImGui::Begin("Debug Information", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
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
