#include "AttackTimelineEditor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>

#include "AttackTimelineFile.h"
#include "imgui/imgui.h"

namespace
{
// --- 色 ---
// 予兆(青)・判定(赤)・硬直(灰)は、エディターと3D表示(剣の通り道)で同じ色にそろえる。
constexpr ImU32 COLOR_WINDUP = IM_COL32(70, 130, 220, 255);
constexpr ImU32 COLOR_ACTIVE = IM_COL32(232, 72, 60, 255);
constexpr ImU32 COLOR_RECOVERY = IM_COL32(112, 116, 128, 255);
constexpr ImU32 COLOR_TRAIL = IM_COL32(240, 200, 90, 255);
constexpr ImU32 COLOR_LINK = IM_COL32(80, 220, 230, 255);
constexpr ImU32 COLOR_ENEMY_HIT = IM_COL32(90, 220, 110, 255);
constexpr ImU32 COLOR_SPEED = IM_COL32(200, 205, 215, 255);
constexpr ImU32 COLOR_PLAYHEAD = IM_COL32(255, 255, 255, 255);
constexpr ImU32 COLOR_ROW_BG = IM_COL32(30, 33, 40, 255);
constexpr ImU32 COLOR_ROW_BG_ALT = IM_COL32(36, 39, 47, 255);
constexpr ImU32 COLOR_OUTSIDE = IM_COL32(0, 0, 0, 120);
constexpr ImU32 COLOR_TEXT = IM_COL32(225, 228, 235, 255);
constexpr ImU32 COLOR_TEXT_DIM = IM_COL32(150, 155, 165, 255);

ImU32 EventColor(Combat::TimelineEventType type)
{
    switch (type)
    {
    case Combat::TimelineEventType::Sound:       return IM_COL32(190, 130, 250, 255);
    case Combat::TimelineEventType::CameraShake: return IM_COL32(250, 150, 60, 255);
    case Combat::TimelineEventType::Sparks:      return IM_COL32(255, 225, 80, 255);
    default:                                     return IM_COL32(255, 255, 255, 255);
    }
}

const char* EventTypeLabel(Combat::TimelineEventType type)
{
    switch (type)
    {
    case Combat::TimelineEventType::Sound:       return "効果音";
    case Combat::TimelineEventType::CameraShake: return "カメラの揺れ";
    case Combat::TimelineEventType::Sparks:      return "火花";
    default:                                     return "";
    }
}

const char* SoundLabel(Combat::TimelineSound sound)
{
    switch (sound)
    {
    case Combat::TimelineSound::Whoosh:       return "風切り音";
    case Combat::TimelineSound::SwordHit:     return "斬撃音";
    case Combat::TimelineSound::DragonAttack: return "竜の攻撃";
    case Combat::TimelineSound::Roar:         return "咆哮";
    default:                                  return "";
    }
}

bool SameEvent(const Combat::TimelineEvent& a, const Combat::TimelineEvent& b)
{
    return a.time == b.time && a.type == b.type && a.sound == b.sound &&
        a.strength == b.strength && a.duration == b.duration && a.count == b.count;
}

bool SameStep(const Combat::PlayerComboStep& a, const Combat::PlayerComboStep& b)
{
    if (a.clipFile != b.clipFile || a.clipStart != b.clipStart || a.hitStart != b.hitStart ||
        a.hitEnd != b.hitEnd || a.clipEnd != b.clipEnd || a.playbackRate != b.playbackRate ||
        a.comboLinkSeconds != b.comboLinkSeconds || a.rootMotionScale != b.rootMotionScale ||
        a.trailStart != b.trailStart || a.trailEnd != b.trailEnd ||
        a.events.size() != b.events.size())
        return false;
    for (std::size_t i = 0; i < a.events.size(); ++i)
        if (!SameEvent(a.events[i], b.events[i]))
            return false;
    return true;
}

std::string ClipDisplayName(const std::string& path)
{
    const std::filesystem::path file = AttackTimelineFile::PathFromUtf8(path);
    const auto stem = file.stem().u8string();
    return std::string(stem.begin(), stem.end());
}

// トラックの並び。上から順に描く。
enum class Row
{
    Ruler,
    Phases,
    Trail,
    ComboLink,
    EnemyHit,
    Speed,
    Sound,
    Shake,
    Sparks,
    Count
};

struct RowLayout
{
    const char* label;
    float height;
};

constexpr RowLayout ROWS[static_cast<int>(Row::Count)] = {
    { "", 22.0f },
    { "予兆・判定・硬直", 30.0f },
    { "剣の軌跡", 22.0f },
    { "次の段へ", 22.0f },
    { "敵に当たる", 18.0f },
    { "剣の速さ", 40.0f },
    { "効果音", 22.0f },
    { "カメラの揺れ", 22.0f },
    { "火花", 22.0f },
};

Row RowOfEvent(Combat::TimelineEventType type)
{
    switch (type)
    {
    case Combat::TimelineEventType::Sound:       return Row::Sound;
    case Combat::TimelineEventType::CameraShake: return Row::Shake;
    default:                                     return Row::Sparks;
    }
}

constexpr float LABEL_WIDTH = 118.0f;
constexpr float HANDLE_GRAB_PIXELS = 6.0f;
} // namespace

void AttackTimelineEditor::Initialize(Host host)
{
    m_host = std::move(host);
    RefreshClipList();
}

void AttackTimelineEditor::Open()
{
    m_open = true;
    m_playing = false;
    m_fitPending = true;
    SetTime(Step().clipStart);
    ++m_analysisRevision;
}

bool AttackTimelineEditor::IsDirty() const
{
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
        if (!SameStep(AttackTimelineFile::SavedState()[i], Combat::EditablePlayerAttack(static_cast<Combat::PlayerAttackSlot>(i))))
            return true;
    return false;
}

void AttackTimelineEditor::Update(float deltaSeconds)
{
    if (!m_open || !m_playing)
        return;
    const Combat::PlayerComboStep& step = Step();
    // 再生はゲームと同じ速さ(クリップの再生速度)に、プレビューの速さ(スロー)を掛けて進める。
    const float previous = m_time;
    float next = m_time + deltaSeconds * step.playbackRate * m_speed;
    // 通り過ぎたイベントを起こす(再生中だけ。再生位置をドラッグしているときは起こさない)。
    const auto fireBetween = [this, &step](float from, float to) {
        if (!m_host.fireEvent)
            return;
        for (const Combat::TimelineEvent& event : step.events)
            if (event.time > from && event.time <= to)
                m_host.fireEvent(event);
    };

    // コンボを通して再生: 次の段への受付の時刻で、ゲームと同じく次の段へ移る
    // (ゲームでは連打していればこの時刻に次の段が始まる)。
    const Combat::PlayerAttackSlot nextSlot = NextComboSlot(m_slot);
    const float linkTime = std::min(step.ComboLinkClipTime(), step.clipEnd);
    if (m_chainPlay && nextSlot != m_slot && next >= linkTime && previous < linkTime)
    {
        fireBetween(previous, linkTime);
        m_chainHistory.push_back(m_slot);
        ChangeSlotKeepingChain(nextSlot);
        return;
    }

    const float end = step.clipEnd;
    if (next >= end)
    {
        fireBetween(previous, end);
        if (!m_loop)
        {
            m_time = end;
            m_playing = false;
            return;
        }
        // くり返し。通しで再生していたなら、最初の段へ戻る。
        if (!m_chainHistory.empty())
        {
            const Combat::PlayerAttackSlot first = m_chainHistory.front();
            m_chainHistory.clear();
            ChangeSlotKeepingChain(first);
        }
        else
            m_time = step.clipStart;
        return;
    }
    fireBetween(previous, next);
    m_time = next;
}

Combat::PlayerAttackSlot AttackTimelineEditor::NextComboSlot(Combat::PlayerAttackSlot slot)
{
    using Slot = Combat::PlayerAttackSlot;
    switch (slot)
    {
    case Slot::Weak1: return Slot::Weak2;
    case Slot::Weak2: return Slot::Weak3;
    case Slot::Heavy1: return Slot::Heavy2;
    case Slot::Heavy2: return Slot::Heavy3;
    // ダッシュ攻撃は弱攻撃1段目の代わりに出るので、続きは弱攻撃の2段目(OneVsOneCombatと同じ)。
    case Slot::Dash: return Slot::Weak2;
    default: return slot;
    }
}

void AttackTimelineEditor::ChangeSlotKeepingChain(Combat::PlayerAttackSlot slot)
{
    // SelectSlot と違い、再生を止めず、通しで再生した段の履歴も消さない。
    m_slot = slot;
    m_selectedEvent = -1;
    m_fitPending = true;
    m_lastClipLength = 0.0f;
    m_time = Step().clipStart;
    ++m_analysisRevision;
}

// ------------------------------------------------------------
// 編集の土台(履歴・制約・保存)
// ------------------------------------------------------------

void AttackTimelineEditor::PushUndo()
{
    PushUndo(Step());
}

void AttackTimelineEditor::PushUndo(const Combat::PlayerComboStep& before)
{
    m_undo.push_back({ m_slot, before });
    if (m_undo.size() > 300)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
}

void AttackTimelineEditor::Undo()
{
    if (m_undo.empty())
        return;
    const Snapshot snapshot = m_undo.back();
    m_undo.pop_back();
    Combat::PlayerComboStep& target = Combat::EditablePlayerAttack(snapshot.slot);
    m_redo.push_back({ snapshot.slot, target });
    const bool clipChanged = target.clipFile != snapshot.step.clipFile;
    target = snapshot.step;
    if (m_slot != snapshot.slot)
        SelectSlot(snapshot.slot);
    if (clipChanged && m_host.reloadClip)
        m_host.reloadClip(snapshot.slot);
    m_selectedEvent = -1;
    MarkEdited(true);
    SetStatus("元に戻しました");
}

void AttackTimelineEditor::Redo()
{
    if (m_redo.empty())
        return;
    const Snapshot snapshot = m_redo.back();
    m_redo.pop_back();
    Combat::PlayerComboStep& target = Combat::EditablePlayerAttack(snapshot.slot);
    m_undo.push_back({ snapshot.slot, target });
    const bool clipChanged = target.clipFile != snapshot.step.clipFile;
    target = snapshot.step;
    if (m_slot != snapshot.slot)
        SelectSlot(snapshot.slot);
    if (clipChanged && m_host.reloadClip)
        m_host.reloadClip(snapshot.slot);
    m_selectedEvent = -1;
    MarkEdited(true);
    SetStatus("やり直しました");
}

void AttackTimelineEditor::Save()
{
    if (AttackTimelineFile::Save())
    {
        char clock[16]{};
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        std::strftime(clock, sizeof(clock), "%H:%M:%S", &local);
        SetStatus(std::string("保存しました(") + clock + ") " + AttackTimelineFile::DefaultPath());
    }
    else
    {
        SetStatus(std::string("保存できませんでした: ") + AttackTimelineFile::DefaultPath(), true);
    }
}

void AttackTimelineEditor::Reload()
{
    std::array<std::string, Combat::PLAYER_ATTACK_SLOT_COUNT> clips;
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
        clips[i] = Combat::EditablePlayerAttack(static_cast<Combat::PlayerAttackSlot>(i)).clipFile;
    if (!AttackTimelineFile::Load())
    {
        SetStatus("ファイルがありません(まだ一度も保存していません)", true);
        return;
    }
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
    {
        const auto slot = static_cast<Combat::PlayerAttackSlot>(i);
        if (clips[i] != Combat::EditablePlayerAttack(slot).clipFile && m_host.reloadClip)
            m_host.reloadClip(slot);
    }
    m_undo.clear();
    m_redo.clear();
    m_selectedEvent = -1;
    MarkEdited(true);
    SetStatus("ファイルから読み直しました");
}

void AttackTimelineEditor::ResetSlotToDefault()
{
    PushUndo();
    const bool clipChanged = Step().clipFile != Combat::DefaultPlayerAttack(m_slot).clipFile;
    Step() = Combat::DefaultPlayerAttack(m_slot);
    if (clipChanged && m_host.reloadClip)
        m_host.reloadClip(m_slot);
    m_selectedEvent = -1;
    MarkEdited(true);
    SetStatus(std::string(Combat::PlayerAttackSlotLabel(m_slot)) + " を既定の値に戻しました");
}

void AttackTimelineEditor::MarkEdited(bool affectsPose)
{
    Clamp(Step());
    if (affectsPose)
        ++m_analysisRevision;
}

void AttackTimelineEditor::Clamp(Combat::PlayerComboStep& step) const
{
    // クリップの長さがまだ分からない(解析が届く前の)うちは、長さで切り詰めない。
    // 切り詰めると「使い終わり」より後ろのイベントなどを壊してしまう。
    const float length = m_lastClipLength > 0.0f ? m_lastClipLength : 1000.0f;
    step.playbackRate = std::clamp(step.playbackRate, 0.1f, 4.0f);
    step.rootMotionScale = std::clamp(step.rootMotionScale, 0.0f, 4.0f);
    step.comboLinkSeconds = std::max(0.0f, step.comboLinkSeconds);
    // 区切りの順番(使い始め ≦ 判定の始まり ≦ 判定の終わり ≦ 使い終わり)を守る。
    step.clipStart = std::clamp(step.clipStart, 0.0f, length);
    step.clipEnd = std::clamp(step.clipEnd, step.clipStart, length);
    step.hitStart = std::clamp(step.hitStart, step.clipStart, step.clipEnd);
    step.hitEnd = std::clamp(step.hitEnd, step.hitStart, step.clipEnd);
    if (step.trailStart >= 0.0f || step.trailEnd >= 0.0f)
    {
        step.trailStart = std::clamp(std::max(step.trailStart, 0.0f), 0.0f, length);
        step.trailEnd = std::clamp(std::max(step.trailEnd, step.trailStart), step.trailStart, length);
    }
    for (Combat::TimelineEvent& event : step.events)
    {
        event.time = std::clamp(event.time, 0.0f, length);
        event.strength = std::clamp(event.strength, 0.0f, 10.0f);
        event.duration = std::clamp(event.duration, 0.01f, 3.0f);
        event.count = std::clamp(event.count, 1, 96);
    }
}

float AttackTimelineEditor::ClipLength() const
{
    if (m_lastClipLength > 0.0f)
        return m_lastClipLength;
    return std::max(Step().clipEnd, 0.1f);
}

float AttackTimelineEditor::FrameStep() const
{
    // 1ゲームフレーム(1/60秒)の間に進むクリップの秒。
    return Step().playbackRate / 60.0f;
}

float AttackTimelineEditor::Snap(float clipSeconds) const
{
    if (!m_snap || ImGui::GetIO().KeyAlt)
        return clipSeconds;
    // ゲームでは技の使い始めから1/60秒ごとに姿勢が進むので、その刻みへ吸着させる。
    const float origin = Step().clipStart;
    const float frame = FrameStep();
    return origin + std::round((clipSeconds - origin) / frame) * frame;
}

int AttackTimelineEditor::GameFrameOf(float clipSeconds) const
{
    return static_cast<int>(std::lround((clipSeconds - Step().clipStart) / FrameStep()));
}

void AttackTimelineEditor::SetTime(float clipSeconds)
{
    m_time = std::clamp(clipSeconds, 0.0f, ClipLength());
}

void AttackTimelineEditor::StepFrames(int frames)
{
    m_playing = false;
    SetTime(Snap(m_time + static_cast<float>(frames) * FrameStep()));
}

void AttackTimelineEditor::TogglePlay()
{
    m_playing = !m_playing;
    if (m_playing && (m_time >= Step().clipEnd - 0.0001f || m_time < Step().clipStart))
        m_time = Step().clipStart;
}

void AttackTimelineEditor::FitView()
{
    const float length = ClipLength();
    const float margin = length * 0.03f;
    m_viewStart = -margin;
    m_viewEnd = length + margin;
}

void AttackTimelineEditor::SelectSlot(Combat::PlayerAttackSlot slot)
{
    // 手で技を選んだら、通しで再生した段の履歴は捨てる(その技を、技を始める位置から見る)。
    m_chainHistory.clear();
    m_slot = slot;
    m_selectedEvent = -1;
    m_playing = false;
    m_fitPending = true;
    m_lastClipLength = 0.0f;
    SetTime(Step().clipStart);
    ++m_analysisRevision;
}

void AttackTimelineEditor::AddEvent(Combat::TimelineEventType type, float clipSeconds)
{
    PushUndo();
    Combat::TimelineEvent event;
    event.type = type;
    event.time = Snap(clipSeconds);
    switch (type)
    {
    case Combat::TimelineEventType::Sound:
        event.sound = Combat::TimelineSound::Whoosh;
        break;
    case Combat::TimelineEventType::CameraShake:
        event.strength = 1.2f;
        event.duration = 0.2f;
        break;
    case Combat::TimelineEventType::Sparks:
        event.count = 14;
        event.strength = 1.0f;
        break;
    default:
        break;
    }
    Step().events.push_back(event);
    m_selectedEvent = static_cast<int>(Step().events.size()) - 1;
    MarkEdited(false);
    SetStatus(std::string(EventTypeLabel(type)) + " を追加しました");
}

void AttackTimelineEditor::DeleteSelectedEvent()
{
    auto& events = Step().events;
    if (m_selectedEvent < 0 || m_selectedEvent >= static_cast<int>(events.size()))
        return;
    PushUndo();
    events.erase(events.begin() + m_selectedEvent);
    m_selectedEvent = -1;
    MarkEdited(false);
    SetStatus("イベントを消しました");
}

void AttackTimelineEditor::FitHitWindowToSwing(const Analysis& analysis)
{
    // 既定の表の値と同じ考え方で置く: 振っている区間 = 剣の速さが最大の50%を超える区間。
    // 「使う範囲」の中で一番速い瞬間を含む、ひと続きの区間を探す。
    const Combat::PlayerComboStep& step = Step();
    const auto& times = analysis.sampleTimes;
    const auto& speeds = analysis.tipSpeeds;
    int peak = -1;
    for (int i = 0; i < static_cast<int>(speeds.size()); ++i)
    {
        if (times[i] < step.clipStart || times[i] > step.clipEnd)
            continue;
        if (peak < 0 || speeds[i] > speeds[peak])
            peak = i;
    }
    if (peak < 0 || speeds[peak] <= 0.0f)
    {
        SetStatus("剣の速さを計算できていないので、判定を置けませんでした", true);
        return;
    }
    const float threshold = speeds[peak] * 0.5f;
    int first = peak;
    int last = peak;
    while (first > 0 && speeds[first - 1] >= threshold && times[first - 1] >= step.clipStart)
        --first;
    while (last + 1 < static_cast<int>(speeds.size()) && speeds[last + 1] >= threshold && times[last + 1] <= step.clipEnd)
        ++last;
    PushUndo();
    // 前後に1フレームずつ余裕を持たせる(振り始めと振り終わりの剣先も拾うため)。
    Combat::PlayerComboStep& editable = Step();
    editable.hitStart = Snap(std::max(step.clipStart, times[first] - FrameStep()));
    editable.hitEnd = Snap(std::min(step.clipEnd, times[last] + FrameStep()));
    MarkEdited(false);
    char text[160]{};
    std::snprintf(text, sizeof(text), "剣が速い区間(最速の半分以上)に判定を置きました: %dF〜%dF",
        GameFrameOf(editable.hitStart), GameFrameOf(editable.hitEnd));
    SetStatus(text);
}

void AttackTimelineEditor::SetStatus(const std::string& message, bool error)
{
    m_status = message;
    m_statusError = error;
    m_statusTime = ImGui::GetTime();
}

template <typename Widget>
bool AttackTimelineEditor::EditField(bool affectsPose, Widget widget)
{
    const Combat::PlayerComboStep before = Step();
    const bool changed = widget();
    // ドラッグで値を動かしている間は毎フレーム変わるが、履歴は動かし始めの1回だけ積む。
    if (changed && !m_fieldEditing)
    {
        PushUndo(before);
        m_fieldEditing = true;
    }
    if (ImGui::IsItemDeactivated())
        m_fieldEditing = false;
    if (changed)
        MarkEdited(affectsPose);
    return changed;
}

void AttackTimelineEditor::RefreshClipList()
{
    m_clipFiles.clear();
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("assets/motion", error))
    {
        if (!entry.is_regular_file())
            continue;
        auto extension = entry.path().extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](char8_t c) { return static_cast<char8_t>(std::tolower(static_cast<unsigned char>(c))); });
        if (extension != u8".fbx")
            continue;
        const auto relative = (std::filesystem::path("assets/motion") / entry.path().filename()).generic_u8string();
        m_clipFiles.emplace_back(relative.begin(), relative.end());
    }
    // 攻撃らしいクリップ(attack / slash / kick)を先に並べる。
    const auto isAttack = [](const std::string& file) {
        return file.find("attack") != std::string::npos || file.find("slash") != std::string::npos ||
            file.find("kick") != std::string::npos;
    };
    std::stable_sort(m_clipFiles.begin(), m_clipFiles.end(),
        [&isAttack](const std::string& a, const std::string& b) {
            if (isAttack(a) != isAttack(b))
                return isAttack(a);
            return a < b;
        });
}

// ------------------------------------------------------------
// 描画
// ------------------------------------------------------------

void AttackTimelineEditor::Draw(const Analysis& receivedAnalysis, bool bladeHitsEnemyNow)
{
    if (!m_open)
        return;

    // いま選んでいる技・クリップの解析結果でなければ使わない(無いものとして描く)。
    // 前の技の結果をそのまま使うと、前のクリップの長さで値を切り詰めて壊してしまう。
    static const Analysis noAnalysis;
    const bool fresh = receivedAnalysis.slot == m_slot && receivedAnalysis.clipFile == Step().clipFile;
    const Analysis& analysis = fresh ? receivedAnalysis : noAnalysis;

    if (analysis.clipLoaded && analysis.clipLength > 0.0f && analysis.clipLength != m_lastClipLength)
    {
        m_lastClipLength = analysis.clipLength;
        Clamp(Step());
    }
    if (m_fitPending && m_lastClipLength > 0.0f)
    {
        FitView();
        m_fitPending = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = 398.0f;
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height), ImGuiCond_Appearing);
    std::string title = std::string("攻撃のタイムライン") + (IsDirty() ? " *" : "") + "###AttackTimeline";
    bool open = true;
    if (!ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        if (!open)
            Close();
        return;
    }
    if (!open)
    {
        ImGui::End();
        Close();
        return;
    }

    HandleShortcuts();
    DrawToolbar();
    DrawSummary(bladeHitsEnemyNow);

    const float propertiesWidth = 300.0f;
    const float timelineWidth = std::max(200.0f, ImGui::GetContentRegionAvail().x - propertiesWidth - 8.0f);
    ImGui::BeginChild("timeline", ImVec2(timelineWidth, 0.0f), ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawTimeline(analysis);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("properties", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    DrawProperties(analysis);
    ImGui::EndChild();

    ImGui::End();
}

void AttackTimelineEditor::HandleShortcuts()
{
    // エディターを開いている間はゲームを止めているので、キーはすべてエディターが使ってよい。
    // ウィンドウを一度クリックしないと効かない、という作りにはしない(開いてすぐ Space で再生できるように)。
    // 文字を打ち込んでいる間(数値の欄など)だけは、そちらへ譲る。
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (shift) Redo(); else Undo();
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { Save(); return; }
    if (ctrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) TogglePlay();
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) StepFrames(shift ? -10 : -1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) StepFrames(shift ? 10 : 1);
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) { m_playing = false; SetTime(Step().clipStart); }
    if (ImGui::IsKeyPressed(ImGuiKey_End, false)) { m_playing = false; SetTime(Step().clipEnd); }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        DeleteSelectedEvent();
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FitView();
    // [ ] で、判定の始まり・終わりを再生位置へ置く。
    // 剣が一番速く動いている瞬間などを再生位置で探し、そのまま区切りを置けるようにする。
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false))
    {
        PushUndo();
        Step().hitStart = std::min(Snap(m_time), Step().hitEnd);
        MarkEdited(false);
        SetStatus("判定の始まりを再生位置に置きました");
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false))
    {
        PushUndo();
        Step().hitEnd = std::max(Snap(m_time), Step().hitStart);
        MarkEdited(false);
        SetStatus("判定の終わりを再生位置に置きました");
    }
    // 1〜3でイベントを再生位置に追加する。
    if (ImGui::IsKeyPressed(ImGuiKey_1, false)) AddEvent(Combat::TimelineEventType::Sound, m_time);
    if (ImGui::IsKeyPressed(ImGuiKey_2, false)) AddEvent(Combat::TimelineEventType::CameraShake, m_time);
    if (ImGui::IsKeyPressed(ImGuiKey_3, false)) AddEvent(Combat::TimelineEventType::Sparks, m_time);
}

void AttackTimelineEditor::DrawToolbar()
{
    // 技の選択。
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
    {
        const auto slot = static_cast<Combat::PlayerAttackSlot>(i);
        if (i == 3 || i == 6)
            ImGui::SameLine(0.0f, 14.0f);
        else if (i > 0)
            ImGui::SameLine();
        const bool selected = slot == m_slot;
        const bool edited = !SameStep(AttackTimelineFile::SavedState()[i], Combat::EditablePlayerAttack(slot));
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        std::string label = std::string(Combat::PlayerAttackSlotLabel(slot)) + (edited ? "*" : "");
        if (ImGui::Button(label.c_str(), ImVec2(i == 6 ? 72.0f : 46.0f, 0.0f)) && !selected)
            SelectSlot(slot);
        if (selected)
            ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 18.0f);
    DrawClipCombo();

    ImGui::SameLine(0.0f, 18.0f);
    if (ImGui::Button("保存"))
        Save();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+S。%s へ書き込む", AttackTimelineFile::DefaultPath());
    ImGui::SameLine();
    ImGui::BeginDisabled(m_undo.empty());
    if (ImGui::Button("元に戻す"))
        Undo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Ctrl+Z");
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redo.empty());
    if (ImGui::Button("やり直し"))
        Redo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Ctrl+Y / Ctrl+Shift+Z");
    ImGui::SameLine();
    if (ImGui::Button("..."))
        ImGui::OpenPopup("timeline_menu");
    if (ImGui::BeginPopup("timeline_menu"))
    {
        if (ImGui::MenuItem("この技を既定の値に戻す"))
            ResetSlotToDefault();
        if (ImGui::MenuItem("ファイルから読み直す(保存していない変更は消える)"))
            Reload();
        if (ImGui::MenuItem("クリップの一覧を読み直す"))
            RefreshClipList();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(
            "マウス\n"
            "  帯の端をドラッグ: 区切りを動かす / 帯の中をドラッグ: 丸ごと動かす\n"
            "  目盛りをクリック・ドラッグ: 再生位置を動かす\n"
            "  イベントの行をダブルクリック: その位置にイベントを追加\n"
            "  イベントを右クリック: 複製・削除\n"
            "  Ctrl+ホイール: 拡大・縮小 / ホイール: 左右へ送る\n"
            "  Altを押しながらドラッグ: フレームへの吸着を外す\n"
            "  3D画面: 右ドラッグで回り込む / ホイールで寄る・引く\n"
            "キー\n"
            "  Space: 再生・停止 / ← →: 1フレーム送る(Shiftで10) / Home End: 始め・終わり\n"
            "  [ ]: 判定の始まり・終わりを再生位置に置く\n"
            "  1 2 3: 効果音・カメラの揺れ・火花を再生位置に追加 / Delete: 選んだイベントを消す\n"
            "  F: 全体を表示 / Ctrl+Z Ctrl+Y: 元に戻す・やり直し / Ctrl+S: 保存 / F6: 閉じる");
        ImGui::EndTooltip();
    }

    // 再生の操作。
    if (ImGui::Button("|<"))
    {
        m_playing = false;
        // 通しで再生していたなら、最初の段の始めへ戻る。
        if (!m_chainHistory.empty())
        {
            const Combat::PlayerAttackSlot first = m_chainHistory.front();
            m_chainHistory.clear();
            ChangeSlotKeepingChain(first);
        }
        SetTime(Step().clipStart);
    }
    ImGui::SameLine();
    if (ImGui::Button("<"))
        StepFrames(-1);
    ImGui::SameLine();
    if (ImGui::Button(m_playing ? "停止" : "再生", ImVec2(52.0f, 0.0f)))
        TogglePlay();
    ImGui::SameLine();
    if (ImGui::Button(">"))
        StepFrames(1);
    ImGui::SameLine();
    if (ImGui::Button(">|"))
    {
        m_playing = false;
        SetTime(Step().clipEnd);
    }
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::TextUnformatted("速さ");
    static constexpr float speeds[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    static constexpr const char* speedLabels[] = { "0.1倍", "0.25倍", "0.5倍", "1倍" };
    for (int i = 0; i < 4; ++i)
    {
        ImGui::SameLine();
        if (ImGui::RadioButton(speedLabels[i], m_speed == speeds[i]))
            m_speed = speeds[i];
    }
    ImGui::SameLine(0.0f, 14.0f);
    if (ImGui::Checkbox("コンボで続ける", &m_chainPlay) && !m_chainPlay && !m_chainHistory.empty())
    {
        // 切ったら、いま見ている段を単独で見る位置へ戻す。
        m_chainHistory.clear();
        ++m_analysisRevision;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("次の段への受付(水色の印)の時刻で、次の段へ移って再生する(連打したときのゲームと同じ)。\n弱1→弱2→弱3 / 強1→強2→強3 / ダッシュ→弱2→弱3。踏み込みもつながる");
    ImGui::SameLine();
    ImGui::Checkbox("くり返す", &m_loop);
    ImGui::SameLine();
    ImGui::Checkbox("フレームに吸着", &m_snap);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("区切りやイベントを、ゲームの1フレーム(1/60秒)の刻みに合わせる。Altを押している間は外れる");

    // 3D表示の切り替え(1行に収まらないので段を分ける)。
    ImGui::TextDisabled("表示");
    ImGui::SameLine();
    if (ImGui::Checkbox("剣の通り道", &m_showSwordPath))
        ++m_analysisRevision;
    ImGui::SameLine();
    if (ImGui::Checkbox("踏み込み", &m_showRootMotion))
        ++m_analysisRevision;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("クリップの腰の移動どおりにキャラクターを前へ出す(ゲームと同じ)");
    ImGui::SameLine();
    ImGui::Checkbox("判定外も剣の箱を出す", &m_showHitboxAlways);
    ImGui::SameLine(0.0f, 14.0f);
    if (ImGui::Checkbox("敵の前に立つ", &m_standInFrontOfEnemy))
        ++m_analysisRevision;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("敵の方を向いて、右の間合いの位置に立つ。「敵に当たる」の行で、届く瞬間が分かる");
    if (m_standInFrontOfEnemy)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        char format[48]{};
        std::snprintf(format, sizeof(format), "間合い %%.0f (%.1fマス)", m_enemyGap / 10.0f);
        ImGui::SliderFloat("##gap", &m_enemyGap, 0.0f, 60.0f, format);
        // 動かし終えたときだけ解析し直す(動かしている間は姿勢だけ追う)。
        if (ImGui::IsItemDeactivatedAfterEdit())
            ++m_analysisRevision;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("敵の体の表面から、プレイヤーまでの距離。1マス=10(約1m)");
    }
}

void AttackTimelineEditor::DrawClipCombo()
{
    Combat::PlayerComboStep& step = Step();
    ImGui::SetNextItemWidth(260.0f);
    const std::string current = ClipDisplayName(step.clipFile);
    if (ImGui::BeginCombo("クリップ", current.c_str(), ImGuiComboFlags_HeightLarge))
    {
        for (const std::string& file : m_clipFiles)
        {
            const bool selected = file == step.clipFile;
            if (ImGui::Selectable(ClipDisplayName(file).c_str(), selected) && !selected)
            {
                PushUndo();
                const std::string previous = step.clipFile;
                step.clipFile = file;
                const bool loaded = m_host.reloadClip ? m_host.reloadClip(m_slot) : false;
                if (loaded)
                {
                    m_lastClipLength = 0.0f;
                    m_fitPending = true;
                    MarkEdited(true);
                    SetStatus("クリップを " + ClipDisplayName(file) + " に替えました。判定の位置を見直してください");
                }
                else
                {
                    step.clipFile = previous;
                    m_undo.pop_back();
                    if (m_host.reloadClip)
                        m_host.reloadClip(m_slot);
                    SetStatus("クリップを読み込めませんでした: " + file, true);
                }
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void AttackTimelineEditor::DrawSummary(bool bladeHitsEnemyNow)
{
    const Combat::PlayerComboStep& step = Step();
    const auto frames = [](float seconds) { return static_cast<int>(std::lround(seconds * 60.0f)); };
    const char* phase = m_time < step.clipStart ? "使う範囲の前"
        : m_time < step.hitStart ? "予兆"
        : m_time <= step.hitEnd ? "判定中"
        : m_time <= step.clipEnd ? "硬直"
        : "使う範囲の後";
    ImGui::Text("予兆 %dF  判定 %dF  硬直 %dF  全体 %dF (%.2f秒)",
        frames(step.WindupSeconds()), frames(step.ActiveSeconds()),
        frames(step.RecoverySeconds()), frames(step.TotalSeconds()), step.TotalSeconds());
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::Text("再生位置: クリップ %.3f秒 / %dF目 [%s]", m_time, GameFrameOf(m_time), phase);
    if (m_time >= step.hitStart && m_time <= step.hitEnd)
    {
        ImGui::SameLine(0.0f, 16.0f);
        if (bladeHitsEnemyNow)
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.45f, 1.0f), "敵に当たっている");
        else
            ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), "敵には届いていない");
    }
    // 直前の操作の結果。数秒で薄くする。
    if (!m_status.empty())
    {
        const float age = static_cast<float>(ImGui::GetTime() - m_statusTime);
        if (age < 6.0f)
        {
            const float alpha = std::clamp(1.0f - (age - 4.0f) / 2.0f, 0.0f, 1.0f);
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::TextColored(
                m_statusError ? ImVec4(1.0f, 0.45f, 0.4f, alpha) : ImVec4(0.6f, 0.9f, 1.0f, alpha),
                "%s", m_status.c_str());
        }
    }
}

void AttackTimelineEditor::DrawTimeline(const Analysis& analysis)
{
    Combat::PlayerComboStep& step = Step();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    float totalHeight = 0.0f;
    for (const RowLayout& row : ROWS)
        totalHeight += row.height;
    const ImVec2 size(width, totalHeight);

    // 操作はこの見えないボタン1つで受け、何をつかんだかは自分で判定する。
    ImGui::InvisibleButton("timeline_canvas", size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImGuiIO& io = ImGui::GetIO();

    const float trackLeft = origin.x + LABEL_WIDTH;
    const float trackRight = origin.x + width - 6.0f;
    const float trackWidth = std::max(trackRight - trackLeft, 10.0f);
    if (m_viewEnd - m_viewStart < 0.02f)
        m_viewEnd = m_viewStart + 0.02f;
    const auto timeToX = [&](float t) {
        return trackLeft + (t - m_viewStart) / (m_viewEnd - m_viewStart) * trackWidth;
    };
    const auto xToTime = [&](float x) {
        return m_viewStart + (x - trackLeft) / trackWidth * (m_viewEnd - m_viewStart);
    };
    float rowTop[static_cast<int>(Row::Count) + 1]{};
    rowTop[0] = origin.y;
    for (int i = 0; i < static_cast<int>(Row::Count); ++i)
        rowTop[i + 1] = rowTop[i] + ROWS[i].height;
    const auto rowY = [&](Row row) { return rowTop[static_cast<int>(row)]; };
    const auto rowH = [&](Row row) { return ROWS[static_cast<int>(row)].height; };
    const auto rowMid = [&](Row row) { return rowY(row) + rowH(row) * 0.5f; };

    // --- 拡大・縮小と送り ---
    if (hovered && io.MouseWheel != 0.0f)
    {
        if (io.KeyCtrl)
        {
            const float pivot = xToTime(io.MousePos.x);
            const float scale = io.MouseWheel > 0.0f ? 0.8f : 1.25f;
            m_viewStart = pivot - (pivot - m_viewStart) * scale;
            m_viewEnd = pivot + (m_viewEnd - pivot) * scale;
        }
        else
        {
            const float shift = (m_viewEnd - m_viewStart) * 0.1f * -io.MouseWheel;
            m_viewStart += shift;
            m_viewEnd += shift;
        }
    }

    // --- 背景 ---
    ImGui::PushClipRect(origin, ImVec2(origin.x + width, origin.y + totalHeight), true);
    for (int i = 0; i < static_cast<int>(Row::Count); ++i)
    {
        drawList->AddRectFilled(
            ImVec2(origin.x, rowTop[i]), ImVec2(origin.x + width, rowTop[i + 1]),
            i % 2 == 0 ? COLOR_ROW_BG : COLOR_ROW_BG_ALT);
        drawList->AddText(ImVec2(origin.x + 6.0f, rowTop[i] + (ROWS[i].height - ImGui::GetTextLineHeight()) * 0.5f),
            COLOR_TEXT_DIM, ROWS[i].label);
    }
    drawList->AddLine(ImVec2(trackLeft, origin.y), ImVec2(trackLeft, origin.y + totalHeight), IM_COL32(70, 75, 85, 255));
    ImGui::PushClipRect(ImVec2(trackLeft, origin.y), ImVec2(trackRight, origin.y + totalHeight), true);

    // クリップの外(0未満・長さ超え)と、使わない範囲を暗くする。
    const float clipLength = ClipLength();
    drawList->AddRectFilled(ImVec2(timeToX(m_viewStart), rowY(Row::Phases)), ImVec2(timeToX(0.0f), origin.y + totalHeight), IM_COL32(0, 0, 0, 170));
    drawList->AddRectFilled(ImVec2(timeToX(clipLength), rowY(Row::Phases)), ImVec2(timeToX(m_viewEnd), origin.y + totalHeight), IM_COL32(0, 0, 0, 170));

    // --- 目盛り ---
    {
        const float pixelsPerSecond = trackWidth / (m_viewEnd - m_viewStart);
        // ラベルの間隔は、文字が重ならない範囲で一番細かいものを選ぶ。
        static constexpr float steps[] = { 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f };
        float labelStep = 1.0f;
        for (float candidate : steps)
            if (candidate * pixelsPerSecond >= 56.0f) { labelStep = candidate; break; }
        const float minorStep = labelStep / 5.0f;
        const float first = std::floor(m_viewStart / minorStep) * minorStep;
        for (float t = first; t <= m_viewEnd; t += minorStep)
        {
            const float x = timeToX(t);
            const bool major = std::fabs(std::remainder(t, labelStep)) < minorStep * 0.25f;
            drawList->AddLine(ImVec2(x, rowY(Row::Ruler) + (major ? 8.0f : 15.0f)), ImVec2(x, rowY(Row::Ruler) + rowH(Row::Ruler)),
                major ? COLOR_TEXT_DIM : IM_COL32(90, 95, 105, 255));
            if (major)
            {
                char text[16]{};
                std::snprintf(text, sizeof(text), labelStep < 0.1f ? "%.2f" : "%.1f", t);
                drawList->AddText(ImVec2(x + 3.0f, rowY(Row::Ruler)), COLOR_TEXT_DIM, text);
                drawList->AddLine(ImVec2(x, rowY(Row::Phases)), ImVec2(x, origin.y + totalHeight), IM_COL32(255, 255, 255, 14));
            }
        }
    }

    // --- 予兆・判定・硬直 ---
    const float phasesTop = rowY(Row::Phases) + 3.0f;
    const float phasesBottom = rowY(Row::Phases) + rowH(Row::Phases) - 3.0f;
    const auto drawSegment = [&](float from, float to, ImU32 color, const char* label, float seconds) {
        const float x0 = timeToX(from);
        const float x1 = timeToX(to);
        drawList->AddRectFilled(ImVec2(x0, phasesTop), ImVec2(x1, phasesBottom), color, 3.0f);
        char text[48]{};
        std::snprintf(text, sizeof(text), "%s %dF", label, static_cast<int>(std::lround(seconds * 60.0f)));
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        if (x1 - x0 > textSize.x + 8.0f)
            drawList->AddText(ImVec2((x0 + x1 - textSize.x) * 0.5f, (phasesTop + phasesBottom - textSize.y) * 0.5f),
                IM_COL32(255, 255, 255, 235), text);
    };
    drawList->AddRectFilled(ImVec2(timeToX(0.0f), phasesTop + 9.0f), ImVec2(timeToX(clipLength), phasesBottom - 9.0f), IM_COL32(70, 74, 84, 255));
    drawSegment(step.clipStart, step.hitStart, COLOR_WINDUP, "予兆", step.WindupSeconds());
    drawSegment(step.hitStart, step.hitEnd, COLOR_ACTIVE, "判定", step.ActiveSeconds());
    drawSegment(step.hitEnd, step.clipEnd, COLOR_RECOVERY, "硬直", step.RecoverySeconds());

    // --- 剣の軌跡 ---
    const bool trailLinked = step.trailStart < 0.0f && step.trailEnd < 0.0f;
    {
        const float top = rowY(Row::Trail) + 5.0f;
        const float bottom = rowY(Row::Trail) + rowH(Row::Trail) - 5.0f;
        drawList->AddRectFilled(ImVec2(timeToX(step.TrailStart()), top), ImVec2(timeToX(step.TrailEnd()), bottom),
            trailLinked ? IM_COL32(240, 200, 90, 110) : COLOR_TRAIL, 3.0f);
        // 帯に収まるときだけ「判定と同じ」と書く(はみ出すと隣の行と重なって読めない)。
        const char* linkedText = "判定と同じ";
        const float barWidth = timeToX(step.TrailEnd()) - timeToX(step.TrailStart());
        if (trailLinked && barWidth > ImGui::CalcTextSize(linkedText).x + 8.0f)
            drawList->AddText(ImVec2(timeToX(step.TrailStart()) + 4.0f, top - 2.0f), IM_COL32(40, 30, 10, 255), linkedText);
    }

    // --- 次の段へ ---
    const float linkTime = step.ComboLinkClipTime();
    {
        const float x = timeToX(linkTime);
        const float y = rowMid(Row::ComboLink);
        drawList->AddLine(ImVec2(timeToX(step.hitEnd), y), ImVec2(x, y), IM_COL32(80, 220, 230, 120), 2.0f);
        drawList->AddQuadFilled(ImVec2(x, y - 7.0f), ImVec2(x + 7.0f, y), ImVec2(x, y + 7.0f), ImVec2(x - 7.0f, y), COLOR_LINK);
    }

    // --- 敵に当たる区間(3Dの側が計算した結果) ---
    if (!analysis.hasEnemy)
        drawList->AddText(ImVec2(trackLeft + 6.0f, rowY(Row::EnemyHit) + 1.0f), COLOR_TEXT_DIM, "敵がいないので計算していない");
    else if (analysis.enemyHitRanges.empty())
        drawList->AddText(ImVec2(trackLeft + 6.0f, rowY(Row::EnemyHit) + 1.0f), COLOR_TEXT_DIM,
            "この位置からは届かない(「敵の前に立つ」と間合いで試せる)");
    else
        for (const auto& [from, to] : analysis.enemyHitRanges)
            drawList->AddRectFilled(ImVec2(timeToX(from), rowY(Row::EnemyHit) + 4.0f),
                ImVec2(std::max(timeToX(to), timeToX(from) + 2.0f), rowY(Row::EnemyHit) + rowH(Row::EnemyHit) - 4.0f),
                COLOR_ENEMY_HIT, 2.0f);

    // --- 剣の速さ(グラフ) ---
    // 剣先が速く動いている所が「振っている」所。判定はこの山に合わせて置くとよい。
    if (analysis.tipSpeeds.size() >= 2)
    {
        const float maxSpeed = std::max(1.0f, *std::max_element(analysis.tipSpeeds.begin(), analysis.tipSpeeds.end()));
        const float top = rowY(Row::Speed) + 4.0f;
        const float bottom = rowY(Row::Speed) + rowH(Row::Speed) - 3.0f;
        std::vector<ImVec2> points;
        points.reserve(analysis.tipSpeeds.size());
        for (std::size_t i = 0; i < analysis.tipSpeeds.size(); ++i)
            points.emplace_back(timeToX(analysis.sampleTimes[i]),
                bottom - (bottom - top) * analysis.tipSpeeds[i] / maxSpeed);
        // 判定の区間の下は塗って、判定と振りの山が重なっているかを見やすくする。
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const float time = analysis.sampleTimes[i];
            if (time < step.hitStart || time > step.hitEnd)
                continue;
            drawList->AddQuadFilled(points[i - 1], points[i], ImVec2(points[i].x, bottom), ImVec2(points[i - 1].x, bottom),
                IM_COL32(232, 72, 60, 70));
        }
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), COLOR_SPEED, ImDrawFlags_None, 1.5f);
        // 使う範囲の中で一番速い瞬間に印を付ける。
        int fastest = -1;
        for (std::size_t i = 0; i < analysis.tipSpeeds.size(); ++i)
        {
            const float time = analysis.sampleTimes[i];
            if (time < step.clipStart || time > step.clipEnd)
                continue;
            if (fastest < 0 || analysis.tipSpeeds[i] > analysis.tipSpeeds[fastest])
                fastest = static_cast<int>(i);
        }
        if (fastest >= 0)
        {
            const ImVec2 peak = points[fastest];
            drawList->AddTriangleFilled(ImVec2(peak.x - 5.0f, peak.y - 9.0f), ImVec2(peak.x + 5.0f, peak.y - 9.0f), ImVec2(peak.x, peak.y - 2.0f), COLOR_TRAIL);
            char text[32]{};
            std::snprintf(text, sizeof(text), "最速 %dF", GameFrameOf(analysis.sampleTimes[fastest]));
            drawList->AddText(ImVec2(peak.x + 7.0f, top - 3.0f), COLOR_TRAIL, text);
        }
    }

    // --- イベント ---
    for (int i = 0; i < static_cast<int>(step.events.size()); ++i)
    {
        const Combat::TimelineEvent& event = step.events[i];
        const Row row = RowOfEvent(event.type);
        const float x = timeToX(event.time);
        const float y = rowMid(row);
        const float r = 7.0f;
        drawList->AddQuadFilled(ImVec2(x, y - r), ImVec2(x + r, y), ImVec2(x, y + r), ImVec2(x - r, y), EventColor(event.type));
        if (i == m_selectedEvent)
            drawList->AddQuad(ImVec2(x, y - r - 3), ImVec2(x + r + 3, y), ImVec2(x, y + r + 3), ImVec2(x - r - 3, y), IM_COL32(255, 255, 255, 255), 2.0f);
    }

    // --- 区切りのつまみ ---
    const auto drawHandle = [&](float t, float top, float bottom, ImU32 color) {
        const float x = timeToX(t);
        drawList->AddRectFilled(ImVec2(x - 2.0f, top - 1.0f), ImVec2(x + 2.0f, bottom + 1.0f), color);
    };
    drawHandle(step.clipStart, phasesTop, phasesBottom, IM_COL32(255, 255, 255, 230));
    drawHandle(step.hitStart, phasesTop, phasesBottom, IM_COL32(255, 255, 255, 230));
    drawHandle(step.hitEnd, phasesTop, phasesBottom, IM_COL32(255, 255, 255, 230));
    drawHandle(step.clipEnd, phasesTop, phasesBottom, IM_COL32(255, 255, 255, 230));
    drawHandle(step.TrailStart(), rowY(Row::Trail) + 4.0f, rowY(Row::Trail) + rowH(Row::Trail) - 4.0f, IM_COL32(255, 240, 200, 230));
    drawHandle(step.TrailEnd(), rowY(Row::Trail) + 4.0f, rowY(Row::Trail) + rowH(Row::Trail) - 4.0f, IM_COL32(255, 240, 200, 230));

    // --- 再生位置 ---
    {
        const float x = timeToX(m_time);
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + totalHeight), COLOR_PLAYHEAD, 1.5f);
        drawList->AddTriangleFilled(ImVec2(x - 6.0f, origin.y), ImVec2(x + 6.0f, origin.y), ImVec2(x, origin.y + 9.0f), COLOR_PLAYHEAD);
    }
    ImGui::PopClipRect();
    ImGui::PopClipRect();

    // ------------------------------------------------------------
    // マウス操作
    // ------------------------------------------------------------
    const ImVec2 mouse = io.MousePos;
    const float mouseTime = xToTime(mouse.x);
    const auto rowAt = [&](float y) {
        for (int i = 0; i < static_cast<int>(Row::Count); ++i)
            if (y >= rowTop[i] && y < rowTop[i + 1])
                return static_cast<Row>(i);
        return Row::Count;
    };
    const auto isNear = [&](float t) { return std::fabs(timeToX(t) - mouse.x) <= HANDLE_GRAB_PIXELS; };
    const Row mouseRow = rowAt(mouse.y);
    const bool overTrack = mouse.x >= trackLeft - 2.0f;

    // マウスの下にあるもの(つかめるもの)を探す。
    DragState hover;
    if (hovered && overTrack)
    {
        if (mouseRow == Row::Ruler)
            hover.target = DragTarget::Playhead;
        else if (mouseRow == Row::Phases)
        {
            // 端は、近い方を優先する(判定の帯が細いときでも両端をつかみ分けられるように)。
            struct Candidate { DragTarget target; float time; };
            const Candidate candidates[] = {
                { DragTarget::HitStart, step.hitStart }, { DragTarget::HitEnd, step.hitEnd },
                { DragTarget::ClipStart, step.clipStart }, { DragTarget::ClipEnd, step.clipEnd } };
            float best = HANDLE_GRAB_PIXELS + 1.0f;
            for (const Candidate& candidate : candidates)
            {
                const float distance = std::fabs(timeToX(candidate.time) - mouse.x);
                if (distance < best)
                {
                    best = distance;
                    hover.target = candidate.target;
                }
            }
            if (hover.target == DragTarget::None && mouseTime > step.hitStart && mouseTime < step.hitEnd)
                hover.target = DragTarget::HitRange;
            if (hover.target == DragTarget::None)
                hover.target = DragTarget::Playhead;
        }
        else if (mouseRow == Row::Trail)
        {
            if (isNear(step.TrailStart())) hover.target = DragTarget::TrailStart;
            else if (isNear(step.TrailEnd())) hover.target = DragTarget::TrailEnd;
            else if (mouseTime > step.TrailStart() && mouseTime < step.TrailEnd()) hover.target = DragTarget::TrailRange;
            else hover.target = DragTarget::Playhead;
        }
        else if (mouseRow == Row::ComboLink)
            hover.target = isNear(linkTime) ? DragTarget::ComboLink : DragTarget::Playhead;
        else if (mouseRow == Row::Sound || mouseRow == Row::Shake || mouseRow == Row::Sparks)
        {
            float best = 9.0f;
            for (int i = 0; i < static_cast<int>(step.events.size()); ++i)
            {
                if (RowOfEvent(step.events[i].type) != mouseRow)
                    continue;
                const float distance = std::fabs(timeToX(step.events[i].time) - mouse.x);
                if (distance < best)
                {
                    best = distance;
                    hover.target = DragTarget::Event;
                    hover.eventIndex = i;
                }
            }
            if (hover.target == DragTarget::None)
                hover.target = DragTarget::Playhead;
        }
        else
            hover.target = DragTarget::Playhead;
    }

    // つかめるものの上では、マウスの形とヒントを変える。
    if (hovered && m_drag.target == DragTarget::None)
    {
        switch (hover.target)
        {
        case DragTarget::ClipStart: case DragTarget::ClipEnd:
        case DragTarget::HitStart: case DragTarget::HitEnd:
        case DragTarget::TrailStart: case DragTarget::TrailEnd:
        case DragTarget::ComboLink:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            break;
        case DragTarget::HitRange: case DragTarget::TrailRange: case DragTarget::Event:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
        default:
            break;
        }
        const char* tip = nullptr;
        switch (hover.target)
        {
        case DragTarget::ClipStart: tip = "使い始め(クリップのここから再生する)"; break;
        case DragTarget::HitStart: tip = "判定の始まり  [ キーで再生位置に置ける"; break;
        case DragTarget::HitEnd: tip = "判定の終わり  ] キーで再生位置に置ける"; break;
        case DragTarget::ClipEnd: tip = "使い終わり(次の入力が無ければここで技が終わる)"; break;
        case DragTarget::HitRange: tip = "判定の区間を丸ごと動かす"; break;
        case DragTarget::TrailStart: case DragTarget::TrailEnd: case DragTarget::TrailRange:
            tip = "剣の軌跡を出す区間"; break;
        case DragTarget::ComboLink: tip = "ここから次の段へ移れる(連打していれば)"; break;
        default: break;
        }
        if (tip != nullptr)
            ImGui::SetTooltip("%s", tip);
        else if (hover.target == DragTarget::Event && hover.eventIndex >= 0)
        {
            const Combat::TimelineEvent& event = step.events[hover.eventIndex];
            if (event.type == Combat::TimelineEventType::Sound)
                ImGui::SetTooltip("%s: %s (%dF目)", EventTypeLabel(event.type), SoundLabel(event.sound), GameFrameOf(event.time));
            else
                ImGui::SetTooltip("%s (%dF目)", EventTypeLabel(event.type), GameFrameOf(event.time));
        }
    }

    // 押した瞬間: つかむ。
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // イベントの行のダブルクリックは追加。
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hover.target != DragTarget::Event &&
            (mouseRow == Row::Sound || mouseRow == Row::Shake || mouseRow == Row::Sparks))
        {
            const Combat::TimelineEventType type = mouseRow == Row::Sound ? Combat::TimelineEventType::Sound
                : mouseRow == Row::Shake ? Combat::TimelineEventType::CameraShake
                : Combat::TimelineEventType::Sparks;
            AddEvent(type, mouseTime);
            m_drag = {};
        }
        else
        {
            m_drag = hover;
            switch (m_drag.target)
            {
            case DragTarget::Playhead:
                m_playing = false;
                break;
            case DragTarget::HitRange:
                PushUndo();
                m_drag.grabOffset = mouseTime - step.hitStart;
                m_drag.rangeLength = step.hitEnd - step.hitStart;
                break;
            case DragTarget::TrailRange:
                PushUndo();
                m_drag.grabOffset = mouseTime - step.TrailStart();
                m_drag.rangeLength = step.TrailEnd() - step.TrailStart();
                break;
            case DragTarget::Event:
                PushUndo();
                m_selectedEvent = m_drag.eventIndex;
                m_drag.grabOffset = mouseTime - step.events[m_drag.eventIndex].time;
                break;
            case DragTarget::None:
                break;
            default:
                PushUndo();
                break;
            }
            // イベント以外をつかんだら、イベントの選択を外す(右の欄が技の設定に戻る)。
            // 目盛りで再生位置を動かすだけのときは、選択を残す(イベントの前後を見比べられるように)。
            if (m_drag.target != DragTarget::Event && m_drag.target != DragTarget::None && mouseRow != Row::Ruler)
                m_selectedEvent = -1;
        }
    }

    // ドラッグ中: 動かす。
    if (active && m_drag.target != DragTarget::None && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float t = Snap(mouseTime);
        switch (m_drag.target)
        {
        case DragTarget::Playhead: SetTime(mouseTime); break;
        case DragTarget::ClipStart: step.clipStart = std::min(t, step.hitStart); break;
        case DragTarget::HitStart: step.hitStart = std::clamp(t, step.clipStart, step.hitEnd); break;
        case DragTarget::HitEnd: step.hitEnd = std::clamp(t, step.hitStart, step.clipEnd); break;
        case DragTarget::ClipEnd: step.clipEnd = std::max(t, step.hitEnd); break;
        case DragTarget::HitRange:
        {
            const float start = std::clamp(Snap(mouseTime - m_drag.grabOffset), step.clipStart, step.clipEnd - m_drag.rangeLength);
            step.hitStart = start;
            step.hitEnd = start + m_drag.rangeLength;
            break;
        }
        case DragTarget::TrailStart:
            step.trailEnd = step.TrailEnd();
            step.trailStart = std::min(t, step.trailEnd);
            break;
        case DragTarget::TrailEnd:
            step.trailStart = step.TrailStart();
            step.trailEnd = std::max(t, step.trailStart);
            break;
        case DragTarget::TrailRange:
        {
            const float start = Snap(mouseTime - m_drag.grabOffset);
            step.trailStart = start;
            step.trailEnd = start + m_drag.rangeLength;
            break;
        }
        case DragTarget::ComboLink:
            step.comboLinkSeconds = std::max(0.0f, (t - step.hitEnd) / step.playbackRate);
            break;
        case DragTarget::Event:
            if (m_drag.eventIndex >= 0 && m_drag.eventIndex < static_cast<int>(step.events.size()))
                step.events[m_drag.eventIndex].time = Snap(mouseTime - m_drag.grabOffset);
            break;
        default:
            break;
        }
        if (m_drag.target != DragTarget::Playhead)
        {
            MarkEdited(false);
            // 区切りを動かしているときは、再生位置もそこへ合わせる(その瞬間の姿勢がすぐ見える)。
            if (m_drag.target != DragTarget::Event && m_drag.target != DragTarget::HitRange &&
                m_drag.target != DragTarget::TrailRange && m_drag.target != DragTarget::ComboLink)
                SetTime(t);
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_drag.target != DragTarget::None)
    {
        // 使い始めを動かすと踏み込みの基準が変わるので、離したときに解析をやり直してもらう。
        if (m_drag.target == DragTarget::ClipStart)
            ++m_analysisRevision;
        m_drag = {};
    }

    // 右クリック: イベントのメニュー。
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hover.target == DragTarget::Event)
    {
        m_selectedEvent = hover.eventIndex;
        ImGui::OpenPopup("event_menu");
    }
    if (ImGui::BeginPopup("event_menu"))
    {
        if (m_selectedEvent >= 0 && m_selectedEvent < static_cast<int>(step.events.size()))
        {
            if (ImGui::MenuItem("再生位置へ移す"))
            {
                PushUndo();
                step.events[m_selectedEvent].time = Snap(m_time);
                MarkEdited(false);
            }
            if (ImGui::MenuItem("複製(1フレーム後ろ)"))
            {
                PushUndo();
                Combat::TimelineEvent copy = step.events[m_selectedEvent];
                copy.time += FrameStep();
                step.events.push_back(copy);
                m_selectedEvent = static_cast<int>(step.events.size()) - 1;
                MarkEdited(false);
            }
            if (ImGui::MenuItem("消す", "Delete"))
                DeleteSelectedEvent();
        }
        ImGui::EndPopup();
    }

    // イベントの追加ボタン(行をダブルクリックしなくても足せるように)。
    ImGui::Spacing();
    ImGui::TextDisabled("再生位置に追加:");
    ImGui::SameLine();
    if (ImGui::SmallButton("効果音 (1)")) AddEvent(Combat::TimelineEventType::Sound, m_time);
    ImGui::SameLine();
    if (ImGui::SmallButton("カメラの揺れ (2)")) AddEvent(Combat::TimelineEventType::CameraShake, m_time);
    ImGui::SameLine();
    if (ImGui::SmallButton("火花 (3)")) AddEvent(Combat::TimelineEventType::Sparks, m_time);
    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::SmallButton("全体を表示 (F)")) FitView();
    if (!analysis.clipLoaded)
    {
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "クリップを読み込めていない: %s", step.clipFile.c_str());
    }
}

void AttackTimelineEditor::DrawProperties(const Analysis& analysis)
{
    Combat::PlayerComboStep& step = Step();
    const float clipLength = ClipLength();

    // 選んだイベントの設定。
    if (m_selectedEvent >= 0 && m_selectedEvent < static_cast<int>(step.events.size()))
    {
        Combat::TimelineEvent& event = step.events[m_selectedEvent];
        ImGui::SeparatorText(EventTypeLabel(event.type));
        ImGui::SetNextItemWidth(150.0f);
        EditField(false, [&] { return ImGui::DragFloat("時刻(秒)", &event.time, 0.002f, 0.0f, clipLength, "%.3f"); });
        ImGui::SameLine();
        ImGui::TextDisabled("%dF目", GameFrameOf(event.time));
        switch (event.type)
        {
        case Combat::TimelineEventType::Sound:
        {
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::BeginCombo("音", SoundLabel(event.sound)))
            {
                for (int i = 0; i < static_cast<int>(Combat::TimelineSound::Count); ++i)
                {
                    const auto sound = static_cast<Combat::TimelineSound>(i);
                    if (ImGui::Selectable(SoundLabel(sound), sound == event.sound) && sound != event.sound)
                    {
                        PushUndo();
                        event.sound = sound;
                        MarkEdited(false);
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case Combat::TimelineEventType::CameraShake:
            ImGui::SetNextItemWidth(150.0f);
            EditField(false, [&] { return ImGui::DragFloat("強さ", &event.strength, 0.02f, 0.0f, 10.0f, "%.2f"); });
            ImGui::SetNextItemWidth(150.0f);
            EditField(false, [&] { return ImGui::DragFloat("長さ(秒)", &event.duration, 0.005f, 0.01f, 3.0f, "%.2f"); });
            ImGui::TextDisabled("参考: 命中 1.1 / 怯み 2.2 / 被弾 2.6");
            break;
        case Combat::TimelineEventType::Sparks:
            ImGui::SetNextItemWidth(150.0f);
            EditField(false, [&] { return ImGui::DragInt("本数", &event.count, 0.2f, 1, 96); });
            ImGui::SetNextItemWidth(150.0f);
            EditField(false, [&] { return ImGui::DragFloat("大きさ", &event.strength, 0.02f, 0.1f, 4.0f, "%.2f"); });
            ImGui::TextDisabled("剣先から、刃の進む向きへ飛ぶ");
            break;
        default:
            break;
        }
        if (ImGui::Button("消す"))
            DeleteSelectedEvent();
        ImGui::SameLine();
        if (ImGui::Button("選択を外す"))
            m_selectedEvent = -1;
        return;
    }

    // 技の設定。
    ImGui::SeparatorText(Combat::PlayerAttackSlotLabel(m_slot));
    ImGui::PushItemWidth(120.0f);
    const auto timeField = [&](const char* label, float* value) {
        EditField(false, [&] { return ImGui::DragFloat(label, value, 0.002f, 0.0f, clipLength, "%.3f"); });
        ImGui::SameLine();
        ImGui::TextDisabled("%dF", GameFrameOf(*value));
    };
    ImGui::TextDisabled("時刻はクリップの秒 (長さ %.2f秒)", clipLength);
    timeField("使い始め", &step.clipStart);
    timeField("判定の始まり", &step.hitStart);
    timeField("判定の終わり", &step.hitEnd);
    timeField("使い終わり", &step.clipEnd);
    ImGui::PopItemWidth();
    if (ImGui::Button("判定を振りに合わせる"))
        FitHitWindowToSwing(analysis);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("剣先の速さが最大の半分を超える区間(=振っている所)に判定を置く。\nクリップを替えたときの最初の当たりを作るのに使う");
    ImGui::PushItemWidth(120.0f);
    EditField(true, [&] { return ImGui::DragFloat("再生速度", &step.playbackRate, 0.005f, 0.1f, 4.0f, "%.2f倍"); });
    EditField(false, [&] {
        float frames = step.comboLinkSeconds * 60.0f;
        const bool changed = ImGui::DragFloat("次の段への受付", &frames, 0.1f, 0.0f, 60.0f, "判定後%.0fF");
        if (changed)
            step.comboLinkSeconds = std::round(frames) / 60.0f;
        return changed;
    });
    EditField(true, [&] { return ImGui::DragFloat("踏み込みの倍率", &step.rootMotionScale, 0.01f, 0.0f, 4.0f, "%.2f倍"); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("1 = クリップどおり。地面を蹴らずに滑って見えるので、跳ぶ技以外は1のままがよい");
    ImGui::PopItemWidth();

    ImGui::SeparatorText("剣の軌跡");
    bool linked = step.trailStart < 0.0f && step.trailEnd < 0.0f;
    if (ImGui::Checkbox("判定と同じ区間", &linked))
    {
        PushUndo();
        if (linked)
        {
            step.trailStart = -1.0f;
            step.trailEnd = -1.0f;
        }
        else
        {
            step.trailStart = step.hitStart;
            step.trailEnd = step.hitEnd;
        }
        MarkEdited(false);
    }
    if (!linked)
    {
        ImGui::PushItemWidth(120.0f);
        timeField("軌跡の始まり", &step.trailStart);
        timeField("軌跡の終わり", &step.trailEnd);
        ImGui::PopItemWidth();
    }

    ImGui::SeparatorText("イベント");
    if (step.events.empty())
        ImGui::TextDisabled("まだ無い。行をダブルクリックか、1 2 3 キーで追加");
    for (int i = 0; i < static_cast<int>(step.events.size()); ++i)
    {
        const Combat::TimelineEvent& event = step.events[i];
        char label[96]{};
        if (event.type == Combat::TimelineEventType::Sound)
            std::snprintf(label, sizeof(label), "%3dF  %s: %s##event%d", GameFrameOf(event.time), EventTypeLabel(event.type), SoundLabel(event.sound), i);
        else
            std::snprintf(label, sizeof(label), "%3dF  %s##event%d", GameFrameOf(event.time), EventTypeLabel(event.type), i);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(EventColor(event.type)));
        if (ImGui::Selectable(label, false))
        {
            m_selectedEvent = i;
            m_playing = false;
            SetTime(event.time);
        }
        ImGui::PopStyleColor();
    }
}
