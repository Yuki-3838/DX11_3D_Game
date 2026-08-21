#include "CCharacterAnimator.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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

	Matrix4x4 MotionKeyToMatrix(const MotionKeyframe& key)
	{
		return Matrix4x4::CreateScale(key.scale) *
			Matrix4x4::CreateRotationX(key.rotation.x) *
			Matrix4x4::CreateRotationY(key.rotation.y) *
			Matrix4x4::CreateRotationZ(key.rotation.z) *
			Matrix4x4::CreateTranslation(key.position.x, key.position.y, key.position.z);
	}
}

void CCharacterAnimator::Initialize(const CAnimationMesh& mesh)
{
	m_boneNames = mesh.GetBoneNames();
	const std::vector<std::string>& boneNames = m_boneNames;
	if (!boneNames.empty())
		m_selectedBone = boneNames.front();
	// Keep the torso chain in the attack pose so the swing is driven by the
	// hips and spine, not only by the right arm. These names also make the
	// .motion file readable by hand.
	m_pelvis = FindBone(boneNames, { "pelvis", "hips", "mixamorig:Hips" });
	m_spine = FindBone(boneNames, { "spine", "mixamorig:Spine" });
	m_spine01 = FindBone(boneNames, { "spine_01", "spine01", "mixamorig:Spine1" });
	m_spine02 = FindBone(boneNames, { "spine_02", "spine02", "mixamorig:Spine2" });
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
	// Quaternius glTF uses explicit Blender-style .L/.R bone names.
	// Add those aliases without disturbing the existing MMD/Mixamo mappings.
	if (m_leftArm.empty()) m_leftArm = FindBone(boneNames, { "upperarm.l" });
	if (m_rightArm.empty()) m_rightArm = FindBone(boneNames, { "upperarm.r" });
	if (m_leftElbow.empty()) m_leftElbow = FindBone(boneNames, { "lowerarm.l" });
	if (m_rightElbow.empty()) m_rightElbow = FindBone(boneNames, { "lowerarm.r" });
	if (m_leftHand.empty()) m_leftHand = FindBone(boneNames, { "fist.l", "hand.l" });
	if (m_rightHand.empty()) m_rightHand = FindBone(boneNames, { "fist.r", "hand.r" });
	// The Sword and Shield Pack uses its original Mixamo names directly.
	if (m_leftArm.empty()) m_leftArm = FindBone(boneNames, { "mixamorig:LeftArm" });
	if (m_rightArm.empty()) m_rightArm = FindBone(boneNames, { "mixamorig:RightArm" });
	if (m_leftElbow.empty()) m_leftElbow = FindBone(boneNames, { "mixamorig:LeftForeArm" });
	if (m_rightElbow.empty()) m_rightElbow = FindBone(boneNames, { "mixamorig:RightForeArm" });
	if (m_leftHand.empty()) m_leftHand = FindBone(boneNames, { "mixamorig:LeftHand" });
	if (m_rightHand.empty()) m_rightHand = FindBone(boneNames, { "mixamorig:RightHand" });
	if (m_leftLeg.empty()) m_leftLeg = FindBone(boneNames, { "upperleg.l" });
	if (m_rightLeg.empty()) m_rightLeg = FindBone(boneNames, { "upperleg.r" });
	if (m_leftKnee.empty()) m_leftKnee = FindBone(boneNames, { "lowerleg.l" });
	if (m_rightKnee.empty()) m_rightKnee = FindBone(boneNames, { "lowerleg.r" });
	if (m_leftFoot.empty()) m_leftFoot = FindBone(boneNames, { "foot.l" });
	if (m_rightFoot.empty()) m_rightFoot = FindBone(boneNames, { "foot.r" });

	m_motionChoices = {
		"assets/motion/sword_shield_attack_safe.motion",
		"assets/motion/sword_shield_attack.motion",
		"assets/motion/sword_shield_attack_2.motion",
		"assets/motion/sword_shield_attack_3.motion",
		"assets/motion/sword_shield_attack_4.motion",
		"assets/motion/sword_shield_slash.motion",
		"assets/motion/sword_shield_slash_2.motion",
		"assets/motion/sword_shield_slash_3.motion",
		"assets/motion/sword_shield_slash_4.motion",
		"assets/motion/sword_shield_slash_5.motion",
	};
	std::ifstream selectedMotion("assets/motion/selected_attack.txt");
	std::string savedMotion;
	if (selectedMotion >> savedMotion &&
		std::find(m_motionChoices.begin(), m_motionChoices.end(), savedMotion) != m_motionChoices.end())
		m_motionFilename = savedMotion;
	const auto selectedIt = std::find(m_motionChoices.begin(), m_motionChoices.end(), m_motionFilename);
	if (selectedIt != m_motionChoices.end())
		m_selectedMotionIndex = static_cast<int>(std::distance(m_motionChoices.begin(), selectedIt));

	LoadMotionFile(m_motionFilename);
	// Start from the target GLB's own bind pose (a stable T-pose) and apply only
	// the two upper-arm rotations authored in sword_shield_idle_safe.motion.
	// The downloaded idle FBX contains a crouched pose and source-unit root
	// translations, so using it as a base makes the feet turn/sink and also
	// corrupts every downloaded attack that is layered on top of it.
	const bool idleLoaded = LoadIdlePose("assets/motion/sword_shield_idle_safe.motion");
	const bool seatedLoaded = LoadSeatedPoseFile("assets/motion/sword_shield_idle.motion");
	std::cout << "[Animator] attack bones=" << m_motionMappedBoneCount
		<< " idle bones=" << m_idlePose.size()
		<< " seated bones=" << m_seatedPose.size()
		<< " idle loaded=" << (idleLoaded ? "yes" : "no")
		<< " seated loaded=" << (seatedLoaded ? "yes" : "no") << std::endl;
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

void CCharacterAnimator::SetMotionFilename(const std::string& filename)
{
	m_motionFilename = filename;
	LoadMotionFile(m_motionFilename);
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
		std::unordered_map<std::string, Matrix4x4> deltas;
		EvaluateCustomMotion(m_motionTime, deltas);
		std::unordered_map<std::string, Matrix4x4> customPose = m_idlePose;
		for (const auto& [boneName, delta] : deltas)
		{
			const auto idle = m_idlePose.find(boneName);
			customPose[boneName] = idle != m_idlePose.end()
				? delta * idle->second
				: delta;
		}
		mesh.UpdateManualPose(boneComb, customPose);
		return;
	}

	const float phase = std::sinf(state.motionTime * 7.0f);
	// The paladin is a heavy armored character.  The old walk used nearly
	// 40-degree hip swings plus a large knee/foot twist, which made the
	// automatically weighted cape and plates appear to tear apart.  Keep the
	// stride readable while leaving enough range for the feet to separate.
	const float armSwing = state.walking ? phase * 0.30f : 0.0f;
	const float legSwing = state.walking ? -phase * 0.38f : 0.0f;
	const float leftKneeSwing = state.walking ? std::max(0.0f, phase) * 0.24f : 0.0f;
	const float rightKneeSwing = state.walking ? std::max(0.0f, -phase) * 0.24f : 0.0f;
	const float footSwing = state.walking ? phase * 0.20f : (state.jumping ? -0.2f : 0.0f);
	const float armRaise = state.jumping ? 0.75f : 0.0f;
	// Idle is intentionally conservative: the bind pose already gives us a
	// stable standing lower body, so only add a tiny breathing motion to the
	// spine.  No idle key is allowed to rotate the legs or feet.
	const float idleBreath = (!state.walking && !state.jumping)
		? std::sinf(state.motionTime * 2.2f) * 0.018f
		: 0.0f;
	std::unordered_map<std::string, Matrix4x4> deltas;
	SetRotation(deltas, m_spine, Matrix4x4::CreateRotationX(idleBreath));
	SetRotation(deltas, m_spine01, Matrix4x4::CreateRotationX(idleBreath * 0.65f));

	const float leftArmSwing = armSwing + armRaise;
	const float rightArmSwing = armSwing - armRaise;
	SetRotation(deltas, m_leftArm,
		Matrix4x4::CreateRotationY(leftArmSwing) * Matrix4x4::CreateRotationZ(leftArmSwing * 0.2f));
	SetRotation(deltas, m_rightArm,
		Matrix4x4::CreateRotationY(rightArmSwing) * Matrix4x4::CreateRotationZ(rightArmSwing * 0.2f));
	SetRotation(deltas, m_leftElbow, Matrix4x4::CreateRotationZ(state.walking ? -phase * 0.10f : 0.0f));
	SetRotation(deltas, m_rightElbow, Matrix4x4::CreateRotationZ(state.walking ? phase * 0.10f : 0.0f));
	SetRotation(deltas, m_leftHand, Matrix4x4::CreateRotationZ(-armSwing * 0.25f));
	SetRotation(deltas, m_rightHand, Matrix4x4::CreateRotationZ(armSwing * 0.25f));
	SetRotation(deltas, m_leftLeg, Matrix4x4::CreateRotationZ(legSwing * 0.25f) * Matrix4x4::CreateRotationX(legSwing));
	SetRotation(deltas, m_rightLeg, Matrix4x4::CreateRotationZ(-legSwing * 0.25f) * Matrix4x4::CreateRotationX(-legSwing));
	SetRotation(deltas, m_leftKnee, Matrix4x4::CreateRotationX(leftKneeSwing));
	SetRotation(deltas, m_rightKnee, Matrix4x4::CreateRotationX(rightKneeSwing));
	SetRotation(deltas, m_leftFoot, Matrix4x4::CreateRotationX(footSwing));
	SetRotation(deltas, m_rightFoot, Matrix4x4::CreateRotationX(-footSwing));

	std::unordered_map<std::string, Matrix4x4> pose = m_idlePose;
	for (const auto& [boneName, delta] : deltas)
	{
		const auto idle = m_idlePose.find(boneName);
		pose[boneName] = idle != m_idlePose.end()
			? delta * idle->second
			: delta;
	}
	mesh.UpdateManualPose(boneComb, pose);
}

void CCharacterAnimator::UpdateSeatedPose(
	CAnimationMesh& mesh,
	BoneCombMatrix& boneComb,
	float amount)
{
	amount = std::clamp(amount, 0.0f, 1.0f);
	if (amount >= 0.999f && !m_seatedPose.empty())
	{
		std::unordered_map<std::string, Matrix4x4> pose = m_idlePose;
		static constexpr const char* seatedBones[] = {
			"mixamorig:Hips", "mixamorig:Spine", "mixamorig:Spine1", "mixamorig:Spine2",
			"mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot",
			"mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot"
		};
		for (const char* boneName : seatedBones)
		{
			const auto seated = m_seatedPose.find(boneName);
			if (seated != m_seatedPose.end())
				pose[boneName] = seated->second;
		}
		mesh.UpdateManualPose(boneComb, pose);
		return;
	}
	std::unordered_map<std::string, Matrix4x4> deltas;
	SetRotation(deltas, m_pelvis,
		Matrix4x4::CreateRotationX(0.62f * amount) *
		Matrix4x4::CreateTranslation(0.0f, -0.72f * amount, 0.0f));
	SetRotation(deltas, m_leftLeg,
		Matrix4x4::CreateRotationZ(-0.18f * amount) * Matrix4x4::CreateRotationX(-1.62f * amount));
	SetRotation(deltas, m_rightLeg,
		Matrix4x4::CreateRotationZ(0.18f * amount) * Matrix4x4::CreateRotationX(-1.62f * amount));
	SetRotation(deltas, m_leftKnee, Matrix4x4::CreateRotationX(2.55f * amount));
	SetRotation(deltas, m_rightKnee, Matrix4x4::CreateRotationX(2.55f * amount));
	SetRotation(deltas, m_leftFoot, Matrix4x4::CreateRotationX(-0.62f * amount));
	SetRotation(deltas, m_rightFoot, Matrix4x4::CreateRotationX(-0.62f * amount));
	SetRotation(deltas, m_spine, Matrix4x4::CreateRotationX(-0.34f * amount));
	SetRotation(deltas, m_spine01, Matrix4x4::CreateRotationX(-0.22f * amount));
	SetRotation(deltas, m_leftArm, Matrix4x4::CreateRotationZ(1.05f * amount));
	SetRotation(deltas, m_rightArm, Matrix4x4::CreateRotationZ(-1.05f * amount));

	std::unordered_map<std::string, Matrix4x4> pose = m_idlePose;
	for (const auto& [boneName, delta] : deltas)
	{
		const auto idle = m_idlePose.find(boneName);
		pose[boneName] = idle != m_idlePose.end()
			? delta * idle->second
			: delta;
	}
	mesh.UpdateManualPose(boneComb, pose);
}

void CCharacterAnimator::UpdateTitleContractPose(
	CAnimationMesh& mesh,
	BoneCombMatrix& boneComb,
	float reachAmount,
	float sheatheAmount,
	float drawAmount,
	float walkTime,
	aiAnimation* walkAnimation,
	int walkFrame)
{
	reachAmount = std::clamp(reachAmount, 0.0f, 1.0f);
	sheatheAmount = std::clamp(sheatheAmount, 0.0f, 1.0f);
	drawAmount = std::clamp(drawAmount, 0.0f, 1.0f);

	// The left hand is the free hand on the title model.  It reaches toward the
	// contract while the right arm lowers toward the sheath.  Once the sword
	// starts coming free, pull the elbow back and carry the contract lower at
	// the side instead of keeping it posed like a precious object at the chest.
	// This keeps the handoff readable while making the departure feel like a
	// hunter preparing to fight.
	const float carryAmount = drawAmount;
	const float reachPose = reachAmount * (1.0f - carryAmount);
	std::unordered_map<std::string, Matrix4x4> deltas;
	SetRotation(deltas, m_leftArm,
		Matrix4x4::CreateRotationZ(0.72f * reachPose + 0.20f * carryAmount) *
		Matrix4x4::CreateRotationX(-0.42f * reachPose + 0.10f * carryAmount) *
		Matrix4x4::CreateRotationY(-0.22f * reachPose - 0.08f * carryAmount));
	SetRotation(deltas, m_leftElbow,
		Matrix4x4::CreateRotationX(-1.05f * reachPose - 0.48f * carryAmount));
	SetRotation(deltas, m_leftHand,
		Matrix4x4::CreateRotationZ(-0.28f * reachPose + 0.08f * carryAmount));

	// First lower the sword hand to the hip, then reverse that pose into a
	// readable draw/ready position after the contract is accepted.
	SetRotation(deltas, m_rightArm,
		Matrix4x4::CreateRotationZ(-0.82f * sheatheAmount) *
		Matrix4x4::CreateRotationX(0.28f * sheatheAmount) *
		Matrix4x4::CreateRotationZ(1.05f * drawAmount) *
		Matrix4x4::CreateRotationX(-0.32f * drawAmount));
	SetRotation(deltas, m_rightElbow,
		Matrix4x4::CreateRotationX(-0.72f * sheatheAmount +
			0.98f * drawAmount));
	SetRotation(deltas, m_rightHand,
		Matrix4x4::CreateRotationZ(-0.24f * sheatheAmount +
			0.26f * drawAmount));

	// Use the supplied walk clip for the lower body.  Keep the previous
	// restrained stride only as a fallback when the optional clip is absent.
	if (walkAnimation == nullptr)
	{
		const float walkBlend = drawAmount * drawAmount * (3.0f - 2.0f * drawAmount);
		const float phase = std::sinf(walkTime * 7.0f) * walkBlend;
		const float legSwing = -phase * 0.38f;
		const float leftKneeSwing = std::max(0.0f, phase) * 0.24f;
		const float rightKneeSwing = std::max(0.0f, -phase) * 0.24f;
		const float footSwing = phase * 0.20f;
		SetRotation(deltas, m_leftLeg,
			Matrix4x4::CreateRotationZ(legSwing * 0.25f) *
			Matrix4x4::CreateRotationX(legSwing));
		SetRotation(deltas, m_rightLeg,
			Matrix4x4::CreateRotationZ(-legSwing * 0.25f) *
			Matrix4x4::CreateRotationX(-legSwing));
		SetRotation(deltas, m_leftKnee, Matrix4x4::CreateRotationX(leftKneeSwing));
		SetRotation(deltas, m_rightKnee, Matrix4x4::CreateRotationX(rightKneeSwing));
		SetRotation(deltas, m_leftFoot, Matrix4x4::CreateRotationX(footSwing));
		SetRotation(deltas, m_rightFoot, Matrix4x4::CreateRotationX(-footSwing));
	}

	std::unordered_map<std::string, Matrix4x4> pose = m_idlePose;
	for (const auto& [boneName, delta] : deltas)
	{
		const auto idle = m_idlePose.find(boneName);
		pose[boneName] = idle != m_idlePose.end()
			? delta * idle->second
			: delta;
	}
	if (walkAnimation != nullptr)
	{
		const std::vector<std::string> lowerBodyBones = {
			// Do not import the FBX Hips rotation.  This clip's root is authored
			// with a different facing basis and flips the entire title character
			// upside down when applied to the SwordShieldPack rig.
			m_leftLeg, m_rightLeg, m_leftKnee, m_rightKnee,
			m_leftFoot, m_rightFoot,
		};
		mesh.UpdateAnimationWithManualPose(
			boneComb, walkAnimation, walkFrame, pose, lowerBodyBones);
	}
	else
	{
		mesh.UpdateManualPose(boneComb, pose);
	}
}

bool CCharacterAnimator::LoadSeatedPoseFile(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file)
		return false;

	std::string token;
	std::string currentBone;
	std::unordered_map<std::string, bool> captured;
	m_seatedPose.clear();
	while (file >> token)
	{
		if (token == "bone")
		{
			std::string requestedBone;
			file >> std::quoted(requestedBone);
			currentBone = FindBone(m_boneNames, { std::string_view(requestedBone) });
			if (currentBone.empty())
				currentBone = requestedBone;
			captured[currentBone] = false;
		}
		else if (token == "key" && !currentBone.empty())
		{
			MotionKeyframe key;
			file >> key.time
				>> key.rotation.x >> key.rotation.y >> key.rotation.z
				>> key.position.x >> key.position.y >> key.position.z
				>> key.scale.x >> key.scale.y >> key.scale.z;
			if (!captured[currentBone])
			{
				key.position = Vector3(0.0f, 0.0f, 0.0f);
				key.scale = Vector3(1.0f, 1.0f, 1.0f);
				m_seatedPose[currentBone] = MotionKeyToMatrix(key);
				captured[currentBone] = true;
			}
		}
		else if (token == "endbone")
			currentBone.clear();
	}
	return !m_seatedPose.empty();
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
	m_motionDuration = 0.95f;
	const auto addKeys = [this](const std::string& boneName, const std::vector<Vector3>& rotations) {
		if (boneName.empty())
			return;
		BoneKeys& keys = m_motionKeys[boneName];
		const float times[] = { 0.0f, 0.12f, 0.28f, 0.42f, 0.56f, 0.75f, 0.95f };
		for (size_t i = 0; i < rotations.size() && i < 7; ++i)
		{
			MotionKeyframe key;
			key.time = times[i];
			key.rotation = rotations[i];
			keys.push_back(key);
		}
	};
	// Keep this fallback in sync with the reference-retargeted attack motion.
	// It is also
	// used when the editable motion file is missing or cannot be parsed.
	addKeys(m_pelvis, { {}, {0.0f, -0.08f, 0.02f}, {0.0f, -0.18f, 0.04f}, {0.0f, 0.22f, -0.03f}, {0.0f, 0.12f, -0.02f}, {0.0f, 0.04f, 0.0f}, {} });
	addKeys(m_spine, { {}, {-0.04f, -0.10f, 0.03f}, {-0.08f, -0.20f, 0.04f}, {0.16f, 0.18f, -0.03f}, {0.10f, 0.10f, -0.02f}, {0.02f, 0.03f, 0.0f}, {} });
	addKeys(m_spine01, { {}, {-0.05f, -0.14f, 0.04f}, {-0.10f, -0.26f, 0.06f}, {0.20f, 0.26f, -0.04f}, {0.14f, 0.14f, -0.03f}, {0.03f, 0.04f, 0.0f}, {} });
	addKeys(m_spine02, { {}, {-0.08f, -0.18f, 0.06f}, {-0.16f, -0.32f, 0.08f}, {0.24f, 0.32f, -0.06f}, {0.16f, 0.18f, -0.04f}, {0.04f, 0.05f, 0.0f}, {} });
	addKeys(m_rightArm, { {}, {-0.55f, 0.15f, -0.45f}, {-0.95f, 0.25f, -0.75f}, {0.85f, -0.50f, 0.70f}, {0.65f, -0.42f, 0.58f}, {0.18f, -0.12f, 0.16f}, {} });
	addKeys(m_rightElbow, { {}, {-0.45f, 0.0f, 0.0f}, {-0.95f, 0.0f, 0.0f}, {0.25f, 0.0f, 0.0f}, {0.35f, 0.0f, 0.0f}, {0.10f, 0.0f, 0.0f}, {} });
	addKeys(m_rightHand, { {}, {0.0f, 0.0f, -0.15f}, {0.0f, 0.0f, -0.25f}, {0.0f, 0.0f, 0.35f}, {0.0f, 0.0f, 0.25f}, {0.0f, 0.0f, 0.08f}, {} });
	addKeys(m_leftArm, { {}, {0.16f, 0.0f, 0.18f}, {0.28f, 0.0f, 0.32f}, {-0.24f, 0.0f, -0.20f}, {-0.18f, 0.0f, -0.15f}, {-0.06f, 0.0f, -0.05f}, {} });
	addKeys(m_leftElbow, { {}, {0.12f, 0.0f, 0.0f}, {0.22f, 0.0f, 0.0f}, {-0.18f, 0.0f, 0.0f}, {-0.12f, 0.0f, 0.0f}, {-0.04f, 0.0f, 0.0f}, {} });
	addKeys(m_leftLeg, { {}, {0.08f, 0.0f, 0.06f}, {0.14f, 0.0f, 0.10f}, {-0.18f, 0.0f, -0.08f}, {-0.10f, 0.0f, -0.05f}, {-0.04f, 0.0f, 0.0f}, {} });
	addKeys(m_rightLeg, { {}, {-0.08f, 0.0f, -0.06f}, {-0.14f, 0.0f, -0.10f}, {0.18f, 0.0f, 0.08f}, {0.10f, 0.0f, 0.05f}, {0.04f, 0.0f, 0.0f}, {} });
	addKeys(m_leftKnee, { {}, {0.0f, 0.0f, 0.08f}, {0.0f, 0.0f, 0.12f}, {0.16f, 0.0f, 0.0f}, {0.10f, 0.0f, 0.0f}, {0.03f, 0.0f, 0.0f}, {} });
	addKeys(m_rightKnee, { {}, {0.0f, 0.0f, -0.06f}, {0.0f, 0.0f, -0.10f}, {0.12f, 0.0f, 0.0f}, {0.08f, 0.0f, 0.0f}, {0.03f, 0.0f, 0.0f}, {} });
	addKeys(m_leftFoot, { {}, {0.0f, 0.0f, -0.04f}, {0.0f, 0.0f, -0.08f}, {-0.08f, 0.0f, 0.0f}, {-0.05f, 0.0f, 0.0f}, {-0.02f, 0.0f, 0.0f}, {} });
	addKeys(m_rightFoot, { {}, {0.0f, 0.0f, 0.04f}, {0.0f, 0.0f, 0.08f}, {-0.08f, 0.0f, 0.0f}, {-0.05f, 0.0f, 0.0f}, {-0.02f, 0.0f, 0.0f}, {} });
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
		Quaternion interpolatedRotation = Quaternion::Identity;
		bool hasInterpolatedRotation = false;
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
					const Matrix4x4 rotationA =
						Matrix4x4::CreateRotationX(a.rotation.x) *
						Matrix4x4::CreateRotationY(a.rotation.y) *
						Matrix4x4::CreateRotationZ(a.rotation.z);
					const Matrix4x4 rotationB =
						Matrix4x4::CreateRotationX(b.rotation.x) *
						Matrix4x4::CreateRotationY(b.rotation.y) *
						Matrix4x4::CreateRotationZ(b.rotation.z);
					// Euler角の直接補間は±PI境界で長い回転を生むため、
					// 回転はクォータニオンの球面補間で滑らかにつなぐ。
					interpolatedRotation = Quaternion::Slerp(
						Quaternion::CreateFromRotationMatrix(rotationA),
						Quaternion::CreateFromRotationMatrix(rotationB), t);
					hasInterpolatedRotation = true;
					pose.position = a.position + (b.position - a.position) * t;
					pose.scale = a.scale + (b.scale - a.scale) * t;
					break;
				}
			}
		}

		const Matrix4x4 rotation = hasInterpolatedRotation
			? Matrix4x4::CreateFromQuaternion(interpolatedRotation)
			: Matrix4x4::CreateRotationX(pose.rotation.x) *
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

bool CCharacterAnimator::LoadIdlePose(const std::string& filename)
{
	std::ifstream file(filename);
	if (!file)
	{
		m_idlePose.clear();
		return false;
	}

	std::string token;
	std::string currentBone;
	std::unordered_map<std::string, bool> captured;
	m_idlePose.clear();
	while (file >> token)
	{
		if (token == "bone")
		{
			std::string requestedBone;
			file >> std::quoted(requestedBone);
			currentBone = FindBone(m_boneNames, { std::string_view(requestedBone) });
			if (currentBone.empty())
				currentBone = requestedBone;
			captured[currentBone] = false;
		}
		else if (token == "key" && !currentBone.empty())
		{
			MotionKeyframe key;
			file >> key.time
				>> key.rotation.x >> key.rotation.y >> key.rotation.z
				>> key.position.x >> key.position.y >> key.position.z
				>> key.scale.x >> key.scale.y >> key.scale.z;
			if (!captured[currentBone])
			{
				// The FBX idle clip contains absolute/root translation in its own
				// importer units (for example Hips Y=-15).  The target GLB already
				// owns the correct rest translations; copying those values here
				// sinks the scaled character into the field and makes retargeting
				// dependent on the source file's unit system.  Keep only the local
				// pose rotation and let GameScene ground the rendered mesh from its
				// actual vertex bounds.
				key.position = Vector3(0.0f, 0.0f, 0.0f);
				key.scale = Vector3(1.0f, 1.0f, 1.0f);
				m_idlePose[currentBone] = MotionKeyToMatrix(key);
				captured[currentBone] = true;
			}
		}
		else if (token == "endbone")
			currentBone.clear();
	}
	// Keep the shield hidden for the current sword-only combat pass.  This is a
	// render choice, not part of the character pose, and does not alter hand or
	// finger transforms.
	const auto shield = m_idlePose.find("mixamorig:Shield_joint");
	if (shield != m_idlePose.end())
		shield->second = Matrix4x4::CreateScale(0.0f, 0.0f, 0.0f);

	// The downloaded idle clip leaves the right hand in an open/T-pose.  The
	// sword is already skinned to Sword_joint, so only the hand fingers need a
	// local curl to make the grip read correctly.  Curling each phalanx around
	// its local X axis keeps the palm/wrist animation untouched and is inherited
	// by every downloaded attack because m_idlePose is used as the attack base.
	const auto addGripCurl = [this](const char* finger, float proximal, float middle, float distal)
	{
		const float curls[] = { proximal, middle, distal };
		for (int segment = 1; segment <= 3; ++segment)
		{
			const std::string boneName =
				std::string("mixamorig:RightHand") + finger + std::to_string(segment);
			const std::string resolved = FindBone(m_boneNames, { std::string_view(boneName) });
			if (!resolved.empty())
				m_idlePose[resolved] = Matrix4x4::CreateRotationX(curls[segment - 1]);
		}
	};
	// Negative X is the curl direction for this Mixamo hand's local axes.
	addGripCurl("Index", -0.85f, -1.05f, -0.80f);
	addGripCurl("Middle", -0.90f, -1.10f, -0.85f);
	addGripCurl("Ring", -0.90f, -1.10f, -0.85f);
	addGripCurl("Pinky", -0.95f, -1.15f, -0.90f);
	const std::string thumb1 = FindBone(m_boneNames, { "mixamorig:RightHandThumb1" });
	if (!thumb1.empty())
		m_idlePose[thumb1] = Matrix4x4::CreateRotationZ(0.45f) * Matrix4x4::CreateRotationX(-0.55f);
	return !m_idlePose.empty();
}

void CCharacterAnimator::RenderMotionEditor()
{
	ImGui::SetNextWindowPos(ImVec2(20.0f, 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 760.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Motion Editor");
	ImGui::Text("攻撃モーション編集：ボーンを選び、時間ごとにポーズを登録します");
	if (!m_motionChoices.empty())
	{
		const std::string currentLabel = std::filesystem::path(m_motionFilename).stem().string();
		if (ImGui::BeginCombo("Downloaded attack", currentLabel.c_str()))
		{
			for (size_t i = 0; i < m_motionChoices.size(); ++i)
			{
				const std::string label = std::filesystem::path(m_motionChoices[i]).stem().string();
				const bool selected = static_cast<int>(i) == m_selectedMotionIndex;
				if (ImGui::Selectable(label.c_str(), selected))
				{
					if (LoadMotionFile(m_motionChoices[i]))
					{
						m_selectedMotionIndex = static_cast<int>(i);
						std::ofstream selection("assets/motion/selected_attack.txt", std::ios::trunc);
						if (selection)
							selection << m_motionFilename << "\n";
					}
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Selected clip is also used by GameScene next launch");
	}
	ImGui::Checkbox("Use custom motion", &m_useCustomMotion);
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_motionLoop);
	ImGui::Text("状態: %s   読込ボーン: %d   時間: %.3f / %.3f",
		m_motionPlaying ? "再生中" : "停止中",
		m_motionMappedBoneCount,
		m_motionTime,
		m_motionDuration);
	ImGui::Text("Idle base pose bones: %d", static_cast<int>(m_idlePose.size()));
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
