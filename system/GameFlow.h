#pragma once

#include <string>
#include <utility>

namespace GameFlow
{
    enum class Result
    {
        Victory,
        Defeat
    };

    inline Result lastResult = Result::Defeat;
    inline std::string requestedScene{};

    /**
     * @brief タイトルとリザルトを使うか(ゲームループ)。
     *
     * true  : タイトル → ゲーム → リザルト → タイトル(提出・お披露目の通常の形)
     * false : ゲーム本編だけ。決着がついたら同じ戦いをやり直す。
     *         動きや手触りを続けて見たいときに、毎回タイトルへ戻らずに済む。
     *
     * 起動時に dev_settings.ini の gameloop で決める。デバッグ表示からも切り替えられる。
     */
    inline bool useGameLoop = true;
    // 本編だけのときに、決着後へ戦いをやり直すための合図。
    inline bool restartRequested = false;

    inline void SetResult(Result result)
    {
        lastResult = result;
    }

    inline Result GetResult()
    {
        return lastResult;
    }

    inline void RequestScene(std::string sceneName)
    {
        requestedScene = std::move(sceneName);
    }

    inline std::string ConsumeRequestedScene()
    {
        std::string result = std::move(requestedScene);
        requestedScene.clear();
        return result;
    }

    inline void RequestRestart()
    {
        restartRequested = true;
    }

    inline bool ConsumeRestart()
    {
        const bool result = restartRequested;
        restartRequested = false;
        return result;
    }
}
