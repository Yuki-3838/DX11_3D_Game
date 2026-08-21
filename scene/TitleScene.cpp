#include "TitleScene.h"

#include "../system/GameFlow.h"
#include "../system/Inputmanager.h"
#include "../system/CShader.h"
#include "../system/DebugUI.h"
#include "../system/meshmanager.h"
#include "../system/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <dinput.h>

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
        // Keep the approach lane in front of the table.  The old target was
        // inside the right chair's footprint, which made the player visibly
        // pass through the chair and table edge during the handoff.
        const Vector3 contractPosition(10.0f, 0.75f, 4.5f);
        // Finish drawing the sword before taking the first step.  The old
        // route covered almost 56 world units in 1.9 seconds, so the player
        // appeared to teleport between footsteps.  This shorter, slower lane
        // keeps the walk readable while still taking the player out of frame.
        // Continue beyond the camera's right edge so the character finishes
        // the walk instead of being cut off at the edge of the frame.
        const Vector3 exitPosition(50.0f, 0.75f, -42.0f);
        // The walk clip advances along the character's local -Z axis.  Match
        // the actor's yaw to the actual exit vector; the previous fixed yaw
        // made the feet walk diagonally across the route, which looked like
        // sideways sliding even though the position interpolation was straight.
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
            // Keep the model's forward axis aligned with the direction of
            // travel so each planted foot moves in the same direction.
            m_playerSrt.rot.y = walkHeading;
        }

        if (m_playerAnimationMesh)
        {
            const bool useWalkAnimation =
                m_transitionSeconds >= kWalkStartSeconds &&
                m_playerWalkAnimation != nullptr;
            if (useWalkAnimation)
            {
                // The FBX clip is authored as a regular keyframe animation.
                // Advance it at 30 fps while the title route moves at the
                // slower cinematic walking speed.
                if ((m_playerWalkTick++ % 2) == 0)
                    ++m_playerWalkFrame;
            }
            const float reachAmount = SmoothStep((m_transitionSeconds - 0.42f) / 0.78f);
            const float sheatheAmount = SmoothStep(m_transitionSeconds / 0.45f);
            const float drawAmount = SmoothStep((m_transitionSeconds - 1.52f) / 0.72f);
            // Keep the title-specific handoff pose on the upper body while
            // the imported clip drives only the legs/feet during departure.
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

        // Change scenes only after the actor has reached the off-screen exit.
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
    // Place the contract on the near/right side of the table so the approach
    // is a short, believable reach instead of dragging the paper through the
    // whole tabletop.
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
            // The debug bone matrix is in player-local space.  Use the actual
            // animated free hand, including the title pose and player SRT, so
            // the document cannot float beside or through the arm.
            const Matrix4x4 handWorld = handIt->second * m_playerSrt.GetMatrix();
            handPosition = Vector3::Transform(
                Vector3(0.0f, 0.10f, 0.04f), handWorld);
            // Keep the hand position itself accurate; the clearance for the
            // paper is applied below so the page can be held outside the arm
            // without moving the hand target.
        }
    }
    const float takeProgress = m_starting
        ? SmoothStep((m_transitionSeconds - 0.85f) / 0.65f)
        : 0.0f;

    SRT document{};
    // The contract lies flat on the table, then turns upright into the free
    // hand.  Keeping the page nearly camera-facing makes the seal and writing
    // read as a held document instead of an edge-on plank.
    document.rot = Vector3(-1.57f * takeProgress,
        0.02f * takeProgress, 0.10f * takeProgress);
    const Matrix4x4 documentRotation = Matrix4x4::CreateFromYawPitchRoll(
        document.rot.y, document.rot.x, document.rot.z);

    // Hold the document by the lower corner nearest the free hand.  The
    // character's free hand is on screen-right, so the page extends inward
    // across the front of the torso, like a hunter presenting a contract,
    // instead of hanging beside the arm.  This local grip point is transformed
    // with the page, guaranteeing that the hand remains on its corner while
    // the character walks away.
    const Vector3 localGripPoint(2.15f, 0.0f, -2.45f);
    Vector3 documentHandPosition = handPosition -
        Vector3::TransformNormal(localGripPoint, documentRotation);
    // Keep a small camera-facing clearance while carrying.  Without this the
    // hand anchor is inside the torso once the walk pose turns the character,
    // so depth testing hides the page even though it still follows the hand.
    // Keep the offset modest; the previous 5-unit correction made the page
    // cover the whole character.
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

    // The band and seal are children of the paper transform.  Drawing them
    // at world-space offsets was the reason they slid through the document
    // as it tilted into the hand.
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

    // A simple dark sheath makes the sword handoff readable even though the
    // player asset keeps the sword embedded in its skinned mesh.  The title
    // pose lowers the sword arm to this prop before drawing it again.
    SRT sheath{};
    sheath.pos = Vector3(m_playerSrt.pos.x - 2.7f, m_playerSrt.pos.y + 6.7f,
        m_playerSrt.pos.z + 0.7f);
    sheath.rot = Vector3(0.0f, 0.0f, -0.38f);
    sheath.scale = Vector3(0.78f, 0.62f, 0.78f);
    m_sheath.Draw(sheath, Color(0.12f, 0.035f, 0.018f, 1.0f));

    // Draw the paper before the character.  The modest camera clearance keeps
    // the page in front of the torso while depth testing lets the forearm and
    // hand appear over its lower corner.
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
    const ImU32 cream = IM_COL32(247, 239, 224, 255);
    const ImU32 dark = IM_COL32(13, 15, 21, 220);

    drawList->AddRectFilled(min, max, IM_COL32(7, 9, 15, 38));

    const float logoSize = std::max(42.0f, w * 0.088f);
    const char* logo = "DRAGON HUNT";
    const ImVec2 logoPos(min.x + w * 0.27f, min.y + h * 0.055f);
    for (const ImVec2& offset : { ImVec2(-3, -3), ImVec2(3, -3), ImVec2(-3, 3), ImVec2(3, 3) })
        drawList->AddText(nullptr, logoSize, ImVec2(logoPos.x + offset.x, logoPos.y + offset.y), IM_COL32(35, 20, 23, 255), logo);
    drawList->AddText(nullptr, logoSize, logoPos, cream, logo);

    const float panelX = min.x + w * 0.035f;
    const float panelY = min.y + h * 0.26f;
    const float panelW = std::min(455.0f, w * 0.39f);
    const float panelH = h * 0.54f;
    drawList->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH), dark, 8.0f);
    drawList->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH), gold, 8.0f, 0, 2.0f);
    drawList->AddText(nullptr, std::max(17.0f, h * 0.026f), ImVec2(panelX + 24.0f, panelY + 22.0f), gold, "GUILD HALL // QUEST BOARD");
    drawList->AddText(nullptr, std::max(30.0f, h * 0.050f), ImVec2(panelX + 24.0f, panelY + 67.0f), cream, "ANCIENT DRAGON");
    drawList->AddText(nullptr, std::max(17.0f, h * 0.025f), ImVec2(panelX + 24.0f, panelY + 118.0f), IM_COL32(204, 184, 160, 255), "HUNTING CONTRACT");
    drawList->AddLine(ImVec2(panelX + 24.0f, panelY + 150.0f), ImVec2(panelX + panelW - 24.0f, panelY + 150.0f), IM_COL32(224, 190, 128, 120), 1.0f);
    drawList->AddText(nullptr, std::max(17.0f, h * 0.024f), ImVec2(panelX + 24.0f, panelY + 174.0f), cream, "A dragon has appeared beyond the guild walls.");
    drawList->AddText(nullptr, std::max(17.0f, h * 0.024f), ImVec2(panelX + 24.0f, panelY + 202.0f), cream, "Take the contract. Strike when it is open.");
    drawList->AddText(nullptr, std::max(17.0f, h * 0.024f), ImVec2(panelX + 24.0f, panelY + 252.0f), gold, "TARGET");
    drawList->AddText(nullptr, std::max(18.0f, h * 0.026f), ImVec2(panelX + 112.0f, panelY + 250.0f), cream, "Ancient Dragon");
    drawList->AddText(nullptr, std::max(17.0f, h * 0.024f), ImVec2(panelX + 24.0f, panelY + 282.0f), gold, "OBJECTIVE");
    drawList->AddText(nullptr, std::max(18.0f, h * 0.026f), ImVec2(panelX + 112.0f, panelY + 280.0f), cream, "Exploit its openings");

    const float buttonX = panelX + 24.0f;
    const float buttonY = panelY + panelH - 86.0f;
    const float buttonW = panelW - 48.0f;
    const float buttonH = 56.0f;
    drawList->AddRectFilled(ImVec2(buttonX, buttonY), ImVec2(buttonX + buttonW, buttonY + buttonH), IM_COL32(108, 24, 17, 245), 4.0f);
    drawList->AddRect(ImVec2(buttonX, buttonY), ImVec2(buttonX + buttonW, buttonY + buttonH), gold, 4.0f, 0, 2.0f);
    drawList->AddText(nullptr, std::max(22.0f, h * 0.032f), ImVec2(buttonX + buttonW * 0.29f, buttonY + 13.0f), cream, "TAKE QUEST");
    drawList->AddText(nullptr, std::max(16.0f, h * 0.022f), ImVec2(panelX + 24.0f, panelY + panelH + 18.0f), IM_COL32(204, 184, 160, 255), "ENTER / SPACE   ACCEPT CONTRACT");

    ImGui::SetNextWindowPos(ImVec2(buttonX, buttonY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(buttonW, buttonH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Guild Quest Start Hitbox", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::InvisibleButton("Take Quest", ImVec2(buttonW, buttonH)))
        {
            StartTitleTransition(m_starting, m_transitionSeconds);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (m_starting)
    {
        const float fadeProgress = SmoothStep((m_transitionSeconds - 4.40f) / 0.70f);
        drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, static_cast<int>(fadeProgress * 235.0f)));
        const char* status = m_transitionSeconds < 0.45f
            ? "SHEATHING SWORD"
            : (m_transitionSeconds < 1.50f
                ? "TAKING CONTRACT"
                : (m_transitionSeconds < 1.92f
                    ? "DRAWING SWORD"
                    : (m_transitionSeconds < kWalkStartSeconds
                        ? "READY TO DEPART"
                        : "LEAVING GUILD")));
        drawList->AddText(nullptr, std::max(18.0f, h * 0.026f),
            ImVec2(min.x + w * 0.5f - 120.0f, min.y + h * 0.84f),
            IM_COL32(255, 240, 220, 240), status);
    }
}

void TitleScene::init()
{
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

    // Use the walk clip supplied for this character instead of synthesizing
    // a lower-body stride.  Keep a second variant as a fallback because both
    // files were supplied with this model pack.
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
    // Start with the sword lowered at the player's side.  Pressing Start then
    // visibly sheaths it before the contract handoff and redraws it on exit.
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
