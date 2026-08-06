#include "CCharacterAnimator.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <cstdio>
#include <string_view>
#include <vector>

#include "DebugUI.h"
#include "Inputmanager.h"
#include "imgui/imgui.h"

namespace
{
	std::string NormalizeBoneName(std::string value)
	{
		std::string normalized;
		normalized.reserve(value.size());
		for (const char c : value)
		{
			if (c == '_' || c == '-' || c == ' ')
				continue;
			normalized.push_back(c >= 'A' && c <= 'Z'
				? static_cast<char>(c - 'A' + 'a') : c);
		}
		return normalized;
	}

	std::string FindBone(
		const std::vector<std::string>& boneNames,
		std::initializer_list<std::string_view> aliases)
	{
		for (const auto alias : aliases)
		{
			const std::string normalizedAlias = NormalizeBoneName(std::string(alias));
			for (const auto& name : boneNames)
				if (NormalizeBoneName(name) == normalizedAlias)
					return name;
		}

		for (const auto& name : boneNames)
		{
			const std::string normalizedName = NormalizeBoneName(name);
			for (const auto alias : aliases)
				if (normalizedName.find(NormalizeBoneName(std::string(alias))) != std::string::npos)
					return name;
		}
		return {};
	}

	void SetRotation(
		std::unordered_map<std::string, Matrix4x4>& rotations,
		const std::string& boneName,
		const Matrix4x4& rotation)
	{
		if (!boneName.empty())
			rotations[boneName] = rotation;
	}
}

void CCharacterAnimator::Initialize(const CAnimationMesh& mesh)
{
	m_boneNames = mesh.GetBoneNames();
	const std::vector<std::string>& boneNames = m_boneNames;
	if (!boneNames.empty())
		m_selectedBone = boneNames.front();
	// D suffixes are Furina/MMD deform bones. They are listed first so that
	// control/IK bones with zero vertex weights are not selected accidentally.
	m_leftArm = FindBone(boneNames, { "左腕D", "mixamorig:LeftArm", "左腕", "leftarm" });
	m_rightArm = FindBone(boneNames, { "右腕D", "mixamorig:RightArm", "右腕", "rightarm" });
	m_leftElbow = FindBone(boneNames, { "左ひじD", "mixamorig:LeftForeArm", "左ひじ", "leftelbow" });
	m_rightElbow = FindBone(boneNames, { "右ひじD", "mixamorig:RightForeArm", "右ひじ", "rightelbow" });
	m_leftHand = FindBone(boneNames, { "左手首D", "mixamorig:LeftHand", "左手首", "lefthand" });
	m_rightHand = FindBone(boneNames, { "右手首D", "mixamorig:RightHand", "右手首", "righthand" });
	m_leftLeg = FindBone(boneNames, { "左足D", "mixamorig:LeftUpLeg", "Bip001 L Thigh", "左足", "leftupleg", "leftthigh" });
	m_rightLeg = FindBone(boneNames, { "右足D", "mixamorig:RightUpLeg", "Bip001 R Thigh", "右足", "rightupleg", "rightthigh" });
	m_leftKnee = FindBone(boneNames, { "左ひざD", "mixamorig:LeftLeg", "Bip001 L Calf", "左ひざ", "leftknee", "leftcalf" });
	m_rightKnee = FindBone(boneNames, { "右ひざD", "mixamorig:RightLeg", "Bip001 R Calf", "右ひざ", "rightknee", "rightcalf" });
	m_leftFoot = FindBone(boneNames, { "左足首D", "mixamorig:LeftFoot", "Bip001 L Foot", "左足首", "leftankle", "leftfoot" });
	m_rightFoot = FindBone(boneNames, { "右足首D", "mixamorig:RightFoot", "Bip001 R Foot", "右足首", "rightankle", "rightfoot" });

	// GameSceneでも専用エディタで保存した攻撃モーションを使用する。
	LoadMotionFile(m_motionFilename);
}

void CCharacterAnimator::PlayAttackMotion()
{
	if (m_motionKeys.empty())
		LoadMotionFile(m_motionFilename);
	m_motionTime = 0.0f;
	m_motionLoop = false;
	m_motionPlaying = !m_motionKeys.empty();
	m_useCustomMotion = m_motionPlaying;
}

void CCharacterAnimator::EnableMotionEditor()
{
	if (m_editorInitialized)
		return;
	DebugUI::RedistDebugFunction([this]() { RenderMotionEditor(); });
	m_editorInitialized = true;
	m_editorEnabled = true;
}

bool CCharacterAnimator::LoadMotionFile(const std::string& filename)
{
	m_motionFilename = filename;
	const bool loaded = LoadMotion(filename);
	if (!loaded)
		BuildFallbackAttackMotion();
	return loaded;
}

void CCharacterAnimator::SelectBone(const std::string& boneName)
{
	if (std::find(m_boneNames.begin(), m_boneNames.end(), boneName) == m_boneNames.end())
		return;
	m_selectedBone = boneName;
	m_editorKey = {};
	m_editorKey.scale = Vector3(1.0f, 1.0f, 1.0f);
	const auto it = m_motionKeys.find(m_selectedBone);
	if (it != m_motionKeys.end())
	{
		for (const auto& key : it->second)
		{
			if (std::abs(key.time - m_motionTime) < 0.001f)
			{
				m_editorKey = key;
				break;
			}
		}
	}
}

void CCharacterAnimator::AdjustSelectedRotation(const Vector3& delta)
{
	m_editorKey.rotation += delta;
	AddOrUpdateCurrentKey();
}

void CCharacterAnimator::AdjustSelectedPosition(const Vector3& delta)
{
	m_editorKey.position += delta;
	AddOrUpdateCurrentKey();
}

void CCharacterAnimator::AdjustSelectedScale(const Vector3& delta)
{
	m_editorKey.scale += delta;
	m_editorKey.scale.x = std::max(m_editorKey.scale.x, 0.01f);
	m_editorKey.scale.y = std::max(m_editorKey.scale.y, 0.01f);
	m_editorKey.scale.z = std::max(m_editorKey.scale.z, 0.01f);
	AddOrUpdateCurrentKey();
}

void CCharacterAnimator::AddOrUpdateCurrentKey()
{
	MotionKeyframe key = m_editorKey;
	key.time = m_motionTime;
	BoneKeys& keys = m_motionKeys[m_selectedBone];
	bool replaced = false;
	for (auto& existing : keys)
	{
		if (std::abs(existing.time - key.time) < 0.001f)
		{
			existing = key;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		keys.push_back(key);
	SortKeys(keys);
	m_useCustomMotion = true;
}

void CCharacterAnimator::Update(
	CAnimationMesh& mesh,
	BoneCombMatrix& boneComb,
	const CharacterAnimationState& state)
{
	// モーション専用シーンではUIのPlayを使い、GameSceneでは左クリックで
	// 保存済みattack.motionを先頭から1回再生する。
	if (m_motionPlaying)
	{
		// The existing scene API does not pass frame delta to the animator.
		// Use the game's fixed 60 Hz update for editor playback.
		m_motionTime += 1.0f / 60.0f;
		if (m_motionTime > m_motionDuration)
		{
			if (m_motionLoop)
				m_motionTime = std::fmod(m_motionTime, std::max(m_motionDuration, 0.001f));
			else
			{
				m_motionTime = m_motionDuration;
				m_motionPlaying = false;
			}
		}
	}

	// The editor must display the pose at the scrubbed timeline even while
	// playback is paused. GameScene still applies it only during one-shot play.
	if (m_useCustomMotion && (m_motionPlaying || m_editorEnabled) && !m_motionKeys.empty())
	{
		std::unordered_map<std::string, Matrix4x4> customPose;
		EvaluateCustomMotion(m_motionTime, customPose);
		mesh.UpdateManualPose(boneComb, customPose);
		return;
	}

	const float phase = std::sinf(state.motionTime * 7.0f);
	const float armSwing = state.walking ? phase * 0.45f : 0.0f;
	const float legSwing = state.walking ? -phase * 0.65f : 0.0f;
	const float kneeSwing = state.walking ? std::max(0.0f, phase) * 0.45f : 0.0f;
	const float footSwing = state.walking ? phase * 0.5f : (state.jumping ? -0.2f : 0.0f);
	const float armRaise = state.jumping ? 0.75f : 0.0f;
	std::unordered_map<std::string, Matrix4x4> rotations;

	const float leftArmSwing = armSwing + armRaise;
	const float rightArmSwing = armSwing - armRaise;
	SetRotation(rotations, m_leftArm,
		Matrix4x4::CreateRotationY(leftArmSwing) * Matrix4x4::CreateRotationZ(leftArmSwing * 0.2f));
	SetRotation(rotations, m_rightArm,
		Matrix4x4::CreateRotationY(rightArmSwing) * Matrix4x4::CreateRotationZ(rightArmSwing * 0.2f));
	SetRotation(rotations, m_leftElbow, Matrix4x4::CreateRotationZ(state.walking ? -phase * 0.18f : 0.0f));
	SetRotation(rotations, m_rightElbow, Matrix4x4::CreateRotationZ(state.walking ? phase * 0.18f : 0.0f));
	SetRotation(rotations, m_leftHand, Matrix4x4::CreateRotationZ(-armSwing * 0.25f));
	SetRotation(rotations, m_rightHand, Matrix4x4::CreateRotationZ(armSwing * 0.25f));
	SetRotation(rotations, m_leftLeg, Matrix4x4::CreateRotationZ(legSwing * 0.25f) * Matrix4x4::CreateRotationX(legSwing));
	SetRotation(rotations, m_rightLeg, Matrix4x4::CreateRotationZ(-legSwing * 0.25f) * Matrix4x4::CreateRotationX(-legSwing));
	SetRotation(rotations, m_leftKnee, Matrix4x4::CreateRotationZ(kneeSwing * 0.2f) * Matrix4x4::CreateRotationX(kneeSwing));
	SetRotation(rotations, m_rightKnee, Matrix4x4::CreateRotationZ(std::max(0.0f, -phase) * 0.04f) * Matrix4x4::CreateRotationX(std::max(0.0f, -phase) * 0.22f));
	SetRotation(rotations, m_leftFoot, Matrix4x4::CreateRotationX(footSwing));
	SetRotation(rotations, m_rightFoot, Matrix4x4::CreateRotationX(-footSwing));

	mesh.UpdateManualPose(boneComb, rotations);
}

void CCharacterAnimator::SortKeys(BoneKeys& keys)
{
	std::sort(keys.begin(), keys.end(), [](const MotionKeyframe& a, const MotionKeyframe& b) {
		return a.time < b.time;
	});
}

void CCharacterAnimator::BuildFallbackAttackMotion()
{
	m_motionKeys.clear();
	m_motionDuration = 0.85f;
	const auto addKeys = [this](const std::string& boneName, const std::vector<Vector3>& rotations) {
		if (boneName.empty())
			return;
		BoneKeys& keys = m_motionKeys[boneName];
		const float times[] = { 0.0f, 0.18f, 0.38f, 0.62f, 0.85f };
		for (size_t i = 0; i < rotations.size() && i < 5; ++i)
		{
			MotionKeyframe key;
			key.time = times[i];
			key.rotation = rotations[i];
			keys.push_back(key);
		}
	};
	addKeys(m_rightArm, { {}, {-0.90f, 0.30f, -0.45f}, {0.95f, -0.55f, 0.75f}, {0.35f, -0.25f, 0.25f}, {} });
	addKeys(m_rightElbow, { {}, {-0.55f, 0.0f, 0.0f}, {0.95f, 0.0f, 0.0f}, {0.35f, 0.0f, 0.0f}, {} });
	addKeys(m_rightHand, { {}, {0.0f, 0.0f, -0.35f}, {0.0f, 0.0f, 0.65f}, {0.0f, 0.0f, 0.25f}, {} });
	addKeys(m_leftArm, { {}, {0.25f, 0.0f, 0.25f}, {-0.20f, 0.0f, -0.20f}, {-0.10f, 0.0f, -0.10f}, {} });
	m_motionTime = 0.0f;
	m_motionPlaying = false;
	m_useCustomMotion = true;
	m_motionFileLoaded = false;
	m_motionMappedBoneCount = static_cast<int>(m_motionKeys.size());
}

void CCharacterAnimator::EvaluateCustomMotion(
	float time,
	std::unordered_map<std::string, Matrix4x4>& rotations) const
{
	for (const auto& [boneName, keys] : m_motionKeys)
	{
		if (keys.empty())
			continue;

		MotionKeyframe pose = keys.front();
		if (time >= keys.back().time)
			pose = keys.back();
		else if (time > keys.front().time)
		{
			for (size_t i = 1; i < keys.size(); ++i)
			{
				if (time <= keys[i].time)
				{
					const MotionKeyframe& a = keys[i - 1];
					const MotionKeyframe& b = keys[i];
					const float span = std::max(b.time - a.time, 0.0001f);
					const float linearT = std::clamp((time - a.time) / span, 0.0f, 1.0f);
					float t = linearT * linearT * (3.0f - 2.0f * linearT);
					// 溜めからインパクトまでは加速、斬り抜け直後は減速させる。
					// 等速補間で起きていた「腕をゆっくり往復するだけ」の見え方を防ぐ。
					if (a.time >= 0.20f && b.time <= 0.40f)
						t = linearT * linearT;
					else if (a.time >= 0.37f && b.time <= 0.55f)
						t = 1.0f - (1.0f - linearT) * (1.0f - linearT);
					pose.rotation = a.rotation + (b.rotation - a.rotation) * t;
					pose.position = a.position + (b.position - a.position) * t;
					pose.scale = a.scale + (b.scale - a.scale) * t;
					break;
				}
			}
		}

		const Matrix4x4 rotation =
			Matrix4x4::CreateRotationX(pose.rotation.x) *
			Matrix4x4::CreateRotationY(pose.rotation.y) *
			Matrix4x4::CreateRotationZ(pose.rotation.z);
		rotations[boneName] = Matrix4x4::CreateScale(pose.scale) * rotation *
			Matrix4x4::CreateTranslation(pose.position.x, pose.position.y, pose.position.z);
	}
}

bool CCharacterAnimator::SaveMotion(const std::string& filename) const
{
	const std::filesystem::path path(filename);
	if (path.has_parent_path())
		std::filesystem::create_directories(path.parent_path());
	std::ofstream file(filename, std::ios::trunc);
	if (!file)
		return false;

	file << "DX11_MOTION 1\n";
	file << std::setprecision(9) << "duration " << m_motionDuration << "\n";
	for (const auto& [boneName, keys] : m_motionKeys)
	{
		file << "bone " << std::quoted(boneName) << "\n";
		for (const auto& key : keys)
		{
			file << "key " << key.time << ' '
				<< key.rotation.x << ' ' << key.rotation.y << ' ' << key.rotation.z << ' '
				<< key.position.x << ' ' << key.position.y << ' ' << key.position.z << ' '
				<< key.scale.x << ' ' << key.scale.y << ' ' << key.scale.z << "\n";
		}
		file << "endbone\n";
	}
	return true;
}

bool CCharacterAnimator::LoadMotion(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file)
		return false;

	std::string token;
	std::string currentBone;
	std::unordered_map<std::string, BoneKeys> loadedKeys;
	float loadedDuration = 1.0f;
	while (file >> token)
	{
		if (token == "DX11_MOTION")
		{
			int version = 0;
			file >> version;
		}
		else if (token == "duration")
			file >> loadedDuration;
		else if (token == "bone")
		{
			std::string requestedBone;
			file >> std::quoted(requestedBone);
			currentBone = FindBone(m_boneNames, { std::string_view(requestedBone) });
			// エディタ側の変形ボーン名「右腕D」と、PMX本体の「右腕」の
			// どちらで保存されていても同じボーンへ割り当てる。
			if (currentBone.empty() && !requestedBone.empty() && requestedBone.back() == 'D')
			{
				const std::string withoutDeformSuffix = requestedBone.substr(0, requestedBone.size() - 1);
				currentBone = FindBone(m_boneNames, { std::string_view(withoutDeformSuffix) });
			}
			if (currentBone.empty())
			{
				const std::string withDeformSuffix = requestedBone + "D";
				currentBone = FindBone(m_boneNames, { std::string_view(withDeformSuffix) });
			}
			if (currentBone.empty())
				currentBone = requestedBone;
			loadedKeys[currentBone];
		}
		else if (token == "key" && !currentBone.empty())
		{
			MotionKeyframe key;
			file >> key.time
				>> key.rotation.x >> key.rotation.y >> key.rotation.z
				>> key.position.x >> key.position.y >> key.position.z
				>> key.scale.x >> key.scale.y >> key.scale.z;
			loadedKeys[currentBone].push_back(key);
		}
		else if (token == "endbone")
			currentBone.clear();
	}

	for (auto& [name, keys] : loadedKeys)
		SortKeys(keys);
	m_motionKeys = std::move(loadedKeys);
	m_motionDuration = std::max(loadedDuration, 0.01f);
	m_motionTime = 0.0f;
	m_motionPlaying = false;
	m_useCustomMotion = true;
	m_motionFileLoaded = true;
	m_motionMappedBoneCount = 0;
	for (const auto& [name, keys] : m_motionKeys)
	{
		if (std::find(m_boneNames.begin(), m_boneNames.end(), name) != m_boneNames.end() && !keys.empty())
			++m_motionMappedBoneCount;
	}
	if (m_motionMappedBoneCount == 0)
		BuildFallbackAttackMotion();
	return true;
}

void CCharacterAnimator::RenderMotionEditor()
{
	ImGui::SetNextWindowPos(ImVec2(20.0f, 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 760.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Motion Editor");
	ImGui::Text("攻撃モーション編集：ボーンを選び、時間ごとにポーズを登録します");
	ImGui::Checkbox("Use custom motion", &m_useCustomMotion);
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_motionLoop);
	ImGui::Text("状態: %s   読込ボーン: %d   時間: %.3f / %.3f",
		m_motionPlaying ? "再生中" : "停止中",
		m_motionMappedBoneCount,
		m_motionTime,
		m_motionDuration);
	ImGui::SliderFloat("Duration", &m_motionDuration, 0.05f, 10.0f, "%.2f sec");
	ImGui::SliderFloat("Timeline", &m_motionTime, 0.0f, m_motionDuration, "%.3f sec");

	if (ImGui::Button(m_motionPlaying ? "Pause" : "Play"))
		m_motionPlaying = !m_motionPlaying;
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		m_motionPlaying = false;
		m_motionTime = 0.0f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add / Update Key"))
		AddOrUpdateCurrentKey();
	ImGui::SameLine();
	ImGui::TextDisabled("手順：①ボーン選択 → ②時間 → ③数値入力 → ④キー追加");

	if (ImGui::BeginCombo("Bone", m_selectedBone.c_str()))
	{
		for (const auto& boneName : m_boneNames)
		{
			const bool selected = boneName == m_selectedBone;
			if (ImGui::Selectable(boneName.c_str(), selected))
				SelectBone(boneName);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	Vector3 degrees(
		m_editorKey.rotation.x * 180.0f / PI,
		m_editorKey.rotation.y * 180.0f / PI,
		m_editorKey.rotation.z * 180.0f / PI);
	if (ImGui::InputFloat3("Rotation (degrees)", &degrees.x))
	{
		m_editorKey.rotation = Vector3(
			degrees.x * PI / 180.0f,
			degrees.y * PI / 180.0f,
			degrees.z * PI / 180.0f);
	}
	ImGui::InputFloat3("Position", &m_editorKey.position.x);
	ImGui::InputFloat3("Scale", &m_editorKey.scale.x);

	const auto keyIt = m_motionKeys.find(m_selectedBone);
	const size_t keyCount = keyIt == m_motionKeys.end() ? 0 : keyIt->second.size();
	ImGui::Text("Selected bone keys: %zu", keyCount);
	if (keyIt != m_motionKeys.end())
	{
		for (size_t i = 0; i < keyIt->second.size(); ++i)
		{
			const MotionKeyframe& key = keyIt->second[i];
			const std::string label = "Key " + std::to_string(i) + "  @ " +
				std::to_string(key.time).substr(0, 5) + " sec";
			if (ImGui::Selectable(label.c_str(), std::abs(m_motionTime - key.time) < 0.001f))
			{
				m_motionTime = key.time;
				m_editorKey = key;
			}
		}
	}
	if (ImGui::Button("Delete Key At Timeline") && keyIt != m_motionKeys.end())
	{
		BoneKeys& keys = m_motionKeys[m_selectedBone];
		keys.erase(std::remove_if(keys.begin(), keys.end(), [this](const MotionKeyframe& key) {
			return std::abs(key.time - m_motionTime) < 0.001f;
		}), keys.end());
	}
	if (ImGui::Button("Save Motion"))
		SaveMotion(m_motionFilename);
	ImGui::SameLine();
	if (ImGui::Button("Load Motion"))
		LoadMotion(m_motionFilename);
	char filenameBuffer[260]{};
	std::snprintf(filenameBuffer, sizeof(filenameBuffer), "%s", m_motionFilename.c_str());
	if (ImGui::InputText("File", filenameBuffer, sizeof(filenameBuffer)))
		m_motionFilename = filenameBuffer;
	ImGui::Text("File format: .motion (plain text, editable by hand)");
	ImGui::Separator();
	ImGui::Text("使い方");
	ImGui::BulletText("ボーンを選択");
	ImGui::BulletText("Timelineで時間を決める");
	ImGui::BulletText("回転・位置・拡縮を入力");
	ImGui::BulletText("ポーズごとにAdd / Update Keyを押す");
	ImGui::BulletText("Playで確認してSave Motionで保存");
	ImGui::End();
}
