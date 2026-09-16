#pragma once

#include <array>
#include <string>
#include <deque>
#include <unordered_map>
#include <vector>

#include "CAnimationMesh.h"
#include "LocomotionBlendSpace.h"

namespace Combat { struct PlayerComboStep; }

struct CharacterAnimationState
{
	bool walking = false;
	bool running = false;
	bool jumping = false;
	float motionTime = 0.0f;
	// GameSceneの固定更新間隔。省略時は既存の60Hz挙動を維持する。
	float deltaSeconds = 1.0f / 60.0f;
	// キャラクターから見た移動速度(単位/秒)。移動のブレンドツリーが使う。
	// 右が正、前が正。ロックオン中の横歩き・後ろ歩きの判定に使うため、向きとは別に持つ。
	// 呼び出し側が並び順で初期化しているため、新しい項目は必ず末尾へ足すこと。
	float velocityRight = 0.0f;
	float velocityForward = 0.0f;
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
	void Initialize(const CAnimationMesh& mesh, const CharacterModelProfile& profile);
	void SetWalkAnimation(aiAnimation* animation);
	void SetLocomotionAnimations(aiAnimation* walkAnimation, aiAnimation* runAnimation);
	// 待機モーション。設定しない場合は従来どおり、静止姿勢へ
	// 背骨のわずかな呼吸だけを手続き的に加えた見た目になる。
	void SetIdleAnimation(aiAnimation* animation);
	// 移動のブレンドツリー用のクリップ。前後左右 × 歩き/走り の8本。
	// 無いクリップ(nullptr)は同じ歩調の前向きクリップで代用する。
	// 足が地面を滑らないよう、クリップの腰の移動量から「1倍速で再生したときの移動速度」を求め、
	// ゲーム内の実際の速さに合わせて再生速度を決める。そのためメッシュとモデルの倍率が要る。
	// walkSpeed/runSpeedはゲーム側の歩き・ダッシュの速さで、歩き・走りのクリップを切り替える境目になる。
	void SetLocomotionBlendSpace(
		const CAnimationMesh& mesh,
		float modelScale,
		const std::array<aiAnimation*, 8>& clips,
		float walkSpeed,
		float runSpeed);
	void SetAttackAnimations(
		const std::array<aiAnimation*, 3>& weakAnimations,
		const std::array<aiAnimation*, 3>& heavyAnimations);
	void EnableMotionEditor();
	bool LoadMotionFile(const std::string& filename);
	void PlayAttackMotion();
	void PlayAttackMotion(int comboStep);
	void PlayHeavyAttackMotion();
	void PlayHeavyAttackMotion(int comboStep);
	void StartComboPreview(bool heavy);
	void TriggerHitStop(float seconds = 0.05f);
	void PlayDodgeMotion();
	// 被弾したときの上体ののけぞり。既存の「sword and shield impact」クリップを使う。
	// 攻撃と同じ上半身レイヤーで再生し、脚は移動・待機のまま残す
	// (腰や脚まで取り込むと接地が崩れることが分かっているため)。
	void SetImpactAnimation(aiAnimation* animation);
	void PlayImpactMotion();
	// ダッシュ攻撃(走りながら弱攻撃)。クリップと時間は Combat::PlayerDashAttackStep()。
	void SetDashAttackAnimation(aiAnimation* animation) { m_dashAttackAnimation = animation; }
	void PlayDashAttackMotion();
	// 攻撃の踏み込み(ルートモーション)。前回取り出してから今までに、クリップの腰が進んだ量を
	// キャラクターから見た向き(右・前、ゲーム内の単位)で返し、内部の値を0へ戻す。
	// 呼び出し側がキャラクターの位置へ足す。進んでいなければfalse。
	bool ConsumeRootMotion(float& right, float& forward);
	// 攻撃中の腰の回転の取り込み方(調整用)。0=取り込まない / 1=バインド姿勢からの変化 / 2=クリップのまま
	void SetAttackHipsRotationMode(int mode) { m_attackHipsRotationMode = mode; }
	bool IsMotionPlaying() const { return m_motionPlaying; }
	const std::vector<std::string>& GetBoneNames() const { return m_boneNames; }
	const std::string& GetSelectedBone() const { return m_selectedBone; }
	const MotionKeyframe& GetEditorKey() const { return m_editorKey; }
	const std::vector<std::string>& GetMotionChoices() const { return m_motionChoices; }
	void SelectBone(const std::string& boneName);
	void AdjustSelectedRotation(const Vector3& delta);
	void AdjustSelectedPosition(const Vector3& delta);
	void AdjustSelectedScale(const Vector3& delta);
	void AddOrUpdateCurrentKey();
	void PreviewEditorKey(const MotionKeyframe& key);
	void ApplyEditorKey(const MotionKeyframe& key);
	void BeginEditTransaction();
	void EndEditTransaction();
	void UndoEditorChange();
	void RedoEditorChange();
	bool CanUndoEditorChange() const { return !m_undoHistory.empty(); }
	bool CanRedoEditorChange() const { return !m_redoHistory.empty(); }
	bool SelectKeyAtTime(float time);
	bool MoveSelectedKey(float fromTime, float toTime);
	bool DeleteKeyAtTime(float time);
	bool DuplicateKeyAtTime(float time);
	void SetMotionFilename(const std::string& filename);
	void Update(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		const CharacterAnimationState& state);
	void UpdateSeatedPose(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		float amount);
	// タイトル画面の動作姿勢では剣を持つ腕と自由な手を別々に制御し、
	// 契約書の受け渡しが意図した動きに見えるようにする。
	void UpdateTitleContractPose(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		float reachAmount,
		float sheatheAmount,
		float drawAmount,
		float walkTime = 0.0f,
		aiAnimation* walkAnimation = nullptr,
		int walkFrame = 0);
	float GetMotionTime() const { return m_motionTime; }
	float GetMotionDuration() const { return m_motionDuration; }
	const char* GetAttackMotionName() const { return m_attackMotionName.c_str(); }
	int GetAttackMotionFrame() const
	{
		return static_cast<int>(m_motionTime * 60.0f);
	}
	int GetAttackWindupEndFrame() const
	{
		return static_cast<int>(m_attackWindupEnd * 60.0f);
	}
	int GetAttackActiveEndFrame() const
	{
		return static_cast<int>(m_attackActiveEnd * 60.0f);
	}

private:
	using BoneKeys = std::vector<MotionKeyframe>;

	void RenderMotionEditor();
	void EvaluateCustomMotion(
		float time,
		std::unordered_map<std::string, Matrix4x4>& rotations) const;
	bool SaveMotion(const std::string& filename) const;
	bool LoadMotion(const std::string& filename);
	bool LoadIdlePose(const std::string& filename);
	bool LoadSeatedPoseFile(const std::string& filename);
	void SortKeys(BoneKeys& keys);
	void NormalizeMotionTiming(float targetDuration);
	struct EditorSnapshot
	{
		std::unordered_map<std::string, BoneKeys> motionKeys;
		MotionKeyframe editorKey{};
		float motionTime = 0.0f;
		float motionDuration = 1.0f;
	};
	EditorSnapshot CaptureEditorSnapshot() const;
	void RestoreEditorSnapshot(const EditorSnapshot& snapshot);
	void CaptureUndoIfNeeded();
	void BuildFallbackAttackMotion();
	void BuildFallbackAttackComboMotion(int comboStep);
	void BuildFallbackHeavyAttackMotion();
	void BuildFallbackHeavyComboMotion(int comboStep);
	void BuildFallbackDodgeMotion();
	void PlayImportedAttackAnimation(
		aiAnimation* animation,
		const char* name,
		float duration,
		float windupEnd,
		float activeEnd);
	// コンボの段の設定(再生を始める位置・再生速度・終わり)どおりにクリップを再生する。
	void PlayImportedComboStep(
		aiAnimation* animation,
		const char* name,
		const Combat::PlayerComboStep& step);
	void ApplyAttackMotionDesign(
		const char* name,
		float windupEnd,
		float activeEnd,
		float torsoYaw,
		float torsoPitch,
		bool overhead);
	void StripAttackLowerBody();
	void BeginAttackBlend();
	// レイヤー合成で下半身側へ流すボーンの一覧。
	// 移動・待機クリップと、攻撃中の下半身で同じ範囲を使うため1箇所にまとめる。
	// Hipsはゲーム側が向きを管理するため含めない。
	const std::vector<std::string>& LowerBodyBones() const;

	std::string m_leftArm;
	std::string m_rightArm;
	std::string m_leftElbow;
	std::string m_rightElbow;
	std::string m_leftHand;
	std::string m_rightHand;
	std::string m_pelvis;
	std::string m_spine;
	std::string m_spine01;
	std::string m_spine02;
	std::string m_leftLeg;
	std::string m_rightLeg;
	std::string m_leftKnee;
	std::string m_rightKnee;
	std::string m_leftFoot;
	std::string m_rightFoot;

	std::vector<std::string> m_boneNames;
	// 読み込んだ攻撃クリップは見せたい上半身のチェーンだけを動かす。
	// ルート・腰・脚・指は、元アセットの軸変更や不安定なキーを受けず、
	// 安定したゲーム側の姿勢を維持する。
	std::vector<std::string> m_importedAttackBones;
	std::unordered_map<std::string, BoneKeys> m_motionKeys;
	// ダウンロードしたパックはTポーズを基準に作られている。
	// 先にこの立ち姿勢を適用し、攻撃ファイルはそこからの差分として重ねる。
	std::unordered_map<std::string, Matrix4x4> m_idlePose;
	std::unordered_map<std::string, Matrix4x4> m_seatedPose;
	// Sword and Shield Packの「sword and shield slash (4).fbx」を基準に出力し、
	// Fallen Paladinへリターゲットした攻撃モーション。
	std::string m_motionFilename = "assets/motion/sword_shield_attack_safe.motion";
	std::string m_attackMotionFilename = "assets/motion/sword_shield_attack_safe.motion";
	std::string m_attackMotionName = "Ready";
	std::vector<std::string> m_motionChoices;
	int m_selectedMotionIndex = 0;
	std::string m_selectedBone;
	MotionKeyframe m_editorKey{};
	float m_motionDuration = 1.0f;
	float m_motionTime = 0.0f;
	float m_attackWindupEnd = 0.18f;
	float m_attackActiveEnd = 0.62f;
	float m_hitStopSeconds = 0.0f;
	float m_attackBlendTime = 1.0f;
	float m_attackBlendDuration = 0.08f;
	float m_lastAttackTorsoYaw = 0.0f;
	float m_lastAttackTorsoPitch = 0.0f;
	std::unordered_map<std::string, Matrix4x4> m_lastRenderedPose;
	std::unordered_map<std::string, Matrix4x4> m_attackBlendFromPose;
	// PlayAttackMotion()はmeshを持たないため、次のUpdateで直前の姿勢を取得する。
	bool m_attackBlendPending = false;
	// 攻撃終了後も、最後の攻撃姿勢から待機/移動姿勢へ段差なく戻す。
	std::unordered_map<std::string, Matrix4x4> m_locomotionBlendFromPose;
	float m_locomotionBlendTime = 1.0f;
	float m_locomotionBlendDuration = 0.10f;
	bool m_locomotionBlendActive = false;
	bool m_useCustomMotion = false;
	bool m_motionPlaying = false;
	// 再生中の手続きモーションが前転か。前転の間は腕を回避を始めた瞬間の姿勢(剣と盾の構え)のまま保つ。
	// 前転のキーは休止姿勢(Tポーズ)からの差で作られていて、腕にそのまま使うと両腕を横へ広げて転がるため。
	bool m_dodgeMotion = false;
	bool m_dodgeBasePosePending = false;
	std::unordered_map<std::string, Matrix4x4> m_dodgeBasePose;
	bool m_motionLoop = true;
	bool m_editorInitialized = false;
	bool m_editorEnabled = false;
	bool m_motionFileLoaded = false;
	// 読み込んだSword and Shieldクリップは完全なローカル姿勢を含むため、
	// 絶対キーの上へ手作業のアイドル腕姿勢を重ねない。
	bool m_importedAttackPose = false;
	int m_motionMappedBoneCount = 0;
	std::deque<EditorSnapshot> m_undoHistory;
	std::deque<EditorSnapshot> m_redoHistory;
	bool m_editTransactionActive = false;
	bool m_editTransactionCaptured = false;
	bool m_timelineDragging = false;
	float m_timelineDragFrom = 0.0f;
	aiAnimation* m_walkAnimation = nullptr;
	aiAnimation* m_runAnimation = nullptr;
	aiAnimation* m_idleAnimation = nullptr;
	mutable std::vector<std::string> m_lowerBodyBonesCache;
	int m_idleFrame = 0;
	float m_idleFrameAccumulator = 0.0f;
	// 元クリップは30fps前後、ゲームは60Hz以上で更新されるため、
	// 1更新あたり0.5キー進めて元の速さを保つ。
	float m_idlePlaybackRate = 0.5f;
	std::array<aiAnimation*, 3> m_weakAttackAnimations{};
	std::array<aiAnimation*, 3> m_heavyAttackAnimations{};
	aiAnimation* m_impactAnimation = nullptr;
	aiAnimation* m_dashAttackAnimation = nullptr;
	bool m_comboPreviewActive = false;
	int m_comboPreviewStep = 0;
	bool m_comboPreviewHeavy = false;
	aiAnimation* m_importedAttackAnimation = nullptr;
	int m_importedAnimationFrame = 0;
	float m_importedAnimationFrameAccumulator = 0.0f;
	float m_importedAnimationFrameRate = 0.5f;
	int m_walkFrame = 0;
	float m_walkFrameAccumulator = 0.0f;
	// FBXは30fps、GameSceneは60Hzで更新される。
	// 読み込みサンプラーをキー番号で進めるため、この値で元データの速度を保つ。
	float m_walkPlaybackRate = 0.33f;
	float m_runPlaybackRate = 0.50f;

	// --- 移動のブレンドツリー ---
	struct BlendSpaceClip
	{
		aiAnimation* animation = nullptr;
		// 1周期で腰が進む距離(ゲーム内の単位)。再生速度を移動速度へ合わせるのに使う。
		float cycleDistance = 0.0f;
		float cycleSeconds = 1.0f;
	};
	// [方向(前,右,後,左) x 2 + 歩調(歩き=0,走り=1)]
	std::array<BlendSpaceClip, 8> m_blendSpaceClips{};
	bool m_blendSpaceReady = false;
	float m_blendSpaceWalkSpeed = 70.0f;
	float m_blendSpaceRunSpeed = 115.0f;
	// 全移動クリップで共有する正規化時間。共有することで、混ぜても左右の足が揃う。
	float m_blendSpacePhase = 0.0f;
	// クリップの単位 → ゲーム内の単位。移動の足合わせと攻撃の踏み込みの両方で使う。
	float m_clipToWorld = 0.0f;
	// 再生中の技の踏み込みの倍率(Combat::PlayerComboStep::rootMotionScale)。
	float m_attackRootMotionScale = 1.0f;
	// 攻撃の踏み込み: 前回サンプルした正規化時間と、まだ取り出されていない移動量。
	float m_rootMotionPreviousTime = 0.0f;
	float m_pendingRootMotionRight = 0.0f;
	float m_pendingRootMotionForward = 0.0f;
	int m_attackHipsRotationMode = 1;
	// 入力の速度をそのまま使うと、キーを押した瞬間に重みが跳ぶので、なめらかに追従させる。
	float m_smoothedVelocityRight = 0.0f;
	float m_smoothedVelocityForward = 0.0f;
	const BlendSpaceClip& BlendSpaceClipOf(Anim::LocomotionDirection direction, Anim::LocomotionGait gait) const;
	bool UpdateLocomotionBlendSpace(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		const CharacterAnimationState& state,
		float deltaSeconds,
		const std::unordered_map<std::string, Matrix4x4>* blendFromPose,
		float blendRate);
};
