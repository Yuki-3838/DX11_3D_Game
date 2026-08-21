#pragma once

#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"
#include "../system/CAnimationMesh.h"
#include "../system/CCharacterAnimator.h"
#include "../system/BoneCombMatrix.h"
#include "../system/camera.h"
#include "../system/C3DShape.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"

#include <memory>

class TitleScene final : public IScene
{
public:
    void update(uint64_t deltatime) override;
    void draw(uint64_t deltatime) override;
    void init() override;
    void dispose() override;

private:
    float m_timeSeconds = 0.0f;
    bool m_starting = false;
    float m_transitionSeconds = 0.0f;

    std::unique_ptr<CAnimationMesh> m_playerAnimationMesh;
    CCharacterAnimator m_playerAnimator;
    BoneCombMatrix m_playerBoneComb;
    CAnimationData m_playerAnimationData;
    aiAnimation* m_playerWalkAnimation = nullptr;
    int m_playerWalkFrame = 0;
    int m_playerWalkTick = 0;
    Camera m_camera;
    SRT m_playerSrt{};

    // CC0 KayKit room and guild props.
    CStaticMesh m_guildFloor;
    CStaticMeshRenderer m_guildFloorRenderer;
    CStaticMesh m_guildBackWall;
    CStaticMeshRenderer m_guildBackWallRenderer;
    CStaticMesh m_guildSideWall;
    CStaticMeshRenderer m_guildSideWallRenderer;
    CStaticMesh m_guildDoorway;
    CStaticMeshRenderer m_guildDoorwayRenderer;
    CStaticMesh m_guildTable;
    CStaticMeshRenderer m_guildTableRenderer;
    CStaticMesh m_guildChair;
    CStaticMeshRenderer m_guildChairRenderer;
    CStaticMesh m_guildBanner;
    CStaticMeshRenderer m_guildBannerRenderer;
    CStaticMesh m_guildTorch;
    CStaticMeshRenderer m_guildTorchRenderer;
    CStaticMesh m_guildBarrel;
    CStaticMeshRenderer m_guildBarrelRenderer;
    CStaticMesh m_guildShelf;
    CStaticMeshRenderer m_guildShelfRenderer;
    CStaticMesh m_guildChest;
    CStaticMeshRenderer m_guildChestRenderer;

    // A small 3D contract card and wax seal. This is intentionally geometry,
    // not a flat UI image, so it participates in the title scene lighting.
    Box m_guildFoundation{ 120.0f, 0.4f, 120.0f };
    Box m_guildBackdrop{ 400.0f, 220.0f, 0.5f };
    Box m_guildCeiling{ 400.0f, 0.5f, 400.0f };
    Cylinder m_sheath{ 0.42f, 7.0f };
    Box m_questDocument{ 5.0f, 0.40f, 6.0f };
    Box m_questDocumentBand{ 0.45f, 0.46f, 4.6f };
    Sphere m_questSeal{ 0.30f };

    static float SmoothStep(float value);
    static Vector3 Lerp(const Vector3& from, const Vector3& to, float amount);
    static void DrawStaticMesh(CStaticMeshRenderer& renderer, const SRT& transform);
    void DrawQuestDocument();
};

REGISTER_CLASS(TitleScene)
