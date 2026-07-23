#include "CCharacterAnimator.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

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
	const std::vector<std::string> boneNames = mesh.GetBoneNames();
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
}

void CCharacterAnimator::Update(
	CAnimationMesh& mesh,
	BoneCombMatrix& boneComb,
	const CharacterAnimationState& state)
{
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
