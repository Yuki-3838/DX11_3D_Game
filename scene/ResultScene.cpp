#include "ResultScene.h"

#include "../Application.h"
#include "../system/DebugUI.h"
#include "../system/GameFlow.h"
#include "../system/Inputmanager.h"
#include "../system/GameInput.h"
#include "../system/SoundManager.h"
#include "../system/imgui/imgui.h"
#include <dinput.h>

void ResultScene::update(uint64_t deltatime)
{
    m_elapsedSeconds += static_cast<float>(deltatime) / 1000000.0f;
    if (GameFlow::autoStart && m_elapsedSeconds > 1.0f)
        m_retrying = true;
    // リトライ。シーンを切り替えて飛来の場面が出るまでの短い間も、リザルトの窓を見せないよう暗くしておく。
    if (m_retrying)
    {
        GameFlow::RequestFlightIntro();
        GameFlow::RequestScene("GameScene");
        return;
    }
    // キーボード(Enter・Space / Esc)でもゲームパッド(A・Start / B)でも進める。
    const GameInput& input = GameInput::Get();
    if (input.IsTriggered(GameAction::Confirm))
    {
        m_retrying = true;
    }
    else if (input.IsTriggered(GameAction::Back))
    {
        GameFlow::RequestScene("TitleScene");
    }
}

void ResultScene::draw(uint64_t)
{
    if (m_retrying)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::GetForegroundDrawList(viewport)->AddRectFilled(
            viewport->WorkPos,
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y),
            IM_COL32(0, 0, 0, 255));
        return;
    }
    ImGui::SetNextWindowPos(
        ImVec2(Application::GetWidth() * 0.5f, Application::GetHeight() * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 230.0f), ImGuiCond_Always);

    if (ImGui::Begin("Hunt Result", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        const bool victory = GameFlow::GetResult() == GameFlow::Result::Victory;
        ImGui::Spacing();
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextWrapped(victory ? "HUNT COMPLETE" : "HUNT FAILED");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        ImGui::TextWrapped(victory ? "ドラゴンを討伐した！" : "力尽きてしまった……");
        ImGui::Spacing();
        if (ImGui::Button("RETRY", ImVec2(-1.0f, 48.0f)))
        {
            m_retrying = true;
        }
        ImGui::Spacing();
        // ゲームパッドがつながっていればパッドのボタン名を出す。
        const GameInput& input = GameInput::Get();
        if (input.GetDisplayDevice() == InputDevice::Gamepad)
            ImGui::Text("%s: リトライ   %s: タイトルへ",
                input.GetButtonLabel(GameAction::Confirm), input.GetButtonLabel(GameAction::Back));
        else
            ImGui::Text("Enter / Space: リトライ   Esc: タイトルへ");
    }
    ImGui::End();
}

void ResultScene::init()
{
    // リザルト画面では戦闘中のデバッグ表示を出さず、マウス操作を有効にする。
    DebugUI::SetVisible(false);
    DebugUI::SetCursorVisible(true);
    SoundManager::StopBgm();
}

void ResultScene::dispose() {}
