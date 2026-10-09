#pragma once

#include "transform.h"
#include	"WeaponFit.h"
#include "CStaticMesh.h"
#include "AssimpPerse.h"
#include "CAnimationData.h"
#include "CTreeNode.h"	
#include "renderer.h"
#include "BoneCombMatrix.h"
#include "CMeshRenderer.h"
#include "CMaterial.h"
#include "CStaticMeshRenderer.h"
#include "CharacterModelProfile.h"
#include <vector>

class CAnimationMesh : public CStaticMesh
{
protected:
	// ボーン辞書
	std::unordered_map<std::string, BONE> m_BoneDictionary{};	// 20240714 DX化

	// カレントのアニメーションデータ
	aiAnimation* m_CurrentAnimation{};

	// assimp ノード名ツリー（親子関係がわかる）
	CTreeNode<std::string>	m_AssimpNodeNameTree{};
	std::unordered_map<std::string, Matrix4x4> m_RestLocalMatrices{};
	std::unordered_map<std::string, Matrix4x4> m_RestGlobalMatrices{};
	std::unordered_map<std::string, Matrix4x4> m_DebugBoneMatrices{};
	// AddBoneLocalRotations()で足した回転の控え。
	// Update()は「再生中のクリップにキーがある骨」しかAnimationMatrixを書き換えない。
	// キーが無い骨(例: dragon_idle.daeにはneck2のキーが無い)へ回転を足し続けると、
	// 毎フレーム積み重なって骨が折れ曲がる。足す前の行列を覚えておき、
	// クリップが書き換えていなければ、次に足すときへ元へ戻してからやり直す。
	struct LayeredBoneRotation
	{
		Matrix4x4 beforeLayer{}; ///< 足す前のAnimationMatrix
		Matrix4x4 afterLayer{};  ///< 足した直後のAnimationMatrix(書き換えられたかの判定に使う)
	};
	std::unordered_map<std::string, LayeredBoneRotation> m_LayeredBoneRotations{};

	// レンダラ
	CStaticMeshRenderer m_StaticMeshRenderer{};
	std::unique_ptr<CStaticMesh> m_swordMesh{};
	CStaticMeshRenderer m_swordRenderer{};
	std::string m_swordAssetPath{};
	std::string m_swordPresetPath{};
	bool m_swordUsesPlayerAsset = false;
	bool m_swordEmbeddedInPlayerAsset = false;
	std::unique_ptr<CMesh> m_swordProxyMesh{};
	CMeshRenderer m_swordProxyRenderer{};
	CMaterial m_swordProxyMaterial{};
	std::string m_swordBoneName{};
	float m_swordScale = 1.0f;
	float m_swordLocalHalfLength = 0.5f;
	float m_swordProxyLength = 75.0f;
	Vector3 m_swordModelCenter{};
	// 読み込んだ剣頂点のローカル空間で最も長い軸。
	// +Z固定と仮定せず、この軸から攻撃判定の両端を作る。
	Vector3 m_swordCollisionLocalAxis{ 0.0f, 0.0f, 1.0f };
	// Fallen Paladinの剣の握り位置を表すローカル座標。
	// 全体の境界中心ではなく、抽出した柄の領域から測定した値を使う。
	Vector3 m_swordGripLocalPoint{};
	// 剣を右手へ合わせる値(調整済み)。
	// 同じ名前のプリセットファイルがあればそこから読み直す(LoadSwordAttachmentPreset)。
	// 以前は画面上の調整ウィンドウから動かして保存できたが、合わせ終わったので外した。
	Vector3 m_swordRotationDegrees{ 81.50f, -46.50f, 76.75f };
	Vector3 m_swordHandOffset{ 0.650f, -0.070f, 0.646f };
	Vector3 m_swordWorldBase{};
	Vector3 m_swordWorldTip{};
	Vector3 m_swordPreviousWorldTip{};
	Matrix4x4 m_swordWorldMatrix = Matrix4x4::Identity;
	bool m_swordWorldSegmentValid = false;
	bool m_swordTransformLogged = false;
	bool m_swordEnabled = true;
	bool m_swordUseGuaranteedProxy = true;
	std::vector<uint32_t> m_embeddedSwordVertexIndices{};
	// 手本にする握り方(剣ボーンのローカル空間)。埋め込みの剣から測る。
	WeaponFit::Result m_handWeaponFit{};
	// いま持ち替えている武器の形(その武器のモデル空間)。
	WeaponFit::Result m_equippedWeaponFit{};
	bool m_greatSwordEquipped = false;
	bool m_greatSwordLoaded = false;

	// 埋め込みの剣から、手の握り方(握りの位置と刃の向き)を測る。
	// 別の武器を自動で持たせるための「手本」になる。
	void MeasureEmbeddedSword();
	void LoadSwordAttachmentPreset();

	// 1本のクリップを、指定したボーンだけへ適用する。
	// ボーン辞書を休止姿勢へ戻す処理は呼び出し側で行う(複数回重ねられるようにするため)。
	void ApplyAnimationToBones(
		aiAnimation* animationdata,
		int currentFrame,
		float frameFraction,
		const std::vector<std::string>& animatedBoneNames,
		bool loopAnimation,
		// このボーンだけはクリップの平行移動も取り込む(通常は腰)。
		// 絶対座標ではなくクリップ先頭からの相対オフセットとして扱うため、
		// ゲーム側が管理するワールド移動と二重にならない。
		const std::string& translationBone = std::string());
	bool BuildEmbeddedSwordWorldSegment(
		const Matrix4x4& parentWorld,
		Vector3& base,
		Vector3& tip) const;

	// ローカルポーズ生成
	void BuildLocalPoseMap(
		const aiAnimation* animationdata,
		int& CurrentFrame,
		std::unordered_map<std::string, SRTQ>& localposemap,
		float frameFraction = 0.0f, bool loopAnimation = true);

public:
	void SetCurentAnimation(aiAnimation* currentanimation);

	void Load(std::string filename, std::string texturedirectory = "");
	void ApplyModelProfile(const CharacterModelProfile& profile);

	// 階層構造を考慮したボーンコンビネーション行列を更新
	void UpdateBoneMatrix(CTreeNode<std::string>* ptree, Matrix4x4 matrix);		// 20240714 DX化	

	// アニメーションの更新
	void Update(BoneCombMatrix& bonecombarray, int& CurrentFrame,
		float frameFraction = 0.0f, bool loopAnimation = true);

	// 2つのアニメーションを回転だけクロスフェードして更新する。
	// 位置・スケールはto側を使い、ルート移動や接地が二重適用されないようにする。
	void UpdateBlendedRotationAnimation(
		BoneCombMatrix& bonecombarray,
		const aiAnimation* fromAnimation,
		int fromFrame,
		float fromFrameFraction,
		bool loopFromAnimation,
		const aiAnimation* toAnimation,
		int toFrame,
		float toFrameFraction,
		bool loopToAnimation,
		float blendRate);

	// 読み込んだ移動モーションは選択したボーンだけを動かし、
	// タイトル姿勢は剣を持つ手と契約書を持つ手を引き続き制御する。
	void UpdateAnimationWithManualPose(
		BoneCombMatrix& bonecombarray,
		aiAnimation* animationdata,
		int& CurrentFrame,
		const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations,
		const std::vector<std::string>& animatedBoneNames,
		bool loopAnimation = true,
		// キー番号の小数部。0.0〜1.0で、CurrentFrameと次のキーの間のどこにいるかを表す。
		// これを渡すとキー間をslerp補間するため、動きが滑らかになる。
		// 元データは30fps前後で焼かれており、60Hz以上で再生すると
		// 補間なしでは同じ姿勢が数フレーム続いてから飛ぶ、カクついた動きになる。
		float frameFraction = 0.0f,
		// 状態遷移時に指定ボーンだけを直前のローカル姿勢から補間する。
		const std::unordered_map<std::string, Matrix4x4>* blendFromPose = nullptr,
		float blendRate = 1.0f);

	/**
	 * @brief 2つのクリップをボーンごとに使い分けて合成する(上半身・下半身のレイヤー分け)。
	 *
	 * @details
	 * 下半身に移動・待機クリップ、上半身に攻撃クリップ、という使い分けを想定している。
	 * これにより「歩きながら攻撃」が表現でき、さらに攻撃中も脚が
	 * 移動クリップで正しく動き続けるため、脚が静止する問題を回避できる。
	 *
	 * overlayBones は baseBones より優先される(同じボーンが両方にある場合は overlay 側)。
	 */
	void UpdateLayeredAnimation(
		BoneCombMatrix& bonecombarray,
		aiAnimation* baseAnimation,
		int baseFrame,
		float baseFrameFraction,
		const std::vector<std::string>& baseBones,
		bool loopBaseAnimation,
		aiAnimation* overlayAnimation,
		int overlayFrame,
		float overlayFrameFraction,
		const std::vector<std::string>& overlayBones,
		bool loopOverlayAnimation,
		const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations,
		// ベース側で平行移動も取り込むボーン(通常は腰)。
		const std::string& baseTranslationBone = std::string(),
		// 攻撃開始時など、オーバーレイ側だけを直前の姿勢から補間する。
		const std::unordered_map<std::string, Matrix4x4>* overlayBlendFromPose = nullptr,
		float overlayBlendRate = 1.0f);

	// 現在適用中のローカルボーン姿勢を保存する。
	// 次の攻撃クリップへ移るとき、直前の歩行姿勢をブレンド元として使う。
	std::unordered_map<std::string, Matrix4x4> CaptureCurrentLocalPose() const;

	// --- ブレンドツリー用 ---
	// クリップの姿勢を、正規化時間(0〜1)で取り出す。ボーン行列の状態は変えない。
	// 返すのは指定ボーンのローカル行列(休止姿勢の平行移動にクリップの回転を合わせたもの)。
	// 複数のクリップを同じ正規化時間で取り出して混ぜることで、足の運びの位相を揃えられる。
	std::unordered_map<std::string, Matrix4x4> SampleLocalPose(
		aiAnimation* animation,
		float normalizedTime,
		const std::vector<std::string>& boneNames);
	// ボーン名→ローカル行列の姿勢を適用する。含まれないボーンは休止姿勢にし、
	// manualLocalRotationsは「姿勢に含まれないボーン」にだけ休止姿勢への加算として掛ける
	// (剣を握る手の姿勢など。UpdateAnimationWithManualPoseと同じ扱い)。
	void ApplyLocalPose(
		BoneCombMatrix& bonecombarray,
		const std::unordered_map<std::string, Matrix4x4>& localPose,
		const std::unordered_map<std::string, Matrix4x4>& manualLocalRotations);
	// 2つのローカル行列をスケール・回転・平行移動ごとに補間する(回転はslerp)。
	static Matrix4x4 BlendLocalMatrix(const Matrix4x4& from, const Matrix4x4& to, float amount);
	// --- 攻撃の踏み込み(ルートモーション)用 ---
	// クリップ先頭からの、ボーン(腰)の位置の変化量(クリップの単位)。
	Vector3 SampleBonePositionOffset(
		aiAnimation* animation, float normalizedTime, const std::string& boneName) const;
	// 腰をその場に置いたまま、クリップの沈み込みと回転の変化だけを取り込んだローカル行列。
	// 前後左右の移動はキャラクターの位置へ移すので、ここでは休止姿勢の位置のままにする
	// (両方に入れると二重に進み、足が滑る)。
	// 腰の高さはクリップの値をそのまま使う(しゃがんだ姿勢で始まるクリップでも足が浮かないように)。
	// クリップとモデルの腰の位置が同じ単位であることが前提(Mixamoはどちらもcm)。
	// rotationMode: 0=回転を取り込まない / 1=クリップのバインド姿勢からの変化を休止姿勢へ足す(既定) / 2=クリップの回転をそのまま使う
	Matrix4x4 SampleHipsInPlace(
		aiAnimation* animation, float normalizedTime, const std::string& boneName, int rotationMode) const;
	// ローカル行列の平行移動のうち、絶対値が最も大きい成分(符号付き)。
	// 腰の休止位置では、これが親空間での「高さ」になる(親の上の軸がYとは限らないため)。
	static float DominantAxisComponent(const Matrix4x4& localMatrix);
	// 休止姿勢のボーンのローカル行列(見つからなければ単位行列)。
	Matrix4x4 GetRestLocalMatrix(const std::string& boneName) const;
	// 休止姿勢でのボーンのモデル空間の高さ(Y)。
	// クリップの腰の高さと比べて、クリップの単位をモデルの単位へ換算するのに使う。
	float GetRestBoneModelHeight(const std::string& boneName) const;

	// レスト姿勢に対して指定ボーンのローカル回転を加えたポーズを更新
	void UpdateManualPose(
		BoneCombMatrix& bonecombarray,
		const std::unordered_map<std::string, Matrix4x4>& localRotations);

	std::vector<std::string> GetBoneNames() const;
	std::unordered_map<std::string, std::string> GetDebugBoneParentNames() const;

	/** 脚の2ボーンIKで使う骨の名前。 */
	struct LegIKChain
	{
		std::string upperLeg;  ///< 股(ももの付け根)
		std::string lowerLeg;  ///< 膝
		std::string foot;      ///< 足首
		std::string toe;       ///< つま先
	};

	/**
	 * @brief つま先を目標位置へ動かす脚の2ボーンIK。
	 *
	 * @param chain 脚の骨の名前。
	 * @param targetToeModelPosition つま先の目標位置(モデル空間)。
	 * @param maxExtensionRatio 脚を伸ばしきる割合の上限(1.0で完全に伸ばす)。
	 * @param softening 上限へ近づいたときに滑らかに抑え始める距離(モデル単位)。
	 * @return 解けたらtrue。
	 *
	 * @details
	 * 今の姿勢をできるだけ残したまま、股と膝の回転だけを足して目標へ寄せる
	 * (アニメーションを置き換えるのではなく、最小限だけ直す)。
	 * 足首の向きは元の姿勢のまま保つので、足の裏が地面と平行なまま残る。
	 * 脚が伸びきる手前から滑らかに制限し、棒のような脚(過伸展)にならないようにする。
	 * 呼んだ後は RefreshBoneMatrices() で階層と定数バッファへ反映すること。
	 */
	bool SolveLegIK(
		const LegIKChain& chain,
		const Vector3& targetToeModelPosition,
		float maxExtensionRatio = 0.985f,
		float softening = 1.5f);

	/** 骨のモデル空間での位置(今の姿勢)。 */
	bool GetBoneModelPosition(const std::string& boneName, Vector3& outPosition) const;

	/** IKで書き換えたローカル行列を、親子関係とGPUへ渡す行列へ反映する。 */
	void RefreshBoneMatrices(BoneCombMatrix& bonecombarray);

	/**
	 * @brief 再生中のクリップの姿勢へ、骨ごとの回転を「足す」。
	 *
	 * @param localRotations 骨の名前 → その骨自身のローカル空間で足す回転。
	 *
	 * @details
	 * UpdateAnimationWithManualPose()は、クリップで動かす骨には手続きの姿勢を足せない(上書きになる)。
	 * 敵の攻撃は1本のクリップを全身へ当てているので、その上から首や尾だけを動かして
	 * 攻撃ごとに違うシルエットを作るために使う。
	 * 行ベクトル規約なので「足す回転 × クリップのローカル行列」の順で、骨の根元を中心に回る。
	 * 呼んだ後は RefreshBoneMatrices() で階層と定数バッファへ反映すること。
	 *
	 * **毎フレーム呼ぶこと。足すのをやめるときは空の表で呼ぶ**(前回足した分を取り消すため)。
	 * Update()はクリップにキーがある骨しか書き換えないので、キーが無い骨は
	 * 前のフレームの姿勢が残ったままになる。そこへ足し続けると回転が積み重なって骨が壊れる。
	 * 実際、待機クリップ(dragon_idle.dae)には首の2番目のキーが無く、
	 * 尾回転の溜め(約1.1秒=約69フレーム)の間に首が1周以上ねじれた。
	 *
	 * @return 骨の姿勢を変えた(足した、または前回の分を取り消した)ならtrue。
	 *         trueのときだけ RefreshBoneMatrices() を呼べばよい。
	 */
	bool AddBoneLocalRotations(const std::unordered_map<std::string, Matrix4x4>& localRotations);
	const std::unordered_map<std::string, Matrix4x4>& GetDebugBoneMatrices() const
	{
		return m_DebugBoneMatrices;
	}
	// 現在のボーン行列でスキニングした頂点のローカルZ最大値を返す。
	// ドラゴンは描画時にX軸へ+90度回転するため、接地判定ではこの値が
	// ワールドYの最下点に対応する。
	float GetAnimatedLocalMaxZ() const;

	// X軸へ(90度 + extraPitchRadians)回転させたときの、
	// スキニング済み頂点のワールドY最小値(モデル単位・スケール前)を返す。
	// 演出で本体をさらに前後へ傾けると最下点が変わるため、
	// 「Z最大値=最下点」という前提が崩れて敵が地面へ埋まる。
	// extraPitchRadians=0のとき、戻り値は -GetAnimatedLocalMaxZ() に一致する。
	float GetAnimatedLowestLocalHeight(float extraPitchRadians) const;
	// 現在のボーン行列でスキニングした頂点のローカルY最小値(モデル単位・スケール前)。
	// 回転させずに描くモデル(プレイヤー)で、前転など体が大きく回る動きの接地に使う。
	float GetAnimatedLocalMinY() const;
	// 現在のボーン行列でスキニングした頂点を、upAxis方向の成分で見たときの最小値。
	float GetAnimatedLowestAlong(const Vector3& upAxis) const;

	// 描画
	void UpdateSwordWorldTransform(const Matrix4x4& parentWorld);

	// --- 武器の持ち替え(大剣) ---
	// 埋め込みの片手剣を隠し、別のモデルを手本の握り方へ自動で合わせて持たせる。
	// 手本は埋め込みの剣(MeasureEmbeddedSword)。武器ごとに角度と位置を手で測り直さなくてよい。
	// 大剣用のモデルがまだ無いので、いまは assets/model/Sword.fbx を大剣の長さで使う。
	// 本物の大剣モデルが用意できたら、読み込むファイルを差し替えるだけで持てる。
	bool EquipGreatSword(bool equip);
	// 持ち替え中に埋め込みの片手剣を隠す。UpdateSwordWorldTransform()の後に毎フレーム呼ぶ。
	void ApplyEquippedWeaponVisibility(BoneCombMatrix& bonecombarray);
	bool IsGreatSwordEquipped() const { return m_greatSwordEquipped; }
	// 測定結果(デバッグ表示用)。手本の握り方と、いま持っている武器の形。
	const WeaponFit::Result& GetHandWeaponFit() const { return m_handWeaponFit; }
	const WeaponFit::Result& GetEquippedWeaponFit() const { return m_equippedWeaponFit; }
	void Draw();
	bool IsSwordLoaded() const { return m_swordMesh != nullptr || m_swordEmbeddedInPlayerAsset; }
	bool GetSwordWorldSweep(Vector3& base, Vector3& tip, Vector3& previousTip) const
	{
		if (!m_swordWorldSegmentValid) return false;
		base = m_swordWorldBase;
		tip = m_swordWorldTip;
		previousTip = m_swordPreviousWorldTip;
		return true;
	}
};
