#include "DebugUI.h"
#include "renderer.h"

namespace
{
bool g_cursorHidden = false;
}

std::vector<std::function<void(void)>> DebugUI::m_debugfunction;

void DebugUI::Init(ID3D11Device* device, ID3D11DeviceContext* context) 
{

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Allow ImGui windows to detach outside the game window.

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.MergeMode = false;
    // Meiryo or Yu Gothic font for Windows debug UI
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\meiryo.ttc",
        18.0f,
        &cfg,
        io.Fonts->GetGlyphRangesJapanese()   // Japanese glyph range
    );
    // The DX11 backend builds the atlas after it registers its texture flags.

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(Application::GetWindow());
    ImGui_ImplDX11_Init(device, context);
}

void DebugUI::DisposeUI() {
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_cursorHidden)
    {
        ShowCursor(TRUE);
        g_cursorHidden = false;
    }
}
// Debug window registration
void DebugUI::RedistDebugFunction(std::function<void(void)> f) {
    m_debugfunction.push_back(std::move(f));
}

void DebugUI::ClearDebugFunctions() {
    m_debugfunction.clear();
}

void DebugUI::Render() {
    // Start a new ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const bool gameIsForeground = GetForegroundWindow() == Application::GetWindow();
    const bool showCursor = !gameIsForeground || ImGui::GetIO().WantCaptureMouse;
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
    // Draw debug windows
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260.0f, 82.0f), ImGuiCond_Always);
    ImGui::Begin("Debug Information", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);

    ImGui::End();
// Debug window registration
    for (auto& f : m_debugfunction)
    {
        f();
    }
    // Finish and render the frame
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    Renderer::RestoreMainRenderTarget();
}
