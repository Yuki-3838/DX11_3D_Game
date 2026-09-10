#include "TitleScene.h"

#include "../system/GameFlow.h"
#include "../system/Inputmanager.h"
#include "../system/CShader.h"
#include "../system/DebugUI.h"
#include "../system/SoundManager.h"
#include "../system/meshmanager.h"
#include "../system/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <Windows.h>

namespace
{
    constexpr const char* kGuildAssetDirectory = "assets/model/KayKitGuild/";
    constexpr float kWalkStartSeconds = 2.20f;
    constexpr float kWalkDurationSeconds = 3.10f;

    void StartTitleTransition(bool& starting, float& transitionSeconds)
    {
        starting = true;
        transitionSeconds = 0.0f;
    }
}

float TitleScene::SmoothStep(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vector3 TitleScene::Lerp(const Vector3& from, const Vector3& to, float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return Vector3(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t);
}

void TitleScene::DrawStaticMesh(CStaticMeshRenderer& renderer, const SRT& transform)
{
    Matrix4x4 world = transform.GetMatrix();
    Renderer::SetWorldMatrix(&world);
    ShaderManager::Get<CShader>("Shader3D")->SetGPU();
    renderer.Draw();
}

void TitleScene::update(uint64_t deltatime)
{
    const float deltaSeconds = static_cast<float>(deltatime) / 1000000.0f;
    m_timeSeconds += deltaSeconds;

    if (m_starting)
    {
        m_transitionSeconds += deltaSeconds;

        const Vector3 startingPosition(20.0f, 0.75f, 5.0f);
        // テーブルの手前を通る接近経路にする。
        // 椅子の占有範囲を通らない位置へ目標地点を置き、受け渡し時に
        // プレイヤーが椅子やテーブルを突き抜けて見えないようにする。
        const Vector3 contractPosition(10.0f, 0.75f, 4.5f);
        // 剣を納めてから歩き始める。移動距離を短くして速度を抑えることで、
        // 足運びが飛んで見えないようにする。
        // 画面外まで歩かせ、画面端でキャラクターが途切れないようにする。
        const Vector3 exitPosition(50.0f, 0.75f, -42.0f);
        // 歩きモーションの進行方向であるローカル-Z軸を退出方向へ合わせ、
        // 移動補間の向きと足の向きがずれて横滑りに見えないようにする。
        const Vector3 exitDirection = exitPosition - contractPosition;
        const float walkHeading = std::atan2(-exitDirection.x, -exitDirection.z);

        if (m_transitionSeconds < 0.85f)
        {
            const float progress = SmoothStep(m_transitionSeconds / 0.85f);
            m_playerSrt.pos = Lerp(startingPosition, contractPosition, progress);
        }
        else if (m_transitionSeconds < kWalkStartSeconds)
        {
            m_playerSrt.pos = contractPosition;
        }
        else
        {
            const float progress = SmoothStep(
                (m_transitionSeconds - kWalkStartSeconds) / kWalkDurationSeconds);
            m_playerSrt.pos = Lerp(contractPosition, exitPosition, progress);
            // モデルの前方向を移動方向へ合わせ、接地した足が同じ方向へ進むようにする。
            m_playerSrt.rot.y = walkHeading;
        }

        if (m_playerAnimationMesh)
        {
            const bool useWalkAnimation =
                m_transitionSeconds >= kWalkStartSeconds &&
                m_playerWalkAnimation != nullptr;
            if (useWalkAnimation)
            {
                // FBXのキーフレームを30fps相当で進める。
                // タイトル演出の移動速度が遅くても歩きが速くなりすぎないようにする。
                if ((m_playerWalkTick++ % 2) == 0)
                    ++m_playerWalkFrame;
            }
            const float reachAmount = SmoothStep((m_transitionSeconds - 0.42f) / 0.78f);
            const float sheatheAmount = SmoothStep(m_transitionSeconds / 0.45f);
            const float drawAmount = SmoothStep((m_transitionSeconds - 1.52f) / 0.72f);
            // 上半身はタイトル専用の受け渡し姿勢を維持し、退出時だけ
            // 読み込んだ歩きモーションで脚と足を動かす。
            m_playerAnimator.UpdateTitleContractPose(
                *m_playerAnimationMesh,
                m_playerBoneComb,
                reachAmount,
                sheatheAmount,
                drawAmount,
                std::max(0.0f, m_timeSeconds - kWalkStartSeconds),
                useWalkAnimation ? m_playerWalkAnimation : nullptr,
                m_playerWalkFrame);
        }

        // キャラクターが画面外へ出た後にゲームシーンへ切り替える。
        if (m_transitionSeconds >= kWalkStartSeconds + kWalkDurationSeconds + 0.15f)
        {
            GameFlow::RequestScene("GameScene");
        }
        return;
    }

    auto& input = CInputManager::GetInstance();
    if (input.IsKeyTriggered(DIK_RETURN) || input.IsKeyTriggered(DIK_SPACE))
    {
        StartTitleTransition(m_starting, m_transitionSeconds);
    }
}

void TitleScene::DrawQuestDocument()
{
    // 契約書をテーブル手前右側へ置き、テーブル全体を横切らずに
    // 自然な距離で手を伸ばせるようにする。
    const Vector3 tablePosition(4.5f, 17.2f, 8.0f);
    Vector3 handPosition(m_playerSrt.pos.x + 3.5f,
        m_playerSrt.pos.y + 16.0f,
        m_playerSrt.pos.z + 0.8f);
    if (m_playerAnimationMesh)
    {
        const auto& bones = m_playerAnimationMesh->GetDebugBoneMatrices();
        const auto handIt = bones.find("mixamorig:LeftHand");
        if (handIt != bones.end())
        {
            // デバッグ用ボーン行列はプレイヤーのローカル空間なので、
            // タイトル姿勢とプレイヤーのSRTを合成した実際の手の位置を使う。
            // これにより契約書が腕の横や内部に浮かないようにする。
            const Matrix4x4 handWorld = handIt->second * m_playerSrt.GetMatrix();
            handPosition = Vector3::Transform(
                Vector3(0.0f, 0.10f, 0.04f), handWorld);
            // 手の位置は正確なまま保持し、紙を腕の外側へ出すための余白だけを
            // 後段で加える。手の目標位置自体は動かさない。
        }
    }
    const float takeProgress = m_starting
        ? SmoothStep((m_transitionSeconds - 0.85f) / 0.65f)
        : 0.0f;

    SRT document{};
    // 契約書はテーブル上では水平に置き、手へ渡すときに立てる。
    // カメラに近い向きを保ち、紙が板の側面に見えないようにする。
    document.rot = Vector3(-1.57f * takeProgress,
        0.02f * takeProgress, 0.10f * takeProgress);
    const Matrix4x4 documentRotation = Matrix4x4::CreateFromYawPitchRoll(
        document.rot.y, document.rot.x, document.rot.z);

    // 自由な手に近い紙の下角を握り位置にする。
    // 紙を胴体の前へ広げ、腕の横に垂れ下がらないようにする。
    // 握り位置を紙のローカル座標で変換するため、歩行中も手と紙の角が一致する。
    const Vector3 localGripPoint(2.15f, 0.0f, -2.45f);
    Vector3 documentHandPosition = handPosition -
        Vector3::TransformNormal(localGripPoint, documentRotation);
    // 運搬中はカメラ側へ少しだけ離す。
    // 歩行姿勢で手が胴体へ入り込んでも深度テストで紙が隠れないようにする。
    // 補正量は小さくし、紙がキャラクター全体を覆わないようにする。
    documentHandPosition += Vector3(0.0f, 0.0f, -0.30f);
    Vector3 towardCamera = m_camera.GetPosition() - documentHandPosition;
    if (towardCamera.LengthSquared() > 0.0001f)
    {
        towardCamera.Normalize();
        documentHandPosition += towardCamera * (1.60f * takeProgress);
    }
    document.pos = Lerp(tablePosition, documentHandPosition, takeProgress);
    const Matrix4x4 documentWorld = document.GetMatrix();
    m_questDocument.Draw(documentWorld, Color(0.95f, 0.80f, 0.52f, 1.0f));

    // 紐と封蝋は紙の子要素として紙の行列で描画する。
    // ワールド座標の固定オフセットにすると、紙を傾けたときにずれるためである。
    const Matrix4x4 bandWorld =
        Matrix4x4::CreateTranslation(0.18f, 0.14f, 0.0f) * documentWorld;
    m_questDocumentBand.Draw(bandWorld, Color(0.31f, 0.07f, 0.05f, 1.0f));

    const Matrix4x4 sealWorld =
        Matrix4x4::CreateTranslation(0.58f, 0.28f, 0.82f) * documentWorld;
    m_questSeal.Draw(sealWorld, Color(0.62f, 0.07f, 0.04f, 1.0f));
}

void TitleScene::draw(uint64_t)
{
    m_camera.Draw();

    SRT foundation{};
    foundation.pos = Vector3(0.0f, -0.55f, 8.0f);
    m_guildFoundation.Draw(foundation, Color(0.09f, 0.06f, 0.055f, 1.0f));

    SRT backdrop{};
    backdrop.pos = Vector3(0.0f, 24.0f, 37.0f);
    m_guildBackdrop.Draw(backdrop, Color(0.075f, 0.055f, 0.065f, 1.0f));
    SRT ceiling{};
    ceiling.pos = Vector3(0.0f, 48.0f, 10.0f);
    m_guildCeiling.Draw(ceiling, Color(0.11f, 0.075f, 0.055f, 1.0f));

    SRT floor{};
    floor.pos = Vector3(0.0f, 0.0f, 0.0f);
    floor.scale = Vector3(12.0f, 12.0f, 12.0f);
    DrawStaticMesh(m_guildFloorRenderer, floor);

    SRT backWall{};
    backWall.pos = Vector3(0.0f, 0.0f, 24.0f);
    backWall.scale = Vector3(12.0f, 12.0f, 12.0f);
    DrawStaticMesh(m_guildBackWallRenderer, backWall);

    SRT sideWall{};
    sideWall.pos = Vector3(-30.0f, 0.0f, 20.0f);
    sideWall.scale = Vector3(12.0f, 12.0f, 12.0f);
    DrawStaticMesh(m_guildSideWallRenderer, sideWall);
    SRT oppositeWall = sideWall;
    oppositeWall.pos = Vector3(30.0f, 0.0f, 20.0f);
    oppositeWall.rot.y = 3.14159f;
    DrawStaticMesh(m_guildSideWallRenderer, oppositeWall);

    SRT doorway{};
    doorway.pos = Vector3(22.0f, 0.0f, 24.0f);
    doorway.scale = Vector3(12.0f, 12.0f, 12.0f);
    DrawStaticMesh(m_guildDoorwayRenderer, doorway);

    SRT table{};
    table.pos = Vector3(0.0f, 0.65f, 10.0f);
    table.scale = Vector3(7.0f, 7.0f, 7.0f);
    DrawStaticMesh(m_guildTableRenderer, table);

    SRT chair{};
    chair.pos = Vector3(-11.0f, 0.65f, 8.0f);
    chair.scale = Vector3(10.0f, 10.0f, 10.0f);
    chair.rot.y = 1.57f;
    DrawStaticMesh(m_guildChairRenderer, chair);

    SRT secondChair = chair;
    secondChair.pos = Vector3(15.0f, 0.65f, 13.0f);
    secondChair.rot.y = -1.57f;
    DrawStaticMesh(m_guildChairRenderer, secondChair);

    SRT banner{};
    banner.pos = Vector3(12.0f, 15.0f, 23.2f);
    banner.scale = Vector3(8.0f, 8.0f, 8.0f);
    DrawStaticMesh(m_guildBannerRenderer, banner);

    SRT torch{};
    torch.pos = Vector3(-17.5f, 12.0f, 22.0f);
    torch.scale = Vector3(12.0f, 12.0f, 12.0f);
    torch.rot.y = -1.57f;
    DrawStaticMesh(m_guildTorchRenderer, torch);

    SRT barrel{};
    barrel.pos = Vector3(16.0f, 0.65f, 19.0f);
    barrel.scale = Vector3(8.0f, 8.0f, 8.0f);
    DrawStaticMesh(m_guildBarrelRenderer, barrel);

    SRT shelf{};
    shelf.pos = Vector3(-17.5f, 15.0f, 22.0f);
    shelf.scale = Vector3(12.0f, 12.0f, 12.0f);
    DrawStaticMesh(m_guildShelfRenderer, shelf);

    SRT chest{};
    chest.pos = Vector3(15.0f, 0.65f, 11.0f);
    chest.scale = Vector3(8.0f, 8.0f, 8.0f);
    DrawStaticMesh(m_guildChestRenderer, chest);

    // プレイヤーモデル内の剣はスキニングメッシュに含まれているため、
    // 受け渡しを分かりやすくする暗い鞘を別モデルとして描画する。
    // タイトル姿勢では剣を持つ腕を鞘の位置まで下げる。
    SRT sheath{};
    sheath.pos = Vector3(m_playerSrt.pos.x - 2.7f, m_playerSrt.pos.y + 6.7f,
        m_playerSrt.pos.z + 0.7f);
    sheath.rot = Vector3(0.0f, 0.0f, -0.38f);
    sheath.scale = Vector3(0.78f, 0.62f, 0.78f);
    m_sheath.Draw(sheath, Color(0.12f, 0.035f, 0.018f, 1.0f));

    // 紙をキャラクターより先に描画する。
    // 小さなカメラ側補正で胴体の前に紙を置き、前腕と手は深度テストで
    // 紙の下角より手前に表示する。
    DrawQuestDocument();

    if (m_playerAnimationMesh)
    {
        Matrix4x4 world = m_playerSrt.GetMatrix();
        Renderer::SetWorldMatrix(&world);
        ShaderManager::Get<CShader>("Shader3DSkin")->SetGPU();
        m_playerBoneComb.Update();
        m_playerBoneComb.SetGPU();
        m_playerAnimationMesh->Draw();
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    const ImVec2 min = viewport->WorkPos;
    const ImVec2 max(min.x + viewport->WorkSize.x, min.y + viewport->WorkSize.y);
    const float w = viewport->WorkSize.x;
    const float h = viewport->WorkSize.y;
    const ImU32 gold = IM_COL32(224, 190, 128, 255);

    // タイトル画面は3Dのギルド背景だけを残し、メニューはSTARTとENDに絞る。
    drawList->AddRectFilled(min, max, IM_COL32(7, 9, 15, 38));

    // タイトル画面の作品名を中央上部へ表示する。
    // メニューの情報量は増やさず、作品名だけをタイトルとして見せる。
    const char* gameTitle = "DragonHunt";
    ImFont* titleFont = ImGui::GetFont();
    const float titleFontSize = 56.0f;
    const ImVec2 titleSize = titleFont->CalcTextSizeA(
        titleFontSize, 1000.0f, 0.0f, gameTitle);
    const ImVec2 titlePosition(
        min.x + (w - titleSize.x) * 0.5f,
        min.y + h * 0.12f);
    drawList->AddText(
        titleFont,
        titleFontSize,
        ImVec2(titlePosition.x + 2.0f, titlePosition.y + 3.0f),
        IM_COL32(0, 0, 0, 180),
        gameTitle);
    drawList->AddText(
        titleFont,
        titleFontSize,
        titlePosition,
        gold,
        gameTitle);

    const float buttonW = std::min(300.0f, w * 0.32f);
    const float buttonH = 54.0f;
    const float buttonGap = 18.0f;
    const float menuX = min.x + (w - buttonW) * 0.5f;
    const float menuY = min.y + h * 0.68f;

    if (!m_starting)
    {
        const float menuHeight = buttonH * 2.0f + 66.0f;
        ImGui::SetNextWindowPos(ImVec2(menuX, menuY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(buttonW, menuHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.06f, 0.08f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.88f, 0.75f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.09f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.16f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.24f, 0.13f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
        if (ImGui::Begin("Title Menu", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button("START", ImVec2(contentWidth, buttonH)))
                StartTitleTransition(m_starting, m_transitionSeconds);

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            const ImVec2 promptSize = ImGui::CalcTextSize("PRESS ENTER");
            ImGui::SetCursorPosX((contentWidth - promptSize.x) * 0.5f + ImGui::GetStyle().WindowPadding.x);
            ImGui::TextDisabled("PRESS ENTER");
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.26f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.34f, 0.36f, 0.40f, 1.0f));
            if (ImGui::Button("END", ImVec2(contentWidth, buttonH)))
                PostQuitMessage(0);
            ImGui::PopStyleColor(3);
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    if (m_starting)
    {
        const float fadeProgress = SmoothStep((m_transitionSeconds - 4.40f) / 0.70f);
        drawList->AddRectFilled(min, max,
            IM_COL32(0, 0, 0, static_cast<int>(fadeProgress * 235.0f)));
    }
}

void TitleScene::init()
{
    DebugUI::SetCursorVisible(true);
    SoundManager::PlayTitleBgm();

    auto shader = std::make_unique<CShader>();
    shader->Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");
    ShaderManager::Register<CShader>("Shader3D", std::move(shader));

    auto skinShader = std::make_unique<CShader>();
    skinShader->Create("shader/vertexLightingOneSkinVSSafe.hlsl", "shader/vertexLightingSkinPS.hlsl");
    ShaderManager::Register<CShader>("Shader3DSkin", std::move(skinShader));

    m_camera.Init();
    m_camera.SetPosition(Vector3(10.0f, 24.0f, -66.0f));
    m_camera.SetLookat(Vector3(3.0f, 16.0f, 10.0f));

    m_playerAnimationMesh = std::make_unique<CAnimationMesh>();
    m_playerAnimationMesh->Load("assets/model/SwordShieldPack/runtime/SwordShieldPack_Player.glb",
        "assets/model/SwordShieldPack/runtime/");
    m_playerAnimator.Initialize(*m_playerAnimationMesh);
    m_playerBoneComb.Create();

    // キャラクターに付属する歩きモーションを使用する。
    // モデルパックには別名の候補もあるため、読み込み失敗時は2つ目を試す。
    const aiScene* walkScene = m_playerAnimationData.LoadAnimation(
        "assets/motion/sword and shield walk.fbx", "walk");
    if (walkScene == nullptr || walkScene->mNumAnimations == 0)
    {
        walkScene = m_playerAnimationData.LoadAnimation(
            "assets/motion/sword and shield walk (2).fbx", "walk");
    }
    if (walkScene != nullptr && walkScene->mNumAnimations > 0)
        m_playerWalkAnimation = m_playerAnimationData.GetAnimation("walk", 0);

    m_playerSrt.scale = Vector3(18.0f, 18.0f, 18.0f);
    m_playerSrt.pos = Vector3(20.0f, 0.75f, 5.0f);
    m_playerSrt.rot.y = 0.46f;
    // 初期状態では剣をプレイヤーの脇へ下げる。
    // START後に剣を納める動作を見せ、退出時に抜刀した状態へ戻す。
    m_playerAnimator.UpdateTitleContractPose(
        *m_playerAnimationMesh,
        m_playerBoneComb,
        0.0f,
        0.0f,
        0.0f);

    m_guildFloor.Load(std::string(kGuildAssetDirectory) + "floor_wood_large_dark.gltf.glb", kGuildAssetDirectory);
    m_guildFloorRenderer.Init(m_guildFloor);
    m_guildBackWall.Load(std::string(kGuildAssetDirectory) + "wall_archedwindow_open.gltf.glb", kGuildAssetDirectory);
    m_guildBackWallRenderer.Init(m_guildBackWall);
    m_guildSideWall.Load(std::string(kGuildAssetDirectory) + "wall_corner.gltf.glb", kGuildAssetDirectory);
    m_guildSideWallRenderer.Init(m_guildSideWall);
    m_guildDoorway.Load(std::string(kGuildAssetDirectory) + "wall_doorway.glb", kGuildAssetDirectory);
    m_guildDoorwayRenderer.Init(m_guildDoorway);
    m_guildTable.Load(std::string(kGuildAssetDirectory) + "table_long_tablecloth_decorated_A.gltf.glb", kGuildAssetDirectory);
    m_guildTableRenderer.Init(m_guildTable);
    m_guildChair.Load(std::string(kGuildAssetDirectory) + "chair.gltf.glb", kGuildAssetDirectory);
    m_guildChairRenderer.Init(m_guildChair);
    m_guildBanner.Load(std::string(kGuildAssetDirectory) + "banner_red.gltf.glb", kGuildAssetDirectory);
    m_guildBannerRenderer.Init(m_guildBanner);
    m_guildTorch.Load(std::string(kGuildAssetDirectory) + "torch_mounted.gltf.glb", kGuildAssetDirectory);
    m_guildTorchRenderer.Init(m_guildTorch);
    m_guildBarrel.Load(std::string(kGuildAssetDirectory) + "barrel_large.gltf.glb", kGuildAssetDirectory);
    m_guildBarrelRenderer.Init(m_guildBarrel);
    m_guildShelf.Load(std::string(kGuildAssetDirectory) + "shelf_large.gltf.glb", kGuildAssetDirectory);
    m_guildShelfRenderer.Init(m_guildShelf);
    m_guildChest.Load(std::string(kGuildAssetDirectory) + "chest.glb", kGuildAssetDirectory);
    m_guildChestRenderer.Init(m_guildChest);

    DebugUI::SetVisible(false);
}

void TitleScene::dispose()
{
    m_playerAnimationMesh.reset();
    m_playerWalkAnimation = nullptr;
    m_camera.Dispose();
    DebugUI::SetVisible(true);
}
