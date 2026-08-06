#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "CAnimationMesh.h"

struct CharacterAnimationState
{
	bool walking = false;
	bool jumping = false;
	float motionTime = 0.0f;
};

struct MotionKeyframe
{
	float time = 0.0f;
	Vector3 rotation{};
	Vector3 position{};
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

// モデルとシーンの間に置く、再利用可能な簡易キャラクターアニメータ。
// 状態だけを受け取り、ボーン名や歩行ポーズの詳細はここに閉じ込める。
class CCharacterAnimator
{
public:
	void Initialize(const CAnimationMesh& mesh);
	void EnableMotionEditor();
	bool LoadMotionFile(const std::string& filename);
	void PlayAttackMotion();
	bool IsMotionPlaying() const { return m_motionPlaying; }
	const std::vector<std::string>& GetBoneNames() const { return m_boneNames; }
	const std::string& GetSelectedBone() const { return m_selectedBone; }
	void SelectBone(const std::string& boneName);
	void AdjustSelectedRotation(const Vector3& delta);
	void AdjustSelectedPosition(const Vector3& delta);
	void AdjustSelectedScale(const Vector3& delta);
	void AddOrUpdateCurrentKey();
	void Update(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		const CharacterAnimationState& state);

private:
	using BoneKeys = std::vector<MotionKeyframe>;

	void RenderMotionEditor();
	void EvaluateCustomMotion(
		float time,
		std::unordered_map<std::string, Matrix4x4>& rotations) const;
	bool SaveMotion(const std::string& filename) const;
	bool LoadMotion(const std::string& filename);
	MotionKeyframe GetEditorKey() const;
	void SortKeys(BoneKeys& keys);
	void BuildFallbackAttackMotion();

	std::string m_leftArm;
	std::string m_rightArm;
	std::string m_leftElbow;
	std::string m_rightElbow;
	std::string m_leftHand;
	std::string m_rightHand;
	std::string m_leftLeg;
	std::string m_rightLeg;
	std::string m_leftKnee;
	std::string m_rightKnee;
	std::string m_leftFoot;
	std::string m_rightFoot;

	std::vector<std::string> m_boneNames;
	std::unordered_map<std::string, BoneKeys> m_motionKeys;
	std::string m_motionFilename = "assets/motion/attack.motion";
	std::string m_selectedBone;
	MotionKeyframe m_editorKey{};
	float m_motionDuration = 1.0f;
	float m_motionTime = 0.0f;
	bool m_useCustomMotion = false;
	bool m_motionPlaying = false;
	bool m_motionLoop = true;
	bool m_editorInitialized = false;
	bool m_editorEnabled = false;
	bool m_motionFileLoaded = false;
	int m_motionMappedBoneCount = 0;
};
