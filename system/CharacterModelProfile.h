#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <SimpleMath.h>

#include "CombatAttackTable.h"

using DirectX::SimpleMath::Vector3;

enum class CanonicalJoint : unsigned char
{
	Root, Hips, Spine, Chest, Neck, Head,
	LeftUpperArm, LeftLowerArm, LeftHand,
	RightUpperArm, RightLowerArm, RightHand,
	LeftUpperLeg, LeftLowerLeg, LeftFoot,
	RightUpperLeg, RightLowerLeg, RightFoot,
	// 後から足した骨。並びを変えると既存の表がずれるので末尾へ足す。
	Spine1, LeftToe, RightToe,
	Count
};

/** 移動のブレンドツリーのクリップの並び(CCharacterAnimator の [方向(前,右,後,左) x 2 + 歩調(歩き,走り)])。 */
enum class LocomotionClipSlot : unsigned char
{
	WalkForward, RunForward,
	WalkRight, RunRight,
	WalkBackward, RunBackward,
	WalkLeft, RunLeft,
	Count
};

/** dev_settings.ini でクリップを差し替えるときのキー(LocomotionClipSlot の並び)。 */
inline const char* LocomotionClipKey(LocomotionClipSlot slot)
{
	static const char* keys[static_cast<size_t>(LocomotionClipSlot::Count)] = {
		"walk_forward", "run_forward", "walk_right", "run_right",
		"walk_backward", "run_backward", "walk_left", "run_left" };
	return keys[static_cast<size_t>(slot)];
}

/**
 * プレイヤーのモデルごとの違いをまとめた表。
 *
 * - Sword and Shield Pack(assets/model/SwordShieldPack): リポジトリに入っている Mixamo のモデルとモーション。
 * - 騎士(assets/model/Knight): 購入した有料のモデル。リポジトリには入れていない(.gitignore)。
 *   tools/blender_export_knight.py で .blend から書き出して置く。置いてあればこちらを使い、無ければ Sword and Shield Pack に戻す。
 *   dev_settings.ini の player_model=paladin で Sword and Shield Pack に固定できる(見比べる用)。
 */
struct CharacterModelProfile
{
	const char* id = "character";
	const char* displayName = "";
	const char* meshPath = "";
	const char* textureDirectory = "";
	Vector3 modelScale{1.0f, 1.0f, 1.0f};
	float groundY = -0.3f;
	std::string weaponBone;
	Vector3 weaponRotationDegrees{0.0f, 0.0f, 0.0f};
	Vector3 weaponHandOffset{0.0f, 0.0f, 0.0f};
	float weaponScale = 1.0f;
	/**
	 * 剣がモデルの一部(weaponBone へスキンした部品)として入っているか。
	 * true なら、メッシュ名に embeddedWeaponMeshKeyword を含む部品を剣とみなし、
	 * その頂点を weaponBone で動かした位置から攻撃判定・軌跡の柄と剣先を求める。
	 * 剣先の向きは weaponHandBone(握っている手)から遠い側。
	 * (Sword and Shield Pack はファイル名で判断する古い経路のままにしている)
	 */
	bool embeddedWeapon = false;
	std::string embeddedWeaponMeshKeyword = "sword";
	std::string weaponHandBone;
	std::array<std::vector<std::string>, static_cast<size_t>(CanonicalJoint::Count)> canonicalAliases;

	// --- クリップのかけ方(CCharacterAnimator) ---
	/**
	 * 待機・移動・攻撃のクリップを、脚と腰を除くすべての骨へかけるか。
	 * false(Sword and Shield Pack)は、背骨と上腕・前腕の7本だけへかける(手は剣の握りを保つため、
	 * 肩・首などは手作業の姿勢のまま)。その作りは Mixamo の骨格に合わせて調整したもの。
	 * true にすると、肩・手首・手・指・首・頭までクリップどおりに動く。
	 */
	bool clipsDriveWholeBody = false;
	/// 脚として扱う骨の追加(腰と太ももの間にある骨など)。脚は移動・待機のクリップが受け持つ。
	std::vector<std::string> extraLowerBodyBones;
	/// Sword and Shield Pack 用に作った姿勢ファイル(assets/motion/*.motion)を重ねるか。骨格が違うモデルでは使わない。
	bool useLegacyMotionFiles = true;
	/**
	 * 攻撃中の腰の回転の取り込み方(CCharacterAnimator::SetAttackHipsRotationMode の既定)。
	 * 1 = クリップの腰の回転を休止姿勢へ重ねる(Mixamo のクリップは腰の休止姿勢が単位回転なので、これで合う)。
	 * 2 = クリップの腰の回転をそのまま使う(クリップとモデルが同じリグから書き出したもの)。
	 */
	int attackHipsRotationMode = 1;

	// --- クリップ ---
	// 移動のブレンドツリー。空なら同じ歩調の前向きクリップで代用する(前歩きは必須)。
	std::array<std::string, static_cast<size_t>(LocomotionClipSlot::Count)> locomotionClips;
	std::string idleClip;
	std::string impactClip;
	std::string deathClip;   ///< 空なら力尽きるモーションなし(立ったまま)
	/// 力尽きるクリップの腰の水平移動を、キャラクターの位置の移動にするか(倒れながら後ずさる等)。
	/// 移動量の計算は Mixamo のクリップの軸(Y が上・Z が前後)を前提にしている。
	bool deathRootMotion = true;
	/// 技(コンボ・ダッシュ攻撃)の既定のクリップと時間の組。値は CombatAttackTable.h の DefaultPlayerAttack。
	Combat::PlayerAttackSet attackSet = Combat::PlayerAttackSet::SwordShield;
};

inline CharacterModelProfile MakeSwordShieldPlayerProfile()
{
	CharacterModelProfile value;
	value.id = "sword_shield_player";
	value.displayName = "Sword and Shield Pack";
	value.meshPath = "assets/model/SwordShieldPack/runtime/SwordShieldPack_Player.glb";
	value.textureDirectory = "assets/model/SwordShieldPack/runtime/";
	value.modelScale = Vector3(10.0f, 10.0f, 10.0f);
	value.groundY = -0.3f;
	value.weaponBone = "mixamorig:Sword_joint";
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
	value.canonicalAliases[static_cast<size_t>(CanonicalJoint::Spine1)] = { "mixamorig:Spine1" };
	value.canonicalAliases[static_cast<size_t>(CanonicalJoint::LeftToe)] = { "mixamorig:LeftToeBase" };
	value.canonicalAliases[static_cast<size_t>(CanonicalJoint::RightToe)] = { "mixamorig:RightToeBase" };

	// 手持ちのクリップの腰の移動量を調べて方向を割り当てた(2026-09-15):
	//   walk=前 / walk (2)=後ろ / run=前 / run (2)=後ろ
	//   strafe (2)=歩き+X / strafe (3)=走り+X / strafe=歩き-X / strafe (4)=走り-X
	// Mixamoのキャラクターは+Zを向いており、+Xがキャラクターの左になる。
	using Slot = LocomotionClipSlot;
	const auto clip = [&value](Slot slot, const char* file) { value.locomotionClips[static_cast<size_t>(slot)] = file; };
	clip(Slot::WalkForward, "assets/motion/sword and shield walk.fbx");
	clip(Slot::RunForward, "assets/motion/sword and shield run.fbx");
	clip(Slot::WalkRight, "assets/motion/sword and shield strafe.fbx");
	clip(Slot::RunRight, "assets/motion/sword and shield strafe (4).fbx");
	clip(Slot::WalkBackward, "assets/motion/sword and shield walk (2).fbx");
	clip(Slot::RunBackward, "assets/motion/sword and shield run (2).fbx");
	clip(Slot::WalkLeft, "assets/motion/sword and shield strafe (2).fbx");
	clip(Slot::RunLeft, "assets/motion/sword and shield strafe (3).fbx");
	value.idleClip = "assets/motion/sword and shield idle.fbx";
	value.impactClip = "assets/motion/sword and shield impact.fbx";
	value.deathClip = "assets/motion/sword and shield death.fbx";
	value.attackSet = Combat::PlayerAttackSet::SwordShield;
	return value;
}

/**
 * 購入した騎士のモデル。骨の名前は Blender のリグのもの(pelvis / thighl / armupperr / handr ...)。
 *
 * - 大きさ: 元のモデルは身長約2.16(足元が0)。FBX はセンチメートルで書かれるので216になり、
 *   倍率0.082で身長約17.7(Sword and Shield Pack と並べて撮って、同じくらいの大きさに見えることを確かめた)。
 * - 剣: 右手の骨 handr.002 へスキンした部品として Knight.fbx に入っている(書き出しスクリプトで作り替えた)。
 * - 歩き・走り・横歩きは、前のモデル(Sword and Shield Pack)のクリップを書き出しスクリプトで
 *   騎士の骨格へ移し替えた Knight_mx_*.fbx を使う。
 *   騎士の元の歩きは腰を落としたすり足(約1.1m/秒)、走りは剣を前へ突き出した小走り(約1.3m/秒)で、
 *   ゲームの移動の速さ(歩き20・ダッシュ45 = 約2.4m/秒・5.5m/秒)に合わせると、歩きが約2.2倍速、
 *   走りが上限の4倍速で回り、足がばたついて見えた(移動の再生速度は足の流れる速さから決まる:
 *   CCharacterAnimator::MeasureLocomotionFootPhases)。前のモデルの移動はこの速さに合わせて選んだもの。
 * - 力尽きるクリップは元のモデルに無いので、Sword and Shield Pack(Mixamo)の death を書き出しスクリプトで
 *   騎士の骨格へ移し替えた Knight_death.fbx を使う。腰の位置の軸が Mixamo と違う(Z が上・-Y が前)ので、
 *   倒れる間の位置の移動(ルートモーション)は使わず、その場で倒れる。
 */
inline CharacterModelProfile MakeKnightPlayerProfile()
{
	CharacterModelProfile value;
	value.id = "knight_player";
	value.displayName = "Knight";
	value.meshPath = "assets/model/Knight/Knight.fbx";
	value.textureDirectory = "assets/model/Knight/";
	// Blender の FBX はセンチメートルで書かれる(身長2.16 → 216)。0.082倍で身長約17.7。
	value.modelScale = Vector3(0.082f, 0.082f, 0.082f);
	value.groundY = -0.3f;
	value.weaponBone = "handr.002";
	value.weaponHandBone = "handr";
	value.embeddedWeapon = true;
	using J = CanonicalJoint;
	const auto alias = [&value](J joint, std::vector<std::string> names) { value.canonicalAliases[static_cast<size_t>(joint)] = std::move(names); };
	alias(J::Hips, { "pelvis" });
	alias(J::Spine, { "spine" });
	alias(J::Spine1, { "spine1" });
	alias(J::Chest, { "CHEST" });
	alias(J::Neck, { "neck" });
	alias(J::Head, { "head" });
	alias(J::LeftUpperArm, { "armupperl" });
	alias(J::LeftLowerArm, { "armlowerl" });
	alias(J::LeftHand, { "handl" });
	alias(J::RightUpperArm, { "armupperr" });
	alias(J::RightLowerArm, { "armlowerr" });
	alias(J::RightHand, { "handr" });
	alias(J::LeftUpperLeg, { "thighl" });
	alias(J::LeftLowerLeg, { "calfl" });
	alias(J::LeftFoot, { "footl" });
	alias(J::LeftToe, { "toesL" });
	alias(J::RightUpperLeg, { "thighr" });
	alias(J::RightLowerLeg, { "calfr" });
	alias(J::RightFoot, { "footr" });
	alias(J::RightToe, { "toesR" });
	value.clipsDriveWholeBody = true;
	// pelvis の子で、太もも(thighl / thighr)の親。脚と一緒に動かす。
	value.extraLowerBodyBones = { "hipl", "hipr" };
	value.useLegacyMotionFiles = false;
	// クリップの腰の回転は休止姿勢(約88度)を含んだ値なので、休止姿勢へ重ねると二重に回る。そのまま使う。
	value.attackHipsRotationMode = 2;

	using Slot = LocomotionClipSlot;
	const auto clip = [&value](Slot slot, const char* file) { value.locomotionClips[static_cast<size_t>(slot)] = file; };
	clip(Slot::WalkForward, "assets/model/Knight/anim/Knight_mx_walk.fbx");
	clip(Slot::RunForward, "assets/model/Knight/anim/Knight_mx_run.fbx");
	clip(Slot::WalkRight, "assets/model/Knight/anim/Knight_mx_strafe.fbx");
	clip(Slot::RunRight, "assets/model/Knight/anim/Knight_mx_strafe_4.fbx");
	clip(Slot::WalkBackward, "assets/model/Knight/anim/Knight_mx_walk_2.fbx");
	clip(Slot::RunBackward, "assets/model/Knight/anim/Knight_mx_run_2.fbx");
	clip(Slot::WalkLeft, "assets/model/Knight/anim/Knight_mx_strafe_2.fbx");
	clip(Slot::RunLeft, "assets/model/Knight/anim/Knight_mx_strafe_3.fbx");
	// 騎士の battle_idle は腰を深く落とした構え(腰の高さが脚の長さの0.68倍)で、へっぴり腰に見えたので、
	// 前のモデルの待機を移し替えたものを使う。
	value.idleClip = "assets/model/Knight/anim/Knight_mx_idle.fbx";
	value.impactClip = "assets/model/Knight/anim/Knight_impact.fbx";
	value.deathClip = "assets/model/Knight/anim/Knight_death.fbx";
	value.deathRootMotion = false;
	value.attackSet = Combat::PlayerAttackSet::Knight;
	return value;
}

/** 騎士のモデルが置いてあるか(購入アセットなので、リポジトリを取ってきただけの環境には無い)。 */
inline bool KnightPlayerModelAvailable()
{
	std::error_code error;
	return std::filesystem::exists(MakeKnightPlayerProfile().meshPath, error);
}

namespace PlayerModelDetail
{
inline const CharacterModelProfile*& Active()
{
	static const CharacterModelProfile* active = nullptr;
	return active;
}
}

/**
 * 使うプレイヤーのモデルを選ぶ。id は "knight" か "paladin"(Sword and Shield Pack)。
 * 騎士を選んでもモデルが置いていなければ Sword and Shield Pack にする。
 * GameScene の初期化の最初に1回呼ぶ(以降の GetPlayerModelProfile はこれを返す)。
 */
inline const CharacterModelProfile& SelectPlayerModelProfile(const std::string& id)
{
	static const CharacterModelProfile swordShield = MakeSwordShieldPlayerProfile();
	static const CharacterModelProfile knight = MakeKnightPlayerProfile();
	const bool wantKnight = id != "paladin" && id != "sword_shield";
	PlayerModelDetail::Active() = (wantKnight && KnightPlayerModelAvailable()) ? &knight : &swordShield;
	return *PlayerModelDetail::Active();
}

inline const CharacterModelProfile& GetPlayerModelProfile()
{
	if (PlayerModelDetail::Active() == nullptr)
		SelectPlayerModelProfile("knight");
	return *PlayerModelDetail::Active();
}
