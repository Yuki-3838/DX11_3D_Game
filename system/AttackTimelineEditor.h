#pragma once

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "CombatAttackTable.h"

/**
 * @class AttackTimelineEditor
 * @brief プレイヤーの技を、タイムライン上で調整するゲーム内エディター(F6)。
 *
 * できること:
 *  - 技(弱1〜3・強1〜3・ダッシュ)を選び、クリップを差し替える
 *  - 「予兆 → 判定 → 硬直」の区切り、剣の軌跡、次の段への受付を、帯の端をドラッグして調整する
 *  - 効果音・カメラの揺れ・火花を、好きな瞬間に置く(ダブルクリックで追加、ドラッグで移動)
 *  - 再生位置を動かすと、キャラクターがその瞬間の姿勢で止まる(GameSceneが姿勢を作る)
 *  - 「剣の速さ」と「敵に当たる区間」を見ながら、判定の位置を決められる
 *  - 元に戻す/やり直し、保存(assets/data/attack_timeline.txt)
 *
 * 値は Combat::EditablePlayerAttack を直接書き換える。ゲームの攻撃も同じ値を使うので、
 * エディターを閉じればすぐに実際の攻撃で試せる。
 *
 * このクラスはUIだけを受け持つ。キャラクターの姿勢や剣の位置は3Dの側(GameScene)が計算し、
 * Analysis として渡してもらう。クリップの読み込みやイベントの再生も Host 経由で頼む。
 */
class AttackTimelineEditor
{
public:
    /** エディターからゲーム側へ頼むこと。 */
    struct Host
    {
        /// 技のクリップ(Combat::EditablePlayerAttack(slot).clipFile)を読み込み直す。読めたらtrue。
        std::function<bool(Combat::PlayerAttackSlot)> reloadClip;
        /// プレビューの再生中にイベントの時刻を通り過ぎたとき、そのイベントを起こす。
        std::function<void(const Combat::TimelineEvent&)> fireEvent;
    };

    /** 3Dの側が計算して渡す、いま選んでいる技の解析結果。 */
    struct Analysis
    {
        /// どの技の、どのクリップを解析した結果か。いま選んでいる技と違う結果は使わない
        /// (技やクリップを替えた直後の1フレームは、前の結果が渡ってくることがあるため)。
        Combat::PlayerAttackSlot slot = Combat::PlayerAttackSlot::Weak1;
        std::string clipFile;
        bool clipLoaded = false;
        float clipLength = 0.0f;          ///< クリップの長さ(秒)
        std::vector<float> sampleTimes;   ///< 下の値を取ったクリップの時刻(秒)
        std::vector<float> tipSpeeds;     ///< 剣先の速さ(ゲーム内の単位/秒)
        bool hasEnemy = false;
        std::vector<std::pair<float, float>> enemyHitRanges; ///< 剣が敵に重なっている区間(クリップの秒)
    };

    void Initialize(Host host);

    bool IsOpen() const { return m_open; }
    void Open();
    void Close() { m_open = false; m_playing = false; }
    void Toggle() { if (m_open) Close(); else Open(); }

    /** プレビューの再生を進める。毎フレーム呼ぶ。 */
    void Update(float deltaSeconds);
    /** ImGuiのウィンドウを描く。 */
    void Draw(const Analysis& analysis, bool bladeHitsEnemyNow);

    Combat::PlayerAttackSlot GetSlot() const { return m_slot; }
    /** 再生位置(クリップの秒)。 */
    float GetTime() const { return m_time; }
    bool IsPlaying() const { return m_playing; }
    float GetPlaybackSpeed() const { return m_speed; }
    /** 姿勢や剣の通り道に関わる値が変わるたびに増える。3Dの側はこれを見て解析をやり直す。 */
    int GetAnalysisRevision() const { return m_analysisRevision; }
    /** 帯やイベントをドラッグしている途中か(途中は重い解析をやり直さない)。 */
    bool IsDragging() const { return m_drag.target != DragTarget::None; }

    // 3D表示の切り替え(ツールバーのチェック)。
    bool ShowSwordPath() const { return m_showSwordPath; }
    bool ShowRootMotion() const { return m_showRootMotion; }
    bool ShowHitboxOutsideWindow() const { return m_showHitboxAlways; }
    // 敵の前に立って試す(敵に当たる区間を見るため)。間合いは敵の体の表面からの距離(ゲームの単位)。
    bool StandInFrontOfEnemy() const { return m_standInFrontOfEnemy; }
    float EnemyGap() const { return m_enemyGap; }

    /** 保存していない変更があるか。 */
    bool IsDirty() const;

    /**
     * @brief コンボを通して再生しているときの、いまの段より前に再生した段(古い順)。
     * 3Dの側は、前の段の踏み込み(使い始め→次の段へ移った時刻)を足して、いまの段を始める位置を決める。
     * 通しで再生していないとき・手で技を選んだときは空になる。
     */
    const std::vector<Combat::PlayerAttackSlot>& GetChainHistory() const { return m_chainHistory; }

    /** その技から、連打したときに続く次の技。続かなければ自分自身を返す。 */
    static Combat::PlayerAttackSlot NextComboSlot(Combat::PlayerAttackSlot slot);

private:
    enum class DragTarget
    {
        None,
        Playhead,
        ClipStart, HitStart, HitEnd, ClipEnd, HitRange,
        TrailStart, TrailEnd, TrailRange,
        ComboLink,
        Event,
    };
    struct DragState
    {
        DragTarget target = DragTarget::None;
        int eventIndex = -1;
        float grabOffset = 0.0f;   ///< つかんだ点と、動かす値の差(帯を丸ごと動かすとき)
        float rangeLength = 0.0f;  ///< 帯を丸ごと動かすときの長さ
    };
    struct Snapshot
    {
        Combat::PlayerAttackSlot slot;
        Combat::PlayerComboStep step;
    };

    Combat::PlayerComboStep& Step() { return Combat::EditablePlayerAttack(m_slot); }
    const Combat::PlayerComboStep& Step() const { return Combat::EditablePlayerAttack(m_slot); }

    void SelectSlot(Combat::PlayerAttackSlot slot);
    void ChangeSlotKeepingChain(Combat::PlayerAttackSlot slot);
    void PushUndo();
    void PushUndo(const Combat::PlayerComboStep& before);
    void Undo();
    void Redo();
    void Save();
    void Reload();
    void ResetSlotToDefault();
    void MarkEdited(bool affectsPose);
    void Clamp(Combat::PlayerComboStep& step) const;
    float ClipLength() const;
    float FrameStep() const;                ///< 1ゲームフレーム分のクリップの秒
    float Snap(float clipSeconds) const;
    int GameFrameOf(float clipSeconds) const;
    void SetTime(float clipSeconds);
    void StepFrames(int frames);
    void TogglePlay();
    void FitView();
    void AddEvent(Combat::TimelineEventType type, float clipSeconds);
    void DeleteSelectedEvent();
    // 剣の速さが最大の半分を超える区間(=振っている所)に、判定を置く。
    void FitHitWindowToSwing(const Analysis& analysis);
    void HandleShortcuts();
    void SetStatus(const std::string& message, bool error = false);

    void DrawToolbar();
    void DrawSummary(bool bladeHitsEnemyNow);
    void DrawTimeline(const Analysis& analysis);
    void DrawProperties(const Analysis& analysis);
    void DrawClipCombo();
    void RefreshClipList();

    // 数値の欄を編集したときに、元に戻す履歴を1回だけ積む。
    template <typename Widget>
    bool EditField(bool affectsPose, Widget widget);

    Host m_host;
    bool m_open = false;
    Combat::PlayerAttackSlot m_slot = Combat::PlayerAttackSlot::Weak1;
    float m_time = 0.0f;
    bool m_playing = false;
    float m_speed = 0.5f;
    bool m_loop = true;
    // コンボを通して再生する(次の段への受付の時刻で、次の段へ移る)。
    bool m_chainPlay = false;
    std::vector<Combat::PlayerAttackSlot> m_chainHistory;
    bool m_snap = true;
    bool m_showSwordPath = true;
    bool m_showRootMotion = true;
    bool m_showHitboxAlways = false;
    bool m_standInFrontOfEnemy = true;
    float m_enemyGap = 15.0f;
    float m_viewStart = 0.0f;
    float m_viewEnd = 2.0f;
    bool m_fitPending = true;
    DragState m_drag;
    int m_selectedEvent = -1;
    bool m_fieldEditing = false;
    int m_analysisRevision = 0;
    float m_lastClipLength = 0.0f;

    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;

    std::vector<std::string> m_clipFiles;
    std::string m_status;
    bool m_statusError = false;
    double m_statusTime = 0.0;
};
