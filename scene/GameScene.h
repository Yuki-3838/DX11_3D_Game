#pragma once
#include <memory>
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/CShader.h"
#include "../system/WeaponTrail.h"
#include "../system/ArenaFloor.h"
#include "../system/AttackTimelineEditor.h"
#include "../system/HitEffect.h"
#include "../system/EnemyFireball.h"
#include "../system/EnemyMotionEffects.h"
#include "../system/ThirdPersonCamera.h"
#include "../system/CAnimationMesh.h"
#include "../system/CAnimationData.h"
#include "../system/CCharacterAnimator.h"
#include "../system/BoneCombMatrix.h"
#include "../system/collision.h"
#include "../system/OneVsOneCombat.h"
#include "../system/FootLock.h"
#include "../gameobject/player.h"
#include "../gameobject/field.h"
#include "../gameobject/wall.h"
#include "../gameobject/enemy.h"

class GameScene : public IScene 
{
public:
	static constexpr uint8_t INITIAL_WALLNUM = 0;
	static constexpr uint8_t INITIAL_ENEMYNUM = 1;

	explicit GameScene();
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;

	player* getplayer();

	// デバッグ表示は1つのウィンドウにタブでまとめる。
	// 以前は7つの独立したウィンドウが画面中に散らばり、
	// ゲーム画面がほとんど見えなくなっていた。
	void DrawDebugWindow();
	void DebugWalls();
	void DebugEnemies();
	void DebugPlayerSRT();
	void DebugCamera();
	void DebugCombat();
	void DebugCollision();
	// 当たり判定の3D上のラベル。タブの選択状態に関わらず描くため、
	// タブの中身とは別の関数に分けている。
	void DrawCollisionWorldLabels();
	// 調整用。dev_settings.ini の enemy_hp を試合開始時の敵の体力にする。
	// 弱り具合(疲れ・瀕死)の見た目を、長い戦闘をしなくても確かめられるようにする。
	void ApplyDebugEnemyStartHp();
	void DebugAudio();
	void UpdateEnemyAnimation(float deltaSeconds = 0.0f);
	void StartEnemyIntro();
	void UpdateEnemyIntro(float deltaSeconds);
	void DrawEnemyIntroOverlay();
	void DrawGameplayHud();
	void UpdateArenaCameraCollision();
	// 敵の体がカメラとプレイヤーの間に入る場合の、許容できるカメラ距離を返す。
	// 遮蔽が無ければ desiredDistance をそのまま返す。
	float CalcEnemyOccludedCameraDistance(float desiredDistance) const;
	// 描画されている敵の、足元の高さと見た目の高さ(ワールド単位)。
	// ドラゴンのモデルは寝た向きで作られ、描画時にX軸へ90度起こしているので、
	// ローカルの境界ボックスのY寸法は高さではなく体の長さになる。境界ボックスの角を描画と同じ変換で動かして測る。
	void GetEnemyVisualVerticalExtent(float& bottomY, float& height) const;
	void UpdateGameplayCamera(float deltaSeconds = 0.0f);
	// 攻撃(コンボの各段)の出だしに、プレイヤーを入力方向(無ければロックオン中の敵)へ向ける。
	void AimPlayerAttack();
	// 画面右下の操作説明。ゲームパッドがつながっていればパッドの表記にする。
	void DrawControlGuide();
	bool m_showControlGuide = true;
	void UpdateCombatCameraDistance(float deltaSeconds);

	// --- 攻撃のタイムライン(F6のエディターと、ゲーム中のイベント) ---
	void OpenTimelineEditor();
	void CloseTimelineEditor();
	// エディターを開いている間の更新。ゲームは止め、プレイヤーを再生位置の姿勢にする。
	void UpdateTimelineEditor(float deltaSeconds);
	// プレイヤーを、技のクリップの指定した時刻の姿勢・位置にする(プレビュー)。
	void PoseTimelinePreview(Combat::PlayerAttackSlot slot, float clipSeconds);
	// プレビューで、いまの技を始める位置と向き。コンボを通して再生しているなら、前の段の踏み込みの分だけ前。
	SRT TimelinePreviewBaseSrt() const;
	// コンボの最初の段を始める位置と向き。「敵の前に立つ」なら、敵を向いて間合いの位置に立つ。
	SRT TimelinePreviewStartSrt() const;
	// クリップ全体を細かく区切って姿勢を作り、剣先の通り道・速さ・敵に当たる区間を求める。
	void RecomputeTimelineAnalysis();
	// エディターを開いている間の3D表示(剣先の通り道・剣の箱)。
	void DrawTimelineEditor3D();
	// 技のクリップ(Combat::EditablePlayerAttack(slot).clipFile)を読み込み直す。
	bool ReloadAttackClip(Combat::PlayerAttackSlot slot);
	// タイムラインのイベント(効果音・カメラの揺れ・火花)を起こす。
	void FireTimelineEvent(const Combat::TimelineEvent& event);
	// ゲーム中、再生している技のクリップの時刻を見て、イベントを起こし、剣の軌跡を出す。
	void UpdateAttackTimelineEvents();
	// 技を始めたときに呼ぶ。どの技のタイムラインを見るかを覚える。
	void BeginAttackTimeline(Combat::PlayerAttackSlot slot);

	AttackTimelineEditor m_timelineEditor;
	AttackTimelineEditor::Analysis m_timelineAnalysis;
	int m_timelineAnalysisRevision = -1;
	Combat::PlayerAttackSlot m_timelineAnalysisSlot = Combat::PlayerAttackSlot::Weak1;
	// 剣先と刃の根元の通り道(ワールド座標)と、それぞれのクリップの時刻。
	std::vector<Vector3> m_timelinePathTips;
	std::vector<Vector3> m_timelinePathBases;
	std::vector<float> m_timelinePathTimes;
	// エディターを開いた瞬間のプレイヤーの位置と向き。プレビューの踏み込みはここから足し、閉じたら戻す。
	SRT m_timelineSavedPlayerSrt{};
	float m_timelineSavedGroundOffsetY = 0.0f;
	bool m_timelineBladeHitsEnemy = false;
	bool m_timelineEditorWasOpen = false;
	// 保存せずにエディターを閉じたときの注意を、画面に出しておく残り時間(秒)。
	float m_timelineUnsavedNoticeTime = 0.0f;
	// エディターのカメラ(右ドラッグで回り込む・ホイールで寄る)。
	float m_timelineCameraYaw = 0.0f;
	float m_timelineCameraPitch = 0.22f;
	float m_timelineCameraDistance = 75.0f;
	// エディターではカメラを直接置くので、揺れはここで作る。
	float m_timelineShakeTime = 0.0f;
	float m_timelineShakeDuration = 0.0f;
	float m_timelineShakeStrength = 0.0f;
	// ゲーム中のイベント: いま再生している技と、前のフレームに見たクリップの時刻。
	Combat::PlayerAttackSlot m_playingAttackSlot = Combat::PlayerAttackSlot::Weak1;
	float m_lastAttackEventClipTime = -1.0f;
	bool m_attackTimelineActive = false;

	// 平行光源から見た深度を書き込み、キャラクターの形をした影を作る。
	void RenderShadowMap();
	Matrix4x4 BuildLightViewProjection() const;

private:
	ThirdPersonCamera m_camera;
	std::unique_ptr<CAnimationMesh> m_playerAnimationMesh;
	std::unique_ptr<CAnimationMesh> m_enemyAnimationMesh;
	CCharacterAnimator m_playerAnimator;
	BoneCombMatrix m_playerBoneComb;
	BoneCombMatrix m_enemyBoneComb;
	CAnimationData m_enemyAnimationData;
	CAnimationData m_playerAnimationData;
	aiAnimation* m_playerWalkAnimation = nullptr;
	aiAnimation* m_enemyIdleAnimation = nullptr;
	aiAnimation* m_enemyWalkAnimation = nullptr;
	aiAnimation* m_enemyAttackAnimation = nullptr;
	aiAnimation* m_enemyDieAnimation = nullptr;
	int m_enemyAnimationFrame = 0;
	// 次のキーまでの進み具合(0〜1)。経過時間で進める(fpsに依存させない)。
	float m_enemyAnimationTick = 0.0f;
	// カットシーン終了時のIdle姿勢を、戦闘開始時のWalk先頭へ短時間でつなぐ。
	static constexpr float ENEMY_INTRO_BATTLE_BLEND_SECONDS = 0.18f;
	bool m_enemyIntroBattleBlendActive = false;
	float m_enemyIntroBattleBlendTime = 0.0f;
	int m_enemyIntroBlendFromFrame = 0;
	float m_enemyIntroBlendFromFraction = 0.0f;
	int m_enemyLastSampledFrame = 0;
	float m_enemyLastSampledFraction = 0.0f;
	// 攻撃の入り。噛みつきはクリップの途中(0.45秒)から再生するので、直前の姿勢から短くつなぐ。
	static constexpr float ENEMY_ATTACK_BLEND_SECONDS = 0.14f;
	aiAnimation* m_enemyAttackBlendFromAnimation = nullptr;
	int m_enemyAttackBlendFromFrame = 0;
	float m_enemyAttackBlendFromFraction = 0.0f;
	float m_enemyAttackBlendTime = 0.0f;
	bool m_enemyAttackBlendActive = false;
	enemy::MotionState m_previousEnemyMotionState = enemy::MotionState::Approach;
	std::array<std::unique_ptr<Segment>,3> m_segments;		// ローカル軸表示用線分

	std::unique_ptr<player>	m_player;						//	プレイヤ
	std::unique_ptr<field>	m_field;						//	フィールド
	std::vector<std::unique_ptr<wall>>	m_walls;			//	ｗａｌｌ
	std::vector<std::unique_ptr<enemy>>	m_enemies;			//	敵
	// 見た目だけの闘技場パーツ。衝突判定はm_wallsが担当し、
	// デバッグ編集画面とゲームプレイが同じ境界を共有する。
	// 床はマス目付き(距離感をつかみやすくするため)。
	ArenaFloor m_arenaFloor;
	Box m_arenaWallVisual{1.0f, 1.0f, 1.0f};
	Box m_arenaWallCapVisual{1.0f, 1.0f, 1.0f};
	// 影生成パス用のシェーダー(深度だけを書き込む)。
	// スキンあり・なしで頂点の変換が違うため2種類持つ。
	CShader m_shadowDepthShader;
	CShader m_shadowDepthSkinShader;
	bool m_shadowEnabled = true;
	// 攻撃中だけカメラを寄せる。引いたままだと画面内のキャラクターが小さく、
	// 振りの迫力が出ないため、踏み込みに合わせて距離を詰める。
	// 剣の軌跡。武器ボーン由来のワールド座標だけを使うため、
	// 剣モデルやプレイヤーモデルを差し替えてもそのまま動く。
	WeaponTrail m_playerWeaponTrail;
	// 命中点の火花。当たったワールド座標と向きだけを渡すため、
	// 敵の攻撃や別の武器にも同じものを使える。
	HitEffect m_hitEffect;
	// 敵が吐く火球。飛んでいる弾・着弾の爆発・吐く前に口に溜まる火。
	EnemyFireball m_enemyFireball;
	// 敵の口の位置(ワールド)。火球を吐く位置と、口に溜まる火の位置に使う。
	// アニメーションの更新で骨から求め直す(首を持ち上げる構えも含めた位置になる)。
	Vector3 m_enemyMouthPosition{};
	bool m_enemyMouthValid = false;
	// 敵の体の動きに付ける演出(土煙・地面の衝撃の輪・咆哮の衝撃波)。
	EnemyMotionEffects m_enemyMotionFx;
	// 敵の攻撃の通り道。剣の軌跡と同じ WeaponTrail を、尾と頭に1本ずつ持たせる。
	WeaponTrail m_enemyTailTrail;
	WeaponTrail m_enemyHeadTrail;
	// 足が地面を踏んだ瞬間を見つけるための、足1本ごとの記録。
	struct EnemyFootTracker
	{
		const char* bone = "";
		float previousHeight = 0.0f;   ///< 前のフレームの足の骨の高さ(床から)
		float liftedHeight = 0.0f;     ///< 前に踏んでから一番高く上がった高さ
		float descentSpeed = 0.0f;     ///< 下ろしている速さ(踏んだ強さに使う)
		bool valid = false;
	};
	EnemyFootTracker m_enemyFeet[4];
	enemy::MotionState m_enemyFxPreviousState = enemy::MotionState::Approach;
	float m_enemyFxPreviousStateTime = 0.0f;
	float m_enemyChargeDustTimer = 0.0f;
	// 咆哮の衝撃波を出し続ける残り時間と、次の輪までの時間。
	float m_enemyRoarFxSeconds = 0.0f;
	float m_enemyRoarRingTimer = 0.0f;

	// 命中の手応え用。残り時間で減衰させる。
	// 敵は被弾時に発光させ、プレイヤー被弾時は画面端を赤くする。
	float m_enemyHitFlashTime = 0.0f;
	float m_playerDamageFlashTime = 0.0f;
	// 体力バーの赤い部分(減った分)。被弾すると緑はすぐ減り、赤は少し待ってから追いかけて減る。
	// どれだけ食らったかが一目で分かるようにする(モンスターハンターの体力バーの見え方を参考)。
	float m_playerHpTrail = Combat::Tuning::PLAYER_MAX_HP;
	float m_playerHpLastFrame = Combat::Tuning::PLAYER_MAX_HP;
	float m_playerHpTrailHoldSeconds = 0.0f;
	// 赤が減り始めるまでの待ち時間と、減る速さ(体力/秒)。連続で食らうと待ち時間はやり直しになる。
	// モンハンワールドの赤い部分は「1.5秒ごとに体力1」ずつ変わる(攻略wikiの実測。
	// 回復速度スキルLv1で0.75秒、ワイルズ・ライズでもLv1で2倍と書かれており、基準の1.5秒と合う)。
	// 最大体力も同じ100なので、その速さ(約0.67/秒)と最初の1目盛りまでの1.5秒をそのまま使う。
	// 以前の0.6秒・25/秒では、10の被弾の赤が1秒で消えてしまい速すぎた(ユーザー判断 2026-11-02)。
	// デバッグ表示の「戦闘」タブで変えられる(ゲームを終えると元に戻る)。
	float m_playerHpTrailHoldTime = 1.5f;
	float m_playerHpTrailDrainPerSecond = 1.0f / 1.5f;
	float m_cameraBaseDistance = 70.0f;
	float m_cameraDistanceCurrent = 70.0f;
	float m_cameraBaseTargetHeight = 25.0f;
	// マウスで視点を回すか(カーソルをゲーム画面に閉じ込める)。左Altで切り替える。
	bool m_mouseLookCaptured = true;
	// 足の接地(フットロック)。移動中、接地している足のつま先をワールドへ固定して脚をIKで合わせる。
	void UpdateFootLock(float deltaSeconds);
	// 撮影・調整用: 動かない・攻撃しない・倒れない敵(練習台)をプレイヤーの正面へ置く。
	void PlaceTrainingDummy();
	// 攻撃ごとのシルエット。クリップの姿勢へ、首・あご・尾・前脚の回転を重ねる。
	// 構えが無いとき(攻撃していないとき)も毎フレーム呼ぶこと。
	// 前のフレームで重ねた回転を取り消すのもこの中で行っている。
	void ApplyEnemyTellPose(const Combat::EnemyTellPose& tell);
	// 構えから「骨の名前 → 重ねる回転」を作る。見た目の計算だけで、メッシュには触らない。
	std::unordered_map<std::string, Matrix4x4> BuildEnemyTellRotations(
		const Combat::EnemyTellPose& tell) const;
	// 敵の口の位置を骨から求め直す。アニメーションの姿勢を確定した直後に呼ぶ。
	void UpdateEnemyMouthPosition();
	// 火球を吐く。狙いは今のプレイヤーの足元で、敵の正面から曲げられる角度に収める。
	void LaunchEnemyFireball(const enemy& source);
	// 火球を進め、着弾したら爆発・ダメージ・吹き飛ばしを出す。口に溜まる火もここで更新する。
	void UpdateEnemyFireballs(float deltaSeconds, bool playerInvincible);
	// 突進が壁にぶつかった。よろめかせ、攻撃を取り消し、破片と土煙を出す(よろめきに入れたときだけ)。
	void OnEnemyChargeHitWall(enemy& e, const Vector3& headPosition, const Vector3& forward);
	// 敵の体(骨の位置に置いた球の集まり)が闘技場の壁にめり込んでいたら、体ごと内側へ押し戻す。
	// アニメーションの姿勢を確定した後に呼ぶ。
	void PushEnemyBodyOutOfWalls();
	// このフレームで壁から押し戻した量(撮影のログ用)。
	Vector3 m_enemyWallPush{};
	// 敵の骨のワールド座標。モデルが読めていない・骨が無いときは false。
	bool GetEnemyBoneWorld(const char* bone, Vector3& out) const;
	// 敵の動きに付ける演出を出す・進める。アニメーションの姿勢を確定した後に毎フレーム呼ぶ
	// (登場演出の間も呼ぶ)。
	void UpdateEnemyMotionEffects(float deltaSeconds);
	// 足が地面を踏んだ瞬間に土煙を出す。
	void UpdateEnemyFootDust(float deltaSeconds);
	// 攻撃の通り道(尾・爪・あご)と、叩き付けが地面を打った瞬間の土煙。
	void UpdateEnemyAttackEffects();
	// 咆哮の衝撃波を seconds 秒のあいだ出し続ける。
	void StartEnemyRoarEffect(float seconds);
	// 出ている演出と攻撃の通り道を消す(リセット・タイムラインエディターを開いたとき)。
	void ClearEnemyMotionEffects();
	bool m_footLockEnabled = true;
	FootLock::Settings m_footLockSettings{};
	FootLock::FootState m_footLockLeft{};
	FootLock::FootState m_footLockRight{};
	CAnimationMesh::LegIKChain m_leftLegChain{};
	CAnimationMesh::LegIKChain m_rightLegChain{};
	bool m_legChainsResolved = false;
	// プレイヤーの描画用の接地。立ち姿勢の持ち上げ量と地面の高さ(前転中だけ体の最下点で合わせ直す)。
	float m_playerStandingGroundOffsetY = 0.0f;
	// 毎フレーム作り直す接地の持ち上げ量(変化の速さを制限するため、前の値を覚えておく)。
	float m_playerGroundOffsetY = 0.0f;
	bool m_playerGroundOffsetReady = false;
	// 足の骨から靴底までの距離。最初のフレームに1度だけ測る。
	float m_playerSoleOffsetY = 0.0f;
	bool m_playerSoleOffsetReady = false;
	float m_playerGroundY = 0.0f;
	// プレイヤーの実際の移動速度(ワールド単位/秒)。カメラの自動追従に使う。
	Vector3 m_playerWorldVelocity{ 0.0f, 0.0f, 0.0f };
	OneVsOneCombat m_combat;
	int m_lastPlayerComboStep = 0;
	bool m_lastPlayerHeavyAttack = false;
	// 先行入力。回避中に押した攻撃や、キャンセルできない攻撃中に押した回避を少しの間覚えておき、
	// 出せるようになった瞬間に出す(モンスターハンター・エルデンリングと同じ)。
	// 覚えるのは最後に押した1つだけ。
	enum class BufferedAction { None, Attack, HeavyAttack, Dodge };
	BufferedAction m_bufferedAction = BufferedAction::None;
	float m_bufferedActionTime = 0.0f;
	static constexpr float INPUT_BUFFER_SECONDS = 0.3f;
	bool m_resultRequested = false;
	bool m_enemyIntroActive = true;
	bool m_enemyRoarPlayed = false;
	bool m_lockOnTarget = false;
	float m_enemyIntroTime = 0.0f;
	Vector3 m_enemyIntroLandingPosition{ 0.0f, 0.0f, -120.0f };
	float m_playerStamina = 100.0f;
	static constexpr float PLAYER_MAX_STAMINA = 100.0f;
	int m_dodgeInvincibleStartFrame = 0;
	// 無敵は前転0.5秒のうち0〜0.27秒。0.2秒では、予兆を見て転がっても着地際に当たることが多く
	// 「ほぼ避けられない」と指摘された(ダークソウルのローリングも転がりの前半が無敵)。
	int m_dodgeInvincibleEndFrame = 16;
	int m_attackCancelEndFrame = 18;
	bool m_drawAttackCollisionDebug = false;
	bool m_drawAttackCollisionLabels = false;
	bool m_drawAttackCollisionXray = false;
	bool m_drawPlayerObb = false;
	bool m_drawEnemyObb = false;
	bool m_drawEnemyAabb = false;
	bool m_drawSwordObb = false;
	bool m_drawAttackAabb = false;
	bool m_drawLegacyPhysicsDebug = false;
	// 攻撃ごとの構えを見比べるための調整用(デバッグ表示からのみ操作する)。
	bool m_forceEnemyAttack = false;
	int m_forcedEnemyAttackIndex = 0;

	// 撮影・調整用の連続保存。dev_settings.ini の capture_dir を指定すると、
	// 戦闘開始から capture_start 秒後から capture_interval 秒ごとに capture_count 枚、
	// ゲームが描いた絵をPNGで保存する。capture_exit=1 なら撮り終えたらゲームを終える。
	// 画面を撮るのと違ってウィンドウを前面へ出さなくてよいので、PCを使っている人の邪魔をしない。
	void CaptureFrameIfRequested();
	// 撮影中は、敵の状態・位置・向きを毎フレーム capture_dir\enemy_log.csv へ書く。
	// 絵だけでは「いつ向き直ったか」「どの攻撃をどの距離で選んだか」が読み取れないため。
	void WriteCaptureLog();
	FILE* m_captureLog = nullptr;
	std::wstring m_captureDir;
	float m_captureStartSeconds = 0.0f;
	float m_captureIntervalSeconds = 0.1f;
	int m_captureCount = 0;
	int m_capturedFrames = 0;
	bool m_captureExit = false;
	// capture_intro=1 のときは、戦闘ではなく登場演出(落下・着地・咆哮)の間を撮る。時刻は演出の始まりから数える。
	bool m_captureIntro = false;
	// capture_ui=1 のときは、体力バーなどのUIも写した絵を保存する(既定は3D描画だけ)。
	bool m_captureUi = false;
	// 登場演出が終わってからの経過秒。撮影の開始時刻の基準にする。
	float m_battleSeconds = 0.0f;

	GM31::GE::Collision::BoundingSphere m_localbsplayer;	//  プレイヤBS（）ローカル座標系
	GM31::GE::Collision::BoundingSphere m_worldbsplayer;	//  プレイヤBS（）ワールド座標系
	GM31::GE::Collision::BoundingSphere m_localbsenemy;		//  敵BS（）ローカル座標系
	GM31::GE::Collision::BoundingBoxAABB m_localPlayerMeshBounds{};
	GM31::GE::Collision::BoundingBoxAABB m_localEnemyMeshBounds{};
	std::vector<wall*> m_hitWallObjects;						//  今フレーム衝突した壁
};

REGISTER_CLASS(GameScene)
