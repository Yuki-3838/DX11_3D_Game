#pragma once

#include <array>
#include <string>
#include <deque>
#include <unordered_map>
#include <vector>

#include "CAnimationMesh.h"
#include "LocomotionBlendSpace.h"
#include "CombatAttackTable.h"

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
	// クリップごとの足運びの位相のずれを測るため、meshは非constで受ける。
	void SetLocomotionBlendSpace(
		CAnimationMesh& mesh,
		float modelScale,
		const std::array<aiAnimation*, 8>& clips,
		float walkSpeed,
		float runSpeed);
	void SetAttackAnimations(
		const std::array<aiAnimation*, 3>& weakAnimations,
		const std::array<aiAnimation*, 3>& heavyAnimations);
	bool LoadMotionFile(const std::string& filename);
	// 持っている武器。攻撃の再生速度と長さがこれで変わる。
	// 戦闘判定(OneVsOneCombat)にも同じ種類を渡すこと。
	void SetWeapon(Combat::WeaponKind weapon) { m_weapon = weapon; }
	/**
	 * @brief いま再生中の技が「地面を離れる技」か。
	 *
	 * 接地の計算で使う。地面を離れる技の間だけ、足を地面へ着けにいくのをやめる。
	 * 腰が上がったかどうかで自動判定しようとしたが、**膝を伸ばしただけでも腰は上がる**ので
	 * 区別できなかった(弱2・弱3で0.8〜0.9単位浮いた)。技ごとに決めるのが確実である。
	 */
	bool AttackLeavesGround() const { return m_attackLeavesGround; }
	void PlayAttackMotion();
	void PlayAttackMotion(int comboStep);
	void PlayHeavyAttackMotion();
	void PlayHeavyAttackMotion(int comboStep);
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

	void EvaluateCustomMotion(
		float time,
		std::unordered_map<std::string, Matrix4x4>& rotations) const;
	bool SaveMotion(const std::string& filename) const;
	bool LoadMotion(const std::string& filename);
	bool LoadIdlePose(const std::string& filename);
	bool LoadSeatedPoseFile(const std::string& filename);
	void SortKeys(BoneKeys& keys);
	void NormalizeMotionTiming(float targetDuration);
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
	// つま先。足首まででつないでも、つま先の骨が残ると**そこだけ1フレーム跳ぶ**。
	// 攻撃クリップはつま先を動かすが、移動・待機側で扱っていないと休止姿勢へ戻るためである。
	std::string m_leftToe;
	std::string m_rightToe;

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
	float m_motionDuration = 1.0f;
	float m_motionTime = 0.0f;
	float m_attackWindupEnd = 0.18f;
	float m_attackActiveEnd = 0.62f;
	float m_hitStopSeconds = 0.0f;
	float m_attackBlendTime = 1.0f;
	float m_attackBlendDuration = 0.08f;
	// **脚だけは長くつなぐ**。
	// 腕は速く動いてほしい(攻撃の出だしが鈍ると操作が重く感じる)が、脚を同じ速さでつなぐと
	// クリップごとに違う足の位置へ一瞬で移ってしまい、人の歩き方にならない。
	// 足を踏みかえるには人間でも0.2秒ほどかかる。
	// 長さは固定ではなく、**足が実際にどれだけ動くか**で決める(攻撃に入る最初のフレームで測る)。
	// 技のつなぎによって足の位置の差は大きく違い、
	// 近いのに長くつなぐと足が止まって見え、遠いのに短いと足が飛んで見える。
	float m_attackLegBlendDuration = 0.24f;
	bool m_attackLegBlendPending = false;
	// 地面を離れる技か(いまはダッシュ攻撃だけ)。
	bool m_attackLeavesGround = false;
	// 技の間、脚をどこから取るか。**技の初めに決めて、終わるまで変えない**。
	// 毎フレーム見直すと、踏み込みで体が動いた拍子に脚の出どころが入れ替わり、
	// その1フレームだけ足が別の場所へ飛ぶ。
	bool m_attackLowerBodyFromLocomotion = false;
	bool m_attackLowerBodyPending = false;
	/**
	 * @brief 脚の「ずらし」(モーションワープ)。骨ごとの、自分のローカル空間で足す回転。
	 *
	 * 2つの姿勢を混ぜるやり方だと、つないでいる間はクリップの動きそのものが鈍る
	 * (混ぜる相手が止まっているため)。踏み込みの勢いが出ない。
	 * そこで**クリップは最初から全開で再生**し、
	 * 「始めた瞬間の足の位置のズレ」だけを足して、時間をかけて0へ戻す。
	 * 足はクリップの勢いのまま動き、しかも前の姿勢から連続する。
	 *
	 * 作り方: 直前の姿勢をF、クリップの最初の姿勢をL0とすると、ずらしは C = F * L0⁻¹。
	 * 行ベクトル規約なので「C × クリップのローカル行列」の順に掛けると骨の根元を中心に回る。
	 * 休止姿勢の平行移動は両方に共通なので、Cは**回転だけ**になり、骨が外れることはない。
	 */
	std::unordered_map<std::string, Matrix4x4> m_attackLegWarp;
	std::unordered_map<std::string, Matrix4x4> m_locomotionLegWarp;
	bool m_locomotionLegWarpPending = false;
	// 攻撃クリップの腰の高さを、技を始めた瞬間の高さへそろえるためのずらし量。
	// クリップの中での腰の上下動(踏み切り・沈み込み)はそのまま残す。
	float m_attackHipsHeightOffset = 0.0f;
	bool m_attackHipsOffsetPending = false;
	float m_lastAttackTorsoYaw = 0.0f;
	float m_lastAttackTorsoPitch = 0.0f;
	std::unordered_map<std::string, Matrix4x4> m_lastRenderedPose;
	std::unordered_map<std::string, Matrix4x4> m_attackBlendFromPose;
	// PlayAttackMotion()はmeshを持たないため、次のUpdateで直前の姿勢を取得する。
	bool m_attackBlendPending = false;
	// 攻撃終了後も、最後の攻撃姿勢から待機/移動姿勢へ段差なく戻す。
	std::unordered_map<std::string, Matrix4x4> m_locomotionBlendFromPose;
	float m_locomotionBlendTime = 1.0f;
	// 攻撃・前転から移動/待機へ戻す時間。脚は上半身より大きく動くので、短すぎると足が一瞬で入れ替わる。
	float m_locomotionBlendDuration = 0.18f;
	float m_locomotionLegBlendDuration = 0.30f;
	bool m_locomotionBlendActive = false;
	bool m_useCustomMotion = false;
	Combat::WeaponKind m_weapon = Combat::WeaponKind::OneHanded;
	bool m_motionPlaying = false;
	// 再生中の手続きモーションが前転か。前転の間は腕を回避を始めた瞬間の姿勢(剣と盾の構え)のまま保つ。
	// 前転のキーは休止姿勢(Tポーズ)からの差で作られていて、腕にそのまま使うと両腕を横へ広げて転がるため。
	bool m_dodgeMotion = false;
	bool m_dodgeBasePosePending = false;
	std::unordered_map<std::string, Matrix4x4> m_dodgeBasePose;
	bool m_motionLoop = true;
	bool m_motionFileLoaded = false;
	// 読み込んだSword and Shieldクリップは完全なローカル姿勢を含むため、
	// 絶対キーの上へ手作業のアイドル腕姿勢を重ねない。
	bool m_importedAttackPose = false;
	int m_motionMappedBoneCount = 0;
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
		// 前向き歩きを基準にした足運びの位相のずれ(0〜1)。
		// クリップはそれぞれ違うタイミングで足を上げるので、同じ正規化時間で混ぜると
		// 「片方は左足を上げ、もう片方は左足を着いている」姿勢が混ざって足が入れ替わる。
		// 読み込み時に測って、サンプルする位相をクリップごとにずらす。
		float phaseOffset = 0.0f;
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
	// 歩きクリップで両足が最も揃う位相。止まるときはここへ足を収める。
	// これを使わずに位相を止めた場所で固めると、脚を開いた姿勢のまま立ち姿へ混ざり、
	// 足の位置が最大79単位(脚の長さは約84)一気に動いていた。
	float m_locomotionStopPhase = 0.0f;
	bool m_footPhaseMeasured = false;
	// クリップごとの足運びの位相を測り、止まる姿勢を作る。
	void MeasureLocomotionFootPhases(CAnimationMesh& mesh);
	// 今の脚の姿勢に最も近い位相へ合わせ直す。攻撃や前転から移動へ戻るときに使う。
	void ResyncLocomotionPhase(
		CAnimationMesh& mesh,
		const std::unordered_map<std::string, Matrix4x4>& pose);
	/**
	 * @brief 脚のつなぎに何秒かけるかを、**足が実際に動く距離**から決める。
	 *
	 * 近い姿勢へ長くかけると足が止まって見え、遠い姿勢へ短くかけると足が飛んで見える。
	 * 距離は脚の長さに対する割合で見るので、モデルを差し替えても同じ判断になる。
	 */
	float LegBlendDurationFor(
		CAnimationMesh& mesh,
		const std::unordered_map<std::string, Matrix4x4>& fromPose,
		const std::unordered_map<std::string, Matrix4x4>& toPose) const;
	// 攻撃・前転から戻るときの、戻り先の脚の姿勢(移動中ならクリップ、止まっていれば休止姿勢)。
	std::unordered_map<std::string, Matrix4x4> LocomotionLegTargetPose(
		CAnimationMesh& mesh,
		const CharacterAnimationState& state);
	const BlendSpaceClip& BlendSpaceClipOf(Anim::LocomotionDirection direction, Anim::LocomotionGait gait) const;
	bool UpdateLocomotionBlendSpace(
		CAnimationMesh& mesh,
		BoneCombMatrix& boneComb,
		const CharacterAnimationState& state,
		float deltaSeconds,
		const std::unordered_map<std::string, Matrix4x4>* blendFromPose,
		float blendRate,
		// 脚だけに使う補間の進み具合(上半身より遅く戻す)。
		float legBlendRate);
	// 骨が脚かどうか(LowerBodyBonesに入っているか)。
	bool IsLowerBodyBone(const std::string& boneName) const;
	/**
	 * @brief ずらし(ワープ)の対象にする骨。脚**と腰**。
	 *
	 * 腰を外すと、技の腰の位置から立ち姿の腰の位置へ一瞬で移る。
	 * 腰は脚の親なので、膝もくるぶしもつま先も**まとめて同じ量だけ平行移動**し、
	 * 脚全体がその場で飛んで見える(実測: 技の最後の1フレームで膝・足首・つま先が
	 * そろって0.27モデル単位ずれていた)。
	 */
	const std::vector<std::string>& WarpBones() const;
	mutable std::vector<std::string> m_warpBonesCache;
	// 「直前の姿勢」と「クリップの最初の姿勢」から、脚のずらしを作る。
	static void BuildLegWarp(
		const std::unordered_map<std::string, Matrix4x4>& fromPose,
		const std::unordered_map<std::string, Matrix4x4>& clipPose,
		const std::vector<std::string>& legBones,
		std::unordered_map<std::string, Matrix4x4>& outWarp);
	// ずらしを、残り具合(1で全部・0で無し)だけ効かせる。
	static void ApplyLegWarp(
		const std::unordered_map<std::string, Matrix4x4>& warp,
		float amount,
		std::unordered_map<std::string, Matrix4x4>& pose);
};
