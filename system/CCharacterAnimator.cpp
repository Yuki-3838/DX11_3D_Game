#include "CCharacterAnimator.h"
#include "CombatAttackTable.h"

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

	std::string FindBone(
		const std::vector<std::string>& boneNames,
		const std::vector<std::string>& aliases)
	{
		for (const auto& alias : aliases)
		{
			const std::string normalizedAlias = NormalizeBoneName(alias);
			for (const auto& name : boneNames)
				if (NormalizeBoneName(name) == normalizedAlias)
					return name;
		}
		for (const auto& name : boneNames)
		{
			const std::string normalizedName = NormalizeBoneName(name);
			for (const auto& alias : aliases)
				if (normalizedName.find(NormalizeBoneName(alias)) != std::string::npos)
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

	Matrix4x4 BlendLocalSrt(
		const Matrix4x4& from,
		const Matrix4x4& to,
		float amount)
	{
		const float rate = std::clamp(amount, 0.0f, 1.0f);
		if (rate <= 0.0f)
			return from;
		if (rate >= 1.0f)
			return to;

		const auto extractScale = [](const Matrix4x4& matrix)
		{
			return Vector3(
				Vector3(matrix._11, matrix._12, matrix._13).Length(),
				Vector3(matrix._21, matrix._22, matrix._23).Length(),
				Vector3(matrix._31, matrix._32, matrix._33).Length());
		};
		const auto removeScale = [&extractScale](const Matrix4x4& matrix)
		{
			Matrix4x4 rotation = matrix;
			const Vector3 scale = extractScale(matrix);
			const float sx = std::max(scale.x, 0.0001f);
			const float sy = std::max(scale.y, 0.0001f);
			const float sz = std::max(scale.z, 0.0001f);
			rotation._11 /= sx; rotation._12 /= sx; rotation._13 /= sx;
			rotation._21 /= sy; rotation._22 /= sy; rotation._23 /= sy;
			rotation._31 /= sz; rotation._32 /= sz; rotation._33 /= sz;
			rotation._41 = 0.0f;
			rotation._42 = 0.0f;
			rotation._43 = 0.0f;
			rotation._44 = 1.0f;
			return rotation;
		};

		const Vector3 fromScale = extractScale(from);
		const Vector3 toScale = extractScale(to);
		const Quaternion fromRotation =
			Quaternion::CreateFromRotationMatrix(removeScale(from));
		const Quaternion toRotation =
			Quaternion::CreateFromRotationMatrix(removeScale(to));
		const Quaternion rotation = Quaternion::Slerp(fromRotation, toRotation, rate);
		const Vector3 scale = Vector3::Lerp(fromScale, toScale, rate);
		const Vector3 position = Vector3::Lerp(
			Vector3(from._41, from._42, from._43),
			Vector3(to._41, to._42, to._43),
			rate);
		return Matrix4x4::CreateScale(scale) *
			Matrix4x4::CreateFromQuaternion(rotation) *
			Matrix4x4::CreateTranslation(position);
	}

	Matrix4x4 BlendLocalPose(
		const Matrix4x4& from,
		const Matrix4x4& to,
		float amount)
	{
		return BlendLocalSrt(from, to, amount);
	}
}

void CCharacterAnimator::Initialize(const CAnimationMesh& mesh)
{
	m_boneNames = mesh.GetBoneNames();
	const std::vector<std::string>& boneNames = m_boneNames;
	if (!boneNames.empty())
		m_selectedBone = boneNames.front();
	// 攻撃姿勢では腰と背骨も動かし、右腕だけでなく上半身全体で剣を振る。
	// ボーン名を固定して、.motionファイルを手作業でも読めるようにする。
	m_pelvis = FindBone(boneNames, { "pelvis", "hips", "mixamorig:Hips" });
	m_spine = FindBone(boneNames, { "spine", "mixamorig:Spine" });
	m_spine01 = FindBone(boneNames, { "spine_01", "spine01", "mixamorig:Spine1" });
	m_spine02 = FindBone(boneNames, { "spine_02", "spine02", "mixamorig:Spine2" });
	// D末尾のボーンはFurina/MMD系の変形ボーンである。
	// 頂点ウェイトを持たない制御用・IK用ボーンを誤選択しないよう、先に検索する。
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
	// QuaterniusのglTFはBlender形式の.L/.Rボーン名を使うため、
	// 既存のMMD/Mixamo対応を壊さず別名として追加する。
	if (m_leftArm.empty()) m_leftArm = FindBone(boneNames, { "upperarm.l" });
	if (m_rightArm.empty()) m_rightArm = FindBone(boneNames, { "upperarm.r" });
	if (m_leftElbow.empty()) m_leftElbow = FindBone(boneNames, { "lowerarm.l" });
	if (m_rightElbow.empty()) m_rightElbow = FindBone(boneNames, { "lowerarm.r" });
	if (m_leftHand.empty()) m_leftHand = FindBone(boneNames, { "fist.l", "hand.l" });
	if (m_rightHand.empty()) m_rightHand = FindBone(boneNames, { "fist.r", "hand.r" });
	// Sword and Shield Packは元のMixamoボーン名をそのまま使用する。
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

	m_importedAttackBones.clear();
	m_importedAttackBones.reserve(m_boneNames.size());
	for (const auto& boneName : m_boneNames)
	{
		const std::string normalized = NormalizeBoneName(boneName);
		const bool isGameplayRoot = boneName == m_pelvis ||
			normalized == "root" ||
			normalized == "mixamorig:root" ||
			normalized == "hips" ||
			 normalized == "mixamorig:hips" ||
			 normalized == "pelvis";
		const bool isFinger = normalized.find("finger") != std::string::npos ||
			normalized.find("index") != std::string::npos ||
			normalized.find("middle") != std::string::npos ||
			normalized.find("ring") != std::string::npos ||
			normalized.find("pinky") != std::string::npos ||
			normalized.find("thumb") != std::string::npos;
		// 剣は専用ボーンへ追従させ、当たり判定もそこから作るため、
		// クリップ側の剣ボーンのキーは取り込まない。
		const bool isWeaponBone = normalized.find("sword") != std::string::npos ||
			normalized.find("weapon") != std::string::npos;
		// 脚は取り込まない。
		// 実際に画面をキャプチャして確認した結果、脚を取り込むと
		// キャラクターが座り込んだように脚が開いて崩れることが分かった。
		// 原因は「脚だけを動かして腰(hips)を固定している」ことにある。
		// Mixamoのクリップの脚の回転は、腰も一緒に回転・上下することを前提に
		// 作られている。ところがCAnimationMesh側は平行移動を常にレスト姿勢へ固定し、
		// さらに腰はゲーム側の向きを守るため除外している。
		// その結果、腰が沈まないまま脚だけが「沈んだ姿勢用の角度」を取るため、
		// 脚が横へ開いて座ったような姿勢になる。
		// 腰の平行移動まで正しく扱えるようになるまでは、脚は除外する。
		const bool isLeg = normalized.find("leg") != std::string::npos ||
			normalized.find("thigh") != std::string::npos ||
			normalized.find("knee") != std::string::npos ||
			normalized.find("calf") != std::string::npos ||
			normalized.find("foot") != std::string::npos ||
			normalized.find("ankle") != std::string::npos ||
			normalized.find("toe") != std::string::npos;
		// 攻撃モーションは体幹(背骨・首・頭)と腕へ適用する。
		// 以前は腕のチェーンだけに絞っていたが、それは当時のプレイヤーモデルが
		// MMD系リグで、Mixamo製クリップとの不一致により胴体キーで全身が折れたためである。
		// 現在のプレイヤーは同じSword and Shield Pack由来のMixamoリグ
		// (assets/model/SwordShieldPack/runtime/SwordShieldPack_Player.glb)なので、
		// 骨格が一致しており、背骨を取り込んでも破綻しない。
		// 腕だけでは体の捻りが出ず「棒立ちで腕を振る」動きになるため、体幹まで広げる。
		//
		// 除外するのは次の4種類:
		//  - ルートと腰 : ワールドの向きはゲーム側(SRT)が管理するため
		//  - 脚         : 上記の理由(腰を固定したまま脚だけ動かすと破綻する)
		//  - 指         : 剣の握りを崩さないため
		//  - 剣         : 専用の追従処理と当たり判定を壊さないため
		if (!isGameplayRoot && !isFinger && !isWeaponBone && !isLeg)
			m_importedAttackBones.push_back(boneName);
	}

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
	m_attackMotionFilename = m_motionFilename;

	LoadMotionFile(m_motionFilename);
	// 対象GLB自身の安定したバインドポーズ（Tポーズ）を基準にし、
	// sword_shield_idle_safe.motionの上腕回転だけを重ねる。
	// ダウンロードしたアイドルFBXを基準にすると、しゃがみ姿勢や単位系のルート移動が
	// 足の回転・沈み込みを起こし、上に重ねた攻撃モーションまで壊すためである。
	const bool idleLoaded = LoadIdlePose("assets/motion/sword_shield_idle_safe.motion");
	const bool seatedLoaded = LoadSeatedPoseFile("assets/motion/sword_shield_idle.motion");
	std::cout << "[Animator] attack bones=" << m_motionMappedBoneCount
		<< " idle bones=" << m_idlePose.size()
		<< " seated bones=" << m_seatedPose.size()
		<< " idle loaded=" << (idleLoaded ? "yes" : "no")
		<< " seated loaded=" << (seatedLoaded ? "yes" : "no") << std::endl;
}

void CCharacterAnimator::SetWalkAnimation(aiAnimation* animation)
{
	m_walkAnimation = animation;
	m_walkFrame = 0;
	m_walkFrameAccumulator = 0.0f;
}

void CCharacterAnimator::SetLocomotionAnimations(
	aiAnimation* walkAnimation,
	aiAnimation* runAnimation)
{
	m_walkAnimation = walkAnimation;
	m_runAnimation = runAnimation;
	m_walkFrame = 0;
	m_walkFrameAccumulator = 0.0f;
}

const std::vector<std::string>& CCharacterAnimator::LowerBodyBones() const
{
	// ボーン名の解決はInitialize時に済んでいるので、初回だけ組み立てて使い回す。
	if (m_lowerBodyBonesCache.empty())
	{
		// 腰(pelvis)は含めない。
		// 腰の沈み込みを平行移動で再現しようとしたが、待機クリップの脚は
		// 腰が大きく下がる前提で作られており、安全な範囲の沈み込みでは全く足りず、
		// 脚が宙で跳ね上がったような姿勢になった(実機キャプチャで確認)。
		// 腰を正しく動かすには、脚のIKで接地を保つ仕組みが要る。
		const std::string* candidates[] = {
			&m_leftLeg, &m_rightLeg, &m_leftKnee, &m_rightKnee,
			&m_leftFoot, &m_rightFoot,
		};
		for (const std::string* name : candidates)
		{
			if (!name->empty())
				m_lowerBodyBonesCache.push_back(*name);
		}
	}
	return m_lowerBodyBonesCache;
}

void CCharacterAnimator::SetIdleAnimation(aiAnimation* animation)
{
	m_idleAnimation = animation;
	m_idleFrame = 0;
	m_idleFrameAccumulator = 0.0f;
}

void CCharacterAnimator::SetAttackAnimations(
	const std::array<aiAnimation*, 3>& weakAnimations,
	const std::array<aiAnimation*, 3>& heavyAnimations)
{
	m_weakAttackAnimations = weakAnimations;
	m_heavyAttackAnimations = heavyAnimations;
}

void CCharacterAnimator::SetLocomotionBlendSpace(
	const CAnimationMesh& mesh,
	float modelScale,
	const std::array<aiAnimation*, 8>& clips,
	float walkSpeed,
	float runSpeed)
{
	m_blendSpaceWalkSpeed = walkSpeed;
	m_blendSpaceRunSpeed = runSpeed;

	// クリップの腰の位置キーは、腰の親(アーマチュア)の空間で書かれている。
	// 休止姿勢の腰の「親空間での高さ」と「モデル空間での高さ」の比が親の倍率になるので、
	// それとモデルの表示倍率を掛けて、クリップの単位をゲーム内の単位へ換算する。
	// モデルを差し替えても、この換算は自動で合う。
	//
	// 注意: 親空間の「上」の軸はモデルによって違う。このプレイヤーモデルは親(アーマチュア)が
	// Z軸を上にしており、腰の休止姿勢の位置は(0.03, 0.47, 95.6)と高さがZに入っている。
	// 一方クリップはY軸が上。Y成分を高さだと決め打ちすると換算が約200倍になり、
	// 攻撃の踏み込みでプレイヤーが闘技場の外まで飛んだ(実機で確認)。
	// 腰の休止位置で最も大きい成分を「高さ」とみなす(腰の高さは他の成分より十分大きい)。
	float clipToModel = 1.0f;
	const float restLocalHeight = CAnimationMesh::DominantAxisComponent(mesh.GetRestLocalMatrix(m_pelvis));
	const float restModelHeight = mesh.GetRestBoneModelHeight(m_pelvis);
	if (std::abs(restLocalHeight) > 0.0001f)
		clipToModel = restModelHeight / restLocalHeight;
	const float clipToWorld = std::abs(clipToModel * modelScale);
	m_clipToWorld = clipToWorld;

	for (size_t index = 0; index < clips.size(); ++index)
	{
		BlendSpaceClip data{};
		data.animation = clips[index];
		if (data.animation != nullptr)
		{
			const double ticksPerSecond = data.animation->mTicksPerSecond > 0.0
				? data.animation->mTicksPerSecond
				: 30.0;
			data.cycleSeconds = std::max(
				0.05f, static_cast<float>(data.animation->mDuration / ticksPerSecond));
			// 腰のチャンネルの先頭と末尾の位置差が、1周期で進む距離。
			for (unsigned int c = 0; c < data.animation->mNumChannels; ++c)
			{
				const aiNodeAnim* channel = data.animation->mChannels[c];
				if (channel == nullptr || channel->mNumPositionKeys < 2 ||
					m_pelvis != channel->mNodeName.C_Str())
					continue;
				const aiVector3D& first = channel->mPositionKeys[0].mValue;
				const aiVector3D& last = channel->mPositionKeys[channel->mNumPositionKeys - 1].mValue;
				const float dx = last.x - first.x;
				const float dz = last.z - first.z;
				data.cycleDistance = std::sqrt(dx * dx + dz * dz) * clipToWorld;
				break;
			}
		}
		m_blendSpaceClips[index] = data;
	}
	// 前向きの歩きが無いと代用先が無くなるので、それだけは必須にする。
	m_blendSpaceReady = m_blendSpaceClips[0].animation != nullptr;

	// 歩き・走りへ切り替える速さは、ゲーム側の歩き・ダッシュの速さ(引数)にする。
	// 歩いているときは歩きクリップだけ、ダッシュ中は走りクリップだけが再生される。
	// 引数が無効なときだけ、クリップを1倍速で再生したときの速さで代用する。
	//
	// 経緯: 移動速度が歩き70・ダッシュ約115と体格に対して速すぎた間は、
	// これを境目にすると歩きクリップを約4.5倍速で回すことになり、足がばたばたした。
	// そのため一時的にクリップ自身の速さを境目にしていたが、移動速度を体格に合う値
	// (歩き20・ダッシュ45)へ下げたので、ゲーム側の速さを境目に戻した。
	const auto naturalSpeedOf = [](const BlendSpaceClip& clip)
	{
		return clip.cycleSeconds > 0.0f ? clip.cycleDistance / clip.cycleSeconds : 0.0f;
	};
	if (walkSpeed <= 0.0f)
		m_blendSpaceWalkSpeed = std::max(0.1f, naturalSpeedOf(m_blendSpaceClips[0]));
	if (runSpeed <= m_blendSpaceWalkSpeed)
		m_blendSpaceRunSpeed = std::max(m_blendSpaceWalkSpeed + 0.1f, naturalSpeedOf(m_blendSpaceClips[1]));
}

const CCharacterAnimator::BlendSpaceClip& CCharacterAnimator::BlendSpaceClipOf(
	Anim::LocomotionDirection direction, Anim::LocomotionGait gait) const
{
	const auto indexOf = [](Anim::LocomotionDirection d, Anim::LocomotionGait g)
	{
		return static_cast<size_t>(d) * 2 + static_cast<size_t>(g);
	};
	const BlendSpaceClip& wanted = m_blendSpaceClips[indexOf(direction, gait)];
	if (wanted.animation != nullptr)
		return wanted;
	// 無い方向は、同じ歩調の前向きで代用する。それも無ければ前向きの歩き。
	const BlendSpaceClip& forward =
		m_blendSpaceClips[indexOf(Anim::LocomotionDirection::Forward, gait)];
	return forward.animation != nullptr ? forward : m_blendSpaceClips[0];
}

bool CCharacterAnimator::UpdateLocomotionBlendSpace(
	CAnimationMesh& mesh,
	BoneCombMatrix& boneComb,
	const CharacterAnimationState& state,
	float deltaSeconds,
	const std::unordered_map<std::string, Matrix4x4>* blendFromPose,
	float blendRate)
{
	if (!m_blendSpaceReady)
		return false;

	// 入力の速度へなめらかに追従する。キーを押した瞬間に重みが跳ぶと、
	// ブレンドツリーにしても歩き出しの一歩目で姿勢が飛んで見える。
	// 指数的に追従させるので、フレームレートが変わっても追従の速さは同じ。
	const float follow = 1.0f - std::exp(-deltaSeconds * 12.0f);
	m_smoothedVelocityRight += (state.velocityRight - m_smoothedVelocityRight) * follow;
	m_smoothedVelocityForward += (state.velocityForward - m_smoothedVelocityForward) * follow;
	const Anim::LocomotionWeights weights = Anim::ComputeLocomotionWeights(
		m_smoothedVelocityRight, m_smoothedVelocityForward,
		m_blendSpaceWalkSpeed, m_blendSpaceRunSpeed);

	// 待機は上半身だけ(脚は休止姿勢)。待機クリップの脚は腰が大きく沈む前提で作られていて、
	// 腰を固定したまま使うと宙に浮いた姿勢になるため(従来の待機と同じ)。
	const std::vector<std::string> upperBones = {
		m_spine, m_spine01, m_spine02,
		m_leftArm, m_rightArm, m_leftElbow, m_rightElbow,
	};
	std::vector<std::string> movingBones = upperBones;
	const std::vector<std::string>& lowerBones = LowerBodyBones();
	movingBones.insert(movingBones.end(), lowerBones.begin(), lowerBones.end());

	// --- 足運びの位相を進める ---
	// 全移動クリップで1つの正規化時間を共有する(同期)。クリップごとに別の時間で進めると、
	// 混ぜたときに右足と左足が同時に前へ出るような破綻が起きる。
	// 進める速さは「実際の速さ ÷ 1周期で進む距離」。これで足が地面を滑らない。
	const float movingWeight = 1.0f - weights.idle;
	if (movingWeight > 0.001f)
	{
		float cyclesPerSecond = 0.0f;
		for (int i = 0; i < weights.movingCount; ++i)
		{
			const auto& entry = weights.moving[i];
			const BlendSpaceClip& clip = BlendSpaceClipOf(entry.direction, entry.gait);
			const float naturalRate = 1.0f / clip.cycleSeconds;
			float rate = clip.cycleDistance > 0.01f
				? weights.speed / clip.cycleDistance
				: naturalRate;
			// 足が地面を滑らないよう、基本は実際の速さに合わせた再生速度をそのまま使う。
			// 歩き出しのごく遅い速度で足がほぼ止まるのと、異常値だけを防ぐため、元の0.5〜4倍に収める。
			// (以前は2.5倍で打ち切っていたため、ダッシュで足が滑っていた)
			rate = std::clamp(rate, naturalRate * 0.5f, naturalRate * 4.0f);
			cyclesPerSecond += (entry.weight / movingWeight) * rate;
		}
		m_blendSpacePhase += cyclesPerSecond * deltaSeconds;
		m_blendSpacePhase -= std::floor(m_blendSpacePhase);
	}

	// --- 姿勢を取り出して重み付きで混ぜる ---
	// 2つずつ順に補間する。k本目を混ぜるときの比率を
	// 「k本目の重み ÷ それまでの重みの合計」にすると、全体として重み付き平均になる。
	std::unordered_map<std::string, Matrix4x4> result;
	float accumulated = 0.0f;
	const auto blendIn = [&](const std::unordered_map<std::string, Matrix4x4>& pose, float weight)
	{
		if (weight <= 0.0001f)
			return;
		if (accumulated <= 0.0f)
		{
			result = pose;
			for (const std::string& bone : movingBones)
				result.try_emplace(bone, mesh.GetRestLocalMatrix(bone));
			accumulated = weight;
			return;
		}
		const float amount = weight / (accumulated + weight);
		for (const std::string& bone : movingBones)
		{
			const auto to = pose.find(bone);
			const Matrix4x4 target = to != pose.end() ? to->second : mesh.GetRestLocalMatrix(bone);
			result[bone] = CAnimationMesh::BlendLocalMatrix(result[bone], target, amount);
		}
		accumulated += weight;
	};

	// 待機は止まっている間も進め続ける。止めておくと、立ち止まるたびに同じ姿勢から始まる。
	m_idleFrameAccumulator += m_idlePlaybackRate * deltaSeconds * 60.0f;
	if (weights.idle > 0.0001f)
	{
		std::unordered_map<std::string, Matrix4x4> idlePose;
		if (m_idleAnimation != nullptr)
		{
			unsigned int keys = 0;
			for (unsigned int c = 0; c < m_idleAnimation->mNumChannels; ++c)
				keys = std::max(keys, m_idleAnimation->mChannels[c]->mNumRotationKeys);
			const float normalized = keys > 1
				? m_idleFrameAccumulator / static_cast<float>(keys - 1)
				: 0.0f;
			idlePose = mesh.SampleLocalPose(m_idleAnimation, normalized, upperBones);
		}
		blendIn(idlePose, weights.idle);
	}
	for (int i = 0; i < weights.movingCount; ++i)
	{
		const auto& entry = weights.moving[i];
		const BlendSpaceClip& clip = BlendSpaceClipOf(entry.direction, entry.gait);
		blendIn(mesh.SampleLocalPose(clip.animation, m_blendSpacePhase, movingBones), entry.weight);
	}

	// 攻撃の終わりから移動・待機へ戻るときのクロスフェード。
	if (blendFromPose != nullptr && !blendFromPose->empty() && blendRate < 1.0f)
	{
		for (auto& [bone, matrix] : result)
		{
			const auto from = blendFromPose->find(bone);
			if (from != blendFromPose->end())
				matrix = CAnimationMesh::BlendLocalMatrix(from->second, matrix, blendRate);
		}
	}

	mesh.ApplyLocalPose(boneComb, result, m_idlePose);
	return true;
}

void CCharacterAnimator::PlayImportedComboStep(
	aiAnimation* animation,
	const char* name,
	const Combat::PlayerComboStep& step)
{
	// 補間の開始や各種の状態の初期化は既存の関数に任せ、再生位置と速さだけを上書きする。
	PlayImportedAttackAnimation(
		animation,
		name,
		step.TotalSeconds(),
		step.WindupSeconds(),
		step.WindupSeconds() + step.ActiveSeconds());

	unsigned int keys = 0;
	for (unsigned int c = 0; c < animation->mNumChannels; ++c)
		keys = std::max(keys, animation->mChannels[c]->mNumRotationKeys);
	const double ticksPerSecond = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 30.0;
	const float clipSeconds = static_cast<float>(animation->mDuration / ticksPerSecond);
	if (keys < 2 || clipSeconds <= 0.0f)
		return;

	// キーはクリップの長さに等間隔で並んでいるとみなす(ApplyAnimationToBonesと同じ前提)。
	const float keysPerSecond = static_cast<float>(keys - 1) / clipSeconds;
	m_importedAnimationFrameAccumulator = step.clipStart * keysPerSecond;
	m_importedAnimationFrame = static_cast<int>(m_importedAnimationFrameAccumulator);
	// 1更新(60Hz換算)あたりに進むキー数。Update側で経過時間を掛けて進めるので、fpsに依存しない。
	m_importedAnimationFrameRate = keysPerSecond * step.playbackRate / 60.0f;
	// 踏み込みは再生を始めた位置から数える(飛ばした溜めの分の移動は足さない)。
	m_rootMotionPreviousTime = std::clamp(step.clipStart / clipSeconds, 0.0f, 0.9999f);
	m_attackRootMotionScale = step.rootMotionScale;
}

void CCharacterAnimator::PlayDashAttackMotion()
{
	// クリップが無ければ通常の弱攻撃1段目で代用する。
	if (m_dashAttackAnimation == nullptr)
	{
		PlayAttackMotion(1);
		return;
	}
	PlayImportedComboStep(
		m_dashAttackAnimation,
		"External dash attack / advancing leap slash",
		Combat::PlayerDashAttackStep());
}

bool CCharacterAnimator::ConsumeRootMotion(float& right, float& forward)
{
	right = m_pendingRootMotionRight;
	forward = m_pendingRootMotionForward;
	m_pendingRootMotionRight = 0.0f;
	m_pendingRootMotionForward = 0.0f;
	return right != 0.0f || forward != 0.0f;
}

void CCharacterAnimator::SetImpactAnimation(aiAnimation* animation)
{
	m_impactAnimation = animation;
}

void CCharacterAnimator::PlayImpactMotion()
{
	if (m_impactAnimation == nullptr)
		return;
	// 攻撃クリップと同じ上半身レイヤーの経路で再生する。
	// 予兆・判定の区間は使わないので0にする(剣の軌跡や攻撃判定には関わらない。
	// それらは戦闘システム側の攻撃状態で決まる)。
	PlayImportedAttackAnimation(m_impactAnimation, "Impact", 0.60f, 0.0f, 0.0f);
}

void CCharacterAnimator::PlayAttackMotion()
{
	PlayAttackMotion(1);
}

void CCharacterAnimator::PlayAttackMotion(int comboStep)
{
	const int profileIndex = std::clamp(comboStep, 1, 3) - 1;
	if (m_weakAttackAnimations[profileIndex] != nullptr)
	{
		static constexpr const char* names[] = {
			"External weak 1 / simple grounded slash",
			"External weak 2 / simple grounded slash",
			"External weak 3 / simple grounded slash",
		};
		// クリップ本来の長さを保ち、段ごとの再生位置・速さで再生する(以前は0.95秒へ詰めていた)。
		PlayImportedComboStep(
			m_weakAttackAnimations[profileIndex],
			names[profileIndex],
			Combat::PlayerComboStepOf(false, comboStep));
		return;
	}

	BeginAttackBlend();
	struct WeakProfile
	{
		const char* name;
		const char* clip;
		float yaw;
		float pitch;
		bool overhead;
	};
	static constexpr WeakProfile profiles[] = {
		{ "Weak 1 / diagonal lead", "assets/motion/sword_shield_slash.motion", -0.15f, 0.04f, false },
		{ "Weak 2 / reverse cut", "assets/motion/sword_shield_slash_3.motion", -0.30f, 0.02f, false },
		{ "Weak 3 / advancing finisher", "assets/motion/sword_shield_slash_4.motion", -0.10f, -0.08f, false },
	};
	const WeakProfile& profile = profiles[std::clamp(comboStep, 1, 3) - 1];
	if (LoadMotionFile(profile.clip))
	{
		NormalizeMotionTiming(0.95f);
		ApplyAttackMotionDesign(profile.name, 0.16f, 0.54f, profile.yaw, profile.pitch, profile.overhead);
	}
	else
	{
		BuildFallbackAttackComboMotion(comboStep);
		NormalizeMotionTiming(0.95f);
		ApplyAttackMotionDesign(profile.name, 0.16f, 0.54f, profile.yaw, profile.pitch, profile.overhead);
	}
	m_motionTime = 0.0f;
	m_motionLoop = false;
	m_motionPlaying = !m_motionKeys.empty();
	m_useCustomMotion = m_motionPlaying;
}

void CCharacterAnimator::Initialize(const CAnimationMesh& mesh, const CharacterModelProfile& profile)
{
	Initialize(mesh);
	const auto resolve = [this, &profile](CanonicalJoint joint) {
		return FindBone(m_boneNames,
			profile.canonicalAliases[static_cast<size_t>(joint)]);
	};
	m_pelvis = resolve(CanonicalJoint::Hips);
	m_spine = resolve(CanonicalJoint::Spine);
	m_spine02 = resolve(CanonicalJoint::Chest);
	m_leftArm = resolve(CanonicalJoint::LeftUpperArm);
	m_leftElbow = resolve(CanonicalJoint::LeftLowerArm);
	m_leftHand = resolve(CanonicalJoint::LeftHand);
	m_rightArm = resolve(CanonicalJoint::RightUpperArm);
	m_rightElbow = resolve(CanonicalJoint::RightLowerArm);
	m_rightHand = resolve(CanonicalJoint::RightHand);
	m_leftLeg = resolve(CanonicalJoint::LeftUpperLeg);
	m_leftKnee = resolve(CanonicalJoint::LeftLowerLeg);
	m_leftFoot = resolve(CanonicalJoint::LeftFoot);
	m_rightLeg = resolve(CanonicalJoint::RightUpperLeg);
	m_rightKnee = resolve(CanonicalJoint::RightLowerLeg);
	m_rightFoot = resolve(CanonicalJoint::RightFoot);
}

void CCharacterAnimator::PlayHeavyAttackMotion()
{
	PlayHeavyAttackMotion(1);
}

void CCharacterAnimator::PlayHeavyAttackMotion(int comboStep)
{
	const int profileIndex = std::clamp(comboStep, 1, 3) - 1;
	if (m_heavyAttackAnimations[profileIndex] != nullptr)
	{
		static constexpr const char* names[] = {
			"External heavy 1 / simple grounded heavy slash",
			"External heavy 2 / simple grounded heavy slash",
			"External heavy 3 / simple grounded heavy slash",
		};
		// クリップ本来の長さを保ち、段ごとの再生位置・速さで再生する(以前は1.10秒へ詰めていた)。
		PlayImportedComboStep(
			m_heavyAttackAnimations[profileIndex],
			names[profileIndex],
			Combat::PlayerComboStepOf(true, comboStep));
		return;
	}

	BeginAttackBlend();
	struct HeavyProfile
	{
		const char* name;
		const char* clip;
		float yaw;
		float pitch;
		bool overhead;
	};
	static constexpr HeavyProfile profiles[] = {
		{ "Heavy 1 / overhead chop", "assets/motion/sword_shield_attack.motion", -0.15f, 0.34f, true },
		{ "Heavy 2 / committed sweep", "assets/motion/sword_shield_attack_2.motion", -0.35f, 0.14f, false },
		{ "Heavy 3 / finishing cleave", "assets/motion/sword_shield_attack_3.motion", -0.15f, 0.42f, true },
	};
	const HeavyProfile& profile = profiles[std::clamp(comboStep, 1, 3) - 1];
	if (LoadMotionFile(profile.clip))
	{
		NormalizeMotionTiming(1.10f);
		ApplyAttackMotionDesign(profile.name, 0.20f, 0.62f, profile.yaw, profile.pitch, profile.overhead);
	}
	else
	{
		BuildFallbackHeavyComboMotion(comboStep);
		NormalizeMotionTiming(1.10f);
		ApplyAttackMotionDesign(profile.name, 0.20f, 0.62f, profile.yaw, profile.pitch, profile.overhead);
	}
	m_motionTime = 0.0f;
	m_motionLoop = false;
	m_motionPlaying = !m_motionKeys.empty();
	m_useCustomMotion = m_motionPlaying;
}

void CCharacterAnimator::StartComboPreview(bool heavy)
{
	m_comboPreviewActive = true;
	m_comboPreviewStep = 1;
	m_comboPreviewHeavy = heavy;
	if (m_comboPreviewHeavy)
		PlayHeavyAttackMotion(m_comboPreviewStep);
	else
		PlayAttackMotion(m_comboPreviewStep);
}

void CCharacterAnimator::TriggerHitStop(float seconds)
{
	m_hitStopSeconds = std::max(m_hitStopSeconds, std::clamp(seconds, 0.0f, 0.20f));
}

void CCharacterAnimator::BeginAttackBlend()
{
	m_attackBlendFromPose = m_lastRenderedPose;
	m_attackBlendTime = 0.0f;
	m_locomotionBlendActive = false;
	// PlayAttackMotion()の時点ではmeshを受け取れないため、実際の現在姿勢は
	// 次のUpdateで取得する。これにより歩行中の上半身姿勢を正確にfrom側へ使える。
	m_attackBlendPending = true;
}

void CCharacterAnimator::PlayImportedAttackAnimation(
	aiAnimation* animation,
	const char* name,
	float duration,
	float windupEnd,
	float activeEnd)
{
	BeginAttackBlend();
	m_importedAttackAnimation = animation;
	// 踏み込みはクリップの先頭から数える。前の攻撃の残りは持ち越さない。
	m_rootMotionPreviousTime = 0.0f;
	m_pendingRootMotionRight = 0.0f;
	m_pendingRootMotionForward = 0.0f;
	m_importedAnimationFrame = 0;
	m_importedAnimationFrameAccumulator = 0.0f;
	unsigned int maxRotationKeys = 0;
	for (unsigned int channel = 0; channel < animation->mNumChannels; ++channel)
		maxRotationKeys = std::max(
			maxRotationKeys,
			animation->mChannels[channel]->mNumRotationKeys);
	if (maxRotationKeys > 1 && duration > 0.0f)
	{
		m_importedAnimationFrameRate = static_cast<float>(maxRotationKeys - 1) /
			(duration * 60.0f);
	}
	else
	{
		m_importedAnimationFrameRate = 0.0f;
	}
	m_motionTime = 0.0f;
	m_motionDuration = duration;
	m_attackWindupEnd = windupEnd;
	m_attackActiveEnd = activeEnd;
	m_attackMotionName = name;
	m_attackMotionFilename = name;
	m_motionLoop = false;
	m_motionPlaying = true;
	// FBX攻撃経路でも、歩行姿勢から攻撃姿勢へ短くクロスフェードする。
	m_attackBlendDuration = 0.10f;
	// 読み込んだFBXは一致するMixamoボーンへ直接適用し、
	// 旧来の手作業による.motion姿勢レイヤーは通さない。
	m_useCustomMotion = false;
	m_importedAttackPose = false;
}

void CCharacterAnimator::PlayDodgeMotion()
{
	// 回避は読み込み攻撃をキャンセルできる。
	// 攻撃クリップを先に解除し、攻撃再生側の処理に回避モーションが隠されないようにする。
	m_importedAttackAnimation = nullptr;
	m_importedAnimationFrame = 0;
	m_importedAnimationFrameAccumulator = 0.0f;
	// 取り出されていない踏み込みを回避へ持ち越さない(回避の移動に上乗せされてしまう)。
	m_pendingRootMotionRight = 0.0f;
	m_pendingRootMotionForward = 0.0f;
	m_attackBlendPending = false;
	m_locomotionBlendActive = false;
	BuildFallbackDodgeMotion();
	// 次のUpdateで、回避を始めた瞬間の姿勢(腕の構え)を保存する。
	m_dodgeBasePosePending = true;
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
	{
		m_importedAttackPose = false;
		BuildFallbackAttackMotion();
	}
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
	CaptureUndoIfNeeded();
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

void CCharacterAnimator::ApplyEditorKey(const MotionKeyframe& key)
{
	m_editorKey = key;
	AddOrUpdateCurrentKey();
}

void CCharacterAnimator::PreviewEditorKey(const MotionKeyframe& key)
{
	m_editorKey = key;
	MotionKeyframe current = key;
	current.time = m_motionTime;
	BoneKeys& keys = m_motionKeys[m_selectedBone];
	bool replaced = false;
	for (auto& existing : keys)
	{
		if (std::abs(existing.time - current.time) < 0.001f)
		{
			existing = current;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		keys.push_back(current);
	SortKeys(keys);
	m_useCustomMotion = true;
}

void CCharacterAnimator::BeginEditTransaction()
{
	if (m_editTransactionActive)
		return;
	m_editTransactionActive = true;
	m_editTransactionCaptured = false;
	CaptureUndoIfNeeded();
}

void CCharacterAnimator::EndEditTransaction()
{
	m_editTransactionActive = false;
	m_editTransactionCaptured = false;
}

CCharacterAnimator::EditorSnapshot CCharacterAnimator::CaptureEditorSnapshot() const
{
	EditorSnapshot snapshot;
	snapshot.motionKeys = m_motionKeys;
	snapshot.editorKey = m_editorKey;
	snapshot.motionTime = m_motionTime;
	snapshot.motionDuration = m_motionDuration;
	return snapshot;
}

void CCharacterAnimator::RestoreEditorSnapshot(const EditorSnapshot& snapshot)
{
	m_motionKeys = snapshot.motionKeys;
	m_dodgeMotion = false;
	m_editorKey = snapshot.editorKey;
	m_motionTime = snapshot.motionTime;
	m_motionDuration = snapshot.motionDuration;
	m_useCustomMotion = !m_motionKeys.empty();
}

void CCharacterAnimator::CaptureUndoIfNeeded()
{
	if (m_editTransactionActive && m_editTransactionCaptured)
		return;
	if (m_undoHistory.size() >= 64)
		m_undoHistory.pop_front();
	m_undoHistory.push_back(CaptureEditorSnapshot());
	m_redoHistory.clear();
	m_editTransactionCaptured = true;
}

void CCharacterAnimator::UndoEditorChange()
{
	if (m_undoHistory.empty())
		return;
	m_redoHistory.push_back(CaptureEditorSnapshot());
	RestoreEditorSnapshot(m_undoHistory.back());
	m_undoHistory.pop_back();
}

void CCharacterAnimator::RedoEditorChange()
{
	if (m_redoHistory.empty())
		return;
	m_undoHistory.push_back(CaptureEditorSnapshot());
	RestoreEditorSnapshot(m_redoHistory.back());
	m_redoHistory.pop_back();
}

bool CCharacterAnimator::SelectKeyAtTime(float time)
{
	const auto keyIt = m_motionKeys.find(m_selectedBone);
	if (keyIt == m_motionKeys.end())
		return false;
	for (const auto& key : keyIt->second)
	{
		if (std::abs(key.time - time) < 0.035f)
		{
			m_motionTime = key.time;
			m_editorKey = key;
			return true;
		}
	}
	return false;
}

bool CCharacterAnimator::MoveSelectedKey(float fromTime, float toTime)
{
	auto keyIt = m_motionKeys.find(m_selectedBone);
	if (keyIt == m_motionKeys.end())
		return false;
	for (auto& key : keyIt->second)
	{
		if (std::abs(key.time - fromTime) < 0.035f)
		{
			CaptureUndoIfNeeded();
			key.time = std::clamp(toTime, 0.0f, m_motionDuration);
			m_motionTime = key.time;
			m_editorKey = key;
			SortKeys(keyIt->second);
			return true;
		}
	}
	return false;
}

bool CCharacterAnimator::DeleteKeyAtTime(float time)
{
	auto keyIt = m_motionKeys.find(m_selectedBone);
	if (keyIt == m_motionKeys.end())
		return false;
	const auto oldSize = keyIt->second.size();
	if (std::none_of(keyIt->second.begin(), keyIt->second.end(), [time](const MotionKeyframe& key) {
		return std::abs(key.time - time) < 0.035f;
	}))
		return false;
	CaptureUndoIfNeeded();
	keyIt->second.erase(std::remove_if(keyIt->second.begin(), keyIt->second.end(), [time](const MotionKeyframe& key) {
		return std::abs(key.time - time) < 0.035f;
	}), keyIt->second.end());
	return keyIt->second.size() != oldSize;
}

bool CCharacterAnimator::DuplicateKeyAtTime(float time)
{
	const auto keyIt = m_motionKeys.find(m_selectedBone);
	if (keyIt == m_motionKeys.end())
		return false;
	for (const auto& key : keyIt->second)
	{
		if (std::abs(key.time - time) < 0.035f)
		{
			CaptureUndoIfNeeded();
			MotionKeyframe copy = key;
			copy.time = std::clamp(key.time + 1.0f / 30.0f, 0.0f, m_motionDuration);
			m_motionKeys[m_selectedBone].push_back(copy);
			SortKeys(m_motionKeys[m_selectedBone]);
			m_motionTime = copy.time;
			m_editorKey = copy;
			return true;
		}
	}
	return false;
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
	const float deltaSeconds = std::clamp(state.deltaSeconds, 0.0f, 0.1f);
	const float frameScale = deltaSeconds * 60.0f;
	// モーション専用シーンではUIのPlayを使い、GameSceneでは左クリックで
	// 保存済みattack.motionを先頭から1回再生する。
	if (m_hitStopSeconds > 0.0f)
	{
		// 数回の固定更新で姿勢を完全に止め、剣がすり抜けて見えず
		// 接触の瞬間が分かるようにする。
		m_hitStopSeconds = std::max(0.0f, m_hitStopSeconds - deltaSeconds);
		if (m_useCustomMotion && !m_motionKeys.empty())
		{
			std::unordered_map<std::string, Matrix4x4> deltas;
			EvaluateCustomMotion(m_motionTime, deltas);
			const bool useIdleBase = !m_importedAttackPose;
			std::unordered_map<std::string, Matrix4x4> pose =
				useIdleBase ? m_idlePose : std::unordered_map<std::string, Matrix4x4>{};
			for (const auto& [boneName, delta] : deltas)
			{
				if (useIdleBase)
				{
					const auto idle = m_idlePose.find(boneName);
					pose[boneName] = idle != m_idlePose.end()
						? delta * idle->second
						: delta;
				}
				else
					pose[boneName] = delta;
			}
			// 読み込んだ攻撃クリップによって装備が意図せず復活しないようにする。
			// 剣だけで戦う設定ではアイドル姿勢で盾を隠しているため、攻撃姿勢にも同じ指定を引き継ぐ。
			if (m_importedAttackPose)
			{
				for (const auto& [boneName, hiddenPose] : m_idlePose)
				{
					if (NormalizeBoneName(boneName).find("shield") != std::string::npos)
						pose[boneName] = hiddenPose;
				}
			}
			mesh.UpdateManualPose(boneComb, pose);
		}
		return;
	}

	if (m_importedAttackAnimation != nullptr)
	{
		if (m_motionPlaying && m_motionTime < m_motionDuration)
		{
			if (m_attackBlendPending)
			{
				// 直前のUpdateで確定した歩行/走行のローカル姿勢を保存する。
				// 上半身のfrom姿勢だけを使い、下半身は今フレームの移動クリップを
				// そのまま適用することで、歩行の接地を崩さない。
				m_attackBlendFromPose = mesh.CaptureCurrentLocalPose();
				m_attackBlendPending = false;
				m_attackBlendTime = 0.0f;
			}

			// 外部クリップはプレイヤーと同じMixamoスケルトンを使うため、
			// 一致する身体チャンネルを適用する。ただし移動はゲーム側が管理するので
			// ルートの平行移動は取り込まない。
			// 累積値の小数部を渡してキー間を補間させる。
			// 攻撃クリップは1秒前後に30キー程度しか無いため、
			// 補間しないとキーの切り替わりがそのまま段差として見えてしまう。
			const float attackFrameFraction = m_importedAnimationFrameAccumulator -
				std::floor(m_importedAnimationFrameAccumulator);

			// 上半身に攻撃、下半身に移動または待機を重ねる(レイヤー合成)。
			//
			// 攻撃クリップの脚をそのまま使うと、腰(hips)を固定しているせいで
			// 脚が開いて座り込んだ姿勢になる(実機で確認済み)。
			// かといって脚を除外すると、今度は攻撃中ずっと棒立ちになる。
			// そこで脚は移動・待機クリップに任せる。こちらは腰の上下が小さく破綻しないため、
			// 脚が動き続けたまま上半身だけが攻撃になる。
			// 副産物として「歩きながら攻撃」も表現できる。
			// 下半身へ流すのは移動クリップだけにする。
			// 待機クリップの脚は腰が大きく沈む前提で作られていて、
			// 腰を固定したまま適用すると宙に浮いた姿勢になるため使わない。
			// 立ち止まって攻撃した場合は、脚は休止姿勢のまま(従来と同じ)。
			const bool useRunForAttack = state.running && m_runAnimation != nullptr;
			aiAnimation* lowerBodyAnimation = state.walking
				? (useRunForAttack ? m_runAnimation : m_walkAnimation)
				: nullptr;
			if (state.walking)
			{
				m_walkFrameAccumulator += (useRunForAttack
					? m_runPlaybackRate
					: m_walkPlaybackRate) * frameScale;
				m_walkFrame = static_cast<int>(m_walkFrameAccumulator);
			}
			const int lowerBodyFrame = m_walkFrame;
			const float lowerBodyFraction =
				m_walkFrameAccumulator - std::floor(m_walkFrameAccumulator);
			float attackBlendRate = 1.0f;
			if (m_attackBlendTime < m_attackBlendDuration)
			{
				m_attackBlendTime += deltaSeconds;
				const float linearBlend = std::clamp(
					m_attackBlendTime / std::max(m_attackBlendDuration, 0.001f),
					0.0f,
					1.0f);
				attackBlendRate = linearBlend * linearBlend * (3.0f - 2.0f * linearBlend);
			}

			// --- 踏み込みのある攻撃(ルートモーション) ---
			// 立ち止まって攻撃するときは、脚と腰の沈み込みも攻撃クリップから取り、
			// クリップの腰が前へ進んだ分だけキャラクターの位置を前へ出す。
			//
			// 以前は脚を攻撃クリップから外し、腰も固定していたため、
			// 攻撃中は棒立ちのまま腕を振るだけで、その場から一歩も動かなかった。
			// 脚を入れたときに脚が開いて座り込んだのは、腰を固定したまま
			// 「腰が沈む前提の脚の角度」だけを使っていたためである。
			// ここでは腰の沈み込み(割合で換算・制限なし)と回転の変化を取り込み、
			// 前後左右の移動はキャラクターの位置へ移す。移動を腰にも残すと二重に進んで足が滑る。
			const bool useRootMotion = lowerBodyAnimation == nullptr &&
				!m_pelvis.empty() && m_clipToWorld > 0.0f;
			if (useRootMotion)
			{
				unsigned int attackKeys = 0;
				for (unsigned int c = 0; c < m_importedAttackAnimation->mNumChannels; ++c)
					attackKeys = std::max(attackKeys, m_importedAttackAnimation->mChannels[c]->mNumRotationKeys);
				// 1.0ちょうどを渡すと、SampleLocalPose側の巻き戻しで先頭へ戻ってしまうので手前で止める。
				const float attackTime = attackKeys > 1
					? std::clamp(m_importedAnimationFrameAccumulator / static_cast<float>(attackKeys - 1),
						0.0f, 0.9999f)
					: 0.0f;

				std::vector<std::string> attackBones;
				attackBones.reserve(m_importedAttackBones.size() + 8);
				for (const std::string& bone : m_importedAttackBones)
				{
					if (bone != m_pelvis)
						attackBones.push_back(bone);
				}
				for (const std::string& bone : LowerBodyBones())
				{
					if (std::find(attackBones.begin(), attackBones.end(), bone) == attackBones.end())
						attackBones.push_back(bone);
				}

				std::unordered_map<std::string, Matrix4x4> attackPose =
					mesh.SampleLocalPose(m_importedAttackAnimation, attackTime, attackBones);
				attackPose[m_pelvis] = mesh.SampleHipsInPlace(
					m_importedAttackAnimation, attackTime, m_pelvis, m_attackHipsRotationMode);

				// 攻撃の出だしは直前の姿勢(歩き・待機)から短く補間する。
				if (attackBlendRate < 1.0f && !m_attackBlendFromPose.empty())
				{
					for (auto& [bone, matrix] : attackPose)
					{
						const auto from = m_attackBlendFromPose.find(bone);
						if (from != m_attackBlendFromPose.end())
							matrix = CAnimationMesh::BlendLocalMatrix(from->second, matrix, attackBlendRate);
					}
				}
				static const std::unordered_map<std::string, Matrix4x4> noManualPose;
				mesh.ApplyLocalPose(boneComb, attackPose, noManualPose);

				// 前回からの腰の水平移動を、キャラクターから見た右・前へ直して溜める。
				// Mixamoのクリップは元は+Zが前・右が-Xだが、読み込み時の aiProcess_ConvertToLeftHanded で
				// Zが反転するので、ゲーム内では**-Zが前**、右は-Xのまま。
				// 以前は+Zを前として足していたため、踏み込む攻撃がすべて後ろへ下がっていた
				// (ダッシュ攻撃で「走っている向きと攻撃の向きが違う」と指摘され、位置のログで発覚)。
				// 調査プログラムで腰の移動を測るときも、同じ読み込みフラグでないと前後が逆に見えるので注意。
				const Vector3 previous = mesh.SampleBonePositionOffset(
					m_importedAttackAnimation, m_rootMotionPreviousTime, m_pelvis);
				const Vector3 current = mesh.SampleBonePositionOffset(
					m_importedAttackAnimation, attackTime, m_pelvis);
				m_pendingRootMotionForward +=
					-(current.z - previous.z) * m_clipToWorld * m_attackRootMotionScale;
				m_pendingRootMotionRight +=
					-(current.x - previous.x) * m_clipToWorld * m_attackRootMotionScale;
				m_rootMotionPreviousTime = attackTime;
			}
			else if (lowerBodyAnimation != nullptr)
			{
				mesh.UpdateLayeredAnimation(
					boneComb,
					lowerBodyAnimation, lowerBodyFrame, lowerBodyFraction,
					LowerBodyBones(), true,
					m_importedAttackAnimation, m_importedAnimationFrame,
					attackFrameFraction, m_importedAttackBones, false,
					m_idlePose,
					std::string(),
					&m_attackBlendFromPose,
					attackBlendRate);
			}
			else
			{
				// 下半身へ流すクリップが無い場合は、従来どおり上半身だけを適用する。
				static const std::unordered_map<std::string, Matrix4x4> noManualPose;
				mesh.UpdateAnimationWithManualPose(
					boneComb,
					m_importedAttackAnimation,
					m_importedAnimationFrame,
					noManualPose,
					m_importedAttackBones,
					false,
					attackFrameFraction,
					&m_attackBlendFromPose,
					attackBlendRate);
			}
			m_motionTime += deltaSeconds;
			m_importedAnimationFrameAccumulator +=
				m_importedAnimationFrameRate * frameScale;
			m_importedAnimationFrame = static_cast<int>(m_importedAnimationFrameAccumulator);
			return;
		}

		m_importedAttackAnimation = nullptr;
		m_importedAnimationFrame = 0;
		m_importedAnimationFrameAccumulator = 0.0f;
		m_attackBlendPending = false;
		// 攻撃の最後の姿勢も保存し、次の待機/歩行へ戻るときに同じように
		// クロスフェードする。開始時だけ滑らかでも、終了時にスナップすれば
		// 一連の動きとしては不自然になるためである。
		m_locomotionBlendFromPose = mesh.CaptureCurrentLocalPose();
		m_locomotionBlendTime = 0.0f;
		m_locomotionBlendActive = true;
		m_motionPlaying = false;
	}

	if (m_motionPlaying)
	{
		// GameSceneから渡された実時間で進める。固定1/60だけに依存すると、
		// 可変フレーム時に攻撃の再生速度と判定タイミングがずれる。
		m_motionTime += deltaSeconds;
		if (m_motionTime > m_motionDuration)
		{
			if (m_motionLoop)
				m_motionTime = std::fmod(m_motionTime, std::max(m_motionDuration, 0.001f));
			else
			{
				m_motionTime = m_motionDuration;
				m_motionPlaying = false;
				if (m_dodgeMotion)
				{
					// 前転の最後の姿勢から待機・移動へ補間して戻す(戻さないと起き上がりの瞬間に姿勢が跳ぶ)。
					m_locomotionBlendFromPose = mesh.CaptureCurrentLocalPose();
					m_locomotionBlendTime = 0.0f;
					m_locomotionBlendActive = true;
				}
			}
		}
	}

	if (m_comboPreviewActive && !IsMotionPlaying())
	{
		if (m_comboPreviewStep < 3)
		{
			++m_comboPreviewStep;
			if (m_comboPreviewHeavy)
				PlayHeavyAttackMotion(m_comboPreviewStep);
			else
				PlayAttackMotion(m_comboPreviewStep);
			return;
		}
		m_comboPreviewActive = false;
	}

	float locomotionBlendRate = 1.0f;
	const std::unordered_map<std::string, Matrix4x4>* locomotionBlendFromPose = nullptr;
	if (m_locomotionBlendActive)
	{
		m_locomotionBlendTime += deltaSeconds;
		const float linearBlend = std::clamp(
			m_locomotionBlendTime / std::max(m_locomotionBlendDuration, 0.001f),
			0.0f,
			1.0f);
		locomotionBlendRate = linearBlend * linearBlend * (3.0f - 2.0f * linearBlend);
		locomotionBlendFromPose = &m_locomotionBlendFromPose;
		if (linearBlend >= 1.0f)
			m_locomotionBlendActive = false;
	}

	// 再生停止中でもタイムラインの現在位置の姿勢をエディターへ表示する。
	// GameSceneではワンショット再生中だけこの姿勢を適用する。
	if (m_useCustomMotion && (m_motionPlaying || m_editorEnabled) && !m_motionKeys.empty())
	{
		std::unordered_map<std::string, Matrix4x4> deltas;
		EvaluateCustomMotion(m_motionTime, deltas);
		const bool useIdleBase = !m_importedAttackPose;
		std::unordered_map<std::string, Matrix4x4> customPose =
			useIdleBase ? m_idlePose : std::unordered_map<std::string, Matrix4x4>{};
		for (const auto& [boneName, delta] : deltas)
		{
			if (useIdleBase)
			{
				const auto idle = m_idlePose.find(boneName);
				customPose[boneName] = idle != m_idlePose.end()
					? delta * idle->second
					: delta;
			}
		else
			customPose[boneName] = delta;
		}
		if (m_importedAttackPose)
		{
			for (const auto& [boneName, hiddenPose] : m_idlePose)
			{
				if (NormalizeBoneName(boneName).find("shield") != std::string::npos)
					customPose[boneName] = hiddenPose;
			}
		}
		if (m_dodgeMotion && m_motionPlaying)
		{
			if (m_dodgeBasePosePending)
			{
				// 回避を始めた瞬間の姿勢。前転の出だしはこの姿勢から補間する。
				// UpdateManualPose()は「休止姿勢からの差」を受け取る(差 * 休止姿勢)ので、
				// 保存したローカル行列(差 * 休止姿勢そのもの)から休止姿勢を外して差へ直す。
				// そのまま渡すと休止姿勢が二重に掛かる。
				m_dodgeBasePose.clear();
				for (const auto& [boneName, local] : mesh.CaptureCurrentLocalPose())
					m_dodgeBasePose[boneName] = local * mesh.GetRestLocalMatrix(boneName).Invert();
				m_attackBlendFromPose = m_dodgeBasePose;
				m_attackBlendTime = 0.0f;
				m_dodgeBasePosePending = false;
			}
			// 肩から先は構えたまま転がる(モンスターハンターの前転も武器を構えたまま転がる)。
			// 前転のキーの腕の値は休止姿勢(Tポーズ)からの差なので、使うと両腕を横へ広げた形になっていた。
			for (const auto& [boneName, basePose] : m_dodgeBasePose)
			{
				const std::string normalized = NormalizeBoneName(boneName);
				if (normalized.find("shoulder") != std::string::npos ||
					normalized.find("arm") != std::string::npos ||
					normalized.find("hand") != std::string::npos)
				{
					customPose[boneName] = basePose;
				}
			}
		}
		if (m_motionPlaying && m_attackBlendTime < m_attackBlendDuration)
		{
			m_attackBlendTime += deltaSeconds;
			const float linearBlend = std::clamp(
				m_attackBlendTime / std::max(m_attackBlendDuration, 0.001f),
				0.0f,
				1.0f);
			const float blend = linearBlend * linearBlend * (3.0f - 2.0f * linearBlend);
			for (auto& [boneName, targetPose] : customPose)
			{
				const auto from = m_attackBlendFromPose.find(boneName);
				if (from != m_attackBlendFromPose.end())
					targetPose = BlendLocalPose(from->second, targetPose, blend);
			}
		}
		m_lastRenderedPose = customPose;
		mesh.UpdateManualPose(boneComb, customPose);
		return;
	}

	// 移動のブレンドツリー。待機・歩き・走りと、ロックオン中の横歩き・後ろ歩きを
	// 速度に応じて混ぜる。設定されていなければ下の従来経路(歩き/走りの切り替え)を使う。
	if (!state.jumping &&
		UpdateLocomotionBlendSpace(
			mesh, boneComb, state, deltaSeconds, locomotionBlendFromPose, locomotionBlendRate))
	{
		return;
	}

	const bool useRunAnimation = state.running && m_runAnimation != nullptr;
	aiAnimation* locomotionAnimation = useRunAnimation ? m_runAnimation : m_walkAnimation;
	if (state.walking && locomotionAnimation != nullptr)
	{
		// ダウンロードした移動クリップはプレイヤーと同じMixamoスケルトンを使う。
		// 脚に加えて背骨と腕も読み込むが、Hips・手・指は除外する。
		// Hipsには元データのルート移動が含まれ、手はプレイヤー側の剣の握りを維持するためである。
		const std::vector<std::string> walkUpperBones = {
			m_spine, m_spine01, m_spine02,
			m_leftArm, m_rightArm, m_leftElbow, m_rightElbow,
		};
		m_walkFrameAccumulator += (useRunAnimation
			? m_runPlaybackRate
			: m_walkPlaybackRate) * frameScale;
		m_walkFrame = static_cast<int>(m_walkFrameAccumulator);
		// 歩き・走りも1キーずつ切り替えるとカクつくため、キー間を補間する。
		const float walkFrameFraction =
			m_walkFrameAccumulator - std::floor(m_walkFrameAccumulator);
		// 歩行・走行は1本のクリップで上下とも動かす(従来どおり)。
		// 腰の平行移動は取り込まない。歩行クリップは腰の上下が小さく、
		// 取り込まなくても破綻しないことが分かっているため。
		std::vector<std::string> walkBones = walkUpperBones;
		const std::vector<std::string>& walkLowerBones = LowerBodyBones();
		walkBones.insert(
			walkBones.end(), walkLowerBones.begin(), walkLowerBones.end());
		mesh.UpdateAnimationWithManualPose(
			boneComb, locomotionAnimation, m_walkFrame, m_idlePose, walkBones,
			true, walkFrameFraction, locomotionBlendFromPose, locomotionBlendRate);
		return;
	}
	if (!state.walking)
	{
		m_walkFrame = 0;
		m_walkFrameAccumulator = 0.0f;
	}

	// 待機モーション。
	// 以前は静止姿勢へ背骨のわずかな呼吸を加えるだけだったため、
	// 腕を下げた棒立ちに見えていた。専用クリップを再生して、
	// 構えと重心の揺れがある待機にする。
	// ジャンプ中は下の手続き的な姿勢(腕を上げる等)を使うので対象外。
	if (!state.walking && !state.jumping && m_idleAnimation != nullptr)
	{
		// 待機でも脚を待機クリップで動かす。
		// 攻撃中のレイヤー合成では下半身を待機・移動クリップが担当するため、
		// 待機時に脚を止めていると、攻撃を出した瞬間に脚だけ別の姿勢へ飛んでしまう。
		// 前後で同じクリップが脚を動かしている状態にして、継ぎ目をなくす。
		const std::vector<std::string> idleBones = {
			m_spine, m_spine01, m_spine02,
			m_leftArm, m_rightArm, m_leftElbow, m_rightElbow,
		};
		m_idleFrameAccumulator += m_idlePlaybackRate * frameScale;
		m_idleFrame = static_cast<int>(m_idleFrameAccumulator);
		const float idleFrameFraction =
			m_idleFrameAccumulator - std::floor(m_idleFrameAccumulator);
		// 待機中は脚を動かさず、上半身だけへ待機クリップを適用する。
		// 待機クリップの脚は腰が大きく沈む前提で作られており、
		// 腰を固定したまま脚だけ適用すると宙に浮いて足をばたつかせたように見える。
		// 立ち止まっている間は攻撃中も脚を動かさないため、
		// 攻撃に入る瞬間に脚が飛ぶこともない。
		mesh.UpdateAnimationWithManualPose(
			boneComb, m_idleAnimation, m_idleFrame, m_idlePose, idleBones,
			true, idleFrameFraction, locomotionBlendFromPose, locomotionBlendRate);
		return;
	}

	const float phase = std::sinf(state.motionTime * 7.0f);
	// パラディンは重い鎧のキャラクターなので、腰の大きな振りと
	// 膝・足首のねじりを抑え、ウェイト付きのマントや装甲が裂けて見えないようにする。
	// 足同士が分かれる範囲は残し、歩幅は読み取りやすくする。
	const float armSwing = state.walking ? phase * 0.30f : 0.0f;
	const float legSwing = state.walking ? -phase * 0.38f : 0.0f;
	const float leftKneeSwing = state.walking ? std::max(0.0f, phase) * 0.24f : 0.0f;
	const float rightKneeSwing = state.walking ? std::max(0.0f, -phase) * 0.24f : 0.0f;
	const float footSwing = state.walking ? phase * 0.20f : (state.jumping ? -0.2f : 0.0f);
	const float armRaise = state.jumping ? 0.75f : 0.0f;
	// アイドル姿勢は控えめにする。バインドポーズの下半身が安定しているため、
	// 背骨に小さな呼吸だけを加え、脚や足を回すキーは使わない。
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
	m_lastRenderedPose = pose;
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

	// タイトルモデルでは左手を自由な手として契約書へ伸ばし、右腕を鞘へ下げる。
	// 抜刀を始めたら肘を引き、契約書を胸の前ではなく脇へ下げる。
	// 受け渡しを読みやすくしつつ、戦闘へ向かう動きに見せる。
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

	// まず剣を持つ手を腰まで下げ、契約書を受け取った後に
	// 抜刀して構える姿勢へ反転させる。
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

	// 下半身には付属の歩きクリップを使う。
	// オプションのクリップがない場合だけ、従来の控えめな歩幅を代替として使う。
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
			// FBXのHips回転は取り込まない。
			// このクリップは異なる正面軸で作られているため、SwordShieldPackへ適用すると
			// タイトルキャラクター全体が上下反転してしまう。
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
	m_importedAttackPose = false;
	m_motionKeys.clear();
	m_dodgeMotion = false;
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
	// この代替姿勢は、基準モーションをリターゲットした攻撃姿勢と同期させる。
	// 編集可能なモーションファイルがない、または解析できない場合にも使う。
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

void CCharacterAnimator::NormalizeMotionTiming(float targetDuration)
{
	const float sourceDuration = std::max(m_motionDuration, 0.001f);
	const float timeScale = targetDuration / sourceDuration;
	for (auto& [boneName, keys] : m_motionKeys)
	{
		for (auto& key : keys)
		{
			key.time *= timeScale;
			// ダウンロードクリップにはFBX側の単位でルート移動が含まれている。
			// ゲーム側ですでにプレイヤーを移動しているため、そのまま使うと二重移動や
			// モデルの沈み込みが発生する。
			key.position = Vector3(0.0f, 0.0f, 0.0f);
		}
	}
	m_motionDuration = targetDuration;
}

void CCharacterAnimator::StripAttackLowerBody()
{
	for (auto it = m_motionKeys.begin(); it != m_motionKeys.end();)
	{
		// 攻撃クリップは武器を持つ腕のチェーンだけを動かす。
		// 首・頭・腰・脚・顔・出力用補助ボーンは対象モデルの初期姿勢に残し、
		// リグの不一致でコンボ遷移中に全身が折れ曲がるのを防ぐ。
		const bool mappedDriverBone =
			it->first == m_spine || it->first == m_spine01 || it->first == m_spine02 ||
			it->first == m_leftArm || it->first == m_rightArm ||
			it->first == m_leftElbow || it->first == m_rightElbow ||
			it->first == m_leftHand || it->first == m_rightHand;
		// 指、肩の補助ボーン、独立した剣ボーンは標準の腕チェーンから姿勢を継承する。
		// 出力形式固有の子ボーンを個別に動かすと、手首のねじれ、武器の浮き、
		// 全身の崩れが起こりやすいためである。
		const bool driverBone = mappedDriverBone;
		if (!driverBone)
			it = m_motionKeys.erase(it);
		else
			++it;
	}
}

void CCharacterAnimator::ApplyAttackMotionDesign(
	const char* name,
	float windupEnd,
	float activeEnd,
	float torsoYaw,
	float torsoPitch,
	bool overhead)
{
	const float yawDelta = std::abs(torsoYaw - m_lastAttackTorsoYaw);
	const float pitchDelta = std::abs(torsoPitch - m_lastAttackTorsoPitch);
	const float poseDelta = std::clamp(yawDelta + pitchDelta, 0.0f, 0.75f);
	m_attackBlendDuration = std::clamp(0.05f + (poseDelta / 0.75f) * 0.10f, 0.05f, 0.15f);

	// 読み込んだクリップは手・前腕・肩の軌道を細かく持つ。
	// ここではFBX単体にないゲーム用の時間設計として、振りかぶり、接触時の加速、
	// 振り切り、構えへの戻りを加える。脚は足IKが整うまで接地を優先して除外する。
	StripAttackLowerBody();
	m_attackMotionName = name ? name : "Attack";
	m_attackWindupEnd = std::clamp(windupEnd, 0.0f, m_motionDuration);
	m_attackActiveEnd = std::clamp(activeEnd, m_attackWindupEnd, m_motionDuration);

	const float duration = std::max(m_motionDuration, 0.001f);
	if (!m_pelvis.empty() && m_motionKeys.find(m_pelvis) == m_motionKeys.end())
	{
		const auto makeNeutralKey = [](float time) {
			MotionKeyframe key{};
			key.time = time;
			key.rotation = Vector3(0.0f, 0.0f, 0.0f);
			key.position = Vector3(0.0f, 0.0f, 0.0f);
			key.scale = Vector3(1.0f, 1.0f, 1.0f);
			return key;
		};
		BoneKeys pelvisKeys;
		const auto spineIt = m_motionKeys.find(m_spine);
		if (!m_spine.empty() && spineIt != m_motionKeys.end() && !spineIt->second.empty())
		{
			pelvisKeys.reserve(spineIt->second.size());
			for (const auto& spineKey : spineIt->second)
				pelvisKeys.push_back(makeNeutralKey(spineKey.time));
		}
		else
		{
			pelvisKeys = {
				makeNeutralKey(0.0f),
				makeNeutralKey(duration * 0.3f),
				makeNeutralKey(duration * 0.6f),
				makeNeutralKey(duration),
			};
		}
		m_motionKeys.emplace(m_pelvis, pelvisKeys);
	}
	const float windup = std::clamp(m_attackWindupEnd / duration, 0.02f, 0.90f);
	const float active = std::clamp(m_attackActiveEnd / duration, windup + 0.03f, 0.98f);
	const auto smooth = [](float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	};
	const auto pulse = [&smooth](float time, float start, float peak, float end) {
		if (time <= start || time >= end)
			return 0.0f;
		if (time < peak)
			return smooth((time - start) / std::max(peak - start, 0.001f));
		return 1.0f - smooth((time - peak) / std::max(end - peak, 0.001f));
	};

	for (auto& [boneName, keys] : m_motionKeys)
	{
		for (auto& key : keys)
		{
			const float time = std::clamp(key.time / duration, 0.0f, 1.0f);
			const float anticipation = pulse(time, 0.0f, windup * 0.62f, windup);
			const float strike = pulse(
				time,
				windup * 0.72f,
				std::min(active - 0.10f, windup + 0.20f),
				std::min(active + 0.04f, 0.98f));
			const float followThrough = pulse(
				time,
				std::max(active - 0.02f, 0.0f),
				std::min(active + 0.08f, 0.96f),
				0.98f);
			const float turn = torsoYaw *
				(-0.55f * anticipation + 0.85f * strike + 0.28f * followThrough);
			const float pitch = torsoPitch *
				(0.85f * anticipation - 0.90f * strike + 0.22f * followThrough);

			if (boneName == m_spine)
			{
				key.rotation.x += pitch;
				key.rotation.y += turn;
				key.rotation.z += torsoYaw * 0.08f * anticipation;
			}
			else if (boneName == m_spine01)
			{
				key.rotation.x += pitch * 0.82f;
				key.rotation.y += turn * 0.92f;
			}
			else if (boneName == m_spine02)
			{
				key.rotation.x += pitch * 0.58f;
				key.rotation.y += turn * 0.72f;
			}
			else if (boneName == m_pelvis)
			{
				key.rotation.x += pitch * 0.20f;
			}
			else if (boneName == m_rightArm)
			{
				key.rotation.x += (overhead ? -0.16f : -0.07f) * anticipation;
				key.rotation.y += turn * 0.18f;
			}
			else if (boneName == m_rightElbow)
			{
				key.rotation.x += (overhead ? -0.20f : -0.10f) * anticipation;
				key.rotation.x += (overhead ? 0.10f : 0.06f) * followThrough;
			}
			else if (boneName == m_leftArm)
			{
				// 盾側の腕を逆方向へ少し回し、剣腕だけが浮かず胸部と一体に見えるようにする。
				key.rotation.y -= turn * 0.24f;
				key.rotation.x += pitch * 0.20f;
			}
			else if (boneName == m_leftElbow)
			{
				key.rotation.x -= pitch * 0.16f;
			}
		}
	}

	// 数値として正しいクリップでも、対象モデルの関節軸に対して大きすぎる場合がある。
	// 作成者のタイミングは維持しつつ、ローカル回転の範囲を制限して
	// 一つの軸の誤リターゲットで胴体全体が折れないようにする。
	const auto clampRotation = [](Vector3& rotation,
		float xLimit, float yLimit, float zLimit)
	{
		rotation.x = std::clamp(rotation.x, -xLimit, xLimit);
		rotation.y = std::clamp(rotation.y, -yLimit, yLimit);
		rotation.z = std::clamp(rotation.z, -zLimit, zLimit);
	};
	for (auto& [boneName, keys] : m_motionKeys)
	{
		for (auto& key : keys)
		{
			if (boneName == m_spine)
				clampRotation(key.rotation, 0.42f, 0.42f, 0.28f);
			else if (boneName == m_spine01)
				clampRotation(key.rotation, 0.34f, 0.34f, 0.24f);
			else if (boneName == m_spine02)
				clampRotation(key.rotation, 0.28f, 0.28f, 0.20f);
			else if (boneName == m_pelvis)
				clampRotation(key.rotation, 0.12f, 0.12f, 0.12f);
			else if (boneName == m_leftArm || boneName == m_rightArm)
				clampRotation(key.rotation, 1.35f, 1.20f, 1.25f);
			else if (boneName == m_leftElbow || boneName == m_rightElbow)
				clampRotation(key.rotation, 1.25f, 0.90f, 0.90f);
			else if (boneName == m_leftHand || boneName == m_rightHand)
				clampRotation(key.rotation, 0.45f, 0.45f, 0.45f);
		}
	}

	m_lastAttackTorsoYaw = torsoYaw;
	m_lastAttackTorsoPitch = torsoPitch;
}

void CCharacterAnimator::BuildFallbackHeavyAttackMotion()
{
	// 安定したリターゲット姿勢を基準にし、同じラジアン単位のボーン空間で
	// 強攻撃専用の振り下ろし姿勢を作る。
	BuildFallbackAttackMotion();
	m_motionDuration = 1.10f;
	for (auto& [boneName, keys] : m_motionKeys)
	{
		for (auto& key : keys)
			key.time *= (m_motionDuration / 0.95f);
	}
	const auto setRotations = [this](const std::string& boneName, const std::vector<Vector3>& rotations)
	{
		auto it = m_motionKeys.find(boneName);
		if (it == m_motionKeys.end()) return;
		for (size_t i = 0; i < rotations.size() && i < it->second.size(); ++i)
			it->second[i].rotation = rotations[i];
	};
	setRotations(m_spine, {
		{}, {-0.10f, -0.12f, 0.02f}, {-0.30f, -0.30f, 0.05f},
		{0.28f, 0.30f, -0.04f}, {0.18f, 0.18f, -0.02f}, {0.04f, 0.04f, 0.0f}, {}
	});
	setRotations(m_spine01, {
		{}, {-0.12f, -0.16f, 0.03f}, {-0.38f, -0.38f, 0.06f},
		{0.34f, 0.38f, -0.05f}, {0.22f, 0.22f, -0.03f}, {0.05f, 0.05f, 0.0f}, {}
	});
	setRotations(m_rightArm, {
		{}, {-0.70f, 0.25f, -0.55f}, {-1.20f, 0.35f, -0.95f},
		{1.15f, -0.65f, 0.95f}, {0.82f, -0.50f, 0.70f}, {0.20f, -0.15f, 0.18f}, {}
	});
	setRotations(m_rightElbow, {
		{}, {-0.55f, 0.0f, 0.0f}, {-1.15f, 0.0f, 0.0f},
		{0.35f, 0.0f, 0.0f}, {0.42f, 0.0f, 0.0f}, {0.12f, 0.0f, 0.0f}, {}
	});
	setRotations(m_leftArm, {
		{}, {0.25f, 0.0f, 0.25f}, {0.42f, 0.0f, 0.42f},
		{-0.32f, 0.0f, -0.28f}, {-0.22f, 0.0f, -0.20f}, {-0.07f, 0.0f, -0.06f}, {}
	});
	m_motionTime = 0.0f;
	m_motionPlaying = false;
	m_useCustomMotion = true;
	m_motionFileLoaded = false;
	m_motionMappedBoneCount = static_cast<int>(m_motionKeys.size());
}

void CCharacterAnimator::BuildFallbackAttackComboMotion(int comboStep)
{
	BuildFallbackAttackMotion();
	if (comboStep <= 1)
		return;

	const auto setRotations = [this](const std::string& boneName, const std::vector<Vector3>& rotations)
	{
		auto it = m_motionKeys.find(boneName);
		if (it == m_motionKeys.end()) return;
		for (size_t i = 0; i < rotations.size() && i < it->second.size(); ++i)
			it->second[i].rotation = rotations[i];
	};
	if (comboStep == 2)
	{
		// 2段目は逆方向の横薙ぎにする。
		// 骨盤と脚を反対へ回し、腕だけでなく体ごと向きを変える攻撃に見せる。
		setRotations(m_pelvis, { {}, {0.0f, -0.12f, 0.12f}, {0.0f, -0.28f, 0.24f}, {0.0f, 0.30f, -0.20f}, {0.0f, 0.18f, -0.10f}, {0.0f, 0.04f, 0.0f}, {} });
		setRotations(m_spine, { {}, {-0.05f, -0.16f, 0.10f}, {-0.12f, -0.32f, 0.18f}, {0.18f, 0.34f, -0.16f}, {0.10f, 0.18f, -0.08f}, {0.02f, 0.04f, 0.0f}, {} });
		setRotations(m_rightArm, { {}, {-0.42f, 0.20f, -0.36f}, {-0.85f, 0.35f, -0.70f}, {0.98f, -0.72f, 0.86f}, {0.68f, -0.52f, 0.62f}, {0.16f, -0.12f, 0.14f}, {} });
		setRotations(m_rightElbow, { {}, {-0.35f, 0.0f, 0.0f}, {-0.85f, 0.0f, 0.0f}, {0.30f, 0.0f, 0.0f}, {0.38f, 0.0f, 0.0f}, {0.10f, 0.0f, 0.0f}, {} });
		setRotations(m_leftArm, { {}, {0.24f, 0.0f, 0.24f}, {0.46f, 0.0f, 0.45f}, {-0.38f, 0.0f, -0.34f}, {-0.24f, 0.0f, -0.20f}, {-0.07f, 0.0f, -0.06f}, {} });
		setRotations(m_leftLeg, { {}, {0.12f, 0.0f, 0.10f}, {0.22f, 0.0f, 0.16f}, {-0.22f, 0.0f, -0.10f}, {-0.12f, 0.0f, -0.05f}, {-0.03f, 0.0f, 0.0f}, {} });
		setRotations(m_rightLeg, { {}, {-0.12f, 0.0f, -0.10f}, {-0.22f, 0.0f, -0.16f}, {0.22f, 0.0f, 0.10f}, {0.12f, 0.0f, 0.05f}, {0.03f, 0.0f, 0.0f}, {} });
	}
	else
	{
		// 3段目は深くしゃがみながら体重を前へ移す振り下ろしの締め技にする。
		// 2回の横薙ぎと明確に違う動きにする。
		m_motionDuration = 1.05f;
		for (auto& [boneName, keys] : m_motionKeys)
			for (auto& key : keys)
				key.time *= (m_motionDuration / 0.95f);
		setRotations(m_pelvis, { {}, {0.12f, 0.08f, 0.0f}, {0.30f, 0.12f, 0.0f}, {-0.24f, -0.10f, 0.0f}, {-0.14f, -0.05f, 0.0f}, {-0.03f, 0.0f, 0.0f}, {} });
		setRotations(m_spine, { {}, {0.10f, 0.08f, 0.0f}, {0.30f, 0.14f, 0.0f}, {-0.34f, -0.12f, 0.0f}, {-0.20f, -0.06f, 0.0f}, {-0.04f, 0.0f, 0.0f}, {} });
		setRotations(m_spine01, { {}, {0.14f, 0.10f, 0.0f}, {0.38f, 0.18f, 0.0f}, {-0.40f, -0.16f, 0.0f}, {-0.24f, -0.08f, 0.0f}, {-0.05f, 0.0f, 0.0f}, {} });
		setRotations(m_rightArm, { {}, {-1.05f, 0.20f, -0.65f}, {-1.65f, 0.25f, -1.00f}, {1.25f, -0.55f, 0.90f}, {0.84f, -0.38f, 0.62f}, {0.20f, -0.10f, 0.14f}, {} });
		setRotations(m_rightElbow, { {}, {-0.70f, 0.0f, 0.0f}, {-1.35f, 0.0f, 0.0f}, {0.42f, 0.0f, 0.0f}, {0.48f, 0.0f, 0.0f}, {0.14f, 0.0f, 0.0f}, {} });
		setRotations(m_leftArm, { {}, {0.38f, 0.0f, 0.28f}, {0.62f, 0.0f, 0.48f}, {-0.46f, 0.0f, -0.30f}, {-0.28f, 0.0f, -0.18f}, {-0.08f, 0.0f, -0.04f}, {} });
	}
	m_motionTime = 0.0f;
	m_motionPlaying = false;
	m_useCustomMotion = true;
	m_motionFileLoaded = false;
	m_motionMappedBoneCount = static_cast<int>(m_motionKeys.size());
}

void CCharacterAnimator::BuildFallbackHeavyComboMotion(int comboStep)
{
	BuildFallbackHeavyAttackMotion();
	if (comboStep <= 1)
		return;
	const auto setRotations = [this](const std::string& boneName, const std::vector<Vector3>& rotations)
	{
		auto it = m_motionKeys.find(boneName);
		if (it == m_motionKeys.end()) return;
		for (size_t i = 0; i < rotations.size() && i < it->second.size(); ++i)
			it->second[i].rotation = rotations[i];
	};
	if (comboStep == 2)
	{
		setRotations(m_pelvis, { {}, {0.10f, -0.10f, 0.10f}, {0.25f, -0.20f, 0.22f}, {-0.20f, 0.20f, -0.18f}, {-0.12f, 0.10f, -0.08f}, {-0.03f, 0.02f, 0.0f}, {} });
		setRotations(m_spine, { {}, {-0.16f, -0.18f, 0.10f}, {-0.42f, -0.32f, 0.20f}, {0.36f, 0.36f, -0.18f}, {0.22f, 0.20f, -0.08f}, {0.04f, 0.04f, 0.0f}, {} });
		setRotations(m_rightArm, { {}, {-0.85f, 0.28f, -0.60f}, {-1.45f, 0.40f, -1.00f}, {1.35f, -0.75f, 1.05f}, {0.95f, -0.55f, 0.75f}, {0.22f, -0.12f, 0.18f}, {} });
		setRotations(m_rightElbow, { {}, {-0.62f, 0.0f, 0.0f}, {-1.35f, 0.0f, 0.0f}, {0.42f, 0.0f, 0.0f}, {0.52f, 0.0f, 0.0f}, {0.16f, 0.0f, 0.0f}, {} });
	}
	else
	{
		m_motionDuration = 1.25f;
		for (auto& [boneName, keys] : m_motionKeys)
			for (auto& key : keys)
				key.time *= (m_motionDuration / 1.10f);
		setRotations(m_pelvis, { {}, {0.20f, 0.0f, 0.0f}, {0.42f, 0.0f, 0.0f}, {-0.30f, 0.0f, 0.0f}, {-0.18f, 0.0f, 0.0f}, {-0.04f, 0.0f, 0.0f}, {} });
		setRotations(m_spine, { {}, {0.18f, 0.0f, 0.0f}, {0.52f, 0.0f, 0.0f}, {-0.46f, 0.0f, 0.0f}, {-0.26f, 0.0f, 0.0f}, {-0.05f, 0.0f, 0.0f}, {} });
		setRotations(m_rightArm, { {}, {-1.10f, 0.20f, -0.70f}, {-1.90f, 0.25f, -1.15f}, {1.55f, -0.65f, 1.10f}, {1.00f, -0.45f, 0.78f}, {0.24f, -0.10f, 0.18f}, {} });
		setRotations(m_rightElbow, { {}, {-0.85f, 0.0f, 0.0f}, {-1.60f, 0.0f, 0.0f}, {0.48f, 0.0f, 0.0f}, {0.58f, 0.0f, 0.0f}, {0.18f, 0.0f, 0.0f}, {} });
	}
	m_motionTime = 0.0f;
	m_motionPlaying = false;
	m_useCustomMotion = true;
	m_motionFileLoaded = false;
	m_motionMappedBoneCount = static_cast<int>(m_motionKeys.size());
}

void CCharacterAnimator::BuildFallbackDodgeMotion()
{
	m_importedAttackPose = false;
	m_motionKeys.clear();
	m_dodgeMotion = true;
	// 前転の長さはプレイヤーの移動と同じ定数を使う(モーションだけ先に終わると、滑りながら立ち上がって見える)。
	m_motionDuration = Combat::Tuning::PLAYER_DODGE_SECONDS;
	const auto addKeys = [this](const std::string& boneName, const std::vector<Vector3>& rotations) {
		if (boneName.empty()) return;
		BoneKeys& keys = m_motionKeys[boneName];
		// キーの位置は元の0.4秒の回避で作った割合のまま、長さに合わせて伸ばす。
		// 中間角度の差を意図的にPIより大きくし、Quaternion::Slerpが立ち姿勢への最短経路ではなく
		// 前転の一回転全体を補間するようにする。
		const float times[] = { 0.00f, 0.05f, 0.12f, 0.20f, 0.29f, 0.36f, 0.40f };
		const float timeScale = m_motionDuration / 0.40f;
		for (size_t i = 0; i < rotations.size() && i < 7; ++i)
		{
			MotionKeyframe key;
			key.time = times[i] * timeScale;
			key.rotation = rotations[i];
			keys.push_back(key);
		}
	};
	const auto addPositions = [this](const std::string& boneName, const std::vector<Vector3>& positions) {
		auto it = m_motionKeys.find(boneName);
		if (it == m_motionKeys.end()) return;
		for (size_t i = 0; i < positions.size() && i < it->second.size(); ++i)
			it->second[i].position = positions[i];
	};

	// 腰を前方向へ一回転させる。このモデルのローカル軸ではX負方向が前転なので、
	// 以前の正方向カーブでは後転に見えていた。-2PIまで連続した曲線にして
	// Slerpが前転一回転を追従するようにする。
	addKeys(m_pelvis, {
		{}, { -0.45f, 0.0f, 0.0f }, { -1.25f, 0.0f, 0.0f },
		{ -3.10f, 0.0f, 0.0f }, { -4.80f, 0.0f, 0.0f },
		{ -6.00f, 0.0f, 0.0f }, { -6.2831853f, 0.0f, 0.0f }
	});
	// 前転中に腰を上下へ移動させない。
	// 以前のY負方向移動はルートの高さを変えて浮き沈みを起こしていたため、
	// しゃがみは脚の回転だけで作る。
	addPositions(m_pelvis, { {}, {}, {}, {}, {}, {}, {} });

	// 逆さになったときも鎧の胴体がまとまって見えるよう、腰に対して背骨を少し丸める。
	// 一つの関節だけがゴムのように曲がらないよう、背骨のチェーンへ分散する。
	addKeys(m_spine, {
		{}, { -0.12f, 0.0f, 0.0f }, { -0.28f, 0.0f, 0.0f },
		{ -0.42f, 0.0f, 0.0f }, { -0.30f, 0.0f, 0.0f },
		{ -0.12f, 0.0f, 0.0f }, {}
	});
	addKeys(m_spine01, {
		{}, { -0.18f, 0.0f, 0.0f }, { -0.38f, 0.0f, 0.0f },
		{ -0.56f, 0.0f, 0.0f }, { -0.40f, 0.0f, 0.0f },
		{ -0.16f, 0.0f, 0.0f }, {}
	});
	addKeys(m_spine02, {
		{}, { -0.22f, 0.0f, 0.0f }, { -0.46f, 0.0f, 0.0f },
		{ -0.68f, 0.0f, 0.0f }, { -0.48f, 0.0f, 0.0f },
		{ -0.20f, 0.0f, 0.0f }, {}
	});

	// 両腿を引き込み、膝を折り、着地に向けて徐々に伸ばす。
	// 左右対称の値を使い、足が別々に振られないようにする。
	addKeys(m_leftLeg, {
		{}, { -0.72f, 0.0f, -0.05f }, { -1.10f, 0.0f, -0.08f },
		{ -0.48f, 0.0f, -0.05f }, { 0.38f, 0.0f, 0.0f },
		{ 0.24f, 0.0f, 0.0f }, {}
	});
	addKeys(m_rightLeg, {
		{}, { -0.72f, 0.0f, 0.05f }, { -1.10f, 0.0f, 0.08f },
		{ -0.48f, 0.0f, 0.05f }, { 0.38f, 0.0f, 0.0f },
		{ 0.24f, 0.0f, 0.0f }, {}
	});
	addKeys(m_leftKnee, {
		{}, { 1.30f, 0.0f, 0.0f }, { 2.20f, 0.0f, 0.0f },
		{ 2.65f, 0.0f, 0.0f }, { 2.10f, 0.0f, 0.0f },
		{ 0.92f, 0.0f, 0.0f }, {}
	});
	addKeys(m_rightKnee, {
		{}, { 1.30f, 0.0f, 0.0f }, { 2.20f, 0.0f, 0.0f },
		{ 2.65f, 0.0f, 0.0f }, { 2.10f, 0.0f, 0.0f },
		{ 0.92f, 0.0f, 0.0f }, {}
	});
	addKeys(m_leftFoot, {
		{}, { -0.32f, 0.0f, 0.0f }, { -0.68f, 0.0f, 0.0f },
		{ -0.82f, 0.0f, 0.0f }, { -0.52f, 0.0f, 0.0f },
		{ -0.22f, 0.0f, 0.0f }, {}
	});
	addKeys(m_rightFoot, {
		{}, { -0.32f, 0.0f, 0.0f }, { -0.68f, 0.0f, 0.0f },
		{ -0.82f, 0.0f, 0.0f }, { -0.52f, 0.0f, 0.0f },
		{ -0.22f, 0.0f, 0.0f }, {}
	});

	// 肘を胴体へ寄せる。手は意図的に触らず、前転中も剣と盾の接続姿勢を安定させる。
	addKeys(m_leftArm, {
		{}, { -0.30f, 0.0f, 0.24f }, { -0.64f, 0.0f, 0.42f },
		{ -0.78f, 0.0f, 0.50f }, { -0.56f, 0.0f, 0.34f },
		{ -0.22f, 0.0f, 0.14f }, {}
	});
	addKeys(m_rightArm, {
		{}, { -0.30f, 0.0f, -0.24f }, { -0.64f, 0.0f, -0.42f },
		{ -0.78f, 0.0f, -0.50f }, { -0.56f, 0.0f, -0.34f },
		{ -0.22f, 0.0f, -0.14f }, {}
	});
	addKeys(m_leftElbow, {
		{}, { 0.68f, 0.0f, 0.0f }, { 1.22f, 0.0f, 0.0f },
		{ 1.48f, 0.0f, 0.0f }, { 1.18f, 0.0f, 0.0f },
		{ 0.58f, 0.0f, 0.0f }, {}
	});
	addKeys(m_rightElbow, {
		{}, { 0.68f, 0.0f, 0.0f }, { 1.22f, 0.0f, 0.0f },
		{ 1.48f, 0.0f, 0.0f }, { 1.18f, 0.0f, 0.0f },
		{ 0.58f, 0.0f, 0.0f }, {}
	});
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
		const Matrix4x4 localPose = Matrix4x4::CreateScale(pose.scale) * rotation *
			Matrix4x4::CreateTranslation(pose.position.x, pose.position.y, pose.position.z);
		if (m_importedAttackPose)
		{
					// ダウンロードクリップは独自のバインド・構え姿勢を基準に作られている。
					// 絶対回転を対象モデルの初期姿勢へ直接適用すると、1フレーム目が
					// Tポーズへ跳ねるため、最初のキーを基準にして差分だけを
					// 現在の戦闘構えへ重ねる。
			const Matrix4x4 sourceStart = MotionKeyToMatrix(keys.front());
			const Matrix4x4 sourceDelta = localPose * sourceStart.Invert();
			const auto guard = m_idlePose.find(boneName);
			rotations[boneName] = sourceDelta *
				(guard != m_idlePose.end() ? guard->second : Matrix4x4::Identity);
		}
		else
		{
			rotations[boneName] = localPose;
		}
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
	const bool isImportedSwordAttack =
		filename.find("sword_shield_") != std::string::npos &&
		filename.find("_safe") == std::string::npos &&
		filename.find("idle") == std::string::npos;
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
	{
		SortKeys(keys);
		for (auto& key : keys)
		{
			const auto finite = [](float value) { return std::isfinite(value); };
			if (!finite(key.time) ||
				!finite(key.rotation.x) || !finite(key.rotation.y) || !finite(key.rotation.z) ||
				!finite(key.position.x) || !finite(key.position.y) || !finite(key.position.z) ||
				!finite(key.scale.x) || !finite(key.scale.y) || !finite(key.scale.z))
			{
				key.time = 0.0f;
				key.rotation = Vector3(0.0f, 0.0f, 0.0f);
				key.position = Vector3(0.0f, 0.0f, 0.0f);
				key.scale = Vector3(1.0f, 1.0f, 1.0f);
			}
			if (isImportedSwordAttack)
			{
				const std::string normalized = NormalizeBoneName(name);
				// 元パックはバインド姿勢が異なるため、腕の大きな軌道は活かしつつ
				// 胴体の回転範囲を制限し、誤ったキーで全身が180度折れないようにする。
				if (normalized.find("spine") != std::string::npos)
				{
					key.rotation.x = std::clamp(key.rotation.x, -1.20f, 1.20f);
					key.rotation.y = std::clamp(key.rotation.y, -1.20f, 1.20f);
					key.rotation.z = std::clamp(key.rotation.z, -0.90f, 0.90f);
				}
				else if (normalized.find("shoulder") != std::string::npos)
				{
					key.rotation.x = std::clamp(key.rotation.x, -1.60f, 1.60f);
					key.rotation.y = std::clamp(key.rotation.y, -1.60f, 1.60f);
					key.rotation.z = std::clamp(key.rotation.z, -1.60f, 1.60f);
				}
				key.scale = Vector3(1.0f, 1.0f, 1.0f);
				key.position = Vector3(0.0f, 0.0f, 0.0f);
			}
		}
	}
	if (isImportedSwordAttack)
	{
		// ダウンロードクリップは下半身のバインド姿勢が異なる。
		// 太腿・脛・足首の回転を対象GLBへ直接適用すると脚が折れて揺れるため、
		// 攻撃の体重移動だけを残し、接地した下半身は対象モデル側で管理する。
		for (auto it = loadedKeys.begin(); it != loadedKeys.end();)
		{
			const std::string normalized = NormalizeBoneName(it->first);
			const bool lowerBody =
				normalized.find("hips") != std::string::npos ||
				normalized.find("pelvis") != std::string::npos ||
				normalized == "root" ||
				normalized.find("upleg") != std::string::npos ||
				normalized.find("thigh") != std::string::npos ||
				normalized.find("leftleg") != std::string::npos ||
				normalized.find("rightleg") != std::string::npos ||
				normalized.find("lowerleg") != std::string::npos ||
				normalized.find("calf") != std::string::npos ||
				normalized.find("foot") != std::string::npos ||
				normalized.find("ankle") != std::string::npos ||
				normalized.find("toe") != std::string::npos;
			if (lowerBody)
				it = loadedKeys.erase(it);
			else
				++it;
		}
	}
	m_motionKeys = std::move(loadedKeys);
	m_dodgeMotion = false;
	m_importedAttackPose = isImportedSwordAttack;
	if (isImportedSwordAttack)
		StripAttackLowerBody();
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
	{
		m_importedAttackPose = false;
		BuildFallbackAttackMotion();
	}
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
				// FBXアイドルには読み込み元単位の絶対・ルート移動（例：Hips Y=-15）が含まれる。
				// 対象GLBは正しい初期位置を持っているため、これをコピーすると
				// 倍率変更後のキャラクターが地面へ沈み、リターゲットが元データの単位系に
				// 依存してしまう。ローカル回転だけを使い、実頂点の境界から接地させる。
				key.position = Vector3(0.0f, 0.0f, 0.0f);
				key.scale = Vector3(1.0f, 1.0f, 1.0f);
				// 以前のアイドルデータはShield_jointのスケールを0にして盾を隠していた。
				// このGLBでは同じ関節が兜の接続にも使われるため、関節は等倍のままにし、
				// 盾のメッシュ部分だけを非表示にする。
				if (NormalizeBoneName(currentBone).find("shield") != std::string::npos)
					key.scale = Vector3(1.0f, 1.0f, 1.0f);
				m_idlePose[currentBone] = MotionKeyToMatrix(key);
				captured[currentBone] = true;
			}
		}
		else if (token == "endbone")
			currentBone.clear();
	}
	// ダウンロードしたアイドルでは右手が開いたTポーズになる。
	// 剣はすでにSword_jointへスキニングされているため、握りを見せるには指だけを
	// ローカルX軸で曲げればよい。手のひらと手首の動きを壊さず、
	// m_idlePoseを攻撃の基準にする全クリップへこの握りを引き継ぐ。
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
	// このMixamo手ボーンのローカル軸ではX負方向が握り込み方向になる。
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
	if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::GetIO().KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Z))
			UndoEditorChange();
		if (ImGui::IsKeyPressed(ImGuiKey_Y))
			RedoEditorChange();
	}
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

	const float timelineWidth = std::max(ImGui::GetContentRegionAvail().x, 300.0f);
	const float timelineHeight = 92.0f;
	const ImVec2 timelinePos = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("MotionTimeline", ImVec2(timelineWidth, timelineHeight));
	const bool timelineHovered = ImGui::IsItemHovered();
	ImDrawList* timelineDraw = ImGui::GetWindowDrawList();
	const float timelineLeft = timelinePos.x + 8.0f;
	const float timelineRight = timelinePos.x + timelineWidth - 8.0f;
	const float timelineTop = timelinePos.y + 8.0f;
	const float timelineBottom = timelinePos.y + timelineHeight - 8.0f;
	const float timelineSpan = std::max(m_motionDuration, 0.05f);
	const auto timelineX = [timelineLeft, timelineRight, timelineSpan](float time) {
		return timelineLeft + std::clamp(time / timelineSpan, 0.0f, 1.0f) * (timelineRight - timelineLeft);
	};
	timelineDraw->AddRectFilled(ImVec2(timelineLeft, timelineTop), ImVec2(timelineRight, timelineBottom), IM_COL32(25, 30, 38, 255), 4.0f);
	for (int frame = 0; frame <= static_cast<int>(std::ceil(timelineSpan * 30.0f)); frame += 5)
	{
		const float time = static_cast<float>(frame) / 30.0f;
		if (time > timelineSpan)
			break;
		const float x = timelineX(time);
		timelineDraw->AddLine(ImVec2(x, timelineTop + 20.0f), ImVec2(x, timelineBottom), IM_COL32(75, 82, 95, 180), 1.0f);
		timelineDraw->AddText(ImVec2(x + 2.0f, timelineTop + 2.0f), IM_COL32(170, 180, 195, 220), std::to_string(frame).c_str());
	}
	const auto selectedKeysIt = m_motionKeys.find(m_selectedBone);
	if (selectedKeysIt != m_motionKeys.end())
	{
		for (const auto& key : selectedKeysIt->second)
		{
			const float x = timelineX(key.time);
			const ImU32 color = std::abs(key.time - m_motionTime) < 0.02f
				? IM_COL32(255, 215, 70, 255) : IM_COL32(75, 205, 240, 255);
			timelineDraw->AddCircleFilled(ImVec2(x, timelineTop + 58.0f), 5.0f, color, 8);
		}
	}
	const float currentX = timelineX(m_motionTime);
	timelineDraw->AddLine(ImVec2(currentX, timelineTop), ImVec2(currentX, timelineBottom), IM_COL32(255, 100, 80, 255), 2.0f);

	const auto timelineTimeFromMouse = [&]() {
		return std::clamp((ImGui::GetIO().MousePos.x - timelineLeft) / (timelineRight - timelineLeft) * timelineSpan, 0.0f, timelineSpan);
	};
	if (timelineHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const float clickedTime = timelineTimeFromMouse();
		if (!SelectKeyAtTime(clickedTime))
			m_motionTime = clickedTime;
		else
		{
			m_timelineDragging = true;
			m_timelineDragFrom = m_motionTime;
			BeginEditTransaction();
		}
	}
	if (timelineHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		DeleteKeyAtTime(timelineTimeFromMouse());
	if (m_timelineDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const float newTime = timelineTimeFromMouse();
		if (MoveSelectedKey(m_timelineDragFrom, newTime))
			m_timelineDragFrom = newTime;
	}
	if (m_timelineDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		m_timelineDragging = false;
		EndEditTransaction();
	}
	ImGui::TextDisabled("Timeline: click to scrub / drag key / right-click key to delete");

	if (ImGui::Button(m_motionPlaying ? "Pause" : "Play"))
		m_motionPlaying = !m_motionPlaying;
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		m_motionPlaying = false;
		m_motionTime = 0.0f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Undo") && CanUndoEditorChange())
		UndoEditorChange();
	ImGui::SameLine();
	if (ImGui::Button("Redo") && CanRedoEditorChange())
		RedoEditorChange();
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

	MotionKeyframe editedKey = m_editorKey;
	Vector3 degrees(
		editedKey.rotation.x * 180.0f / PI,
		editedKey.rotation.y * 180.0f / PI,
		editedKey.rotation.z * 180.0f / PI);
	if (ImGui::InputFloat3("Rotation (degrees)", &degrees.x))
	{
		editedKey.rotation = Vector3(
			degrees.x * PI / 180.0f,
			degrees.y * PI / 180.0f,
			degrees.z * PI / 180.0f);
		BeginEditTransaction();
		PreviewEditorKey(editedKey);
	}
	if (ImGui::IsItemActivated())
		BeginEditTransaction();
	if (ImGui::IsItemDeactivatedAfterEdit())
		EndEditTransaction();
	if (ImGui::InputFloat3("Position", &editedKey.position.x))
	{
		BeginEditTransaction();
		PreviewEditorKey(editedKey);
	}
	if (ImGui::IsItemActivated())
		BeginEditTransaction();
	if (ImGui::IsItemDeactivatedAfterEdit())
		EndEditTransaction();
	if (ImGui::InputFloat3("Scale", &editedKey.scale.x))
	{
		editedKey.scale.x = std::max(editedKey.scale.x, 0.01f);
		editedKey.scale.y = std::max(editedKey.scale.y, 0.01f);
		editedKey.scale.z = std::max(editedKey.scale.z, 0.01f);
		BeginEditTransaction();
		PreviewEditorKey(editedKey);
	}
	if (ImGui::IsItemActivated())
		BeginEditTransaction();
	if (ImGui::IsItemDeactivatedAfterEdit())
		EndEditTransaction();

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
		DeleteKeyAtTime(m_motionTime);
	}
	ImGui::SameLine();
	if (ImGui::Button("Duplicate Key"))
		DuplicateKeyAtTime(m_motionTime);
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
