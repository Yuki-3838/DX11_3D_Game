#include "MotionEditorScene.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <DirectXMath.h>

#include "../system/CShader.h"
#include "../system/DebugUI.h"
#include "../system/Inputmanager.h"
#include "../system/MeshManager.h"
#include "../system/imgui/imgui.h"
#include "../system/renderer.h"
#include "../system/SphereDrawer.h"
#include "../system/transform.h"
#include "../application.h"

void MotionEditorScene::init()
{
	m_camera.Init();
	m_camera.SetUP(Vector3(0.0f, 1.0f, 0.0f));

	std::unique_ptr<CShader> skinShader = std::make_unique<CShader>();
	skinShader->Create(
		"shader/vertexLightingOneSkinVSSafe.hlsl",
		"shader/vertexLightingPS.hlsl");
	ShaderManager::Register<CShader>("Shader3DSkin", std::move(skinShader));

	m_animationMesh = std::make_unique<CAnimationMesh>();
	m_animationMesh->Load(
		"assets/model/SwordShieldPack/runtime/SwordShieldPack_Player.glb",
		"assets/model/SwordShieldPack/runtime/");
	m_boneComb.Create();
	m_animator.Initialize(*m_animationMesh);
	m_animator.EnableMotionEditor();
	// CCharacterAnimator restores the last downloaded clip selected in the
	// editor, so GameScene and MotionEditorScene use the same attack.

	ApplyCamera();
	DebugUI::RedistDebugFunction([this]() { RenderEditorCamera(); });
	DebugUI::RedistDebugFunction([this]() { RenderBoneOverlay(); });
}

void MotionEditorScene::dispose()
{
	m_animationMesh.reset();
}

void MotionEditorScene::update(uint64_t delta)
{
	(void)delta;
	ApplyCamera();
	m_animator.Update(
		*m_animationMesh,
		m_boneComb,
		CharacterAnimationState{});
	m_animationMesh->UpdateSwordWorldTransform(Matrix4x4::Identity);
}

void MotionEditorScene::draw(uint64_t delta)
{
	(void)delta;
	m_camera.Draw();

	Matrix4x4 world = Matrix4x4::Identity;
	Renderer::SetWorldMatrix(&world);
	ShaderManager::Get<CShader>("Shader3DSkin")->SetGPU();
	m_boneComb.Update();
	m_boneComb.SetGPU();
	m_animationMesh->Draw();

	if (m_showBoneMarkers)
	{
		for (const auto& [name, matrix] : m_animationMesh->GetDebugBoneMatrices())
		{
			if (std::find(m_animator.GetBoneNames().begin(), m_animator.GetBoneNames().end(), name) == m_animator.GetBoneNames().end())
				continue;
			const Vector3 position(matrix._41, matrix._42, matrix._43);
			const bool selected = name == m_animator.GetSelectedBone();
			SphereDrawerDraw(
				selected ? 0.32f : 0.18f,
				selected ? Color(1.0f, 0.85f, 0.1f, 0.9f) : Color(0.1f, 0.8f, 1.0f, 0.65f),
				position.x, position.y, position.z);
		}
	}
}

void MotionEditorScene::ApplyCamera()
{
	const Vector3 target(0.0f, m_cameraTargetHeight, 0.0f);
	const float cosPitch = std::cos(m_cameraPitch);
	const Vector3 offset(
		std::sin(m_cameraYaw) * cosPitch * m_cameraDistance,
		std::sin(m_cameraPitch) * m_cameraDistance,
		-std::cos(m_cameraYaw) * cosPitch * m_cameraDistance);
	m_camera.SetPosition(target + offset);
	m_camera.SetLookat(target);
}

void MotionEditorScene::RenderEditorCamera()
{
	ImGui::SetNextWindowPos(ImVec2(460.0f, 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360.0f, 470.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Preview Camera");
	ImGui::TextWrapped("カメラ操作：右ドラッグで回転、マウスホイールでズーム");
	ImGui::TextWrapped("モデル上の関節をクリックしてボーン選択。選択後にドラッグで回転。");
	ImGui::Text("選択中: %s", m_animator.GetSelectedBone().c_str());
	ImGui::Checkbox("ボーンの泡を表示", &m_showBoneMarkers);
	ImGui::Text("ドラッグモード");
	ImGui::RadioButton("Rotate", &m_gizmoMode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Move", &m_gizmoMode, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Scale", &m_gizmoMode, 2);
	ImGui::Text("Rotate: Y / Shift:X / Ctrl:Z");
	ImGui::Text("F1: Game Scene    F3: Motion Editor");
	const bool cameraWindowHovered = ImGui::IsWindowHovered();
	if (cameraWindowHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
	{
		m_cameraYaw += ImGui::GetIO().MouseDelta.x * 0.01f;
		m_cameraPitch = std::clamp(
			m_cameraPitch - ImGui::GetIO().MouseDelta.y * 0.01f,
			-1.4f, 1.4f);
	}
	if (cameraWindowHovered && std::abs(ImGui::GetIO().MouseWheel) > 0.001f)
		m_cameraDistance = std::clamp(m_cameraDistance - ImGui::GetIO().MouseWheel * 0.8f, 2.5f, 30.0f);

	float yawDegrees = m_cameraYaw * 180.0f / 3.14159265f;
	float pitchDegrees = m_cameraPitch * 180.0f / 3.14159265f;
	if (ImGui::SliderFloat("Yaw", &yawDegrees, -180.0f, 180.0f, "%.1f deg"))
		m_cameraYaw = yawDegrees * 3.14159265f / 180.0f;
	if (ImGui::SliderFloat("Pitch", &pitchDegrees, -5.0f, 75.0f, "%.1f deg"))
		m_cameraPitch = std::clamp(pitchDegrees * 3.14159265f / 180.0f, -1.4f, 1.4f);
	ImGui::SliderFloat("Distance", &m_cameraDistance, 2.5f, 30.0f, "%.1f");
	ImGui::SliderFloat("Target Height", &m_cameraTargetHeight, 0.0f, 3.0f, "%.2f");

	if (ImGui::Button("Reset Preview Camera"))
	{
		m_cameraYaw = 0.0f;
		m_cameraPitch = 0.08f;
		m_cameraDistance = 6.0f;
		m_cameraTargetHeight = 1.45f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Front"))
		m_cameraYaw = 0.0f;
	ImGui::SameLine();
	if (ImGui::Button("Back"))
		m_cameraYaw = 3.14159265f;
	ImGui::SameLine();
	if (ImGui::Button("Side"))
		m_cameraYaw = 1.57079633f;
	ImGui::Text("Camera position: %.1f, %.1f, %.1f",
		m_camera.GetPosition().x,
		m_camera.GetPosition().y,
		m_camera.GetPosition().z);
	ImGui::End();
}

void MotionEditorScene::RenderBoneOverlay()
{
	if (!m_showBoneMarkers)
	{
		m_boneScreenPositions.clear();
		m_draggingBone = false;
		return;
	}

	const auto& io = ImGui::GetIO();
	const Matrix4x4 view = m_camera.GetViewMatrix();
	const Matrix4x4 projection = m_camera.GetProjMatrix();
	const Matrix4x4 world = Matrix4x4::Identity;
	const float width = static_cast<float>(Application::GetWidth());
	const float height = static_cast<float>(Application::GetHeight());
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	m_boneScreenPositions.clear();

	for (const auto& [name, matrix] : m_animationMesh->GetDebugBoneMatrices())
	{
		if (std::find(m_animator.GetBoneNames().begin(), m_animator.GetBoneNames().end(), name) == m_animator.GetBoneNames().end())
			continue;
		const Vector3 worldPosition(matrix._41, matrix._42, matrix._43);
		const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
			DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f),
			0.0f, 0.0f, width, height, 0.0f, 1.0f,
			projection, view, world);
		const Vector3 screen(
			DirectX::XMVectorGetX(projected),
			DirectX::XMVectorGetY(projected),
			DirectX::XMVectorGetZ(projected));
		if (screen.z < 0.0f || screen.z > 1.0f)
			continue;

		m_boneScreenPositions[name] = screen;
		const bool selected = name == m_animator.GetSelectedBone();
		const ImU32 color = selected ? IM_COL32(255, 220, 30, 255) : IM_COL32(40, 190, 240, 220);
		drawList->AddCircleFilled(ImVec2(screen.x, screen.y), selected ? 9.0f : 5.0f, color);
		if (selected)
			drawList->AddText(ImVec2(screen.x + 12.0f, screen.y - 10.0f), color, name.c_str());
		const float mouseDx = screen.x - io.MousePos.x;
		const float mouseDy = screen.y - io.MousePos.y;
		if (!io.WantCaptureMouse && mouseDx * mouseDx + mouseDy * mouseDy < 100.0f)
		{
			ImGui::BeginTooltip();
			ImGui::Text("%s", name.c_str());
			ImGui::TextUnformatted("クリックで選択");
			ImGui::EndTooltip();
		}
	}

	if (io.WantCaptureMouse)
	{
		m_draggingBone = false;
		return;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const ImVec2 mouse = io.MousePos;
		float nearestDistance = 20.0f;
		std::string nearestBone;
		for (const auto& [name, screen] : m_boneScreenPositions)
		{
			const float dx = screen.x - mouse.x;
			const float dy = screen.y - mouse.y;
			const float distance = std::sqrt(dx * dx + dy * dy);
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearestBone = name;
			}
		}
		if (!nearestBone.empty())
		{
			m_animator.SelectBone(nearestBone);
			m_draggingBone = true;
		}
	}

	if (m_draggingBone && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const float horizontal = io.MouseDelta.x * 0.01f;
		const float vertical = -io.MouseDelta.y * 0.01f;
		if (m_gizmoMode == 1)
			m_animator.AdjustSelectedPosition(Vector3(horizontal * 10.0f, vertical * 10.0f, 0.0f));
		else if (m_gizmoMode == 2)
			m_animator.AdjustSelectedScale(Vector3(horizontal, horizontal, horizontal));
		else if (io.KeyShift)
			m_animator.AdjustSelectedRotation(Vector3(vertical, 0.0f, 0.0f));
		else if (io.KeyCtrl)
			m_animator.AdjustSelectedRotation(Vector3(0.0f, 0.0f, horizontal));
		else
			m_animator.AdjustSelectedRotation(Vector3(0.0f, horizontal, 0.0f));
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		m_draggingBone = false;
}
