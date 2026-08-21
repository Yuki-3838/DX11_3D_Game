#include "ResultScene.h"

#include "../Application.h"
#include "../system/GameFlow.h"
#include "../system/Inputmanager.h"
#include "../system/imgui/imgui.h"
#include <dinput.h>

void ResultScene::update(uint64_t)
{
    auto& input = CInputManager::GetInstance();
    if (input.IsKeyTriggered(DIK_RETURN) || input.IsKeyTriggered(DIK_SPACE))
    {
        GameFlow::RequestScene("GameScene");
    }
    else if (input.IsKeyTriggered(DIK_ESCAPE))
    {
        GameFlow::RequestScene("TitleScene");
    }
}

void ResultScene::draw(uint64_t)
{
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
            GameFlow::RequestScene("GameScene");
        }
        ImGui::Spacing();
        ImGui::Text("Enter / Space: リトライ   Esc: タイトルへ");
    }
    ImGui::End();
}

void ResultScene::init() {}
void ResultScene::dispose() {}
