#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"
#include "../system/camera.h"
#include "../system/CAnimationMesh.h"
#include "../system/CCharacterAnimator.h"
#include "../system/BoneCombMatrix.h"

class MotionEditorScene final : public IScene
{
public:
	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

private:
	void RenderEditorCamera();
	void RenderBoneOverlay();
	void ApplyCamera();

	Camera m_camera;
	std::unique_ptr<CAnimationMesh> m_animationMesh;
	CCharacterAnimator m_animator;
	BoneCombMatrix m_boneComb;

	float m_cameraYaw = 0.0f;
	float m_cameraPitch = 0.08f;
	float m_cameraDistance = 42.0f;
	float m_cameraTargetHeight = 10.0f;
	std::unordered_map<std::string, Vector3> m_boneScreenPositions;
	bool m_draggingBone = false;
	int m_gizmoMode = 0; // 0 rotate, 1 move, 2 scale
	bool m_showBoneMarkers = false;
};

REGISTER_CLASS(MotionEditorScene)
