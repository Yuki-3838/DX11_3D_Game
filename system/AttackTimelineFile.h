#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "CombatAttackTable.h"

/**
 * @file AttackTimelineFile.h
 * @brief プレイヤーの技のタイムライン(時間・判定・軌跡・イベント)をテキストファイルへ読み書きする。
 *
 * ファイルは assets/data/attack_timeline.txt。ゲーム内のタイムラインエディター(F6)が保存する。
 * 手で読めて、gitで差分が見やすい形にしている(1行に1つの値)。例:
 *
 *   [weak1]
 *   clip = assets/motion/sword and shield slash (5).fbx
 *   clip_start = 0.240
 *   hit_start = 0.440
 *   ...
 *   event = sound 0.450 whoosh
 *   event = shake 0.600 1.20 0.20
 *   event = sparks 0.650 14 1.00
 *
 * 時刻はすべてクリップの秒。ファイルに無い技・値は既定値(Combat::DefaultPlayerAttack)のまま。
 */
namespace AttackTimelineFile
{
/** 技の組(プレイヤーのモデル)ごとに別のファイルにする。クリップの骨格が違うので、値を共有できない。 */
inline const char* DefaultPath()
{
    return Combat::ActivePlayerAttackSet() == Combat::PlayerAttackSet::Knight
        ? "assets/data/attack_timeline_knight.txt"
        : "assets/data/attack_timeline.txt";
}

/**
 * @brief 使う技の組を切り替え、技の値をその組の既定値へ戻してから、組のファイルを読む。
 * GameScene の初期化で1回呼ぶ(プレイヤーのモデルを選んだ直後)。
 */
inline bool SelectSetAndLoad(Combat::PlayerAttackSet set);

/** UTF-8の文字列からパスを作る(クリップのファイル名に日本語が入っても読めるように)。 */
inline std::filesystem::path PathFromUtf8(const std::string& text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

inline const char* SoundKey(Combat::TimelineSound sound)
{
    switch (sound)
    {
    case Combat::TimelineSound::Whoosh:       return "whoosh";
    case Combat::TimelineSound::SwordHit:     return "sword_hit";
    case Combat::TimelineSound::DragonAttack: return "dragon_attack";
    case Combat::TimelineSound::Roar:         return "roar";
    default:                                  return "whoosh";
    }
}

inline Combat::TimelineSound SoundFromKey(const std::string& key)
{
    for (int i = 0; i < static_cast<int>(Combat::TimelineSound::Count); ++i)
    {
        const auto sound = static_cast<Combat::TimelineSound>(i);
        if (key == SoundKey(sound))
            return sound;
    }
    return Combat::TimelineSound::Whoosh;
}

namespace Detail
{
inline std::string Trim(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

inline bool ParseEvent(const std::string& value, Combat::TimelineEvent& event)
{
    std::istringstream stream(value);
    std::string type;
    if (!(stream >> type >> event.time))
        return false;
    if (type == "sound")
    {
        std::string sound;
        stream >> sound;
        event.type = Combat::TimelineEventType::Sound;
        event.sound = SoundFromKey(sound);
        return true;
    }
    if (type == "shake")
    {
        event.type = Combat::TimelineEventType::CameraShake;
        stream >> event.strength >> event.duration;
        return true;
    }
    if (type == "sparks")
    {
        event.type = Combat::TimelineEventType::Sparks;
        stream >> event.count >> event.strength;
        return true;
    }
    return false;
}

inline std::string FormatEvent(const Combat::TimelineEvent& event)
{
    char buffer[128]{};
    switch (event.type)
    {
    case Combat::TimelineEventType::Sound:
        std::snprintf(buffer, sizeof(buffer), "sound %.3f %s", event.time, SoundKey(event.sound));
        break;
    case Combat::TimelineEventType::CameraShake:
        std::snprintf(buffer, sizeof(buffer), "shake %.3f %.2f %.2f", event.time, event.strength, event.duration);
        break;
    case Combat::TimelineEventType::Sparks:
        std::snprintf(buffer, sizeof(buffer), "sparks %.3f %d %.2f", event.time, event.count, event.strength);
        break;
    default:
        break;
    }
    return buffer;
}
} // namespace Detail

/**
 * @brief 最後に読んだ・保存したときの値(=ファイルの中身)。
 * エディターが「保存していない変更があるか」を判断するのに使う。
 * シーンを作り直してもエディターの判断がずれないよう、ここ(プロセスに1つ)で覚えておく。
 */
inline std::array<Combat::PlayerComboStep, Combat::PLAYER_ATTACK_SLOT_COUNT>& SavedState()
{
    static std::array<Combat::PlayerComboStep, Combat::PLAYER_ATTACK_SLOT_COUNT> saved = [] {
        std::array<Combat::PlayerComboStep, Combat::PLAYER_ATTACK_SLOT_COUNT> result;
        for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
            result[i] = Combat::DefaultPlayerAttack(static_cast<Combat::PlayerAttackSlot>(i));
        return result;
    }();
    return saved;
}

inline void RememberCurrentAsSaved()
{
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
        SavedState()[i] = Combat::EditablePlayerAttack(static_cast<Combat::PlayerAttackSlot>(i));
}

/**
 * @brief ファイルを読み、Combat::EditablePlayerAttack を上書きする。
 * @return ファイルを読めたか(無ければfalse。そのときは既定値のまま)。
 */
inline bool Load(const std::string& path = DefaultPath())
{
    std::ifstream file(AttackTimelineFile::PathFromUtf8(path));
    if (!file)
        return false;
    struct RememberOnExit
    {
        ~RememberOnExit() { RememberCurrentAsSaved(); }
    } rememberOnExit;

    Combat::PlayerComboStep* current = nullptr;
    std::string line;
    while (std::getline(file, line))
    {
        line = Detail::Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            const std::string name = line.substr(1, line.size() - 2);
            current = nullptr;
            for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
            {
                const auto slot = static_cast<Combat::PlayerAttackSlot>(i);
                if (name == Combat::PlayerAttackSlotKey(slot))
                {
                    // 既定値から始め、ファイルにある値だけを上書きする
                    // (何度読み直しても、前に読んだ値やイベントが残らないように)。
                    current = &Combat::EditablePlayerAttack(slot);
                    *current = Combat::DefaultPlayerAttack(slot);
                }
            }
            continue;
        }
        if (current == nullptr)
            continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos)
            continue;
        const std::string key = Detail::Trim(line.substr(0, equal));
        const std::string value = Detail::Trim(line.substr(equal + 1));
        const auto number = [&value]() { return std::stof(value); };
        try
        {
            if (key == "clip") current->clipFile = value;
            else if (key == "clip_start") current->clipStart = number();
            else if (key == "hit_start") current->hitStart = number();
            else if (key == "hit_end") current->hitEnd = number();
            else if (key == "clip_end") current->clipEnd = number();
            else if (key == "playback_rate") current->playbackRate = std::max(0.05f, number());
            else if (key == "combo_link") current->comboLinkSeconds = number();
            else if (key == "root_motion_scale") current->rootMotionScale = number();
            else if (key == "trail_start") current->trailStart = number();
            else if (key == "trail_end") current->trailEnd = number();
            else if (key == "event")
            {
                Combat::TimelineEvent event;
                if (Detail::ParseEvent(value, event))
                    current->events.push_back(event);
            }
        }
        catch (...)
        {
            // 数値として読めない行は飛ばす(手で書き換えたときの打ち間違いでゲームを止めない)。
        }
    }
    return true;
}

/** Combat::EditablePlayerAttack の内容をファイルへ書く。 */
inline bool Save(const std::string& path = DefaultPath())
{
    std::error_code error;
    std::filesystem::create_directories(AttackTimelineFile::PathFromUtf8(path).parent_path(), error);
    std::ofstream file(AttackTimelineFile::PathFromUtf8(path), std::ios::binary);
    if (!file)
        return false;

    const auto line = [&file](const std::string& text) { file << text << "\r\n"; };
    const auto number = [](float value) {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%.3f", value);
        return std::string(buffer);
    };
    line("# 攻撃のタイムライン。ゲーム内のタイムラインエディター(F6)で編集・保存する。");
    line("# 時刻はすべてクリップの秒。trail_start / trail_end が負なら、軌跡は攻撃判定と同じ区間。");
    line("# event = sound <時刻> <whoosh|sword_hit|dragon_attack|roar>");
    line("# event = shake <時刻> <強さ> <秒>");
    line("# event = sparks <時刻> <本数> <大きさ>");
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
    {
        const auto slot = static_cast<Combat::PlayerAttackSlot>(i);
        const Combat::PlayerComboStep& step = Combat::EditablePlayerAttack(slot);
        line("");
        line(std::string("[") + Combat::PlayerAttackSlotKey(slot) + "]");
        line("clip = " + step.clipFile);
        line("clip_start = " + number(step.clipStart));
        line("hit_start = " + number(step.hitStart));
        line("hit_end = " + number(step.hitEnd));
        line("clip_end = " + number(step.clipEnd));
        line("playback_rate = " + number(step.playbackRate));
        line("combo_link = " + number(step.comboLinkSeconds));
        line("root_motion_scale = " + number(step.rootMotionScale));
        line("trail_start = " + number(step.trailStart));
        line("trail_end = " + number(step.trailEnd));
        for (const Combat::TimelineEvent& event : step.events)
            line("event = " + Detail::FormatEvent(event));
    }
    file.close();
    if (file.fail())
        return false;
    RememberCurrentAsSaved();
    return true;
}

inline bool SelectSetAndLoad(Combat::PlayerAttackSet set)
{
    Combat::ActivePlayerAttackSet() = set;
    for (int i = 0; i < Combat::PLAYER_ATTACK_SLOT_COUNT; ++i)
    {
        const auto slot = static_cast<Combat::PlayerAttackSlot>(i);
        Combat::EditablePlayerAttack(slot) = Combat::DefaultPlayerAttack(slot);
    }
    RememberCurrentAsSaved();
    return Load();
}
} // namespace AttackTimelineFile
