#pragma once

#include <array>
#include <string>
#include <vector>

#include <SimpleMath.h>

using DirectX::SimpleMath::Vector3;

enum class CanonicalJoint : unsigned char
{
	Root, Hips, Spine, Chest, Neck, Head,
	LeftUpperArm, LeftLowerArm, LeftHand,
	RightUpperArm, RightLowerArm, RightHand,
	LeftUpperLeg, LeftLowerLeg, LeftFoot,
	RightUpperLeg, RightLowerLeg, RightFoot,
	Count
};

struct CharacterModelProfile
{
	const char* id = "character";
	const char* meshPath = "";
	const char* textureDirectory = "";
	Vector3 modelScale{1.0f, 1.0f, 1.0f};
	Vector3 modelRotationRadians{0.0f, 0.0f, 0.0f};
	float groundY = -0.3f;
	std::string weaponBone;
	Vector3 weaponRotationDegrees{0.0f, 0.0f, 0.0f};
	Vector3 weaponHandOffset{0.0f, 0.0f, 0.0f};
	float weaponScale = 1.0f;
	// This model shares the exact Mixamo rest rig with the imported FBX clips.
	// Their rotation keys are local deltas and must be composed with rest rotation.
	bool importedAnimationRotationIsRestRelative = false;
	std::array<std::vector<std::string>, static_cast<size_t>(CanonicalJoint::Count)> canonicalAliases;
};

inline const CharacterModelProfile& GetPlayerModelProfile()
{
	static const CharacterModelProfile profile = [] {
		CharacterModelProfile value;
		value.id = "fallen_paladin_rigged_player";
		value.meshPath = "assets/model/FallenPaladin/runtime/fallen_paladin_rigged.glb";
		// GLBへ埋め込み済み。開発時に元テクスチャを参照できるよう、
		// パス自体は残すがランタイムの読み込みはGLB内の画像を使う。
		value.textureDirectory = "assets/model/FallenPaladin/source/";
		value.modelScale = Vector3(10.0f, 10.0f, 10.0f);
		// ゲームのプレイヤー正面(-Z)へ合わせるため、GLB側の向きをY軸で反転する。
		value.modelRotationRadians = Vector3(0.0f, 3.14159265358979323846f, 0.0f);
		value.groundY = -0.3f;
		value.importedAnimationRotationIsRestRelative = true;
		// 剣はGLBに含まれる原本メッシュをそのまま使う。外部の簡易剣は追加しない。
		value.weaponBone.clear();
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Root)] = { "root", "Root" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Hips)] = { "mixamorig:Hips", "Hips", "pelvis" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Spine)] = { "mixamorig:Spine", "Spine", "spine" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Chest)] = { "mixamorig:Spine2", "Spine2", "chest" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Neck)] = { "mixamorig:Neck", "Neck", "neck" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Head)] = { "mixamorig:Head", "Head", "head" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftUpperArm)] = { "mixamorig:LeftArm", "upperarm.l", "LeftArm" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftLowerArm)] = { "mixamorig:LeftForeArm", "lowerarm.l", "LeftForeArm" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftHand)] = { "mixamorig:LeftHand", "hand.l", "LeftHand" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightUpperArm)] = { "mixamorig:RightArm", "upperarm.r", "RightArm" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightLowerArm)] = { "mixamorig:RightForeArm", "lowerarm.r", "RightForeArm" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightHand)] = { "mixamorig:RightHand", "hand.r", "RightHand" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftUpperLeg)] = { "mixamorig:LeftUpLeg", "upperleg.l", "LeftUpLeg" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftLowerLeg)] = { "mixamorig:LeftLeg", "lowerleg.l", "LeftLeg" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftFoot)] = { "mixamorig:LeftFoot", "foot.l", "LeftFoot" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightUpperLeg)] = { "mixamorig:RightUpLeg", "upperleg.r", "RightUpLeg" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightLowerLeg)] = { "mixamorig:RightLeg", "lowerleg.r", "RightLeg" };
		value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightFoot)] = { "mixamorig:RightFoot", "foot.r", "RightFoot" };
		return value;
	}();
	return profile;
}
